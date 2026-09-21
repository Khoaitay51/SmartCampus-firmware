#include "rc522.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "RC522";
static spi_device_handle_t s_spi = NULL;
static rfid_card_cb_t s_callback = NULL;

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

// Commands
#define PCD_IDLE             0x00
#define PCD_TRANSCEIVE       0x0C
#define PCD_RESETPHASE       0x0F
#define PICC_REQIDL          0x26
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
    rc522_set_bitmask(FIFOLevelReg, 0x80);
    rc522_write_reg(CommandReg, PCD_IDLE);

    for (uint8_t i = 0; i < send_len; i++) {
        rc522_write_reg(FIFODataReg, send_data[i]);
    }

    rc522_write_reg(CommandReg, cmd);
    if (cmd == PCD_TRANSCEIVE) {
        rc522_set_bitmask(BitFramingReg, 0x80);
    }

    uint16_t i = 2000;
    uint8_t n = 0;
    do {
        n = rc522_read_reg(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & wait_irq));

    rc522_clear_bitmask(BitFramingReg, 0x80);

    if (i != 0) {
        if (!(rc522_read_reg(ErrorReg) & 0x1B)) {
            if (n & irq_en & 0x01) return false;
            if (cmd == PCD_TRANSCEIVE) {
                uint8_t fifo_len = rc522_read_reg(FIFOLevelReg);
                uint8_t last_bits = rc522_read_reg(ControlReg) & 0x07;
                if (last_bits) *back_len = (fifo_len - 1) * 8 + last_bits;
                else *back_len = fifo_len * 8;

                if (fifo_len == 0) fifo_len = 1;
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
    return rc522_to_card(PCD_TRANSCEIVE, tag_type, 1, tag_type, &back_len);
}

static bool rc522_anticoll(uint8_t *ser_num) {
    rc522_write_reg(BitFramingReg, 0x00);
    ser_num[0] = PICC_ANTICOLL;
    ser_num[1] = 0x20;
    uint32_t back_len = 0;
    return rc522_to_card(PCD_TRANSCEIVE, ser_num, 2, ser_num, &back_len);
}

static void rc522_task(void *pvParameters) {
    uint8_t str[16];
    char last_uid_str[32] = {0};
    TickType_t last_tap_time = 0;

    while (1) {
        if (rc522_request(PICC_REQIDL, str)) {
            if (rc522_anticoll(str)) {
                char uid_str[32];
                snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X", str[0], str[1], str[2], str[3]);

                TickType_t now = xTaskGetTickCount();
                // Chống quẹt liên tục lặp thẻ trong vòng 2 giây
                if (strcmp(uid_str, last_uid_str) != 0 || (now - last_tap_time > pdMS_TO_TICKS(2000))) {
                    ESP_LOGI(TAG, "Card detected! UID: %s", uid_str);
                    strncpy(last_uid_str, uid_str, sizeof(last_uid_str) - 1);
                    last_tap_time = now;
                    if (s_callback) s_callback(uid_str);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
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
        .spics_io_num = -1, // CS điều khiển thủ công
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

    xTaskCreate(rc522_task, "rc522_task", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "RC522 initialized on SPI2 (SCK:%d, MOSI:%d, MISO:%d, CS:%d)",
             PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO, PIN_RC522_CS);
}
