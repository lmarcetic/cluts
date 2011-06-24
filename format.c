#define XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>
#include <locale.h>

/**
 ** \file depends: fprintf, strerror, malloc, setlocale
 ** \author Luka Marčetić, 2011
 **/

struct strto_tests {
    char        *nptr;
    int         *bases;
    long double result;
    int         error;
};

static int* seq(int from, int to);
static int* seq_x(int from, int to, int *exclude);
static int test_function (int function_nr, struct strto_tests t, int base);
static char* e_name(int error);

/**
 **  Tests strto* functions' return values when given corner-case arguments
 **  Likewise, it tests if they set correct errno values
 **/
int main()
{
    setlocale(LC_NUMERIC, "POSIX");
    unsigned int i, j;
    int *ip;
    //note that given a sufficiently large base, x=34 and z=36
    int hexadecimal[] = {0, 16, -1},
        sixteen[]     = {16, -1},
        *all_bases    = seq(0, 36);
    const struct strto_tests t[] = {
        //nptr   bases                  result  error
        {"0 1",  all_bases,             0,      0},
        {"01",   all_bases,             1,      0},
        {"0+1",  all_bases,             0,      0},
        {"0-1",  all_bases,             0,      0},
        {"00x1", seq(0,33),             0,      0},
        {"0X0x", seq(0,33),             0,      0},
        //the standard says to match the first char, making the rest invalid:
        {"- 1",  all_bases,             0,      EINVAL},
        {"--1",  all_bases,             0,      EINVAL},
        {"-+1",  all_bases,             0,      EINVAL},
        {"+-1",  all_bases,             0,      EINVAL},
        //the 'longest initial subsequence' is the invalid "0x":
        {"0x",   hexadecimal,           0,      EINVAL},
        {"0x 1", hexadecimal,           0,      EINVAL},
        {"0x+1", sixteen,               0,      EINVAL},
        {"0x-1", sixteen,               0,      EINVAL},
        {"0Xx",  sixteen,               0,      EINVAL},
        {"0xX",  sixteen,               0,      EINVAL},
        //should be a valid zero in a non-16 base, save bases>=34:
        {"0xX",  seq_x(0,33, sixteen),  0,      0},
        {"0Xx",  seq_x(0,33, sixteen),  0,      0},
        {"0x+1", seq_x(0,33, sixteen),  0,      0},
        {"0x-1", seq_x(0,33, sixteen),  0,      0}
    };
    const struct strto_tests dt[] = {
        //nptr   (bases) result    error
        {"0x.1",  NULL,  0.062500, 0},
        {".10x1", NULL,    0.1,    0},
        //a 'non-empty sequence' missing before E/P:
        {"e1",    NULL,    0,      EINVAL},
        {"E1",    NULL,    0,      EINVAL},
        {"p1",    NULL,    0,      EINVAL},
        {"P1",    NULL,    0,      EINVAL},
        //missing exponent (+/- optional, the rest isn't):
        {"1e",    NULL,    0,      EINVAL},
        {"1E",    NULL,    0,      EINVAL},
        {"0x1p",  NULL,    0,      EINVAL},
        {"0x1P",  NULL,    0,      EINVAL},
        
        {"1e+",   NULL,    0,      EINVAL},
        {"1E+",   NULL,    0,      EINVAL},
        {"0x1p+", NULL,    0,      EINVAL},
        {"0x1P+", NULL,    0,      EINVAL},
        {"1e-",   NULL,    0,      EINVAL},
        {"1E-",   NULL,    0,      EINVAL},
        {"0x1p-", NULL,    0,      EINVAL},
        {"0x1P-", NULL,    0,      EINVAL},
        //'optional' does not make invalid, also x^0=1 o.O
        {"2e+0",  NULL,    1,      0},
        {"2E+0",  NULL,    1,      0},
        {"2e-0",  NULL,    1,      0},
        {"2E-0",  NULL,    1,      0}
    };
    for (i=0; i<sizeof(t)/sizeof(struct strto_tests); ++i) { //nr of nptrs
        for (j=0; j<9; ++j) {                                //nr of functions
            if (j<6) {
                for (ip=t[i].bases; *ip!=-1; ++ip) {         //number of bases
                    if (test_function(j,t[i],*ip))
                        break;
                }
            } else
                test_function(j,t[i],0);
        }
    }
    for (i=0; i<sizeof(dt)/sizeof(struct strto_tests); ++i) {
        for (j=6; j<9; ++j)
            test_function(j,dt[i],0);
    }
    return 0;
}

/**
 ** Preforms the actual tests, takes:
 ** \param function_nr a 'number' of a function to be called (see the switch)
 ** \param t a structure containing arguments to be passed, return values etc
 ** \param base - this function preforms the test for a single base
 ** \returns a non-zero on failure
 **/
