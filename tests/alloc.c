#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <setjmp.h>
#include <stdlib.h>
#include <wait.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h> //PTRDIFF_MAX, SIZE_MAX

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
 ** tests: malloc, realloc, calloc, posix_memalign
 ** depends: sysconf, sqrtl, free, fork,wait, srand,rand, setjmp,longjmp, qsort,
 **          fprintf, memcmp,memset, open,mmap
 **/

#define MB_RAM 16
jmp_buf env;
struct s_mem {
    const size_t page, pages;
    const unsigned long long free;
} *mem;
struct block {
    char *ptr;
    size_t size;
};

static size_t posix_memalign_alignment(size_t);
static size_t blocks_alloc(const void *, struct block *, size_t);
static size_t blocks_overlapping(struct block *, size_t);
static void blocks_sort(struct block *, size_t);
static int blocks_sort_compar(const void *, const void *);
static int block_misaligned(struct block, size_t);
static int blocks_misaligned(struct block *, size_t);
static int blocks_notrw(struct block *, size_t);
static int calloc_overflows(long int);
static void bridge_sig_jmp(int);

int main()
{
    //encapsulate some memory info in a struct:
    mem = &(struct s_mem){
        //size of one page in bytes:
        .page = sysconf(_SC_PAGE_SIZE),
        //number of available pages, free memory in bytes:
        #ifdef _SC_AVPHYS_PAGES
            .pages = sysconf(_SC_AVPHYS_PAGES),
            .free  = sysconf(_SC_PAGE_SIZE) * sysconf(_SC_AVPHYS_PAGES),
        #else
            .pages = (MB_RAM*1024*1024/mem->page), ///<ehm
            .free  = sysconf(_SC_PAGE_SIZE) * (MB_RAM*1024*1024/mem->page),
        #endif
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
            free:1;
    } *err; //a bitfield to indicate errors
    const size_t max_blocks  = mem->pages*1.6; //should alloc >= mem->free bytes
    struct block *b          = malloc(max_blocks * sizeof(struct block));
    const void   *fun[]      = { malloc,  realloc,  calloc,  posix_memalign };
    const char   *fun_name[] = {"malloc","realloc","calloc","posix_memalign"};
    const int    fd          = open("/dev/zero", O_RDWR);
    size_t       nr_blocks;
    int          ret, stat;
    unsigned int i, j;
    err = mmap(NULL,sizeof(struct s_err),PROT_READ|PROT_WRITE, MAP_SHARED,fd,0);
    
    ret = 0;
    for (i=0; i<sizeof(fun)/sizeof(*fun); ++i) {
        memset(err, 0, sizeof(*err)); //all zeros = no errors
        //if (!fork()){
            if (fun[i]==calloc) {
                if(calloc_overflows(SIZE_MAX))
                    err->ov_size_t = 1;
                if(calloc_overflows(PTRDIFF_MAX))
                    err->ov_ptrdiff_t = 1;
            }
            
            if (fun[i] == realloc)
                nr_blocks = blocks_alloc(malloc,  b, max_blocks);
            else
                nr_blocks = max_blocks;
            nr_blocks = blocks_alloc(fun[i], b, nr_blocks);
            printf("(%u)", nr_blocks); //---
            
            if (nr_blocks < mem->pages*0.9) //at least
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
                err->free = 1;
                for(j=0; j<nr_blocks; ++j)
                    free(b[j].ptr);
                err->free = 0;
            }
            return 0;
        /*}*/fork();//---
        
        wait(&stat);
        if (!WIFEXITED(stat) && !err->free) {
            ++ret;
            fprintf(stderr, "%s test quit unexpectedly (status code: %i)!\n",
                            fun_name[i], WEXITSTATUS(stat));
        }else {
            ret += (memcmp(err,(struct s_err []){{.free=0}},sizeof(*err)) != 0);
            if (err->alloc)
                fprintf(
                    stderr,
                    "%s failed to allocate sufficient amount of"
                    " memory to run the test\n",
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
            if (err->free)
                fprintf(
                    stderr, "free() failed to free memory allocated by %s\n",
                    fun_name[i]
                );
            if (err->ov_size_t)
                fprintf(
                    stderr,
                    "%s tried to allocate more than SIZE_MAX bytes, which"
                    " means the block's size would no be representable"
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
        }
    }
    
    return ret;
}

/**
 ** (re)Allocates a number of blocks of random sizes, saves info about them
 ** \param fun a pointer to the function to call to (re)allocate the blocks
 ** \param b a pointer to a struct block array to be filled with blocks info
 ** \param n a maximum number of blocks<=max.nr. of elements the array can store
 ** \returns the number of blocks successfully allocated
 **/
static size_t blocks_alloc(const void *fun, struct block *b, size_t n)
{
    size_t  i, size, nr_bytes, nr_blocks;
    char    *ptr;
    
    srand((unsigned int)b[0].ptr);
    nr_bytes = nr_blocks = 0;
    for (i=0; i<n && nr_bytes < mem->free; ++i) {
        ptr = NULL;
        size = rand() % mem->page*2;
        if (fun == malloc)
            ptr = malloc(size);
        else if (fun == calloc)
            ptr = calloc(1, size);
        else if (fun == posix_memalign)
            posix_memalign((void **)&ptr, posix_memalign_alignment(i),size);
        else if (fun == realloc)
            ptr = realloc(b[nr_blocks].ptr, size);
        else
            return 0;
        if (ptr != NULL) {
            b[nr_blocks].ptr  = ptr;
            b[nr_blocks].size = size;
            ++nr_blocks;
            nr_bytes += size;
        }
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
    while (seed%sizeof(void*) || (seed/sizeof(void*) & seed/sizeof(void*)-1))
        ++seed;
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
 **         1 - if one of the blocks isn't writable
 **         2 - if one of the blocks isn't readable
 **         3 - if a byte that was read didn't match a byte that was written
 **/
static int blocks_notrw(struct block *b, size_t n)
{
    size_t i, j;
    struct sigaction oldact, act;
    int ret = 0;
    
    act.sa_handler = bridge_sig_jmp;
    act.sa_flags   = SA_NODEFER;
    sigaction(SIGSEGV, &act, &oldact);
    for (i=0; i<n && !ret; ++i) {
        for (j=0; j<=3 && !ret; ++j) {
            if(!setjmp(env)) {
                b[i].ptr[j*b[i].size] = '\r';
                if(!setjmp(env)) {
                    if(b[i].ptr[j*b[i].size] != '\r')
                        ret = 3;
                }else
                    ret = 2;
            }else
                ret = 1;
        }
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
static int calloc_overflows(long int limit)//long>size_t for this/sqrtl to work!
{
    long int d;
    void *vp;
    for (d = sqrtl(limit);  limit%d != 0;  --d);
    if (d < 2) {
        d = 2;
        ++limit;
    }
    if ((vp = calloc(d+1 , limit/d)) != NULL) {
        free(vp);
        return 1;
    }else
        return 0;
}

//A bridge function for sigaction, that longjmp's and restores env
static void bridge_sig_jmp(int sig)
{
    longjmp(env, sig);
    return;
}
