#include <string.h>

#ifndef STRH
#define STRH

#define INIT_BUFSIZE 32

char** split(char* str, const char* delim, int* result_count){
    int i = 0;
    int current_size = INIT_BUFSIZE;
    char** tokens = malloc(current_size * sizeof(char*));

    char* current_token;
    current_token = strtok(str, delim);

    while((current_token != NULL){
        tokens[i] = current_token;
        i++;
        if(i >= current_size){
            current_size += INIT_BUFSIZE;
            tokens = realloc(tokens, current_size * sizeof(char*));
            if(!tokens){
                err_exit("realloc failed");
            }
        }
        current_token = strtok(NULL, delim);
    }

    tokens[i] = NULL;
    (*result_count) = i;
    return tokens;
}

#endif