#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>

typedef struct _entry {
    int source_fd;
    int target_fd;
    char * filename;
} Entry;

typedef struct _node {
    Entry entry;
    struct _node * next;
} Node;

Entry produce(char * source_path, char * target_path, char * filename);
void consume(Node * item);
void fill_buffer(char * source_path, char * target_path);


int producer_finished = 0;
int BUFFER_SIZE;
int NUM_OF_CONSUMERS;
int COPY_BUFFER_SIZE = 1024 * 1024;
pthread_t *consumer_thread;
pthread_mutex_t lock;
sem_t empty, full;
Node * head = NULL, * last;

void signal_handler(int sig) {
    pthread_exit(NULL);
}

void * producer(void *arg) {
    char ** directories = arg;
    char * source_dir = directories[0];
    char * target_dir = directories[1];
    fill_buffer(source_dir, target_dir);
    producer_finished = 1;

    //printf("Finished producing\n");
    pthread_exit(NULL);
}

void * consumer(void * arg) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    int count = 0;
    if (count == 0) {
        // Give producer a chance to enter its loop
        pthread_yield();
        sem_getvalue(&full, &count);
    }
    while (!producer_finished || count > 0) {
        //printf("Consumer waiting\n");
        sem_wait(&full);
        pthread_mutex_lock(&lock);
        //printf("Consumer gone\n");
        Node * current = head;
        if (head != last) {     // There were more than one item in buffer
            head = head->next;
        }
        else {
            head = NULL;
            last = NULL;
        }
        pthread_mutex_unlock(&lock);
        sem_post(&empty);
        printf("item = %s\n", current->entry.filename);
        consume(current);
        sem_getvalue(&full, &count);
    }
    // Finished consuming signal other consumer threads if theyre waiting on semaphore
    pthread_t tid = pthread_self();
    for(size_t i = 0; i < NUM_OF_CONSUMERS ; i++)
    {
        if (tid != consumer_thread[i]) {
            pthread_kill(consumer_thread[i], SIGINT);
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct timeval start, end;
    NUM_OF_CONSUMERS = 10;
    BUFFER_SIZE = 5;
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Mutex init failed\n");
        return 1;
    }

    if (sem_init(&empty, 0, BUFFER_SIZE)) {
        printf("Couldn't create semaphore\n");
        return 1;
    }

    if (sem_init(&full, 0, 0)) {
        printf("Couldn't create semaphore for full\n");
        return 1;
    }

    gettimeofday(&start, NULL);
    pthread_t producer_thread_id;
    if (pthread_create(&producer_thread_id, NULL, producer, (void *) &argv[1])) {
        printf("Couldn't create producer thread.\n");
        return 1;
    }
    consumer_thread = malloc(NUM_OF_CONSUMERS * sizeof(pthread_t));
    for(size_t i = 0; i < NUM_OF_CONSUMERS ; i++)
    {
        if (pthread_create(&consumer_thread[i], NULL, consumer, NULL)) {
            printf("Couldn't create consumer thread. - %ld\n", i);
            return 1;
        }
    }
    
    pthread_join(producer_thread_id, NULL);
    for(size_t i = 0; i < NUM_OF_CONSUMERS; i++)
    {
        pthread_join(consumer_thread[i], NULL);
    }
    free(consumer_thread);
    
    gettimeofday(&end, NULL);
    pthread_mutex_destroy(&lock);
    sem_destroy(&full);
    sem_destroy(&empty);
    return 0;
}

Entry produce(char * source_path, char * target_path, char * filename) {
    Entry entry;
    int size = fmax(strlen(source_path), strlen(target_path)) + strlen(filename) + 2;
    char str_buffer[size];

    memset(str_buffer, '\0', size);
    entry.filename = filename;

    sprintf(str_buffer, "%s/%s", source_path, filename);
    entry.source_fd = open(str_buffer, O_RDONLY);
    mkdir(target_path, S_IRWXG | S_IRWXO | S_IRWXU);
    sprintf(str_buffer, "%s/%s", target_path, filename);
    entry.target_fd = open(str_buffer, O_WRONLY | O_CREAT | O_TRUNC);

    return entry;
}

void fill_buffer(char * source_path, char * target_path) {   
    printf("Directory to copy from: %s\n", source_path);
    printf("Directory to copy to: %s\n", target_path);

    DIR * d;
    struct dirent *dir;
    d = opendir(source_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strlen(dir->d_name) > 0 && dir->d_name[0] == '.') continue;
            if (dir->d_type == DT_REG) {
                Entry entry = produce(source_path, target_path, dir->d_name);
                
                sem_wait(&empty);
                pthread_mutex_lock(&lock);

                if (head == NULL) {
                    head = malloc(sizeof(Node));
                    last = head;
                }
                else {
                    last->next = malloc(sizeof(Node));
                    last = last->next;
                }
                last->entry = entry;
                last->next = NULL;

                pthread_mutex_unlock(&lock);
                sem_post(&full);
            }
            else if (dir->d_type == DT_DIR) {
                int size = fmax(strlen(source_path), strlen(target_path)) + strlen(dir->d_name) + 1;
                char source_buffer[size], target_buffer[size];
                memset(source_buffer, 0, size);
                memset(target_buffer, 0, size);
                
                if (source_path[strlen(source_path) - 1] != '/') {
                    sprintf(source_buffer, "%s/%s", source_path, dir->d_name);
                }
                else {
                    sprintf(source_buffer, "%s%s", source_path, dir->d_name);
                }

                if (target_path[strlen(target_path) - 1] != '/') {
                    sprintf(target_buffer, "%s/%s", target_path, dir->d_name);
                }
                else {
                    sprintf(target_buffer, "%s%s", target_path, dir->d_name);
                }
                printf("Inner call %s - %s\n", source_buffer, target_buffer);
                fill_buffer(source_buffer, target_buffer);
            }
            else if (dir->d_type == DT_FIFO) {
                // Not handled for now.
            }
        }
        closedir(d);
    }
}

void consume(Node * item) {
    int source = item->entry.source_fd;
    int target = item->entry.target_fd;
    int count = 0;
    char buffer[COPY_BUFFER_SIZE];
    memset(buffer, '\0', COPY_BUFFER_SIZE);
    while((count = read(source, buffer, COPY_BUFFER_SIZE)) > 0) {
        write(target, buffer, count);
    }
    free(item);
}