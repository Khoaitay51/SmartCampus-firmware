#include "app_mqtt.h"
#include "app_config.h"
#include "app_nvs.h"
#include "app_wifi.h"
#include "rgb_led.h"
#include "dht22.h"
#include "mq_sensor.h"
#include "servo_door.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
#include "ir_occupancy.h"
#include "rc522.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <string.h>

static const char *TAG = "APP_MQTT";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static char s_mac_address[18] = {0};
static char s_room_id[40] = {0};
static char s_device_id[40] = {0};
static char s_last_mode[24] = "SAVING";
static char s_last_card[32] = "";
static float s_last_temp = 25.0f;
static float s_last_hum = 60.0f;

bool app_mqtt_is_connected(void) {
    return s_mqtt_connected;
}

static void subscribe_room_topics(const char *room_id) {
    if (!s_mqtt_client || !room_id || strlen(room_id) == 0) return;
    char topic[128];

    snprintf(topic, sizeof(topic), TOPIC_ROOM_STATE, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "Subscribed to %s", topic);

    snprintf(topic, sizeof(topic), TOPIC_ROOM_COMMAND, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "Subscribed to %s", topic);
}

static void send_provision_request(void) {
    char payload_str[256];
    snprintf(payload_str, sizeof(payload_str),
        "{\"message_id\":\"msg-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
        "\"payload\":{\"mac_address\":\"%s\",\"firmware_version\":\"1.0.0\","
        "\"hardware_revision\":\"ESP32-WROOM-32D\",\"device_type\":\"ESP32_NODE\"}}",
        esp_timer_get_time() / 1000, s_mac_address);

    esp_mqtt_client_publish(s_mqtt_client, TOPIC_PROVISION_REQUEST, payload_str, 0, 1, 0);
    ESP_LOGI(TAG, "Sent Provisioning Request for MAC: %s", s_mac_address);
}

static void send_command_ack(const char *command_id, bool success, const char *err_msg) {
    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_COMMAND_ACK, s_mac_address, command_id);

    char payload_str[256];
    snprintf(payload_str, sizeof(payload_str),
        "{\"message_id\":\"ack-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
        "\"payload\":{\"mac_address\":\"%s\",\"command_id\":\"%s\",\"success\":%s,\"error_message\":\"%s\"}}",
        esp_timer_get_time() / 1000, s_mac_address, command_id, success ? "true" : "false", err_msg ? err_msg : "");

    esp_mqtt_client_publish(s_mqtt_client, topic, payload_str, 0, 1, 0);
    ESP_LOGI(TAG, "Sent ACK for command: %s", command_id);
}

// Callback khi có người bước qua cửa (FR-SD-02)
static void on_occupancy_detected(occupancy_dir_t dir) {
    if (!s_mqtt_connected || strlen(s_room_id) == 0) return;

    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_OCC, s_room_id);

    const char *dir_str = (dir == OCCUPANCY_IN) ? "IN" : "OUT";
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"message_id\":\"occ-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
        "\"payload\":{\"room_id\":\"%s\",\"occupancy_type\":\"%s\"}}",
        esp_timer_get_time() / 1000, s_room_id, dir_str);

    esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
    buzzer_play(BUZZER_PATTERN_SHORT); // Beep nhẹ báo nhận diện
}

// Callback khi quẹt thẻ RFID (FR-SD-04)
static void on_rfid_card_scanned(const char *card_uid) {
    strncpy(s_last_card, card_uid, sizeof(s_last_card) - 1);
    oled_display_status(s_last_mode, s_last_temp, s_last_hum, s_last_card, servo_door_is_locked());
    buzzer_play(BUZZER_PATTERN_SHORT);

    if (!s_mqtt_connected || strlen(s_room_id) == 0) return;

    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_ROOM_EVENT_RFID, s_room_id);

    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"message_id\":\"rfid-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
        "\"payload\":{\"room_id\":\"%s\",\"card_uid\":\"%s\",\"event_type\":\"check_in\"}}",
        esp_timer_get_time() / 1000, s_room_id, card_uid);

    esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published RFID card tap: %s", card_uid);
}

