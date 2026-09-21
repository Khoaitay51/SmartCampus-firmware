#pragma once
#include <stdbool.h>

typedef enum {
    OCCUPANCY_NONE = 0,
    OCCUPANCY_IN,
    OCCUPANCY_OUT
} occupancy_dir_t;

typedef void (*occupancy_cb_t)(occupancy_dir_t dir);

void ir_occupancy_init(occupancy_cb_t callback);
