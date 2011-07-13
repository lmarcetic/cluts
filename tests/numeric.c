#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <locale.h>
#include <limits.h> //*LONG_MAX
#include <math.h>   //HUGE_VAL*
#include <wchar.h>
#include <float.h>  //[FLT|(L)DBL]_MAX
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
 ** depends: setlocale, malloc, free, fprintf, sprintf,
 **          strcpy, wcstombs, mbstowcs
 **/

/** used to store expected return values of tested functions (see struct tests)
 ** these 'big' types are cast to the appropriate ones in test_function
 **/
struct function_result {
    uintmax_t           ut;
    intmax_t            it;
    unsigned int        ui;
    long                l;
    long long           ll;
    unsigned long       ul;
    unsigned long long  ull;
    float               f;
    double              d;
    long double         ld;
};
///used to identify respective functions or arguments
enum function_nrs {
    fnr_strtoumax,
    fnr_strtoimax,
    fnr_strtol,
    fnr_strtoll,
    fnr_strtoul,
    fnr_strtoull,
    fnr_strtof,
    fnr_strtod,
    fnr_strtold,
    
    fnr_wcstoumax,
    fnr_wcstoimax,
    fnr_wcstol,
    fnr_wcstoll,
    fnr_wcstoul,
    fnr_wcstoull,
    fnr_wcstof,
    fnr_wcstod,
    fnr_wcstold,
    
    fnr_sscanfumax,
    fnr_sscanfimax,
    fnr_sscanfl,
    fnr_sscanfll,
    fnr_sscanful,
    fnr_sscanfull,
    fnr_sscanff,
    fnr_sscanfd,
    fnr_sscanfld,
    fnr_sscanfo,
    fnr_sscanfx
};

static int test_function(const int function_nr, const int base,
                         const wchar_t *wnptr, const void *x,
                         const struct function_result result, const int error);
static wchar_t* spp2ws(char *s);
static char* strhex(unsigned char *object, size_t length);

/**
 **  Tests [wcs|str]to* functions' and sscanf's return values when given
 **  corner-case arguments, as well as whether they set correct errno values
 **/
