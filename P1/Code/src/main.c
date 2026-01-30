#include "circularBuffer.h"
#include "circularBuffer.c"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void process_binary(int fd, int buffer_size);
void process_txt(int fd, int buffer_size);

int main(int argc, char *argv[]){
    char out_buff[256];
    int len;
    
    if (argc != 4){
        len = sprintf(out_buff, "Format is: binary|text pathToFile sizeOfTheBuffer\n");
        write(2, out_buff, len);
        return 1;
    }
    
    char *mode = argv[1];
    char *path = argv[2];
    int buffer_size = atoi(argv[3]);

    int fd = open(path, O_RDONLY, 0640);
    
    if (fd < 0) {
        len = sprintf(out_buff, "Error: Could not open file %s \n", path);
        write(2, out_buff, len);
        return 1;
    }
    if (strcmp(mode, "binary") == 0){
        process_binary(fd, buffer_size);
    }
    else if (strcmp(mode, "text") == 0){
        process_txt(fd, buffer_size);
    }
    else{
        len = sprintf(out_buff, "Error; Wrong file format, binary or text\n");
        write(2, out_buff, len);
        close (fd);
        return 1;
    }
    
    close (fd);
    return 0;
}

void process_binary(int fd, int buffer_size){
    int multiple = (buffer_size/sizeof(int))*sizeof(int);
    int *buffer = (int*)malloc(multiple);
    int total =0;
    int bytes = 0;
    while ((bytes = read(fd, buffer, multiple )) > 0){
        int num = (int)bytes / (int) (sizeof(int));
        for (int i=0; i<num; i++){
            total += buffer[i];
        }

    }
    char result[128];
    int result_len = sprintf(result, "%d\n",total);
    write (1, result, result_len);
    
    free(buffer);

}

void process_txt(int fd, int buffer_size){
    CircularBuffer buffer;
    buffer_init(&buffer, buffer_size);

    int total =0;
    unsigned char *linear_buf = malloc(buffer_size);
    int bytes =0;
    int eof = 0;

    while (!eof){
        int free = buffer_free_bytes (&buffer);
        if (free > 0){
            bytes = read (fd, linear_buf, free);
            if (bytes <= 0){
                eof =1;
            }
            else{
                for (int i=0; i<bytes;i++){
                    buffer_push(&buffer, linear_buf[i]);
                }
            }
        }
        int len;
        while ((len=buffer_size_next_element(&buffer, ',', eof))!=-1){
            char c[buffer_size+1];
            int j=0;
            for (; j<len;j++){
                unsigned char val = buffer_pop(&buffer); 
                if (val == ',') {
                    break;
                } 
                c[j] =(char) val;
            }
            c[j]= '\0';
            total += atoi(c);

        }
    }
    char out[128];
    int out_len = sprintf(out, "%d\n", total);
    write(1, out, out_len);

    free(linear_buf);
    buffer_deallocate(&buffer);

    
}
