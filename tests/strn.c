#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
 ** Tests correct behavior(return values, errno values...)
 ** depends: memcpy, memchr, memcmp, fprintf, malloc, free
 ** tests: strncat,
 **/

///main structure, for test data
struct tests {
    enum {
            fnr_strncat,
    } function_nr;
    String *S1, *S2;
    int n;
    struct {
            int i;
            char *s;
    } ret;
    int err;
};

static int test_function (struct tests t);

///defines test data and passes it to test_function
int main()
{
    unsigned int i, failed = 0;
    String
        aNa = {5, (char[]){"a\0aaa"}},
        b   = {2, (char[]){"bb"}};
    ///test data:
    struct tests test[] = {
        //function_nr  *S1     *S2  n   ret          err
        {fnr_strncat,  &aNa,   &b,  1,  {.s=aNa.s},  -1},
    };
    
    for (i=0; i<sizeof(test)/sizeof(test[0]); ++i)
        failed += (test_function(test[i]) != 0);
    
    return failed;
}

static int test_function(struct tests t)
{
    ///format strings for output, function_nr is used as the index:
    static const char *did[] = {
        " is correct", //---
        " returned %zu instead of %zu\n",
        " set errno = %s instead of %s\n",
        " wrote beyond '\\0'\n"
    };
    enum {NOT, RET, ERR, WRT} wrong = NOT;
    struct {
        int err;
        struct {
            int i;
            char *s;
        } val;
        enum {I, S} type;
    } r;
    char *s[2];
    String newS1, newS2;
    size_t index;
    
    r.err = 0;
    switch (t.function_nr) {
        case fnr_strncat:
            //copy the original:
            newS1.s = malloc(t.S1->size+1);
            memcpy(newS1.s, t.S1->s, t.S1->size+1);
            newS1.size = t.S1->size;
            //get results
            r.val.s = strncat(t.S1->s, t.S2->s, t.n);
            r.type = S;
            r.err = errno;
            
            if(r.val.s != t.S1->s)
                wrong = RET;
            else if (t.err!=-1 && r.err != t.err)
                wrong = ERR;
            else {
                //compare everything from \0 onward
                index = (char*)memchr(newS1.s, '\0', newS1.size+1) // '\0'
                        + t.n+1                                    // +cat
                        - newS1.s;                                 // -&[0]
                if (memcmp(&newS1.s[index],&(t.S1->s[index]),newS1.size-index))
                    wrong = WRT;
            }
            free(newS1.s);
            
            if (wrong) {
                newS1 = addslashes(newS1);
                newS2 = addslashes(*(t.S2));
                fprintf (
                    stderr,
                    "strncat(%s, %s, %i)",
                    newS1.s, newS2.s, t.n
                );
                free(newS1.s);
                free(newS2.s);
            }
        break;
    }
    
    switch (wrong) {
        case RET:
            fprintf(stderr, did[RET], r.val.s, t.ret.s);
        break;
        case ERR:
            s[0] = e_name(t.err);
            s[1] = e_name(r.err);
            fprintf(stderr, did[ERR], s[0], s[1]);
            free(s[0]);
            free(s[1]);
        break;
        case WRT:
            fprintf(stderr, did[WRT]);
        break;
        default:
        break;
    }
    
    return (wrong);
}
