#include "oled_ssd1306.h"
#include "app_config.h"
#include "fan_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "OLED";
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_oled_dev = NULL;
static SemaphoreHandle_t s_oled_mutex = NULL;

// Bang ma ky tu font 5x7 don gian (ASCII 32 ' ' den 126 '~')
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x10, 0x08, 0x04, 0x08}, // ~
};

static uint8_t s_buffer[1024] = {0};

static void oled_write_cmd(uint8_t cmd) {
    if (!s_oled_dev) return;
    uint8_t buf[2] = {0x00, cmd};
    i2c_master_transmit(s_oled_dev, buf, 2, 100);
}

static void oled_update_screen(void) {
    if (!s_oled_dev) return;
    for (uint8_t page = 0; page < 8; page++) {
        oled_write_cmd(0xB0 + page);
        oled_write_cmd(0x00);
        oled_write_cmd(0x10);

        uint8_t buf[129];
        buf[0] = 0x40;
        memcpy(&buf[1], &s_buffer[page * 128], 128);
        i2c_master_transmit(s_oled_dev, buf, 129, 100);
    }
}

static void oled_draw_char(int x, int page, char c) {
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    for (int col = 0; col < 5; col++) {
        if (x + col < 128) {
            s_buffer[page * 128 + x + col] = font5x7[idx][col];
        }
    }
}

static void oled_draw_string(int x, int page, const char *str) {
    while (*str && x < 123) {
        oled_draw_char(x, page, *str);
        x += 6;
        str++;
    }
}

static void oled_draw_char_inverted(int x, int page, char c) {
    if (c < 32 || c > 126) c = ' ';
    int idx = c - 32;
    for (int col = 0; col < 5; col++) {
        if (x + col < 128) {
            s_buffer[page * 128 + x + col] = (uint8_t)~font5x7[idx][col];
        }
    }
}

static void oled_draw_string_inverted(int x, int page, const char *str) {
    while (*str && x < 123) {
        oled_draw_char_inverted(x, page, *str);
        if (x + 5 < 128) {
            s_buffer[page * 128 + x + 5] = 0xFF;
        }
        x += 6;
        str++;
    }
}

static void oled_draw_header(const char *left_text, bool wifi_ok, bool mqtt_ok) {
    memset(&s_buffer[0 * 128], 0xFF, 128);
    if (left_text) {
        oled_draw_string_inverted(2, 0, left_text);
    }
    char net_str[16];
    snprintf(net_str, sizeof(net_str), "W:%s M:%s", wifi_ok ? "OK" : "--", mqtt_ok ? "OK" : "--");
    oled_draw_string_inverted(74, 0, net_str);
}

void oled_init(void) {
    s_oled_mutex = xSemaphoreCreateMutex();

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OLED_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    ret = i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_oled_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add OLED I2C device: %s", esp_err_to_name(ret));
        return;
    }

    // SSD1306 full init sequence (128x64)
    oled_write_cmd(0xAE);                           // Display OFF
    oled_write_cmd(0xD5); oled_write_cmd(0x80);     // Set display clock divide ratio
    oled_write_cmd(0xA8); oled_write_cmd(0x3F);     // MUX ratio = 64
    oled_write_cmd(0xD3); oled_write_cmd(0x00);     // Display offset = 0
    oled_write_cmd(0x40);                           // Display start line = 0
    oled_write_cmd(0x8D); oled_write_cmd(0x14);     // Charge pump enabled
    oled_write_cmd(0x20); oled_write_cmd(0x02);     // Page addressing mode (QUAN TRONG!)
    oled_write_cmd(0xA1);                           // Segment remap (col 127 -> SEG0)
    oled_write_cmd(0xC8);                           // COM output scan direction remapped
    oled_write_cmd(0xDA); oled_write_cmd(0x12);     // COM pins config cho 128x64
    oled_write_cmd(0x81); oled_write_cmd(0xCF);     // Contrast = 0xCF
    oled_write_cmd(0xD9); oled_write_cmd(0xF1);     // Pre-charge period
    oled_write_cmd(0xDB); oled_write_cmd(0x40);     // VCOMH deselect level
    oled_write_cmd(0xA4);                           // Display ON theo RAM content
    oled_write_cmd(0xA6);                           // Normal display (khong invert)
    oled_write_cmd(0xAF);                           // Display ON

    memset(s_buffer, 0, sizeof(s_buffer));
    memset(&s_buffer[0 * 128], 0xFF, 128);
    oled_draw_string_inverted(16, 0, "SMARTCAMPUS BMS");
    oled_draw_string(10, 3, "KHOI DONG HE THONG");
    oled_draw_string(18, 5, "INITIALIZING...");
    oled_update_screen();
    ESP_LOGI(TAG, "OLED SSD1306 initialized on I2C master (SDA:%d, SCL:%d)", PIN_OLED_SDA, PIN_OLED_SCL);
}

