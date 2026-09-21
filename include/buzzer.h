#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    BUZZER_PATTERN_OFF = 0,
    BUZZER_PATTERN_SHORT,       // 1 beep ngắn (100ms) - Quẹt thẻ thành công
    BUZZER_PATTERN_LONG,        // 1 beep dài (800ms) - Thẻ bị từ chối
    BUZZER_PATTERN_DOUBLE,      // 2 beep ngắn - Bắt đầu giờ thi
    BUZZER_PATTERN_EMERGENCY    // Kêu dồn dập liên tục - Báo cháy khẩn cấp
} buzzer_pattern_t;

void buzzer_init(void);
void buzzer_play(buzzer_pattern_t pattern);
void buzzer_set_state(bool on);
