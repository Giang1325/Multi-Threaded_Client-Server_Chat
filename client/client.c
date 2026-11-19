#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>

#define LENGTH 2048
#define FILE_MAX 1048576
#define COLORED "\033[33m"
#define RESET  "\033[0m"

// Global variables
volatile sig_atomic_t flag = 0; // 1 = nguoi dung chu dong thoat (/quit, Ctrl+D); 0 ma mat ket noi = server dong
int sockfd = 0;
char name[32]; // them '\0' o cuoi de biet ket thuc ten

void str_overwrite_stdout() {
    printf("%s", "> ");
    fflush(stdout); // in > ra ngay de khong nam o buffer nua
}

int read_line(char *buf, int size) { // 0; -1
    if (fgets(buf, size, stdin) == NULL) {
        return -1;
    }
    char *nl = strchr(buf, '\n'); // tim \n
    if (nl != NULL) {
        *nl = '\0';
    }
    else {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {} // kiem tra tung ki tu tim \n hoac EOF
    }
    return 0;
}

int name_is_valid(const char *s) { // 1; 0
    size_t len = strlen(s);
    if (len < 2 || len > 31) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (isspace((unsigned char)s[i])) {
            return 0;
        }
    }
    return 1;
}

int parse_port(const char *s) {
    char *end;
    errno = 0; // strtol khong tu reset
    long v = strtol(s, &end, 10); // return so doc duoc
    if (errno != 0 || end == s || *end != '\0' || v < 1 || v > 65535) {
        return -1;
    }
    return (int)v;
}

void catch_ctrl_c_and_exit(int sig) {
    (void)sig; // co y khong dung
    const char msg[] = "\nBye\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1); // tranh deadlock = write
    _exit(EXIT_SUCCESS);
}

