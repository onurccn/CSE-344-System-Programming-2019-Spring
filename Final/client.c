#define _GNU_SOURCE
#define _POSIX_C_SOURCE >= 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <errno.h>

#define BUFFER_SIZE 1024

void send_dir(int socket_fd, char * source_path);
void receive_server_files(int server_socket);
void remove_server_base(char * server_dir, char * filename);

int main(int argc, char *argv[])
{
    if (argc != 3) { 
        printf("Usage: ./BibakBOXClient [dirName] [portNumber]\n");
        return -1;
    }
    
    int sock;
    int port = atoi(argv[2]);
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[CLIENT] Created socket on client side.\n");
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, '\0', sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(port); 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)  
    { 
        printf("[CLIENT] Invalid address / Address not supported \n"); 
        return -1; 
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    {
        printf("[CLIENT] Connection Failed \n");
        return -1;
    }

    send_dir(sock, argv[1]);
    printf("[CLIENT] Finished initial directory scan for %s\n", argv[1]);
    // Receive remaining files from server. Maybe server has files that client doesn't have.
    memset(buffer, 0, BUFFER_SIZE);
    sprintf(buffer, "SND");
    send(sock, buffer, BUFFER_SIZE, 0);
    receive_server_files(sock);

    int shouldExit = 0;
    while (!shouldExit) {
        sleep(5); // Check every 5 seconds
        // Send current directory to server
        printf("[CLIENT] Client scanning for changes\n");
        send_dir(sock, argv[1]);
    }
    
    /* Close socket. */
    close(sock);

    exit(EXIT_SUCCESS);
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

                struct stat f_stat;
                stat(source_buffer, &f_stat);
                f_stat.st_mtime;
                send(socket_fd, &f_stat.st_mtime, sizeof(time_t), 0);
                read(socket_fd, buffer, BUFFER_SIZE);

                int file_fd, read_file = 1;
                if (buffer[0] == '-') {
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
                    file_fd = open(source_buffer, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRWXO | S_IRWXG);
                    while (read_file && read(socket_fd, buffer, BUFFER_SIZE) > 0) {
                        if (strncmp(buffer, "END", 3) == 0) {
                            // File finished
                            read_file = 0;
                            close(file_fd);
                            printf("\n");
                        }
                        else {
                            int write_size;
                            read(socket_fd, &write_size, sizeof(int)); 
                            write(file_fd, buffer, write_size);
                        }
                    }
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
}

void receive_server_files(int server_socket) {
    int client_socket = server_socket;
    char buffer[BUFFER_SIZE];
    char server_base[BUFFER_SIZE];
    read(client_socket, server_base, BUFFER_SIZE);

    while(read(client_socket, buffer, BUFFER_SIZE) > 0) {
        if (strncmp(buffer, "REG", 3) == 0) {
            // New regular file received
            char file_name[BUFFER_SIZE];
            int read_file = 0, file_fd;
            struct stat f_stat;
            memset(buffer, 0, BUFFER_SIZE);
            memset(file_name, 0, BUFFER_SIZE);
            read(client_socket, file_name, BUFFER_SIZE);
            printf("[CLIENT] Checking server file '%s'\n", file_name); // Log status.
            
            remove_server_base(server_base, file_name);
            
            // We will only check if file exists or not on client directory. We just updated current files so no need to update existing files. 
            if (stat(file_name, &f_stat) == 0) {
                // Client has file. No need to read from server.
                read_file = 0;
                sprintf(buffer, "+");
            }
            else {
                // Client doesn't have file. Server will send it.
                read_file = 1;
                sprintf(buffer, "-");
            }
            send(client_socket, buffer, BUFFER_SIZE, 0);        // Notify server about file status

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
            remove_server_base(server_base, dir_name);
            printf("[CLIENT] Checking server directory '%s'\n", dir_name);
            mkdir(dir_name, S_IRWXG | S_IRWXO | S_IRWXU);    // Dont care error case. Just make sure directory is there. 
            // TODO: probably need to erase if dir is already present
        }
        else if (strncmp(buffer, "END", 3) == 0) {
            break;
        }
    }
    printf("[CLIENT] Server sync finished\n");
}

void remove_server_base(char * server_dir, char * filename) {
    if (filename[0] == '/') {
        return;
    }
    else {
        int base_length = strlen(server_dir) + 1;
        int filename_length = strlen(filename);
        for (size_t i = 0; i < filename_length; i++)
        {
            filename[i] = filename[i + base_length];
        }
    }
}