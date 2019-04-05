#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

#define _GREEN "\x1B[32m"
#define _RED "\x1B[31m"
#define _RST "\x1B[0m"

#define ARG_LIMIT 100
#define ARG_BUFFER 1024
#define SLICE_DELIMETER " \t\n"

#define EXTERNAL_UTILITY_COUNT 5
#define INTERNAL_UTILITY_COUNT 3
char external_utilities[EXTERNAL_UTILITY_COUNT][10] = { "lsf", "pwd", "cat", "wc", "bunedu" };
char internal_utilities[INTERNAL_UTILITY_COUNT][10] = { "exit", "help", "cd"};

int saveIn = -1;
int saveOut = -1;

char ** commandHistory;
int currentIndex = 0;
// Interpreter and executor loop
void shellLoop();

// Retrieves command line from terminal
char * getCommand();

// Organizes retrieved command line
char ** sliceCommand(char * command, int * argc);

// Evaluates and executes the given command
int evaluateAndExecuteCommand(char ** args, int argc);

// Executes both args sliced by sliceIndex (pipe operator) but redirects first ones output to second ones input
void executePipedCommand(char ** args, int sliceIndex);


// HELPER FUNCTIONS
int isExternalUtility(char * arg);
int isInternalUtility(char * arg);
void changeInOut(char op, char* redir);
void restoreInOut(char op) ;
void printHelp();

void getFullPathExecutable(char ** utility);

char * command_line = NULL;     // Need to be global in order to free in signal handler.

char * cwd = NULL;  // shell directory

void signal_handler(int sig) {   
    if (command_line) free(command_line);
    printf("\t%sTry to use %sexit command%s next time.%s Goodbye!%s\n", _RED, _GREEN, _RED, _GREEN, _RST);
    for(size_t i = 0; i < currentIndex; i++)
    {
        free(commandHistory[i]);
    }
    free(commandHistory);
    exit(EXIT_FAILURE);
}

// There will be no arguments for this one.
int main(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    cwd = get_current_dir_name();
    char * l = malloc(5);
    memset(l, 0, 5);
    l[0] = 'M';
    getFullPathExecutable(&l);
    
    commandHistory = malloc(sizeof(char *) * 100);

    shellLoop();
    for(size_t i = 0; i < currentIndex; i++) free(commandHistory[i]);
    free(commandHistory);
    free(cwd);
    return 0;
}

void shellLoop() {
    int result = 1;

    while (result >= 0) {
        printf("%s$%s ", _GREEN, _RST);
        int argc;
        char * line = getCommand();

        if (line[0] == '!'){
            int nth = atoi(&line[1]);
            free(line);
            if (currentIndex == 0 || nth > currentIndex || nth < 1) continue;
            
            line = malloc(strlen(commandHistory[currentIndex - nth]) + 1);
            memcpy(line, commandHistory[currentIndex - nth], strlen(commandHistory[currentIndex - nth]) + 1);
        }
        
        commandHistory[currentIndex] = malloc(strlen(line) + 1);
        memcpy(commandHistory[currentIndex++], line, strlen(line) + 1);

        char ** args = sliceCommand(line, &argc);
        result = evaluateAndExecuteCommand(args, argc);
        
        free(line);
        free(args);
    }

    printf("Goodbye!\n");
}

char * getCommand(){
    size_t size = 0;
    getline(&command_line, &size, stdin);

    return command_line;
}

char ** sliceCommand(char * command, int * argc) {
    char ** args = malloc(sizeof(char *) * ARG_LIMIT);
    char * arg;
    int pos = 0;

    arg = strtok(command, SLICE_DELIMETER);
    while (arg && pos < ARG_LIMIT) {
        args[pos++] = arg;
        arg = strtok(NULL, SLICE_DELIMETER);
    }
    args[pos == 0 ? ++pos : pos] = NULL;
    *argc = pos;
    return args;
}

