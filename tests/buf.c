#define _POSIX_C_SOURCE 200809L
#include <errno.h>
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
 **        readlink, strerror_r, strptime
 ** depends: malloc, free, strlen, mkstemp, fileno, ftruncate,
 **          iconv_open, iconv_close, fdopen, strptime
 **/
int main()
{
    int function, wrong, failed, err, err_expected;
    char *s, *fun, filename[] = "/tmp/clutsXXXXXX";
    wchar_t *ws;
    size_t size;
    iconv_t cd;
    FILE *stream = NULL;
    int fd;
    struct tm tm;
    // numbers of those functions that depend on the temporary file:
    int ffun[] = {2, 3,8,12};
    
    if ((fd = mkstemp(filename)) != -1)
        stream = fdopen(fd, "w+b");
    
    failed = 0;
    function = 1;
    do {
        err = wrong = 0;
        s = fun = NULL;
        ws = NULL;
        switch(function) {
            case 1:
                fun = sreturnf("confstr(_CS_PATH, s, sizeof(s)-1)");
                size = confstr(_CS_PATH, NULL, 0);
                s = malloc(size);
                s[size-1] = '\r';
                
                confstr(_CS_PATH, s, size-1);
                /*if ((err = errno))
                    wrong = 4;
                else */if (s[size-1] != '\r')
                    wrong = 1;
            break;
            case 2:
                fun = sreturnf("getcwd(s, sizeof(s)-1)");
                s = malloc(PATH_MAX);
                size = strlen(getcwd(s, PATH_MAX))+1;
                s[size-1] = '\r';
                
                getcwd(s, size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = ERANGE))
                    wrong = 2;
            break;
            case 3:
                //same for getline
                fun = sreturnf("getdelim(&s, &<strlen(s)>, '\\n', stream)");
                if (stream == NULL)
                    break;
                ftruncate(fd, 0);
                fwrite("123\n", (size = 4), 1, stream);
                s = malloc(size);
                s[size-1] = '\r';
                
                err = getdelim(&s, (size_t[]){size-1}, '\n', stream);
                if (s[size-1] != '\r')
                    wrong = 1;
            break;
            case 4:
                fun = sreturnf("gethostname(s, sizeof(s)-1)");
                s = malloc(HOST_NAME_MAX);
                gethostname(s, HOST_NAME_MAX);
                size = strlen(s) + 1;
                s[size-1] = '\r';
                
                getcwd(s, size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = ERANGE))
                    wrong = 2;
            break;
            case 5:
                fun = sreturnf("iconv(&<\"abcd\">, &<4>, &s, &<3>)");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                cd = iconv_open("", "");
                iconv(cd, 
                      (char **)(char *[]){"abcd"},
                      (size_t *)(size_t []){size},
                      (char **)(char *[]){s},
                      (size_t *)(size_t []){size-1}
                );
                iconv_close(cd);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = E2BIG))
                    wrong = 2;
            break;
            case 6:
                fun = sreturnf("mbstowcs(ws, \"abc\", 3)");
                size = 4;
                ws = malloc(size * sizeof(wchar_t));
                ws[size-1] = L'\r';
                
                mbstowcs(ws, "abcd", size-1);
                if (ws[size-1] != L'\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = E2BIG))
                    wrong = 2;
            break;
            case 7:
                fun = sreturnf("snprintf(s, 3, \"abc\")");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                snprintf(s, size-1, "abc");
                if (s[size-1] != '\r')
                    wrong = 1;
            break;
            case 8:
                fun=sreturnf("readlink(\"%s.sym\", s, sizeof(s)-1)", filename);
                if (stream == NULL) {
                    wrong = -1;
                    break;
                }
                else if (symlink(filename, sreturnf("%s.sym", filename))) {
                    wrong = 3;
                    break;
                }
                s = malloc(PATH_MAX);
                size = readlink(sreturnf("%s.sym", filename), s, PATH_MAX);
                s[size-1] = '\r';
                
                readlink(sreturnf("%s.sym", filename), s, size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
            break;
            case 9:
                /*fun = sreturnf("strerror_r(1, s, sizeof(s)-1)");
                s = malloc(80);
                strerror_r(1, s, size-1); //glibc fail
                size = strlen(s);
                s[size-1] = '\r';
                
                strerror_r(1, s, size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = ERANGE))
                    wrong = 2;*/
            break;
            case 10:
                /*fun = sreturnf("strfmon(s, sizeof(s)-1, \"%%!i\", 123.0)");
                s = malloc(80);
                size = strfmon(s, 80, "%!i", 123.0) + 1;
                s[size-1] = '\r';
                
                strfmon(s, size-1, "%!i", 123.0);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = E2BIG))
                    wrong = 2;*/
            break;
            case 11:
                fun = sreturnf("strftime(s, sizeof(s)-1, \"%Y\", tm)");
                s = malloc(80);
                tm = *gmtime((time_t []){time(NULL)});
                strftime(s, 80, "%Y", &tm);
                size = strlen(s) + 1;
                s[size-1] = '\r';
                
                strftime(s, size-1, "%Y", &tm);
                if (s[size-1] != '\r')
                    wrong = 1;
            break;
            case 12:
                fun = sreturnf("ttyname_r(STDERR_FILENO, s, sizeof(s)-1)");
                s = malloc(TTY_NAME_MAX);
                ttyname_r(STDERR_FILENO, s, TTY_NAME_MAX);
                size = strlen(s) + 1;
                s[size-1] = '\r';
                
                ttyname_r(STDERR_FILENO, s, size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
                else if ((err = errno) != (err_expected = ERANGE))
                    wrong = 2;
            break;
            case 13:
                fun = sreturnf("wcstombs(s, L\"abc\" 3)");
                s = malloc((size = 4));
                s[size-1] = '\r';
                
                wcstombs(s, L"abcd", size-1);
                if (s[size-1] != '\r')
                    wrong = 1;
            break;
            default:
                function = 0; //exit while
            break;
        }
        if (stream == NULL && seq_has(function, ffun))
            wrong = -1;
        
        if (wrong) {
            ++failed;
            if (wrong == -1)
                fprintf(stderr, "%s is unable to run, opening \"%s\" failed\n",
                        fun, filename);
            else if (wrong == 1)
                fprintf(stderr, "%s wrote too many bytes to the buffer\n",fun);
            else if (wrong == 2)
                fprintf(stderr, "%s failed to set errno=%s (it is %s)\n",
                        fun, e_name(err_expected), e_name(err));
            else if (wrong == 3)
                fprintf(stderr, "%s was not called,"
                                " symlink(\"%s\", \"%s.sym\") failed\n",
                        fun, filename, filename);
            else if (wrong == 4)
                fprintf(stderr, "%s set errno = %s\n", fun, e_name(err));
        }
        free(ws);
        free(fun);
        free(s);
    } while(function++);
    
    return failed;
}
