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
 ** open, close, fprintf, setrlimit, mmap, sem_init, sem_post, sem_wait, pipe,
 ** fork, waitpid, wait, sched_yield, read, pthread_create, pthread_join
 **/
 
sem_t *sem; //will point to a shared semaphore
int pfd[2]; //will hold file pointers for pipe-based process synchronization
static void* set_uid(void *uid);

int main()
{
    //tweaking done here:
    const uid_t           uid        = MAXUID-4;
    const struct rlimit   nproc      = {78, 78};
    const unsigned int    nr_threads = nproc.rlim_cur,
                          nr_tries   = 10,
                          max_spins  = 1000;
    const int             fd         = open("/dev/zero", O_RDWR);
    uid_t           uids[nr_threads];
    pthread_t       tid [nr_threads];
    unsigned int    i, j, k, mismatch, *foo;
    int             stat, failed_setuid;
    pid_t           pid;
    int8_t          ret;
    
    if (getuid() != 0) {
        fprintf(stderr, "This test must be run with uid=0 (root)\n");
        return 1;
    }
    setrlimit(RLIMIT_NPROC, &nproc);
    foo = mmap(NULL, sizeof(int),   PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sem = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sem_init(sem, 1, 0);
    
    ret = 0;
    for (j=0; j<nr_tries && ret<=0; ++j) {
        for (k=0; k<max_spins && ret<=0; ++k) {
            pipe(pfd);
            close(pfd[1]);
            //spawn slot squatting processes:
            for (i=0; i<nproc.rlim_cur && !fork(); ++i) {
                setuid(uid);
                sem_post(sem);        //ready
                read(pfd[0], NULL, 1);//wait
                if (j%2 == 0)
                    sched_yield();
                
                return 0;
            }
            //spawn the threaded process:
            if (!(pid = fork())) {
                mismatch = ret = 0;   //reuse
                for (i=0; i<nproc.rlim_cur; ++i)
                    pthread_create(&tid[i], NULL, set_uid, &uids[i]);
                
                sem_post(sem);        //ready
                read(pfd[0], NULL, 1);//wait
                for(i=0; i<k; ++i)
                    *foo ^= i;        //waste cycles
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
                            fprintf(stderr,
                                "getuid per thread(non-root='.', root='0'): \n["
                            );
                            mismatch = 0; //reuse!
                            for (i=0; i<nproc.rlim_cur; ++i) {
                                fprintf(stderr, "%c", *(".0"+(uids[i] == 0)));
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
            sem_wait(sem);
            sem_wait(sem);
            //allow them to continue:
            close(pfd[0]);
            //wait for them to finish, evaluate threaded process' return value:
            waitpid(pid, &stat, 0);
            if(WIFEXITED(stat)) {
                if (WEXITSTATUS(stat)) //if 0 is returned, don't change ret
                    ret = WEXITSTATUS(stat);
            }
            else if (ret == 0)
                ret = -2;
            for (i=0; i<nproc.rlim_cur; ++i)
                wait(NULL);
        }
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
                "<Try %u, iteration %u> Process' call to setuid() succeeded,"
                " but %i/%i of its threads still reported getuid() == 0(root)!"
                "\n",
                j, k, ret, (int)nproc.rlim_cur
            );
    }
    fprintf(stdout,"%s",("\0\0"+(*foo%2)));//necessary for the cycle-wasting loop
    return (ret == 0);
}
//waits for pfd[0] to close, then sets a void* to the value of getuid
static void* set_uid(void *uid)
{
    read(pfd[0], uid, 1);//wait
    *(uid_t*)uid = getuid();
    return NULL;
}
