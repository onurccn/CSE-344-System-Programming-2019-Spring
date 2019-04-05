#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main () {
    char * cwd = get_current_dir_name();

    if (cwd) {
        printf("%s\n", cwd);
        free(cwd);
    }

    return 0;
}