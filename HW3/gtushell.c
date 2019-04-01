#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#define _GREEN "\x1B[32m"
#define _RST "\x1B[0m"

#define ARG_LIMIT 100
#define SLICE_DELIMETER " \t\n"

// Interpreter and executor loop
void shellLoop();

// Retrieves command line from terminal
char * getCommand();

// Organizes retrieved command line
char ** sliceCommand(char * command, int * argc);

// Evaluates and executes the given command
int evaluateAndExecuteCommand(char ** args, int argc);


// There will be no arguments for this one.
int main(){
    shellLoop();

    return 0;
}

void shellLoop() {
    int result = 1;

    while (result >= 0) {
        printf("%s$%s ", _GREEN, _RST);
        char * line = getCommand();
        int argc;
        char ** args = sliceCommand(line, &argc);

        result = evaluateAndExecuteCommand(args, argc);
        //printf("result = %d\n", result);
    }
}

char * getCommand(){
    char * line;
    size_t size = 0;
    getline(&line, &size, stdin);

    return line;
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
    *argc = pos;
    return args;
}

int evaluateAndExecuteCommand(char ** args, int argc) {
    pid_t pid;
    int status;
    char errorMessage[1024];
    int outFile;
    if (strcmp(args[0], "exit") == 0) return -1;

    switch ((pid = fork())) {
        case 0:     // Child
            
            outFile = open("out.txt", O_WRONLY);
            if (outFile == -1){
                sprintf(errorMessage, "ERROR ON OPEN FILE %s", "out.txt");
                perror(errorMessage);
                exit(EXIT_FAILURE);
            }

            //int save_out = dup(fileno(stdout));    // no need to save for the child
            if(dup2(outFile, STDOUT_FILENO) == -1){
                sprintf(errorMessage, "ERROR ON REDIRECTING OUTPUT TO %s", "out.txt");
                perror(errorMessage);
                exit(EXIT_FAILURE);
            }
            
            if (execv(args[0], args) == -1) {
                sprintf(errorMessage, "ERROR ON EXECUTION OF UTILITY %s", args[0]);
                perror(errorMessage);
            }
            fflush(stdout); 
            close(outFile);

            //dup2(save_out, fileno(stdout));       // no need to restore output for the child
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
    return status;
}