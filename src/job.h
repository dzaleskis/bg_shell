

#ifndef JOBH
#define JOBH

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

#endif