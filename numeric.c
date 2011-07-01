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

/**
 ** \file depends: fprintf, sprintf, vsnprintf, va_start, strcpy, strerror, malloc, free, setlocale
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
        fnr_strtold
};

static int test_function (int function_nr, int base, char *nptr, struct function_result result, int error);
static int* seq(int from, int to);
static int* seq_x(int from, int to, int *exclude);
static int* seq_1(int the);
static char* e_name(int error);
static char* sreturnf(const char *format, ...);
static char* incs(char *s);

/**
 **  Tests strto* functions' return values when given corner-case arguments
 **  Likewise, it tests if they set correct errno values
 **/
int main()
{
    setlocale(LC_NUMERIC, "POSIX");
    unsigned int i, j, k;
    //note that given a sufficiently large base, x=34 and z=36:
    int
        b_zero[]         =  {0, -1},
        b_one[]          =  {1, -1},
        b_sixteen[]      =  {16, -1},
        b_hexadecimal[]  =  {0, 16, -1},
        *b_all           =  seq_x(0,36, b_one);
    int
        *f_all           =  seq(fnr_strtoumax, fnr_strtold),
        f_decimal[]      =  {fnr_strtof, fnr_strtod, fnr_strtold, -1};
    struct function_result
        r_zero           = {0, 0, 0},
        r_one            = {1, 1, 1};
    
    struct tests {
        int                    *function_nrs; //a 'sequence' (see seq_x)
        char                   *nptr;
        int                    *bases;        //a 'sequence'
        struct function_result result;
        int                    error;
    } t[] = {
        
        //Universal strto tests:
            //functions     nptr    bases                    result      error
            {f_all,         "0 1",  b_all,                   r_zero,     0},
            {f_all,         "01",   b_all,                   r_one,      0},
            {f_all,         "0+1",  b_all,                   r_zero,     0},
            {f_all,         "0-1",  b_all,                   r_zero,     0},
            {f_all,         "00x1", seq_x(0,33, b_one),      r_zero,     0},
            {f_all,         "0X0x", seq_x(0,33, b_one),      r_zero,     0},
            //standard says to match the first char making the rest invalid:
            {f_all,         "- 1",  b_all,                   r_zero,     EINVAL},
            {f_all,         "--1",  b_all,                   r_zero,     EINVAL},
            {f_all,         "-+1",  b_all,                   r_zero,     EINVAL},
            {f_all,         "+-1",  b_all,                   r_zero,     EINVAL},
            //the 'longest initial subsequence' is the invalid "0x":
            {f_all,         "0x",   b_hexadecimal,           r_zero,     EINVAL},
            {f_all,         "0x 1", b_hexadecimal,           r_zero,     EINVAL},
            {f_all,         "0x+1", b_sixteen,               r_zero,     EINVAL},
            {f_all,         "0x-1", b_sixteen,               r_zero,     EINVAL},
            {f_all,         "0Xx",  b_sixteen,               r_zero,     EINVAL},
            {f_all,         "0xX",  b_sixteen,               r_zero,     EINVAL},
            //should be a valid zero in a non-16 base, save bases>=34:
            {f_all,         "0xX",  seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,         "0Xx",  seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,         "0x+1", seq_x(0,33, b_sixteen),  r_zero,     0},
            {f_all,         "0x-1", seq_x(0,33, b_sixteen),  r_zero,     0},
        
        //Decimal strto* tests:
            //functions  nptr     (bases)  result          error
            {f_decimal,  "0x.1",  b_zero,  {.ld=0.062500}, 0},
            {f_decimal,  ".10x1", b_zero,  {.ld=0.062500}, 0},
            //a 'non-empty sequence' miss ing before E/P:
            {f_decimal,  "e1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  "E1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  "p1",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  "P1",    b_zero,  r_zero,         EINVAL},
            //missing exponent (+/- optional, the rest isn't):
            {f_decimal,  "1e",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  "1E",    b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1p",  b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1P",  b_zero,  r_zero,         EINVAL},
            
            {f_decimal,  "1e+",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  "1E+",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1p+", b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1P+", b_zero,  r_zero,         EINVAL},
            {f_decimal,  "1e-",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  "1E-",   b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1p-", b_zero,  r_zero,         EINVAL},
            {f_decimal,  "0x1P-", b_zero,  r_zero,         EINVAL},
            //'optional' does not make invalid, also x^0=1 o.O
            {f_decimal,  "2e+0",  b_zero,  r_one,          0},
            {f_decimal,  "2E+0",  b_zero,  r_one,          0},
            {f_decimal,  "2e-0",  b_zero,  r_one,          0},
            {f_decimal,  "2E-0",  b_zero,  r_one,          0},
        
        //Min/max value tests of individual functions:
            //function(!)           nptr                                  bases    result                error
            {seq_1(fnr_strtoimax),  incs(sreturnf("%jd",  INTMAX_MAX)),   b_zero,  {.ll = INTMAX_MAX},   ERANGE},
            {seq_1(fnr_strtoimax),  incs(sreturnf("%jd",  INTMAX_MIN)),   b_zero,  {.ll = INTMAX_MIN},   ERANGE},
            {seq_1(fnr_strtoumax),  incs(sreturnf("%ju",  UINTMAX_MAX)),  b_zero,  {.ull= UINTMAX_MAX},  ERANGE},
            {seq_1(fnr_strtol),     incs(sreturnf("%ld",  LONG_MAX)),     b_zero,  {.ll = LONG_MAX},     ERANGE},
            {seq_1(fnr_strtol),     incs(sreturnf("%ld",  LONG_MIN)),     b_zero,  {.ll = LONG_MIN},     ERANGE},
            {seq_1(fnr_strtoll),    incs(sreturnf("%lld", LLONG_MAX)),    b_zero,  {.ll = LLONG_MAX},    ERANGE},
            {seq_1(fnr_strtoll),    incs(sreturnf("%lld", LLONG_MIN)),    b_zero,  {.ll = LLONG_MIN},    ERANGE},
            {seq_1(fnr_strtoul),    incs(sreturnf("%lu",  ULONG_MAX)),    b_zero,  {.ull= ULONG_MAX},    ERANGE},
            {seq_1(fnr_strtoull),   incs(sreturnf("%llu", ULLONG_MAX)),   b_zero,  {.ull= ULLONG_MAX},   ERANGE},
            
            {seq_1(fnr_strtof),     incs(sreturnf("%f",   HUGE_VALF)),    b_zero,  {.ld = HUGE_VALF},    ERANGE},
            {seq_1(fnr_strtof),     incs(sreturnf("%f",   -HUGE_VALF)),   b_zero,  {.ld = -HUGE_VALF},   ERANGE},
            {seq_1(fnr_strtod),     incs(sreturnf("%f",   HUGE_VAL)),     b_zero,  {.ld = HUGE_VAL},     ERANGE},
            {seq_1(fnr_strtod),     incs(sreturnf("%f",   -HUGE_VAL)),    b_zero,  {.ld = -HUGE_VAL},    ERANGE},
            {seq_1(fnr_strtold),    incs(sreturnf("%Lf",  HUGE_VALL)),    b_zero,  {.ld = HUGE_VALL},    ERANGE},
            {seq_1(fnr_strtold),    incs(sreturnf("%Lf",  -HUGE_VALL)),   b_zero,  {.ld = -HUGE_VALL},   ERANGE},
    };
    
    //Execute the tests:
    for (i=0; i<sizeof(t)/sizeof(struct tests); ++i)
        for (j=0; t[i].function_nrs[j]!=-1; ++j)
            for (k=0; t[i].bases[k]!=-1; ++k)
                test_function(t[i].function_nrs[j], t[i].bases[k], t[i].nptr, t[i].result, t[i].error);
    
    return 0;
}

/**
 ** Preforms the actual tests, takes:
 ** \param function_nr a 'number' of a function to be called (see function_nrs)
 ** \param base - this function preforms the test for a single base
 ** \params nprtr, result, error - from struct tests
 ** \param result a pointer to the result(to be cast according to function_nr)
 ** \returns a non-zero on failure
 **/
static int test_function (int function_nr, int base, char *nptr, struct function_result result, int error)
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
            "strtold"
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
    int err, wrong;
    
    char *endptr, returned[80], *s;
    errno = err = wrong = 0;
    switch (function_nr)
    {
        case fnr_strtoumax:
            r.val.ut = strtoumax(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%ju", r.val.ut);
            if (r.val.ut != (uintmax_t)result.ull)
                wrong=1;
        break;
        case fnr_strtoimax:
            r.val.it = strtoimax(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%jd", r.val.it);
            if (r.val.it != (intmax_t)result.ll)
                wrong=1;
        break;
        case fnr_strtol:
            r.val.l = strtol(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%ld", r.val.l);
            if (r.val.l != (long)result.ll)
                wrong=1;
        break;
        case fnr_strtoll:
            r.val.ll = strtoll(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%lld", r.val.ll);
            if (r.val.ll != (long long)result.ll)
                wrong=1;
        break;
        case fnr_strtoul:
            r.val.ul = strtoul(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%lu", r.val.ul);
            if (r.val.ul != (unsigned long)result.ull)
                wrong=1;
        break;
        case fnr_strtoull:
            r.val.ull = strtoull(nptr, &endptr, base);
            err = errno;
            sprintf(returned, "%llu", r.val.ull);
            if (r.val.ull != (unsigned long long)result.ull)
                wrong=1;
        break;
        case fnr_strtof:
            r.val.f = strtof(nptr, &endptr);
            err = errno;
            sprintf(returned, "%f", r.val.f);
            if (r.val.f != (float)result.ld)
                wrong=1;
        break;
        case fnr_strtod:
            r.val.d = strtod(nptr, &endptr);
            err = errno;
            sprintf(returned, "%f", r.val.d);
            if (r.val.d != (double)result.ld)
                wrong=1;
        break;
        case fnr_strtold:
            r.val.ld = strtold(nptr, &endptr);
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
      if (function_nr<fnr_strtof) {
          if (   function_nr == fnr_strtoumax
              || function_nr == fnr_strtoul
              || function_nr == fnr_strtoull
          )
              fprintf (
                stderr,
                "%s(\"%s\", NULL, %i) should return %llu, errno=%s\n",
                f_names[function_nr], nptr, base, result.ull, s
              );
          else
              fprintf (
                stderr,
                "%s(\"%s\", NULL, %i) should return %lld, errno=%s\n",
                f_names[function_nr], nptr, base, result.ll, s
              );
      } else
          fprintf (
            stderr,
            "%s(\"%s\", NULL) should return %Lf, errno=%s\n",
            f_names[function_nr], nptr, result.ld, s
          );
      free(s);
      
      s = e_name(error);
      fprintf(stderr, "\tinstead, it returned %s, errno=%s\n", returned, s);
      free(s);
    }
    return wrong;
}

/**
 **  A "sequence" is a sorted tuple of integers, ending with a final -1
 **  \param from the lower bound
 **  \param to the upper bound
 **  \param exclude another sequence of integers to excluded from the new one
 **  \returns pointer to a sequence of integers from a set: {[from,to]/exclude}
 **/
static int* seq_x(int from, int to, int *exclude)
{
    int i, j=0, *sequence;
    for (i=0; exclude[i] != -1; ++i); //the length of exclude
    sequence = malloc(sizeof(int) * (to-from+1 - i));
    
    for (i=from; i<=to; ++i) {
        if (i != *exclude)
            sequence[j++] = i;
        else
            ++exclude;
    }
    sequence[j] = -1;
    
    return sequence;
}
///A handy auxiliary function, passes seq_x an empty exclude sequence
static int* seq(int from, int to)
{
    return seq_x(from, to, (int[]){-1});
}
///A handy auxiliary function for constructing single-member sequences
static int* seq_1(int the)
{
    return seq_x(the, the, (int[]){-1});
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
 ** \returns s
 ** note: will not enlarge the string, overflow is theoretically possible
 **/
static char* incs(char *s)
{
    char *sp;
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
    
    return s;
}

//TODO: [w]scanf and strto* wide counterparts (without adding fnr_w*?)
