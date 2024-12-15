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
#define MAX_USERNAME_LENGTH 50

// Fungsi untuk validasi username
int validate_username(const char *username) {
    if (strlen(username) == 0 || strlen(username) > MAX_USERNAME_LENGTH) {
        printf("Username tidak valid. Harus antara 1-%d karakter.\n", MAX_USERNAME_LENGTH);
        return 0;
    }
    for (size_t i = 0; i < strlen(username); i++) {
        if (!isalnum(username[i]) && username[i] != '_') {
            printf("Username hanya boleh mengandung karakter alfanumerik atau '_'.\n");
            return 0;
        }
    }
    return 1;
}

// Fungsi untuk menghubungkan client ke server
void connect_to_server(const char *username, int batch_mode) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    struct timespec start, end;

    // Membuat socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Gagal membuat socket");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Konversi alamat IP ke format biner
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Alamat tidak valid atau tidak didukung");
        close(sock);
        return;
    }

    // Mencatat waktu sebelum mencoba koneksi
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Menghubungkan ke server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Gagal menghubungkan ke server");
        close(sock);
        return;
    }

    // Mencatat waktu setelah koneksi berhasil
    clock_gettime(CLOCK_MONOTONIC, &end);
    double response_time = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("Client %s terhubung dalam waktu %.2f ms\n", username, response_time);

    // Mengirimkan username ke server dalam format yang sesuai
    char username_message[BUFFER_SIZE];
    snprintf(username_message, sizeof(username_message), "USERNAME:%s", username);
    if (send(sock, username_message, strlen(username_message), 0) <= 0) {
        perror("Gagal mengirimkan username ke server");
        close(sock);
        return;
    }

    fd_set readfds;
    int max_sd = sock;

    // Loop utama untuk chat
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        if (!batch_mode) {
            FD_SET(STDIN_FILENO, &readfds); // Tambahkan input keyboard
        }

        // Menampilkan prompt jika tidak dalam mode batch
        if (!batch_mode) {
            printf("Ketik pesan (ketik 'exit' untuk keluar): ");
            fflush(stdout);
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Error pada fungsi select");
            break;
        }

        // Jika ada data dari server
        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = read(sock, buffer, BUFFER_SIZE);
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    printf("Server menutup koneksi.\n");
                } else {
                    perror("Gagal menerima pesan dari server");
                }
                break;
            }
            printf("\nPesan dari server: %s\n", buffer);
        }

        // Jika ada input dari pengguna
        if (!batch_mode && FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
                if (feof(stdin)) {
                    printf("Input berakhir. Memutus koneksi...\n");
                    break;
                } else {
                    perror("Error membaca input");
                    break;
                }
            }

            // Menghapus karakter newline
            buffer[strcspn(buffer, "\n")] = '\0';

            if (strlen(buffer) == 0) {
                printf("Pesan tidak boleh kosong.\n");
                continue;
            } else if (strlen(buffer) >= BUFFER_SIZE) {
                printf("Pesan terlalu panjang. Maksimum %d karakter.\n", BUFFER_SIZE - 1);
                continue;
            }

            // Jika pengguna mengetik 'exit', putus koneksi
            if (strcmp(buffer, "exit") == 0) {
                printf("Memutus koneksi...\n");
                break;
            }

            // Mengirimkan pesan ke server
            if (send(sock, buffer, strlen(buffer), 0) <= 0) {
                perror("Gagal mengirim pesan");
                break;
            }
        }
    }

    // Memutus koneksi ke server
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Penggunaan: %s <username> [batch]\n", argv[0]);
        return -1;
    }

    // Validasi username
    if (!validate_username(argv[1])) {
        return -1;
    }

    int batch_mode = 0;
    if (argc == 3 && strcmp(argv[2], "batch") == 0) {
        batch_mode = 1; // Aktifkan mode batch jika ada argumen "batch"
    }

    connect_to_server(argv[1], batch_mode);
    return 0;
}
