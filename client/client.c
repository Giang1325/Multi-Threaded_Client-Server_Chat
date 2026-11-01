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

#define LENGTH 2048

// Global variables
volatile sig_atomic_t flag = 0; // 1 = nguoi dung chu dong thoat (/quit, Ctrl+D); 0 ma mat ket noi = server dong
int sockfd = 0;
char name[32]; // them '\0' o cuoi de biet ket thuc ten

void str_overwrite_stdout() {
	printf("%s", "> ");
  	fflush(stdout); // in > ra ngay de khong nam o buffer nua
}

void str_trim_lf (char* arr, int length) { // trim \n thay bang \0
  	int i;
  	for (i = 0; i < length; i++) {
    	if (arr[i] == '\n') {
      		arr[i] = '\0';
      		break;
    	}
  	}
}

void catch_ctrl_c_and_exit(int sig) {
	(void)sig;
	const char msg[] = "\nBye\n";
	write(STDOUT_FILENO, msg, sizeof(msg) - 1); // tranh deadlock = write
	_exit(EXIT_SUCCESS);
}

void *send_msg_handler(void *arg) {
	(void)arg;
	char message[LENGTH] = {};
	char buffer[LENGTH + 32] = {};

	while (1) {
		str_overwrite_stdout();
		if (fgets(message, LENGTH, stdin) == NULL) { // Ctrl+D (het du lieu nhap) hoac loi doc
			break;
		}
		str_trim_lf(message, LENGTH);

		if (strcmp(message, "/quit") == 0) {
			break;
		}
		else {
			sprintf(buffer, "%s: %s\n", name, message);
			if (send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL) < 0) { // MSG_NOSIGNAL: server da dong thi send tra ve -1 thay vi bi SIGPIPE giet
				break;
			}
		}

		bzero(message, LENGTH);
		bzero(buffer, LENGTH + 32);
	}
	flag = 1;
	shutdown(sockfd, SHUT_RDWR); // dong ket noi 2 chieu, recv o luong nhan tra ve 0
	return NULL;
}

// moi tin hien len man hinh deu di qua ham nay -> Nam them gio vao day.
void print_msg(const char *line) {
	if (strncmp(line, "*** ", 4) == 0) { // tin he thong ?
		printf("\033[33m%s\033[0m\n", line); // in mau
	}
	else {
		printf("%s\n", line);
	}
	str_overwrite_stdout();
}

void *recv_msg_handler(void *arg) {
	(void)arg;
	char buf[LENGTH];
	size_t len = 0;   // so byte dang co trong buf

	while (1) {
		if (len == sizeof(buf) - 1) {
			buf[len] = '\0';
			print_msg(buf);
			len = 0;
		}

		ssize_t n = recv(sockfd, buf + len, sizeof(buf) - 1 - len, 0); // ghi noi tiep sau phan du, chua 1 byte cho '\0'
		if (n <= 0) { // 0: server dong ket noi (hoac da shutdown), -1: loi ket noi
			break;
		}
		len += n;

		size_t start = 0;
		for (size_t i = 0; i < len; i++) {
			if (buf[i] == '\n') {
				buf[i] = '\0';
				print_msg(buf + start);
				start = i + 1;
			}
		}

		memmove(buf, buf + start, len - start); // phan du doi ve dau buf
		len -= start;
	}

	if (len > 0) {
		buf[len] = '\0';
		print_msg(buf);
	}
	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]); // doi port tu chuoi thanh so

	signal(SIGINT, catch_ctrl_c_and_exit); // Ctrl C thì lich su hon

	printf("Nhap ten: ");
  	fgets(name, 32, stdin);
  	str_trim_lf(name, strlen(name));

	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Ten phai nho hon 30 va lon hon 2 ky tu.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr; // dia chi server

	/* Socket settings */ // thong tin server
	sockfd = socket(AF_INET, SOCK_STREAM, 0); //TCP
  	server_addr.sin_family = AF_INET;
  	server_addr.sin_addr.s_addr = inet_addr(ip);
  	server_addr.sin_port = htons(port);

  	// Connect to Server
  	int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  	if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
  	if(pthread_create(&send_msg_thread, NULL, send_msg_handler, NULL) != 0){ // giao viec cho ham
		printf("ERROR: pthread\n");
    	return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  	if(pthread_create(&recv_msg_thread, NULL, recv_msg_handler, NULL) != 0){ // giao viec cho ham
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	pthread_join(recv_msg_thread, NULL);

	if (!flag) {
		printf("\nMat ket noi voi server.\n");
	}
	printf("Bye\n");
	fflush(stdout);
	close(sockfd);
	_exit(EXIT_SUCCESS);
}