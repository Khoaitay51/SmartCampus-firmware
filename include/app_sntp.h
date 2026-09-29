#pragma once

#include <stdbool.h>

/**
 * @brief Khởi tạo SNTP client để đồng bộ thời gian thực từ Internet.
 *
 * Gọi SAU khi Wi-Fi đã kết nối. Sử dụng NTP server pool.ntp.org.
 * Hàm này non-blocking: SNTP sẽ đồng bộ ngầm trong background.
 */
void app_sntp_init(void);

/**
 * @brief Chờ cho đến khi thời gian thực được đồng bộ thành công.
 *
 * @param timeout_ms Thời gian chờ tối đa (ms). Dùng 0 để không chờ.
 * @return true  Đã đồng bộ thành công.
 * @return false Hết timeout mà chưa đồng bộ được.
 */
bool app_sntp_wait_synced(uint32_t timeout_ms);

/**
 * @brief Kiểm tra SNTP đã đồng bộ thành công hay chưa.
 */
bool app_sntp_is_synced(void);

/**
 * @brief Lấy timestamp hiện tại dạng ISO 8601 (UTC).
 *
 * Ví dụ: "2026-09-28T06:30:00Z"
 * Nếu SNTP chưa đồng bộ, trả về thời gian ước lượng từ boot (epoch + uptime).
 *
 * @param buf     Buffer đầu ra (khuyến nghị >= 30 bytes).
 * @param buf_len Kích thước buffer.
 */
void app_sntp_get_iso8601(char *buf, size_t buf_len);
