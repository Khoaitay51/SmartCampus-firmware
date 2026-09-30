#include "rc522.h"
#include "app_config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <string.h>

static const char *TAG = "RC522";
static spi_device_handle_t s_spi = NULL;
static rfid_card_cb_t s_callback = NULL;

#define RFID_CARD_COOLDOWN_MS 2500 // Thoi gian cho toi thieu giua 2 lan quet cung the (2.5s)

// RC522 Registers (NXP MFRC522 Standard)
#define CommandReg           0x01
#define ComIEnReg            0x02
#define ComIrqReg            0x04
#define ErrorReg             0x06
#define FIFODataReg          0x09
#define FIFOLevelReg         0x0A
#define ControlReg           0x0C
#define BitFramingReg        0x0D
#define CollReg              0x0E
#define ModeReg              0x11
#define TxModeReg            0x12
#define RxModeReg            0x13
#define TxControlReg         0x14
#define TxASKReg             0x15
#define ModWidthReg          0x24
#define RFCfgReg             0x26
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
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 16,
        .tx_data = { (uint8_t)((reg << 1) & 0x7E), val },
    };
    gpio_set_level(PIN_RC522_CS, 0);
    esp_rom_delay_us(2);
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_RC522_CS, 1);
    esp_rom_delay_us(2);
}

static uint8_t rc522_read_reg(uint8_t reg) {
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
        .tx_data = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00 },
    };
    gpio_set_level(PIN_RC522_CS, 0);
    esp_rom_delay_us(2);
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_RC522_CS, 1);
    esp_rom_delay_us(2);
    return t.rx_data[1];
}

static void rc522_set_bitmask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) | mask);
}

static void rc522_clear_bitmask(uint8_t reg, uint8_t mask) {
    rc522_write_reg(reg, rc522_read_reg(reg) & (~mask));
}

static void rc522_antenna_on(void) {
    uint8_t temp = rc522_read_reg(TxControlReg);
    if ((temp & 0x03) != 0x03) {
        rc522_write_reg(TxControlReg, temp | 0x03);
    }
}

