#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "parsePGM.h"
#include "producer.c"

#define BLOCK_SIZE 16384 

unsigned char **buffer;
int buffer_size;

int producers_finished = 0; 

void *producer(void *arg) {
    int fd = open(input_path, O_RDONLY);
    if (fd < 0) pthread_exit(NULL);
    while (1){

    }

}

void *consumer (void *arg){
    while (1){

    }
}

int main(int argc, char *argv[]) {

    input_path = argv[1];
    char *output_path = argv[2];
    int n_producers = atoi(argv[3]);
    int n_consumers = atoi(argv[4]);
    buffer_size = atoi(argv[5]);

    int width, height, maxval;
    int headerBytes = parse_pgm_header(imagePath, &width, &height, &maxval);
    buffer = malloc(sizeof(unsigned char*) * buffer_size);
    bytes_in_block = malloc(sizeof(int) * buffer_size);

    pthread_t prods[n_producers], cons[n_consumers];

    for (int i = 0; i < n_producers; i++){
        pthread_create(&prods[i], NULL, producer, NULL);
    }
    for (int i = 0; i < n_consumers; i++){ 
        pthread_create(&cons[i], NULL, consumer, NULL);
    }
    for (int i = 0; i < n_producers; i++){
        pthread_join(prods[i], NULL);
    }


}