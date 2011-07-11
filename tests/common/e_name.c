#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//\file depends: strcpy, strerror, malloc, [sreturnf]
/**
 ** \returns a string with a human-readable error name
 ** \param error - errno to be "stringized"
 **/
char* e_name(int error)
{
    char *s = malloc(7);
    
    if (error == -1)
        strcpy(s, "<any>");
    else if (error == EINVAL)
        strcpy(s, "EINVAL");
    else if (error == ERANGE)
        strcpy(s, "ERANGE");
    else if (error == ENOMEM)
        strcpy(s, "ENOMEM");
    else if (error == E2BIG)
        strcpy(s, "E2BIG");
    else {
        free(s);
        s = sreturnf("%i(%s)", error, strerror(error));
    }
    return s;
}
