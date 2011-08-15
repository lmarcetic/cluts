#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <sched.h>
#include <stdint.h>
#include <setjmp.h>
#include "common/common.h"
#include <pthread.h>

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */
 
/**
 ** \file
 ** Tests that EINTR isn't returned by interrupted pthread_* functions for which
 ** this is a specified behavior
 ** tests: pthread_create, pthread_cancel, pthread_once, pthread_setspecific,
 ** pthread_key_delete, pthread_join, pthread_atfork, pthread_sigmask,
 ** pthread_setschedprio, pthread_setconcurrency, pthread_detach,
 ** pthread_key_create pthread_rwlock_unlock, pthread_kill
 ** depends: open,pipe,close, mmap, sigaction, kill,waitpid,nanosleep, fprintf,
 **          pause, pthread_cleanup_push, pthread_cleanup_pop, setjmp,longjmp
 **          sched_getscheduler,sched_get_priority_min,pthread_setschedprio,
 **          pthread_key_create, pthread_getconcurrency
 **/

#define NANOSLEEP_MAX 100000  /* a multiple of 100 (that's the loop step) */
volatile char blocked;        //when unset, signals child that it may exit
jmp_buf env;                  //context storage for sigset and longjmp
int pfd[2];                   //pipe file descriptors

static void handle(int sig);
static void block(void);
static void* thread(void *fun);

#define WRAP_START                                                             \
    int err = 0;                                                               \
    if (!setjmp(env)) {                                                        \
        blocked = 1;                                                           \
        close(pfd[1]); /*ready*/                                               
#define WRAP_END                                                               \
        blocked = 0;   /*reaching this = failure, see handle() for more info*/ \
    }                                                                          \
    return err;

static int wrap_pthread_create(pthread_t *restrict thread,
                               const pthread_attr_t *restrict attr,
                               void *(*start_routine)(void*),
                               void *restrict arg);
static int wrap_pthread_cancel(pthread_t thread);
static int wrap_pthread_once(pthread_once_t *once_control,
                        void (*init_routine)(void));
static int wrap_pthread_setspecific(pthread_key_t key, const void *value);
static int wrap_pthread_key_delete(pthread_key_t key);
static int wrap_pthread_join(pthread_t thread, void **value_ptr);
#ifndef GLIBC
static int wrap_pthread_atfork(void (*prepare)(void),
                               void (*parent)(void),
                               void (*child)(void));
#endif
static int wrap_pthread_sigmask(int how,
                                const sigset_t *restrict set,
                                sigset_t *restrict oset);
#ifndef MUSL
static int wrap_pthread_setschedprio(pthread_t thread, int prio);
#endif
static int wrap_pthread_setconcurrency(int new_level);
static int wrap_pthread_detach(pthread_t thread);
static int wrap_pthread_key_create(pthread_key_t *key,
                                   void (*destructor)(void*));
static int wrap_pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
static int wrap_pthread_kill(pthread_t thread, int sig);
//static int wrap_pthread_equal(pthread_t t1, pthread_t t2);



