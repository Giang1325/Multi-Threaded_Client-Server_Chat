# 1 Code server.c co nhung ham nao ? Chuc nang cua cac ham do la gi ?
Code server.c gom 5 ham chinh
- ham main(): nhan ket noi va sinh luong
Dung ham accept() de nhan ket noi moi, roi dung ham pthread_create() sinh luong handle_client() cho tung client

- handle_client(): phuc vu 1 client

- send_message(): phat chuoi tin cho tat ca client tru sender (t nghi la ten ham de gay hieu nham, can nhac nen doi ten ham. Luc dau t nghi no la ham gui tin nhan tu 1 client nhung khi doc lai code client thi no dung ham send())

- queue_add() va queue_remove(): quan ly danh sach client

Ngoai ra co 3 ham phu nua la str_trim_lf(), print_client_addr(), str_overwrite_stdout()
nhung ham str_overwrite_stdout() khong duoc dung,co the tac gia code copy thua tu client.c sang.

# 2. Khai bao du lieu dung chung
Moi client duoc khai bao gom cac truong thong tin sau:

- address (struct sockaddr_in): Dia chi mang cua client
+ sin_family
+ sin_port
+ sin_addr
+ sin_zero[8]
- sockfd (int): Socket File Descriptor (Mo ta tep socket)
- uid (int): User Identifier (Ma dinh danh user)
- name (char, [32]): Ten client

B1: Khai bao 1 mang cac pointers toi cac client_t
client_t *clients[MAX_CLIENTS];
Moi 1 pointer luu tru DIA CHI cua 1 struct client_t, khong luu GIA TRI cua cac thong tin trong struct do

B2: Khai bao khoa mutex

Tom lai
- client_t: 1 struct gom 4 truong thong tin
- clients[]: 1 mang chua cac dia chi cua cac clients
- clients_mutex la khoa mutex

Giai thich them ve "mutex" cho ae: mutual exclusion (cơ chế chỉ cho một luồng vào)

# 3. Luong hoat dong
socket → bind → listen → vòng accept → sinh luồng → handle_client