#define _GNU_SOURCE

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

int depthFirstApply(char * path, int pathfun (char * path1));

int sizePathFun(char * path);

int tryPrintFileDetail(char * path, int pathfun(char * path1));

char * getCurrentPath(char * path, char * name);

bool detailFlag = false;
static char * detailOption = "-z";
static char * filename = "161044057sizes.txt";
static char * processCounterFile = "161044057p.tmp";
int mainProcessID;
int done = 0;
void signal_handler(int sig) {
    // Since homework pdf doesnt explain which signals to handle and what to do,
    // Kill all processes except main process here, then main process prints all the information after all child processes termination.
    
    if (mainProcessID == getpid()){
        if (done) printf("==== PROCESSING FINISHED, WRITING INFO INTO TERMINAL, PLEASE WAIT!\n");
        else printf("ABNORMAL INTERRUPTION - Program terminating, Results so far:\n");
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
    

    pid_t pid;
    int wstatus;
    if ((pid = fork())){
        // Parent process
        if(waitpid(pid, &wstatus, 0)){
            done = 1;
            // print file contents
            FILE * fp = fopen(filename, "r");
            if (fp != NULL) {
                char * line = NULL;
                size_t len = 0;
                ssize_t read;
                printf("PROCESS\tSIZE\tPATH\n");
                while((read = getline(&line, &len, fp)) != -1){
                    printf("==%d==", atoi(line));
                    int i = 0;
                    while (line[i] != '-' && line[i] != '\0') i++;    
                    printf("\t%ld", atol(line + ++i));
                    while (line[i] != '-' && line[i] != '\0') i++;
                    printf("\t%s", line + ++i);
                    
                    
                    // printf("%ld", atol(line + ++i));
                    // while (line[i] != '-' || line[i] != '\0') i++;
                    // printf("%s", line + ++i);
                }
                
                if (line) free(line);
                fclose(fp);
            }

            FILE * pp = fopen(processCounterFile, "r");
            int childCount = 0;
            if (pp != NULL){
                fscanf(pp, "%d", &childCount);
                fclose(pp);
            }
            remove(processCounterFile);
            printf("%d child processes created. Main process is %d.\n", childCount, mainProcessID);
            return 0;
        }
    }
    else if (pid < 0){
        printf("Couldn't create first child process\n");
    }
    else {
        // Child process
        depthFirstApply(argv[pathIndex], sizePathFun);

        exit(EXIT_SUCCESS);
    }
}

int depthFirstApply(char * path, int pathfun(char * path1)){
    FILE * pp = fopen(processCounterFile, "r+");
    if (pp != NULL){
        int child = 0;
        if (flock(fileno(pp), LOCK_EX) == 0){
            if (fscanf(pp, "%d", &child) > 0){
                fseek(pp, 0, SEEK_SET);
                child++;
                fprintf(pp, "%d", child);
            }
            else {
                fprintf(pp, "%d", 1);
            }
            fflush(pp);
            flock(fileno(pp), LOCK_UN);
        }
        fclose(pp);
    }
    else {
        pp = fopen(processCounterFile, "wb");
        if (pp){
            if (flock(fileno(pp), LOCK_EX) == 0){
                fprintf(pp, "%d", 1);
                fflush(pp);
                flock(fileno(pp), LOCK_UN);
            }
            fclose(pp);
        }
    }

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

            /* If its a directory enter into recursively. This has priority over pathfun */
            if (dir->d_type == DT_DIR) {
                pid_t pid;
                if ((pid = fork()) == 0) {
                    // Child process call directory apply function with this process
                    closedir(d);
                    char subDir[strlen(currentPath) + 1];
                    strcpy(subDir, currentPath);
                    free(currentPath);
                    depthFirstApply(subDir, pathfun);
                    exit(0);
                }
                else if (pid < 0){
                    printf("Couldn't fork for directory: %s\n", currentPath);
                }
                else {
                    // parent process after fork, wait for this child to finish if detail flag is set for retrieving child processes calculation from file's last entry
                    if (detailFlag) {
                        if (waitpid(pid, &wstatus, 0) > 0){
                            FILE * fp = fopen(filename, "r");
                            if (fp != NULL){
                                char * line = NULL, * lastLine = (char *) malloc(1);
                                size_t len = 0;
                                ssize_t read;
                                while((read = getline(&line, &len, fp)) != -1){
                                    free(lastLine);
                                    lastLine = (char *) malloc(len);
                                    strcpy(lastLine, line);
                                }
                                int i = 0;
                                while(lastLine[i] != '-' && lastLine[i] != '\0') i++;
                                long subDirSize = atol(lastLine + ++i);
                                totalPathSize += subDirSize;
                                free(lastLine);
                                fclose(fp);
                                if (line) free(line);
                            }
                        }
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

    while(wait(&wstatus) > 0); // Wait for all childs to finish, this provides post order for writing to file.
    // Print path and its size, write these to a file
    int fd = open(filename, O_CREAT|O_WRONLY|O_APPEND);
    if (fd > 0){
        if(flock(fd, LOCK_EX) == 0) {
            char* directoryInfo;
            int strSize;

            if ((strSize = asprintf(&directoryInfo, "%d-%ld-%s\n", getpid(), totalPathSize, path)) > 0)
                write(fd, directoryInfo, strSize);

            free(directoryInfo);
            flock(fd, LOCK_UN);
        }
        close(fd);
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
    int fd = open(filename, O_CREAT|O_WRONLY|O_APPEND);
    if (fd > 0){
        if(flock(fd, LOCK_EX) == 0) {
            char* directoryInfo;
            int strSize;

            if ((strSize = asprintf(&directoryInfo, "%d-0-Special file %s\n", getpid(), strrchr(path, '/') + 1)) > 0)
                write(fd, directoryInfo, strSize);

            free(directoryInfo);
            flock(fd, LOCK_UN);
        }
        close(fd);
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