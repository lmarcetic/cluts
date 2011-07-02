#define XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <locale.h>
#include <stdarg.h>
#include <limits.h> //*LONG_MAX
#include <math.h>   //HUGE_VAL*
#include <wchar.h>
#include "common/sequence.h"

/**
 ** \file depends: fprintf, sprintf, vsnprintf, va_start, strcpy, strerror, strlen, wcslen, wcstombs, mbstowcs, malloc, free, setlocale
 ** \author Luka Marčetić, 2011
 **/

/** used to store expected return values of tested functions (see struct tests)
 ** these 'big' types are cast to appropriate ones in test_function
 **/
struct function_result {
    long long          ll;
    unsigned long long ull;
    long double        ld;
};
///used to identify respective functions
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
        fnr_wcstold
};

static int test_function (int function_nr, int base, const wchar_t *wnptr, struct function_result result, int error);
static char* e_name(int error);
static char* sreturnf(const char *format, ...);
static wchar_t* spp2ws(char *s);


/**
 **  Tests [wcs|str]to* functions' return values when given corner-case
 **  arguments and if they set correct errno values
 **/
int main()
{
    setlocale(LC_NUMERIC, "POSIX");
    int i, j, k;
    //note that given a sufficiently large base, x=34 and z=36:
    int
        b_zero[]         =  {1, 0},
        b_one[]          =  {1, 1},
        b_sixteen[]      =  {1, 16},
        b_hexadecimal[]  =  {2, 0,16},
        *b_all           =  seq_x(0,36, b_one);
    int
        *f_all           =  seq(fnr_strtoumax, fnr_wcstold),
        f_decimal[]      =  {6, fnr_strtof, fnr_strtod, fnr_strtold,
                                fnr_wcstof, fnr_wcstod, fnr_wcstold};
    struct function_result
        r_zero           = {0, 0, 0},
        r_one            = {1, 1, 1};
    
    struct tests {
        int                    *function_nrs; //a 'sequence' (see seq_x)
        wchar_t                *wnptr;
        int                    *bases;        //a 'sequence'
        struct function_result result;
        int                    error;
    } t[] = {
        
        //Universal strto tests:
            //functions  wnptr    bases                    result      error
            {f_all,      L"0 1",  b_all,                   r_zero,     0},
            {f_all,      L"01",   b_all,                   r_one,      0},
            {f_all,      L"0+1",  b_all,                   r_zero,     0},
            {f_all,      L"0-1",  b_all,                   r_zero,     0},
            {f_all,      L"00x1", seq_x(0,33, b_one),      r_zero,     0},
            {f_all,      L"0X0x", seq_x(0,33, b_one),      r_zero,     0},
            //standard says to match the first char making the rest invalid:
            {f_all,      L"- 1",  b_all,                   r_zero,     EINVAL},
            {f_all,      L"--1",  b_all,                   r_zero,     EINVAL},
            {f_all,      L"-+1",  b_all,                   r_zero,     EINVAL},
            {f_all,      L"+-1",  b_all,                   r_zero,     EINVAL},
            //the 'longest initial subsequence' is the invalid "0x":
            {f_all,      L"0x",   b_hexadecimal,           r_zero,     EINVAL},
            {f_all,      L"0x 1", b_hexadecimal,           r_zero,     EINVAL},
            {f_all,      L"0x+1", b_sixteen,               r_zero,     EINVAL},
            {f_all,      L"0x-1", b_sixteen,               r_zero,     EINVAL},
            {f_all,      L"0Xx",  b_sixteen,               r_zero,     EINVAL},
            {f_all,      L"0xX",  b_sixteen,               r_zero,     EINVAL},
            //should be a valid zero in a non-16 base, save bases>=34:
            {f_all,      L"0xX",  seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,      L"0Xx",  seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,      L"0x+1", seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,      L"0x-1", seq_x(0,33, b_sixteen),  r_zero,     0},
        
        //Decimal strto* tests:
            //functions  wnptr     (bases)  result          error
            {f_decimal,  L"0x.1",  b_zero,  {.ld=0.062500}, 0},
            {f_decimal,  L".10x1", b_zero,  {.ld=0.062500}, 0},
            //a 'non-empty sequence' miss ing before E/P:
            {f_decimal,  L"e1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"E1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"p1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"P1",    b_zero,  r_zero,         EINVAL},
            //missing exponent (+/- optional, the rest isn't):
            {f_decimal,  L"1e",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"1E",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1p",  b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1P",  b_zero,  r_zero,         EINVAL},
            
            {f_decimal,  L"1e+",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"1E+",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1p+", b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1P+", b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"1e-",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"1E-",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1p-", b_zero,  r_zero,         EINVAL},
            {f_decimal,  L"0x1P-", b_zero,  r_zero,         EINVAL},
            //'optional' does not make invalid, also x^0=1 o.O
            {f_decimal,  L"2e+0",  b_zero,  r_one,          0},
            {f_decimal,  L"2E+0",  b_zero,  r_one,          0},
            {f_decimal,  L"2e-0",  b_zero,  r_one,          0},
            {f_decimal,  L"2E-0",  b_zero,  r_one,          0},
        
        //Min/max value tests of individual functions:
            //function(!)                              wnptr                                   bases    result                error
            {(int[]){2, fnr_strtoimax, fnr_wcstoimax}, spp2ws(sreturnf("%jd",  INTMAX_MAX)),   b_zero,  {.ll = INTMAX_MAX},   ERANGE},
            {(int[]){2, fnr_strtoimax, fnr_wcstoimax}, spp2ws(sreturnf("%jd",  INTMAX_MIN)),   b_zero,  {.ll = INTMAX_MIN},   ERANGE},
            {(int[]){2, fnr_strtoumax, fnr_wcstoumax}, spp2ws(sreturnf("%ju",  UINTMAX_MAX)),  b_zero,  {.ull= UINTMAX_MAX},  ERANGE},
            {(int[]){2, fnr_strtol, fnr_wcstol},       spp2ws(sreturnf("%ld",  LONG_MAX)),     b_zero,  {.ll = LONG_MAX},     ERANGE},
            {(int[]){2, fnr_strtol, fnr_wcstol},       spp2ws(sreturnf("%ld",  LONG_MIN)),     b_zero,  {.ll = LONG_MIN},     ERANGE},
            {(int[]){2, fnr_strtoll, fnr_wcstoll},     spp2ws(sreturnf("%lld", LLONG_MAX)),    b_zero,  {.ll = LLONG_MAX},    ERANGE},
            {(int[]){2, fnr_strtoll, fnr_wcstoll},     spp2ws(sreturnf("%lld", LLONG_MIN)),    b_zero,  {.ll = LLONG_MIN},    ERANGE},
            {(int[]){2, fnr_strtoul, fnr_wcstoul},     spp2ws(sreturnf("%lu",  ULONG_MAX)),    b_zero,  {.ull= ULONG_MAX},    ERANGE},
            {(int[]){2, fnr_strtoull, fnr_wcstoull},   spp2ws(sreturnf("%llu", ULLONG_MAX)),   b_zero,  {.ull= ULLONG_MAX},   ERANGE},
            
            {(int[]){2, fnr_strtof, fnr_wcstof},       spp2ws(sreturnf("%f",   HUGE_VALF)),    b_zero,  {.ld = HUGE_VALF},    ERANGE},
            {(int[]){2, fnr_strtof, fnr_wcstof},       spp2ws(sreturnf("%f",   -HUGE_VALF)),   b_zero,  {.ld = -HUGE_VALF},   ERANGE},
            {(int[]){2, fnr_strtod, fnr_wcstod},       spp2ws(sreturnf("%f",   HUGE_VAL)),     b_zero,  {.ld = HUGE_VAL},     ERANGE},
            {(int[]){2, fnr_strtod, fnr_wcstod},       spp2ws(sreturnf("%f",   -HUGE_VAL)),    b_zero,  {.ld = -HUGE_VAL},    ERANGE},
            {(int[]){2, fnr_strtold, fnr_wcstold},     spp2ws(sreturnf("%Lf",  HUGE_VALL)),    b_zero,  {.ld = HUGE_VALL},    ERANGE},
            {(int[]){2, fnr_strtold, fnr_wcstold},     spp2ws(sreturnf("%Lf",  -HUGE_VALL)),   b_zero,  {.ld = -HUGE_VALL},   ERANGE},
    };
    
    //Execute the tests:
    for (i=0; i<(int)(sizeof(t)/sizeof(t[0])); ++i)
        for (j=0; j<t[i].function_nrs[0]; ++j)
            for (k=0; k<t[i].bases[0]; ++k)
                if (test_function(t[i].function_nrs[1+j], t[i].bases[1+k], t[i].wnptr, t[i].result, t[i].error))
                    break;
    
    return 0;
}

/**
 ** Preforms the actual tests, and prints the results. Takes:
 ** \param function_nr a 'number' of a function to be called (see function_nrs)
 ** \param base - the function preforms the test for a single base
 ** \params wnprtr, result, error - from struct tests
 ** \param result a pointer to the result(to be cast according to function_nr)
 ** \returns a non-zero on failure
 **/
static int test_function (int function_nr, int base, const wchar_t *wnptr, struct function_result result, int error)
{
    ///function names for the output (function_nr can be used as an index):
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
            "wcstold"
        };
    ///remembering return values
    struct f_return {
        union {
            uintmax_t ut;
            intmax_t it;
            long l;
            long long ll;
            unsigned long ul;
            unsigned long long ull;
            float f;
            double d;
            long double ld;
        } val;
    } r;
    //other stuff
    int
        err, wrong;
        f_decimal[]  =  {6, fnr_strtof, fnr_strtod, fnr_strtold,
                            fnr_wcstof, fnr_wcstod, fnr_wcstold};
        f_unsigned[] =  {6, fnr_strtoumax, fnr_strtoul, fnr_strtoull,
                            fnr_wcstoumax, fnr_wcstoul, fnr_wcstoull};
    wchar_t *wendptr;
    char *endptr, returned[80], *s, *nptr = malloc(sizeof(char) * wcslen(wnptr));
    wcstombs(nptr, wnptr, wcslen(wnptr));
    
    errno = err = wrong = 0;
    switch (function_nr)
    {
        case fnr_strtoumax:
        case fnr_wcstoumax:
            if (fnr_strtoumax)
                r.val.ut = strtoumax(nptr, &endptr, base);
            else
                r.val.ut = wcstoumax(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%ju", r.val.ut);
            if (r.val.ut != (uintmax_t)result.ull)
                wrong=1;
        break;
        case fnr_strtoimax:
        case fnr_wcstoimax:
            if (fnr_strtoimax)
                r.val.it = strtoimax(nptr, &endptr, base);
            else
                r.val.it = wcstoimax(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%jd", r.val.it);
            if (r.val.it != (intmax_t)result.ll)
                wrong=1;
        break;
        case fnr_strtol:
        case fnr_wcstol:
            if (fnr_strtol)
                r.val.l = strtol(nptr, &endptr, base);
            else
                r.val.l = wcstol(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%ld", r.val.l);
            if (r.val.l != (long)result.ll)
                wrong=1;
        break;
        case fnr_strtoll:
        case fnr_wcstoll:
            if (fnr_strtoll)
                r.val.ll = strtoll(nptr, &endptr, base);
            else
                r.val.ll = wcstoll(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%lld", r.val.ll);
            if (r.val.ll != (long long)result.ll)
                wrong=1;
        break;
        case fnr_strtoul:
        case fnr_wcstoul:
            if (fnr_strtoul)
                r.val.ul = strtoul(nptr, &endptr, base);
            else
                r.val.ul = wcstoul(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%lu", r.val.ul);
            if (r.val.ul != (unsigned long)result.ull)
                wrong=1;
        break;
        case fnr_strtoull:
        case fnr_wcstoull:
            if (fnr_strtoull)
                r.val.ull = strtoull(nptr, &endptr, base);
            else
                r.val.ull = wcstoull(wnptr, &wendptr, base);
            err = errno;
            sprintf(returned, "%llu", r.val.ull);
            if (r.val.ull != (unsigned long long)result.ull)
                wrong=1;
        break;
        case fnr_strtof:
        case fnr_wcstof:
            if (fnr_strtof)
                r.val.f = strtof(nptr, &endptr);
            else
                r.val.f = wcstof(wnptr, &wendptr);
            err = errno;
            sprintf(returned, "%f", r.val.f);
            if (r.val.f != (float)result.ld)
                wrong=1;
        break;
        case fnr_strtod:
        case fnr_wcstod:
            if (fnr_strtod)
                r.val.d = strtod(nptr, &endptr);
            else
                r.val.d = wcstod(wnptr, &wendptr);
            err = errno;
            sprintf(returned, "%f", r.val.d);
            if (r.val.d != (double)result.ld)
                wrong=1;
        break;
        case fnr_strtold:
        case fnr_wcstold:
            if (fnr_strtold)
                r.val.ld = strtold(nptr, &endptr);
            else
                r.val.ld = wcstold(wnptr, &wendptr);
            err = errno;
            sprintf(returned, "%Lf", r.val.ld);
            if (r.val.ld != (long double)result.ld)
                wrong=1;
        break;
        default:
            fprintf(stdout, "???%i???\n", function_nr);//---
            return 0;
        break;
    }
    if(error != error)
        wrong = 1;
    
    if (wrong) {
      s = e_name(error);
      if (seq_has(function_nr, f_unsigned))
          fprintf (
            stderr,
            "%s(\"%ls\", NULL, %i) should return %llu, errno=%s\n",
            f_names[function_nr], wnptr, base, result.ull, s
          );
      else if (seq_has(function_nr, f_decimal))
          fprintf (
            stderr,
            "%s(\"%ls\", NULL, %i) should return %lld, errno=%s\n",
            f_names[function_nr], wnptr, base, result.ll, s
          );
      else
          fprintf (
            stderr,
            "%s(\"%ls\", NULL) should return %Lf, errno=%s\n",
            f_names[function_nr], wnptr, result.ld, s
          );
      free(s);
      
      s = e_name(error);
      fprintf(stderr, "\tinstead, it returned %s, errno=%s\n", returned, s);
      free(s);
    }
    free(nptr);
    
    return wrong;
}

/**
 ** Returns a string with a human-readable error name
 ** \param error - errno to be "stringized"
 **/
static char* e_name(int error)
{
    char *s = malloc(80*sizeof(char));

    if (error == EINVAL)
        strcpy(s, "EINVAL");
    else if (error == ERANGE)
        strcpy(s, "ERANGE");
    else
        sprintf(s, "%i(%s)", error, strerror(error));
    return s;
}

/**
 ** A wrapper function for vsnprintf that automatically allocates space for a
 ** string and passes to it (max. 80 chars, counting the trailing '\0').
 ** \params (same as those for printf)
 ** \returns a pointer to the newly constructed string
 **/
static char* sreturnf(const char *format, ...)
{
    static const int s_size = 80*sizeof(char);
    char *s = malloc(s_size);
    va_list ap;
    
    va_start(ap, format);
    vsnprintf(s, s_size, format, ap);
    return s;
}

/**
 ** Increments the absolute value(that is, regardless of the +/- sign) of:
 ** \param s - a number passed as a string
 ** \returns a wide char equivalent of s
 ** note: will not enlarge the string, overflow is theoretically possible
 **/
static wchar_t* spp2ws(char *s)
{
    char *sp;
    wchar_t *ws=malloc(sizeof(wchar_t) * strlen(s));
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
    
    mbstowcs(ws, s, strlen(s));
    return ws;
}

//TODO: [w]scanf
