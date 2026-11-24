#pragma once
#include "common.h"
#include <stddef.h>

int name_is_valid(const char *name);
int client_add_if_name_free(client_t *cl);
void queue_remove(int uid);
void list_online(char *buf, size_t size);
void registry_broadcast(const void *data, size_t len, int exclude_uid);
