#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Khởi tạo module điều khiển quạt thông gió DC 5V.
 *
 * Hỗ trợ 2 chế độ phần cứng (cấu hình bởi FAN_2WIRE_GPIO trong app_config.h):
 *   - FAN_2WIRE_GPIO = 1: Quạt 2 dây (VCC + GND) → Điều khiển bật/tắt qua GPIO + Transistor NPN/MOSFET.
 *   - FAN_2WIRE_GPIO = 0: Quạt 3-4 dây (có tín hiệu PWM) → Điều khiển tốc độ qua LEDC PWM.
 */
void fan_control_init(void);

/**
 * @brief Bật hoặc tắt quạt.
 * @param on true: Bật quạt, false: Tắt quạt
 */
void fan_control_set_state(bool on);

/**
 * @brief Cài đặt tốc độ quạt theo phần trăm.
 *
 * Với quạt 2 dây: speed > 0 → bật, speed = 0 → tắt (không có speed control thực).
 * Với quạt PWM: điều chỉnh duty cycle tương ứng 0-100%.
 *
 * @param speed_pct 0: Tắt, 1-100: Tốc độ theo %
 */
void fan_control_set_speed(uint8_t speed_pct);

/**
 * @brief Kiểm tra trạng thái quạt.
 * @return true nếu quạt đang bật, false nếu đang tắt
 */
bool fan_control_is_on(void);

/**
 * @brief Lấy tốc độ quạt hiện tại.
 *
 * Với quạt 2 dây: trả về 100 (bật) hoặc 0 (tắt).
 * Với quạt PWM: trả về giá trị 0-100%.
 */
uint8_t fan_control_get_speed(void);
