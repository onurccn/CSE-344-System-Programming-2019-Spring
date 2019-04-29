#define _POSIX_C_SOURCE 199309L

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

#define BANK_SIG SIGUSR1
#define SERVICE_SIG SIGUSR2
#define BUFFER_SIZE 100
#define SERVICE_BUFFER_SIZE 100
#define SERVICE_COUNT 4

typedef struct bank_log {
    int clientId;
    int processId;
    int money;
    long msec;
} bank_log;

typedef struct request_node {
    struct request_node * next_request;
    int client;
} request_node;

void printUsage();
int initializeBank(int runTime);
void runBank(int runTime);
int bankService(int pipe);
int getChildIndex(int pid);
timer_t setBankTimeout(int timeout);
timer_t setServiceTimeout();

int logsBuffer = 100;
int logIndex = -1;
bank_log * logs;
request_node * head = NULL;

int mainProcessId;

char *clientFifo = "/tmp/161044057_fifo";
int currentClientId = -1;
int _runTime;

int globalChildWritePipe[2];
int serviceProcesses[SERVICE_COUNT];
int servicePipes[SERVICE_COUNT][2];
char serviceBuffer[BUFFER_SIZE];
int serviceFlag = 1;

timer_t currentServiceTimerId;
timer_t bankTimerId;

struct timespec serviceStart;

void signal_handler(int sig) {
    if (getpid() == mainProcessId) {
        free(logs);
        struct request_node * temp = head;
        while(head != NULL){
            temp = head;
            head = head->next_request;
            free(temp);
        }
        if (bankTimerId)
            timer_delete(bankTimerId);

        printf("KAPANDI-SIGNAL\n");
        unlink(clientFifo);
        
    }
    else {
        if(currentServiceTimerId) {
            timer_delete(currentServiceTimerId);
        }
        if (currentClientId > 0) {
            sprintf(serviceBuffer, "%s_%d", clientFifo, currentClientId);
            int clientFd = open(serviceBuffer, O_WRONLY);
            if (clientFd > 0) {
                char noMoney[BUFFER_SIZE] = "!";
                write(clientFd, noMoney, BUFFER_SIZE);
            }
        }
    }
    exit(EXIT_FAILURE);
}

void pipe_handler(int sig) {
    if (getpid() == mainProcessId) {
        free(logs);
        struct request_node * temp = head;
        while(head != NULL){
            temp = head;
            head = head->next_request;
            free(temp);
        }
        if (bankTimerId)
            timer_delete(bankTimerId);
        
    }
    else {
        if(currentServiceTimerId) {
            timer_delete(currentServiceTimerId);
        }
        if (currentClientId > 0) {
            sprintf(serviceBuffer, "%s_%d", clientFifo, currentClientId);
            int clientFd = open(serviceBuffer, O_WRONLY);
            if (clientFd > 0) {
                char noMoney[BUFFER_SIZE] = "!";
                write(clientFd, noMoney, BUFFER_SIZE);
            }
        }
    }

    printf("KAPANDI-PIPE\n");
    unlink(clientFifo);
    exit(EXIT_FAILURE);
}

static void bankTimeoutHandler(int sig, siginfo_t *si, void *uc)
{
    for(size_t i = 0; i < SERVICE_COUNT; i++)
    {
        kill(serviceProcesses[i], SIGINT);
    }
    timer_delete(bankTimerId);
    bankTimerId = NULL;
    struct request_node * temp = head;
    while(head != NULL){
        temp = head;
        head = head->next_request;
        free(temp);
    }

    int fd = open("bank_log.txt", O_WRONLY | O_CREAT, S_IRWXU | S_IRWXO | S_IRWXG);
    char text[500];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);


    strftime(text, sizeof(text)-1, "%d %m %Y", t);
    sprintf(text, "%s tarihinde islem başladı. Bankamiz %d saniye hizmet verecek.\nclientPid\tprocessNo\tPara\tislem bitis zamani\n", text, _runTime);
    write(fd, text, strlen(text));
    for(size_t i = 0; i <= logIndex && logIndex != -1; i++)
    {
        sprintf(text, "%d\t%d\t%d\t%ld msec\n", logs[i].clientId, logs[i].processId, logs[i].money, logs[i].msec);
        write(fd, text, strlen(text));
    }
    int serveCount[SERVICE_COUNT] = {0, 0, 0, 0};
    for(size_t i = 0; i < SERVICE_COUNT && logIndex != -1; i++)
    {
        int j = 0;
        while(j <= logIndex) {
            if (serviceProcesses[i] == logs[j].processId)
                serveCount[i]++;
            j++;
        }
    }

    sprintf(text, "...\n%d saniye dolmustur. %d musteriye hizmet verdik\np1 %d\np2 %d\np3 %d\np4 %d musteriye hizmet sundu.\n", _runTime, logIndex + 1, serveCount[0], serveCount[1], serveCount[2], serveCount[3]);
    write(fd, text, strlen(text));
    printf("KAPANDI\n");
    unlink(clientFifo);
    free(logs); // Also print them here
    exit(EXIT_SUCCESS);
}

