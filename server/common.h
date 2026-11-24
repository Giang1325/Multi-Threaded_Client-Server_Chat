#pragma once
#include <netinet/in.h>

#define MAX_CLIENTS 300
#define BUFFER_SZ 2048

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

typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;