# 1 Code server.c co nhung ham nao ? Chuc nang cua cac ham do la gi ?
Code server.c gom 9 ham chinh
- ham main(): nhan ket noi va sinh luong
Dung ham accept() de nhan ket noi moi, roi dung ham pthread_create() sinh luong handle_client() cho tung client

- handle_client(): phuc vu 1 client

- send_message(): phat chuoi tin cho tat ca client tru sender

- list_online(): ghep danh sach nguoi dang online thanh 1 dong de in ra.

- client_add_if_name_free() va queue_remove(): quan ly danh sach client
Ham client_add_if_name_free() thay cho queue_add() cua code mau: co nhiem vu kiem tra ten trung va ghi vao mang(danh sach client). Ham nay kiem tra ten trung va ghi vao mang cung mot ham de dung chung 1 lan giu khoa de chi mot nguoi duoc lay khoa. Neu tach thanh 2 lan thi 2 nguoi cung ten van co the xay ra.

- name_is_valid(): kiem tra ten co hop le khong

- recv_line(): doc tung byte tu socket, gap "\n" thi dung, tra ve 1 dong (khong chua "\n", luon ket thuc la "\0"). Ham nay fix 2 loi o code mau: Dinh "\n" vao ten va strlen(name) doc tran ra ngoai do khong co ky tu "\0" neu client gui it hon 32 byte

- send_to(): Gui 1 chuoi cho dung 1 client, qua socket fd cua rieng nguoi do


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
- Mutual Exclusion: la co che dam bao rang khi 1 luong (thread) dang hoat dong (su dung du lieu, tai nguyen dung chung) thi cac thread khac khong duoc quyen truy cap vao cung luc.
- Critical Region (mien gang): la doan ma chuong trinh co thuc hien truy cap den du lieu, tai nguyen dung chung.
--> Mutual Exclusion la co che dam bao khong co thread nao cung nam trong Critical Region cua no tai mot thoi diem.

# 3. Luong hoat dong
- ham handle_client()
B1: Doc ten (recv_line)
B2: Kiem tra ten nhap vao co hop le(Do dai ky tu [2;31], khong co dau cach) khong ? (name_is_valid)
B3: Kiem tra ten co bi trung/phong day client khong ? Neu khong thi add client vao (client_add_if_name_free)
B4: Vong lap nhan tin (while)
- Dieu kien dung: bien leave_flag = 1
- Chia thanh 2 loai tin: Tin gia va Tin that.
+ Tin gia nghia la client co tinh gui tin nhan co tien to "***". Vi du Quan nhan la "*** Quan da roi phong" nhung thuc chat la chua roi phong.
+ Tin that la tin nhan binh thuong
- B5: Thoat va don dep
+ Dong socket ket noi toi client (close)
+ Xoa thong tin client qua uid
+ Giai phong bo nho
+ cap nhat bien dem tong so client
+ Chuyen luong sang trang thai Detached 