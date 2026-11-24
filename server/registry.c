#include "registry.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <pthread.h>
#include "io.h"

static client_t *clients[MAX_CLIENTS];
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void registry_broadcast(const void *data, size_t len, int exclude_uid){
    pthread_mutex_lock(&clients_mutex);

    for(int i=0; i < MAX_CLIENTS; ++i){
        if(clients[i]){
            if(clients[i]->uid != exclude_uid){
                if(write_all(clients[i]->sockfd, data, len) < 0){
                    perror("ERROR: write to descriptor failed");
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}