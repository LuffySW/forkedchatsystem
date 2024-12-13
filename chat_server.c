#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#define PORT 8080
#define BUF_SIZE 1024
#define MAX_CLIENTS 10

int clients[MAX_CLIENTS] = {0};

void handle_client(int client_sock) {
    char buffer[BUF_SIZE];
    char username[50];

    // Get username from client
    send(client_sock, "Enter your username: ", strlen("Enter your username: "), 0);
    int read_size = recv(client_sock, username, sizeof(username) - 1, 0);
    if (read_size <= 0) {
        close(client_sock);
        exit(0);
    }
    username[read_size - 1] = '\0'; // Remove newline character

    // Add client to the client list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = client_sock;
            break;
        }
    }

    // Notify all clients about the new connection
    snprintf(buffer, sizeof(buffer), "%s has joined the chat\n", username);
    broadcast_message(buffer, client_sock);

    // Interaction with client
    while ((read_size = recv(client_sock, buffer, BUF_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received message from %s: %s\n", username, buffer); // Debug statement

        // Broadcast message to all clients
        snprintf(buffer, sizeof(buffer), "[%s]: %s\n", username, buffer);
        broadcast_message(buffer, client_sock);
    }

    // Remove client from the list when they disconnect
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_sock) {
            clients[i] = 0;
            break;
        }
    }

    // Notify all clients about the disconnection
    snprintf(buffer, sizeof(buffer), "%s has left the chat\n", username);
    broadcast_message(buffer, client_sock);

    close(client_sock);
    exit(0);
}

int main() {
    int server_sock, client_sock, client_len;
    struct sockaddr_in server, client;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_sock, 3);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        client_len = sizeof(client);
        client_sock = accept(server_sock, (struct sockaddr *)&client, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected...\n");

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            close(client_sock);
            continue;
        }

        if (pid == 0) {
            // Child process handles the client
            close(server_sock); // Child process does not need the server socket
            handle_client(client_sock);
            close(client_sock);
            exit(0);
        } else {
            // Parent process continues to listen for other clients
            close(client_sock);
        }
    }

    return 0;
}
