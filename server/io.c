#include "io.h"
#include "common.h"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

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