int evaluateAndExecuteCommand(char ** args, int argc) {
    pid_t pid;
    int status; 
    if (strcmp(args[0], internal_utilities[0]) == 0) return -1;

    int operatorIndex = -1;
    for(size_t i = 0; i < argc; i++)
    {
        if (strcmp(args[i], "|") == 0 || strcmp(args[i], ">") == 0 || strcmp(args[i], "<") == 0) {
            operatorIndex = i;
            break;
        }
    }

    if (isInternalUtility(args[0])) {
        if (operatorIndex > 0) {
            changeInOut(args[operatorIndex][0], args[operatorIndex + 1]);
        }
        if (strcmp(args[0], internal_utilities[1]) == 0) {
            printHelp();
        }
        else if (strcmp(args[0], internal_utilities[2]) == 0) {     // execute cd
            if (operatorIndex != -1 && operatorIndex == 2) {
                size_t size = 0;
                char * arg = NULL;
                getline(&arg, &size, stdin);

                if (chdir(arg) == -1) {
                    restoreInOut(args[operatorIndex][0]);
                    printf("Couldn't change current directory!\n");
                    free(arg);
                    return -1;
                }
                free(arg);
            }
            else {
                if (chdir(args[1]) == -1){
                    printf("Couldn't change current directory!\n");
                    return -1;
                }
            }
        }

        if (operatorIndex > 0) {
            restoreInOut(args[operatorIndex][0]);
        }
        return 0;
    }

    if (operatorIndex > 0) {
        char op;
        switch ((op = args[operatorIndex][0])){
            case '|':
                if (isExternalUtility(args[0]) == 0 || isExternalUtility(args[operatorIndex + 1]) == 0) {
                    printf("%sNOT ALLOWED PIPE USAGE!%s\n", _RED, _RST); 
                    return 1;
                }
                
                executePipedCommand(args, operatorIndex);
                return 0;                
            case '<':
            case '>':
                if ((pid = fork())) {
                    int status;
                    do {
                        waitpid(pid, &status, WUNTRACED);
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
                    return 0;
                }
                else if (pid == 0) {
                    changeInOut(op, args[operatorIndex + 1]);
                    
                    args[operatorIndex] = NULL;
                    char * comm = malloc((strlen(args[0]) + 1) * sizeof(char));
                    memcpy(comm, args[0], (strlen(args[0]) + 1) * sizeof(char));
                    getFullPathExecutable(&comm);
                    execv(comm, args);
                    free(comm);
                    exit(EXIT_FAILURE);
                }
                return -1;
        }
    }

    if (isExternalUtility(args[0])) {
        char * comm;
        switch ((pid = fork())) {
            case 0:     // Child
                comm = malloc((strlen(args[0]) + 1) * sizeof(char));
                memcpy(comm, args[0], (strlen(args[0]) + 1) * sizeof(char));
                getFullPathExecutable(&comm);
                if (execv(comm, args) == -1) {
                    printf("ERROR ON EXECUTION OF UTILITY %s\n", args[0]);
                }
                free(comm);
                exit(EXIT_FAILURE);
                break;
            case -1:
                perror("COULDN'T FORK FOR UTILITY EXECUTION");
                break;
            default:
                do {
                    waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }

    return 0;
}

void executePipedCommand(char ** args, int sliceIndex){
    int pipeline[2], pid[2];
    pipe(pipeline);
    
    if ((pid[0] = fork())) {
        close(pipeline[1]); // We won't write into pipe in this process
        // Parent
        if ((pid[1] = fork())) {
            // Block main process until both child finished

            int status;
            do {
                waitpid(pid[0], &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            do {
                waitpid(pid[1], &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
        else if (pid[1] == 0) {
            // Child 2
            if(dup2(pipeline[0], STDIN_FILENO) == -1){
                perror("ERROR ON REDIRECTING INPUT TO pipe");
                exit(EXIT_FAILURE);
            }
            close(pipeline[0]);
            
            char * comm = malloc((strlen(args[sliceIndex + 1]) + 1) * sizeof(char));
            memcpy(comm, args[0], (strlen(args[sliceIndex + 1]) + 1) * sizeof(char));
            getFullPathExecutable(&comm);
            if (execv(comm, &args[sliceIndex + 1]) == -1) {
                free(comm);
                char errorMessage[100];
                sprintf(errorMessage, "ERROR ON EXECUTION OF UTILITY %s", args[sliceIndex + 1]);
                perror(errorMessage);
            }

            exit(EXIT_FAILURE); // Exec should've exit the process peacefully
        }
        else {
            perror("ERROR ON PIPED COMMAND SECOND FORK, TERMINATING...");
            exit(EXIT_FAILURE);
        }
    }
    else if (pid[0] == 0) {
        // Child 1
        close(pipeline[0]); // We won't read into pipe in this process

        if(dup2(pipeline[1], STDOUT_FILENO) == -1){
            perror("ERROR ON REDIRECTING OUTPUT TO pipe");
            exit(EXIT_FAILURE);
        }
        close(pipeline[1]);

        args[sliceIndex] = NULL;

        char * comm = malloc((strlen(args[0]) + 1) * sizeof(char));
        memcpy(comm, args[0], (strlen(args[0]) + 1) * sizeof(char));
        getFullPathExecutable(&comm);
        if (execv(comm, args) == -1) {
            free(comm);
            char errorMessage[100];
            sprintf(errorMessage, "ERROR ON EXECUTION OF UTILITY %s", args[0]);
            perror(errorMessage);
        }

        fflush(stdout);
        exit(EXIT_FAILURE); // Exec should've exit the process peacefully
    }
    else {
        perror("ERROR ON PIPED COMMAND FIRST FORK, TERMINATING...");
        exit(EXIT_FAILURE);
    }
}

int isExternalUtility(char * arg) {
    for(size_t i = 0; i < EXTERNAL_UTILITY_COUNT; i++)
    {
        if (strcmp(arg, external_utilities[i]) == 0) return 1;
    }

    return 0;
}

int isInternalUtility(char * arg) {
    for(size_t i = 0; i < INTERNAL_UTILITY_COUNT; i++)
    {
        if (strcmp(arg, internal_utilities[i]) == 0) return 1;
    }

    return 0;
}

void changeInOut(char op, char * redir){
    int fd;
    if (op == '<'){
        saveIn = dup(STDIN_FILENO);
        if ((fd = open(redir, O_RDONLY)) == -1) { printf("%sCouldn't opened file %s!%s\n", _RED, redir, _RST); exit(EXIT_FAILURE); }
        if (dup2(fd, STDIN_FILENO) == -1) { printf("%sCouldn't redirect file %s!%s\n", _RED, redir, _RST); exit(EXIT_FAILURE); }
    }
    else {
        fflush(stdout);
        saveOut = dup(STDOUT_FILENO);
        if ((fd = open(redir, O_WRONLY | O_CREAT | O_TRUNC)) == -1) { printf("%sCouldn't opened file %s!%s\n", _RED, redir, _RST); exit(EXIT_FAILURE); }
        if (dup2(fd, STDOUT_FILENO) == -1) { printf("%sCouldn't redirect file %s!%s\n", _RED, redir, _RST); exit(EXIT_FAILURE); }

    }
    close(fd);
}

void restoreInOut(char op) {
    if (op == '<') {
        dup2(saveIn, STDIN_FILENO);
        close(saveIn);
        saveIn = -1;
    }
    else {
        dup2(saveOut, STDOUT_FILENO);
        close(saveOut);
        saveOut = -1;
    }
}

void printHelp() {
    printf("lsf - List current working directory contents with their access rights and file type\n");
    printf("pwd - Prints current working directory\n");
    printf("cd path - Changes current working directory to given path (relative or full path)\n");
    printf("help - Prints the list of supported commands (this command)\n");
    printf("cat [path] - Prints file contents at given path or retrieved path from stdin to stdout\n");
    printf("wc [path] - Prints line count at given file path or retrieved path from stdin to stdout\n");
    printf("bunedu [-z] [path] - Prints estimate file space usage\n");
    printf("exit - Exits terminal\n");
}

void getFullPathExecutable(char ** utility) {
    char * newPath = malloc(strlen(cwd) + strlen(*utility) + 2);
    strcpy(newPath, cwd);
    newPath[strlen(cwd)] = '/';
    strcpy(&newPath[strlen(cwd) + 1], *utility);
    free(*utility);
    *utility = newPath;
}