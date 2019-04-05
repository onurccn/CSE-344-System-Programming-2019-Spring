#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    char * path;
    int shouldFree = 0;
    
    if (argc > 1) {
        path = argv[1];
    }
    else {
        size_t size = 0;
        shouldFree = 1;
        getline(&path, &size, stdin);
    }

    FILE * fp = fopen(path, "r");
    if (fp == NULL) { printf("Couldn't opened file %s\n", path); if (shouldFree) free(path); return -1; }
    char * line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) > 0) {
        printf("%s", line);
    }
    
    // Cleanup
    fclose(fp);
    free(line);
    if (shouldFree) free(path);
    return 0;
}