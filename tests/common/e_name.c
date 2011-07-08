#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//\file depends: sprintf, strcpy, malloc, strerror
/**
 ** \returns a string with a human-readable error name
 ** \param error - errno to be "stringized"
 **/
char* e_name(int error)
{
    char *s = malloc(80*sizeof(char));
    
    if (error == -1)
        strcpy(s, "<unspecified>");
    else if (error == EINVAL)
        strcpy(s, "EINVAL");
    else if (error == ERANGE)
        strcpy(s, "ERANGE");
    else
        sprintf(s, "%i(%s)", error, strerror(error));
    return s;
}
