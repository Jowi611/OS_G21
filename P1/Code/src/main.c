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
    
    if (argc != 4){ //we want 3 arguments + the program name, if not we show error message
        //len is an int that returns the number of charachters in the string
        //the function sprintf also saves the string in the out_buff variable
        len = sprintf(out_buff, "Format is: binary|text pathToFile sizeOfTheBuffer\n");
        write(2, out_buff, len); //the 2 means that the system call is returning an error.
        return 1;
    }
    
    char *mode = argv[1]; //we read the array of arguments and save the second one to mode
    char *path = argv[2]; //we save the third argument to path
    //atoi means ascii to integer so we convert the string argument to an integer to declare the real buffer_size
    int buffer_size = atoi(argv[3]); 

    int fd = open(path, O_RDONLY, 0640); //we open the file in read only mode
    
    if (fd < 0) { // error message for when we can't open the file
        len = sprintf(out_buff, "Error: Could not open file %s \n", path);
        write(2, out_buff, len);
        return 1;
    }
    if (strcmp(mode, "binary") == 0){ //if mode == binary we use the process_binnary function
        process_binary(fd, buffer_size);
    }
    else if (strcmp(mode, "text") == 0){ //if mode == text we use the process_txt function
        process_txt(fd, buffer_size);
    }
    else{ // else we return an error message
        len = sprintf(out_buff, "Error; Wrong file format, binary or text\n");
        write(2, out_buff, len);
        close (fd);
        return 1;
    }
    
    close (fd);
    return 0;
}

void process_binary(int fd, int buffer_size){ //reads file of 1 or 0
    //we adjust the buffer size so it can only fit full integers
    int multiple = (buffer_size/sizeof(int))*sizeof(int); 
    //we create an array of ints with the size of the buffer
    int *buffer = (int*)malloc(multiple);
    //we set the counters
    int total =0;
    int bytes = 0;
    while ((bytes = read(fd, buffer, multiple )) > 0){ //we save in buffer multiple number of ints
        int num = (int)bytes / (int) (sizeof(int)); // the total number of integers read.
        for (int i=0; i<num; i++){ //for each integer we add it to the total variable
            total += buffer[i];
        }

    }
    char result[128]; //we define the result string
    int result_len = sprintf(result, "%d\n",total); //we format the int total to a string result and the funtion returns the size of the string formatted
    write (1, result, result_len);   // we write the result with the size that we get with the sprintf function
    
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
        int free = buffer_free_bytes (&buffer); //the number of bytes that are free
        if (free > 0){ // if there are free bytes
            //we store in bytes the number of bytes read and in linear_buff the value of (free) integers read
            bytes = read (fd, linear_buf, free);
            if (bytes <= 0){ 
                //if bytes == 0 it means we reached eof and if it's -1 we have an error anyways we stop reading the file
                eof =1;
            }
            else{
                for (int i=0; i<bytes;i++){ // we push into the cicular buffer the bytes we just read
                    buffer_push(&buffer, linear_buf[i]);
                }
            }
        }
        int len;
        while ((len=buffer_size_next_element(&buffer, ',', eof))!=-1){ //while we find elements
            char c[buffer_size+1];
            int j=0;
            for (; j<len;j++){ //we start getting one by one the chars of the circular buffer
                unsigned char val = buffer_pop(&buffer); 
                if (val == ',') { // if it's a , we stop
                    break;
                } 
                c[j] =(char) val; //we store the value read in the c string
            }
            c[j]= '\0'; //at the end finally we add a full stop
            total += atoi(c); //we convert each part of characters between , .... , to integer and add it to total.
        }
    }
    char out[128];
    int out_len = sprintf(out, "%d\n", total);
    write(1, out, out_len);

    free(linear_buf);
    buffer_deallocate(&buffer);

    
}