int main()
{
    int8_t           x;
    pid_t            pid;
    pthread_t        tid;
    pthread_key_t    tkey;
    char             *s, c;
    struct timespec  ns = {.tv_nsec=0};
    unsigned int     i, nr_fun, failed;
    int              err, ret[2]={0,0}, stat;
    pthread_rwlock_t tlock = PTHREAD_RWLOCK_INITIALIZER;
    const char      *function[] = {
        "pthread_create",
        "pthread_cancel",
        "pthread_once",
        "pthread_setspecific",
        "pthread_key_delete",
        "pthread_join",
        "pthread_atfork",
        "pthread_sigmask",
        "pthread_setschedprio",
        "pthread_setconcurrency",
        "pthread_detach",
        "pthread_key_create",
        "pthread_rwlock_unlock",
        "pthread_kill",
        //"pthread_equal",
    };
    struct {
        unsigned int
            kill:1,
            wait:1;
    } error, no_error = {.wait=0};
    struct sigaction act = {.sa_handler=handle, .sa_flags=SA_NODEFER};
    
    
    failed = 0;
    for (i=0;  i < (nr_fun=sizeof(function)/sizeof(*function));  ++i) {
        pipe(pfd);
        memset(&error, 0, sizeof(error));
        if (!(pid = fork())) {
            close(pfd[0]);
            blocked = 0; //wrappers set it to 1 after saving env
            sigaction(SIGTERM, &act, NULL);
            sigaction(SIGABRT, &act, NULL);
            pthread_create(&tid, NULL, thread, block);
            switch (i) {
                case 0:
                    err = wrap_pthread_create(&tid, NULL, thread, NULL);
                break;
                case 1:
                    err = wrap_pthread_cancel(tid);
                break;
                case 2:
                    err = wrap_pthread_once(
                                    (pthread_once_t []){PTHREAD_ONCE_INIT},
                                    block
                    );
                break;
                case 3:
                    if (!pthread_key_create(&tkey, NULL)) {
                        err = wrap_pthread_setspecific(tkey, NULL);
                        pthread_key_delete(tkey);
                    } else
                        err = -3;
                break;
                case 4:
                    //key creation(inicialization) done in the wrapper:
                    err = wrap_pthread_key_delete(tkey);
                break;
                case 5:
                    err = wrap_pthread_join(tid, NULL);
                break;
                #ifndef GLIBC
                case 6:
                    err = wrap_pthread_atfork(NULL, NULL, NULL);
                break;
                #endif
                case 7:
                    err = wrap_pthread_sigmask(SIG_UNBLOCK, NULL, NULL);
                break;
                #ifndef MUSL
                case 8:
                     err = wrap_pthread_setschedprio(
                                   tid,
                                   sched_get_priority_min(sched_getscheduler(0))
                    );
                break;
                #endif
                case 9:
                    err = wrap_pthread_setconcurrency(pthread_getconcurrency());
                break;
                case 10:
                    err = wrap_pthread_detach(tid);
                break;
                case 11:
                    //key deletion done in the wrapper:
                    err = wrap_pthread_key_create(&tkey, NULL);
                break;
                case 12:
                    if (!pthread_rwlock_init(&tlock, NULL)) {
                        err = wrap_pthread_rwlock_unlock(&tlock);
                        pthread_rwlock_destroy(&tlock);
                    }
                    else
                        err = -2;
                break;
                case 13:
                    err = wrap_pthread_kill(tid, 0);
                break;
                /*
                case 14:
                    err = wrap_pthread_equal(tid, tid);
                break;
                */
                default:
                    blocked = 0;
                    close(pfd[1]);
                    err = -2;
                break;
            }
            return err;
        }
        
        if (!pid) {
            fprintf(stdout, "BUG: child i=%d escaped!\n", i);
            exit(0); //exit/contain child
        }else if (pid == -1) {
            close(pfd[1]);
            close(pfd[0]);
            fprintf(stderr,
                "A call to fork() nr %d/%d yielded -1 (errno=%s)!\n",
                i+1, nr_fun, (s = e_name(errno))
            );
            free(s);
            failed += 1;
        }else{
            close(pfd[1]);
            read(pfd[0], &c, 1); //wait for the child to become ready
            close(pfd[0]);
            
            //Send SIGABRT to child in delays of <NANOSLEEP_MAX/100,0> microsec:
            ns.tv_nsec = NANOSLEEP_MAX;
            while(
                (ret[0] = kill(pid, SIGABRT)) == 0
                && (ret[1] = waitpid(pid, &stat, WNOHANG)) == 0
                && (ns.tv_nsec -= 100)
            )
                nanosleep(&ns, NULL);
            //let child's function out of its loop/blocked state
            kill(pid, SIGTERM);
            if (!ns.tv_nsec)
                waitpid(pid, &stat, 0);
            
            //Process errors:
            s = e_name(errno);
            error.kill = (ret[0] != 0);
            error.wait = (ret[1] != 0 && ret[1] != pid);
            if (memcmp(&error, &no_error, sizeof(error))) {
                ++failed;
                if (error.kill)
                    fprintf(stderr,
                            "A call to kill() returned %d, errno=%s\n",
                            ret[0], s
                    );
                if (error.wait)
                    fprintf(stderr,
                            "A call to waitpid() returned %d, errno=%s\n",
                            ret[1], s
                    );
            }else if (WIFEXITED(stat)) {
                if ((x = WEXITSTATUS(stat))) {
                    ++failed;
                    if (x == -2)
                        fprintf(stderr,
                           "%s() and/or its dependencies missing!\n",
                           function[i]
                        );
                    else if (x == -3)
                        fprintf(stderr,
                                "%s() was not tested - its dependency failed\n",
                                function[i]
                        );
                    else if (x) {
                        free(s);
                        s = e_name(x);
                        fprintf(stderr, "%s() returned %s\n", function[i], s);
                    }
                }
            }else{
                fprintf(stderr, "%s()'s host process crashed!\n", function[i]);
                if (WIFSIGNALED(stat))
                    fprintf(stderr,
                            "\tTerminating signal: %d\n",
                            (int)WTERMSIG(stat)
                    );
            }
            free(s);
        }
    }
    return failed;
}

/**
 ** Signal handler. Upon receiving the first SIGTERM it unsets blocked, longjmps
 ** Otherwise - on another signal or if blocked is unset - it simply returns
 **/