int main()
{
    setlocale(LC_NUMERIC, "POSIX");
    int i, j, k, err, failed, decimal;
    //note that given a sufficiently large base, x=34 and z=36:
    int
        b_zero[]           =  {1, 0},
        b_sixteen[]        =  {1, 16},
        *b_notsixteen      =  seq_x(0,33, b_sixteen),
        b_hexadecimal[]    =  {2, 0,16},
        *b_nothexadecimal  =  seq_x(0,33, b_hexadecimal),
        *b_all             =  seq(0,36);
    int
        *f_strwcsto           =  seq(fnr_strtoumax, fnr_wcstold),
        *f_sscanf             =  seq(fnr_sscanfumax, fnr_sscanfx),
        f_sscanf_decimal[]    =  {3, fnr_sscanff, fnr_sscanfd, fnr_sscanfld},
        f_strwcsto_decimal[]  =  {6, fnr_strtof, fnr_strtod, fnr_strtold,
                                     fnr_wcstof, fnr_wcstod, fnr_wcstold},
        f_sscanfx[]           =  {1, fnr_sscanfx},
        *f_sscanf_nonx        =  seq(fnr_sscanfumax, fnr_sscanfo);
    struct function_result
        r_zero    =  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        r_one     =  {1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        r_0_0625  =  {.f=0.0625, .d=0.0625, .ld=0.0625L},
        r_0_1     =  {.f=0.1,    .d=0.1,    .ld=0.1L};
    
    struct strwcsto_tests {
        int                    *function_nrs;
        wchar_t                *wnptr;
        size_t                 endptr_offset;
        int                    *bases;
        struct function_result result;
        int                    error;
    } t[] = {
        //Universal strto* tests:
        //functions   wnptr    end  bases             result   error
        {f_strwcsto,  L"0 1",  1,   b_all,            r_zero,  0},
        {f_strwcsto,  L"01",   2,   b_all,            r_one,   0},
        {f_strwcsto,  L"0+1",  1,   b_all,            r_zero,  0},
        {f_strwcsto,  L"0-1",  1,   b_all,            r_zero,  0},        
        //standard says to match the first char making the rest invalid:
        {f_strwcsto,  L"- 1",  0,   b_all,            r_zero,  -1},
        {f_strwcsto,  L"--1",  0,   b_all,            r_zero,  -1},
        {f_strwcsto,  L"-+1",  0,   b_all,            r_zero,  -1},
        {f_strwcsto,  L"+-1",  0,   b_all,            r_zero,  -1},
        //the 'longest initial subsequence' is the invalid "0x":
        {f_strwcsto,  L"0x",   0,   b_hexadecimal,    r_zero,  -1},
        {f_strwcsto,  L"0x 1", 0,   b_hexadecimal,    r_zero,  -1},
        {f_strwcsto,  L"0x+1", 0,   b_sixteen,        r_zero,  -1},
        {f_strwcsto,  L"0x-1", 0,   b_sixteen,        r_zero,  -1},
        {f_strwcsto,  L"0Xx",  0,   b_sixteen,        r_zero,  -1},
        {f_strwcsto,  L"0xX",  0,   b_sixteen,        r_zero,  -1},
        //should be a valid zero in a non-16 base, save bases>=34:
        {f_strwcsto,  L"00x1", 2,   b_notsixteen,     r_zero,  0},
        {f_strwcsto,  L"0xX",  1,   b_notsixteen,     r_zero,  0},
        {f_strwcsto,  L"0Xx",  1,   b_notsixteen,     r_zero,  0},
        {f_strwcsto,  L"0x+1", 1,   b_notsixteen,     r_zero,  0},
        {f_strwcsto,  L"0x-1", 1,   b_notsixteen,     r_zero,  0},
        {f_strwcsto,  L"0X0x", 1,   b_nothexadecimal, r_zero,  0},
    
        //Decimal strto* tests:
        //functions          wnptr     end (bases)     result        error
        {f_strwcsto_decimal, L"0x.1",  4,  b_sixteen,  r_0_0625,     0},
        {f_strwcsto_decimal, L".10x1", 3,  b_sixteen,  r_0_1,        0},
        //a 'non-empty sequence' missing before E/P:
        {f_strwcsto_decimal, L"e1",    0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"E1",    0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"p1",    0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"P1",    0,  b_sixteen,  r_zero,       -1},
        //missing exponent (+/- optional, the rest isn't):
        {f_strwcsto_decimal, L"1e",    0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"1E",    0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1p",  0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1P",  0,  b_sixteen,  r_zero,       -1},
        
        {f_strwcsto_decimal, L"1e+",   0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"1E+",   0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1p+", 0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1P+", 0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"1e-",   0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"1E-",   0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1p-", 0,  b_sixteen,  r_zero,       -1},
        {f_strwcsto_decimal, L"0x1P-", 0,  b_sixteen,  r_zero,       -1},
        //'optional' does not make invalid:
        {f_strwcsto_decimal, L"1e+0",  4,  b_sixteen,  r_one,        0},
        {f_strwcsto_decimal, L"1E+0",  4,  b_sixteen,  r_one,        0},
        {f_strwcsto_decimal, L"1e-0",  4,  b_sixteen,  r_one,        0},
        {f_strwcsto_decimal, L"1E-0",  4,  b_sixteen,  r_one,        0},
    
        //Min/max value tests of individual functions(code's wide here,i know):
        //functions                               wnptr                                 end bases      result               error
        {(int[]){2,fnr_strtoimax,fnr_wcstoimax},  spp2ws(sreturnf("%jd",  INTMAX_MAX)),  0, b_zero,    {.it = INTMAX_MAX},  ERANGE},
        {(int[]){2,fnr_strtoimax,fnr_wcstoimax},  spp2ws(sreturnf("%jd",  INTMAX_MIN)),  0, b_zero,    {.it = INTMAX_MIN},  ERANGE},
        {(int[]){2,fnr_strtoumax,fnr_wcstoumax},  spp2ws(sreturnf("%ju",  UINTMAX_MAX)), 0, b_zero,    {.ut = UINTMAX_MAX}, ERANGE},
        {(int[]){2,fnr_strtol,   fnr_wcstol},     spp2ws(sreturnf("%ld",  LONG_MAX)),    0, b_zero,    {.l  = LONG_MAX},    ERANGE},
        {(int[]){2,fnr_strtol,   fnr_wcstol},     spp2ws(sreturnf("%ld",  LONG_MIN)),    0, b_zero,    {.l  = LONG_MIN},    ERANGE},
        {(int[]){2,fnr_strtoll,  fnr_wcstoll},    spp2ws(sreturnf("%lld", LLONG_MAX)),   0, b_zero,    {.ll = LLONG_MAX},   ERANGE},
        {(int[]){2,fnr_strtoll,  fnr_wcstoll},    spp2ws(sreturnf("%lld", LLONG_MIN)),   0, b_zero,    {.ll = LLONG_MIN},   ERANGE},
        {(int[]){2,fnr_strtoul,  fnr_wcstoul},    spp2ws(sreturnf("%lu",  ULONG_MAX)),   0, b_zero,    {.ul = ULONG_MAX},   ERANGE},
        {(int[]){2,fnr_strtoull, fnr_wcstoull},   spp2ws(sreturnf("%llu", ULLONG_MAX)),  0, b_zero,    {.ull= ULLONG_MAX},  ERANGE},
        
        {(int[]){2,fnr_strtof,   fnr_wcstof},     spp2ws(sreturnf("%f",   FLT_MAX)),     0, b_sixteen, {.f  = HUGE_VALF},   ERANGE},
        {(int[]){2,fnr_strtof,   fnr_wcstof},     spp2ws(sreturnf("%f",   FLT_MIN)),     0, b_sixteen, {.f  = -HUGE_VALF},  ERANGE},
        {(int[]){2,fnr_strtod,   fnr_wcstod},     spp2ws(sreturnf("%lf",  DBL_MAX)),     0, b_sixteen, {.d  = HUGE_VAL},    ERANGE},
        {(int[]){2,fnr_strtod,   fnr_wcstod},     spp2ws(sreturnf("%lf",  DBL_MIN)),     0, b_sixteen, {.d  = -HUGE_VAL},   ERANGE},
        {(int[]){2,fnr_strtold,  fnr_wcstold},    spp2ws(sreturnf("%Lf",  LDBL_MAX)),    0, b_sixteen, {.ld = HUGE_VALL},   ERANGE},
        {(int[]){2,fnr_strtold,  fnr_wcstold},    spp2ws(sreturnf("%Lf",  LDBL_MIN)),    0, b_sixteen, {.ld = -HUGE_VALL},  ERANGE},
    };
    
    struct sscanf_tests {
        int                    *function_nrs;
        wchar_t                *wnptr;
        int                    sscanf_return;
        struct function_result result;
        int                    error;
    } t2[] = {
        //functions         wnptr    s_r  result   error
        {f_sscanf,          L"0 1",  1,   r_zero,  0},
        {f_sscanf,          L"01",   1,   r_one,   0},
        {f_sscanf,          L"0+1",  1,   r_zero,  0},
        {f_sscanf,          L"0-1",  1,   r_zero,  0},
        {f_sscanf,          L"00x1", 1,   r_zero,  0},
        {f_sscanf,          L"0X0x", 1,   r_zero,  0},
        //standard says to match the first char making the rest invalid:
        {f_sscanf,          L"- 1",  0,   r_zero,  -1},
        {f_sscanf,          L"--1",  0,   r_zero,  -1},
        {f_sscanf,          L"-+1",  0,   r_zero,  -1},
        {f_sscanf,          L"+-1",  0,   r_zero,  -1},
        //the 'longest initial subsequence' is the invalid "0x":
        {f_sscanfx,         L"0x",   0,   r_zero,  -1},
        {f_sscanfx,         L"0x 1", 0,   r_zero,  -1},
        {f_sscanfx,         L"0x+1", 0,   r_zero,  -1},
        {f_sscanfx,         L"0x-1", 0,   r_zero,  -1},
        {f_sscanfx,         L"0Xx",  0,   r_zero,  -1},
        {f_sscanfx,         L"0xX",  0,   r_zero,  -1},
        //should be a valid zero interpreted as a non-16 base number:
        {f_sscanf_nonx,     L"0xX",  1,   r_zero,  0},
        {f_sscanf_nonx,     L"0Xx",  1,   r_zero,  0},
        {f_sscanf_nonx,     L"0x+1", 1,   r_zero,  0},
        {f_sscanf_nonx,     L"0x-1", 1,   r_zero,  0},
    
        //Decimal sscanf tests:
        //functions         wnptr     s_r  result    error
        {f_sscanf_decimal,  L"0x.1",  1,   r_0_0625, 0},
        {f_sscanf_decimal,  L".10x1", 1,   r_0_1,    0},
        //a 'non-empty sequence' missing before E/P:
        {f_sscanf_decimal,  L"e1",    0,   r_zero,   0},
        {f_sscanf_decimal,  L"E1",    0,   r_zero,   0},
        {f_sscanf_decimal,  L"p1",    0,   r_zero,   0},
        {f_sscanf_decimal,  L"P1",    0,   r_zero,   0},
        //missing exponent (+/- optional, the rest isn't):
        {f_sscanf_decimal,  L"1e",    0,   r_zero,   0},
        {f_sscanf_decimal,  L"1E",    0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1p",  0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1P",  0,   r_zero,   0},
        
        {f_sscanf_decimal,  L"1e+",   0,   r_zero,   0},
        {f_sscanf_decimal,  L"1E+",   0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1p+", 0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1P+", 0,   r_zero,   0},
        {f_sscanf_decimal,  L"1e-",   0,   r_zero,   0},
        {f_sscanf_decimal,  L"1E-",   0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1p-", 0,   r_zero,   0},
        {f_sscanf_decimal,  L"0x1P-", 0,   r_zero,   0},
        //'optional' does not make invalid:
        {f_sscanf_decimal,  L"1e+0",  1,   r_one,    0},
        {f_sscanf_decimal,  L"1E+0",  1,   r_one,    0},
        {f_sscanf_decimal,  L"1e-0",  1,   r_one,    0},
        {f_sscanf_decimal,  L"1E-0",  1,   r_one,    0},
    };
    
    failed=0;
    //Execute strwcsto tests (decimal functions only if testing base 16 too):
    for (i=0; i<(int)(sizeof(t)/sizeof(t[0])); ++i) {
        err = 0;
        for (j=1; !err && j<=t[i].function_nrs[0]; ++j) {
            decimal = seq_has(t[i].function_nrs[j], f_strwcsto_decimal);
            for (k=1; !err && k <= t[i].bases[0]; ++k)
                if (t[i].bases[k] != 1 && (!decimal || t[i].bases[k]==16))
                    err = test_function(
                                        t[i].function_nrs[j],
                                        t[i].bases[k],
                                        t[i].wnptr,
                                        &t[i].endptr_offset,
                                        t[i].result,
                                        t[i].error
                    );
        }
        if (err)
            ++failed;
    }
    
    //Execute sscanf tests:
    for (i=0; i<(int)(sizeof(t2)/sizeof(t2[0])); ++i) {
        err = 0;
        for (j=1; !err && j<=t2[i].function_nrs[0]; ++j)
            err = test_function(
                                t2[i].function_nrs[j],
                                0,
                                t2[i].wnptr,
                                &t2[i].sscanf_return,
                                t2[i].result,
                                t2[i].error
            );
        if (err)
            ++failed;
    }
    
    return failed;
}

/**
 ** Preforms the actual tests, and prints the results. Takes:
 ** \param function_nr a 'number' of a function to be called (see function_nrs)
 ** \param base - the function preforms the test for a single base
 ** \params wnprtr,result,error - from struct [strwcsto|sscanf]_tests
 ** \param x - a void pointer to either endptr_offset or sscanf_return
 ** \param result a pointer to the result(to be cast according to function_nr)
 ** \returns a non-zero on failure
 **/
static int test_function(const int function_nr, const int base,
                         const wchar_t *wnptr, const void *x,
                         const struct function_result result, const int error)
{
    ///function names/formats for the output (function_nr used as the index):
    static const char
        *f_names[] = {
            "strtoumax",
            "strtoimax",
            "strtol",
            "strtoll",
            "strtoul",
            "strtoull",
            "strtof",
            "strtod",
            "strtold",
            
            "wcstoumax",
            "wcstoimax",
            "wcstol",
            "wcstoll",
            "wcstoul",
            "wcstoull",
            "wcstof",
            "wcstod",
            "wcstold",
            
            "%ju",
            "%jd",
            "%ld",
            "%lld",
            "%lu",
            "%llu",
            "%f",
            "%lf",
            "%Lf",
            "%o",
            "%u"
        };
    ///remembering return values
    struct function_result rval;
    //other stuff
    int err, wrong;
    int
        *f_sscanf    =  seq(fnr_sscanfumax, fnr_sscanfx),
        f_decimal[]  =  {9, fnr_strtof,  fnr_strtod,  fnr_strtold,
                            fnr_wcstof,  fnr_wcstod,  fnr_wcstold,
                            fnr_sscanff, fnr_sscanfd, fnr_sscanfld},
        *f_wide      =  seq(fnr_wcstoumax, fnr_wcstold);
    size_t wnptr_size = wcstombs(NULL, wnptr, 0)+1;
    char *endptr, *resulted, *expected, *s,
         *nptr = malloc(sizeof(char) + wnptr_size);
    wcstombs(nptr, wnptr, wnptr_size);
    wchar_t *wendptr;
    size_t off, endptr_offset;
    int ret, sscanf_return;
    
    errno = err = wrong = 0;
    switch (function_nr)
    {
        case fnr_strtoumax:
        case fnr_wcstoumax:
        case fnr_sscanfumax:
            if (function_nr == fnr_strtoumax)
                rval.ut = strtoumax(nptr, &endptr, base);
            else if (function_nr == fnr_wcstoumax)
                rval.ut = wcstoumax(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%ju", &rval.ut);
            err = errno;
            
            if (rval.ut != result.ut || (error!=-1 && err!=error)){
                resulted = sreturnf("%ju", rval.ut);
                expected = sreturnf("%ju", result.ut);
                wrong = 1;
            }
        break;
        case fnr_strtoimax:
        case fnr_wcstoimax:
        case fnr_sscanfimax:
            if (function_nr == fnr_strtoimax)
                rval.it = strtoimax(nptr, &endptr, base);
            else if (function_nr == fnr_wcstoimax)
                rval.it = wcstoimax(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%jd", &rval.it);
            err = errno;
            
            if (rval.it != result.it || (error!=-1 && err!=error)){
                resulted = sreturnf("%jd", rval.it);
                expected = sreturnf("%jd", result.it);
                wrong = 1;
			}
        break;
        case fnr_strtol:
        case fnr_wcstol:
        case fnr_sscanfl:
            if (function_nr == fnr_strtol)
                rval.l = strtol(nptr, &endptr, base);
            else if (function_nr == fnr_wcstol)
                rval.l = wcstol(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%ld", &rval.l);
            err = errno;
            
            if (rval.l != result.l || (error!=-1 && err!=error)){
                resulted = sreturnf("%ld", rval.l);
                expected = sreturnf("%ld", result.l);
                wrong = 1;
			}
        break;
        case fnr_strtoll:
        case fnr_wcstoll:
        case fnr_sscanfll:
            if (function_nr == fnr_strtoll)
                rval.ll = strtoll(nptr, &endptr, base);
            else if (function_nr == fnr_wcstoll)
                rval.ll = wcstoll(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%lld", &rval.ll);
            err = errno;
            
            if (rval.ll != result.ll || (error!=-1 && err!=error)){
                resulted = sreturnf("%lld", rval.ll);
                expected = sreturnf("%lld", result.ll);
                wrong = 1;
			}
        break;
        case fnr_strtoul:
        case fnr_wcstoul:
        case fnr_sscanful:
            if (function_nr == fnr_strtoul)
                rval.ul = strtoul(nptr, &endptr, base);
            else if (function_nr == fnr_wcstoul)
                rval.ul = wcstoul(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%lu", &rval.ul);
            err = errno;
            
            if(rval.ul != result.ul || (error!=-1 && err!=error)){
                resulted = sreturnf("%lu", rval.ul);
                expected = sreturnf("%lu", result.ul);
                wrong = 1;
			}
        break;
        case fnr_strtoull:
        case fnr_wcstoull:
        case fnr_sscanfull:
            if (function_nr == fnr_strtoull)
                rval.ull = strtoull(nptr, &endptr, base);
            else if (function_nr == fnr_wcstoull)
                rval.ull = wcstoull(wnptr, &wendptr, base);
            else
                ret = sscanf(nptr, "%llu", &rval.ull);
            err = errno;
            
            if (rval.ull != result.ull || (error!=-1 && err!=error)){
                resulted = sreturnf("%llu", rval.ull);
                expected = sreturnf("%llu", result.ull);
                wrong = 1;
			}
        break;
        case fnr_strtof:
        case fnr_wcstof:
        case fnr_sscanff:
            if (function_nr == fnr_strtof)
                rval.f = strtof(nptr, &endptr);
            else if (function_nr == fnr_wcstof)
                rval.f = wcstof(wnptr, &wendptr);
            else
                ret = sscanf("1e-", "%f", &rval.f);
            err = errno;
            
            if (rval.f != result.f || (error!=-1 && err!=error)){
                resulted = sreturnf("%f(%a)", rval.f, (double)rval.f);
                expected = sreturnf("%f(%a)", result.f, (double)result.f);
                wrong = 1;
			}
        break;
        case fnr_strtod:
        case fnr_wcstod:
        case fnr_sscanfd:
            if (function_nr == fnr_strtod)
                rval.d = strtod(nptr, &endptr);
            else if (function_nr == fnr_wcstod)
                rval.d = wcstod(wnptr, &wendptr);
            else
                ret = sscanf(nptr, "%lf", &rval.d);
            err = errno;
            
            if (rval.d != result.d || (error!=-1 && err!=error)){
                resulted = sreturnf("%lf(%a)", rval.d, rval.d);
                expected = sreturnf("%lf(%a)", result.d, result.d);
                wrong = 1;
			}
        break;
        case fnr_strtold:
        case fnr_wcstold:
        case fnr_sscanfld:
            if (function_nr == fnr_strtold)
                rval.ld = strtold(nptr, &endptr);
            else if (function_nr == fnr_wcstold)
                rval.ld = wcstold(wnptr, &wendptr);
            else
                ret = sscanf(nptr, "%Lf", &rval.ld);
            err = errno;
            
            if (rval.ld != result.ld || (error!=-1 && err!=error)){
                s = strhex((unsigned char*)&rval.ld, sizeof(rval.ld));
                resulted = sreturnf("%Lf(%s)", rval.ld, s);
                free(s);
                s = strhex((unsigned char*)&result.ld, sizeof(result.ld));
                expected = sreturnf("%Lf(%s)", result.ld, s);
                free(s);
                wrong = 1;
			}
        break;
        case fnr_sscanfo:
        case fnr_sscanfx:
            if (function_nr == fnr_sscanfo)
                ret = sscanf(nptr, "%o", &rval.ui);
            else if (function_nr == fnr_sscanfx)
                ret = sscanf(nptr, "%d", &rval.ui);
            err = errno;
            
            if (rval.ui != result.ui || (error!=-1 && err!=error)){
                resulted = sreturnf("%u(%o)", rval.ui, rval.ui);
                expected = sreturnf("%u(%a)", result.ui, rval.ui);
                wrong = 1;
			}
        break;
        default:
            fprintf(stdout, "???%i???\n", function_nr);//---
            return 0;
        break;
    }
    
    if (wrong) {
        s = e_name(error);
        if (!seq_has(function_nr, f_sscanf)) {
            fprintf(stderr, "%s(\"%ls\", &endptr", f_names[function_nr],wnptr);
            if (seq_has(function_nr, f_decimal))
                fprintf(stderr, ", %d", base);
            fprintf(stderr, ") should return %s, errno=%s\n", expected, s);
            fprintf(stderr,"\tinstead, it returns");
        } else {
            fprintf(
                stderr,
                "sscanf(\"%ls\", \"%s\", ...) should produce %s, errno=%s\n",
                wnptr, f_names[function_nr], expected, s
            );
            fprintf(stderr,"\tinstead, it produces");
        }
        free(s);
        fprintf(stderr, " %s, errno=%s\n", resulted, e_name(err));
        free(resulted);
        free(expected);
    } else if (seq_has(function_nr, f_sscanf)) {
        sscanf_return = *((int *)x);
        if (ret != sscanf_return)
        {
            fprintf (
                stderr,
                "sscanf(\"%ls\", \"%s\", ...) returns %d instead of %d\n",
                wnptr, f_names[function_nr], ret, sscanf_return
            );
        }
    } else {
        endptr_offset = *((size_t *)x);
        if(seq_has(function_nr, f_wide))
          off = (size_t)wendptr/sizeof(wchar_t) -(size_t)wnptr/sizeof(wchar_t);
        else
          off = (size_t)endptr/sizeof(char) -(size_t)nptr/sizeof(char);
        
        if (off != endptr_offset)
        {
            wrong = 1;
            fprintf(stderr, "%s(\"%ls\", &endptr", f_names[function_nr],wnptr);
            if(!seq_has(function_nr, f_decimal))
                fprintf(stderr, "%i", base);
            fprintf (
                stderr,
                ") offsets endptr by %zu instead of by %zu\n",
                off, endptr_offset
            );
        }
    }
    free(nptr);
    
    return wrong;
}

/**
 ** Increments the 'absolute' value(that is, regardless of the +/- sign) of:
 ** \param s - a number passed as a string (WILL BE FREED AFTERWARDS)
 ** \returns a wide char equivalent of s
 ** note: will not enlarge the string, overflow is theoretically possible
 **/
static wchar_t* spp2ws(char *s)
{
    char *sp;
    int ws_size = mbstowcs(NULL, s, 0)+1;
    wchar_t *ws = malloc(sizeof(wchar_t) * ws_size);
    int carry;
    
    for (sp=s; *(sp+1)!='\0'; ++sp);
    carry=1;
    while (carry  &&  *sp>='0' && *sp<='9'  &&  sp>=s) {
        *sp += carry;
        if (*sp>'9') {
            *sp -= '9';
            carry=1;
        }
        else
            carry=0;
        --sp;
    }
    
    mbstowcs(ws, s, ws_size);
    free(s);
    return ws;
}

/**
 ** Represents bytes as a string of 'hexadecimal numerals'
 ** \param object - pointer to the memory to be represented
 ** \param length - the number of bytes to be processed
 ** \returns a string with the hexadecimal representation
 **/
static char* strhex(unsigned char *object, size_t length)
{
    unsigned int i;
    char *s = malloc(sizeof(char) * length*2+1);
    
    for (i=0; i<length; ++i)
            sprintf(&s[i*2], "%.2x", object[i]);
    s[i*2]='\0';
    
    return s;
}
