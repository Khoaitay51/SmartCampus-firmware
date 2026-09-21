#pragma once
#include <stdbool.h>

void servo_door_init(void);
void servo_door_set_locked(bool locked);
bool servo_door_is_locked(void);
