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

#define FILENAME_BUFFER_SIZE 250

typedef struct _entry {
    int source_fd;
    int target_fd;
    char filename[FILENAME_BUFFER_SIZE];
    long size;
} Entry;

typedef struct _node {
    Entry entry;
    struct _node * next;
} Node;

Entry produce(char * source_path, char * target_path, char * filename);
void consume(Node * item);
void fill_buffer(char * source_path, char * target_path);


int producer_finished = 0;
int consumer_finished = 0;
int consumed_item_count = 0;
long consumed_bytes = 0;
int BUFFER_SIZE;
int NUM_OF_CONSUMERS;
int COPY_BUFFER_SIZE = 1024 * 1024;
pthread_t *consumer_thread, main_thread_id;
pthread_mutex_t lock, thread_kill;
sem_t empty, full;
Node * head = NULL, * last;

void signal_handler(int sig) {
    if (main_thread_id == pthread_self()) {
        // clear memory
        free(consumer_thread);
        pthread_mutex_destroy(&lock);
        pthread_mutex_destroy(&thread_kill);
        sem_destroy(&full);
        sem_destroy(&empty);
        exit(EXIT_FAILURE);
    }
    else {
        pthread_exit(NULL);
    }
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
        if (consumer_finished) {
            pthread_mutex_unlock(&lock);
            break;
        }
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
        //printf("Current consumer item %s\n", current->entry.filename);
        consume(current);
        sem_getvalue(&full, &count);
    }
    // Finished consuming signal other consumer threads if theyre waiting on semaphore
    //printf("Finished consuming %ld\n", pthread_self());
    consumer_finished = 1;
    sem_post(&full); // Unlock other thread if they've hung in sem_wait

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    main_thread_id = pthread_self();
    
    struct timeval start, end;
    NUM_OF_CONSUMERS = atoi(argv[1]);
    BUFFER_SIZE = atoi(argv[2]);
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("Mutex init failed\n");
        return 1;
    }

    if (pthread_mutex_init(&thread_kill, NULL) != 0)
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
    if (pthread_create(&producer_thread_id, NULL, producer, (void *) &argv[3])) {
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
    printf("\nTotal:\n%d files\n%ld microseconds\n%ld bytes\n", consumed_item_count, (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000, consumed_bytes);
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&thread_kill);
    sem_destroy(&full);
    sem_destroy(&empty);

    return 0;
}

Entry produce(char * source_path, char * target_path, char * filename) {
    Entry entry;
    int size = fmax(strlen(source_path), strlen(target_path)) + strlen(filename) + 2;
    char str_buffer[size];

    memset(str_buffer, '\0', size);
    memset(entry.filename, 0, FILENAME_BUFFER_SIZE);
    strcpy(entry.filename, filename);

    sprintf(str_buffer, "%s/%s", source_path, filename);
    entry.source_fd = open(str_buffer, O_RDONLY);
    mkdir(target_path, S_IRWXG | S_IRWXO | S_IRWXU);
    sprintf(str_buffer, "%s/%s", target_path, filename);
    entry.target_fd = open(str_buffer, O_WRONLY | O_CREAT | O_TRUNC);

    return entry;
}

void fill_buffer(char * source_path, char * target_path) {   
    // printf("Directory to copy from: %s\n", source_path);
    // printf("Directory to copy to: %s\n", target_path);

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
    item->entry.size = 0;

    while((count = read(source, buffer, COPY_BUFFER_SIZE)) > 0) {
        //printf("writing %s\n", item->entry.filename);
        write(target, buffer, count);
        item->entry.size += count;
        pthread_yield();
    }
    consumed_item_count++;
    consumed_bytes += item->entry.size;
    close(source);
    close(target);
    printf("Copied %s (%ld bytes).\n", item->entry.filename, item->entry.size);
    pthread_yield();
    free(item);
}