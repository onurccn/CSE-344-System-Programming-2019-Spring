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
    char * path = NULL;
    if (argc > 1 && strcmp(argv[1], "-z") == 0){
        detailFlag = true;
        if (argc > 2) path = argv[2];
    }

    else if (argc > 2 && strcmp(argv[2], "-z") == 0) {
        detailFlag = true;
        path = argv[1];
    }
    
    if (!detailFlag && argc == 2) {
        path = argv[1];
    }
    
    if (path == NULL) {
        size_t size = 0;
        getline(&path, &size, stdin);
        if (path[strlen(path) - 1] == '\n') path[strlen(path) -1] = '\0';
    }
    printf("PATH TO WORK ON: %s\n", path);
    
    depthFirstApply(path, sizePathFun);

    return 0;
}

int depthFirstApply(char * path, int pathfun(char * path1)){
    DIR * d;
    struct dirent *dir;
    long currentPathSize, totalPathSize = 0, subDirSize;
    char * currentPath;
    struct stat fileStat;
    d = opendir(path);
    if (d) {
        while((dir = readdir(d)) != NULL) {
            /* Append file name to the directory */
            if (strlen(dir->d_name) > 0 && dir->d_name[0] == '.') continue;
            
            currentPath = (char *) malloc(strlen(path) + strlen(dir->d_name) + 2);
            strcpy(currentPath, path);
            if (path[strlen(path) - 1] != '/'){
                const char seperator[2] = "/";
                strcat(currentPath, seperator); /* Also add a slash if it doesnt have one in the end */
            }
            strcat(currentPath, dir->d_name);
            
            /* If its a directory enter into recursively. This has priority over pathfun */
            if (dir->d_type == DT_DIR) {
                subDirSize = depthFirstApply(currentPath, pathfun);
                if (detailFlag && subDirSize > 0) totalPathSize += subDirSize;
            }
            /* retrieve file info */
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
        /* Check if given path is a single file and if so open it and print its details. */
        if ((currentPathSize = pathfun(path)) >= 0){
            if (lstat(path, &fileStat) < 0){
                printf("Couldn't opened given file.\n");
                return -2;
            }
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
            printf("\n");
        }
        else if (currentPathSize != -1){
            printf("Couldn't open directory/file %s \n", path);
        }
        return -1;
    }
    printf("%ld\t%s\n", totalPathSize, path);
    
    return totalPathSize;
}

int sizePathFun(char * path){
    struct stat fileStat;
    /* Error checking */
    if (lstat(path, &fileStat) < 0) 
        return -2;

    /* Print detailed info if path is a regular file */
    if (S_ISREG(fileStat.st_mode)) {
        return fileStat.st_size;
    }
    printf("Special file %s\n", strrchr(path, '/') + 1);

    return -1;
}