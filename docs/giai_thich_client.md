# Giải thích code Client (`client/client.c`)

Người viết: Nguyễn Trường Giang – Tuần 2

## 1. Nguồn

Code lấy nguyên bản từ repo [nikhilroxtomar/Chatroom-in-C](https://github.com/nikhilroxtomar/Chatroom-in-C) (file `client.c`), **chưa chỉnh sửa**. Các chỉnh sửa sẽ làm từ tuần 3.

## 2. Cách build và chạy

```bash
gcc -Wall -g3 -fsanitize=address -pthread client/client.c -o client/client
./client/client 4444        # 4444 là cổng của server
```

- `-pthread`: bắt buộc, vì chương trình dùng luồng.
- `-fsanitize=address`: bật AddressSanitizer để phát hiện lỗi bộ nhớ khi chạy (lấy theo Makefile gốc).
- Khi biên dịch có 1 cảnh báo `-Wformat-overflow` ở dòng 50 (`sprintf`), xem mục 6.

Client chỉ kết nối tới `127.0.0.1` (cùng máy với server), cổng nhập từ dòng lệnh.

## 3. Ý tưởng chính

Client cần làm **hai việc cùng lúc**:

1. Chờ người dùng gõ phím rồi gửi tin đi.
2. Chờ tin từ server rồi in ra màn hình.

Cả hai việc đều **bị chặn (blocking)**: `fgets` đứng chờ bàn phím, `recv` đứng chờ mạng. Nếu chỉ có một luồng thì khi đang chờ gõ phím sẽ không nhận được tin của người khác, và ngược lại. Vì vậy client dùng **3 luồng**:

| Luồng | Hàm | Việc làm |
|---|---|---|
| Luồng chính | `main` | Kết nối, tạo 2 luồng con, rồi chờ cờ `flag` để thoát |
| Luồng gửi | `send_msg_handler` | Đọc bàn phím → gửi lên server |
| Luồng nhận | `recv_msg_handler` | Nhận từ server → in ra màn hình |

Ba luồng dùng chung các biến toàn cục `sockfd`, `name`, `flag` (luồng trong cùng tiến trình dùng chung vùng nhớ).

## 4. Đi qua từng phần

### Biến toàn cục (dòng 12–17)

- `LENGTH 2048`: độ dài tối đa một tin nhắn.
- `flag` (kiểu `volatile sig_atomic_t`): cờ báo "cần thoát". `volatile` để trình biên dịch luôn đọc lại giá trị thật từ bộ nhớ; `sig_atomic_t` là kiểu được phép ghi an toàn trong hàm xử lý tín hiệu.
- `sockfd`: mã kết nối (file descriptor) tới server.
- `name[32]`: tên người dùng.

### Hàm phụ (dòng 19–36)

- `str_overwrite_stdout`: in dấu `> ` và `fflush` để dấu nhắc hiện ngay.
- `str_trim_lf`: thay ký tự xuống dòng `\n` (do `fgets` để lại) bằng `\0`.
- `catch_ctrl_c_and_exit`: hàm xử lý tín hiệu Ctrl+C (SIGINT), chỉ đặt `flag = 1`. Trong hàm xử lý tín hiệu chỉ nên làm việc tối thiểu như vậy.

### Luồng gửi – `send_msg_handler` (dòng 38–58)

1. In `> `, đọc một dòng bằng `fgets` (dòng 44).
2. Nếu người dùng gõ `exit` → thoát vòng lặp, gọi `catch_ctrl_c_and_exit(2)` để đặt `flag = 1`.
3. Ngược lại, ghép thành `"tên: nội dung\n"` bằng `sprintf` (dòng 50) rồi `send` lên server (dòng 51).
4. Xóa bộ đệm bằng `bzero` rồi lặp lại.

### Luồng nhận – `recv_msg_handler` (dòng 60–74)

`recv` (dòng 63) trả về:

- `> 0`: có dữ liệu → in ra, in lại dấu `> `.
- `0`: server đã đóng kết nối → thoát vòng lặp.
- `-1`: lỗi → code gốc bỏ qua, không xử lý.

### Hàm `main` (dòng 76–140)

1. Kiểm tra tham số: phải có đúng 1 tham số là cổng.
2. `signal(SIGINT, ...)` (dòng 85): đăng ký hàm xử lý Ctrl+C.
3. Nhập tên (dòng 88), kiểm tra độ dài.
4. Tạo socket TCP: `socket(AF_INET, SOCK_STREAM, 0)` (dòng 100). `AF_INET` là IPv4, `SOCK_STREAM` là TCP.
5. Điền địa chỉ server: `inet_addr` đổi chuỗi IP thành số, `htons` đổi cổng sang thứ tự byte của mạng.
6. `connect` (dòng 107) tới server; lỗi thì in `ERROR: connect`.
7. `send(sockfd, name, 32, 0)` (dòng 114): gửi tên trước tiên, **luôn đủ 32 byte**.
8. `pthread_create` hai lần (dòng 119, 125) để tạo luồng gửi và luồng nhận.
9. Vòng `while(1)` (dòng 130) liên tục kiểm tra `flag`; khi `flag = 1` thì in `Bye`, đóng socket (dòng 137) và kết thúc.

### "Giao thức" giữa client và server

Code không ghi rõ nhưng hai bên ngầm thống nhất:

- Tin đầu tiên luôn là **tên, dài đúng 32 byte** (phần thừa là ký tự rỗng). Server cũng đọc đúng 32 byte.
- Các tin sau có dạng `tên: nội dung\n`. Client tự gắn tên, server chỉ chuyển tiếp.

## 5. Kết quả chạy thử

Thử với `nc -l 4444` đóng vai server (chưa có server của nhóm):

| Thao tác | Kết quả | Đạt? |
|---|---|---|
| Kết nối, nhập tên `Giang` | `nc` hiện `Giang` | Đạt |
| Client gửi `xin chao` | `nc` hiện `Giang: xin chao` (dính liền sau tên, xem mục 6) | Đạt |
| `nc` gửi `hello` | Client hiện `hello` | Đạt |
| Tắt `nc` bằng Ctrl+C | Client **bị treo**, không thoát | Lỗi |
| Ctrl+C ở client | In `Bye` nhưng **không thoát** | Lỗi |
| Gõ `exit` sau khi đã in `Bye` | Chương trình thoát | Đạt |

*(Chèn ảnh chụp màn hình ở đây.)*

Thử với server thật: *(bổ sung sau khi có `server.c` của Quân.)*

## 6. Lỗi và hạn chế đã phát hiện

1. **Server tắt thì client treo.** Khi `recv` trả về 0, luồng nhận thoát nhưng **không đặt `flag = 1`**, nên luồng chính vẫn chờ mãi và luồng gửi vẫn đứng ở `fgets`.

2. **Ctrl+C in `Bye` nhưng không thoát.** Luồng chính kết thúc chương trình, lúc này thư viện C cần lấy **khóa của `stdin`** để dọn dẹp. Nhưng luồng gửi đang đứng ở `fgets(stdin)` và **giữ khóa đó** trong lúc chờ bàn phím. Luồng chính chờ khóa, luồng gửi chờ người gõ, nên chương trình đứng im. Gõ `exit` thì luồng gửi thoát vòng lặp, nhả khóa, chương trình mới kết thúc. Đây là ví dụ về một luồng chờ tài nguyên mà luồng khác đang giữ (liên quan Chương 2: mutex, deadlock).

3. **Chờ bận (busy waiting) ở luồng chính.** Vòng `while(1){ if(flag) ... }` kiểm tra liên tục không nghỉ, làm một nhân CPU chạy gần 100% chỉ để chờ. Nên thay bằng `pthread_join` hoặc condition variable.

4. **Tin đầu tiên dính vào tên** khi thử với `nc` (`GiangGiang: xin chao`), vì tên gửi đủ 32 byte và không có `\n`. Với server thật thì không bị, vì server đọc đúng 32 byte.

5. **Kiểm tra độ dài tên sai.** `fgets(name, 32, ...)` chỉ đọc tối đa 31 ký tự nên `strlen(name) > 32` không bao giờ đúng; thông báo lỗi lại ghi "less than 30".

6. **Cảnh báo `sprintf`.** Tên (tối đa 31) + nội dung (tối đa 2047) + `": "` + `"\n"` có thể vượt 2080 byte của `buffer`. Nên dùng `snprintf`.

7. **`recv` có thể làm đầy cả 2048 byte**, không còn chỗ cho `\0`, khiến `printf("%s")` đọc lố. Nên dùng `recv(..., LENGTH - 1, ...)`.

8. **IP cố định `127.0.0.1`**, chỉ chạy được cùng máy với server.

Các lỗi 1, 2 sẽ sửa ở tuần 3 (yêu cầu "server tắt thì client báo rồi thoát, không bị treo").

## 7. Liên hệ lý thuyết hệ điều hành

- **Tiến trình và luồng:** client là một tiến trình có 3 luồng dùng chung bộ nhớ (`sockfd`, `name`, `flag`).
- **Lời gọi bị chặn:** `fgets`, `recv` khiến luồng gọi nó đứng chờ; tách luồng để việc chờ này không chặn việc khác.
- **Tín hiệu (signal):** SIGINT khi nhấn Ctrl+C, xử lý bằng hàm `catch_ctrl_c_and_exit`.
- **Khóa và chờ tài nguyên:** lỗi số 2 ở mục 6.
- **Chờ bận:** lỗi số 3 ở mục 6.