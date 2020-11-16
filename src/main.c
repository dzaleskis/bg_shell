#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "str.h"
#include "job.h"

#define MAX_BG_JOBS 10

const job empty_job;
static job fg_job;
static job bg_jobs[MAX_BG_JOBS];
static int bg_job_count;

void cleanup(){
    for(int i = 0; i < MAX_BG_JOBS; i++){
        if(bg_jobs[i].pid > 0){
            kill(bg_jobs[i].pid, SIGHUP);
        }
    }
}

void err_exit(const char* msg){
    fputs(msg, stderr);
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

pid_t continue_job(const char* pid_str){
    pid_t parsed_pid = atoi(pid_str);
    job bg_job = get_bg_job(parsed_pid);
    if(bg_job.pid == 0){
        fputs("job with given pid not found", stderr);
        return -1;
    }
    if(bg_job.status != STOPPED){
        fputs("job can't be continued", stderr);
        return -1;
    }
    kill(bg_job.pid, SIGCONT);
    return bg_job.pid;
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

static void update_fg_job(){
    if(fg_job.pid != 0){
        job_status status = get_status(fg_job.pid, WNOHANG | WUNTRACED);
        if(status != NO_CHANGE){
            fg_job.status = status;
            if(fg_job.status == STOPPED){
                print_job(fg_job);
                add_bg_job(fg_job);
                fg_job = empty_job;
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
    update_fg_job();
    update_bg_jobs();
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


void setup_handler(int signo, sig_t handler){
    if (signal(signo, handler) == SIG_ERR) {
        err_exit("error setting signal handler\n");
    }
}

int print_shell(){
    printf("bg_shell$ ");
    return 1;
}

int main(int argc, char *argv[]){

    setup_handler(SIGTSTP, sigtstp_handler);
    setup_handler(SIGCHLD, sigchld_handler);
    setup_handler(SIGTERM, sigterm_handler);

    char* line = NULL;
    char** args = NULL;
    size_t buflen = 0;
    ssize_t len = 0;

    while(print_shell() && (len = getline(&line, &buflen, stdin)) > 0) {
        if(args != NULL){
            free(args);
            args = NULL;
        }
        int args_count = 0;
        args = split(line, " \r\n", &args_count);
        if(args_count == 0){
            continue;
        }

        int run_bg = 0;
        if(strcmp(args[args_count-1], "&") == 0){
            run_bg = 1;
            args[args_count-1] = NULL;
        }

        char* command = args[0];
        // process builtin commands here
        if(strcmp(command, "exit") == 0){
            break;
        }
        if(strcmp(command, "jobs") == 0){
            report_bg_jobs();
            continue;
        }
        if(strcmp(command, "fg") == 0){
            if(args_count == 2){
                pid_t job_pid = continue_job(args[1]);
                if(job_pid <= 0){
                    continue;
                }
                remove_bg_job(job_pid);
                job new_fg_job = {RUNNING, job_pid};
                fg_job = new_fg_job;
                while(fg_job.status == RUNNING){
                    sleep(1);
                }
            }
            else{
                puts("format: fg {pid}\n");
            }
            continue;
        }
        if(strcmp(command, "bg") == 0){
            if(args_count == 2){
                pid_t job_pid = continue_job(args[1]);
                if(job_pid <= 0){
                    continue;
                }
                setpgid(job_pid, job_pid);
                continue;
            }
            else{
                puts("format: bg {pid}");
            }
            continue;
        }

        pid_t child_pid = fork();
        if(child_pid == 0){
            // child process
            if(execvp(args[0], args) < 0){
                perror("error ");
                exit(EXIT_FAILURE);
            }
        }
        else{
            // parent process
            if(run_bg){
                // detach background process from the shell process group
                setpgid(child_pid, child_pid);
                job bg_job = {RUNNING, child_pid};
                add_bg_job(bg_job);
            }
            else{
                job new_fg_job = {RUNNING, child_pid};
                fg_job = new_fg_job;
                while(fg_job.status == RUNNING){
                    sleep(1);
                }
            }
        }
    }

    if(args != NULL){
        free(args);
    }
    clean_exit();
}
