#include "rc522.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>

static const char *TAG = "RC522";
static spi_device_handle_t s_spi = NULL;
static rfid_card_cb_t s_callback = NULL;

#define RFID_CARD_COOLDOWN_MS 2500 // Thoi gian cho toi thieu giua 2 lan quet cung the (2.5s)

// RC522 Registers
#define CommandReg           0x01
#define ComIEnReg            0x02
#define ComIrqReg            0x04
#define ErrorReg             0x06
#define FIFODataReg          0x09
#define FIFOLevelReg         0x0A
#define ControlReg           0x0C
#define BitFramingReg        0x0D
#define ModeReg              0x11
#define TxControlReg         0x14
#define TxASKReg             0x15
#define TModeReg             0x2A
#define TPrescalerReg        0x2B
#define TReloadRegH          0x2C
#define TReloadRegL          0x2D
#define VersionReg           0x37

// Commands
#define PCD_IDLE             0x00
#define PCD_TRANSCEIVE       0x0C
#define PCD_RESETPHASE       0x0F
#define PICC_REQIDL          0x26
#define PICC_REQALL          0x52
#define PICC_ANTICOLL        0x93

static void rc522_write_reg(uint8_t reg, uint8_t val) {
    uint8_t tx_data[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
    };
    gpio_set_level(PIN_RC522_CS, 0);
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_RC522_CS, 1);
}

static uint8_t rc522_read_reg(uint8_t reg) {
    uint8_t tx_data[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00 };
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    gpio_set_level(PIN_RC522_CS, 0);
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_RC522_CS, 1);
    return rx_data[1];
}

static void rc522_set_bitmask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_clear_bitmask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) & (~mask));
}

static void rc522_antenna_on(void) {
    uint8_t temp = rc522_read_reg(TxControlReg);
    if (!(temp & 0x03)) {
        rc522_set_bitmask(TxControlReg, 0x03);
    }
}

static bool rc522_to_card(uint8_t cmd, uint8_t *send_data, uint8_t send_len, uint8_t *back_data, uint32_t *back_len) {
    uint8_t irq_en = 0x77;
    uint8_t wait_irq = 0x30;
    rc522_write_reg(ComIEnReg, irq_en | 0x80);
    rc522_clear_bitmask(ComIrqReg, 0x80);
    rc522_set_bitmask(FIFOLevelReg, 0x80); // Flush FIFO
    rc522_write_reg(CommandReg, PCD_IDLE);

    for (uint8_t i = 0; i < send_len; i++) {
        rc522_write_reg(FIFODataReg, send_data[i]);
    }

    rc522_write_reg(CommandReg, cmd);
    if (cmd == PCD_TRANSCEIVE) {
        rc522_set_bitmask(BitFramingReg, 0x80); // StartSend = 1
    }

    uint16_t i = 2000;
    uint8_t n = 0;
    do {
        n = rc522_read_reg(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & wait_irq));

    rc522_clear_bitmask(BitFramingReg, 0x80);

    if (i != 0) {
        uint8_t err = rc522_read_reg(ErrorReg);
        if (!(err & 0x1B)) { // No BufferOvfl, CollErr, ParityErr, ProtocolErr
            if (n & irq_en & 0x01) return false;
            if (cmd == PCD_TRANSCEIVE) {
                uint8_t fifo_len = rc522_read_reg(FIFOLevelReg);
                uint8_t last_bits = rc522_read_reg(ControlReg) & 0x07;
                if (last_bits) *back_len = (fifo_len - 1) * 8 + last_bits;
                else *back_len = fifo_len * 8;

                if (fifo_len == 0) return false;
                if (fifo_len > 16) fifo_len = 16;
                for (uint8_t j = 0; j < fifo_len; j++) {
                    back_data[j] = rc522_read_reg(FIFODataReg);
                }
            }
            return true;
        }
    }
    return false;
}

static bool rc522_request(uint8_t req_mode, uint8_t *tag_type) {
    rc522_write_reg(BitFramingReg, 0x07);
    tag_type[0] = req_mode;
    uint32_t back_len = 0;
    bool status = rc522_to_card(PCD_TRANSCEIVE, tag_type, 1, tag_type, &back_len);
    // ATQA response phai co do dai dung 16 bits (2 bytes)
    if (status && back_len == 16) {
        return true;
    }
    return false;
}

