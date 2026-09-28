#pragma once
#include <stdbool.h>

typedef enum {
    OCCUPANCY_NONE = 0,
    OCCUPANCY_IN,
    OCCUPANCY_OUT
} occupancy_dir_t;

typedef void (*occupancy_cb_t)(occupancy_dir_t dir);
typedef void (*ir_state_cb_t)(int in_level, int out_level);

void ir_occupancy_init(occupancy_cb_t callback, ir_state_cb_t state_cb);
int ir_get_in_level(void);
int ir_get_out_level(void);
