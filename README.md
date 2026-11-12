# Hệ thống Multi-threaded Chat Client/Server

Bài tập lớn môn **Hệ điều hành** — Đại học Bách khoa Hà Nội (HUST)

- **Mã lớp:** 173876
- **Giảng viên hướng dẫn:** TS. Nguyễn Quang Minh

> 🚧 Dự án đang trong giai đoạn khởi động. Nội dung chi tiết sẽ được cập nhật trong quá trình thực hiện.

## Giới thiệu

Một server xử lý tuần tự chỉ phục vụ được một client tại một thời điểm: khi đang chờ dữ liệu từ client này, các client khác phải xếp hàng chờ. Với một ứng dụng chat nhiều người, điều đó là không chấp nhận được.

Đề tài xây dựng một hệ thống chat theo mô hình **Client/Server** trên Linux, trong đó server áp dụng mô hình **đa luồng (multi-threading)**: mỗi client được phục vụ bởi một luồng riêng, nhờ đó nhiều người có thể trò chuyện đồng thời mà không chặn lẫn nhau. Dữ liệu dùng chung giữa các luồng được bảo vệ bằng cơ chế **đồng bộ hóa (Mutex)** để tránh xung đột.

## Mục tiêu

- Xây dựng ứng dụng chat cho phép nhiều client kết nối và trò chuyện đồng thời trong một phòng chung
- Vận dụng các kiến thức của môn Hệ điều hành: luồng, vùng găng, race condition, đồng bộ hóa, vào/ra
- Kiểm thử và đánh giá khả năng hoạt động của hệ thống khi có nhiều client

## Công nghệ sử dụng

- Hệ điều hành: Ubuntu (Linux)
- Ngôn ngữ: C
- Thư viện: POSIX Threads (Pthreads), Socket API
- Giao thức truyền: TCP

## Giao thức (nhóm thống nhất — tuần 2)

### Client gửi gì

| Khi nào | Nội dung |
|---|---|
| Ngay sau khi kết nối | `<tên>\n` |
| Mỗi lần chat | `<nội dung>\n` |
| Xem ai đang online | `/list\n` |
| Gửi file (tối đa 1 MB = 1048576 byte) | `/file <ten_file> <so_byte>\n` rồi đúng `<so_byte>` byte dữ liệu nhị phân ngay sau đó (không có `\n` kết thúc, vì dữ liệu file có thể chứa bất kỳ byte nào) |

### Server phát gì

| Tình huống | Server gửi |
|---|---|
| Tên hợp lệ, vào được phòng | `*** <tên> da vao phong\n` |
| Rời phòng (chủ động hoặc đứt kết nối) | `*** <tên> da roi phong\n` |
| Tin nhắn thường | `<tên>: <nội dung>\n` |
| Gõ `/list` | `*** Dang online (n): ten1, ten2, ...\n` — thứ tự theo ô trong mảng, không theo thời gian vào phòng |
| Trùng tên | `*** Ten "<tên>" da co nguoi dung. Vui long chon ten khac.\n` |
| Tên không hợp lệ | `*** Ten khong hop le. Ten dai 2-31 ky tu, khong chua dau cach.\n` |
| Phòng đầy | `*** Phong da day, vui long thu lai sau.\n` |
| Có người gửi file | `/file <ten_nguoi_gui> <ten_file> <so_byte>\n` rồi đúng `<so_byte>` byte dữ liệu nhị phân — gửi cho mọi người trong phòng trừ người gửi |
| File vượt quá 1 MB | `*** File qua lon, toi da 1048576 byte.\n` — Server không đọc phần dữ liệu đó, huỷ ngay, không chuyển cho ai |

**Dấu hiệu phân biệt:** mọi tin do Server sinh ra đều mở đầu bằng `*** `, nên Client chỉ cần so 4 ký tự đầu (`strncmp(buf, "*** ", 4)`) là biết đó không phải tin của người dùng. Tin của người dùng nếu bắt đầu bằng `*** ` sẽ bị Server lọc bỏ. Dòng `/file ...` là trường hợp riêng, không mang tiền tố `*** `: Client nhận diện bằng cách so khớp đúng 6 ký tự đầu `/file `, và sau khi đọc được dòng đó phải chuyển sang đọc tiếp đúng `<so_byte>` byte nhị phân — không được áp dụng cách đọc theo dòng (`\n`) cho phần dữ liệu này.

Giao thức trên **khác** code của repo mẫu (mẫu gửi tên đúng 32 byte cố định, Client tự gắn tên, không có `/list` lẫn kiểm tra trùng tên). Phần cài đặt làm ở tuần 3.


## Phạm vi

Đề tài tập trung vào phần lõi hệ thống (kết nối, đa luồng, đồng bộ hóa), giao tiếp qua giao diện dòng lệnh. Không bao gồm giao diện đồ họa, mã hóa tin nhắn hay lưu lịch sử trò chuyện.

## Thành viên

| Họ tên | MSSV | Vai trò |
|---|---|---|
| Nguyễn Trường Giang | 20233375 | Trưởng nhóm |
| Ngô Anh Quân | 20233595 | Thành viên |
| Chu Đức Nam | 20233540 | Thành viên |