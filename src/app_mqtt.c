#include "app_mqtt.h"
#include "app_config.h"
#include "app_nvs.h"
#include "app_sntp.h"
#include "app_wifi.h"
#include "rgb_led.h"
#include "dht22.h"
#include "mq_sensor.h"
#include "servo_door.h"
#include "buzzer.h"
#include "oled_ssd1306.h"
#include "ir_occupancy.h"
#include "rc522.h"
#include "fan_control.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <string.h>
#include <stdatomic.h>

static const char *TAG = "APP_MQTT";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static char s_mac_address[18] = {0};
static char s_room_id[40] = DEFAULT_ROOM_ID;
static char s_device_id[40] = {0};
static char s_last_mode[24] = "SAVING";
static char s_last_card[32] = "";
static char s_card_status[32] = "SAN SANG";
#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
static char s_corridor_info[32] = "QUET THE DANG KY";
#endif
static float s_last_temp = 25.0f;
static float s_last_hum = 60.0f;
#if CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
static float s_last_smoke = 0.0f;
static bool s_smoke_alarm = false;
#endif
static _Atomic int s_occupancy_count = 0;

bool app_mqtt_is_connected(void) {
    return s_mqtt_connected;
}

static void app_mqtt_refresh_display(void) {
    bool wifi_ok = app_wifi_is_connected();
    bool mqtt_ok = s_mqtt_connected;

#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
    char ip_str[24];
    app_wifi_get_ip_str(ip_str, sizeof(ip_str));
    oled_display_corridor(NODE_ROLE_NAME, wifi_ok, mqtt_ok,
                          s_last_card, s_card_status, s_corridor_info,
                          wifi_ok ? ip_str : s_mac_address);
#elif CURRENT_NODE_ROLE == ROLE_OLED_DISPLAY_NODE
    oled_display_room(NODE_ROLE_NAME, s_last_mode, wifi_ok, mqtt_ok,
                      s_last_temp, s_last_hum,
                      fan_control_is_on(), fan_control_get_speed(),
                      servo_door_is_locked(), s_occupancy_count,
                      1, 1, s_last_smoke, s_smoke_alarm,
                      s_last_card, s_card_status);
#else
    oled_display_room(NODE_ROLE_NAME, s_last_mode, wifi_ok, mqtt_ok,
                      s_last_temp, s_last_hum,
                      fan_control_is_on(), fan_control_get_speed(),
                      servo_door_is_locked(), s_occupancy_count,
                      ir_get_in_level(), ir_get_out_level(),
                      s_last_smoke, s_smoke_alarm,
                      s_last_card, s_card_status);
#endif
}

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
static void on_ir_state_changed(int in_level, int out_level) {
    app_mqtt_refresh_display();
}
#endif

static void subscribe_room_topics(const char *room_id) {
    if (!s_mqtt_client || !room_id || strlen(room_id) == 0) return;
    char topic[128];

    snprintf(topic, sizeof(topic), TOPIC_ROOM_STATE, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "Subscribed to %s", topic);

    snprintf(topic, sizeof(topic), TOPIC_ROOM_COMMAND, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "Subscribed to %s", topic);

#if CURRENT_NODE_ROLE == ROLE_OLED_DISPLAY_NODE
    // Node OLED dong bo Occupancy tu Node IR qua MQTT
    snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_OCC, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "OLED Node subscribed to Occupancy: %s", topic);

    // Node OLED dong bo Nhiet do & Do am tu Node IR qua MQTT
    snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_ENV, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "OLED Node subscribed to Environment: %s", topic);

    // Node OLED dong bo The RFID quet tu Node IR qua MQTT
    snprintf(topic, sizeof(topic), TOPIC_ROOM_EVENT_RFID, room_id);
    esp_mqtt_client_subscribe(s_mqtt_client, topic, 1);
    ESP_LOGI(TAG, "OLED Node subscribed to RFID: %s", topic);
#elif CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
    // Node Corridor dang ky topic nhan phan hoi dang ky the
    char reg_topic[128];
    snprintf(reg_topic, sizeof(reg_topic), TOPIC_CARD_REG_RESPONSE, s_mac_address);
    esp_mqtt_client_subscribe(s_mqtt_client, reg_topic, 1);
    ESP_LOGI(TAG, "Corridor Node subscribed to Registration Response: %s", reg_topic);