static void serviceTimeoutHandler(int sig, siginfo_t *si, void *uc)
{   
    timer_delete(currentServiceTimerId);
    currentServiceTimerId = NULL;

    int money = rand() % 100;
    struct timespec serviceEnd;
    clock_gettime(CLOCK_MONOTONIC_RAW, &serviceEnd);
    sprintf(serviceBuffer, "%d %d %d %ld", getpid(), currentClientId, money, (serviceEnd.tv_sec - serviceStart.tv_sec) * 1000000 + (serviceEnd.tv_nsec - serviceStart.tv_nsec) / 1000);
    write(globalChildWritePipe[1], serviceBuffer, BUFFER_SIZE);    // Notify parent process to log data
    currentClientId = -1;
    serviceFlag = 0; // Done serving. unblock service process to grab another client from parent process.
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printUsage();
        return -1;
    }
    
    mkfifo(clientFifo, S_IRWXU | S_IRWXG | S_IRWXO);
    clock_gettime(CLOCK_MONOTONIC_RAW, &serviceStart);
    struct sigaction sa,sa_pipe;
    memset(&sa, 0, sizeof(sa));
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa.sa_handler = &signal_handler;
    sa_pipe.sa_handler = &pipe_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa_pipe, NULL);
    mainProcessId = getpid();

    initializeBank(atoi(argv[1]));
}

void printUsage() {
    printf("Usage for bank: ./bank [ServiceTime]\n");
}

int initializeBank(int runTime) {
    _runTime = runTime;
    if (pipe(globalChildWritePipe) == -1) {
        printf("Error creating global pipe");
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < SERVICE_COUNT; i++) {
        if (pipe(servicePipes[i]) == -1) {
            printf("Error creating pipes");
            exit(EXIT_FAILURE);
        }
        if ((serviceProcesses[i] = fork())) {
            close(servicePipes[i][0]); // Close read end in parent
            continue;
        }
        else if (serviceProcesses[i] == 0){
            close(servicePipes[i][1]); // Close write end in child
            close(globalChildWritePipe[0]);
            bankService(servicePipes[i][0]);  // Call bank service on child process
            exit(EXIT_SUCCESS);
        }
        else {
            printf("Error on creating child process. Exiting bank.\n");
            exit(EXIT_FAILURE);
        }
    }
    close(globalChildWritePipe[1]);
    // Set global child read non blocking
    int prevFlags = fcntl(globalChildWritePipe[0], F_GETFL, 0); 
    fcntl(globalChildWritePipe[0], F_SETFL, prevFlags | O_NONBLOCK);

    logs = calloc(sizeof(bank_log), logsBuffer);
    runBank(runTime);

    return 0;
}

int getChildIndex(int pid){
    for(size_t i = 0; i < SERVICE_COUNT; i++)
        if (serviceProcesses[i] == pid) 
            return i;
    return -1;
}

