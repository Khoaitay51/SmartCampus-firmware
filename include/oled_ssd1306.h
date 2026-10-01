#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void oled_init(void);

// Man hinh khoi dong / tien trinh boot he thong
void oled_display_boot(const char *node_name, const char *step_text, const char *sub_text);

// Man hinh chinh danh cho Node Phong (Room Node 1, Room Node 2, Full Node)
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
                       const char *card_status);

// Man hinh chinh danh cho Node Hanh lang (Corridor Node)
void oled_display_corridor(const char *node_name,
                          bool wifi_ok,
                          bool mqtt_ok,
                          const char *card_uid,
                          const char *status_str,
                          const char *user_info,
                          const char *extra_info);

// Ham tuong thich nguoc (legacy support)
void oled_display_status(const char *room_mode, float temp, float hum, const char *card_uid, bool door_locked, int occ_count, int ir_in, int ir_out);
void oled_display_corridor_legacy(const char *card_uid, const char *status_str, const char *user_info);
