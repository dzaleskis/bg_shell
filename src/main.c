#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define ARGS_LENGTH 20
#define MAX_BG_JOBS 10

// global vars are zero-initialized by default
static pid_t current_fg_pid;
static pid_t bg_pids[MAX_BG_JOBS];
static int bg_pid_count;

static void add_bg_pid(pid_t bg_pid){
    if(bg_pid_count == MAX_BG_JOBS){
        fputs("error: maximum number of background jobs reached.\n", stderr);
        // TODO: call cleanup here
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_pids[i] == 0){
            bg_pids[i] = bg_pid;
            break;
        }
    }
    bg_pid_count++;
}

static void remove_bg_pid(pid_t bg_pid){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_pids[i] == bg_pid){
            bg_pids[i] = 0;
            bg_pid_count--;
            break;
        }
    }
}

static void report_status(pid_t bg_pid){
    int status;
    int wait_result = waitpid(bg_pid, &status, WNOHANG);

    printf("id: %i | status: ", bg_pid);

    if(wait_result == -1){
        printf("unknown error\n");
    }
    else if(wait_result == 0){
        printf("running\n");
    }
    else if (WIFEXITED(status)) {
        printf("exited, status=%d\n", WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        printf("killed by signal %d\n", WTERMSIG(status));
    }
    else if (WIFSTOPPED(status)) {
        printf("stopped by signal %d\n", WSTOPSIG(status));
    }
    else if (WIFCONTINUED(status)) {
        printf("continued\n");
    }
}

static void print_bg_jobs(){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_pids[i] > 0 ){
            report_status(bg_pids[i]);
        }
    }
}

static void sigchld_handler(int signo) {
    printf("signal %i caught in sigchld handler\n", signo);
    // some child has terminated, determine which and clear it from background job table
    pid_t child_pid;
    int status;
    // WNOHANG doesn't block, but doesn't catch stopped processes
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        remove_bg_pid(child_pid);
    }
}

static void sigstp_handler(int signo) {
    printf("signal %i caught in sigstp handler\n", signo);
    // if a process is currently running in the foreground, stop it and add to background
    if(current_fg_pid > 0){
        kill(current_fg_pid, SIGTSTP);
        printf("[%i]: stopped\n", current_fg_pid);
        add_bg_pid(current_fg_pid);
        current_fg_pid = 0;
    }
}

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

    if (signal(SIGTSTP, sigstp_handler) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }
    if (signal(SIGCHLD, sigchld_handler) == SIG_ERR) {
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }

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
        if(strcmp(command, "jobs") == 0){
            print_bg_jobs();
        }
        else{
            printf("%i", strcmp(command, "jobs"));
        }

        int run_bg = 0;
        for(int i = 0; i < token_count; i++){
            if(i == token_count-1 && strcmp(tokens[i], "&") == 0){
                run_bg = 1;
                break;
            }
            args[i] = tokens[i];
        }

        pid_t child_pid = fork();
        if(child_pid == 0){
            // child process
            // if(run_bg){
            //     setpgid(0, 0);
            // }
            if(execvp(args[0], args) < 0){
                if(errno == 2){
                    printf("command not found\n");
                }
                else{
                    printf("error executing process, code: %i\n", errno);
                }
                exit(EXIT_FAILURE);
            }
        }
        else{
            // main process
            if(run_bg){
                printf("%i \n", child_pid);
            }
            else{
                int status;
                pid_t wait_result;
                current_fg_pid = child_pid;
                // on success, returns the process ID of the child whose state has changed
                // wuntraced also returns if child has stopped
                wait_result = waitpid(child_pid, &status, WUNTRACED);
                if (wait_result == -1) {
                    // error!
                    printf("error waiting on process %i. code: %i \n", child_pid, errno);
                }
                current_fg_pid = 0;
            }
        }
    }

    return 0;
}
