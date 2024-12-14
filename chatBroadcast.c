#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10
#define LOG_BUFFER_SIZE 2048 // Increase log buffer size

int client_sockets[MAX_CLIENTS] = {0};
int pipe_fd[2]; // Pipe for inter-process communication

void log_message(const char *level, const char *message) {
    FILE *log_file = fopen("chat_log.txt", "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }

    time_t now = time(NULL);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp) - 1] = '\0'; // Remove newline character

    fprintf(log_file, "[%s] [%s] %s\n", timestamp, level, message);
    fclose(log_file);
}

// Broadcast message to all clients except the sender
void broadcast_message(int sender_fd, char *message) {
    char log_buffer[LOG_BUFFER_SIZE];
    snprintf(log_buffer, LOG_BUFFER_SIZE, "Broadcasting message from client %d: %.900s", sender_fd, message);
    log_message("INFO", log_buffer);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender_fd) {
            if (send(client_sockets[i], message, strlen(message), 0) == -1) {
                perror("Broadcast send error");
                snprintf(log_buffer, LOG_BUFFER_SIZE, "Broadcast send error to client %d", client_sockets[i]);
                log_message("ERROR", log_buffer);
            }
        }
    }
}

// Handle client communication in child process
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int valread;

    // Menerima username dari klien
    valread = read(client_socket, buffer, BUFFER_SIZE);
    if (valread <= 0) {
        perror("Failed to receive username");
        close(client_socket);
        return;
    }
    buffer[valread] = '\0'; // Null-terminate the username
    char username[50];
    strncpy(username, buffer, 50);

    printf("Client connected with username: %s\n", username);
    char log_buffer[LOG_BUFFER_SIZE];
    snprintf(log_buffer, LOG_BUFFER_SIZE, "Client connected with username: %s, socket fd: %d", username, client_socket);
    log_message("INFO", log_buffer);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        valread = read(client_socket, buffer, BUFFER_SIZE);

        // Check for disconnection or error
        if (valread <= 0) {
            if (valread == 0) {
                printf("Client %s disconnected, socket fd: %d\n", username, client_socket);
                snprintf(log_buffer, LOG_BUFFER_SIZE, "Client %s disconnected, socket fd: %d", username, client_socket);
                log_message("INFO", log_buffer);
            } else {
                perror("Read error");
                log_message("ERROR", "Read error");
            }
            close(client_socket);

            // Remove client from the active socket list
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == client_socket) {
                    client_sockets[i] = 0;
                    break;
                }
            }

            exit(0); // Exit child process
        }

        buffer[valread] = '\0'; // Null-terminate the message
        printf("Message from client %s: %s\n", username, buffer);
        snprintf(log_buffer, LOG_BUFFER_SIZE, "Message from client %s: %.900s", username, buffer);
        log_message("INFO", log_buffer);

        // Lock the pipe before writing to it
        flock(pipe_fd[1], LOCK_EX);
        // Write the message to the pipe for the parent to broadcast
        dprintf(pipe_fd[1], "%d:%s\n", client_socket, buffer);
        // Unlock the pipe after writing to it
        flock(pipe_fd[1], LOCK_UN);
    }
}

// Signal handler to clean up zombie processes
void sigchld_handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Set up signal handler for SIGCHLD
    signal(SIGCHLD, sigchld_handler);

    // Create pipe for inter-process communication
    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        exit(EXIT_FAILURE);
    }

    // Make the read end of the pipe non-blocking
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set up the address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port : %d\n", PORT);
    log_message("INFO", "Server started listening");

    fd_set readfds;
    int max_sd;

    while (1) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        // Add pipe read end to set
        FD_SET(pipe_fd[0], &readfds);
        if (pipe_fd[0] > max_sd) {
            max_sd = pipe_fd[0];
        }

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];

            // If valid socket descriptor then add to read list
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }

            // Highest file descriptor number, need it for the select function
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        // Wait for an activity on one of the sockets
        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
            continue;
        }

        // If something happened on the server socket, then it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("New client connected, socket fd: %d\n", new_socket);
            char log_buffer[LOG_BUFFER_SIZE];
            snprintf(log_buffer, LOG_BUFFER_SIZE, "New client connected, socket fd: %d", new_socket);
            log_message("INFO", log_buffer);

            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }

            // Fork a new process to handle the client
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                close(server_fd); // Child does not need the listening socket
                handle_client(new_socket);
            } else if (pid > 0) {
                // Parent process
                // Do not close the new_socket here
            } else {
                perror("Fork failed");
                close(new_socket);
            }
        }

        // Check for messages in the pipe and broadcast them
        if (FD_ISSET(pipe_fd[0], &readfds)) {
            char pipe_buffer[BUFFER_SIZE];
            while (read(pipe_fd[0], pipe_buffer, BUFFER_SIZE) > 0) {
                int sender_fd;
                char message[BUFFER_SIZE];

                // Parse the message format: "sender_fd:message"
                sscanf(pipe_buffer, "%d:%[^\n]", &sender_fd, message);

                // Broadcast the message to other clients
                broadcast_message(sender_fd, message);
            }
        }

        // Check for client socket events
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = read(sd, buffer, sizeof(buffer));

                if (valread <= 0) {
                    // Handle disconnection
                    if (valread == 0) {
                        printf("Client disconnected, socket fd: %d\n", sd);
                        char log_buffer[LOG_BUFFER_SIZE];
                        snprintf(log_buffer, LOG_BUFFER_SIZE, "Client disconnected, socket fd: %d", sd);
                        log_message("INFO", log_buffer);
                    } else {
                        perror("Read error");
                        log_message("ERROR", "Read error");
                    }
                    close(sd);
                    FD_CLR(sd, &readfds); // Remove from select set
                    client_sockets[i] = 0; // Remove client from list
                } else {
                    buffer[valread] = '\0'; // Null-terminate the message
                    printf("Message from client %d: %s\n", sd, buffer);
                    char log_buffer[LOG_BUFFER_SIZE];
                    snprintf(log_buffer, LOG_BUFFER_SIZE, "Message from client %d: %.900s", sd, buffer);
                    log_message("INFO", log_buffer);
                }
            }
        }
    }

    return 0;
}