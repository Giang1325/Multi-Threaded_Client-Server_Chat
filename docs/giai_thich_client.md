# Giải thích code Client (`client/client.c`)

Người viết: Nguyễn Trường Giang – Tuần 2

## 1. Nguồn

Tham khảo [nikhilroxtomar/Chatroom-in-C](https://github.com/nikhilroxtomar/Chatroom-in-C) (file `client.c`)

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

Lý do gửi tên **đúng 32 byte** (không phải `strlen(name)`):

1. **Ranh giới:** TCP là một dòng byte liền nhau, không có vạch ngăn giữa các lần gửi. Quy ước "32 byte đầu là tên" giúp server biết tên kết thúc ở đâu.
2. **Ký tự `\0`:** server cất tên vào một mảng cục bộ (chứa rác) và `recv` không tự thêm `\0`. Gửi đủ 32 byte thì các ô thừa đều là `\0` (vì `name` của client là biến toàn cục, được khởi tạo bằng 0), nên server có chuỗi kết thúc đúng.

Gửi `strlen(name) + 1` byte giải quyết được ý 2 nhưng không giải quyết được ý 1: nếu tin đầu tiên đến cùng lúc, `recv(32)` của server sẽ nuốt mất phần đầu tin đó.

## 5. Kết quả chạy thử

Thử với `nc -l 4444` đóng vai server (chưa có server của nhóm):

| Thao tác | Kết quả | Đạt? |
|---|---|---|
| Kết nối, nhập tên `Giang` | `nc` hiện `Giang` | Đạt |
| Client gửi `xin chao` | `nc` hiện `Giang: xin chao` (dính liền sau tên, xem lỗi 10) | Đạt |
| `nc` gửi `hello` | Client hiện `hello` | Đạt |
| Tắt `nc` bằng Ctrl+C | Client **bị treo**, không thoát | Lỗi |
| Ctrl+C ở client | In `Bye` nhưng **không thoát** | Lỗi |
| Gõ `exit` sau khi đã in `Bye` | Chương trình thoát | Đạt |
| Kết thúc dữ liệu nhập (Ctrl+D) ngay sau tên | `nc` nhận khoảng **3 MB** dòng `Giang: ` trong 2 giây | Lỗi |

*(Chèn ảnh chụp màn hình ở đây.)*

Thử với server thật: *(bổ sung sau khi có `server.c` của Quân.)*

## 6. Lỗi và hạn chế đã phát hiện

### 6.1. Lỗi nghiêm trọng (sửa ở tuần 3)

1. **Server tắt thì client treo.** Khi `recv` trả về 0, luồng nhận thoát nhưng **không đặt `flag = 1`**, nên luồng chính vẫn chờ mãi và luồng gửi vẫn đứng ở `fgets`. Nếu sau đó người dùng gõ tin, `send` vào kết nối đã đóng có thể sinh tín hiệu `SIGPIPE` và giết client không báo gì.

2. **Ctrl+C in `Bye` nhưng không thoát.** `return` từ `main` gọi `exit()`, hàm này phải **dọn dẹp `stdin`, `stdout`** trước khi tắt tiến trình, và muốn dọn `stdin` thì phải lấy **khóa của `stdin`**. Trong khi đó luồng gửi đang ở trong `fgets`: `fgets` lấy khóa `stdin` lúc bắt đầu và chỉ nhả khi đọc xong một dòng, nên luồng gửi **giữ khóa trong lúc chờ bàn phím**. Luồng chính chờ khóa, luồng gửi chờ người gõ, nên chương trình đứng im sau khi in `Bye` (đã kiểm tra: luồng chính đứng ở `futex_wait`, luồng gửi đứng ở `read`).
   - Gõ `exit` thì thoát được: luồng gửi đọc xong, nhả khóa, **ra khỏi vòng lặp và kết thúc**, không còn ai giành lại khóa.
   - Nhấn Enter trống thì vẫn treo: `fgets` nhả khóa nhưng vòng lặp gọi lại `fgets` ngay, giành lại khóa trước khi luồng chính kịp lấy.
   - Ctrl+\ (`SIGQUIT`) thì thoát ngay: hệ điều hành giết thẳng tiến trình, bỏ qua bước dọn dẹp.

3. **Chỉ thêm `flag = 1` cho lỗi 1 là chưa đủ.** Đã thử: khi server tắt, client in `Server disconnected.` rồi `Bye`, nhưng vẫn treo đúng như lỗi 2. Vì vậy **lỗi 1 và lỗi 2 phải sửa cùng nhau**. Các hướng sửa:
   - Thay `return` cuối `main` bằng `fflush(stdout); _exit(0);` (bỏ qua bước dọn dẹp). Đã thử: client thoát được khi server tắt.
   - Cho luồng gửi chờ bàn phím **có thời hạn** bằng `poll()`/`select()`, hết hạn thì kiểm tra `flag`, để các luồng tự kết thúc đàng hoàng.

4. **Ctrl+D (hết dữ liệu nhập) làm ngập server.** `fgets` trả về `NULL` nhưng code không kiểm tra, nên `message` rỗng được gửi đi thành `Giang: `, rồi vòng lặp gọi lại `fgets`, lại nhận `NULL` ngay lập tức, cứ thế lặp không nghỉ. Thử nghiệm: khoảng 3 MB trong 2 giây. Sửa: `if (fgets(message, LENGTH, stdin) == NULL) break;`

5. **Chờ bận (busy waiting) ở luồng chính.** Vòng `while(1){ if(flag) ... }` kiểm tra liên tục không nghỉ, làm một nhân CPU chạy gần 100% chỉ để chờ (xem bằng lệnh `top`). Nên thay bằng `pthread_join` hoặc condition variable.

### 6.2. Lỗi về dữ liệu và bộ nhớ

6. **Luật độ dài tên của client và server không khớp.** `fgets(name, 32, ...)` cho tên tối đa 31 ký tự, nên điều kiện `strlen(name) > 32` không bao giờ đúng; thông báo lỗi lại ghi "less than 30". Server mẫu chỉ chấp nhận tên **2–30 ký tự** (`strlen(name) >= 32-1` thì từ chối). Nhập đúng 31 ký tự: client vẫn in `WELCOME`, nhưng server từ chối và đóng kết nối, client tưởng đã vào phòng rồi treo (lỗi 1). Ngoài ra, tên dài hơn 31 ký tự thì phần thừa còn trong bộ nhớ đầu vào và bị đọc thành tin nhắn đầu tiên.

7. **Cảnh báo `sprintf`.** Trường hợp xấu nhất: tên 31 + `": "` 2 + nội dung 2047 + `"\n"` 1 + `\0` 1 = **2082 byte**, vượt 2080 byte của `buffer`. Đây là nguồn gốc cảnh báo `-Wformat-overflow` khi biên dịch. Nên dùng `snprintf`.

8. **`recv` có thể làm đầy cả 2048 byte**, không còn chỗ cho `\0`, khiến `printf("%s")` đọc lố ra ngoài mảng. Nên dùng `recv(..., LENGTH - 1, ...)`.

9. **Hàm luồng sai dạng.** `pthread_create` yêu cầu hàm dạng `void *f(void *arg)`, nhưng `send_msg_handler` và `recv_msg_handler` có dạng `void f()` và bị ép bằng `(void *)`. Chạy được trên x86-64 Linux nhưng theo chuẩn C là hành vi không được đảm bảo. Nên viết lại đúng dạng và `return NULL;`.

### 6.3. Hạn chế khác

10. **Tin đầu tiên dính vào tên** khi thử với `nc` (`GiangGiang: xin chao`): TCP là dòng byte liên tục, tên gửi đủ 32 byte (27 byte `\0` không hiển thị) và không có `\n`. Với server thật thì không bị, vì server đọc đúng 32 byte.

11. **Tên tiếng Việt có dấu:** Linux dùng UTF-8, mỗi chữ có dấu chiếm 2–3 byte và `strlen` đếm byte. Ví dụ `Trường` là 6 chữ nhưng 9 byte, nên tên có dấu bị giới hạn chặt hơn 31.

12. **Giả danh người khác:** client tự gắn tên vào tin (`sprintf(buffer, "%s: %s\n", ...)`), server chỉ chuyển tiếp nguyên văn. Một client bị sửa hoặc dùng `nc` có thể gửi `An: ...` dù không phải An. An toàn hơn là để server tự gắn tên theo kết nối.

13. **Thiếu kiểm tra lỗi:** không kiểm tra kết quả `socket()` và `send()`; `atoi` không báo lỗi khi cổng nhập sai (`abc` thành 0, chỉ thấy `ERROR: connect`); `ERROR: connect` không nói lý do (nên dùng `perror`); nhánh `recv` trả `-1` bị bỏ trống; `server_addr` không được xóa trắng bằng `memset` trước khi điền.

14. **IP cố định `127.0.0.1`**, chỉ chạy được cùng máy với server.

15. **Màn hình lộn xộn khi đang gõ dở mà có tin đến** (ví dụ `> xinBinh: hi`): hai luồng cùng in ra một màn hình không phối hợp. Chữ đang gõ không mất, chỉ hiển thị xấu.

## 7. Liên hệ lý thuyết hệ điều hành

- **Tiến trình và luồng:** mỗi client là 1 tiến trình gồm 3 luồng (chính, gửi, nhận) dùng chung biến toàn cục (`sockfd`, `name`, `flag`); biến cục bộ nằm trên stack riêng của từng luồng. Hai client là hai tiến trình với vùng nhớ tách biệt. Server mẫu là 1 tiến trình gồm 1 + N luồng (N là số client), dùng `clients_mutex` để bảo vệ danh sách client. Đã kiểm chứng bằng `ps -T`.
- **Lời gọi bị chặn:** `fgets`, `recv`, `connect` khiến luồng gọi nó đứng chờ; tách luồng để việc chờ bàn phím và chờ mạng không chặn nhau. Cách khác dùng một luồng là `select()`/`poll()`.
- **Lập lịch:** sau `pthread_create`, thứ tự chạy các luồng do bộ lập lịch của hệ điều hành quyết định.
- **Tín hiệu (signal):** SIGINT (Ctrl+C) được bắt bằng `signal()`; hàm xử lý chỉ dựng cờ vì tín hiệu có thể chen ngang bất kỳ lúc nào. `volatile` buộc đọc lại giá trị thật từ bộ nhớ, `sig_atomic_t` đảm bảo đọc/ghi trong một bước. SIGQUIT (Ctrl+\) và SIGPIPE giữ hành vi mặc định là kết thúc tiến trình.
- **Khóa và chờ tài nguyên – có phải deadlock?** Lỗi 2 thỏa 3 điều kiện của deadlock (loại trừ tương hỗ, giữ và chờ, không chiếm đoạt) nhưng **không có chờ vòng tròn**: luồng gửi chờ bàn phím, không chờ thứ gì luồng chính đang giữ. Vì vậy, nói chặt chẽ thì đây **không phải deadlock**, mà là một luồng bị chặn vô hạn vì chờ khóa mà luồng khác giữ trong lúc chờ nhập xuất; chỉ cần người dùng gõ một dòng là được giải thoát.
- **Chờ bận:** lỗi 5, lãng phí CPU.
- **Bộ đệm nhập xuất:** `printf` ghi vào bộ đệm, chỉ thật sự in khi gặp `\n`, bộ đệm đầy, gọi `fflush`, hoặc khi chuẩn bị đọc bàn phím (glibc). Vì vậy `fflush(stdout)` là bắt buộc trong luồng nhận.
- **Bảo mật (Chương 7):** lỗi 12, server tin tưởng tên do client tự khai.