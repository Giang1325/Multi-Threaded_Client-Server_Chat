#include "client_handler.h"
#include "io.h"
#include "registry.h"
#include "broadcast.h"
#include "file_transfer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

void *handle_client(void *arg){
	char in[BUFFER_SZ];     /* dong vua nhan duoc tu client */
	char out[OUT_SZ];       /* chuoi se phat di, lon hon in vi con ghep them ten */
	int leave_flag = 0;
	int n;

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
  pthread_detach(pthread_self());

	return NULL;
}