static bool rc522_anticoll(uint8_t *ser_num) {
    rc522_write_reg(BitFramingReg, 0x00);
    ser_num[0] = PICC_ANTICOLL;
    ser_num[1] = 0x20;
    uint32_t back_len = 0;
    bool status = rc522_to_card(PCD_TRANSCEIVE, ser_num, 2, ser_num, &back_len);
    // Anti-collision response phai dung 40 bits (5 bytes: 4 bytes UID + 1 byte BCC)
    if (status && back_len == 40) {
        // Kiem tra ma kiem tra BCC (Byte 4 = Byte 0 ^ 1 ^ 2 ^ 3)
        uint8_t check = ser_num[0] ^ ser_num[1] ^ ser_num[2] ^ ser_num[3];
        if (check != ser_num[4]) {
            return false; // Sai checksum -> Nhieu tin hieu, loai bo
        }
        // Loai bo UID toan 0 hoac toan FF do duong truyen SPI ho
        if ((ser_num[0] == 0 && ser_num[1] == 0 && ser_num[2] == 0 && ser_num[3] == 0) ||
            (ser_num[0] == 0xFF && ser_num[1] == 0xFF && ser_num[2] == 0xFF && ser_num[3] == 0xFF)) {
            return false;
        }
        return true;
    }
    return false;
}

static void rc522_task(void *pvParameters) {
    uint8_t str[16];
    char active_card_uid[32] = {0};
    char last_tap_uid[32] = {0};
    int64_t last_tap_time = 0;
    int absent_count = 0;

    while (1) {
        bool card_present = false;

        // Kiem tra the o trang thai IDLE (REQIDL) hoac ACTIVE/HALT (REQALL)
        // giup tranh hien tuong the van dat tren dau doc nhung bi rot frame do doi trang thai ISO14443
        bool found = rc522_request(PICC_REQIDL, str);
        if (!found) {
            found = rc522_request(PICC_REQALL, str);
        }

        if (found) {
            if (rc522_anticoll(str)) {
                card_present = true;
                absent_count = 0;

                char uid_str[32];
                snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X", str[0], str[1], str[2], str[3]);

                int64_t now = esp_timer_get_time() / 1000;
                bool is_same_as_last = (strcmp(uid_str, last_tap_uid) == 0);
                bool in_cooldown = is_same_as_last && ((now - last_tap_time) < RFID_CARD_COOLDOWN_MS);

                // Chi kich hoat callback khi:
                // 1. The khac voi the dang active tren dau doc
                // 2. Hoac da qua thoi gian cooldown 2.5s (chong spam quet the lien tuc)
                if (!in_cooldown && strcmp(uid_str, active_card_uid) != 0) {
                    ESP_LOGI(TAG, "New card detected! UID: %s", uid_str);
                    strncpy(active_card_uid, uid_str, sizeof(active_card_uid) - 1);
                    strncpy(last_tap_uid, uid_str, sizeof(last_tap_uid) - 1);
                    last_tap_time = now;
                    if (s_callback) s_callback(uid_str);
                } else {
                    // The van dang nam tren module hoac trong thoi gian cooldown -> duy tri active_card_uid
                    strncpy(active_card_uid, uid_str, sizeof(active_card_uid) - 1);
                }
            }
        }

        if (!card_present) {
            absent_count++;
            // The da thuc su duoc nhac ra khoi dau doc (can it nhat 8 chu ky ~ 800ms khong co tin hieu de chong jitter)
            if (absent_count >= 8 && active_card_uid[0] != '\0') {
                ESP_LOGI(TAG, "Card released: %s", active_card_uid);
                active_card_uid[0] = '\0';
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void rc522_init(rfid_card_cb_t callback) {
    s_callback = callback;

    gpio_reset_pin(PIN_RC522_CS);
    gpio_set_direction(PIN_RC522_CS, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RC522_CS, 1);

    gpio_reset_pin(PIN_RC522_RST);
    gpio_set_direction(PIN_RC522_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RC522_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_RC522_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_RC522_MISO,
        .mosi_io_num = PIN_RC522_MOSI,
        .sclk_io_num = PIN_RC522_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5 * 1000 * 1000, // 5 MHz
        .mode = 0,
        .spics_io_num = -1, // CS dieu khien thu cong
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

    rc522_write_reg(CommandReg, PCD_RESETPHASE);
    vTaskDelay(pdMS_TO_TICKS(10));
    rc522_write_reg(TModeReg, 0x8D);
    rc522_write_reg(TPrescalerReg, 0x3E);
    rc522_write_reg(TReloadRegL, 30);
    rc522_write_reg(TReloadRegH, 0);
    rc522_write_reg(TxASKReg, 0x40);
    rc522_write_reg(ModeReg, 0x3D);
    rc522_antenna_on();

    uint8_t ver = rc522_read_reg(VersionReg);
    ESP_LOGI(TAG, "RC522 VersionReg: 0x%02X", ver);

    xTaskCreate(rc522_task, "rc522_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "RC522 initialized on SPI2 (SCK:%d, MOSI:%d, MISO:%d, CS:%d)",
             PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO, PIN_RC522_CS);
}
