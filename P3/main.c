#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "parsePGM.h"
#define READ_BUF 1024

//our thread structure
typedef struct {
    const char *path;
    int offset;      // from start of file, including header
    int bytesToRead;
    unsigned int localHist[256];
} ThreadInfo;

static void *worker(void *arg) {
    ThreadInfo *t = (ThreadInfo *)arg;
    //we initialize first the localHist as all 0
    for (int i = 0; i < 256; i++) {
        t->localHist[i] = 0;
    }
    //we open the file in read only mode
    int fd = open(t->path, O_RDONLY);
    if (fd < 0) {
        perror("open (worker)");
        return NULL;
    }
    //we jump to the offset
    if (lseek(fd, t->offset, SEEK_SET) == (off_t)-1) {
        perror("lseek (worker)");
        close(fd);
        return NULL;
    }

    unsigned char buf[READ_BUF]; //where we will be storing the temporary bytes we read
    int remaining = t->bytesToRead; //number of bytes remaining to read

    while(remaining > 0){
        int toRead;

        if(remaining > READ_BUF){
            toRead = READ_BUF;
        }
        else{
            toRead = remaining;
        }

        int n = read(fd, buf, toRead); //we read (toRead) number of bytes from fd and store them into buf

        if (n < 0) {
            perror("read (worker)");
            break;
        }

        if (n == 0) {
            printf("Unexpected EOF while reading pixel data\n");
            break;
        }

        //we go though every byte in the buffer and count how many appearences there are of each type
        for (int i = 0; i < n; i++) {
            unsigned char pixel = buf[i];
            t->localHist[pixel]++;
        }
        remaining = remaining - n;
    }

    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {

    if (argc != 4) {
        printf("Usage: %s pathToImage pathToHistogramOut numThreads\n", argv[0]);
        return 1;
    }

    //we define our arguments here
    const char *imagePath = argv[1];
    const char *outputPath = argv[2];
    int numThreads = atoi(argv[3]);

    //we handle the possible errors on the numThreads parameter
    if (numThreads <= 0) {
        printf("Error: numThreads must be > 0\n");
        return 1;
    }

    //we get the header info
    int width, height, maxval;
    int headerBytes = parse_pgm_header(imagePath, &width, &height, &maxval);

    //we handle the possible errors
    if (headerBytes <= 0) {
        printf("Error: could not parse PGM header\n");
        return 1;
    }

    if (maxval > 255) {
        printf("Error: expecting 1-byte pixels (maxval <= 255)\n");
        return 1;
    }

    //we distribute the bytes for each thread
    int totalBytes = width * height;   // 1 byte per pixel
    int base = totalBytes / numThreads;
    int rem  = totalBytes % numThreads;

    //we create the array of threads and the array of ThreadInfos
    pthread_t *threads = malloc(sizeof(pthread_t) * numThreads);
    ThreadInfo *infos = malloc(sizeof(ThreadInfo) * numThreads);

    if (threads == NULL || infos == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    int start = 0;

    //for every thread we assign the offset
    for (int i = 0; i < numThreads; i++) {

        int chunk;

        if (i < rem) {
            chunk = base + 1;
        } else {
            chunk = base;
        }

        infos[i].path = imagePath;
        infos[i].offset = headerBytes + start;
        infos[i].bytesToRead = chunk;

        start = start + chunk;
    }

    for (int i = 0; i < numThreads; i++) {

        /*
        the pthread_create function creates the id for the thread passed in the first argument
        it tells it to run the worker function and use the info given in the struct.
        */
        int rc = pthread_create(&threads[i], NULL, worker, &infos[i]);

        if (rc != 0) {
            printf("pthread_create failed\n");
            return 1;
        }
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    //now we create the global historiogram and initialize it with zeros
    unsigned int globalHist[256];
    for (int i = 0; i < 256; i++) {
        globalHist[i] = 0;
    }

    //and now for each thread we get each color and sum it to the global historiogram
    for (int t = 0; t < numThreads; t++) {
        for (int v = 0; v < 256; v++) {
            globalHist[v] = globalHist[v] + infos[t].localHist[v];
        }
    }

    //we open the fd out file and we truncate it, this means that if the file existed before we wipe it
    int fdout = open(outputPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fdout < 0) {
        perror("open (output)");
        return 1;
    }

    for (int v = 0; v < maxval; v++) {
        char line[64];
        int len = snprintf(line, sizeof(line), "%d,%u\n", v, globalHist[v]);
        write(fdout, line, len);
    }

    close(fdout);
    free(infos);
    free(threads);

    return 0;
}