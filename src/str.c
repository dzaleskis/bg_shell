#include <string.h>
#include <stdlib.h>
#include "str.h"

char** split(char* str, const char* delim, int* result_count){
    int i = 0;
    int current_size = INIT_BUFSIZE;
    char** tokens = malloc(current_size * sizeof(char*));

    char* current_token;
    current_token = strtok(str, delim);

    while(current_token != NULL){
        tokens[i] = current_token;
        i++;
        if(i >= current_size){
            current_size += INIT_BUFSIZE;
            tokens = realloc(tokens, current_size * sizeof(char*));
        }
        current_token = strtok(NULL, delim);
    }

    tokens[i] = NULL;
    (*result_count) = i;
    return tokens;
}