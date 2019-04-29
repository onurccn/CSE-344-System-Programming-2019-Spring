#define _POSIX_C_SOURCE 199309L
//#define __USE_XOPEN_EXTENDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h> 
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 100

char *bankFifo = "/tmp/161044057_fifo";
char clientFifo[BUFFER_SIZE];
int writeBankFD, readBankFD;
timer_t intervalId;

void printUsage();
void clientProcess();
void printClientError();
void printClientMoneyReceive(int money);
void waitForBank();
void exitClient();

void signal_handler(int sig) {

    unlink(clientFifo);    
    exit(EXIT_FAILURE);
}

void pipe_error(int sig) {
    printClientError();
    unlink(clientFifo);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printUsage();
        return -1;
    }
    
    // Interrupt signal handling
    struct sigaction sa, sa_pipe;
    memset(&sa, 0, sizeof(sa));
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa.sa_handler = &signal_handler;
    sa_pipe.sa_handler = &pipe_error;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa_pipe, NULL);
    
    int clientCount = atoi(argv[1]);
    
    for(size_t i = 0; i < clientCount; i++)
    {
        int pId;
        if ((pId = fork())) {
            // Parent
            continue;
        }
        else if (pId == 0) {
            clientProcess();
            exit(EXIT_SUCCESS);
        }
        else {
            printf("Error on fork");
            exit(EXIT_FAILURE);
        }
    }
    int status;
    while(wait(&status) > 0);
}

void printUsage() {
    printf("Usage for client: ./client [ClientCount]\n");
}

void printClientError() {
    printf("Musteri %d parasini alamadi :(\n", getpid());
}

void clientProcess(){
    int currentPid = getpid();
    sprintf(clientFifo, "%s_%d", bankFifo, currentPid);
    int writeBank = open(bankFifo, O_WRONLY);
    if (writeBank == -1) {
        printf("-");
        printClientError();
        exit(EXIT_FAILURE);
    }
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    sprintf(buffer, "%d", currentPid);
    write(writeBank, buffer, BUFFER_SIZE);
    
    mkfifo(clientFifo, S_IRWXU | S_IRWXG | S_IRWXO);

    close(writeBank);

    waitForBank();

    printClientError();
    
}

void waitForBank(){
    char receiveMessageBuf[BUFFER_SIZE];
    while ((writeBankFD = open(bankFifo, O_WRONLY)) > 0) {
        readBankFD = open(clientFifo, O_RDONLY);
        if (readBankFD > 0) {
            int readCount = read(readBankFD, receiveMessageBuf, BUFFER_SIZE);
            if (readCount > 0) {
                if (receiveMessageBuf[0] == '!') {
                    printf("!");
                    printClientError();
                }
                else {
                    printf("Musteri %d %s lira aldi :)\n", getpid(), receiveMessageBuf);
                }
                unlink(clientFifo);
                close(writeBankFD);
                close(readBankFD);
                exit(EXIT_SUCCESS);
            }
            close(readBankFD);
        }
       close(writeBankFD);
    }
    
    printf("+");
    printClientError();
    unlink(clientFifo);
    exit(EXIT_FAILURE);
}
