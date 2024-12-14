#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#define PORT 8080
#define BUFFER_SIZE 1024

void connect_to_server(const char *username, int batch_mode) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    struct timespec start, end;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return;
    }

    if (strlen(username) >= BUFFER_SIZE) {
        printf("Username is too long. Maximum length is %d characters.\n", BUFFER_SIZE - 1);
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return;
    }

    // Record start time
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return;
    }

    // Record end time
    clock_gettime(CLOCK_MONOTONIC, &end);
    double response_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("Client %s connected in %.2f ms\n", username, response_time);

    // Send username to server
    send(sock, username, strlen(username), 0);

    fd_set readfds;
    int max_sd = sock;

    // Chat loop
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        if (!batch_mode) {
            FD_SET(STDIN_FILENO, &readfds);
        }

        // Always show the prompt at the beginning of each iteration if not in batch mode
        if (!batch_mode) {
            printf("Enter message (type 'exit' to disconnect): ");
            fflush(stdout);
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        // Check if there is data from the server
        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = read(sock, buffer, BUFFER_SIZE);
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    printf("Server closed the connection.\n");
                } else {
                    perror("Failed to receive message from server");
                }
                break;
            }
            printf("\nServer reply: %s\n", buffer);
        }

        // Check if there is input from the user
        if (!batch_mode && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                if (feof(stdin)) {
                    printf("End of input detected. Disconnecting...\n");
                    break;
                } else {
                    perror("Error reading input");
                    break;
                }
            }

            // Remove newline character
            buffer[strcspn(buffer, "\n")] = '\0';

            if (strlen(buffer) == 0) {
                printf("Message cannot be empty.\n");
                continue;
            } else if (strlen(buffer) >= BUFFER_SIZE) {
                printf("Message is too long. Maximum length is %d characters.\n", BUFFER_SIZE - 1);
                continue;
            }

            // Check if the user wants to exit
            if (strcmp(buffer, "exit") == 0) {
                printf("Disconnecting...\n");
                break;
            }

            // Send the message to the server
            if (send(sock, buffer, strlen(buffer), 0) <= 0) {
                perror("Failed to send message");
                break;
            }
        }
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <username> [batch]\n", argv[0]);
        return -1;
    }

    int batch_mode = 0;
    if (argc == 3 && strcmp(argv[2], "batch") == 0) {
        batch_mode = 1;
    }

    connect_to_server(argv[1], batch_mode);
    return 0;
}