#define _BSD_SOURCE   //scandir, alphasort
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <limits.h>   //PATH_MAX

/*
 * Copyright (c) 2011 Luka Marčetić<paxcoder@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 */
 
/// \file depends: scandir, alphasort, printf, getcwd, stat, access, fork, wait

/** goes through the tests/ directory executing all executables,
 ** makes a report based on their return values
 **/
int main(int argc, char *argv[])
{
    int i, n;
    char s[PATH_MAX + FILENAME_MAX], tests_path[]="tests/";
    struct dirent **items;
    struct stat status;
    int returned, all, failed;
    
    n = scandir(tests_path, &items, 0, alphasort);
    if (n < 1) {
        getcwd(s, 3);
        fprintf(stderr, "No tests found at location \"%s/tests/\"\n", s);
        return 1;
    }
    
    failed = all = 0;
    for (i=0; i<n; ++i) {
        *s='\0';
        strcat(s, tests_path);
        strcat(s, items[i]->d_name);
        
        stat(s, &status);
        if (S_ISREG(status.st_mode) && !access(s, X_OK)) {
            ++all;
            printf("Executing '%s' test collection...\n", items[i]->d_name);
            if (!fork() && execv(s, argv)==-1)
                    return -1;
            wait(&returned);
            
            printf("The '%s' test collection ", items[i]->d_name);
            if (!WIFEXITED(returned))
                printf("crashed!\n");
            else if (WEXITSTATUS(returned) == -1)
                printf("failed to start.\n");
            else if (WEXITSTATUS(returned) == 0)
                printf("passed.\n");
            else
                printf("failed %i test(s).\n", WEXITSTATUS(returned));
            
            if (!WIFEXITED(returned) || WEXITSTATUS(returned)!=0)
                ++failed;
        }
    }
    
    printf("\nTest collections passed: %d/%d\n", all-failed, all);
    return 0;
}
