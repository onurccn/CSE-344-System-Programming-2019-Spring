#define _GNU_SOURCE

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ftw.h>

#define BUFFER_SIZE 1024
#define DIR_BUFFER 100

char server_dir[DIR_BUFFER];

int remove_directory(char *path);
void append_server_dir(char * filename);
void * client_handler(void *arg);
void send_dir(int socket_fd, char * source_path);

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./BibakBOXServer [directory] [threadPoolSize] [portnumber]\n");
        return -1;
    }

    int server_fd, incoming_socket; 
    struct sockaddr_in address; 
    int opt = 1, threadPoolSize = atoi(argv[2]), port = atoi(argv[3]);
    int addrlen = sizeof(address); 
    pthread_t tId;

    memset(server_dir, 0, DIR_BUFFER);
    strcpy(server_dir, argv[1]);
    // Operate under given directory, create if not present
    mkdir(server_dir, S_IRWXG | S_IRWXO | S_IRWXU);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        printf("[SERVER] Couldn't create socket\n");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080 
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) 
    { 
        perror("[SERVER] Set socket error\n"); 
        exit(EXIT_FAILURE); 
    } 

    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) 
    {
        printf("[SERVER] Bind error\n"); 
        exit(EXIT_FAILURE); 
    }

    if (listen(server_fd, threadPoolSize) < 0) 
    { 
        printf("[SERVER] Listen error\n"); 
        exit(EXIT_FAILURE);
    }

    while ((incoming_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0)
    { 
        char address_str[100];
        inet_ntop(AF_INET, &(address.sin_addr), address_str, INET_ADDRSTRLEN);
        printf("[SERVER] Accepted incoming client connection for %s\n", address_str);
        pthread_create(&tId, NULL, client_handler, &incoming_socket);
        
    }

    return 0;
}

void append_server_dir(char * filename) {
    char temp[BUFFER_SIZE];
    memset(temp, 0, BUFFER_SIZE);
    if (filename[0] == '/') {
        // Path is absolute path, do nothin.
        return;
    }

    strcpy(temp, server_dir);
    if (temp[strlen(temp) - 1] != '/') {
        strcat(temp, "/");
    }
    strcat(temp, filename);
    memcpy(filename, temp, BUFFER_SIZE);
}

