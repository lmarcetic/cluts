#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <setjmp.h>

/**
 ** \file depends: sysconf, open, mmap, mprotect,
 **                sigaction, setjmp, longjmp, fprintf
 ** \author Luka Marčetić, 2011
 **/

jmp_buf env; ///< stores the stack context/environment (for jmping upon errors)

void bridge_sig_jmp(int sig);
size_t min(size_t a, size_t b);
size_t own_strncmp(char *s1, char *s2, size_t n);

/**
 ** Tests string.h functions for invalid memory access
 ** and for correctnes when handling various byte values.
 **
 ** functions tested: memcpy
 **/
int main()
{
    char *m1, *m2, *m3, *s1, *s2,  tmpc, *tmps;
    int i, j, k,
        reported[100]; ///< prevents recurring error messages !0 = "reported"
    const int fd = open("/dev/zero", O_RDWR);
    const size_t pg_size = sysconf(_SC_PAGESIZE);
    size_t t, real_size, buf_size[] =
    {
        1,                  //also tests 0 lenth
        sizeof(size_t)+1,   //brute force tests (keep it small)
        
        2,
        pg_size+1,
        4*pg_size,
        4*1024*1024
    };
    struct sigaction oldact, act;

    //call bridge_sig_jmp on segmentation fault:
    act.sa_handler=bridge_sig_jmp;
    act.sa_flags=SA_NODEFER;
    sigaction(SIGSEGV, &act, &oldact);
    for (i=0; i<sizeof(buf_size)/sizeof(buf_size[0]); ++i)
    {
        //stretch to a page size:
        real_size = (buf_size[i]/pg_size) * pg_size;
        if (buf_size[i]%pg_size != 0)
            real_size += pg_size;
        //allocate three memory segments, restrict the access to the last one:
        m1 = mmap(NULL, real_size*3, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        m3 = m1+(real_size*2);
        mprotect(m3, real_size, PROT_NONE);
        m2 = m3-buf_size[i]; //just before the restricted segment
        
        if (buf_size[i] == 1)
        {
            /* -- 0 length tests -- */
            //memcpy
            if (!setjmp(env))
            {
                if (memcpy(m3, m1, 0) != m3)
                    fprintf(stderr, "memcpy returned a wrong value when asked to copy 0B TO restricted memory\n");
            }
            else
                fprintf(stderr, "memcpy tried to access restricted memory when asked to copy 0B TO it\n");
            if (!setjmp(env))
            {
                if (memcpy(m1, m3, 0) != m1)
                    fprintf(stderr, "memcpy returned a wrong value when asked to copy 0B FROM restricted memory\n");
            }
            else
                fprintf(stderr, "memcpy tried to access restricted memory when asked to copy 0B FROM it\n");
            
            //memchr
            *m2 = 'a';
            if (!setjmp(env))
            {
                if (memchr(m2, '\0', 0) != NULL || memchr(m2, 'a', 0) != NULL || memchr(m3, '\0', 0) != NULL)
                    fprintf(stdout, "memchr failed to return a NULL pointer when given 0B to search through\n");
            }
            else
                fprintf(stderr, "memchr tried to access restricted memory when given 0B to search through\n");
                
            
            
            /* -- single byte tests with all possible byte values -- */
            for (j=0; j<3; ++j)
                reported[j] = 0;
            
            for (j=0; j<=255; ++j)
            {
                //memcpy
                for (k=0; k<=255; ++k)
                {
                    if (!reported[0])
                    {
                        *m1 = (char)j;
                        *m2 = (char)k;
                        tmpc = *m2;
                        if (!setjmp(env))
                        {
                            if (memcpy(m1, m2, 1) != m1)
                               reported[0] = fprintf(stderr, "memcpy returned a wrong value after copying a single byte(%d)\n", k);
                            if (*m1 != tmpc)
                               reported[0] = fprintf(stderr, "memcpy incorrectly copied a single byte(%d)\n", k);
                        }
                        else
                            reported[0] = fprintf(stderr, "memcpy failed to stop after reading a single byte(%d)\n", k);
                    }
                    
                    if (!reported[1])
                    {
                        *m1 = (char)j;
                        *m2 = (char)k;
                        tmpc = *m1;
                        if (!setjmp(env))
                        {
                            if (memcpy(m2, m1, 1) != m2)
                                reported[1] = fprintf(stderr, "memcpy returned a wrong value after copying a single byte(%d)\n", j);
                            if (*m2 != tmpc)
                               reported[1] = fprintf(stderr, "memcpy incorrectly copied a single byte(%d)\n", j);
                        }
                        else
                            reported[1] = fprintf(stderr, "memcpy failed to stop after writing a single byte(%d)\n", j);
                    }
                }
                 
                //memchr
                if (!reported[2])
                {
                    tmpc = *m2 = (char)j;
                    if (!setjmp(env))
                    {
                        if (memchr(m2, tmpc, 1) != m2)
                            reported[2] = fprintf(stderr, "memchr returned a wrong value searching for THE one byte(%d)\n", j);
                    }
                    else
                       reported[2] = fprintf(stderr, "memchr failed to stop searching for THE one byte(%d)\n", j);
                }
            }
        }
        /* -- brute force alignment/bitfield tests -- */
        else if (buf_size[i] == sizeof(size_t)+1)
        {   
            for (j=0; j<2; ++j)
               reported[j] = 0;
            for (s2=m2; s2<m3; ++s2)
            {
                //memcpy
                for (s1=m1; s1<m2;  ++s1)
                {
                    for (t=0; !reported[0] && t<=min(m2-s1, m3-s2); ++t)
                    {
                        for (k=0; k<buf_size[i]; ++k)
                            m2[k] = 256-(m1[k] = k%256); //various Bs
                        if (!setjmp(env))
                        {
                            if (memcpy(s1, s2, t) != s1 || memcpy(s2, s1, t) != s2)
                                reported[0] = fprintf(stderr, "memcpy(%zu,%zu,%zu) returned a wrong value\n", (size_t)s1, (size_t)s2, t);
                            if (own_strncmp(s1, s2, t))
                                reported[0] = fprintf(stderr, "memcpy(%zu,%zu,%zu) incorrectly copied memory\n", (size_t)s1, (size_t)s2, t);
                        }
                        else
                            reported[0] = fprintf(stderr, "memcpy(%zu,%zu,%zu) tried to access restricted memory\n", (size_t)s1, (size_t)s2, t);
                    }
                }
                
                //memchr
                for (j=0; j<=255; ++j)
                {
                    for (k=0; k<buf_size[i]; ++k)
                    {
                        m2[k] = k%256;
                        if (m2[k] == j)
                            ++m2[k];
                    }
                    for (k=0; k<m3-s2; ++k)
                    {
                        for (t=0; !reported[1] && t<m3-s2; ++t)
                        {
                            tmpc = s2[k];
                            s2[k] = (char)j;
                            if (!setjmp(env))
                            {
                                if ((t>k && (tmps=memchr(s2, (char)j, t)) != s2+k) || (t<=k && (tmps=memchr(s2, (char)j, t)) != NULL))
                                    reported[1] = fprintf(stderr, "memchr(%zu,%d,%zu) returned a wrong value(%zu)\n", (size_t)s2, j, t, (size_t)tmps);
                            }
                            else
                                reported[1] = fprintf(stderr, "memchr(%zu,%d,%zu) tried to access restricted memory\n", (size_t)s2, j, t);
                            s2[k] = tmpc;
                        }
                    }
                }
            }
             
        }
        /* -- tests for other lengths -- */
        else
        {
            for (j=0; j<2; ++j)
            {
                //memcpy
                for (k=0; k<buf_size[i]; ++k)
                    m2[k] = 255-(m1[k] = k%255);
                if (!setjmp(env))
                {
                    if (!j && memcpy(m1, m2, buf_size[i]) != m1)
                        fprintf(stderr, "memcpy returned a wrong value after reading %zuB\n", buf_size[i]);
                    else if (j && memcpy(m2, m1, buf_size[i]) != m2)
                        fprintf(stderr, "memcpy returned a wrong value after writing %zuB\n", buf_size[i]);
                    if (own_strncmp(m1, m2, buf_size[i]))
                        fprintf(stderr, "memcpy incorrectly copied %zuB\n", buf_size[i]);
                }
                else
                {
                    if (!j)
                        fprintf(stderr, "memcpy failed to stop after copying %zuB\n", buf_size[i]);
                    else if (j)
                        fprintf(stderr, "memcpy failed to stop after writing %zuB\n", buf_size[i]);
                }
            }
            
            //memchr
            for (k=0; k<buf_size[i]; ++k)
            {
                m2[k] = k%256;
                if (m2[k] == 1)
                    ++m2[k];
            }
            m2[k-1] = 1;
            if (!setjmp(env))
            {
                if ((tmps = memchr(m2, 1, buf_size[i])) != &m2[k-1])
                    fprintf(stderr, "memchr returned a wrong value searching %zuB for a 1\n", buf_size[i]);
            }
            else
               fprintf(stderr, "memchr failed to stop searching %zuB for a 1\n", buf_size[i]);
        }

        
        munmap(m1, real_size*3);
    }
    sigaction(SIGSEGV, &oldact, NULL);
    
    return 0;
}
/**
 ** A strcmp replacement function (man strncmp)
 **/
size_t own_strncmp(char *s1, char *s2, size_t n)
{
    size_t i;
    if (n<1)
        return 0;
    for (i=0; i<n-1 && s2[i]==s1[i]; ++i);
    return (size_t)(s1[i]-s2[i]);
}

/**
 ** \returns smaller of the two parameters passed
 ** \param a first value for the comparison
 ** \param b second value for the comparison
 **/
size_t min(size_t a, size_t b)
{
    if (a < b)
        return a;
    return b;
}

/**
 ** A bridge function for sigaction(), jumps via longjmp() back to a setjmp()
 **/
void bridge_sig_jmp(int sig)
{
    longjmp(env, 1);
    return;
}
