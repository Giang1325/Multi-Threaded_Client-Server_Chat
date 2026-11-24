#include "file_transfer.h"
#include "io.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>

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

	registry_broadcast(buf, (size_t)hdr_len + fsize, cli->uid);
	free(buf);
	return 0;
}