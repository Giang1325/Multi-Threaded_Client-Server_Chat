# Makefile khung - Multi-Threaded Client/Server Chat
# Moi truong: Linux hoac WSL (Ubuntu). Code dung socket POSIX va Pthreads nen KHONG build bang MinGW.
#
#   make         -> build server va client
#   make test    -> chay kiem thu
#   make clean   -> xoa file build
#
# File chay dat trong bin/ (bin/server, bin/client) vi thu muc goc da co
# thu muc server/ va client/, khong the tao them file trung ten.
# Tuan 3: Quan bo sung cac target that (server, client, test); tuan 4: make unsafe.

CC      = gcc
CFLAGS  = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -g -pthread
LDFLAGS = -pthread
BIN     = bin

.PHONY: all test clean

all:
	@echo "Chua co ma nguon de build"

test:
	@echo "Chua co test"

clean:
	rm -rf $(BIN) *.o server/*.o client/*.o common/*.o tests/*.o
