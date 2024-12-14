#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char username[50];
    fd_set read_fds;

    // Membuat socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Konversi alamat IP ke binary
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // Terhubung ke server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to the server.\n");

    // Meminta username
    printf("Enter your username: ");
    fgets(username, 50, stdin);
    username[strcspn(username, "\n")] = 0; // Hapus karakter newline

    // Kirim username ke server
    send(sock, username, strlen(username), 0);

    printf("Welcome, %s..! You can now start chatting.\n", username);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        int max_sd = sock;

        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select error");
            continue;
        }

        // Jika ada pesan dari server
        if (FD_ISSET(sock, &read_fds)) {
            int valread = read(sock, buffer, BUFFER_SIZE);
            if (valread <= 0) {
                printf("Server disconnected\n");
                close(sock);
                exit(0);
            }

            buffer[valread] = '\0';
            printf("%s\n", buffer);
        }

        // Jika ada input dari pengguna
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            fgets(buffer, BUFFER_SIZE, stdin);

            // Hapus karakter newline
            buffer[strcspn(buffer, "\n")] = 0;

            // Cek input kosong
            if (strlen(buffer) == 0) {
                continue;
            }

            // Kirim pesan ke server
            send(sock, buffer, strlen(buffer), 0);
        }
    }

    return 0;
}
