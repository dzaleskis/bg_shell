#include <sys/types.h>

#ifndef JOBH
#define JOBH

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
};
typedef struct job job;

job_status get_status(pid_t job_pid, int flags);
void print_job(job to_print);

#endif