void oled_display_boot(const char *node_name, const char *step_text, const char *sub_text) {
    if (s_oled_mutex) xSemaphoreTake(s_oled_mutex, portMAX_DELAY);
    memset(s_buffer, 0, sizeof(s_buffer));

    // Page 0: Thanh tieu de dao nguoc
    memset(&s_buffer[0 * 128], 0xFF, 128);
    char title[32];
    snprintf(title, sizeof(title), "SMARTCAMPUS: %s", node_name ? node_name : "NODE");
    oled_draw_string_inverted(2, 0, title);

    // Page 2: Trang thai
    oled_draw_string(2, 2, "KHOI DONG HE THONG:");

    // Page 3: Buoc hien tai
    if (step_text) {
        char step_buf[32];
        snprintf(step_buf, sizeof(step_buf), "> %s", step_text);
        oled_draw_string(4, 3, step_buf);
    }

    // Page 5: Thong tin phu (SSID, IP...)
    if (sub_text) {
        oled_draw_string(4, 5, sub_text);
    }

    // Page 7: Footer
    oled_draw_string(2, 7, "Vui long cho...");

    oled_update_screen();
    if (s_oled_mutex) xSemaphoreGive(s_oled_mutex);
}

void oled_display_room(const char *node_name,
                       const char *room_mode,
                       bool wifi_ok,
                       bool mqtt_ok,
                       float temp,
                       float hum,
                       bool fan_on,
                       uint8_t fan_spd,
                       bool door_locked,
                       int occ_count,
                       int ir_in,
                       int ir_out,
                       float smoke_val,
                       bool smoke_alert,
                       const char *card_uid,
                       const char *card_status) {
    if (s_oled_mutex) xSemaphoreTake(s_oled_mutex, portMAX_DELAY);
    memset(s_buffer, 0, sizeof(s_buffer));

    // Page 0: Status bar (Inverted)
    oled_draw_header(node_name ? node_name : "ROOM", wifi_ok, mqtt_ok);

    char line[32];

    // Page 1: Che do phong & Khoa cua
    snprintf(line, sizeof(line), "MODE:%-7s D:%-4s",
             room_mode ? room_mode : "SAVING",
             door_locked ? "LOCK" : "OPEN");
    oled_draw_string(0, 1, line);

    // Page 2: Nhiet do, Do am, Quat (FAN)
    if (fan_on) {
        if (fan_spd > 0 && fan_spd < 100) {
            snprintf(line, sizeof(line), "T:%-4.1fC H:%-2.0f%% F:%d%%", temp, hum, (int)fan_spd);
        } else {
            snprintf(line, sizeof(line), "T:%-4.1fC H:%-2.0f%% F:ON ", temp, hum);
        }
    } else {
        snprintf(line, sizeof(line), "T:%-4.1fC H:%-2.0f%% F:OFF", temp, hum);
    }
    oled_draw_string(0, 2, line);

    // Page 3: Khoi & Khong khi (Air / Safety)
    if (smoke_alert) {
        snprintf(line, sizeof(line), "!CANH BAO KHOI!(%.0f)", smoke_val);
    } else {
        snprintf(line, sizeof(line), "AIR:NORMAL GAS:OK");
    }
    oled_draw_string(0, 3, line);

    // Page 4: So nguoi (Occupancy) & 2 mat IR
    snprintf(line, sizeof(line), "OCC:%-3d   IR:[%d,%d]",
             occ_count, ir_in, ir_out);
    oled_draw_string(0, 4, line);

    // Page 5: Duong ke ngan cach
    oled_draw_string(0, 5, "---------------------");

    // Page 6: The RFID quet gan nhat
    if (card_uid && strlen(card_uid) > 0) {
        snprintf(line, sizeof(line), "THE: %s", card_uid);
    } else {
        snprintf(line, sizeof(line), "THE: -- (SAN SANG)");
    }
    oled_draw_string(0, 6, line);

    // Page 7: Trang thai the / Ket qua diem danh
    if (card_status && strlen(card_status) > 0) {
        snprintf(line, sizeof(line), "TT: %s", card_status);
    } else {
        snprintf(line, sizeof(line), "TT: SAN SANG QUET");
    }
    oled_draw_string(0, 7, line);

    oled_update_screen();
    if (s_oled_mutex) xSemaphoreGive(s_oled_mutex);
}

