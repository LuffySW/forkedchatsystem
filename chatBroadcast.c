#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024
#define LOG_FILE "chat_log.txt"

// Variabel untuk mencatat klien
int client_sockets[FD_SETSIZE] = {0};
char client_usernames[FD_SETSIZE][BUFFER_SIZE];

// Fungsi untuk mencatat pesan ke log
void log_message(const char *level, const char *username, const char *message) {
    FILE *log_file = fopen(LOG_FILE, "a"); // Membuka file log dalam mode append
    if (log_file == NULL) {
        perror("[ERROR] Gagal membuka file log");
        return;
    }

    // Mendapatkan waktu saat ini
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // Format waktu
    char time_str[20];
    if (strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t) == 0) {
        fprintf(stderr, "[ERROR] Gagal memformat waktu untuk log\n");
        fclose(log_file);
        return;
    }

    // Menulis pesan ke log dengan timestamp, level, username, dan pesan
    if (username != NULL && strlen(username) > 0) {
        fprintf(log_file, "[%s] [%s] %s: %s\n", time_str, level, username, message);
    } else {
        fprintf(log_file, "[%s] [%s] %s\n", time_str, level, message);
    }

    fclose(log_file);
}

// Fungsi untuk broadcast pesan ke semua klien
void broadcast_message(int sender_fd, const char *message) {
    char broadcast_message[BUFFER_SIZE + BUFFER_SIZE];
    char sender_username[BUFFER_SIZE] = "Unknown";

    // Cari username pengirim
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (client_sockets[i] == sender_fd) {
            strncpy(sender_username, client_usernames[i], BUFFER_SIZE);
            break;
        }
    }

    snprintf(broadcast_message, sizeof(broadcast_message), "[CHAT] [%s]: %s", sender_username, message);

    for (int i = 0; i < FD_SETSIZE; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender_fd) {
            if (send(client_sockets[i], broadcast_message, strlen(broadcast_message), 0) == -1) {
                perror("Gagal mengirim pesan");
            }
        }
    }
}

void handle_client_message(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        // Handle disconnect
        close(client_fd);
        for (int i = 0; i < FD_SETSIZE; i++) {
            if (client_sockets[i] == client_fd) {
                char disconnect_message[BUFFER_SIZE];
                snprintf(disconnect_message, sizeof(disconnect_message), "Klien %s keluar dari chat.", client_usernames[i]);
                log_message("INFO", NULL, disconnect_message);
                printf("[INFO] %s\n", disconnect_message);
                client_sockets[i] = 0;
                memset(client_usernames[i], 0, BUFFER_SIZE);
                break;
            }
        }
    } else {
        buffer[bytes_received] = '\0';

        // Jika pesan adalah username
        if (strncmp(buffer, "USERNAME:", 9) == 0) {
            char *username = buffer + 9; // Ambil username setelah "USERNAME:"
            username[strcspn(username, "\n")] = '\0'; // Hilangkan karakter newline
            for (int i = 0; i < FD_SETSIZE; i++) {
                if (client_sockets[i] == client_fd) {
                    strncpy(client_usernames[i], username, BUFFER_SIZE);
                    char connect_message[BUFFER_SIZE];
                    snprintf(connect_message, sizeof(connect_message), "Klien %s terhubung.", username);
                    log_message("INFO", NULL, connect_message);
                    printf("[INFO] %s\n", connect_message);
                    break;
                }
            }
        } else {
            // Ambil username pengirim
            char sender_username[BUFFER_SIZE] = "Unknown";
            for (int i = 0; i < FD_SETSIZE; i++) {
                if (client_sockets[i] == client_fd) {
                    strncpy(sender_username, client_usernames[i], BUFFER_SIZE - 1);
                    break;
                }
            }

            // Log pesan dengan username
            log_message("CHAT", sender_username, buffer);

            // Tampilkan pesan di server
            printf("[CHAT] %s: %s\n", sender_username, buffer);

            // Broadcast pesan ke klien lain
            char full_message[BUFFER_SIZE + BUFFER_SIZE];
            snprintf(full_message, sizeof(full_message), "[CHAT] [%s]: %s", sender_username, buffer);
            broadcast_message(client_fd, full_message);
        }
    }
}

void handle_new_connection(int new_socket) {
    while (1) {
        handle_client_message(new_socket);
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Membuat socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Mengatur socket untuk memungkinkan multiple connections
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // Binding socket ke port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Mendengarkan koneksi
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port 8080\n");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("New connection, socket fd is %d, ip is : %s, port : %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                break;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_fd);
            handle_new_connection(new_socket);
            exit(0);
        } else if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
