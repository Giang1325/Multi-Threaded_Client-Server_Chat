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

#define MAX_CLIENTS 300
#define BUFFER_SZ 2048

/* Giao thuc: xem muc "Giao thuc" trong README.md */
#define SYS_PREFIX "*** "       /* moi tin do Server sinh ra deu mo dau bang day nay */
#define NAME_MIN 2
#define NAME_MAX 31             /* name[32] = 31 ky tu + '\0' */
#define OUT_SZ (MAX_CLIENTS * (NAME_MAX + 2))  /* du cho "ten: noi dung" va cho ca danh sach /list, voi MAX_CLIENTS = 300, NAME_MAX = 31 thi OUT_SZ = 9900 byte ~ 9.7 KB */


/* Ma tra ve cua recv_line (so khong am = so ky tu doc duoc) */
#define LINE_ERR    -1
#define LINE_LONG   -2
#define LINE_CLOSED -3

/* Giao thuc gui file: /file <ten_file> <so_byte>\n roi du lieu nhi phan */
#define MAX_FILE_SZ   (1024 * 1024)   /* 1 MB, theo README */
#define FILE_NAME_MAX 255
#define FILE_HDR_MAX  320             /* du cho "/file <ten:31> <file:255> <so:10>\n" */

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

/* Ghi du len byte ra fd, lap lai neu write() ghi thieu (short write).
   write() khong dam bao ghi het trong 1 lan goi; voi tin nhan vai chuc
   byte thi hiem khi lo ra, nhung voi file toi 1 MB thi rat de thieu
   vi bo dem gui cua he dieu hanh co gioi han.
   Tra ve 0 neu ghi du, -1 neu loi. */
int write_all(int fd, const void *data, size_t len){
	size_t sent = 0;

	while(sent < len){
		ssize_t w = write(fd, (const char*)data + sent, len - sent);

		if(w < 0){
			if(errno == EINTR){
				continue;
			}
			return -1;
		}
		sent += (size_t)w;
	}
	return 0;
}

