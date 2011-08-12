#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdint.h> //PTRDIFF_MAX, SIZE_MAX
#include <signal.h>
#include "common/common.h"

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
 ** Tests that blocks allocated by various functions do not overlap, that they 
 ** are successfully writable, and that they are freeable by free().
 ** In case of posix_memalign, it allso tests that blocks are correctly aligned,
 ** and in the case of calloc, that allocation of huge values results in failure
 ** tests: malloc, realloc, calloc, posix_memalign
 ** depends: sysconf, free, fork,wait, rand, setjmp,longjmp, qsort, fprintf
 **          memcmp,memset, open,mmap
 **/

jmp_buf env;
struct {
    size_t
        page,
        prog,
        blocks;
} mem;
struct block {
    char *ptr;
    size_t size;
};
struct s_err {
    unsigned int
        alloc:1,
        overlapping:1,
        notr:1,
        notw:1,
        notsame:1,
        misaligned:1,
        ov_size_t:1,
        ov_ptrdiff_t:1,
        free:1,
        mallocfree:1,
        rand:1,
        limit:1,
        no;
} *err;

#define TRY_FREE(PTR) { err->free = 0; free(PTR); err->free = 0; }
static int vm_limit(const size_t);
static size_t posix_memalign_alignment(size_t);
static size_t blocks_alloc(const void *, struct block *, size_t);
static size_t blocks_overlapping(struct block *, size_t);
static void blocks_sort(struct block *, size_t);
static int blocks_sort_compar(const void *, const void *);
static int block_misaligned(struct block, size_t);
static int blocks_misaligned(struct block *, size_t);
static int blocks_notrw(struct block *, size_t);
static int calloc_overflows(long long unsigned int);
static void bridge_sig_jmp(int);

