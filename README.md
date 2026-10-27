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

- **Gửi tên:** ngay sau khi kết nối, Client gửi `<tên>\n`; Server đọc đến khi gặp `\n`. Tên dài 2–31 ký tự, không chứa dấu cách.
- **Trùng tên:** Server trả `*** Ten "<tên>" da co nguoi dung. Vui long chon ten khac.` rồi đóng kết nối.
- **Tin nhắn thường:** `<tên>: <nội dung>\n` — **Server** gắn tên, không phải Client, để không ai mạo danh được người khác.
- **Dấu hiệu phân biệt:** mọi tin do Server sinh ra đều mở đầu bằng `*** `, nên Client chỉ cần so 4 ký tự đầu (`strncmp(buf, "*** ", 4)`) là biết đó không phải tin của người dùng. Tin của người dùng nếu bắt đầu bằng `*** ` sẽ bị Server lọc bỏ.
- **Vào/rời phòng:** `*** <tên> da vao phong` và `*** <tên> da roi phong`.
- **`/list`:** trả về đúng một dòng — `*** Dang online (3): Giang, Quan, Nam`.

Giao thức trên **khác** code của repo mẫu (mẫu gửi tên đúng 32 byte cố định, Client tự gắn tên, không có `/list` lẫn kiểm tra trùng tên). Phần cài đặt làm ở tuần 3.

## Phạm vi

Đề tài tập trung vào phần lõi hệ thống (kết nối, đa luồng, đồng bộ hóa), giao tiếp qua giao diện dòng lệnh. Không bao gồm giao diện đồ họa, mã hóa tin nhắn hay lưu lịch sử trò chuyện.

## Thành viên

| Họ tên | MSSV | Vai trò |
|---|---|---|
| Nguyễn Trường Giang | 20233375 | Trưởng nhóm |
| Ngô Anh Quân | 20233595 | Thành viên |
| Chu Đức Nam | 20233540 | Thành viên |