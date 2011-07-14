#define _XOPEN_SOURCE 1
#ifndef MAXUID
    #define MAXUID 2000
#endif
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/resource.h> //RLIMIT_NPROC
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/select.h>

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

/** \file tests the setuid/getuid combo in a threaded environment
 ** depends:
 ** setuid, getuid, setrlimit, fork, wait, sleep, fprintf, mmap, open
 ** pthread_create, pthread_join, sem_wait, sem_post
 **/
 
sem_t *sem; //will point to two semaphores
int pfd[2]; //will hold file pointers for pipe-based synchronization
void* return_getuid(void *foo);

int main()
{
    unsigned int
        max_tries = 30,
        max_delay = 1000;
    struct rlimit
        nproc = {10, 10};
    unsigned int    i, j, d;
    int             stat, failed, fd = open("/dev/zero", O_RDWR);
    uid_t           *ret[2], uid = MAXUID-4;
    pthread_t       tid[nproc.rlim_cur];
    pid_t           pid;
    
    
    pipe(pfd);
    sem = mmap(NULL, sizeof(sem_t)*2, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sem_init(&sem[0], 1, 0);
    sem_init(&sem[1], 1, 0);
    setrlimit(RLIMIT_NPROC, &nproc);
    
    for (i=0; i < nproc.rlim_cur-1; ++i) {
        if (!fork()) {
            setuid(uid);
            sem_wait(&sem[0]);
            sem_post(&sem[0]);
            return 0;
        }
    }
    failed = 0;
    for (j=0; j<max_tries && !failed; ++j) {
        for (d=0; d<max_delay && !failed; ++d) {
            if (!fork()) {
                setuid(uid);
                sem_post(&sem[1]);    //ready
                read(pfd[0], NULL, 0);//wait
                for(i=d; i<max_delay; i++)
                    sched_yield();
                return 0;
            }
            if (!(pid = fork())) {
                for (i=0; i<nproc.rlim_cur; ++i)
                    pthread_create(&tid[i], NULL, return_getuid, NULL);
                sem_post(&sem[1]);    //ready
                setuid(uid);
                for(i=0; i<d; i++)
                    sched_yield();
                
                for (i=1; i<nproc.rlim_cur-1; ++i)
                        pthread_join(tid[i], NULL);
                pthread_join(tid[0], (void **)&ret[0]);
                pthread_join(tid[i], (void **)&ret[1]);
                return ret[1]-ret[0];
            }
            //wait for both proceses to become ready:
            sem_wait(&sem[1]);
            sem_wait(&sem[1]);
            //allow them to continue:
            close(pfd[0]);
            close(pfd[1]);
            
            waitpid(pid, &stat, 0);
            if(WIFEXITED(stat))
                failed |= WEXITSTATUS(stat);
            else
                failed = 2;
            wait(NULL);
        }
    }
    
    if (failed == 1)
        fprintf(stderr, "setuid() fails in a threaded environment:"
            " NPROC limit was reached, and then setuid called in the same time"
            " as one of the slot-holding processes exited. Threads in the"
            " setuid-calling program got contradictory getuid() results."
        );
    else if (failed == 2)
        fprintf(stderr, "A child process that spawned threads crashed!\n");
    
    sem_post(&sem[0]);
    for (i=0; i < nproc.rlim_cur-1; ++i)
        wait(NULL);
    return failed;
}
void* return_getuid(void *foo)
{
    read(pfd[0], NULL, 0);//wait
    return &(uid_t []){getuid()};
}
