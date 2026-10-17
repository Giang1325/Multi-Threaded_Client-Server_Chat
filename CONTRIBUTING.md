# Nội quy làm việc của nhóm

Dự án: **Hệ thống Multi-threaded Chat Client/Server** — môn Hệ điều hành, mã lớp 173876.
Mọi thành viên đọc kỹ file này trước khi commit lần đầu. Có gì chưa rõ thì hỏi trưởng nhóm (Giang).

## 1. Môi trường

| Hạng mục | Quy ước |
|---|---|
| Hệ điều hành | Linux hoặc **WSL (Ubuntu)** trên Windows. Code dùng socket POSIX, Pthreads, tín hiệu và `/proc` nên **không** build bằng MinGW |
| Ngôn ngữ | C99 (kèm các hàm POSIX qua `-D_POSIX_C_SOURCE=200809L`) |
| Trình biên dịch | `gcc` (cài bằng `sudo apt install build-essential`) |
| Công cụ thử | `nc` (netcat) để giả lập Client/Server |
| Editor | VS Code + extension **WSL**; clone repo vào thư mục nhà của Ubuntu (`~`), không clone vào `/mnt/c/...` hay `/mnt/d/...` |

| Lệnh | Tác dụng |
|---|---|
| `make` | Build `bin/server` và `bin/client` |
| `make test` | Chạy kiểm thử |
| `make clean` | Xóa file build |

## 2. Năm quy tắc bắt buộc

1. **Không commit, không push thẳng lên `main`.** Nhánh `main` đã được khóa, push thẳng sẽ bị từ chối.
2. **Mỗi việc một nhánh**, tạo từ `main` mới nhất.
3. Trước khi mở Pull Request: `make` **không có cảnh báo** và `make test` chạy qua.
4. **Không commit file sinh ra** khi build hoặc chạy (`*.o`, `bin/`, file kết quả đo, file tạm của Word) và file thử nghiệm cá nhân.
5. Muốn sửa file của người khác thì **báo chủ sở hữu** (mục 6) trước.

## 3. Quy trình cho mỗi việc

```bash
git checkout main
git pull                              # lấy main mới nhất
git checkout -b feature/recv-line     # tạo nhánh cho việc mới
# ... code, sau đó:
make && make test                     # build không cảnh báo, test qua
git add <cac-file-cua-viec-nay>
git commit -m "feat: them recv_line doc dung 1 dong"
git push -u origin feature/recv-line
```

Sau đó lên GitHub mở Pull Request. Người duyệt góp ý thì **sửa tiếp trên cùng nhánh** rồi push lại (PR tự cập nhật). Khi được duyệt, trưởng nhóm merge, nhánh tự bị xóa trên GitHub; mọi người chạy `git checkout main && git pull` để cập nhật.

## 4. Đặt tên nhánh

Dạng `<loai>/<mo-ta-ngan>`, chữ thường, không dấu, nối bằng dấu gạch ngang.

| Tiền tố | Dùng khi | Ví dụ |
|---|---|---|
| `feature/` | Thêm chức năng | `feature/client-list`, `feature/sigint-shutdown` |
| `fix/` | Sửa lỗi | `fix/sigpipe-crash` |
| `test/` | Thêm, sửa kiểm thử | `test/stress-client` |
| `docs/` | Tài liệu | `docs/dac-ta-giao-thuc` |
| `build/` | Makefile, cấu hình repo | `build/make-unsafe` |

## 5. Commit message và Pull Request

**Commit message** dạng `<loai>: <mo ta ngan>`, viết **không dấu** (tránh lỗi font trên terminal), tối đa khoảng 70 ký tự. Loại gồm `feat`, `fix`, `test`, `docs`, `build`.

- Tốt: `feat: them client_add kiem tra trung ten trong 1 lan giu khoa`, `fix: chan SIGPIPE bang MSG_NOSIGNAL`
- Không tốt: `update`, `sua`, `final`, `code moi`

**Pull Request**

- Tiêu đề viết như commit message. Mô tả gồm 3 phần: **Làm gì / Đã kiểm tra / Ghi chú** (trên GitHub được viết tiếng Việt có dấu).
- Mỗi PR một việc; PR càng nhỏ càng dễ duyệt.
- Chỉ merge khi có **ít nhất 1 người không phải tác giả** bấm Approve.
- **Khi mở Pull Request, chọn người duyệt theo cột Người review ở mục 6.**

## 6. Chủ sở hữu module

Chủ sở hữu viết và chịu trách nhiệm file; người review duyệt PR và là người dự phòng khi chủ vắng. Có xung đột trong file của ai thì người đó quyết định. `client_list.h` và `protocol.h` là **hợp đồng chung**: muốn sửa phải báo cả nhóm.

| File / thư mục | Chủ sở hữu | Người review |
|---|---|---|
| `CONTRIBUTING.md`, `.gitignore`, `.gitattributes` | Giang | Quân |
| `Makefile`, `README.md` | Quân | Giang |
| `common/protocol.h` | Nam | Giang |
| `common/protocol.c` | Giang | Nam |
| `server/client_list.h` | Quân | Giang |
| `server/client_list.c` | Giang | Quân |
| `server/server.c` (phần tắt sạch khi Ctrl+C do Giang viết) | Quân | Nam, Giang |
| `client/client.c` | Nam | Quân |
| `tests/test_protocol.c` | Giang | Nam |
| `tests/test_client_list.c` | Giang | Quân |
| `tests/stress_client.c` | Nam | Quân |
| `tests/manual_checklist.md` | Nam | Giang |
| `docs/` | Mỗi người viết phần của mình; Giang ghép | — |

## 7. Viết code

- Thụt dòng bằng **4 dấu cách** trong file `.c`, `.h`. Riêng `Makefile` bắt buộc dùng **phím Tab**.
- Tên hàm, biến: chữ thường nối gạch dưới (`client_add`, `recv_line`); hằng số viết hoa (`MAX_CLIENTS`).
- Mỗi hàm trong file `.h` có chú thích: làm gì, tham số, trả về, **có lấy khóa không**.
- Đồng bộ hóa: mọi truy cập danh sách Client nằm giữa `lock` và `unlock`; không gọi hàm có khóa khi đang giữ khóa; không `return` khi đang giữ khóa; giữ khóa càng ngắn càng tốt.
- Kiểm tra giá trị trả về của mọi lời gọi hệ thống (`socket`, `send`, `recv`, `pthread_create`...).
- Không để cảnh báo khi build.