#pragma once
#include <stdbool.h>

typedef struct {
    float smoke_raw;            // Giá trị ADC khói (0 - 4095)
    bool smoke_detected;        // Phát hiện có khói
    const char *smoke_state;    // "normal", "suspected", "emergency"
    int co2_ppm;                // Nồng độ CO2 ước lượng (ppm)
    int air_quality;            // Chỉ số AQI (0 - 300)
} mq_data_t;

void mq_sensor_init(void);
void mq_sensor_read(mq_data_t *data);