void * client_handler(void *arg) {
    int *args = (int *)arg;
    int client_socket = args[0], iter = 0, scan_mode = 0;
    char buffer[BUFFER_SIZE];
    char client_dir[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    while(read(client_socket, buffer, BUFFER_SIZE) > 0) {
        if (strncmp(buffer, "REG", 3) == 0) {
            // New regular file received
            char file_name[BUFFER_SIZE];
            int read_file = 1, file_fd;
            struct stat f_stat;
            time_t client_file_time;
            memset(buffer, 0, BUFFER_SIZE);
            memset(file_name, 0, BUFFER_SIZE);
            read(client_socket, file_name, BUFFER_SIZE);
            printf("[SERVER] Checking client file '%s'\n", file_name); // Log status.
            
            append_server_dir(file_name);
            
            read(client_socket, &client_file_time, sizeof(time_t));
            if (stat(file_name, &f_stat) == 0) {
                double time_diff = difftime(f_stat.st_mtime, client_file_time);
                if (time_diff > 0) {
                    // Server file is newer send +
                    sprintf(buffer, "+");
                    write(client_socket, buffer, BUFFER_SIZE);

                    file_fd = open(file_name, O_RDONLY);
                    int read_count;
                    while ((read_count = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                        send(client_socket, buffer, BUFFER_SIZE, 0);
                        send(client_socket, &read_count, sizeof(int), 0);
                    }
                    sprintf(buffer, "END");
                    send(client_socket, buffer, BUFFER_SIZE, 0);
                    
                    // Sent regular file to the client. Wait for next file from client
                    continue;
                }
            }

            // Means we are short about this file, receive it from client
            sprintf(buffer, "-");
            send(client_socket, buffer, BUFFER_SIZE, 0);

            file_fd = open(file_name, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRWXO | S_IRWXG);
            while (read_file && read(client_socket, buffer, BUFFER_SIZE) > 0) {
                if (strncmp(buffer, "END", 3) == 0) {
                    // File finished
                    read_file = 0;
                    close(file_fd);
                    printf("\n");
                }
                else {
                    int write_count;
                    read(client_socket, &write_count, sizeof(int));
                    write(file_fd, buffer, write_count);
                    printf(".");
                }
            }
        }
        else if (strncmp(buffer, "DIR", 3) == 0) {
            // Mkdir
            char dir_name[BUFFER_SIZE];
            memset(dir_name, 0, BUFFER_SIZE);
            read(client_socket, dir_name, BUFFER_SIZE);
            append_server_dir(dir_name);
            if (iter == 0) {
                // Hold base client directory
                strcpy(client_dir, dir_name);
            }
            iter++;
            printf("[SERVER] New client directory '%s'\n", dir_name);
            int dir_result = mkdir(dir_name, S_IRWXG | S_IRWXO | S_IRWXU);
            if (scan_mode && dir_result == -1) {
                remove_directory(dir_name);
            }
        }
        else if (strncmp(buffer, "END", 3) == 0) {
            // end loop
            break;
        }
        else if (strncmp(buffer, "SND", 3) == 0) {
            send(client_socket, server_dir, BUFFER_SIZE, 0);
            send_dir(client_socket, client_dir);
            sprintf(buffer, "END");
            send(client_socket, buffer, BUFFER_SIZE, 0);
            // After this part we can continue on scane mode
            scan_mode = 1;
        }
    }
    printf("[SERVER] Client for %s exited\n", client_dir);
    close(client_socket);
    pthread_exit(NULL);
}

void send_dir(int socket_fd, char * source_path) {
    DIR * d;
    struct dirent *dir;
    char buffer[BUFFER_SIZE];
    
    // SEND MKDIR COMMAND
    sprintf(buffer, "DIR");
    send(socket_fd, buffer, BUFFER_SIZE, 0);
    send(socket_fd, source_path, BUFFER_SIZE, 0);

    d = opendir(source_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strlen(dir->d_name) > 0 && dir->d_name[0] == '.') continue;
            memset(buffer, 0, BUFFER_SIZE);
            int size = strlen(source_path) + strlen(dir->d_name) + 1;
            char source_buffer[BUFFER_SIZE];
            memset(source_buffer, 0, size);
            
            if (source_path[strlen(source_path) - 1] != '/') {
                sprintf(source_buffer, "%s/%s", source_path, dir->d_name);
            }
            else {
                sprintf(source_buffer, "%s%s", source_path, dir->d_name);
            }
            
            if (dir->d_type == DT_REG) {
                sprintf(buffer, "REG");
                send(socket_fd, buffer, BUFFER_SIZE, 0);    // Send reg operand
                send(socket_fd, source_buffer, BUFFER_SIZE, 0); // Send filename

                read(socket_fd, buffer, BUFFER_SIZE);

                int file_fd;
                if (buffer[0] == '-') {
                    // '-' means client doesnt have this file, so send it.
                    file_fd = open(source_buffer, O_RDONLY);
                    int read_count;
                    while((read_count = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
                        send(socket_fd, buffer, BUFFER_SIZE, 0);
                        send(socket_fd, &read_count, sizeof(int), 0);
                    }
                    sprintf(buffer, "END");
                    send(socket_fd, buffer, BUFFER_SIZE, 0);
                }
                else {
                    // client has file.
                    continue;
                }
            }
            else if (dir->d_type == DT_DIR) {
                send_dir(socket_fd, source_buffer);
            }
            else if (dir->d_type == DT_FIFO) {
                // Not handled for now.
            }
        }
        closedir(d);
    }

    printf("[SERVER] Finished initial directory scan for %s\n", source_path);
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    return remove(fpath);
}

int remove_directory(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}