int main()
{
    struct block *b;
    size_t       i, j, nr_blocks;
    int          ret, stat;
    const void   *fun[]      = { malloc,  realloc,  calloc,  posix_memalign };
    const char   *fun_name[] = {"malloc","realloc","calloc","posix_memalign"};
    const int    fd          = open("/dev/zero", O_RDWR);
    char         *s;
    
    mem.page   = sysconf(_SC_PAGE_SIZE);
    mem.blocks = 1000;                  //max. nr. of blocks to be generated
    mem.prog   = mem.blocks*mem.page;   //memory reserved for dynamic allocation
    b = malloc(mem.blocks*sizeof(struct block));
    err = mmap(NULL,sizeof(struct s_err),PROT_READ|PROT_WRITE, MAP_SHARED,fd,0);
    
    ret = 0;
    for (i=0; i<sizeof(fun)/sizeof(*fun); ++i) {
        memset(err, 0, sizeof(*err)); //all zeros = no errors
        if (!fork()){
            if (fun[i] == calloc) {
                if(calloc_overflows(SIZE_MAX))
                    err->ov_size_t = 1;
                if(calloc_overflows(PTRDIFF_MAX))
                    err->ov_ptrdiff_t = 1;
            }
            
            if (!vm_limit(mem.prog)) {
                err->limit = 1;
                free(b);
                return 0; //!!!
            }
            
            if (fun[i] == realloc) {
                nr_blocks = blocks_alloc(malloc, b, mem.blocks);
                if (nr_blocks > 5) {
                    //free some for subsequent realloc
                    err->mallocfree = 1;
                    for(j = nr_blocks; nr_blocks >= j-5; --nr_blocks)
                        free(b[nr_blocks-1].ptr);
                    err->mallocfree = 0;
                    nr_blocks = blocks_alloc(fun[i], b, nr_blocks);
                }
            }
            else
                nr_blocks = blocks_alloc(fun[i], b, mem.blocks);
            printf("%zu: %zu\n", i, nr_blocks); //---
            
            if (nr_blocks < 10) //an imposed minimum
                err->alloc = 1;
            else {
                if (fun[i]==posix_memalign && blocks_misaligned(b, nr_blocks))
                    err->misaligned = 1;
                //universal:
                if (blocks_overlapping(b, nr_blocks))
                    err->overlapping = 1;
                switch (blocks_notrw(b, nr_blocks)) {
                    case 1: err->notw    = 1; break;
                    case 2: err->notr    = 1; break;
                    case 3: err->notsame = 1; break;
                } 
            }
            for(j=0; j<nr_blocks; ++j)
                TRY_FREE(b[j].ptr);
            free(b);
            return 0;
        }
        
        wait(&stat);
        if (!WIFEXITED(stat) && !err->free) {
            ++ret;
            fprintf(stderr, "%s test quit unexpectedly!\n", fun_name[i]);
            if (WIFSIGNALED(stat))
                fprintf(stderr, "\tTerminating signal: %d\n", WTERMSIG(stat));
        }else {
            ret += (memcmp(err,(struct s_err []){{.free=0}},sizeof(*err)) != 0);
            if (err->rand)
                fprintf(stderr,
                       "An unlikely situation occurred while generating size"
                       " arguments for %s: rand()%%(%zu)+1 on average resulted"
                       " in values no greater than 10%% of the possible maximum"
                       " value. Subsequent errors may result from this.\n",
                       fun_name[i],
                       mem.page*10
                );
            if (err->limit)
                fprintf(stderr,
                        "Initial virtual memory allocation failed."
                        "%s tests will not run\n", fun_name[i]
                );
            if (err->alloc)
                fprintf(
                    stderr,
                    "%s failed to allocate sufficient amount of memory required"
                    " for the test\n",
                    fun_name[i]
                );
            if (err->overlapping)
               fprintf(stderr,"%s allocated blocks that overlap\n",fun_name[i]);
            if (err->notr)
               fprintf(stderr,"%s 'allocated' unreadable memory\n",fun_name[i]);
            if (err->notw)
               fprintf(stderr,"%s 'allocated' unwritable memory\n",fun_name[i]);
            if (err->notsame)
                fprintf(stderr,
                        "a byte read from %s-allocated memory does not"
                        " match the one previously writen there\n",
                        fun_name[i]
                );
            if (err->misaligned)
                fprintf(
                    stderr, "%s allocated at least one misaligned block\n",
                    fun_name[i]
                );
            if (err->mallocfree)
                fprintf(
                    stderr,
                    "%s can't be tested: free() failed to free memory allocated"
                    " by malloc(), which was required for the test to run\n",
                    fun_name[i]
                );
            if (err->free)
                fprintf(
                    stderr, "free() failed to free memory allocated by %s\n",
                    fun_name[i]
                );
            if (err->ov_size_t)
                fprintf(
                    stderr,
                    "%s tried to allocate more than SIZE_MAX bytes, which"
                    " means the block's size would not be representable"
                    " by a size_t type\n",
                    fun_name[i]
                );
            if (err->ov_ptrdiff_t)
                fprintf(
                    stderr,
                    "%s tried to allocate more than PTRDIFF_MAX bytes, which"
                    " would cause an overflow of ptrdiff_t when subtracting"
                    " two pointers from the opposite ends of the block\n",
                    fun_name[i]
                );
            if (err->no) {
                fprintf(stderr, "%s test set errno=%s\n",
                        fun_name[i], s = e_name(err->no));
                free(s);
            }
        }
    }
    
    free(b);
    return ret;
}

/**
 ** Allocates all remaining virtual memory, leaving limit both for brk and mmap
 ** \limit a desired number of free bytes upon returning from the function
 ** \returns 1 on success, 0 on failure
 **/
