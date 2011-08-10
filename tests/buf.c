#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "common/common.h"
#include <string.h>   //strerror_r
#include <limits.h>   //PATH_MAX
#include <stdlib.h>   //mbstowcs
#include <stdio.h>    //getdelim, snprintf
#include <unistd.h>   //confstr, getcwd, gethostname, readlink
#include <iconv.h>    //iconv
#include <time.h>     //time, gmtime
//#include <monetary.h> //strfmon

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
 ** Tests functions for writing beyond string lenght and errno's they set
 ** tests: confstr, getcwd, getdelim, gethostname, iconv, mbstowcs, snprintf,
 **        readlink, strfmon, strftime, wcstombs, ttyname_r, strerror_r
 ** depends: malloc, sigaction, strlen, mkstemp, fdopen,fclose,fileno, strptime
 **          iconv_open,iconv_close, free, setjmp, longjmp, open,close,
 **          mmap,mprotect,munmap
 **/

jmp_buf env;
struct sigaction act, oldact[2]; //set by main, used in wrappers:

void bridge_sig_jmp(int sig);
static int safe_free(void* vp);
static int wrap_confstr(int name, char *buf, size_t len);
static int wrap_getcwd(char *buf, size_t size);
static int wrap_getdelim(char **lineptr, size_t *n, int delimiter,FILE *stream);
static int wrap_gethostname(char *name, size_t namelen);
static int wrap_mbstowcs(wchar_t *pwcs, char *s, size_t n);
static int wrap_snprintf(char *s, size_t n, char *format/*, ...*/);
static int wrap_readlink(char *path, char *buf, size_t bufsize);
//static int wrap_strfmon(char *s, size_t maxsize, char *format/*,...*/, float F);
static int wrap_strftime(char *s, size_t maxsize,
                         char *format, struct tm *timeptr);
static int wrap_wcstombs(char *s, wchar_t *pwcs, size_t n);
static int wrap_strerror_r(int errnum, char *strerrbuf, size_t buflen);
static int wrap_ttyname_r(int fildes, char *name, size_t namesize);
static int wrap_iconv(iconv_t cd, char **inbuf,  size_t *inbytesleft,
                                  char **outbuf, size_t *outbytesleft);

