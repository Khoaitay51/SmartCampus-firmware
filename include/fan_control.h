#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Khởi tạo module điều khiển quạt làm mát/thông gió DC 5V (XD-4010 / XD-40100)
 */
void fan_control_init(void);

/**
 * @brief Bật hoặc tắt quạt
 * @param on true: Bật quạt (100% công suất), false: Tắt quạt
 */
void fan_control_set_state(bool on);

/**
 * @brief Cài đặt tốc độ quạt theo phần trăm (PWM)
 * @param speed_pct 0: Tắt, 1-100: Tốc độ theo %
 */
void fan_control_set_speed(uint8_t speed_pct);

/**
 * @brief Kiểm tra trạng thái quạt
 * @return true nếu quạt đang bật, false nếu đang tắt
 */
bool fan_control_is_on(void);

/**
 * @brief Lấy tốc độ quạt hiện tại (0-100%)
 */
uint8_t fan_control_get_speed(void);
