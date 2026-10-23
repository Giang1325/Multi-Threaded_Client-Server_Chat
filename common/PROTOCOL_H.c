#ifndef PROTOCOL_H
#define PROTOCOL_H

/* =========================================================================
 * ĐẶC TẢ KÍCH THƯỚC (Theo chuẩn tài liệu thiết kế)
 * ========================================================================= */
#define MAX_BUFFER 1024  // Kích thước tối đa của một gói tin (bao gồm cả \n và \0)
#define MAX_USERNAME 20  // Chiều dài tối đa của tên người dùng

/* =========================================================================
 * KHAI BÁO CÁC HÀM GIAO TIẾP MẠNG LÕI
 * ========================================================================= */

/**
 * @brief Gửi một chuỗi dữ liệu qua socket. Hàm sẽ tự động đính kèm ký tự '\n' 
 *        vào cuối gói tin trước khi đẩy xuống tầng TCP.
 * 
 * @param sockfd File descriptor của socket cần gửi.
 * @param data Chuỗi dữ liệu thô cần gửi (chưa có ký tự \n).
 * @return int Số byte thực tế đã được gửi đi thành công, hoặc -1 nếu đứt cáp/lỗi mạng.
 */
int send_line(int sockfd, const char *data);

/**
 * @brief Lắng nghe và kéo dữ liệu từ socket về bộ đệm cho đến khi gặp ký tự '\n'.
 *        Ký tự '\n' sẽ được loại bỏ và thay thế bằng ký tự kết thúc chuỗi '\0'.
 * 
 * @param sockfd File descriptor của socket đang lắng nghe.
 * @param buffer Mảng ký tự (vùng nhớ) dùng để chứa thông điệp nhận được.
 * @param max_len Kích thước an toàn tối đa của buffer để tránh lỗi tràn bộ đệm (Buffer Overflow).
 * @return int Số lượng byte đọc được, 0 nếu Client đối phương đã ngắt kết nối, -1 nếu lỗi.
 */
int recv_line(int sockfd, char *buffer, int max_len);

/* =========================================================================
 * KHAI BÁO HÀM PHÂN TÍCH CÚ PHÁP (PARSER)
 * ========================================================================= */

/**
 * @brief Bóc tách gói tin thô thành Tiền tố lệnh (Command) và Nội dung (Argument).
 *        Ví dụ: "MSG Xin chao" -> command = "MSG", argument = "Xin chao".
 * 
 * @param raw_input Chuỗi dữ liệu thô vừa nhận được từ hàm recv_line.
 * @param command Con trỏ vùng nhớ để lưu tên lệnh (VD: JOIN, MSG, FROM, ERR).
 * @param argument Con trỏ vùng nhớ để lưu tham số đi kèm phía sau khoảng trắng.
 * @return int Trả về 1 nếu bóc tách thành công, 0 nếu gói tin sai định dạng.
 */
int parse_command(const char *raw_input, char *command, char *argument);

#endif // PROTOCOL_H