#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/syscall.h>

#define _XOPEN_SOURCE 9001

/**
 ** \file depends: fprintf, malloc, setjmp, longjmp, sigaction!, sqrt, sprintf, sem_init, sem_wait, sem_post
 ** \author Luka Marčetić, 2011
 **/

#define M (long long)500*1024*1024  ///<allocation goal in bytes
#define NR_THREADS 10               ///<number of threads to spawn
#define N (long long)30             ///<M(/NR_THREADS)/N is maxum block size

jmp_buf env[2]; ///< will store the stack context/environment

struct block {
        char *ptr;
        size_t size;
        struct block *next;
} *ring;   ///< ring-queue keeps track of blocks, shared between threads
sem_t sem; ///< semaphore is used for mutex when working with the above

void bridge_sig_jmp(int sig);
int safe_free(void *vp, char *err_msg);
void* test_malloc(void *ptr);
int overlapping(struct block *a, struct block *b);

/**
 ** functions tested: malloc, calloc, posix_memalign
 **/

int main()
{
    int i, j, *blocks=malloc(sizeof(int));
    pthread_t *pt;
    char *s, tmps[160];
    void *vp;
    size_t st;
    const size_t pg_size = sysconf(_SC_PAGESIZE);
    struct block *bp;
    struct sigaction oldact[2], act;

    act.sa_handler = bridge_sig_jmp;
    act.sa_flags   = SA_NODEFER;
    sigaction(SIGSEGV, &act, &oldact[0]);
    
    /* -- malloc: -- */
    errno=0;
    //make a head element for the ring-queue which will hold info about blocks:
    ring = malloc(sizeof(struct block));
    if (!setjmp(env[0]) && ring!=NULL) {
        ring->ptr  = NULL;
        ring->size = 0;
        ring->next = ring;
    }else
        errno = -fprintf(stderr, "malloc failed to allocate the initialize %zu B\n", sizeof(struct block));
    if (!errno){
        blocks=(int*)test_malloc(NULL);
        //free all:
        for (i=0; !errno && i<=*blocks; ++i) {
            if (!setjmp(env[0])) {
                safe_free(ring->next->ptr, "free caused a SIGABRT (pointer provided by malloc)\n");
                bp = ring->next->next;
                safe_free(ring->next, "free caused a SIGABRT (pointer provided by malloc)\n");
                ring->next = bp;
            }else{
                fprintf(stdout, "W:free caused SIGSEGV\n");
                break;
            }
        }
        fprintf(stdout, "<multithreaded>\n"); //--------------
        pt = malloc(sizeof(pthread_t)*NR_THREADS);
        sem_init(&sem, 0, 1);
        for (i=0; i<NR_THREADS; ++i)
            pthread_create(&pt[i], NULL, test_malloc, (void*)1);//--------
        for (i=0; i<NR_THREADS; ++i)
            pthread_join(pt[i], NULL);
        fprintf(stdout, "</multithreaded>\n");//--------------
    }
    
    /* -- calloc (size_t overflow only): -- */
    errno = 0;
    vp = (void *)1;
    if ((vp=calloc(SIZE_MAX/2, 3)) != NULL) {
        fprintf(stdout, "W:calloc returned non-NULL when asked for more than SIZE_MAX bytes\n");
        if (errno == ENOMEM)
            fprintf(stderr, " errno=ENOMEM, implying calloc tried to allocate SIZE_MAX+ B\n");
        else {
            if (setjmp(env[0])) {
                s = (char *)vp + SIZE_MAX;
                ++s[1];
            }
            else
                fprintf(stderr, " (SIZE_MAX+1)th byte is inaccessible implying calloc underallocated\n");
                
            sprintf(tmps, "free following a calloc(%zu, %zu) caused a SIGABRT\n", SIZE_MAX/2, (size_t)3);
            errno = safe_free(vp, tmps);
        }
    }
    
    /* -- posix_memalign: -- */
    errno = 0;
    vp = (void *)1;
    for (st=1; st<pg_size+1 && !errno; ++st) {
        for (i=1; (size_t)i<pg_size+1 && !errno; ++i) {
            errno = posix_memalign((void **)&vp, st, i);
            s = (char*)vp;
            if (s != NULL) {
                if (errno == EINVAL)
                    fprintf(stdout, "W:posix_memalign returned EINVAL, but gave a non-NULL pointer (alignment=%d)\n", i);
                else {
                    if (st%sizeof(char *)!=0 || sqrt(st)!=(int)st) {
                        if(errno!=EINVAL)
                            errno = -fprintf(stderr, "posix_memalign failed to return EINVAL when given alignment=%zu\n", st);
                    }
                    else if ((size_t)s%st != 0)
                        errno = -fprintf(stderr, "posix_memalign gave a pointer that is not a multiple of %zu\n", st);
                    else {
                        for (j=0; j<i && !errno; ++j) {
                            if (!setjmp(env[0])) {
                                if (s[j] != 0)
                                    errno = -fprintf(stderr, "posix_memalign failed to initialize memory to 0\n");
                                if (!setjmp(env[0]))
                                    ++s[j];
                                else
                                    errno = -fprintf(stderr, "posix_memalign 'allocated' unwritable memory\n");
                            }
                            else
                                errno = -fprintf(stderr, "posix_memalign 'allocated' unreadable memory\n");
                        }
                    }
                    
                    sprintf(tmps, "free following a posix_memalign(%zu, %zu, %d) caused a SIGABRT\n", (size_t)s, st, i);
                    errno = safe_free(vp, tmps);
                }
            }
            else if (!errno)
                errno = -fprintf(stderr, "posix_memalign NULL :-/\n");
            
        }
    }
    
    sigaction(SIGSEGV, &oldact[0], NULL);
    return 0;
}

