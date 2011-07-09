#include <stdlib.h>
/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */
//file depends: malloc

///a type for non-null terminated "Strings"
typedef struct {
    size_t size; ///< excludes the trailing '\0'
    char *s;
} String;

static String *real_addslashes(const String *S,String *newS,unsigned int index);

/**
 ** creates a printable "String" from the old one by copying all characters
 ** except for the special ones, which are replaced with printable equivalents.
 ** \param S - a string with special characters
 ** \returns a "String" with backslash-escaped characters
 **/
String addslashes(String S)
{
    String newS = {.size=0};
    real_addslashes(&S, &newS, 0);
    return newS;
}
///does the actual work for addslashes (not intended to be used directly)
static String *real_addslashes(const String *S,String *newS,unsigned int index)
{
    static const struct {
        char chr;
        char str[3];
    } spec[] = {
        {'\0', "\\0"},
        {'\n', "\\n"},
        {'\"', "\\\""},
        {'\\', "\\\\"},
        {'\t', "\\t"},
        {'\r', "\\r"},
        {'\b', "\\b"},
        {'\f', "\\f"},
        {'\a', "\\a"},
        {'\v', "\\v"},
        {'\?', "\\?"},
        //{'\'', "\\'"}
    };
    static const unsigned int nr_spec_chars = sizeof(spec) / sizeof(spec[0]);
    unsigned int cur, j;
    
    if (index < S->size) {
        cur = newS->size;
        for (j=0;  j < nr_spec_chars  &&  S->s[index] != spec[j].chr;  ++j);
        if (j < nr_spec_chars)
            ++newS->size;
        
        ++newS->size;
        real_addslashes(S, newS, index+1);
        
        if (j < nr_spec_chars) {
            newS->s[cur]   = spec[j].str[0];
            newS->s[cur+1] = spec[j].str[1];
        }
        else
            newS->s[cur] = S->s[index];
    } else {
        newS->s = malloc(newS->size+1);
        newS->s[newS->size+1] = '\0';
    }
    return newS;
}
