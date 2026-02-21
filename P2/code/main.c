#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "splitCommand.h"
#include "circularBuffer.h"

static char *read_line(CircularBuffer *cb, int *reachedEOF){
    while(1){
        //we check if we have a full line and if we have reached EOF
        int len = buffer_size_next_element(cb, '\n',*reachedEOF);

        if(len > 0){ //if we have a full line len equals the number of characters of the line
            char *line = malloc(len + 1); 
            if(!line){
                return NULL;
            }
            for(int i = 0; i < len; i++){
                //we store in line variable the whole line we just read by popping it out the buffer
                line[i] = (char)buffer_pop(cb);
            }
            line[len] = '\0';
            return line;
        }

        if(*reachedEOF){return NULL;}

        //if we don't have a full line yet we read 256 bytes more of data and push it to the buffer
        unsigned char tmp[256];
        ssize_t r = read(STDIN_FILENO, tmp, sizeof(tmp));

        if(r == 0){
            *reachedEOF = 1;
        }
        else if(r < 0){
            return NULL;
        }
        else{
            for(ssize_t i = 0; i < r; i++){
                buffer_push(cb,tmp[i]);
            }
        }

    }
}
//little function to remove the newline at the end of the command to compare it correctly
static void strip_newline(char *s) {
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n'){
        s[n - 1] = '\0';
    }
}



int main(){

    //definition of our buffer
    CircularBuffer cb;
    buffer_init(&cb, 4096);
    int reachedEOF = 0;

    while(1){
        char *mode = read_line(&cb, &reachedEOF);
        if (!mode) break;
        
        strip_newline(mode); //we remove the newline to compare

        //we check different modes
        if(strcmp(mode, "EXIT") == 0){
            free(mode);
            break;
        }

        if(strcmp(mode,"SINGLE") == 0 || strcmp(mode,"CONCURRENT") == 0){
            char *cmdline = read_line(&cb,&reachedEOF); 
            //we read the command line
            if(!cmdline){
                free(mode);
                break;
            }
            char **argv = split_command(cmdline);
            if (!argv || !argv[0]) {
                free(argv);
                free(cmdline);
                continue;
            }

            pid_t pid = fork(); //we fork the process
            if(pid == 0){
                execvp(argv[0],argv);//we convert the child process into the command we entered
                perror("execvp");
                _exit(1);
            }
            //we only wait in single mode
            if (pid > 0 && strcmp(mode, "SINGLE") == 0) {
                waitpid(pid, NULL, 0);
            }
            else if (pid < 0) {
                perror("fork");
            }

            free(argv);
            
            

            free(cmdline);
        }
        if(strcmp(mode,"PIPE") == 0){
            char *cmd1 = read_line(&cb, &reachedEOF);
            if (!cmd1) { free(mode); break; }

            char *cmd2 = read_line(&cb, &reachedEOF);
            if (!cmd2) { free(cmd1); free(mode); break; }

            char **argv1 = split_command(cmd1);
            char **argv2 = split_command(cmd2);

            if (!argv1 || !argv1[0] || !argv2 || !argv2[0]) {
                free(argv1);
                free(argv2);
                free(cmd1);
                free(cmd2);
                free(mode);
                continue;
            }
            
            //we create the pipe, fds has first element as read end and second as write end
            int fds[2];
            //we just check if the pipe was correctly created
            if (pipe(fds) < 0) {
            perror("pipe");
            free(argv1); free(argv2);
            free(cmd1); free(cmd2);
            free(mode);
            continue;
            }

            pid_t p1 = fork();
            if(p1 == 0){
                //we change the output from the standard output to the write pipe.
                dup2(fds[1], STDOUT_FILENO);
                close(fds[0]);
                close(fds[1]);
                //we execute the command
                execvp(argv1[0],argv1);
                perror("execvp");
                _exit(1);
            }
            pid_t p2 = fork();
            if(p2 == 0){
                //we change the input to the read pipe instead of the standard input
                dup2(fds[0], STDIN_FILENO);
                close(fds[0]);
                close(fds[1]);
                execvp(argv2[0],argv2);
                perror("execvp");
                _exit(1);
            }

            close(fds[0]);
            close(fds[1]);
            if (p1 > 0) waitpid(p1, NULL, 0);
            if (p2 > 0) waitpid(p2, NULL, 0);
            free(argv1);
            free(argv2);
            free(cmd1);
            free(cmd2);           
            
        }

        //we check for any child process already finished and we clean it.
        while (waitpid(-1, NULL, WNOHANG) > 0) {}
        free(mode);
    }
    buffer_deallocate(&cb);


}