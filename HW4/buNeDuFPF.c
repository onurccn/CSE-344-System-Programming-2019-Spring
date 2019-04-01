#define _GNU_SOURCE
#define MAX_BUF 1024

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>



int depthFirstApply(char * path, int pathfun (char * path1));
int sizePathFun(char * path);
int tryPrintFileDetail(char * path, int pathfun(char * path1));
char * getCurrentPath(char * path, char * name);

bool detailFlag = false;
static char * detailOption = "-z";
static char * fifo = "/tmp/161044057sizes";
int mainProcessID;
int done = 0;
int processCountPipe[2];
int fifoWrite, fifoRead;

void signal_handler(int sig) {
    if (mainProcessID == getpid()){
        if (done) printf("\n==== PROCESSING FINISHED, WRITING INFO INTO TERMINAL, PLEASE WAIT!\n");
        else printf("\nABNORMAL INTERRUPTION - Program terminating, Results so far:\n");
    }
    else {
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){
    if (argc < 2 || (argc == 2 && strcmp(argv[1], "-z") == 0) || argc > 3) {
        fprintf(stderr, "Try using './buNeDu [-z] path' format \n");
        return -1;
    }
    
    int pathIndex = 1;
    if (strcmp(argv[1], detailOption) == 0) {
        detailFlag = true;
        pathIndex = 2;
    }
    else if(argc > 2 && strcmp(argv[2], detailOption) == 0)
        detailFlag = true;
    
    mainProcessID = getpid();
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);
    int fifoRes;
    if ((fifoRes= mkfifo(fifo, S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
        
        printf("Couldn't make FIFO %d. %s\n", fifoRes, strerror(errno));
        unlink(fifo);
        exit(EXIT_FAILURE);
    }

    
    if (pipe(processCountPipe) == -1){
        printf("Couldn't create process counter pipe\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid;
    int wstatus;
    if ((pid = fork())){
        // Parent process
        close(processCountPipe[1]);
        fifoRead = open(fifo, O_RDONLY);
        if (fifoRead > 0) {
            char line[MAX_BUF];
            printf("PROCESS\tSIZE\tPATH\n");
            while(read(fifoRead, line, MAX_BUF) > 0){
                if(line[0] == '!'){
                    printf("==%d==", atoi(line + 1));
                    printf("\t-1");
                    int i = 1;
                    while (line[i] != '!' && line[i] != '\0') i++;    
                    printf("\tSpecial file %s", line + ++i);
                }
                else {
                    printf("==%d==", atoi(line));
                    int i = 0;
                    while (line[i] != '=' && line[i] != '\0') i++;    
                    printf("\t%ld", atol(line + ++i));
                    while (line[i] != '=' && line[i] != '\0') i++;
                    printf("\t%s", line + ++i);
                }
            }  
            close(fifoRead);

            int childCount = 0;
            int readCount;
            char buf[MAX_BUF]; // junk buffer
            while((readCount = read(processCountPipe[0], buf, MAX_BUF)) > 0) childCount += readCount;
            printf("%d child processes created. Main process is %d.\n", childCount, mainProcessID);
            unlink(fifo);
            return 0;
        }
        return 0;
    }
    else if (pid < 0){
        printf("Couldn't create first child process\n");
        unlink(fifo);
    }
    else {
        // Child process
        fifoWrite = open(fifo, O_WRONLY);
        depthFirstApply(argv[pathIndex], sizePathFun);
        close(fifoWrite);
        exit(EXIT_SUCCESS);
    }
}

int depthFirstApply(char * path, int pathfun(char * path1)){
    close(processCountPipe[0]);         // Close read end of process counter pipe
    write(processCountPipe[1], "1", 1);         // Write onyly 1 byte for each process, read byte count afterwards
    DIR * d;
    struct dirent *dir;
    long currentPathSize, totalPathSize = 0;
    char * currentPath;
    int wstatus = 0;
    d = opendir(path);
    if (d) {
        while((dir = readdir(d)) != NULL) {
            /* Append file name to the directory */
            if (strlen(dir->d_name) > 0 && dir->d_name[0] == '.') continue;
            
            // Retrieve file path
            currentPath = getCurrentPath(path, dir->d_name);
            //printf("%s\n", currentPath);
            /* If its a directory enter into recursively. This has priority over pathfun */
            if (dir->d_type == DT_DIR) {
                pid_t pid;
                int pipeArr[2];
                if (detailFlag && pipe(pipeArr) == -1){
                    printf("Error in pipe on %s", currentPath);
                }
                
                if ((pid = fork()) == 0) {
                    // Child process call directory apply function with this process
                    closedir(d);
                    if (detailFlag) close(pipeArr[0]);      // Close read end
                    char subDir[strlen(currentPath) + 1];
                    strcpy(subDir, currentPath);
                    free(currentPath);
                    long subSize = depthFirstApply(subDir, pathfun);
                    if (detailFlag) {
                        char subSizeStr[10];
                        memset(subSizeStr, 0, 10);
                        sprintf(subSizeStr, "%ld", subSize);
                        write(pipeArr[1], subSizeStr, 10);
                        close(pipeArr[1]);
                    }
                    exit(0);
                }
                else if (pid < 0){
                    printf("Couldn't fork for directory: %s\n", currentPath);
                }
                else {
                    if (detailFlag) {
                        close(pipeArr[1]);      // Close write end
                        char forkedDirSize[sizeof(long) + 1];
                        read(pipeArr[0], forkedDirSize, sizeof(long) + 1);
                        close(pipeArr[0]);
                        long subDirSize = atol(forkedDirSize);
                        totalPathSize += subDirSize;
                    }
                }
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
        int fileError;
        if ((fileError = tryPrintFileDetail(path, pathfun)) < 0) return fileError;
        return -1;
    }
    
    close(processCountPipe[1]);         // Close process counter pipes write end after directory completed. we dont need it anymore
    
    if (fifoWrite > 0){
        char dirSize[MAX_BUF];
        memset(dirSize, 0, MAX_BUF);
        sprintf(dirSize, "%d=%ld=%s\n", getpid(), totalPathSize, path);
        while(wait(&wstatus) > 0);          // Wait before writing current entry if we are done processing current directory before child process in order to accomplish post order.
        write(fifoWrite, dirSize, MAX_BUF);
    }
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

    if (fifoWrite > 0){
        char directoryInfo[MAX_BUF];
        
        sprintf(directoryInfo, "!%d!%s\n", getpid(), strrchr(path, '/') + 1);
        write(fifoWrite, directoryInfo, MAX_BUF);
        
    }
    return -1;
}

int tryPrintFileDetail(char * path, int pathfun(char * path1)) {
    if (pathfun(path) >= 0){
        struct stat fileStat;
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
    else {
        printf("Couldn't open directory/file %s \n", path);
    }

    return 0;
}

char * getCurrentPath(char * path, char * name){
    char * currentPath = (char *) malloc(strlen(path) + strlen(name) + 2);
    strcpy(currentPath, path);
    if (path[strlen(path) - 1] != '/'){
        const char seperator[2] = "/";
        strcat(currentPath, seperator); /* Also add a slash if it doesnt have one in the end */
    }
    strcat(currentPath, name);
    return currentPath;
}