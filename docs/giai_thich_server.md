# 1. Code server.c có những hàm nào? Chức năng của các hàm đó là gì?

Code `server.c` gồm 9 hàm chính:

- **`main()`**: Nhận kết nối và sinh luồng.  
  Dùng hàm `accept()` để nhận kết nối mới, rồi dùng hàm `pthread_create()` sinh luồng `handle_client()` cho từng client.

- **`handle_client()`**: Phục vụ 1 client.

- **`send_message()`**: Phát chuỗi tin cho tất cả client trừ người gửi (sender).

- **`list_online()`**: Ghép danh sách người đang online thành 1 dòng để in ra.

- **`client_add_if_name_free()` và `queue_remove()`**: Quản lý danh sách client.  
  Hàm `client_add_if_name_free()` thay cho `queue_add()` của code mẫu: có nhiệm vụ kiểm tra tên trùng và ghi vào mảng (danh sách client). Hàm này kiểm tra tên trùng và ghi vào mảng cùng một hàm để dùng chung 1 lần giữ khóa, nhằm đảm bảo chỉ một người được lấy khóa tại một thời điểm. Nếu tách thành 2 lần thì hai người trùng tên vẫn có thể xảy ra.

- **`name_is_valid()`**: Kiểm tra tên có hợp lệ hay không.

- **`recv_line()`**: Đọc từng byte từ socket, gặp `"\n"` thì dừng, trả về 1 dòng (không chứa `"\n"`, luôn kết thúc là `"\0"`). Hàm này khắc phục 2 lỗi ở code mẫu:  
  1. Đính `"\n"` vào tên.  
  2. `strlen(name)` đọc tràn ra ngoài do không có ký tự `"\0"` nếu client gửi ít hơn 32 byte.

- **`send_to()`**: Gửi 1 chuỗi cho đúng 1 client thông qua socket fd của riêng người đó.

---

# 2. Khai báo dữ liệu dùng chung

Mỗi client được khai báo gồm các trường thông tin sau:

- **`address`** (`struct sockaddr_in`): Địa chỉ mạng của client  
  - `sin_family`  
  - `sin_port`  
  - `sin_addr`  
  - `sin_zero[8]`  
- **`sockfd`** (`int`): Socket File Descriptor (Mô tả tệp socket)  
- **`uid`** (`int`): User Identifier (Mã định danh người dùng)  
- **`name`** (`char[32]`): Tên client  

- **Bước 1**: Khai báo 1 mảng các con trỏ tới các `client_t`:  
  `client_t *clients[MAX_CLIENTS];`  
  Mỗi con trỏ lưu trữ **ĐỊA CHỈ** của 1 `struct client_t`, không lưu **GIÁ TRỊ** của các thông tin trong struct đó.

- **Bước 2**: Khai báo khóa mutex.

### Tóm lại:
- **`client_t`**: 1 struct gồm 4 trường thông tin.
- **`clients[]`**: 1 mảng chứa các địa chỉ của các client.
- **`clients_mutex`**: Là khóa mutex.

### Giải thích thêm về "mutex" (Mutual Exclusion - Cơ chế chỉ cho một luồng vào):
- **Mutual Exclusion**: Là cơ chế đảm bảo rằng khi 1 luồng (thread) đang hoạt động (sử dụng dữ liệu, tài nguyên dùng chung) thì các thread khác không được quyền truy cập vào cùng lúc.
- **Critical Region (Miền găng)**: Là đoạn mã chương trình có thực hiện truy cập đến dữ liệu, tài nguyên dùng chung.
$$\rightarrow$$ **Mutual Exclusion** là cơ chế đảm bảo không có thread nào cùng nằm trong Miền găng của nó tại một thời điểm.

---

# 3. Luồng hoạt động

- **Hàm `handle_client()`**:
  - **Bước 1**: Đọc tên (`recv_line`).
  - **Bước 2**: Kiểm tra tên nhập vào có hợp lệ (Độ dài ký tự trong khoảng $[2; 31]$, không có dấu cách) hay không (`name_is_valid`).
  - **Bước 3**: Kiểm tra tên có bị trùng hoặc phòng đầy client không? Nếu không thì thêm client vào (`client_add_if_name_free`).
  - **Bước 4**: Vòng lặp nhận tin (`while`):
    - **Điều kiện dừng**: Biến `leave_flag = 1`.
    - **Chia thành 2 loại tin**: Tin giả và Tin thật.
      - *Tin giả*: Client cố tình gửi tin nhắn có tiền tố `***`. Ví dụ Quân nhắn là `*** Quan da roi phong` nhưng thực chất là chưa rời phòng.
      - *Tin thật*: Tin nhắn bình thường.
  - **Bước 5**: Thoát và dọn dẹp:
    - Đóng socket kết nối tới client (`close`).
    - Xóa thông tin client qua `uid`.
    - Giải phóng bộ nhớ.
    - Cập nhật biến đếm tổng số client.
    - Chuyển luồng sang trạng thái Detached (`pthread_detach(pthread_self())`).