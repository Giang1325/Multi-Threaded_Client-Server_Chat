#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include "common.h"
#include "io.h"
#include "registry.h"
#include "broadcast.h"
#include "file_transfer.h"
#include "client_handler.h"

static int uid = 10;

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

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR: Socket binding failed");
    return EXIT_FAILURE;
  }

  /* Listen. SOMAXCONN de he dieu hanh tu quyet
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

		int rc = pthread_create(&tid, NULL, &handle_client, (void*)cli);
		if(rc != 0){
			fprintf(stderr, "ERROR: pthread_create failed: %s\n", strerror(rc));
			close(connfd);
			free(cli);
			continue;
		}
	}

	return EXIT_SUCCESS;
}