static void handle_mqtt_message(const char *topic, int topic_len, const char *data, int data_len) {
    char t[128] = {0};
    if (topic_len < sizeof(t)) strncpy(t, topic, topic_len);

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (!root) return;

    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!payload) payload = root;

    char prov_resp_topic[128];
    snprintf(prov_resp_topic, sizeof(prov_resp_topic), TOPIC_PROVISION_RESPONSE, s_mac_address);

    // 1. Phản hồi Auto-Provisioning
    if (strcmp(t, prov_resp_topic) == 0) {
        cJSON *j_room = cJSON_GetObjectItem(payload, "room_id");
        cJSON *j_dev = cJSON_GetObjectItem(payload, "device_id");
        if (j_room && j_room->valuestring && strlen(j_room->valuestring) > 0) {
            strncpy(s_room_id, j_room->valuestring, sizeof(s_room_id) - 1);
            app_nvs_set_room_id(s_room_id);
            subscribe_room_topics(s_room_id);
            ESP_LOGI(TAG, "Provisioned Room ID: %s", s_room_id);
        }
        if (j_dev && j_dev->valuestring) {
            strncpy(s_device_id, j_dev->valuestring, sizeof(s_device_id) - 1);
            app_nvs_set_device_id(s_device_id);
        }
    }
    // 2. Cập nhật Room State FSM
    else if (strstr(t, "/state") != NULL) {
        cJSON *j_mode = cJSON_GetObjectItem(payload, "room_mode");
        if (j_mode && j_mode->valuestring) {
            strncpy(s_last_mode, j_mode->valuestring, sizeof(s_last_mode) - 1);
            rgb_led_set_room_mode(s_last_mode);

            // Xử lý tự động theo FSM:
            if (strcasecmp(s_last_mode, "emergency") == 0) {
                servo_door_set_locked(false); // Mở cửa ngay khi khẩn cấp (FR-AC-02)
                buzzer_play(BUZZER_PATTERN_EMERGENCY);
            } else if (strcasecmp(s_last_mode, "exam") == 0) {
                servo_door_set_locked(true);  // Khóa cửa khi bắt đầu thi
                buzzer_play(BUZZER_PATTERN_DOUBLE);
            } else {
                buzzer_play(BUZZER_PATTERN_OFF);
            }

            oled_display_status(s_last_mode, s_last_temp, s_last_hum, s_last_card, servo_door_is_locked());
        }
    }
    // 3. Nhận lệnh Room Command (door, buzzer, led_strip...)
    else if (strstr(t, "/command/room/") != NULL) {
        cJSON *j_cmd = cJSON_GetObjectItem(payload, "command_type");
        cJSON *j_val = cJSON_GetObjectItem(payload, "command_value");

        if (j_cmd && j_cmd->valuestring) {
            if (strcmp(j_cmd->valuestring, "door") == 0 && j_val && j_val->valuestring) {
                bool lock = (strcasecmp(j_val->valuestring, "locked") == 0);
                servo_door_set_locked(lock);
                buzzer_play(BUZZER_PATTERN_SHORT);
            } else if (strcmp(j_cmd->valuestring, "buzzer") == 0 && j_val && j_val->valuestring) {
                bool on = (strcasecmp(j_val->valuestring, "on") == 0);
                buzzer_set_state(on);
            } else if (strcmp(j_cmd->valuestring, "led_strip") == 0) {
                cJSON *j_col = cJSON_GetObjectItem(payload, "color");
                cJSON *j_eff = cJSON_GetObjectItem(payload, "effect");
                cJSON *j_br = cJSON_GetObjectItem(payload, "brightness");
                const char *col = j_col ? j_col->valuestring : "#FFFFFF";
                const char *eff_str = j_eff ? j_eff->valuestring : "static";
                uint8_t br = j_br ? (uint8_t)j_br->valueint : 255;
                led_effect_t eff = LED_EFFECT_STATIC;
                if (eff_str && strcmp(eff_str, "breathe") == 0) eff = LED_EFFECT_BREATHE;
                else if (eff_str && strcmp(eff_str, "strobe") == 0) eff = LED_EFFECT_STROBE;
                rgb_led_set_hex(col, eff, br);
            }
            oled_display_status(s_last_mode, s_last_temp, s_last_hum, s_last_card, servo_door_is_locked());
        }
    }
    // 4. Nhận lệnh Device Command (restart...)
    else if (strstr(t, "/command/device/") != NULL) {
        cJSON *j_cmd = cJSON_GetObjectItem(payload, "command_type");
        cJSON *j_cid = cJSON_GetObjectItem(payload, "command_id");
        const char *cmd_id = j_cid ? j_cid->valuestring : "unknown";

        if (j_cmd && j_cmd->valuestring && strcmp(j_cmd->valuestring, "restart") == 0) {
            send_command_ack(cmd_id, true, NULL);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        }
    }

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT Broker");
            s_mqtt_connected = true;

            char prov_topic[128];
            snprintf(prov_topic, sizeof(prov_topic), TOPIC_PROVISION_RESPONSE, s_mac_address);
            esp_mqtt_client_subscribe(s_mqtt_client, prov_topic, 1);

            char dev_cmd_topic[128];
            snprintf(dev_cmd_topic, sizeof(dev_cmd_topic), TOPIC_DEVICE_COMMAND, s_mac_address);
            esp_mqtt_client_subscribe(s_mqtt_client, dev_cmd_topic, 1);

            if (strlen(s_room_id) > 0) {
                subscribe_room_topics(s_room_id);
            }

            send_provision_request();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT Broker");
            s_mqtt_connected = false;
            break;

        case MQTT_EVENT_DATA:
            handle_mqtt_message(event->topic, event->topic_len, event->data, event->data_len);
            break;

        default:
            break;
    }
}

