# Makefile khung - Multi-Threaded Client/Server Chat
# Moi truong: Linux hoac WSL (Ubuntu). Code dung socket POSIX va Pthreads nen KHONG build bang MinGW.
#
#   make         -> build server va client
#   make test    -> chay kiem thu
#   make clean   -> xoa file build
#
# File chay dat trong bin/ (bin/server, bin/client) vi thu muc goc da co
# thu muc server/ va client/, khong the tao them file trung ten.
# Tuan 2: da co target that cho server va client. Tuan 3: them target test;
# tuan 4: make unsafe.

CC      = gcc
# _DEFAULT_SOURCE: can cho SO_REUSEPORT va bzero (hai thu nay la mo rong BSD,
# khong co trong POSIX.1-2008 nen -D_POSIX_C_SOURCE mot minh se bao loi).
CFLAGS  = -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Wall -Wextra -g -pthread
LDFLAGS = -pthread
BIN     = bin

.PHONY: all asan test clean

all: $(BIN)/server $(BIN)/client

$(BIN)/server: server/server.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN)/client: client/client.c | $(BIN)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BIN):
	mkdir -p $(BIN)

# Build kem AddressSanitizer (lay theo Makefile cua repo mau)
asan: CFLAGS += -g3 -fsanitize=address
asan: clean all

test:
	@echo "Chua co test"

clean:
	rm -rf $(BIN) *.o server/*.o client/*.o common/*.o tests/*.o
