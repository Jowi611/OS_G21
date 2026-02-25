#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "parsePGM.h"

#define BLOCK_SIZE 16384

unsigned char **buffer;
int *bytes_in_block;
int buffer_size;

int in = 0, out = 0, count = 0;

pthread_mutex_t mtxBuf = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notFull = PTHREAD_COND_INITIALIZER;
pthread_cond_t notEmpty = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mtxHist = PTHREAD_MUTEX_INITIALIZER;
unsigned int histogram[256] = {0};

int producers_alive = 0;

pthread_mutex_t mtxRead = PTHREAD_MUTEX_INITIALIZER;
int next_offset = 0;

char *input_path = NULL;
char *output_path = NULL;

static void putstr(int fd, const char *s) {
    write(fd, s, strlen(s));
}

static int u32_to_str(unsigned int x, char *out) {
    char tmp[16];
    int n = 0;
    if (x == 0) {
        out[0] = '0';
        return 1;
    }
    while (x > 0) {
        tmp[n++] = (char)('0' + (x % 10));
        x /= 10;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

static void *producer(void *arg) {
    (void)arg;

    int fd = open(input_path, O_RDONLY);
    if (fd < 0) {
        pthread_mutex_lock(&mtxBuf);
        producers_alive--;
        pthread_cond_broadcast(&notEmpty);
        pthread_cond_broadcast(&notFull);
        pthread_mutex_unlock(&mtxBuf);
        return NULL;
    }

    while (1) {
        int my_off;

        pthread_mutex_lock(&mtxRead);
        my_off = next_offset;
        next_offset += BLOCK_SIZE;
        pthread_mutex_unlock(&mtxRead);

        unsigned char *blk = (unsigned char *)malloc(BLOCK_SIZE);
        if (!blk) break;

        if (lseek(fd, my_off, SEEK_SET) == (off_t)-1) {
            free(blk);
            break;
        }

        int n = (int)read(fd, blk, BLOCK_SIZE);
        if (n <= 0) {
            free(blk);
            break;
        }

        pthread_mutex_lock(&mtxBuf);
        while (count == buffer_size) {
            pthread_cond_wait(&notFull, &mtxBuf);
        }

        buffer[in] = blk;
        bytes_in_block[in] = n;
        in = (in + 1) % buffer_size;
        count++;

        pthread_cond_broadcast(&notEmpty);
        pthread_mutex_unlock(&mtxBuf);
    }

    close(fd);

    pthread_mutex_lock(&mtxBuf);
    producers_alive--;
    pthread_cond_broadcast(&notEmpty);
    pthread_cond_broadcast(&notFull);
    pthread_mutex_unlock(&mtxBuf);

    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;

    unsigned int local[256];

    while (1) {
        unsigned char *blk;
        int n;

        pthread_mutex_lock(&mtxBuf);
        while (count == 0 && producers_alive > 0) {
            pthread_cond_wait(&notEmpty, &mtxBuf);
        }

        if (count == 0 && producers_alive == 0) {
            pthread_mutex_unlock(&mtxBuf);
            break;
        }

        blk = buffer[out];
        n = bytes_in_block[out];
        out = (out + 1) % buffer_size;
        count--;

        pthread_cond_broadcast(&notFull);
        pthread_mutex_unlock(&mtxBuf);

        for (int i = 0; i < 256; i++) local[i] = 0;
        for (int i = 0; i < n; i++) local[blk[i]]++;

        pthread_mutex_lock(&mtxHist);
        for (int i = 0; i < 256; i++) histogram[i] += local[i];
        pthread_mutex_unlock(&mtxHist);

        free(blk);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        putstr(2, "Usage: computeHistogram input.pgm output.txt N_producers N_consumers sizeBuffer\n");
        return 1;
    }

    input_path = argv[1];
    output_path = argv[2];
    int n_producers = atoi(argv[3]);
    int n_consumers = atoi(argv[4]);
    buffer_size = atoi(argv[5]);

    if (n_producers <= 0 || n_consumers <= 0 || buffer_size <= 0) {
        putstr(2, "Error: arguments must be positive integers.\n");
        return 1;
    }

    int width, height, maxval;
    int headerBytes = parse_pgm_header(input_path, &width, &height, &maxval);
    if (headerBytes < 0) {
        putstr(2, "Error: could not parse PGM header.\n");
        return 1;
    }

    next_offset = headerBytes;
    producers_alive = n_producers;

    buffer = (unsigned char **)malloc(sizeof(unsigned char *) * buffer_size);
    bytes_in_block = (int *)malloc(sizeof(int) * buffer_size);
    if (!buffer || !bytes_in_block) return 1;

    pthread_t prods[n_producers];
    pthread_t cons[n_consumers];

    for (int i = 0; i < n_consumers; i++) {
        pthread_create(&cons[i], NULL, consumer, NULL);
    }
    for (int i = 0; i < n_producers; i++) {
        pthread_create(&prods[i], NULL, producer, NULL);
    }

    for (int i = 0; i < n_producers; i++) {
        pthread_join(prods[i], NULL);
    }
    for (int i = 0; i < n_consumers; i++) {
        pthread_join(cons[i], NULL);
    }

    int outfd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outfd < 0) {
        putstr(2, "Error: could not open output file.\n");
        free(bytes_in_block);
        free(buffer);
        return 1;
    }

    char line[64];
    for (int i = 0; i < 256; i++) {
        int p = 0;
        p += u32_to_str((unsigned int)i, line + p);
        line[p++] = ',';
        p += u32_to_str(histogram[i], line + p);
        line[p++] = '\n';
        write(outfd, line, p);
    }

    close(outfd);

    free(bytes_in_block);
    free(buffer);

    return 0;
}