#ifndef XOPEN_SOURCE
    #define XOPEN_SOURCE 700
#endif
#include <stdlib.h>
#include <stdarg.h>

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */
 
/**
 ** \file depends: malloc, vsnprintf, va_start, va_end
 ** \author Luka Marčetić, 2011
 **/

/**
 ** A wrapper function for vsnprintf that automatically allocates space for a
 ** string that it passes to it.
 ** \params (same as those for printf)
 ** \returns a pointer to the newly constructed string
 **/
char* sreturnf(const char *format, ...)
{
    char *s;
    size_t s_size;
    va_list ap;
    
    va_start(ap, format);
    s_size = vsnprintf(NULL, 0, format, ap) +1;
    va_end(ap);
    s = malloc(sizeof(char) * s_size);
    va_start(ap, format);
    vsnprintf(s, s_size, format, ap);
    va_end(ap);
    
    return s;
}
