#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common/common.h"

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

#define USLEEP_MAX 100
static void bridge_sig_jmp(int foo)
{
    return;
}
void* thread_fun(void *null)
{
    return null;
}

int main()
{
    pid_t         pid;
    pthread_t     tid;
    useconds_t    us;
    char          *s;
    unsigned int  i, nr_fun, failed;
    int           sig, ret[2], stat, pfd[2];
    const int     fd = open("/dev/zero", O_RDWR);
    unsigned char *exit_child=mmap(NULL,1,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    const char    *function[] = {
        "pthread_cancel",
        "pthread_once",
        "pthread_setspecific",
        "pthread_key_delete",
        "pthread_join",
        "pthread_atfork",
        "pthread_sigmask",
        "pthread_equal",
        "pthread_setschedprio",
        "pthread_setconcurrency",
        "pthread_create",
        "pthread_setconcurrency",
        "pthread_detach",
        "pthread_sigmask",
        "pthread_setspecific",
        "pthread_key_create",
        "pthread_rwlock_unlock",
        "pthread_kill",
    };
    struct {
        unsigned int
            kill:1,
            wait:1;
    } err, no_err={.wait=0};
    struct sigaction
        oldact,
        act = {.sa_handler = bridge_sig_jmp, .sa_flags = SA_NODEFER };
    
    
    failed = 0;
    for (i=0;  i < (nr_fun=sizeof(function)/sizeof(*function));  ++i) {
        memset(&err, 0, sizeof(err));
        pipe(pfd);
        *exit_child = 0;
        if(!(pid = fork())) {
            sigaction(SIGABRT, &act, &oldact);
            sig = 0;
            close(pfd[0]); //|
            close(pfd[1]); //| ready
            switch (i) {
                case 0:
                    while(sig != EINTR && !*exit_child)
                        sig = pthread_create(&tid, NULL, thread_fun, NULL);
                break;
                default:
                    fprintf(stdout, "BUG: test i=%d missing!\n", i);
                break;
            }
            return (sig == EINTR);
        }
        
        if (!pid) {
            fprintf(stdout, "BUG: child i=%d escaped!\n", i);
            exit(0); //exit/contain child
        }else if (pid == -1) {
            fprintf(stderr,
                "A call to fork() nr %d/%d yeilded -1 (errno=%s)!\n",
                i+1, nr_fun, (s = e_name(errno))
            );
            free(s);
            close (pfd[0]);
            close (pfd[1]);
            failed += 1;
        }else{
            close (pfd[1]);
            read(pfd[0], NULL, 1); //wait for the child to become ready
            close (pfd[0]);
            
            //Send signals in intervals of MAX_USLEEP to 0 microsecond:
            us = USLEEP_MAX;
            while(
                (ret[0] = kill(pid, SIGABRT)) == 0
                && (ret[1] = waitpid(pid, &stat, WNOHANG)) == 0
                && --us
            )
                usleep(us);
            *exit_child=1; //let the child loose from its loop (if any)
            
            //Process errors:
            s = e_name(errno);
            err.kill = (ret[0] != 0);
            err.wait = (ret[1] != 0 && ret[1] != pid);
            if (!(!memcmp(&err, &no_err, sizeof(err)))) {
                ++failed;
                if (err.kill)
                    fprintf(stderr,
                            "A call to kill() returned %d, errno=%s\n",
                            ret[0], s
                    );
                if (err.wait)
                    fprintf(stderr,
                            "A call to waitpid() returned %d, errno=%s\n",
                            ret[1], s
                    );
            }else if (WIFEXITED(stat)) {
                *ret = WEXITSTATUS(stat); //repurpose ret[0]
                if (*ret == 1) {
                    ++failed;
                    fprintf(stderr, "%s() returned EINTR\n", function[i]);
                }
                else if (*ret != 0)
                    fprintf(stdout, "BUG: child nr %d ended with status %d!\n",
                            i, *ret
                    );
            }else{
                fprintf(stderr, "A child process crashed!\n");
                if (WIFSIGNALED(stat))
                    fprintf(stderr,"\tTerminating signal: %d\n",WTERMSIG(stat));
            }
            free(s);
        }
    }
    return failed;
}
