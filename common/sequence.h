#include <stdlib.h>

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */

/**
 **  \file depends: bsearch
 **  \author Luka Marčetić, 2011
 **  A "sequence" is a tuple of sorted integers. It is preceded by a number
 **  indicating length (ie the first value in the array = nr. of elements-1)
 **/

int* seq_x(int from, int to, int *exclude);
int* seq(int from, int to);

int seq_cmp(const void *a, const void *b);
int seq_has(int key, int *sequence);

/**
 **  Generates a sequence given bounds, and a sequence of integers to exclude
 **  \param from the lower bound
 **  \param to the upper bound
 **  \param exclude another sequence of integers to excluded from the new one
 **  \returns a pointer to the newly created sequence.
 **/
int* seq_x(int from, int to, int *exclude)
{
    int i, j=0, *sequence, *x = &exclude[1];
    
    i = (to-from+2);
    sequence = malloc(sizeof(int) * i);
    for (i=from; i<=to; ++i) {
        if (exclude[0] == 0  ||  i != *x)
            sequence[++j] = i;
        else
            ++x;
    }
    sequence[0] = j+1;
    
    return sequence;
}
///A handy auxiliary function, passes seq_x an empty exclude sequence
int* seq(int from, int to)
{
    static int none[] = {0, 0};
    return seq_x(from, to, none);
}

///Compares two integers given pointers to them (needed for bsearch in seq_has)
int seq_cmp(const void *a, const void *b)
{
    if (*(int*)a < *(int*)b)
        return -1;
    else if (*(int*)a == *(int*)b)
        return 0;
    return 1;
}
/**
 ** \returns 1 if 'key' was found in the sequence, otherwise 0
 ** \param key - an integer to search for
 ** \param sequence - a 'sequence' to be searched in
 **/
int seq_has(int key, int *sequence)
{
    if (bsearch(&key, &sequence[1], sequence[0], sizeof(int), seq_cmp) != NULL)
        return 1;
    return 0;
}
