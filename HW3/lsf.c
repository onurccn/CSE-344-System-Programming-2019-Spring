#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

int main () {
    char * cwd = get_current_dir_name();

    if (cwd) {
        struct dirent * dir;
        struct stat fileStat;
        DIR * d = opendir(cwd);
        if (d) {
            while((dir = readdir(d)) != NULL) {
                if (strlen(dir->d_name) > 0 && dir->d_name[0] == '.') continue;
                char * currentPath = (char *) malloc(strlen(cwd) + strlen(dir->d_name) + 2);
                strcpy(currentPath, cwd);
                if (cwd[strlen(cwd) - 1] != '/'){
                    const char seperator[2] = "/";
                    strcat(currentPath, seperator); /* Also add a slash if it doesnt have one in the end */
                }
                strcat(currentPath, dir->d_name);
                if (dir->d_type == DT_DIR) continue;
                if (lstat(currentPath, &fileStat) < 0){
                    printf("Couldn't opened file at %s.\n", currentPath);
                    free(currentPath);
                    closedir(d);
                    return -1;
                }
                printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
                printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
                printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
                printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
                printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
                printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
                printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
                printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
                printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
                printf("\t%ld bytes\t%c\t%s\n",fileStat.st_size, S_ISREG(fileStat.st_mode) ? 'R' : 'S', dir->d_name);
                
                free(currentPath);
            }
            closedir(d);
        }
        free(cwd);
    }

    return 0;
}