/**
 ** A bridge function to be passed to signal(), to call siglongjmp().
 **/
void bridge_sig_jmp(int sig)
{
    if (sig == SIGSEGV)
        longjmp(env[0], 1);
    else
        longjmp(env[1], 1);
    return;
}
/**
 ** \param vp a void pointer to memory to free
 ** \param err_msg an error message to be printed to stderr on failure(SIGABRT)
 ** \returns errno (set to <0 on failure)
 **/
int safe_free(void *vp, char *err_msg)
{
        struct sigaction oldact, act;
        act.sa_handler = bridge_sig_jmp;
        act.sa_flags   = SA_NODEFER;
        
        sigaction(SIGABRT, &act, &oldact);
        if (!setjmp(env[1]))
            free(vp);
        else
            errno = -fprintf(stderr, err_msg);
        sigaction(SIGABRT, &oldact, NULL);
        
        return errno;
}

/**
 ** Tests the malloc function and fragmentates memory
 ** \returns a void pointer to the number of blocks
 **/
void* test_malloc(void *ptr)
{
    int i, j, *blocks = malloc(sizeof(int));
    struct block *bp;
    long long bytes, max;
    char *s;
    void *vp;
    unsigned rnd;
    
    if(ptr == NULL)
        max = M;
    else
        max = M/NR_THREADS;

    //allocate blocks and fill the ring-queue with their info:
    errno = rnd = bytes = *blocks = 0;
    while(!errno && bytes<max) {
        rnd = rand_r(&rnd);
        vp = malloc(rnd%max/N +1);
        if (ptr != NULL)
            sem_wait(&sem);
        bp = ring->next;
        ring->next = malloc(sizeof(struct block));
        if (ring->next != NULL && vp != NULL) {
            if (!setjmp(env[0])) {
                for (s=(char*)vp; s<(char*)vp+(rnd%max/N +1); ++s)
                    *s = 1;
                ring->next->ptr = vp;
                ring->next->size = rnd%max/N+1;
                ring->next->next = bp;
                
                bytes += ring->next->size;
                ++(*blocks);
            }else
                errno = -fprintf(stderr, "malloc 'allocated' unwritable memory\n");
        }else if (errno != ENOMEM) {
            ring->next = bp; //restore old ring->next
            errno = -fprintf(stderr, "malloc returned a NULL without setting errno to ENOMEM\n");
        }
        if (ptr != NULL)
            sem_post(&sem);
    }
    
    /*
      fragmentate by QUASI-randomly re(m)allocating blocks and ring elements,
      check for overlapping memory segments (blocks):
    */
    for (i=0; !errno && i<=(*blocks)*2; ++i) {
        rnd = rand_r(&rnd);
        vp = malloc(rnd%max/N +1);
        if (ptr != NULL)
            sem_wait(&sem);
        if (ring->next->ptr != NULL) {
            errno = safe_free(ring->next->ptr, "free caused a SIGABRT (pointer provided by malloc)\n");
            bp = ring->next->next;
            errno = safe_free(ring->next, "free caused a SIGABRT (pointer provided by malloc)\n");
            if (!errno) {
                ring->next = malloc(sizeof(struct block));
                if (ring->next != NULL && vp != NULL) {
                    if (!setjmp(env[0])) {
                        for (s=(char*)vp; s<(char*)vp+(rnd%max/N +1); ++s)
                            *s = 1;
                        ring->next->ptr = vp;
                        ring->next->size = rnd%max/N +1;
                        ring->next->next = bp;
                    }else
                        errno = -fprintf(stderr, "malloc \"allocated\" unwritable memory\n");
                    
                    for (j=0; !errno && j<*blocks; ++j) {
                        if (ring->next != bp && overlapping(ring->next, bp))
                            errno = -fprintf(stderr, "malloc incorrectly allocated memory: segments overlapping\n");
                        bp = bp->next;
                    }
                }else if (errno != ENOMEM) {
                    ring->next = bp; //restore old ring->next
                    errno = -fprintf(stderr, "malloc returned a NULL without setting errno to ENOMEM\n");
                }
            }
        }
        for (j=0; !errno && j<(int)rnd%100; ++j)
            ring = ring->next;
        if (ptr != NULL)
            sem_post(&sem);
    }
    return blocks;
}

/**
 ** Tests memory blocks for overlapping
 ** \returns 1 if blocks are overlapping, otherwise 0
 **/
int overlapping(struct block *a, struct block *b)
{
    if (
           (b->ptr <= a->ptr          &&  a->ptr         <  b->ptr+b->size)
        || (b->ptr <  a->ptr+a->size  &&  a->ptr+a->size <= b->ptr+b->size)
        || (a->ptr <= b->ptr          &&  b->ptr         <  a->ptr+a->size)
        || (a->ptr <  b->ptr+b->size  &&  b->ptr+b->size <= a->ptr+a->size)
    )
        return 1;    
    return 0;
}

/*
    spike, multithreaded "Killed", multithreaded printout, realloc
*/
