#pragma once
#include <stdbool.h>

void oled_init(void);
void oled_display_status(const char *room_mode, float temp, float hum, const char *card_uid, bool door_locked, int occ_count, int ir_in, int ir_out);
void oled_display_corridor(const char *card_uid, const char *status_str, const char *user_info);