static bool rc522_to_card(uint8_t cmd, uint8_t *send_data, uint8_t send_len, uint8_t *back_data, uint32_t *back_len) {
    uint8_t irq_en = 0x77;
    uint8_t wait_irq = 0x30;
    rc522_write_reg(ComIEnReg, irq_en | 0x80);
    rc522_write_reg(ComIrqReg, 0x7F);      // Xoá sạch 7 cờ ngắt cũ (Set1=0 clears all)
    rc522_set_bitmask(FIFOLevelReg, 0x80); // Flush FIFO
    rc522_write_reg(CommandReg, PCD_IDLE);

    for (uint8_t i = 0; i < send_len; i++) {
        rc522_write_reg(FIFODataReg, send_data[i]);
    }

    rc522_write_reg(CommandReg, cmd);
    if (cmd == PCD_TRANSCEIVE) {
        rc522_set_bitmask(BitFramingReg, 0x80); // StartSend = 1
    }

    uint16_t i = 3000;
    uint8_t n = 0;
    do {
        n = rc522_read_reg(ComIrqReg);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & wait_irq));

    rc522_clear_bitmask(BitFramingReg, 0x80);

    // Kiem tra neu co interrupt hoan thanh nhan tin hieu
    if (i != 0 && (n & wait_irq)) {
        uint8_t err = rc522_read_reg(ErrorReg);
        if (!(err & 0x1B)) { // No BufferOvfl, CollErr, ParityErr, ProtocolErr
            if (cmd == PCD_TRANSCEIVE) {
                uint8_t fifo_len = rc522_read_reg(FIFOLevelReg);
                if (fifo_len == 0) return false;

                uint8_t last_bits = rc522_read_reg(ControlReg) & 0x07;
                if (last_bits) *back_len = (fifo_len - 1) * 8 + last_bits;
                else *back_len = fifo_len * 8;

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
    rc522_clear_bitmask(CollReg, 0x80); // ValuesAfterColl=0
    rc522_write_reg(BitFramingReg, 0x07); // 7 bits for REQA / WUPA
    tag_type[0] = req_mode;
    uint32_t back_len = 0;
    bool status = rc522_to_card(PCD_TRANSCEIVE, tag_type, 1, tag_type, &back_len);
    // ATQA response phai co do dai dung 16 bits (2 bytes)
    if (status && (back_len == 16 || back_len == 2)) {
        return true;
    }
    return false;
}

static bool rc522_anticoll(uint8_t *ser_num) {
    rc522_clear_bitmask(CollReg, 0x80); // ValuesAfterColl=0
    rc522_write_reg(BitFramingReg, 0x00);
    ser_num[0] = PICC_ANTICOLL;
    ser_num[1] = 0x20;
    uint32_t back_len = 0;
    bool status = rc522_to_card(PCD_TRANSCEIVE, ser_num, 2, ser_num, &back_len);
    // Anti-collision response phai dung 40 bits (5 bytes: 4 bytes UID + 1 byte BCC)
    if (status && (back_len == 40 || back_len == 5)) {
        // Kiem tra ma kiem tra BCC (Byte 4 = Byte 0 ^ 1 ^ 2 ^ 3)
        uint8_t check = ser_num[0] ^ ser_num[1] ^ ser_num[2] ^ ser_num[3];
        if (check != ser_num[4]) {
            ESP_LOGW(TAG, "Anticoll BCC checksum error: calc=0x%02X, recv=0x%02X", check, ser_num[4]);
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
    int absent_count = 0;

    while (1) {
        bool card_present = false;

        // Kiem tra the o trang thai IDLE (REQIDL) hoac ACTIVE/HALT (REQALL)
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

                // Chi kich hoat khi the moi duoc dat vao (khac the dang giu tren dau doc)
                if (strcmp(uid_str, active_card_uid) != 0) {
                    ESP_LOGI(TAG, "Card detected! UID: %s (Raw: %02X %02X %02X %02X, BCC: 0x%02X)",
                             uid_str, str[0], str[1], str[2], str[3], str[4]);
                    strncpy(active_card_uid, uid_str, sizeof(active_card_uid) - 1);
                    if (s_callback) s_callback(uid_str);
                }
            } else {
                ESP_LOGW(TAG, "Card detected in RF field but anticollision failed");
            }
        }

        if (!card_present) {
            absent_count++;
            // The da duoc nhac ra khoi dau doc (it nhat 3 chu ky ~ 300ms de chong jitter)
            if (absent_count >= 3 && active_card_uid[0] != '\0') {
                ESP_LOGI(TAG, "Card released: %s", active_card_uid);
                active_card_uid[0] = '\0';
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void rc522_pin_diagnostic(void) {
    ESP_LOGI(TAG, "--------------------------------------------------------");
    ESP_LOGI(TAG, "🔍 BẮT ĐẦU KIỂM TRA PHẦN CỨNG CHÂN GPIO CỦA ESP32:");
    
    // 1. Kiểm tra chân MISO (Input)
    gpio_reset_pin(PIN_RC522_MISO);
    gpio_set_direction(PIN_RC522_MISO, GPIO_MODE_INPUT);
    
    // Thử kéo Pull-up nội bộ của ESP32
    gpio_set_pull_mode(PIN_RC522_MISO, GPIO_PULLUP_ONLY);
    vTaskDelay(pdMS_TO_TICKS(10));
    int level_pu = gpio_get_level(PIN_RC522_MISO);
    
    // Thử kéo Pull-down nội bộ của ESP32
    gpio_set_pull_mode(PIN_RC522_MISO, GPIO_PULLDOWN_ONLY);
    vTaskDelay(pdMS_TO_TICKS(10));
    int level_pd = gpio_get_level(PIN_RC522_MISO);

    ESP_LOGI(TAG, "   👉 Chân MISO (GPIO %d): Khi PullUp = %d | Khi PullDown = %d", 
             PIN_RC522_MISO, level_pu, level_pd);

    if (level_pu == 0 && level_pd == 0) {
        ESP_LOGE(TAG, "   ⚠️ CẢNH BÁO NGUY HIỂM: Chân MISO (GPIO %d) đang bị ghim chặt ở mức 0V (GND)!", PIN_RC522_MISO);
        ESP_LOGE(TAG, "      Khả năng 1: Dây MISO cắm nhầm sang chân GND trên breadboard / RC522.");
        ESP_LOGE(TAG, "      Khả năng 2: Chân GPIO %d của ESP32 bị chập chết xuống mass (Hỏng chân ESP32)!", PIN_RC522_MISO);
    } else if (level_pu == 1 && level_pd == 1) {
        ESP_LOGW(TAG, "   ⚠️ CẢNH BÁO: Chân MISO (GPIO %d) đang bị kéo lên 3.3V liên tục!", PIN_RC522_MISO);
    } else {
        ESP_LOGI(TAG, "   ✅ Chân MISO (GPIO %d) của ESP32 HOẠT ĐỘNG HOÀN HẢO! (Không bị chập cháy)", PIN_RC522_MISO);
    }
    ESP_LOGI(TAG, "--------------------------------------------------------");
}

void rc522_init(rfid_card_cb_t callback) {
    s_callback = callback;

    // Chạy kiểm tra phần cứng chân ESP32 trước
    rc522_pin_diagnostic();

    // 1. Khoi tao CS pin
    gpio_reset_pin(PIN_RC522_CS);
    gpio_set_direction(PIN_RC522_CS, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(PIN_RC522_CS, GPIO_PULLUP_ONLY);
    gpio_set_level(PIN_RC522_CS, 1);

    // 2. Hard reset RC522 qua chan RST
    gpio_reset_pin(PIN_RC522_RST);
    gpio_set_direction(PIN_RC522_RST, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(PIN_RC522_RST, GPIO_PULLUP_ONLY);
    gpio_set_level(PIN_RC522_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_RC522_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 3. Khoi tao SPI Bus
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_RC522_MISO,
        .mosi_io_num = PIN_RC522_MOSI,
        .sclk_io_num = PIN_RC522_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_DISABLED);

    // Bật pull-up cho MISO ngay sau khi bus init để chống nhiễu lơ lửng
    gpio_set_pull_mode(PIN_RC522_MISO, GPIO_PULLUP_ONLY);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, // 1 MHz - tối ưu tuyệt đối cho dây cắm breadboard
        .mode = 0,
        .spics_io_num = -1, // CS dieu khien thu cong
        .queue_size = 7,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &s_spi);

    // 4. Soft reset MFRC522
    rc522_write_reg(CommandReg, PCD_RESETPHASE);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 5. Reset baud rates & modulation width (ISO 14443-A standard 106 kbd)
    rc522_write_reg(TxModeReg, 0x00);
    rc522_write_reg(RxModeReg, 0x00);
    rc522_write_reg(ModWidthReg, 0x26);

    // 6. Timer standard NXP AN10833 (f_timer = 40kHz, 25ms timeout)
    rc522_write_reg(TModeReg, 0x80);        // TAuto=1
    rc522_write_reg(TPrescalerReg, 0xA9);   // 40 kHz (25us per tick)
    rc522_write_reg(TReloadRegH, 0x03);     // 0x03E8 = 1000 ticks = 25ms timeout
    rc522_write_reg(TReloadRegL, 0xE8);

    // 7. Cau hinh ASK, CRC va Anten
    rc522_write_reg(TxASKReg, 0x40);        // Force 100% ASK
    rc522_write_reg(ModeReg, 0x3D);         // CRC preset 0x6363
    rc522_write_reg(RFCfgReg, 0x70);        // 48 dB (max antenna sensitivity)
    rc522_antenna_on();

    uint8_t ver = rc522_read_reg(VersionReg);
    uint8_t tx = rc522_read_reg(TxControlReg);
    ESP_LOGI(TAG, "RC522 VersionReg: 0x%02X, TxControlReg: 0x%02X (Antenna %s)", 
             ver, tx, (tx & 0x03) == 0x03 ? "ON" : "OFF");

    if (ver == 0x00 || ver == 0xFF) {
        ESP_LOGE(TAG, "⚠️ RC522 khong phan hoi (Ver 0x%02X)! Hay kiem tra day noi: SCK=%d, MOSI=%d, MISO=%d, CS(SDA)=%d, RST=%d, 3.3V & GND!",
                 ver, PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO, PIN_RC522_CS, PIN_RC522_RST);
    } else {
        ESP_LOGI(TAG, "✅ RC522 ket noi thanh cong! (Chip version: 0x%02X)", ver);
    }

    xTaskCreate(rc522_task, "rc522_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "RC522 initialized on SPI2 (SCK:%d, MOSI:%d, MISO:%d, CS:%d)",
             PIN_RC522_SCK, PIN_RC522_MOSI, PIN_RC522_MISO, PIN_RC522_CS);
}

uint8_t rc522_get_version(void) {
    if (!s_spi) return 0x00;
    return rc522_read_reg(VersionReg);
}

uint8_t rc522_get_raw_probe(uint8_t *raw_rx0, uint8_t *raw_rx1) {
    if (!s_spi) return 0x00;
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
        .length = 16,
        .tx_data = { (uint8_t)(((VersionReg << 1) & 0x7E) | 0x80), 0x00 },
    };
    gpio_set_level(PIN_RC522_CS, 0);
    esp_rom_delay_us(2);
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(PIN_RC522_CS, 1);
    esp_rom_delay_us(2);
    if (raw_rx0) *raw_rx0 = t.rx_data[0];
    if (raw_rx1) *raw_rx1 = t.rx_data[1];
    return t.rx_data[1];
}

void rc522_dump_registers(void) {
    if (!s_spi) {
        ESP_LOGW(TAG, "Cannot dump registers: SPI not initialized");
        return;
    }
    uint8_t ver = rc522_read_reg(VersionReg);
    uint8_t mode = rc522_read_reg(ModeReg);
    uint8_t tx = rc522_read_reg(TxControlReg);
    uint8_t rfc = rc522_read_reg(RFCfgReg);
    uint8_t cmd = rc522_read_reg(CommandReg);
    uint8_t stat = rc522_read_reg(0x07); // Status1Reg
    uint8_t fifo = rc522_read_reg(FIFOLevelReg);

    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       RC522 REGISTER DUMP REPORT       ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║ VersionReg    (0x37) : 0x%02X             ║", ver);
    ESP_LOGI(TAG, "║ ModeReg       (0x11) : 0x%02X             ║", mode);
    ESP_LOGI(TAG, "║ TxControlReg  (0x14) : 0x%02X (Antenna %s)║", tx, (tx & 0x03) == 0x03 ? "ON " : "OFF");
    ESP_LOGI(TAG, "║ RFCfgReg      (0x26) : 0x%02X             ║", rfc);
    ESP_LOGI(TAG, "║ CommandReg    (0x01) : 0x%02X             ║", cmd);
    ESP_LOGI(TAG, "║ Status1Reg    (0x07) : 0x%02X             ║", stat);
    ESP_LOGI(TAG, "║ FIFOLevelReg  (0x0A) : 0x%02X             ║", fifo);
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
}