static int vm_limit(const size_t limit) {
    const int fd = open("/dev/zero", O_RDWR);
    size_t i,j,last; 
    void *mmapd, *vp;
    void **brkd, **vpp;
    
    //allocate mmap area of size limit:
    mmapd = mmap(NULL, limit, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
    //allocate brk area of size limit:
    brkd = (vpp = malloc(mem.page));
    for(i=0;  vpp!=NULL && (i+=mem.page)<limit;  vpp = *vpp = malloc(mem.page));
    
    if (mmapd != MAP_FAILED && i>=limit) {
        //repeated mmaps of binary-search-determined sizes to fill everything:
        do {
            last = 0;
            for (i=j=SIZE_MAX/2+1;  i>0;  i/=2) {
                if ((vp=mmap(NULL, j,PROT_NONE,MAP_PRIVATE,fd,0)) == MAP_FAILED)
                    j -= i/2;
                else {
                    munmap(vp, (last=j));
                    j += i/2;
                }
            }
        }while(last && mmap(NULL, last,PROT_NONE,MAP_PRIVATE,fd,0)!=MAP_FAILED);
        
        //free that mmap area:
        munmap(mmapd, limit);
        //free that brk area:
        for(i=0; i<limit; i+=mem.page) {
            vpp = *brkd;
            free(brkd);
            brkd = vpp;
        }
    }
    
    return (mmapd != MAP_FAILED && i>=limit && !last);
}

/**
 ** (re)Allocates blocks of random sizes to fill memory, saves info about them
 ** NOTE: Function not stand-alone, requires initialized *err and mem structures
 ** \param fun a pointer to the function to call to (re)allocate the blocks
 ** \param b a pointer to a struct block array to be filled with blocks info
 ** \param n for realloc: a number of blocks that are allocated (<=mem.blocks)
 ** \returns the number of blocks successfully allocated
 **/
static size_t blocks_alloc(const void *fun, struct block *b, size_t n)
{
    long int rndm;
    size_t  i, size, nr_blocks;
    char *ptr;
    int error;
    
    nr_blocks = error = 0;
    for (i=0; !error &&  i<mem.blocks; ++i) {
        ptr = NULL;
        rndm = random(); 
        size = rndm % (mem.page*10) +1; //see error report for err->rand
        
        if (fun == malloc)
            ptr = malloc(size);
        else if (fun == calloc)
            ptr = calloc(1, size);
        else if (fun == posix_memalign)
            error=posix_memalign((void**)&ptr,posix_memalign_alignment(i),size);
        else if (fun == realloc) {
            if (i<n)
                ptr = realloc(b[nr_blocks].ptr, size);
            else
                ptr = malloc(size);
        }
        
        if (ptr != NULL && (fun != posix_memalign || error == 0)) {
            if (fun != realloc && rndm % 10) {
                TRY_FREE(ptr);
                --i;
            }else{
                b[nr_blocks].ptr  = ptr;
                b[nr_blocks].size = size;
                ++nr_blocks;
            }
        }else{
            if (fun != posix_memalign)
               error = errno;
            --i;
        }
    }
    if (error != ENOMEM) {
        if (i==mem.blocks)
            err->rand = 1;
        else
            err->no = (unsigned int)error;
        
        for (i=0; i<nr_blocks; ++i)
            TRY_FREE(b[i].ptr);
        nr_blocks = 0; //will set err->alloc in main()
    }
    return nr_blocks;
}
/**
 ** Generates a value to be used as a 2nd argument(alignment) to posix_memalign
 ** \param seed - required for the calculation (index of the block element)
 ** \returns a power of two value that is a multiple of sizeof(void *)
 **/
static size_t posix_memalign_alignment(size_t seed)
{
    do{
        ++seed;
    }while(seed%sizeof(void*) || (seed/sizeof(void*) & (seed/sizeof(void*)-1)));
    return seed;
}

/**
 ** Searches for overlapping blocks
 ** \param b a pointer to an array of type struct block
 ** \param n number of elements in the array to search through (will be sorted!)
 ** \returns an index to b of an element whose .ptr points to an address in the
 **          preceeding block's space[ptr, ptr+size], or 0 if none such is found
 **/
static size_t blocks_overlapping(struct block *b, size_t n)
{
    blocks_sort(b, n);
    for (size_t i=1; i<n; ++i)
        if(b[i].ptr < b[i-1].ptr+b[i-1].size)
            return i;
    return 0;
}
/**
 ** Sorts the array of type struct block
 ** \parm b a pointer to the array
 ** \param n the number of array elements to sort
 **/
static void blocks_sort(struct block *b, size_t n)
{
    qsort(b, n, sizeof(b[0]), blocks_sort_compar);
}
//A comparison function used by qsort() in blocks_sort()
static int blocks_sort_compar(const void *v1, const void *v2)
{
    struct block b1 = *(struct block *)v1,
                 b2 = *(struct block *)v2;
    if (b1.ptr < b2.ptr)
        return -1;
    return (b1.ptr > b2.ptr);
}

/**
 ** Check blocks for read/write-ability (+consistence)
 ** \param b a pointer to an array of type struct block
 ** \param n the number of array elements to check
 ** \returns
 **         0 - if all blocks are readable and writable
 **         1 - if at least one of the blocks isn't writable
 **         2 - if at least one of the blocks isn't readable
 **         3 - if a byte read didn't match a byte previously written
 **/
static int blocks_notrw(struct block *b, size_t n)
{
    size_t i;
    struct sigaction
        oldact,
        act = {.sa_handler = bridge_sig_jmp, .sa_flags = SA_NODEFER};
    int ret = 0;
    
    sigaction(SIGSEGV, &act, &oldact);
    for (i=0; !ret && i<n; ++i) {
        if(!setjmp(env)) {
            b[i].ptr[0] = '\r';
            b[i].ptr[b[i].size/2] = '\r';
            b[i].ptr[b[i].size-1] = '\r';
            if(!setjmp(env)) {
                if(b[i].ptr[0] != '\r'
                   || b[i].ptr[b[i].size/2] != '\r'
                   || b[i].ptr[b[i].size-1] != '\r'
                )
                    ret = 3;
            }else
                ret = 2;
        }else
            ret = 1;
    }
    sigaction(SIGSEGV, &oldact, NULL);
    return ret;
}


/**
 ** Check that a block (allocated by posix_memalign) is correctly aligned
 ** \param b1 a block to check (type struct block)
 ** \param alignment value of the 2nd argument that was passed to posix_memalign
 ** \returns 1 if the block is incorrectly aligned, otherwise 0
 **/
static int block_misaligned(struct block b1, size_t alignment)
{
    return ((size_t)b1.ptr % alignment != 0);
}
/** Does for an array what block_misaligned does for a single block
 ** \note the function assumes that posix_memalign_alignment() generated  the 
 ** alignment value, seeded with an index of each element. Likewise, it assumes
 ** that assuming b[0] is truly the first element of the array
 ** \param b a pointer to the first(!) element of an array of type struct block
 ** \param n the number of elements in the array to check
 ** \returns 1 if any of the calls to block_misaligned() returns 1, otherwise 0
 **/
static int blocks_misaligned(struct block *b, size_t n)
{
    for(size_t i=0; i<n; ++i)
        if (block_misaligned(b[i], posix_memalign_alignment(i)))
            return 1;
    return 0;
}

/**
 ** Calls calloc with arguments whose product is greater than limit, to see if
 ** it returns a non-NULL pointer(ie allocates space, in which case it is freed)
 ** \param limit when incremented by 1, a value for which allocation should fail
 ** \returns 1 if calloc returns a non-NULL (incorrect behavior), otherwise 0
 **/
static int calloc_overflows(long long unsigned int limit)
{
    void *vp;
    
    vp = calloc(8, limit/8 + 1);
    free(vp);
    return (vp != NULL);
}

//A bridge function for sigaction, that longjmp's and restores env
static void bridge_sig_jmp(int sig)
{
    longjmp(env, sig);
    return;
}
