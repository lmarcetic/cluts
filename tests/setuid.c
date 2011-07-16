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
#include <stdint.h>  //int8_t, uintptr_t

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
 ** setuid, getuid, setrlimit, fork, wait, waitpid, sleep, fprintf, mmap, open
 ** pthread_create, pthread_join, sem_wait, sem_post, pipe, read
 **/
 
sem_t *sem; //will point to two semaphores
int pfd[2]; //will hold file pointers for pipe-based synchronization
static void* ret_uid(void *foo);

int main()
{
    const unsigned int    max_tries = 10000;
    const struct rlimit   nproc     = {10, 10};
    const uid_t           uid       = MAXUID-4;
    unsigned int    i, j, mismatch;
    int             stat, failed_setuid, fd = open("/dev/zero", O_RDWR);
    pid_t           pid;
    pthread_t       tid[nproc.rlim_cur];
    uid_t           uids[nproc.rlim_cur];
    int8_t          ret;
    
    if (getuid() != 0) {
        fprintf(stderr, "This test must be run with uid=0 (root)\n");
        return 1;
    }
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
    
    ret = 0;
    for (j=0; j<max_tries && ret<=0; ++j) {
        pipe(pfd);
        close(pfd[1]);
        if (!fork()) {
            sem_post(&sem[1]);    //ready
            read(pfd[0], NULL, 1);//wait
            if (j%2 == 0)
                sched_yield();
            setuid(uid);
            
            return 0;
        }
        if (!(pid = fork())) {
            mismatch = ret = 0; //for this process, that is
            for (i=0; i<nproc.rlim_cur; ++i)
                pthread_create(&tid[i], NULL, ret_uid, &uids[i]);
            
            sem_post(&sem[1]);    //ready
            read(pfd[0], NULL, 1);//wait
            if (j%2 != 0)
                sched_yield();
            failed_setuid = setuid(uid);
            
            for (i=0; i<nproc.rlim_cur; ++i) {
                pthread_join(tid[i], NULL);
                if (uids[0] != uids[i])
                    mismatch = 1;
            }
            if (mismatch) {
                ret = -1;
                if (!failed_setuid) {
                    for (i=0; i<nproc.rlim_cur && uids[i]!=0; ++i);
                    if (i < nproc.rlim_cur) {
                        fprintf(stderr, "getuid() per threads(non-root='.', root='0'):\n[");
                        mismatch = 0;
                        for (i=0; i<nproc.rlim_cur; ++i) {
                            fprintf (stderr, "%c", *(".0"+(uids[i] == 0)));
                            if (uids[i] == 0)
                                ++mismatch;
                        }
                        ret = mismatch;
                        fprintf(stderr, "]\n");
                    }
                }
            }
            else
                ret = 0;
            return ret;
        }
        //wait for both proceses to become ready:
        sem_wait(&sem[1]);
        sem_wait(&sem[1]);
        //allow them to continue:
        close(pfd[0]);
        printf("Try #%i/%i\n", j+1, max_tries); //---
        
        waitpid(pid, &stat, 0);
        if(WIFEXITED(stat)) {
            if (WEXITSTATUS(stat)) //if the return value is 0, don't set ret
                ret = WEXITSTATUS(stat);
        }
        else if (ret == 0)
            ret = -2;
        wait(NULL);
    }
    if (ret) {
        if (ret == -1)
            fprintf(
                stderr,
                "getuid returned differing values in threads"
                " created by a setuid-calling process\n"
            );
        else if (ret == -2)
            fprintf(stderr, "A child process that spawned threads crashed!\n");
        else
            fprintf(
                stderr,
                "<Iteration %u> Process' call to setuid() succeeded, but %i/%i"
                " of its threads still reported getuid() == 0(root)!\n",
                j, ret, nproc.rlim_cur
            );
    }
    
    sem_post(&sem[0]);
    for (i=0; i < nproc.rlim_cur-1; ++i)
        wait(NULL);
    return (ret == 0);
}
/** waits for pdf[0] file descriptor to close,
 ** \returns a getuid value cast to void*
 **/
static void* ret_uid(void *uid)
{
    read(pfd[0], uid, 1);//wait
    *(uid_t*)uid = getuid();
    return NULL;
}
