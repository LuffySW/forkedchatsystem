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

// Struktur antrean klien
typedef struct {
    int client_socket;
    struct sockaddr_in client_address;
    char username[BUFFER_SIZE];
} ClientQueueItem;

// Variabel untuk manajemen antrean klien
ClientQueueItem client_queue[MAX_CLIENTS];
int queue_front = 0;
int queue_rear = -1;
int queue_size = 0;

// Variabel untuk komunikasi klien
int client_pipes[MAX_CLIENTS][2]; 
int client_count = 0;
int client_sockets[MAX_CLIENTS]; // Menyimpan socket klien aktif
char client_usernames[MAX_CLIENTS][BUFFER_SIZE]; // Menyimpan nama pengguna klien

// Fungsi untuk mencatat pesan ke log
void log_message(const char *level, const char *message) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Gagal membuka file log");
        return;
    }

    // Mendapatkan waktu saat ini
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // Format waktu
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // Menulis pesan log dengan timestamp
    fprintf(log_file, "[%s] [%s] %s", time_str, level, message);
    fclose(log_file);
}

// Menambahkan klien baru ke antrean
void enqueue_client(int client_socket, struct sockaddr_in client_address) {
    if (queue_size == MAX_CLIENTS) {
        printf("Antrean klien penuh. Tidak dapat menerima lebih banyak klien.\n");
        return;
    }
    queue_rear = (queue_rear + 1) % MAX_CLIENTS;
    client_queue[queue_rear].client_socket = client_socket;
    client_queue[queue_rear].client_address = client_address;
    queue_size++;
}

// Mengambil klien dari antrean
ClientQueueItem dequeue_client() {
    ClientQueueItem item = client_queue[queue_front];
    queue_front = (queue_front + 1) % MAX_CLIENTS;
    queue_size--;
    return item;
}

// Mengecek apakah antrean kosong
int is_queue_empty() {
    return queue_size == 0;
}

// Membersihkan proses zombie
void clean_up_zombie_processes(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Mengirim pesan ke semua klien kecuali pengirim
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

    // Membuat socket server
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Pembuatan socket gagal");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Pengaturan socket gagal");
        exit(EXIT_FAILURE);
    }

    // Konfigurasi alamat server
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Binding socket ke alamat dan port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind gagal");
        exit(EXIT_FAILURE);
    }

    // Server mulai mendengarkan koneksi
    if (listen(server_fd, 3) < 0) {
        perror("Listen gagal");
        exit(EXIT_FAILURE);
    }

    printf("Server berjalan di port %d\n", PORT);

    // Penanganan proses zombie
    signal(SIGCHLD, clean_up_zombie_processes);

    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Menambahkan socket klien ke set
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
            perror("Kesalahan pada select");
        }

        // Koneksi baru diterima
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("Kesalahan pada accept");
                exit(EXIT_FAILURE);
            }
            enqueue_client(new_socket, address);
        }

        // Memproses klien dalam antrean
        while (!is_queue_empty()) {
            ClientQueueItem client = dequeue_client();
            int client_socket = client.client_socket;

            // Menerima nama pengguna
            char username[BUFFER_SIZE];
            int bytes_received = read(client_socket, username, BUFFER_SIZE);
            if (bytes_received <= 0) {
                perror("Gagal menerima nama pengguna");
                close(client_socket);
                continue;
            }
            username[bytes_received] = '\0';

            // Menambahkan klien baru
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = client_socket;
                    strncpy(client_usernames[i], username, BUFFER_SIZE);
                    client_count++;
                    printf("Klien %s terhubung dari IP %s, port %d\n", username, inet_ntoa(client.client_address.sin_addr), ntohs(client.client_address.sin_port));

                    char join_message[BUFFER_SIZE];
                    snprintf(join_message, BUFFER_SIZE, "[Server]: %s telah bergabung dalam chat.\n", username);
                    send_to_all_clients(join_message, client_socket);
                    log_message("INFO", join_message);
                    break;
                }
            }
        }

        // Memproses pesan dari klien
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE];
                int valread = read(sd, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    getpeername(sd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("Klien %s terputus, IP %s, port %d\n", client_usernames[i], inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    char leave_message[BUFFER_SIZE];
                    snprintf(leave_message, BUFFER_SIZE, "[Server]: %s telah meninggalkan chat.\n", client_usernames[i]);
                    send_to_all_clients(leave_message, sd);
                    log_message("INFO", leave_message);

                    close(sd);
                    client_sockets[i] = 0;
                    client_count--;
                } else {
                    buffer[valread] = '\0';
                    printf("%s: %s\n", client_usernames[i], buffer);

                    char message[BUFFER_SIZE];
                    snprintf(message, BUFFER_SIZE, "%s: %s\n", client_usernames[i], buffer);
                    send_to_all_clients(message, sd);
                    log_message("CHAT", message);
                }
            }
        }
    }

    return 0;
}