int main()
{
    iconv_t cd;
    wchar_t *ws;
    size_t size;
    struct tm tm;
    FILE *stream = NULL;
    char *s, *fun, *tmp, filename[] = "/tmp/clutsXXXXXX";
    int fd, sig, function, wrong, failed, err, err_expected;
    int ffun[] = {2, 3,8}; //functions that depend on the temporary file(two do)
    
    //GLOBAL:
    act=(struct sigaction){.sa_handler=bridge_sig_jmp, .sa_flags=SA_NODEFER};

    failed = 0;
    function = 1;
    do {
        err = wrong = 0;
        s = fun = tmp = NULL;
        ws = NULL;
        stream = NULL;
        if (seq_has(function, ffun) && (fd=mkstemp(filename)) != -1) {
            if ((stream=fdopen(fd, "w+b")) == NULL)
                wrong = -1;
        }
        switch(function) {
            case 1:
                fun = sreturnf("confstr(_CS_PATH, s, sizeof(s)-1)");
                size = confstr(_CS_PATH, NULL, 0);
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                
                if ((err=wrap_confstr(_CS_PATH, s, size-1)) == -1)
                    wrong = 1;
                else
                    /*if ((err))
                        wrong = 5;
                    else */if (s[size-1] != '\r')
                        wrong = 2;
            break;
            case 2:
                fun = sreturnf("getcwd(s, sizeof(s)-1)");
                s = malloc(PATH_MAX);
                size = strlen(getcwd(s, PATH_MAX))+1;
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err=wrap_getcwd(s, size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
                else if (err != (err_expected = ERANGE))
                    wrong = 3;
            break;
            case 3:
                //same for getline
                fun = sreturnf("getdelim(&s, &<strlen(s)>, '\\n', stream)");
                if (stream == NULL)
                    break;
                fwrite("123\n", (size = 4), 1, stream);
                s = malloc(size);
                s[size-1] = '\r';
                
                if((err=wrap_getdelim(&s,(size_t[]){size-1},'\n',stream)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            case 4:
                fun = sreturnf("gethostname(s, sizeof(s)-1)");
                s = malloc(HOST_NAME_MAX);
                gethostname(s, HOST_NAME_MAX);
                size = strlen(s) + 1;
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err=wrap_gethostname(s, size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            case 5:
                fun = sreturnf("iconv(&<\"abcd\">, &<4>, &s, &<3>)");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                cd = iconv_open("", "");
                err = wrap_iconv(
                        cd, 
                        (char **)(char *[]){"abcd"},
                        (size_t *)(size_t []){size},
                        (char **)(char *[]){s},
                        (size_t *)(size_t []){size-1}
                );
                iconv_close(cd);
                if (err == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
                else if (err != (err_expected = E2BIG))
                    wrong = 3;
            break;
            case 6:
                fun = sreturnf("mbstowcs(ws, \"abc\", 3)");
                size = 4;
                ws = malloc(size * sizeof(wchar_t));
                ws[size-1] = L'\r';
                
                if ((err=wrap_mbstowcs(ws, "abcd", size-1)) == -1)
                    wrong = 1;
                else if (ws[size-1] != L'\r')
                    wrong = 2;
            break;
            case 7:
                fun = sreturnf("snprintf(s, 3, \"abc\")");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                if ((err=wrap_snprintf(s, size-1, "abc")) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            case 8:
                fun=sreturnf("readlink(\"%s.sym\", s, sizeof(s)-1)", filename);
                tmp = sreturnf("%s.sym", filename);
                if (stream == NULL)
                    break;
                else if (symlink(filename, tmp)) {
                    wrong = 4;
                    break;
                }
                s = malloc(PATH_MAX);
                size = readlink(tmp, s, PATH_MAX);
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err=wrap_readlink(tmp, s, size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            case 9:
                fun = sreturnf("strerror_r(1, s, sizeof(s)-1)");
                s = malloc(80);
                strerror_r(1, s, 80);
                size = strlen(s);
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err = wrap_strerror_r(1, s, size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
                else if (err != (err_expected = ERANGE))
                    wrong = 3;
            break;
            case 10:
                /*fun = sreturnf("strfmon(s, sizeof(s)-1, \"%%!i\", 123.0)");
                s = malloc(80);
                size = strfmon(s, 80, "%!i", 123.0) + 1;
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err=wrap_strfmon(s, size-1, "%!i", 123.0)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
                else if (err != (err_expected = E2BIG))
                    wrong = 3;*/
            break;
            case 11:
                fun = sreturnf("strftime(s, sizeof(s)-1, \"%Y\", tm)");
                s = malloc(80);
                tm = *gmtime((time_t []){time(NULL)});
                strftime(s, 80, "%Y", &tm);
                size = strlen(s) + 1;
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err=wrap_strftime(s, size-1, "%Y", &tm)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            case 12:
                fun = sreturnf("ttyname_r(STDERR_FILENO, s, sizeof(s)-1)");
                s = malloc(TTY_NAME_MAX);
                err = ttyname_r(STDERR_FILENO, s, TTY_NAME_MAX);
                size = strlen(s) + 1;
                free(s); s = NULL;
                if (size < 2) {
                    wrong = -2;
                    break;
                }
                s = malloc(size);
                s[size-1] = '\r';
                
                if ((err = wrap_ttyname_r(STDERR_FILENO, s, size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
                else if (err != (err_expected = ERANGE))
                    wrong = 3;
            break;
            case 13:
                fun = sreturnf("wcstombs(s, L\"abc\" 3)");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                if ((err=wrap_wcstombs(s, L"abcd", size-1)) == -1)
                    wrong = 1;
                else if (s[size-1] != '\r')
                    wrong = 2;
            break;
            default:
                function = 0; //exit while
            break;
        }
        if(((sig=safe_free(ws)) || (sig=safe_free(s))) && !wrong)
            wrong = 6;

        if (wrong) {
            ++failed;
            if (wrong == -2)
                fprintf(
                    stderr,
                    "%s could not be tested, because a preceeding call to the"
                    " same function returned length (of s) lesser than 2\n",
                    fun
                );
            else if (wrong == -1)
                fprintf(stderr, "%s is unable to run, opening \"%s\" failed\n",
                        fun, filename);
            else if (wrong == 1)
                fprintf(stderr, "%s caused a SIGSEGV!\n", fun);
            else if (wrong == 2)
                fprintf(stderr, "%s wrote too many bytes to the buffer\n",fun);
            else if (wrong == 3) {
                free(tmp);
                fprintf(stderr, "%s failed to set errno=%s",
                        fun, (tmp = e_name(err_expected)));
                free(tmp);
                fprintf(stderr, " (it is %s)\n", (tmp = e_name(err)));
            }
            else if (wrong == 4)
                fprintf(stderr,
                        "%s was not called,"
                        " symlink(\"%s\", \"%s.sym\") failed\n",
                        fun, filename, filename);
            else if (wrong == 5) {
                free(tmp);
                fprintf(stderr, "%s set errno = %s\n", fun, (tmp=e_name(err)));
            }
            else if (wrong == 6)
                fprintf(
                    stderr,
                    "%s caused a signal %d to be received. A pointer passed to"
                    " the function might be errorneously redirected\n",
                    fun, sig
                );
        }
        free(fun);
        free(tmp);
        if (stream != NULL && seq_has(function, ffun))
            fclose(stream);
    } while(function++);
    
    return failed;
}

///A bridge function for sigaction(), jumps via longjmp() back to a setjmp()
void bridge_sig_jmp(int sig)
{
    longjmp(env, sig);
    return;
}

///A SIGABRT/SIGSEGV - resistant free()
static int safe_free(void* vp) {
    int sig;
    struct sigaction
        act = {.sa_handler=bridge_sig_jmp},
        oldact[2];
    sigaction(SIGABRT, &act, &oldact[0]);
    sigaction(SIGSEGV, &act, &oldact[0]);
    if (!(sig = setjmp(env)))
        free(vp);
    sigaction(SIGSEGV, &oldact[0], NULL);
    sigaction(SIGABRT, &oldact[0], NULL);
    return sig;
}

///< Wrappers start here: They take the same~ arguments as the functions that
///< they call, but catch SIGSEGVs, and return error codes on failure
static int wrap_confstr(int name, char *buf, size_t len)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (confstr(name, buf, len)  ==  0)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_getcwd(char *buf, size_t size)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (getcwd(buf, size)  ==  NULL)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (getdelim(lineptr, n, delimiter, stream)  ==  -1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_gethostname(char *name, size_t namelen)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (gethostname(name, namelen)  ==  -1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_iconv(iconv_t cd, char **inbuf,  size_t *inbytesleft,
                                  char **outbuf, size_t *outbytesleft)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft)==(size_t)-1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_mbstowcs(wchar_t *pwcs, char *s, size_t n)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (mbstowcs(pwcs, s, n)  ==  (size_t)-1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_snprintf(char *s, size_t n, char *format/*, ...*/)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (snprintf(s, n, format)  <  0)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_readlink(char *path, char *buf, size_t bufsize)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (readlink(path, buf, bufsize)  ==  -1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
//static int wrap_strfmon(char *s, size_t maxsize, char *format/*, ...*/, float F)
/*{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (strfmon(s, maxsize, format, F)  ==  -1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}*/
static int wrap_strftime(char *s,size_t maxsize,char *format,struct tm *timeptr)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (strftime(s, maxsize, format, timeptr)  ==  0)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_wcstombs(char *s, wchar_t *pwcs, size_t n)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env)) {
            if (wcstombs(s, pwcs, n)  ==  (size_t)-1)
                err = errno;
        }else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env))
            err = strerror_r(errnum, strerrbuf, buflen);
        else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
static int wrap_ttyname_r(int fildes, char *name, size_t namesize)
{
        int err = 0;
        sigaction(SIGSEGV, &act, &oldact[0]);
        if(!setjmp(env))
            err = ttyname_r(fildes, name, namesize);
        else
            err = -1;
        sigaction(SIGSEGV, &oldact[0], NULL);
        return err;
}