void runBank(int runTime){
    srand(time(NULL));
    bankTimerId = setBankTimeout(runTime);

    // Make fifo and open its descriptor in non blocking mode.
    int readClient = open(clientFifo, O_RDONLY);
    int prevFlags = fcntl(readClient, F_GETFL, 0); 
    fcntl(readClient, F_SETFL, prevFlags | O_NONBLOCK);
    
    char buffer[BUFFER_SIZE];
    int availableProcesses[SERVICE_COUNT];
    for(size_t i = 0; i < SERVICE_COUNT; i++)
    {
        availableProcesses[i] = 1;
    }
    struct request_node * last = head;
    while (1){
        // Client - Bank fifo read 
        if (read(readClient, buffer, BUFFER_SIZE) > 0) {
            int client = atoi(buffer);
            if (head == NULL) {
                head = malloc(sizeof(struct request_node));
                head->next_request = NULL;
                last = head;
                last->client = client;
            }
            else {
                last->next_request = malloc(sizeof(struct request_node));
                last = last->next_request;
                last->client = client;
            }
        }

        int i = 0;
        while(i < SERVICE_COUNT && availableProcesses[i] == 0) i++;

        // Send it to available child if there is
        if (i != SERVICE_COUNT){
            if (head != NULL) {
                int client = head->client;
                struct request_node * temp = head;                
                head = head->next_request;
                free(temp);

                int writePipe = servicePipes[i][1];
                sprintf(buffer, "%d", client);
                write(writePipe, buffer, BUFFER_SIZE);
                availableProcesses[i] = 0;
            }
        }

        // Service - Bank Communication. Get done work.
        if (read(globalChildWritePipe[0], buffer, BUFFER_SIZE) > 0) {
            int servicePid = atoi(buffer);
            int serviceIndex = getChildIndex(servicePid);
            availableProcesses[serviceIndex] = 1;
            
            // LOG THESE TOKENS HERE
            char ** tokens = malloc(sizeof(char *) * 4);
            char * token;
            int pos = 0;
            token = strtok(buffer, " ");
            while(token && pos < 4){
                tokens[pos++] = token;
                token = strtok(NULL, " ");
            }

            if (logIndex == logsBuffer - 1) {
                logsBuffer *= 2;
                logs = realloc(logs, sizeof(bank_log) * logsBuffer);
            }
            logs[++logIndex].processId = atoi(tokens[0]);
            logs[logIndex].clientId = atoi(tokens[1]);
            logs[logIndex].money = atoi(tokens[2]);
            logs[logIndex].msec = atol(tokens[3]) / 1000;

            char clientMessage[BUFFER_SIZE];
            memset(clientMessage, '\0', BUFFER_SIZE);
            sprintf(clientMessage, "%s", tokens[2]);
            char clientSpecificFifo[BUFFER_SIZE];
            sprintf(clientSpecificFifo, "%s_%s", clientFifo, tokens[1]);
            int clientFifoFd = open(clientSpecificFifo, O_WRONLY);
            write(clientFifoFd, clientMessage, BUFFER_SIZE);
            //printf("Sent %s %s\n", tokens[1], clientMessage);
            close(clientFifoFd);
          
            free(tokens);
        }

    }
    free(logs);
}

// Bank Child Process
int bankService(int pipe) {
    // Loops until finished by parent process
    while(1) {
        read(pipe, serviceBuffer, BUFFER_SIZE);    // Blocks service until new client assigned to this child
        // New client came through, process it after timeout
        currentClientId = atoi(serviceBuffer);
        
        currentServiceTimerId = setServiceTimeout();

        serviceFlag = 1;
        while(serviceFlag); // Wait here for service to finish after timeout 
    }

    return 0;
}

timer_t setServiceTimeout() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = serviceTimeoutHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SERVICE_SIG, &sa, NULL) == -1)
        exit(EXIT_FAILURE);

    // Create Timer here
    timer_t timerId;
    struct sigevent sigev;
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = SERVICE_SIG;
    sigev.sigev_value.sival_ptr = &timerId;
    if (timer_create(CLOCK_REALTIME, &sigev, &timerId) == -1)
        exit(EXIT_FAILURE);

    struct itimerspec timer;
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_nsec = 500000000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 0;
    if(timer_settime(timerId, 0, &timer, NULL) == -1)
        exit(EXIT_FAILURE);
    
    return timerId;
}

timer_t setBankTimeout(int timeout){
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = bankTimeoutHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(BANK_SIG, &sa, NULL) == -1)
        exit(EXIT_FAILURE);

    // Create Timer here
    timer_t timerId;
    struct sigevent sigev;
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = BANK_SIG;
    sigev.sigev_value.sival_ptr = &timerId;
    if (timer_create(CLOCK_REALTIME, &sigev, &timerId) == -1)
        exit(EXIT_FAILURE);

    struct itimerspec timer;
    timer.it_value.tv_sec = timeout;
    timer.it_value.tv_nsec = 0;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_nsec = 0;
    if(timer_settime(timerId, 0, &timer, NULL) == -1)
        exit(EXIT_FAILURE);

    return timerId;
}