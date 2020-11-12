#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define ARGS_LENGTH 10

void err_exit(const char* msg){
    perror(msg);
    exit(1);
}

void split(char* str, const char* delim, char** output, int* output_size){
    int i = 0;
    output[i] = strtok(str, delim);

    while( output[i] != NULL ) {
        char* new_token = strtok(NULL, delim);
        if(new_token == NULL){
            break;
        }
        i++;
        output[i] = new_token;
    }
    (*output_size) = i + 1;
}

int main(int argc, char *argv[]){

    char* line = NULL;
    size_t buflen = 0;
    ssize_t len = 0;

    while ((len = getline(&line, &buflen, stdin)) > 0) {
        if(line[len-1] == '\n'){
            if(len == 1){
                continue;
            }
            line[--len] = '\0';
        }

        char* args[ARGS_LENGTH];
        memset(args, 0, ARGS_LENGTH);

        char* tokens[ARGS_LENGTH];
        int token_count = 0;
        split(line, " ", tokens, &token_count);

        char* command = tokens[0];

        // process builtin commands here, not just exit
        if(strcmp(command, "exit") == 0){
            exit(0);
        }

        int run_bg = 0;
        for(int i = 0; i < token_count; i++){
            if(i == token_count-1 && strcmp(tokens[i], "&") == 0){
                run_bg = 1;
                break;
            }
            args[i] = tokens[i];
        }

        pid_t child_process_id = fork();
        if(child_process_id == 0){
            // child process
            if(run_bg){
                setpgid(0, 0);
            }
            if(execvp(args[0], args) < 0){
                // if errno is 2, can print that program is not found
                if(errno == 2){
                    printf("command not found\n");
                }
                else{
                    printf("error executing process, code: %i\n", errno);
                }
                exit(1);
            }
        }
        else{
            // main process
            if(run_bg){
                printf("%i \n", child_process_id);
            }
        }
    }

    return 0;
}
