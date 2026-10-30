#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

/* Giao thuc: xem muc "Giao thuc" trong README.md */
#define SYS_PREFIX "*** "       /* moi tin do Server sinh ra deu mo dau bang day nay */
#define NAME_MIN 2
#define NAME_MAX 31             /* name[32] = 31 ky tu + '\0' */
#define OUT_SZ 4096             /* du cho "ten: noi dung" va cho ca danh sach /list */

/* Ma tra ve cua recv_line (so khong am = so ky tu doc duoc) */
#define LINE_ERR    -1
#define LINE_LONG   -2
#define LINE_CLOSED -3

static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

/* Gui mot chuoi cho dung mot client. Khong lay khoa. */
void send_to(int fd, const char *s){
	if(write(fd, s, strlen(s)) < 0){
		perror("ERROR: write to descriptor failed");
	}
}

/* Doc mot dong tu socket, doc den khi gap '\n'.
   Ky tu '\n' KHONG duoc luu vao buf; buf luon ket thuc bang '\0'.
   Khong lay khoa.
   Tra ve: >= 0  so ky tu doc duoc (0 = dong rong, chi bam Enter)
           LINE_ERR    loi recv
           LINE_LONG   dong dai qua size-1 ky tu
           LINE_CLOSED dau kia da dong ket noi */
int recv_line(int fd, char *buf, size_t size){
	size_t n = 0;

	while(n + 1 < size){
		char c;
		ssize_t r = recv(fd, &c, 1, 0);

		if(r == 0){
			return LINE_CLOSED;
		}
		if(r < 0){
			return LINE_ERR;
		}
		if(c == '\n'){
			buf[n] = '\0';
			return (int)n;
		}
		if(c != '\r'){          /* bo '\r' de client tren Windows cung chay */
			buf[n++] = c;
		}
	}

	buf[size - 1] = '\0';
	return LINE_LONG;
}

/* Ten hop le: dai NAME_MIN..NAME_MAX ky tu, khong chua dau cach.
   Khong lay khoa. Tra ve 1 neu hop le, 0 neu khong. */
int name_is_valid(const char *name){
	size_t len = strlen(name);

	if(len < NAME_MIN || len > NAME_MAX){
		return 0;
	}
	for(size_t i = 0; i < len; ++i){
		if(isspace((unsigned char)name[i])){
			return 0;
		}
	}
	return 1;
}

/* Them client vao danh sach, chi khi ten chua ai dung.
   LAY KHOA clients_mutex.
   Thay cho queue_add cua code mau. Viec kiem tra ten trung va viec ghi vao
   mang nam trong DUNG MOT lan giu khoa: neu tach thanh hai lan lock thi
   hai nguoi cung ten vao dong thoi van cung lot qua duoc.
   Tra ve:  0  them duoc
           -1  da co nguoi dung ten nay
           -2  phong da day */
int client_add_if_name_free(client_t *cl){
	int result;
	int slot = -1;

	pthread_mutex_lock(&clients_mutex);

	result = -2;
	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(strcmp(clients[i]->name, cl->name) == 0){
				result = -1;
				break;
			}
		} else if(slot < 0){
			slot = i;      /* ghi nho o trong dau tien, van duyet het de tim ten trung */
		}
	}

	if(result != -1 && slot >= 0){
		clients[slot] = cl;
		result = 0;
	}

	pthread_mutex_unlock(&clients_mutex);
	return result;
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Ghep danh sach nguoi dang online thanh DUNG MOT dong:
       *** Dang online (3): Giang, Quan, Nam
   LAY KHOA clients_mutex. Chi ghep chuoi vao buf, KHONG gui di; nho vay
   luc write ra mang thi khoa da duoc nha, khong giu khoa khi vao/ra.
   Thu tu la thu tu o trong mang, khong phai thu tu vao phong. */