static int test_function (int function_nr, struct strto_tests t, int base)
{
    //some strings for the output:
    static const char
        *f_names[] = {
            "strtoumax", "strtoimax",
            "strtol", "strtoll",
            "strtoul", "strtoull",
            "strtof", "strtod", "strtold"
        };
    //remembering return values
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
        enum {UT, IT, L, LL, UL, ULL, F, D, LD} type;
    } r;
    int error, wrong;
    
    char *endptr, returned[80];
    errno = error = wrong = 0;
    switch (function_nr)
    {
        case 0:
            r.val.ut = strtoumax(t.nptr, &endptr, base);
            error = errno;
            r.type = UT;
            sprintf(returned, "%ju", r.val.ut);
            if (r.val.ut != (uintmax_t)t.result)
                wrong=1;
        break;
        case 1:
            r.val.it = strtoimax(t.nptr, &endptr, base);
            error = errno;
            r.type = IT;
            sprintf(returned, "%jd", r.val.it);
            if (r.val.it != (intmax_t)t.result)
                wrong=1;
        break;
        case 2:
            r.val.l = strtol(t.nptr, &endptr, base);
            error = errno;
            r.type = L;
            sprintf(returned, "%ld", r.val.l);
            if (r.val.l != (long)t.result)
                wrong=1;
        break;
        case 3:
            r.val.ll = strtoll(t.nptr, &endptr, base);
            error = errno;
            r.type = LL;
            sprintf(returned, "%lld", r.val.ll);
            if (r.val.ll != (long long)t.result)
                wrong=1;
        break;
        case 4:
            r.val.ul = strtoul(t.nptr, &endptr, base);
            error = errno;
            r.type = UL;
            sprintf(returned, "%lu", r.val.ul);
            if (r.val.ul != (unsigned long)t.result)
                wrong=1;
        break;
        case 5:
            r.val.ull = strtoull(t.nptr, &endptr, base);
            error = errno;
            r.type = ULL;
            sprintf(returned, "%llu", r.val.ull);
            if (r.val.ull != (unsigned long long)t.result)
                wrong=1;
        break;
        case 6:
            r.val.f = strtof(t.nptr, &endptr);
            error = errno;
            r.type = F;
            sprintf(returned, "%f", r.val.f);
            if (r.val.f != (float)t.result)
                wrong=1;
        break;
        case 7:
            r.val.d = strtod(t.nptr, &endptr);
            error = errno;
            r.type = D;
            sprintf(returned, "%f", r.val.d);
            if (r.val.d != (double)t.result)
                wrong=1;
        break;
        case 8:
            r.val.ld = strtold(t.nptr, &endptr);
            error = errno;
            r.type = LD;
            sprintf(returned, "%Lf", r.val.ld);
            if (r.val.ld != (long double)t.result)
                wrong=1;
        break;
        default:
            fprintf(stdout, "???%i???\n", function_nr);//---
            return 0;
        break;
    }
    if(error != t.error)
        wrong = 1;
    
    if (wrong) {
      if (function_nr<6)
        fprintf(stderr, "%s(\"%s\", NULL, %i) should return %Lf, errno=%s\n",
        f_names[function_nr], t.nptr, base, t.result, e_name(t.error));
      else
          fprintf(stderr, "%s(\"%s\", NULL) should return %Lf, errno=%s\n",
          f_names[function_nr], t.nptr, t.result, e_name(t.error));
      fprintf(stderr, "\tinstead, it returned %s, errno=%s\n",
      returned, e_name(error));
    }
    return wrong;
}
/**
 ** Returns a string with a human-readable error name
 ** \param error - errno to be "stringized"
 **/
static char* e_name(int error)
{
    static const char *e_names[] = {
        "0", "EINVAL",
    };
    char *s;
    int i=-1;
    
    if (error==0)
        i = 0;
    else if (error==EINVAL)
        i = 1;
    else
        s = strerror(error);
    
    if(i!=-1) {
        s = malloc(strlen(e_names[i]));
        strcpy(s, e_names[i]);
    }
    return s;
}

/**
 **  A "sequence" is a sorted tuple of integers excluding 1, plus a final -1
 **  \param from the lower bound
 **  \param to the upper bound
 **  \param exclude another sequence of integers to excluded from the new one
 **  \returns pointer to a "sequence" of integers from {[from,to]/exclude}/1
 **/
static int* seq_x(int from, int to, int *exclude)
{
    int i, j=0, *sequence;
    for (i=0; exclude[i] != -1; ++i); //length of exclude
    sequence = malloc(sizeof(int) * (to-from+1 - i));
    
    for (i=from; i<=to; ++i) {
        if (i != *exclude && i != 1)
            sequence[j++] = i;
        else
            ++exclude;
    }
    sequence[j] = -1;
    
    return sequence;
}
//A handy auxiliary function, passes seq_x an empty exclude sequence
static int* seq(int from, int to)
{
    return seq_x(from, to, (int[]){-1});
}
