#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
    FILE * fp;
    if (argc > 1) {
        fp = fopen(argv[1], "r");
    }
    else {
        fp = fdopen(STDIN_FILENO, "r");
    }

    if (fp == NULL) { printf("Couldn't opened input stream\n"); return -1; }
    char * line = NULL;
    size_t size = 0;
    int lineCount = 0;
    while (getline(&line, &size, fp) > 0) lineCount++;
    
    printf("%d\n", lineCount);
    // Cleanup
    fclose(fp);
    free(line);
    return 0;
}