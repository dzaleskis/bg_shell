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

enum job_status{
    NOT_FOUND = 0,
    STOPPED = 1,
    EXITED = 2,
    SIGNALED = 3,
    CONTINUED = 4,
    RUNNING = 5,
    NO_CHANGE = 6
};
typedef enum job_status job_status;

struct job{
    job_status status;
    pid_t pid;
    pid_t pgid;
};
typedef struct job job;

// global vars are zero-initialized by default
const job empty_job;
static job fg_job;
static job bg_jobs[MAX_BG_JOBS];
static int bg_job_count;


static void cleanup(){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid > 0){
            kill(bg_jobs[i].pid, SIGTERM);
        }
    }
    
    sleep(1);
    int status;
    
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid > 0){
            kill(bg_jobs[i].pid, SIGTERM);
        }
    }
}

void err_exit(const char* msg){
    perror(msg);
    cleanup();
    exit(EXIT_FAILURE);
}

void clean_exit(){
    cleanup();
    exit(0);
}

static void add_bg_job(job bg_job){
    if(bg_job_count == MAX_BG_JOBS){
        err_exit("maximum background job count reached");
    }
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid == 0){
            bg_jobs[i] = bg_job;
            bg_job_count++;
            return;
        }
    }
}

static void remove_bg_job(pid_t bg_pid){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid == bg_pid){
            bg_jobs[i] = empty_job;
            bg_job_count--;
            break;
        }
    }
}

static job get_bg_job(pid_t bg_pid){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid == bg_pid){
            return bg_jobs[i];
        }
    }
    return empty_job;
}


static job_status get_status(pid_t job_pid, int flags){
    int status;
    int wait_result = waitpid(job_pid, &status, flags);

    if((flags && WNOHANG) && wait_result == 0){
        return NO_CHANGE;
    }
    else if (WIFEXITED(status)) {
        return EXITED;
    }
    else if ((flags && WUNTRACED) && WIFSTOPPED(status)) {
        return STOPPED;
    }
    else if (WIFSIGNALED(status)) {
        return SIGNALED;
    }
    else if ((flags && WCONTINUED) && WIFCONTINUED(status)) {
        return CONTINUED;
    }
    else{
        return NOT_FOUND;
    }
}

static void print_job(job to_print){
    printf("[%i]: ", to_print.pid);
    switch (to_print.status){
    case RUNNING:
        printf("Running \n");
        break;
    case STOPPED:
        printf("Stopped \n");
        break;
    case CONTINUED:
        printf("Continued \n");
        break;
    case EXITED:
        printf("Exited \n");
        break;
    case SIGNALED:
        printf("Terminated by signal \n");
        break;
    default:
        printf("Error: couldn't determine status \n");
        break;
    }
}

static void update_bg_jobs(){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid != 0){
            job_status status = get_status(bg_jobs[i].pid, WNOHANG | WUNTRACED | WCONTINUED);
            if(status != NO_CHANGE){
                bg_jobs[i].status = status;
            }
        }
    }
}

static void report_bg_jobs(){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid != 0){
            print_job(bg_jobs[i]);
            if(bg_jobs[i].status == SIGNALED || bg_jobs[i].status == EXITED || bg_jobs[i].status == NOT_FOUND){
                remove_bg_job(bg_jobs[i].pid);
            }
        }
    }
}

static void sigchld_handler(int signo) {
    // some child has changed status, determine which
    // if it was foreground process, update its status
    job_status fg_job_status;
    if(fg_job.pid != 0 && (fg_job_status = get_status(fg_job.pid, WNOHANG | WUNTRACED)) != RUNNING){
        fg_job.status = fg_job_status;
        if(fg_job_status == STOPPED){
            print_job(fg_job);
            add_bg_job(fg_job);
            fg_job = empty_job;
        }
    }
    else{
        // if it was a background process, wait on it & update its status
        update_bg_jobs();
    }
}

static void sigtstp_handler(int signo) {
    // if a process is currently running in the foreground, stop it and add to background
    if(fg_job.pid != 0){
        kill(fg_job.pid, SIGSTOP);
    }
}

static void sigterm_handler(int signo) {
    cleanup();
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

void setup_handler(int signo, sig_t handler){
    if (signal(signo, handler) == SIG_ERR) {
        err_exit("error setting signal handler\n");
    }
}

int print_shell(){
    printf("@bg_shell$ ");
    return 1;
}

int main(int argc, char *argv[]){

    setup_handler(SIGTSTP, sigtstp_handler);
    setup_handler(SIGCHLD, sigchld_handler);
    setup_handler(SIGTERM, sigterm_handler);

    char* line = NULL;
    size_t buflen = 0;
    ssize_t len = 0;

    while(print_shell() && (len = getline(&line, &buflen, stdin)) > 0) {
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
        // process builtin commands here
        if(strcmp(command, "exit") == 0){
            clean_exit();
        }
        if(strcmp(command, "jobs") == 0){
            report_bg_jobs();
            continue;
        }
        if(strcmp(command, "fg") == 0){
            if(token_count == 2){
                int parsed_pid = atoi(tokens[1]);
                job bg_job = get_bg_job(parsed_pid);
                if(bg_job.pid == 0){
                    printf("job with given pid not found\n");
                    continue;
                }
                if(bg_job.status != STOPPED){
                    printf("job can't be continued\n");
                    continue;
                }
                kill(bg_job.pid, SIGCONT);
                remove_bg_job(bg_job.pid);
                job new_fg_job = {RUNNING, bg_job.pid, 0};
                fg_job = new_fg_job;
                while(fg_job.status == RUNNING){
                    sleep(1);
                }
                continue;
            }
            else{
                printf("format: fg {pid}\n");
            }
            continue;
        }
        if(strcmp(command, "bg") == 0){
            if(token_count == 2){
                int parsed_pid = atoi(tokens[1]);
                job bg_job = get_bg_job(parsed_pid);
                if(bg_job.pid == 0){
                    printf("job with given pid not found\n");
                    continue;
                }
                if(bg_job.status != STOPPED){
                    printf("job can't be continued\n");
                    continue;
                }
                kill(bg_job.pid, SIGCONT);
                setpgid(bg_job.pid, bg_job.pid);
                continue;
            }
            else{
                printf("format: bg {pid}\n");
            }
            continue;
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
            if(execvp(args[0], args) < 0){
                if(errno == 2){
                    printf("command not found\n");
                }
                else{
                    printf("error executing process, code: %i\n", errno);
                }
                cleanup();
                exit(EXIT_FAILURE);
            }
        }
        else{
            // main process
            if(run_bg){
                // detach background process from the shell process group
                setpgid(child_pid, child_pid);
                job bg_job = {RUNNING, child_pid, child_pid};
                add_bg_job(bg_job);
            }
            else{
                job new_fg_job = {RUNNING, child_pid, 0};
                fg_job = new_fg_job;
                while(fg_job.status == RUNNING){
                    sleep(1);
                }
            }
        }
    }

    return 0;
}