static void handle(int sig)
{
    if (sig == SIGTERM) {
        if(blocked) {
            blocked = 0;
            longjmp(env, 1);
        }
        //else {printf("BUG: Misplaced SIGTERM!\n");} //-----
    }
}
///waits for blocked to be unset by a signal[!] via the signal handler, returns
static void block(void)
{
    while(blocked)
        sched_yield(); //can't use pause() - handle() doesn't return on SIGTERM
    sleep(3);printf("exit block()\n");//---
}

/**
 ** Establishes the function passed to it as a thread cancellation handler and
 ** calls it. Thus, if the executing thread is canceled, the said function will
 ** still have to be executed before the thread is terminated.
 ** \param fun a void* cast of (void (*)()) or NULL (only return)
 **/
static void* thread(void *fun)
{
    if (fun != NULL) {
        pthread_cleanup_push(fun, NULL);
        ((void (*)())fun)();
        pthread_cleanup_pop(0);
    }
    return NULL;
}

///<Wrappers start here: They take the same arguments as the functions they
///<call but install the SIGTERM handler and optionally loop until it's received

static int wrap_pthread_create(pthread_t *restrict thread,
                               const pthread_attr_t *restrict attr,
                               void *(*start_routine)(void*),
                               void *restrict arg)
{
    WRAP_START
        while (!err)
            err = pthread_create(thread, attr, start_routine, arg);
    WRAP_END
}
static int wrap_pthread_cancel(pthread_t thread)
{
    WRAP_START
        err = pthread_cancel(thread);
    WRAP_END
}
static int wrap_pthread_once(pthread_once_t *once_control,
                        void (*init_routine)(void))
{
    WRAP_START
        err = pthread_once(once_control, init_routine);
    WRAP_END
}
static int wrap_pthread_setspecific(pthread_key_t key, const void *value)
{
    WRAP_START
        while (!err)
            err = pthread_setspecific(key, value);
    WRAP_END
}
static int wrap_pthread_key_delete(pthread_key_t key)
{
    /* this is clumsy: having to key_create lessens key_delete's chances
       of being interrupted - test not definite */
    int err2;
    WRAP_START
        while(
            !(err2 = pthread_key_create(&key, NULL))
            &&
            !(err  = pthread_key_delete(key))
        );
        if (err2)
            err = -3; //meaning the dependecy failed
    WRAP_END
}
static int wrap_pthread_join(pthread_t thread, void **value_ptr)
{
    WRAP_START
        err = pthread_join(thread, value_ptr);
    WRAP_END
}
#ifndef GLIBC
static int wrap_pthread_atfork(void (*prepare)(void),
                               void (*parent)(void),
                               void (*child)(void))
{
    WRAP_START
        while (!err)
            err = pthread_atfork(prepare, parent, child);
    WRAP_END
}
#endif
static int wrap_pthread_sigmask(int how,
                                const sigset_t *restrict set,
                                sigset_t *restrict oset)
{
    WRAP_START
        while (!err)
            err = pthread_sigmask(how, set, oset);
    WRAP_END
}
#ifndef MUSL
static int wrap_pthread_setschedprio(pthread_t thread, int prio)
{
    WRAP_START
        while (!err)
            err = pthread_setschedprio(thread, prio);
    WRAP_END
}
#endif
static int wrap_pthread_setconcurrency(int new_level)
{
    WRAP_START
        while (!err)
            err = pthread_setconcurrency(new_level);
    WRAP_END
}
static int wrap_pthread_detach(pthread_t thread)
{
    WRAP_START
        while (!err)
            err = pthread_detach(thread);
    WRAP_END
}
static int wrap_pthread_key_create(pthread_key_t *key,
                                   void (*destructor)(void*))
{
    /* this is clumsy: having to key_create lessens key_delete's chances
       of being interrupted - test not definite */
    int err2;
    WRAP_START
        while(
            !(err  = pthread_key_create(key, destructor))
            &&
            !(err2 = pthread_key_delete(*key))
        );
        if (!err && err2)
            err = -3; //meaning the dependecy failed
    WRAP_END
}
static int wrap_pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    WRAP_START
        while (!err)
            err = pthread_rwlock_unlock(rwlock);
    WRAP_END
}
static int wrap_pthread_kill(pthread_t thread, int sig)
{
    WRAP_START
        while(!err)
            err = pthread_kill(thread, sig);
    WRAP_END
}
/*static int wrap_pthread_equal(pthread_t t1, pthread_t t2)
{
    WRAP_START
        do{
            err = pthread_equal(t1, t2);
        }while(err != 0 && err != EINTR);
    WRAP_END
}*/
