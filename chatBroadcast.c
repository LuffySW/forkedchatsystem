#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 30
#define LOG_FILE "chat_log.txt"

// Struktur untuk pesan dalam pipe
typedef struct {
    int sender_fd;
    char message[BUFFER_SIZE];
} PipeMessage;

typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
    char username[BUFFER_SIZE];
} ClientQueueItem;

ClientQueueItem client_queue[MAX_CLIENTS];
int queue_front = 0;
int queue_rear = -1;
int queue_size = 0;

int client_pipes[MAX_CLIENTS][2]; // Pipe untuk komunikasi antar-proses
int client_count = 0;
int client_sockets[MAX_CLIENTS]; // Track active client sockets
char client_usernames[MAX_CLIENTS][BUFFER_SIZE]; // Track client usernames

void log_message(const char *level, const char *message) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return;
    }
    fprintf(log_file, "[%s] %s\n", level, message);
    fclose(log_file);
}

void enqueue_client(int client_socket, struct sockaddr_in client_address) {
    if (queue_size == MAX_CLIENTS) {
        printf("Client queue is full. Cannot accept more clients.\n");
        return;
    }
    queue_rear = (queue_rear + 1) % MAX_CLIENTS;
    client_queue[queue_rear].client_socket = client_socket;
    client_queue[queue_rear].client_address = client_address;
    queue_size++;
}

ClientQueueItem dequeue_client() {
    ClientQueueItem item = client_queue[queue_front];
    queue_front = (queue_front + 1) % MAX_CLIENTS;
    queue_size--;
    return item;
}

int is_queue_empty() {
    return queue_size == 0;
}

void clean_up_zombie_processes(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void send_to_all_clients(const char *message, int exclude_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != exclude_fd) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d\n", PORT);

    // Handle zombie processes
    signal(SIGCHLD, clean_up_zombie_processes);

    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Add client sockets to set
        for (int i = 0; i < client_count; i++) {
            int sd = client_sockets[i];
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0 && errno != EINTR) {
            perror("Select error");
        }

        // If something happened on the master socket, then it's an incoming connection
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("Accept error");
                exit(EXIT_FAILURE);
            }

            // Enqueue the new client
            enqueue_client(new_socket, address);
        }

        // Process clients in the queue
        while (!is_queue_empty()) {
            ClientQueueItem client = dequeue_client();
            int client_socket = client.client_socket;

            // Receive username from client
            char username[BUFFER_SIZE];
            int bytes_received = read(client_socket, username, BUFFER_SIZE);
            if (bytes_received <= 0) {
                perror("Failed to receive username");
                close(client_socket);
                continue;
            }
            username[bytes_received] = '\0';

            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    strncpy(client_usernames[i], username, BUFFER_SIZE);
                    client_count++;
                    printf("Client %s connected from ip %s, port %d\n", username, inet_ntoa(client.client_address.sin_addr), ntohs(client.client_address.sin_port));

                    // Notify all clients about the new connection
                    char join_message[BUFFER_SIZE];
                    snprintf(join_message, BUFFER_SIZE, "[Server]: %s has joined the chat.\n", username);
                    send_to_all_clients(join_message, client_socket);

                    // Log the join message
                    log_message("INFO", join_message);
                    break;
                }
            }
        }

        // Handle IO operations for each client
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];

            if (FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("Host %s disconnected, ip %s, port %d\n", client_usernames[i], inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    // Notify all clients about the disconnection
                    char leave_message[BUFFER_SIZE];
                    snprintf(leave_message, BUFFER_SIZE, "[Server]: %s has left the chat.\n", client_usernames[i]);
                    send_to_all_clients(leave_message, sd);

                    // Log the leave message
                    log_message("INFO", leave_message);

                    close(sd);
                    client_sockets[i] = 0;
                    client_count--;
                } else {
                    // Echo back the message that came in
                    buffer[valread] = '\0';
                    printf("%s: %s\n", client_usernames[i], buffer);

                    // Send the message to all clients
                    char message[BUFFER_SIZE];
                    snprintf(message, BUFFER_SIZE, "%s: %s\n", client_usernames[i], buffer);
                    send_to_all_clients(message, sd);

                    // Log the message
                    log_message("CHAT", message);
                }
            }
        }
    }

    return 0;
}