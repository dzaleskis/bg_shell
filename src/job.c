#include "job.h"
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>

job_status get_status(pid_t job_pid, int flags){
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

void print_job(job to_print){
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