#endif
}

static void send_provision_request(void) {
    char ts[30];
    app_sntp_get_iso8601(ts, sizeof(ts));

    char payload_str[320];
    snprintf(payload_str, sizeof(payload_str),
        "{\"message_id\":\"msg-%lld\",\"source_timestamp\":\"%s\","
        "\"payload\":{\"mac_address\":\"%s\",\"firmware_version\":\"1.0.0\","
        "\"hardware_revision\":\"ESP32-WROOM-32D\",\"device_type\":\"ESP32_NODE\"}}",
        esp_timer_get_time() / 1000, ts, s_mac_address);

    esp_mqtt_client_publish(s_mqtt_client, TOPIC_PROVISION_REQUEST, payload_str, 0, 1, 0);
    ESP_LOGI(TAG, "Sent Provisioning Request for MAC: %s", s_mac_address);
}

static void send_command_ack(const char *command_id, bool success, const char *err_msg) {
    char topic[128];
    snprintf(topic, sizeof(topic), TOPIC_COMMAND_ACK, s_mac_address, command_id);

    char ts[30];
    app_sntp_get_iso8601(ts, sizeof(ts));

    char payload_str[320];
    snprintf(payload_str, sizeof(payload_str),
        "{\"message_id\":\"ack-%lld\",\"source_timestamp\":\"%s\","
        "\"payload\":{\"mac_address\":\"%s\",\"command_id\":\"%s\","
        "\"success\":%s,\"error_message\":\"%s\"}}",
        esp_timer_get_time() / 1000, ts, s_mac_address, command_id, success ? "true" : "false", err_msg ? err_msg : "");

    esp_mqtt_client_publish(s_mqtt_client, topic, payload_str, 0, 1, 0);
    ESP_LOGI(TAG, "Sent ACK for command: %s", command_id);
}

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
// Callback khi co nguoi buoc qua cua (FR-SD-02)
static void on_occupancy_detected(occupancy_dir_t dir) {
    if (dir == OCCUPANCY_IN) {
        s_occupancy_count++;
        ESP_LOGI(TAG, ">>> Occupancy IN (+1). Room count: %d", s_occupancy_count);
    } else if (dir == OCCUPANCY_OUT) {
        if (s_occupancy_count > 0) s_occupancy_count--;
        ESP_LOGI(TAG, ">>> Occupancy OUT (-1). Room count: %d", s_occupancy_count);
    }

    buzzer_play(BUZZER_PATTERN_DOUBLE);

    // QUAN TRONG: Publish MQTT TRUOC khi refresh OLED.
    if (s_mqtt_connected && strlen(s_room_id) > 0) {
        char topic[128];
        snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_OCC, s_room_id);

        char ts[30];
        app_sntp_get_iso8601(ts, sizeof(ts));

        const char *dir_str = (dir == OCCUPANCY_IN) ? "IN" : "OUT";
        char payload[320];
        snprintf(payload, sizeof(payload),
            "{\"message_id\":\"occ-%lld\",\"source_timestamp\":\"%s\","
            "\"payload\":{\"room_id\":\"%s\",\"occupancy_type\":\"%s\",\"occupancy_count\":%d}}",
            esp_timer_get_time() / 1000, ts, s_room_id, dir_str, s_occupancy_count);

        esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published Occupancy %s to topic %s", dir_str, topic);
    } else {
        ESP_LOGW(TAG, "MQTT not connected or room_id empty, skipping occupancy publish");
    }

    app_mqtt_refresh_display();
}
#endif

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE
// Callback khi quet the RFID (FR-SD-04)
void app_mqtt_on_rfid_card_scanned(const char *card_uid) {
    static char s_prev_card[32] = {0};
    static int64_t s_prev_tap_ms = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;

    // Chong double-tap / spam su kien cho cung 1 the trong vong 2.5 giay
    if (strcmp(card_uid, s_prev_card) == 0 && (now_ms - s_prev_tap_ms < 2500)) {
        ESP_LOGW(TAG, "Ignoring duplicate card tap within cooldown: %s", card_uid);
        return;
    }
    strncpy(s_prev_card, card_uid, sizeof(s_prev_card) - 1);
    s_prev_tap_ms = now_ms;

    strncpy(s_last_card, card_uid, sizeof(s_last_card) - 1);

#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
    // Node Hanh lang: Quet the gui yeu cau dang ky / luu DB pending neu chua dang ky
    buzzer_play(BUZZER_PATTERN_DOUBLE);
    rgb_led_set_color(40, 20, 0); // Vang cam: Dang cho duyet tren DB

    strncpy(s_card_status, "CHO DUYET (PENDING)", sizeof(s_card_status) - 1);
    strncpy(s_corridor_info, "DANG GUI REQUEST...", sizeof(s_corridor_info) - 1);
    app_mqtt_refresh_display();

    if (s_mqtt_connected) {
        char ts[30];
        app_sntp_get_iso8601(ts, sizeof(ts));

        char payload[384];
        snprintf(payload, sizeof(payload),
            "{\"message_id\":\"reg-%lld\",\"source_timestamp\":\"%s\","
            "\"payload\":{\"mac_address\":\"%s\",\"card_uid\":\"%s\","
            "\"room_id\":\"%s\",\"status\":\"pending\",\"node_role\":\"corridor\","
            "\"note\":\"Tapped at Corridor RFID Station\"}}",
            esp_timer_get_time() / 1000, ts, s_mac_address, card_uid, s_room_id);

        esp_mqtt_client_publish(s_mqtt_client, TOPIC_CARD_REG_REQUEST, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published Card Registration Request: UID=%s to %s", card_uid, TOPIC_CARD_REG_REQUEST);
    } else {
        ESP_LOGW(TAG, "Card scanned (UID: %s), but MQTT not connected -> registration request queued/skipped", card_uid);
    }
#else
    buzzer_play(BUZZER_PATTERN_SHORT);
    strncpy(s_card_status, "CHECK-IN OK", sizeof(s_card_status) - 1);

    if (s_mqtt_connected && strlen(s_room_id) > 0) {
        char topic[128];
        snprintf(topic, sizeof(topic), TOPIC_ROOM_EVENT_RFID, s_room_id);

        char ts[30];
        app_sntp_get_iso8601(ts, sizeof(ts));

        char payload[320];
        snprintf(payload, sizeof(payload),
            "{\"message_id\":\"rfid-%lld\",\"source_timestamp\":\"%s\","
            "\"payload\":{\"room_id\":\"%s\",\"card_uid\":\"%s\",\"event_type\":\"check_in\"}}",
            esp_timer_get_time() / 1000, ts, s_room_id, card_uid);

        esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        ESP_LOGI(TAG, "Published RFID card tap: %s", card_uid);
    } else {
        ESP_LOGW(TAG, "Card scanned (UID: %s), but MQTT not connected -> registered locally on OLED & Buzzer", card_uid);
    }

    app_mqtt_refresh_display();
#endif
}
#endif

static void handle_mqtt_message(const char *topic, int topic_len, const char *data, int data_len) {
    char t[256] = {0};
    int copy_len = (topic_len < (int)sizeof(t) - 1) ? topic_len : (int)sizeof(t) - 1;
    memcpy(t, topic, copy_len);

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (!root) return;

    cJSON *payload = cJSON_GetObjectItem(root, "payload");
    if (!payload) payload = root;

    char prov_resp_topic[128];
    snprintf(prov_resp_topic, sizeof(prov_resp_topic), TOPIC_PROVISION_RESPONSE, s_mac_address);

    // 1. Phan hoi Auto-Provisioning
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
    // 2. Nhan cap nhat Room State (topic ket thuc bang "/state")
    else if (strlen(t) >= 6 && strcmp(t + strlen(t) - 6, "/state") == 0) {
        cJSON *j_mode = cJSON_GetObjectItem(payload, "room_mode");
        if (j_mode && j_mode->valuestring) {
            strncpy(s_last_mode, j_mode->valuestring, sizeof(s_last_mode) - 1);
            ESP_LOGI(TAG, "Room Mode changed to: %s", s_last_mode);

            // Cap nhat mau LED theo room mode
            rgb_led_set_room_mode(s_last_mode);

            if (strcasecmp(s_last_mode, "emergency") == 0) {
                servo_door_set_locked(false);
                fan_control_set_state(false); // Tat quat de chong thoi lan toa khoi
                buzzer_play(BUZZER_PATTERN_EMERGENCY);
            } else if (strcasecmp(s_last_mode, "exam") == 0 || strcasecmp(s_last_mode, "lock") == 0) {
                servo_door_set_locked(true);
                buzzer_play(BUZZER_PATTERN_DOUBLE);
            } else if (strcasecmp(s_last_mode, "lecture") == 0 || strcasecmp(s_last_mode, "self_study") == 0 || strcasecmp(s_last_mode, "self-study") == 0) {
                servo_door_set_locked(false);
                buzzer_play(BUZZER_PATTERN_OFF);
            } else if (strcasecmp(s_last_mode, "saving") == 0) {
                servo_door_set_locked(true);
                fan_control_set_state(false); // Tat quat khi tiet kiem dien
                buzzer_play(BUZZER_PATTERN_OFF);
            } else {
                buzzer_play(BUZZER_PATTERN_OFF);
            }

            app_mqtt_refresh_display();
        }
    }
    // 3. Nhan lenh Room Command (door, buzzer, led_strip...)
    else if (strstr(t, "/command/room/") != NULL) {
        cJSON *j_cmd = cJSON_GetObjectItem(payload, "command_type");
        cJSON *j_val = cJSON_GetObjectItem(payload, "command_value");

        if (j_cmd && j_cmd->valuestring) {
            if (strcmp(j_cmd->valuestring, "door") == 0 && j_val && j_val->valuestring) {
                bool lock = (strcasecmp(j_val->valuestring, "locked") == 0);
                servo_door_set_locked(lock);
                buzzer_play(BUZZER_PATTERN_SHORT);
            } else if (strcmp(j_cmd->valuestring, "fan") == 0 && j_val) {
                if (cJSON_IsNumber(j_val)) {
                    fan_control_set_speed((uint8_t)j_val->valueint);
                } else if (j_val->valuestring) {
                    bool on = (strcasecmp(j_val->valuestring, "on") == 0 ||
                               strcmp(j_val->valuestring, "1") == 0 ||
                               strcasecmp(j_val->valuestring, "true") == 0);
                    fan_control_set_state(on);
                }
                buzzer_play(BUZZER_PATTERN_SHORT);
                ESP_LOGI(TAG, "Fan command: %s (%d%%)", 
                         fan_control_is_on() ? "ON" : "OFF", fan_control_get_speed());
            } else if (strcmp(j_cmd->valuestring, "buzzer") == 0 && j_val && j_val->valuestring) {
                const char *val = j_val->valuestring;
                if (strcasecmp(val, "off") == 0 || strcmp(val, "0") == 0 || strcasecmp(val, "false") == 0) {
                    buzzer_play(BUZZER_PATTERN_OFF);
                } else if (strcasecmp(val, "emergency") == 0 || strcasecmp(val, "alarm") == 0) {
                    buzzer_play(BUZZER_PATTERN_EMERGENCY);
                } else if (strcasecmp(val, "double") == 0) {
                    buzzer_play(BUZZER_PATTERN_DOUBLE);
                } else if (strcasecmp(val, "long") == 0 || strcasecmp(val, "reject") == 0) {
                    buzzer_play(BUZZER_PATTERN_LONG);
                } else if (strcasecmp(val, "on") == 0 || strcmp(val, "1") == 0 || strcasecmp(val, "true") == 0) {
                    if (strcasecmp(s_last_mode, "emergency") == 0) {
                        buzzer_play(BUZZER_PATTERN_EMERGENCY);
                    } else {
                        buzzer_play(BUZZER_PATTERN_SHORT);
                    }
                } else {
                    buzzer_play(BUZZER_PATTERN_SHORT);
                }
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
            app_mqtt_refresh_display();
        }
    }
    // 4. Nhan lenh Device Command (restart...)
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
    // 5. Nhan du lieu Occupancy (Dong bo hien thi cho Node OLED)
    else if (strstr(t, "/telemetry/room/") != NULL && strstr(t, "/occupancy") != NULL) {
        cJSON *j_cnt = cJSON_GetObjectItem(payload, "occupancy_count");
        cJSON *j_type = cJSON_GetObjectItem(payload, "occupancy_type");
        if (j_cnt) {
            s_occupancy_count = j_cnt->valueint;
        } else if (j_type && j_type->valuestring) {
            if (strcasecmp(j_type->valuestring, "IN") == 0) s_occupancy_count++;
            else if (strcasecmp(j_type->valuestring, "OUT") == 0 && s_occupancy_count > 0) s_occupancy_count--;
        }
        ESP_LOGI(TAG, "Sync Occupancy from MQTT -> Room Count: %d", s_occupancy_count);
        buzzer_play(BUZZER_PATTERN_DOUBLE);
        app_mqtt_refresh_display();
    }
    // 6. Nhan du lieu Moi truong (Dong bo hien thi cho Node OLED)
    else if (strstr(t, "/telemetry/room/") != NULL && strstr(t, "/environment") != NULL) {
        cJSON *j_t = cJSON_GetObjectItem(payload, "temperature");
        cJSON *j_h = cJSON_GetObjectItem(payload, "humidity");
        if (j_t) s_last_temp = (float)j_t->valuedouble;
        if (j_h) s_last_hum = (float)j_h->valuedouble;
        app_mqtt_refresh_display();
    }
    // 7. Nhan su kien RFID tap (Dong bo the vua quet cho Node OLED)
    else if (strstr(t, "/rfid") != NULL) {
        cJSON *j_card = cJSON_GetObjectItem(payload, "card_uid");
        if (j_card && j_card->valuestring) {
            strncpy(s_last_card, j_card->valuestring, sizeof(s_last_card) - 1);
            buzzer_play(BUZZER_PATTERN_SHORT);
            app_mqtt_refresh_display();
        }
    }
    // 8. Nhan phan hoi dang ky the RFID (Duyet the tai hanh lang)
    else if (strstr(t, "/card/registration/response/") != NULL) {
        cJSON *j_card = cJSON_GetObjectItem(payload, "card_uid");
        cJSON *j_stat = cJSON_GetObjectItem(payload, "status");
        cJSON *j_name = cJSON_GetObjectItem(payload, "assigned_user_name");
        cJSON *j_msg = cJSON_GetObjectItem(payload, "message");

        const char *card = (j_card && j_card->valuestring) ? j_card->valuestring : s_last_card;
        const char *stat = (j_stat && j_stat->valuestring) ? j_stat->valuestring : "pending";
        const char *info = (j_name && j_name->valuestring) ? j_name->valuestring : ((j_msg && j_msg->valuestring) ? j_msg->valuestring : "");

        ESP_LOGI(TAG, "Registration Response: Card=%s, Status=%s, Info=%s", card, stat, info);

#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
        if (strcasecmp(stat, "approved") == 0 || strcasecmp(stat, "registered") == 0) {
            strncpy(s_card_status, "DA DUYET / OK", sizeof(s_card_status) - 1);
            strncpy(s_corridor_info, info, sizeof(s_corridor_info) - 1);
            rgb_led_set_color(0, 40, 0); // Xanh la: The da duoc gán & kích hoat
            buzzer_play(BUZZER_PATTERN_SHORT);
        } else if (strcasecmp(stat, "rejected") == 0) {
            strncpy(s_card_status, "TU CHOI", sizeof(s_card_status) - 1);
            strncpy(s_corridor_info, info, sizeof(s_corridor_info) - 1);
            rgb_led_set_color(40, 0, 0); // Do: The bi tu choi
            buzzer_play(BUZZER_PATTERN_LONG);
        } else { // pending
            strncpy(s_card_status, "CHO DUYET (PENDING)", sizeof(s_card_status) - 1);
            strncpy(s_corridor_info, info, sizeof(s_corridor_info) - 1);
            rgb_led_set_color(40, 20, 0); // Vang cam: Dang cho admin duyet
            buzzer_play(BUZZER_PATTERN_DOUBLE);
        }
        app_mqtt_refresh_display();
#endif
    }

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT Broker");
            s_mqtt_connected = true;
            app_mqtt_refresh_display();

            char prov_topic[128];
            snprintf(prov_topic, sizeof(prov_topic), TOPIC_PROVISION_RESPONSE, s_mac_address);
            esp_mqtt_client_subscribe(s_mqtt_client, prov_topic, 1);

            char dev_cmd_topic[128];
            snprintf(dev_cmd_topic, sizeof(dev_cmd_topic), TOPIC_DEVICE_COMMAND, s_mac_address);
            esp_mqtt_client_subscribe(s_mqtt_client, dev_cmd_topic, 1);

#if CURRENT_NODE_ROLE == ROLE_CORRIDOR_NODE
            char card_resp[128];
            snprintf(card_resp, sizeof(card_resp), TOPIC_CARD_REG_RESPONSE, s_mac_address);
            esp_mqtt_client_subscribe(s_mqtt_client, card_resp, 1);
            ESP_LOGI(TAG, "Corridor Node subscribed to: %s", card_resp);
#endif

            if (strlen(s_room_id) > 0) {
                subscribe_room_topics(s_room_id);
            }

            send_provision_request();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT Broker");
            s_mqtt_connected = false;
            app_mqtt_refresh_display();
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR: err_type=%d, sock_errno=%d",
                     event->error_handle ? event->error_handle->error_type : -1,
                     event->error_handle ? event->error_handle->esp_transport_sock_errno : -1);
            break;

        case MQTT_EVENT_DATA:
            handle_mqtt_message(event->topic, event->topic_len, event->data, event->data_len);
            break;

        default:
            ESP_LOGD(TAG, "MQTT event: %d", event_id);
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
            char ts[30];
            app_sntp_get_iso8601(ts, sizeof(ts));

            char payload[320];
            snprintf(payload, sizeof(payload),
                "{\"message_id\":\"hb-%lld\",\"source_timestamp\":\"%s\","
                "\"payload\":{\"device_id\":\"%s\",\"alive\":true,\"firmware_version\":\"1.0.0\",\"uptime\":%lu}}",
                esp_timer_get_time() / 1000, ts,
                strlen(s_device_id) > 0 ? s_device_id : s_mac_address,
                (unsigned long)uptime_sec);

            esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
        }
    }
}

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
// Task do luong va gui du lieu moi truong (DHT22 + MQ-2 + MQ-135)
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
        s_last_smoke = mq.smoke_raw;
        s_smoke_alarm = mq.smoke_detected;

        app_mqtt_refresh_display();

        if (s_mqtt_connected && strlen(s_room_id) > 0) {
            char topic[128];
            snprintf(topic, sizeof(topic), TOPIC_ROOM_TELEMETRY_ENV, s_room_id);

            char ts[30];
            app_sntp_get_iso8601(ts, sizeof(ts));

            char payload[448];
            snprintf(payload, sizeof(payload),
                "{\"message_id\":\"env-%lld\",\"source_timestamp\":\"%s\","
                "\"payload\":{\"room_id\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,"
                "\"smoke_detected\":%s,\"smoke_value\":%.1f,\"smoke_threshold\":1400.0,"
                "\"smoke_state\":\"%s\",\"co2\":%d,\"air_quality\":%d}}",
                esp_timer_get_time() / 1000, ts, s_room_id,
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
#endif

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

    // F1 fix: Doc MQTT credentials tu NVS (fallback ve DEFAULT_)
    static char mqtt_uri[128];
    static char mqtt_pass[64];
    app_nvs_get_mqtt_uri(mqtt_uri, sizeof(mqtt_uri));
    app_nvs_get_mqtt_pass(mqtt_pass, sizeof(mqtt_pass));
    ESP_LOGI(TAG, "MQTT broker URI: %s (from NVS/default)", mqtt_uri);

    static char lwt_topic[128];
    snprintf(lwt_topic, sizeof(lwt_topic), TOPIC_DEVICE_STATUS, s_mac_address);
    static char lwt_msg[256];
    snprintf(lwt_msg, sizeof(lwt_msg),
        "{\"message_id\":\"lwt\",\"payload\":{\"mac_address\":\"%s\",\"device_status\":\"offline\",\"reason\":\"lwt_disconnect\"}}",
        s_mac_address);

    static char client_id_str[64];
    snprintf(client_id_str, sizeof(client_id_str), "%s_%s", MQTT_CLIENT_PREFIX, s_mac_address);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_uri,
        .credentials.client_id = client_id_str,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = mqtt_pass,
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

#if CURRENT_NODE_ROLE != ROLE_OLED_DISPLAY_NODE && CURRENT_NODE_ROLE != ROLE_CORRIDOR_NODE
    // Kich hoat IR va telemetry tasks tren Node IR / Sensor
    ir_occupancy_init(on_occupancy_detected, on_ir_state_changed);
    xTaskCreate(telemetry_task, "telemetry_task", 3072, NULL, 5, NULL);
#endif

    xTaskCreate(heartbeat_task, "heartbeat_task", 3072, NULL, 5, NULL);
}