void list_online(char *buf, size_t size){
	int n = 0;
	int first = 1;
	size_t len;

	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			++n;
		}
	}

	len = (size_t)snprintf(buf, size, "%sDang online (%d):", SYS_PREFIX, n);
	if(len >= size){
		len = size - 1;
	}

	for(int i=0; i < MAX_CLIENTS && len + 1 < size; ++i){
		if(!clients[i]){
			continue;
		}
		int w = snprintf(buf + len, size - len, "%s %s",
		                 first ? "" : ",", clients[i]->name);
		if(w < 0 || (size_t)w >= size - len){
			break;      /* het cho trong buf, dung lai cho an toan */
		}
		len += (size_t)w;
		first = 0;
	}

	pthread_mutex_unlock(&clients_mutex);

	if(len + 1 < size){
		buf[len++] = '\n';
		buf[len] = '\0';
	}
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				/* continue chu khong break: mot client loi khong duoc lam
				   nhung nguoi xep sau trong mang mat tin */
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					continue;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char in[BUFFER_SZ];     /* dong vua nhan duoc tu client */
	char out[OUT_SZ];       /* chuoi se phat di, lon hon in vi con ghep them ten */
	int leave_flag = 0;
	int n;

	cli_count++;
	client_t *cli = (client_t *)arg;

	/* Doc ten theo dong, theo giao thuc trong README: Client gui "<ten>\n".
	   Thay cho recv(...,32,...) cua code mau: nho doc den '\n' nen ten
	   khong con dinh '\n' va khong con doc tran ra ngoai bo dem. */
	n = recv_line(cli->sockfd, cli->name, sizeof(cli->name));

	if(n == LINE_LONG || (n >= 0 && !name_is_valid(cli->name))){
		send_to(cli->sockfd,
			SYS_PREFIX "Ten khong hop le. Ten dai 2-31 ky tu, khong chua dau cach.\n");
		leave_flag = 1;
	} else if(n < 0){
		leave_flag = 1;         /* dut ket noi hoac loi truoc khi kip gui ten */
	} else {
		int added = client_add_if_name_free(cli);

		if(added == -1){
			snprintf(out, sizeof(out),
				SYS_PREFIX "Ten \"%s\" da co nguoi dung. Vui long chon ten khac.\n",
				cli->name);
			send_to(cli->sockfd, out);
			leave_flag = 1;
		} else if(added < 0){
			send_to(cli->sockfd, SYS_PREFIX "Phong da day, vui long thu lai sau.\n");
			leave_flag = 1;
		} else {
			snprintf(out, sizeof(out),
				SYS_PREFIX "%s da vao phong\n", cli->name);
			printf("%s", out);
			send_message(out, cli->uid);
		}
	}

	while(1){
		if (leave_flag) {
			break;
		}

		n = recv_line(cli->sockfd, in, sizeof(in));

		if(n == LINE_CLOSED || n == LINE_ERR){
			/* client dong binh thuong hoac dut dot ngot: ca hai xu ly nhu nhau */
			snprintf(out, sizeof(out),
				SYS_PREFIX "%s da roi phong\n", cli->name);
			printf("%s", out);
			send_message(out, cli->uid);
			leave_flag = 1;
		} else if(n == LINE_LONG){
			send_to(cli->sockfd, SYS_PREFIX "Tin nhan qua dai, da bo qua.\n");
		} else if(n == 0){
			/* chi bam Enter, khong phat gi */
		} else if(strcmp(in, "/list") == 0){
			list_online(out, sizeof(out));
			send_to(cli->sockfd, out);      /* chi gui cho nguoi go lenh */
		} else if(strncmp(in, SYS_PREFIX, strlen(SYS_PREFIX)) == 0){
			/* Loc tin mao danh tin he thong (README: "se bi Server loc bo") */
			printf("Da loc tin gia tin he thong tu %s: %s\n", cli->name, in);
		} else {
			/* SERVER gan ten, khong phai client, de khong ai mao danh nguoi khac */
			snprintf(out, sizeof(out), "%s: %s\n", cli->name, in);
			printf("%s", out);
			send_message(out, cli->uid);
		}
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
  queue_remove(cli->uid);
  free(cli);
  cli_count--;
  pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
  struct sockaddr_in serv_addr;
  struct sockaddr_in cli_addr;
  pthread_t tid;

  /* Socket settings */
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port = htons(port);

  /* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  }

  /* Listen */
  if (listen(listenfd, 10) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Khong con chot chan o day: chot cu doc cli_count NGOAI khoa trong
		   khi clients[] lai duoc clients_mutex bao ve, va dung == nen neu
		   lo vuot qua mot lan la mat tac dung mai mai. Chot that nam trong
		   client_add_if_name_free (dem o trong trong luc giu khoa), tra ve
		   -2 khi day; handle_client se gui cau "Phong da day" cho client. */

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		cli->name[0] = '\0';

		/* Chua them vao danh sach o day: phai doc ten truoc da, roi
		   handle_client moi goi client_add_if_name_free de chan trung ten. */
		pthread_create(&tid, NULL, &handle_client, (void*)cli);
	}

	return EXIT_SUCCESS;
}
