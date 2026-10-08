/*
 * ==============================================================================
 * Test Bật/Tắt Quạt DC bằng Module Relay 5V / Transistor với ESP-12E / NodeMCU
 * ==============================================================================
 * 
 * ĐẶC BIỆT: Kích đồng thời TẤT CẢ các chân GPIO an toàn:
 *   - D1 (GPIO 5)
 *   - D2 (GPIO 4)
 *   - D5 (GPIO 14)
 *   - D6 (GPIO 12)
 *   - D7 (GPIO 13)
 * => Bạn có thể cắm chân IN của Relay 5V vào BẤT KỲ chân nào ở trên (D1, D2, D5, D6, D7) đều được điều khiển!
 * 
 * Chu kỳ tự động: Bật 5 giây, Tắt 5 giây tuần hoàn.
 * ==============================================================================
 */

#include <Arduino.h>

// 1: Module Relay 5V Active LOW (Kích mức LOW để đóng rơ-le / BẬT quạt - MẶC ĐỊNH)
// 0: Active HIGH (Transistor NPN / Relay kích mức HIGH)
#define RELAY_ACTIVE_LOW        1

// Danh sách các chân xuất tín hiệu điều khiển Quạt
const int FAN_PINS[] = { 5, 4, 14, 12, 13 }; // Tương ứng D1, D2, D5, D6, D7 trên NodeMCU
const int NUM_PINS = sizeof(FAN_PINS) / sizeof(FAN_PINS[0]);

#define PIN_STATUS_LED          2       // LED xanh trên ESP-12E (GPIO 2 / D4, Active LOW)

#define AUTO_CYCLE_ON_TIME_MS   5000    // Bật 5 giây
#define AUTO_CYCLE_OFF_TIME_MS  5000    // Tắt 5 giây

bool isAutoMode = true;
bool fanState = false;
unsigned long lastToggleTime = 0;

void setFanOutput(bool enable) {
    fanState = enable;
    
    // Xuất tín hiệu ra các chân: Nếu RELAY_ACTIVE_LOW thì BẬT = LOW, TẮT = HIGH
    int level = enable ? (RELAY_ACTIVE_LOW ? LOW : HIGH) : (RELAY_ACTIVE_LOW ? HIGH : LOW);
    for (int i = 0; i < NUM_PINS; i++) {
        digitalWrite(FAN_PINS[i], level);
    }

    // Đèn LED tích hợp (Active LOW: LOW là SÁNG, HIGH là TẮT)
    digitalWrite(PIN_STATUS_LED, enable ? LOW : HIGH);
}

void printStatus() {
    Serial.println(F("--------------------------------------------------"));
    if (fanState) {
#if RELAY_ACTIVE_LOW
        Serial.println(F("🟢 [TRẠNG THÁI] QUẠT: ĐANG BẬT (Relay 5V Active LOW: Mức 0V / LOW -> ĐÓNG RƠ-LE)"));
        Serial.println(F("👉 Tín hiệu LOW (0V) đang xuất trên: D1, D2, D5, D6, D7"));
#else
        Serial.println(F("🟢 [TRẠNG THÁI] QUẠT: ĐANG BẬT (Active HIGH: Mức 3.3V / HIGH)"));
        Serial.println(F("👉 Tín hiệu HIGH (3.3V) đang xuất trên: D1, D2, D5, D6, D7"));
#endif
        Serial.println(F("👉 Đèn LED xanh trên bo mạch: ĐANG SÁNG"));
    } else {
#if RELAY_ACTIVE_LOW
        Serial.println(F("🔴 [TRẠNG THÁI] QUẠT: ĐANG TẮT (Relay 5V Active LOW: Mức 3.3V / HIGH -> NGẮT RƠ-LE)"));
        Serial.println(F("👉 Tín hiệu HIGH (3.3V) đang xuất trên: D1, D2, D5, D6, D7"));
#else
        Serial.println(F("🔴 [TRẠNG THÁI] QUẠT: ĐANG TẮT (Active HIGH: Mức 0V / LOW)"));
        Serial.println(F("👉 Tín hiệu LOW (0V) đang xuất trên: D1, D2, D5, D6, D7"));
#endif
        Serial.println(F("👉 Đèn LED xanh trên bo mạch: ĐÃ TẮT"));
    }
    Serial.println(F("--------------------------------------------------"));
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Cấu hình toàn bộ chân xuất và tắt ban đầu
    int initialLevel = RELAY_ACTIVE_LOW ? HIGH : LOW;
    for (int i = 0; i < NUM_PINS; i++) {
        pinMode(FAN_PINS[i], OUTPUT);
        digitalWrite(FAN_PINS[i], initialLevel);
    }
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH);

    // Nhấp nháy LED 3 lần để báo hiệu bo mạch khởi động thành công
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_STATUS_LED, LOW);
        delay(100);
        digitalWrite(PIN_STATUS_LED, HIGH);
        delay(100);
    }

    Serial.println(F("\n========================================================"));
    Serial.println(F("🚀 ESP-12E / NodeMCU 5V Relay Fan Test Started!"));
    Serial.println(F("Các chân kích: D1 (GPIO5), D2 (GPIO4), D5 (GPIO14), D6 (GPIO12), D7 (GPIO13)"));
    Serial.println(F("Cấu hình: Module Relay 5V (Active LOW)"));
    Serial.println(F("Chế độ: TỰ ĐỘNG BẬT 5 GIÂY / TẮT 5 GIÂY TUẦN HOÀN"));
    Serial.println(F("========================================================"));

    // Khởi đầu bật quạt ngay lập tức để test
    setFanOutput(true);
    printStatus();
    lastToggleTime = millis();
}

void loop() {
    // Nhận lệnh Serial nếu có
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '1') {
            isAutoMode = false;
            setFanOutput(true);
            Serial.println(F(">> Nhận lệnh '1': BẬT QUẠT thủ công!"));
            printStatus();
        } else if (c == '0') {
            isAutoMode = false;
            setFanOutput(false);
            Serial.println(F(">> Nhận lệnh '0': TẮT QUẠT thủ công!"));
            printStatus();
        } else if (c == 'a' || c == 'A') {
            isAutoMode = true;
            lastToggleTime = millis();
            Serial.println(F(">> Chuyển về chế độ TỰ ĐỘNG 5s BẬT / 5s TẮT"));
        }
    }

    // Tự động chuyển đổi chu kỳ 5s
    if (isAutoMode) {
        unsigned long currentMillis = millis();
        unsigned long interval = fanState ? AUTO_CYCLE_ON_TIME_MS : AUTO_CYCLE_OFF_TIME_MS;

        if (currentMillis - lastToggleTime >= interval) {
            lastToggleTime = currentMillis;
            setFanOutput(!fanState);
            printStatus();
        }
    }

    delay(50);
}
