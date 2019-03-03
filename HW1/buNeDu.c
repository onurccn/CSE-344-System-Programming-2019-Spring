#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


int depthFirstApply(char * path, int pathfun (char * path1));

int sizePathFun(char * path);

bool detailFlag = false;

int main(int argc, char *argv[]){
    if (argc < 2 || (argc < 3 && strcmp(argv[1], "-z") == 0)) {
        fprintf(stderr, "Try using './buNeDu [-z] path' format \n");
        return -1;
    }

    if (strcmp(argv[1], "-z") == 0){
        detailFlag = true;
        depthFirstApply(argv[2], sizePathFun);
    }
    else if (argc > 2 && strcmp(argv[2], "-z") == 0) {
        detailFlag = true;
    }

    depthFirstApply(argv[1], sizePathFun);

    return 0;
}

int depthFirstApply(char * path, int pathfun(char * path1)){
    DIR * d;
    struct dirent *dir;
    long currentPathSize, totalPathSize = 0, subDirSize;
    char * currentPath;
    const char seperator[2] = "/";
    d = opendir(path);
    if (d) {
        dir = readdir(d);
        dir = readdir(d);
        while((dir = readdir(d)) != NULL) {
            currentPath = (char *) malloc(strlen(path) + strlen(dir->d_name) + 2);
            strcpy(currentPath, path);
            if (path[strlen(path) - 1] != '/'){
                strcat(currentPath, seperator);
            }
            strcat(currentPath, dir->d_name);
            
            
            if (dir->d_type == DT_DIR) {
                subDirSize = depthFirstApply(currentPath, pathfun);
                if (detailFlag && subDirSize > 0) totalPathSize += subDirSize;
            }
            else {
                currentPathSize = pathfun(currentPath);
                if (currentPathSize > 0) {
                    totalPathSize += currentPathSize;
                }   
            }

            free(currentPath);

        }
        closedir(d);
    }
    else {
        printf("Couldn't open dir %s \n", path);
        
        return -1;
    }
    printf("%ld\t\t%s\n", totalPathSize, path);
    
    return totalPathSize;
}

int sizePathFun(char * path){
    struct stat fileStat;
    /* Error checking */
    if (stat(path, &fileStat) < 0) 
        return -2;

    /* Print detailed info if path is a regular file */
    if (S_ISREG(fileStat.st_mode)) {
        /*
        printf("Information for %s\n",path);
        printf("---------------------------\n");
        printf("File Size: %ld bytes \tBlock Size: %ld \tBlocks: %ld\n",fileStat.st_size, fileStat.st_blksize, fileStat.st_blocks);
        printf("Number of Links: \t%ld\n",fileStat.st_nlink);
        printf("File inode: \t\t%ld\n",fileStat.st_ino);
        printf("File mode: \t\t%d\n",fileStat.st_mode);
        printf("File Permissions: \t");
        printf( (S_ISDIR(fileStat.st_mode)) ? "d" : "-");
        printf( (fileStat.st_mode & S_IRUSR) ? "r" : "-");
        printf( (fileStat.st_mode & S_IWUSR) ? "w" : "-");
        printf( (fileStat.st_mode & S_IXUSR) ? "x" : "-");
        printf( (fileStat.st_mode & S_IRGRP) ? "r" : "-");
        printf( (fileStat.st_mode & S_IWGRP) ? "w" : "-");
        printf( (fileStat.st_mode & S_IXGRP) ? "x" : "-");
        printf( (fileStat.st_mode & S_IROTH) ? "r" : "-");
        printf( (fileStat.st_mode & S_IWOTH) ? "w" : "-");
        printf( (fileStat.st_mode & S_IXOTH) ? "x" : "-");
        printf("\n\n");
        printf("The file %s a symbolic link\n", (S_ISLNK(fileStat.st_mode)) ? "is" : "is not");
        */
        return fileStat.st_size;
    }
    printf("Special file %s\n", strrchr(path, '/') + 1);

    return -1;
}