int send_all(int fd, const void *buf, size_t len) {
    const char *p = buf; // chay tren tung byte can gui
    while (len > 0) {
        ssize_t n = send(fd, p, len, MSG_NOSIGNAL); // MSG_NOSIGNAL: server da dong thi send tra ve -1 thay vi bi SIGPIPE giet
        if (n < 0) {
            if (errno == EINTR) { // bi tin hieu cat ngang truoc khi gui duoc gi: gui lai
                continue;
            }
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/'); // '/' cuoi cung
    return (slash != NULL) ? (slash + 1) : path;
}

int name_is_safe(const char *name) {
    size_t len = strlen(name);
    if (len == 0 || len > 255 || name[0] == '.') { // sai thu muc
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == '/' || isspace(c) || c < 32 || c == 127) { // ky tu dieu khien
            return 0;
        }
    }
    return 1;
}

FILE *open_file_to_send(const char *path, off_t *size) {
    struct stat st;
    if (stat(path, &st) < 0) { // hoi nhan file in4: 0; -1
        printf(COLORED "*** Khong tim thay file \"%s\": %s" RESET "\n", path, strerror(errno));
        return NULL;
    }
    if (!S_ISREG(st.st_mode)) { // file thuong ?
        printf(COLORED "*** \"%s\" khong phai file thuong" RESET "\n", path);
        return NULL;
    }
    if (st.st_size == 0) {
        printf(COLORED "*** File rong, khong gui" RESET "\n");
        return NULL;
    }
    if (st.st_size > FILE_MAX) {
        printf(COLORED "*** File qua lon (%lld byte), toi da %d byte" RESET "\n", (long long)st.st_size, FILE_MAX);
        return NULL;
    }

    if (!name_is_safe(path_basename(path))) {
        printf(COLORED "*** Ten file khong hop le: toi da 255 ky tu, khong dau cach, khong bat dau bang '.'" RESET "\n");
        return NULL;
    }

    FILE *fp = fopen(path, "rb"); // read binary
    if (fp == NULL) {
        printf(COLORED "*** Khong mo duoc file \"%s\": %s" RESET "\n", path, strerror(errno));
        return NULL;
    }
    *size = st.st_size;
    return fp;
}

int send_file(int fd, FILE *fp, const char *name, off_t size) {
    char header[LENGTH];
    snprintf(header, sizeof(header), "/file %s %lld\n", name, (long long)size);
    if (send_all(fd, header, strlen(header)) < 0) {
        fclose(fp);
        return -1;
    }

    char chunk[4096]; // tach tin
    off_t remaining = size;
    while (remaining > 0) {
        size_t want = (remaining < (off_t)sizeof(chunk)) ? (size_t)remaining : sizeof(chunk);
        size_t got = fread(chunk, 1, want, fp);
        if (got == 0) {
            printf(COLORED "*** Loi doc file giua chung, ngat ket noi" RESET "\n");
            fclose(fp); // sai loi hua
            return -1;
        }
        if (send_all(fd, chunk, got) < 0) {
            fclose(fp);
            return -1;
        }
        remaining -= (off_t)got;
    }
    fclose(fp);
    return 0;
}

void *send_msg_handler(void *arg) {
    (void)arg; // co y khong dung
    char message[LENGTH];
    char buffer[LENGTH + 1];

    while (1) {
        str_overwrite_stdout();
        if (read_line(message, sizeof(message)) < 0) { // Ctrl+D
            flag = 1;
            break;
        }
        if (strcmp(message, "/quit") == 0) {
            flag = 1;
            break;
        }
        if (message[0] == '\0') {
            continue;
        }
        if (strncmp(message, "/sendfile", 9) == 0 && (message[9] == ' ' || message[9] == '\0')) {
            const char *path = message + 9;
            while (*path == ' ') {
                path++;
            }
            if (*path == '\0') {
                printf(COLORED "*** Cach gan file: /sendfile <duong dan file>" RESET "\n");
                continue;
            }
            off_t size;
            FILE *fp = open_file_to_send(path, &size);
            if (fp == NULL) {
                continue;
            }
            const char *fname = path_basename(path);
            if (send_file(sockfd, fp, fname, size) < 0) {
                break;
            }
            printf(COLORED "*** Da gui \"%s\" (%lld byte)" RESET "\n", fname, (long long)size);
            continue;
        }
        if (strncmp(message, "/file", 5) == 0 && (message[5] == ' ' || message[5] == '\0')) {
            printf(COLORED "*** Dong bat dau bang /file danh rieng cho giao thuc, hay dung /sendfile <duong dan>" RESET "\n");
            continue;
        }
        snprintf(buffer, sizeof(buffer), "%s\n", message);
        if (send_all(sockfd, buffer, strlen(buffer)) < 0) {
            break;
        }
    }
    shutdown(sockfd, SHUT_RDWR); // dong ket noi 2 chieu, recv o luong nhan tra ve 0
    return NULL;
}

void current_hhmm(char *out, size_t size) {
    time_t now = time(NULL);
    struct tm tm_now;
    if (localtime_r(&now, &tm_now) == NULL || strftime(out, size, "%H:%M", &tm_now) == 0) {
        snprintf(out, size, "--:--");
    }
}

void print_msg(const char *line) {
    char hhmm[6];
    current_hhmm(hhmm, sizeof(hhmm));
    if (strncmp(line, "*** ", 4) == 0) { // tin he thong ?
        printf("[%s] " COLORED "%s" RESET "\n", hhmm, line); // gio mau thuong, noi dung mau vang
    }
    else {
        printf("[%s] %s\n", hhmm, line);
    }
    str_overwrite_stdout();
}

typedef struct {
    FILE *out;
    off_t left;
    char sender[32];
    char name[256];
    char path[300]; // "downloads/<ten file>"
} recv_file_t;

int file_begin(recv_file_t *rf, const char *line) {
    long long size;
    if (sscanf(line, "/file %31s %255s %lld", rf->sender, rf->name, &size) != 3 || size <= 0 || size > FILE_MAX) {
        return -1;
    }
    rf->left = (off_t)size;

    if (!name_is_safe(rf->name)) {
        printf(COLORED "*** Bo qua file co ten khong an toan tu %s" RESET "\n", rf->sender);
        rf->out = NULL;
        return 0;
    }

    mkdir("downloads", 0755); // access role bat phan
    snprintf(rf->path, sizeof(rf->path), "downloads/%s", rf->name);
    rf->out = fopen(rf->path, "wb"); // "wb": ghi nhi phan, ghi de neu trung ten
    if (rf->out == NULL) {
        printf(COLORED "*** Khong luu duoc \"%s\": %s (bo qua file nay)" RESET "\n", rf->path, strerror(errno));
    }
    return 0;
}

void file_write(recv_file_t *rf, const char *data, size_t n) {
    if (rf->out != NULL && fwrite(data, 1, n, rf->out) != n) { // ghi thieu
        printf(COLORED "*** Loi ghi \"%s\": %s (bo qua phan con lai)" RESET "\n", rf->path, strerror(errno));
        fclose(rf->out);
        remove(rf->path);
        rf->out = NULL; // doc bo
    }
    rf->left -= (off_t)n;

    if (rf->left == 0 && rf->out != NULL) {
        int rc = fclose(rf->out); // 0; =1
        rf->out = NULL;
        if (rc != 0) {
            printf(COLORED "*** Loi ghi \"%s\": %s" RESET "\n", rf->path, strerror(errno));
            remove(rf->path);
            return;
        }
        char msg[LENGTH];
        snprintf(msg, sizeof(msg), "*** Nhan file \"%s\" tu %s -> %s", rf->name, rf->sender, rf->path);
        print_msg(msg);
    }
}

void file_abort(recv_file_t *rf) { // dut ket noi
    if (rf->out != NULL) {
        fclose(rf->out);
        remove(rf->path);
        rf->out = NULL;
        printf(COLORED "*** File \"%s\" chua nhan du, da xoa" RESET "\n", rf->name);
    }
    rf->left = 0;
}

void *recv_msg_handler(void *arg) {
    (void)arg;
    char buf[LENGTH];
    size_t len = 0;   // so byte dang co trong buf
    recv_file_t rf = { .out = NULL, .left = 0 };
    int broken = 0; // 1 = header fail

    while (!broken) {
        if (len == sizeof(buf) - 1) {
            buf[len] = '\0';
            print_msg(buf);
            len = 0;
        }

        ssize_t n = recv(sockfd, buf + len, sizeof(buf) - 1 - len, 0); // ghi noi tiep sau phan du, chua 1 byte cho '\0'
        // 0: khong co flag nao, lam nhu mac dinh
        if (n <= 0) { // 0: server dong ket noi (hoac da shutdown), -1: loi ket noi
            break;
        }
        len += n;

        size_t pos = 0;
        while (pos < len) {
            if (rf.left > 0) { // ? file mode
                size_t take = len - pos;
                if ((off_t)take > rf.left) {
                    take = (size_t)rf.left;
                }
                file_write(&rf, buf + pos, take);
                pos += take;
                continue;
            }

            char *nl = memchr(buf + pos, '\n', len - pos); // tim '\n'
            if (nl == NULL) {
                break;
            }
            *nl = '\0';
            char *line = buf + pos;
            pos = (size_t)(nl - buf) + 1;

            if (strncmp(line, "/file ", 6) == 0) {
                if (file_begin(&rf, line) < 0) {
                    printf(COLORED "*** Header file khong hop le, ngat ket noi" RESET "\n");
                    broken = 1;
                    break;
                }
                continue;
            }
            print_msg(line);
        }

        memmove(buf, buf + pos, len - pos);
        len -= pos;
    }

    file_abort(&rf);
    if (!broken && len > 0) {
        buf[len] = '\0';
        print_msg(buf);
    }
    return NULL;
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip = "127.0.0.1";
    int port = parse_port(argv[1]);
    if (port < 0) {
        fprintf(stderr, "Cong khong hop le: %s (phai la so tu 1 den 65535)\n", argv[1]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, catch_ctrl_c_and_exit); // lich su

    char line[LENGTH]; // strlen tra do dai that
    while (1) {
        printf("Nhap ten: ");
        fflush(stdout);
        if (read_line(line, sizeof(line)) < 0) { // Ctrl + D
            printf("\nBye\n");
            return EXIT_SUCCESS;
        }
        if (name_is_valid(line)) {
            break;
        }
        printf("Ten phai dai 2-31 ky tu va khong co dau cach. Nhap lai.\n");
    }
    strcpy(name, line); // ok

    struct sockaddr_in server_addr; // dia chi server IPv4

    /* Socket settings */ // thong tin server
    sockfd = socket(AF_INET, SOCK_STREAM, 0); //TCP; 0: free choice
    if (sockfd < 0) {
        perror("ERROR: socket");
        return EXIT_FAILURE;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    // Connect to Server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) { // ep kieu chung
        perror("ERROR: connect");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // Send name
    char name_line[sizeof(name) + 1];
    snprintf(name_line, sizeof(name_line), "%s\n", name);
    if (send_all(sockfd, name_line, strlen(name_line)) < 0) {
        perror("ERROR: send name");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHATROOM ===\n");

    pthread_t send_msg_thread;
    int rc = pthread_create(&send_msg_thread, NULL, send_msg_handler, NULL); // NULL1: stack mac dinh; NULL2: khong truyen vao arg cua ham
    if (rc != 0) {
        fprintf(stderr, "ERROR: pthread_create (gui): %s\n", strerror(rc));
        close(sockfd);
        return EXIT_FAILURE;
    }

    pthread_t recv_msg_thread;
    rc = pthread_create(&recv_msg_thread, NULL, recv_msg_handler, NULL); // 0: ok; != 0: loi
    if (rc != 0) {
        fprintf(stderr, "ERROR: pthread_create (nhan): %s\n", strerror(rc));
        fflush(stdout);
        close(sockfd);
        _exit(EXIT_FAILURE); // deadlock
    }

    pthread_join(recv_msg_thread, NULL); // NULL: khong quan tam gia tri luong nhan

    if (!flag) {
        printf("\nMat ket noi voi server.\n");
    }
    printf("Bye\n");
    fflush(stdout);
    close(sockfd);
    _exit(EXIT_SUCCESS);
}
