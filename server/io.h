#pragma once
#include <stddef.h>

int write_all(int fd, const void *data, size_t size);
void send_to(int fd, const char *s);
int recv_line(int fd, char *buf, size_t size);
int recv_exact(int fd, void *buf, size_t n);
void drain_bytes(int fd, unsigned long long n);