static void heartbeat_task(void *pvParameters) {
    uint32_t uptime_sec = 0;
    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_DEVICE_HEARTBEAT, s_mac_address);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_SEC * 1000));
        uptime_sec += HEARTBEAT_INTERVAL_SEC;

        if (s_mqtt_connected) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                "{\"message_id\":\"hb-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
                "\"payload\":{\"device_id\":\"%s\",\"alive\":true,\"firmware_version\":\"1.0.0\",\"uptime\":%lu}}",
                esp_timer_get_time() / 1000,
                strlen(s_device_id) > 0 ? s_device_id : s_mac_address,
                (unsigned long)uptime_sec);

            esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        }
    }
}

// Task đo lường & gửi dữ liệu môi trường (DHT22 + MQ-2 + MQ-135)
static void telemetry_task(void *pvParameters) {
    dht22_data_t dht;
    mq_data_t mq;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_INTERVAL_MS));

        if (dht22_read(&dht) == ESP_OK) {
            s_last_temp = dht.temperature;
            s_last_hum = dht.humidity;
        }
        mq_sensor_read(&mq);

        oled_display_status(s_last_mode, s_last_temp, s_last_hum, s_last_card, servo_door_is_locked());

        if (s_mqtt_connected && strlen(s_room_id) > 0) {
            char topic[128];
            snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_ENV, s_room_id);

            char payload[384];
            snprintf(payload, sizeof(payload),
                "{\"message_id\":\"env-%lld\",\"source_timestamp\":\"2026-09-21T00:00:00Z\","
                "\"payload\":{\"room_id\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,"
                "\"smoke_detected\":%s,\"smoke_value\":%.1f,\"smoke_threshold\":1400.0,"
                "\"smoke_state\":\"%s\",\"co2\":%d,\"air_quality\":%d}}",
                esp_timer_get_time() / 1000, s_room_id,
                s_last_temp, s_last_hum,
                mq.smoke_detected ? "true" : "false",
                mq.smoke_raw,
                mq.smoke_state,
                mq.co2_ppm,
                mq.air_quality);

            esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        }
    }
}

void app_mqtt_start(void) {
    app_wifi_get_mac_str(s_mac_address);
    if (app_nvs_get_room_id(s_room_id, sizeof(s_room_id)) == ESP_OK) {
        ESP_LOGI(TAG, "Loaded room_id from NVS: %s", s_room_id);
    } else {
        strncpy(s_room_id, DEFAULT_ROOM_ID, sizeof(s_room_id) - 1);
        ESP_LOGI(TAG, "Using default room_id: %s", s_room_id);
    }
    if (app_nvs_get_device_id(s_device_id, sizeof(s_device_id)) == ESP_OK) {
        ESP_LOGI(TAG, "Loaded device_id from NVS: %s", s_device_id);
    }

    char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), TOPIC_DEVICE_STATUS, s_mac_address);
    static char lwt_msg[256];
    snprintf(lwt_msg, sizeof(lwt_msg),
        "{\"message_id\":\"lwt\",\"payload\":{\"mac_address\":\"%s\",\"device_status\":\"offline\",\"reason\":\"lwt_disconnect\"}}",
        s_mac_address);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.last_will = {
            .topic = lwt_topic,
            .msg = lwt_msg,
            .msg_len = strlen(lwt_msg),
            .qos = 1,
            .retain = 1,
        }
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    // Kích hoạt các sensors ngoại vi
    ir_occupancy_init(on_occupancy_detected);
    rc522_init(on_rfid_card_scanned);

    xTaskCreate(heartbeat_task, "heartbeat_task", 3072, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telemetry_task", 3072, NULL, 5, NULL);
}