void oled_display_corridor(const char *node_name,
                          bool wifi_ok,
                          bool mqtt_ok,
                          const char *card_uid,
                          const char *status_str,
                          const char *user_info,
                          const char *extra_info) {
    if (s_oled_mutex) xSemaphoreTake(s_oled_mutex, portMAX_DELAY);
    memset(s_buffer, 0, sizeof(s_buffer));

    // Page 0: Status bar (Inverted)
    oled_draw_header(node_name ? node_name : "CORRIDOR", wifi_ok, mqtt_ok);

    char line[32];

    // Page 1: Tieu de nghiep vu
    oled_draw_string(0, 1, "QUET THE DANG KY / REG");

    // Page 2: Ma the UID
    if (card_uid && strlen(card_uid) > 0) {
        snprintf(line, sizeof(line), "UID:  %s", card_uid);
    } else {
        snprintf(line, sizeof(line), "UID:  -- (CHO THE)");
    }
    oled_draw_string(0, 2, line);

    // Page 3: Duong ke
    oled_draw_string(0, 3, "---------------------");

    // Page 4: Trang thai duyet the
    snprintf(line, sizeof(line), "ST: %s", (status_str && strlen(status_str) > 0) ? status_str : "SAN SANG");
    oled_draw_string(0, 4, line);

    // Page 5: Thong tin nguoi dung hoac huong dan
    if (user_info && strlen(user_info) > 0) {
        snprintf(line, sizeof(line), "INFO: %s", user_info);
    } else {
        snprintf(line, sizeof(line), "INFO: SMARTCAMPUS BMS");
    }
    oled_draw_string(0, 5, line);

    // Page 6: MAC Address hoac ID thiet bi
    if (extra_info && strlen(extra_info) > 0) {
        snprintf(line, sizeof(line), "DEV: %s", extra_info);
    } else {
        snprintf(line, sizeof(line), "DEV: GATEWAY ONLINE");
    }
    oled_draw_string(0, 6, line);

    // Page 7: He thong san sang
    oled_draw_string(0, 7, "GATEWAY DANG KY THE");

    oled_update_screen();
    if (s_oled_mutex) xSemaphoreGive(s_oled_mutex);
}

void oled_display_status(const char *room_mode, float temp, float hum, const char *card_uid, bool door_locked, int occ_count, int ir_in, int ir_out) {
    oled_display_room("ROOM", room_mode, true, true, temp, hum,
                      fan_control_is_on(), fan_control_get_speed(),
                      door_locked, occ_count, ir_in, ir_out, 0, false, card_uid, "OK");
}

void oled_display_corridor_legacy(const char *card_uid, const char *status_str, const char *user_info) {
    oled_display_corridor("CORRIDOR", true, true, card_uid, status_str, user_info, "OK");
}