/* Gui mot chuoi cho dung mot client. Khong lay khoa. */
void send_to(int fd, const char *s){
	if(write_all(fd, s, strlen(s)) < 0){
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

	/* buf da day nhung chua toi '\n': doc bo (vut di) phan con lai cua
	   dong nay. Neu khong, phan con lai van nam trong socket, va lan
	   recv_line tiep theo se vo tinh doc trung no, coi la mot dong moi.
	   (phat hien boi Giang khi review: tin qua dai, phan duoi van lot
	   ra ngoai thanh mot tin rieng gui cho nguoi khac) */
	char c;
	ssize_t r;
	while((r = recv(fd, &c, 1, 0)) > 0 && c != '\n'){
		/* chi doc va vut di, khong luu vao dau ca */
	}
	if(r == 0){
		return LINE_CLOSED;
	}
	if(r < 0){
		return LINE_ERR;
	}
	return LINE_LONG;
}

/* Doc dung n byte vao buf, khong dung lai o '\n' (dung cho du lieu nhi phan
   nhu file: co the chua bat ky byte nao, ke ca '\0' va '\n'). Khac
   recv_line o cho khong co khai niem "mot dong". Khong lay khoa.
   Tra ve: 0  doc du n byte
           LINE_ERR     loi recv
           LINE_CLOSED  dau kia dong ket noi giua chung */
int recv_exact(int fd, void *buf, size_t n){
	size_t got = 0;

	while(got < n){
		ssize_t r = recv(fd, (char*)buf + got, n - got, 0);

		if(r == 0){
			return LINE_CLOSED;
		}
		if(r < 0){
			return LINE_ERR;
		}
		got += (size_t)r;
	}
	return 0;
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

/* Ghep danh sach nguoi dang online thanh 1 dong:
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

/* Phat du lieu nhi phan cho tat ca client tru nguoi gui.
   LAY KHOA clients_mutex va giu khoa SUOT qua trinh ghi — quan trong voi
   /file: header va noi dung file nam trong CUNG MOT buffer, ghi trong MOT
   lan giu khoa duy nhat, de tin cua luong khac khong the xen vao giua
   (neu goi rieng cho header va payload, giua hai lan do mot luong khac
   co the chen mot tin nhan vao, pha vo khung giao thuc cua nguoi nhan). */
void send_data(const void *data, size_t len, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){
				/* continue chu khong break: mot client loi khong duoc lam
				   nhung nguoi xep sau trong mang mat tin */
				if(write_all(clients[i]->sockfd, data, len) < 0){
					perror("ERROR: write to descriptor failed");
					continue;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	send_data(s, strlen(s), uid);
}

/* Doc va vut bo dung n byte tu socket (dung khi tu choi nhan file nhung
   van phai giu dung khung giao thuc, khong de phan con lai lam lech
   lan doc tiep theo). Khong lay khoa. */
void drain_bytes(int fd, unsigned long long n){
	char trash[4096];

	while(n > 0){
		size_t chunk = n < sizeof(trash) ? (size_t)n : sizeof(trash);
		ssize_t r = recv(fd, trash, chunk, 0);

		if(r <= 0){
			break;      /* dut giua chung: vong lap chinh cua handle_client se bat */
		}
		n -= (unsigned long long)r;
	}
}

/* Nhan va chuyen tiep file: dong dau la "/file <ten_file> <so_byte>",
   roi dung so_byte byte du lieu nhi phan ngay sau do (xem muc Giao thuc
   trong README.md). Goi tu nhanh /file trong vong lap chinh cua
   handle_client.
   Tra ve 1 neu can dong ket noi (cu phap /file sai, khong the dam bao
   dong bo lai duoc voi socket), 0 neu van tiep tuc chat binh thuong. */
int relay_file(client_t *cli, const char *header_line){
	char fname[FILE_NAME_MAX + 1];
	unsigned long long fsize_raw;

	/* Gioi han 10 chu so (~9999999999): neu khong, so qua lon se tran
	   kieu unsigned long long, sscanf tu dong kep ve ULLONG_MAX ma
	   KHONG bao loi, khien drain_bytes cho doc gan vo han byte. */
	if(sscanf(header_line, "/file %255s %10llu", fname, &fsize_raw) != 2){
		/* Khong parse duoc nghia la khong biet chinh xac con bao nhieu
		   byte trong socket -> khong the doc bo de dong bo lai. Dong
		   luon ket noi thay vi tiep tuc doc sai khung. */
		send_to(cli->sockfd, SYS_PREFIX "Cu phap /file sai. Dong ket noi.\n");
		return 1;
	}
    if(fsize_raw == 0){
		send_to(cli->sockfd, SYS_PREFIX "File rong, khong gui.\n");
		return 0;
	}
	if(fsize_raw > MAX_FILE_SZ){
		char msg[64];

		drain_bytes(cli->sockfd, fsize_raw);
		snprintf(msg, sizeof(msg), SYS_PREFIX "File qua lon, toi da %d byte.\n", MAX_FILE_SZ);
		send_to(cli->sockfd, msg);
		return 0;
	}

	size_t fsize = (size_t)fsize_raw;
	char *buf = malloc(FILE_HDR_MAX + fsize);

	if(!buf){
		drain_bytes(cli->sockfd, fsize_raw);
		send_to(cli->sockfd, SYS_PREFIX "Server het bo nho, khong nhan duoc file.\n");
		return 0;
	}

	int hdr_len = snprintf(buf, FILE_HDR_MAX, "/file %s %s %zu\n",
	                        cli->name, fname, fsize);

	if(hdr_len < 0 || (size_t)hdr_len >= FILE_HDR_MAX){
		/* khong nen xay ra voi gioi han NAME_MAX/FILE_NAME_MAX hien tai,
		   nhung kiem tra cho chac truoc khi ghi tiep vao buf */
		free(buf);
		drain_bytes(cli->sockfd, fsize_raw);
		send_to(cli->sockfd, SYS_PREFIX "Loi noi bo, khong nhan duoc file.\n");
		return 0;
	}

	if(recv_exact(cli->sockfd, buf + hdr_len, fsize) != 0){
		free(buf);
		return 0;   /* dut giua chung: vong lap chinh se bat o lan recv_line ke tiep */
	}

	send_data(buf, (size_t)hdr_len + fsize, cli->uid);
	free(buf);
	return 0;
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
		} else if(strncmp(in, "/file ", 6) == 0){
			if(relay_file(cli, in)){
				leave_flag = 1;
			}
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

	/* Chi SO_REUSEADDR: cho bind lai cong ngay sau khi server cu vua tat,
	   khong phai cho nhieu server cung nghe chung mot cong.
	   (phat hien boi Giang: (SO_REUSEPORT | SO_REUSEADDR) la OR hai TEN
	   hang so, SO_REUSEADDR=2 SO_REUSEPORT=15, 2|15=15 nen chi
	   SO_REUSEPORT duoc bat - no cho phep nhieu tien trinh dung chung
	   1 cong, nen 2 server chay cung luc van khoi dong duoc, client bi
	   chia ngau nhien sang 2 ben) */
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  }

  /* Listen. SOMAXCONN thay cho so 10 tu doan: de he dieu hanh tu quyet
     dinh muc hang doi hop ly, thay vi tu doan mot con so co dinh. */
  if (listen(listenfd, SOMAXCONN) < 0) {
    perror("ERROR: Socket listening failed");
    return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
		if(connfd < 0){
			perror("ERROR: accept failed");
			continue;
		}

		/* Khong con chot chan o day: chot cu doc cli_count NGOAI khoa trong
		   khi clients[] lai duoc clients_mutex bao ve, va dung == nen neu
		   lo vuot qua mot lan la mat tac dung mai mai. Chot that nam trong
		   client_add_if_name_free (dem o trong trong luc giu khoa), tra ve
		   -2 khi day; handle_client se gui cau "Phong da day" cho client. */

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		if(!cli){
			perror("ERROR: malloc client_t failed");
			close(connfd);
			continue;
		}
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		cli->name[0] = '\0';

		/* Chua them vao danh sach o day: phai doc ten truoc da, roi
		   handle_client moi goi client_add_if_name_free de chan trung ten. */
		int rc = pthread_create(&tid, NULL, &handle_client, (void*)cli);
		if(rc != 0){
			/* pthread_create tra ve ma loi truc tiep, khong qua errno,
			   nen dung strerror(rc) chu khong phai perror. */
			fprintf(stderr, "ERROR: pthread_create failed: %s\n", strerror(rc));
			close(connfd);
			free(cli);
			continue;
		}
	}

	return EXIT_SUCCESS;
}
