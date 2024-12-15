# Server Multi-Client Chat Server Broadcast

Anggota:
1. Fadilah Akbar : 231524041
2. Luthfi Satrio Wicaksono  : 231524049

Proyek ini adalah implementasi **server multi-client** sederhana untuk aplikasi chat berbasis **socket TCP** dengan fitur **broadcast pesan**. Server ini dirancang untuk menangani banyak klien sekaligus dengan memanfaatkan metode **fork()** pada bahasa pemrograman C, memungkinkan setiap klien untuk dilayani oleh proses anak yang independen.

## Fitur Utama
- **Broadcast Pesan**: Pesan dari satu klien akan diteruskan ke semua klien lain yang terhubung.
- **Multi-Client Handling**: Server dapat menangani hingga 30 klien secara bersamaan.
- **Pencatatan Log**: Semua aktivitas server, termasuk pesan yang dikirimkan dan klien yang bergabung atau keluar, dicatat dalam file log.
- **Mode Interaktif dan Batch pada Klien**:
  - Mode interaktif: Klien dapat mengetik pesan langsung di terminal.
  - Mode batch: Klien hanya menerima pesan tanpa memasukkan input.
- **Manajemen Proses Zombie**: Server membersihkan proses anak yang sudah selesai menggunakan sinyal `SIGCHLD`.

## Arsitektur Program
Program ini terdiri dari dua komponen utama:
1. **Server**:
   - Membuat socket untuk mendengarkan koneksi klien.
   - Menerima koneksi masuk dan menciptakan proses anak menggunakan `fork()` untuk menangani komunikasi dengan klien tersebut.
   - Menggunakan **pipe** untuk komunikasi antar proses.
   - Memantau aktivitas menggunakan fungsi **select()**, memungkinkan server untuk menangani beberapa klien sekaligus tanpa terblokir.
   - Mencatat semua aktivitas ke file log.

2. **Client**:
   - Menghubungkan diri ke server melalui socket TCP.
   - Mengirimkan username ke server sebagai identifikasi.
   - Mendukung mode interaktif (mengirim pesan) atau batch (hanya menerima pesan).
   - Menerima pesan broadcast dari server dan menampilkannya di terminal.

## Cara Kerja
1. **Server**:
   - Server berjalan dan mendengarkan koneksi di port 8080.
   - Saat klien terhubung, server menerima koneksi, mencatat username klien, dan memulai proses anak untuk menangani komunikasi.
   - Pesan yang diterima dari klien akan disebarkan ke semua klien lain melalui fungsi broadcast.

2. **Client**:
   - Klien terhubung ke server dengan alamat IP dan port yang telah ditentukan.
   - Klien dapat mengetik pesan di terminal, yang akan dikirimkan ke server dan diteruskan ke klien lain.

## Teknologi yang Digunakan
- **Bahasa Pemrograman**: C
- **Protokol**: TCP
- **Library Utama**:
  - `<arpa/inet.h>`: Untuk manajemen alamat IP dan komunikasi jaringan.
  - `<sys/socket.h>`: Untuk operasi socket.
  - `<unistd.h>`: Untuk operasi file descriptor dan proses.
  - `<sys/select.h>`: Untuk memantau banyak file descriptor.
  - `<signal.h>`: Untuk menangani sinyal `SIGCHLD` dalam membersihkan proses zombie.
  - `<stdio.h>` dan `<stdlib.h>`: Untuk operasi standar input/output dan manajemen memori.

## Pengujian
- **Kecepatan Koneksi**: Koneksi ke server memiliki waktu respons rata-rata di bawah 10 ms dalam lingkungan lokal.
- **Kemampuan Multi-Client**: Server berhasil menangani hingga 30 klien secara bersamaan tanpa penurunan performa yang signifikan.
- **Stabilitas**: Server tetap stabil bahkan ketika klien keluar atau koneksi terputus secara tiba-tiba.

## Cara Menggunakan
### 1. Menjalankan Server
1. Pastikan Anda memiliki compiler GCC di sistem Anda.
2. Kompilasi program server:
   ```bash
   gcc -o server chatBroadcast.c
3.  Kompilasi program client:
   ```bash
   gcc -o client clientChat.c
