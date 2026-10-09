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

## Phạm vi

Đề tài tập trung vào phần lõi hệ thống (kết nối, đa luồng, đồng bộ hóa), giao tiếp qua giao diện dòng lệnh. Không bao gồm giao diện đồ họa, mã hóa tin nhắn hay lưu lịch sử trò chuyện.

## Thành viên

| Họ tên | MSSV | Vai trò |
|---|---|---|
| Nguyễn Trường Giang | 20233375 | Trưởng nhóm |
| Ngô Anh Quân | 20233595 | Thành viên |
| Chu Đức Nam | 20233540 | Thành viên |