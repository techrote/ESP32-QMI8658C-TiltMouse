#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t submitted;
    uint32_t busy_skips;
    esp_err_t last_error;
    bool running;
} tm_sensor_indicator_status_t;

/* Qualification only. Single app-task caller, no runtime reinitialization. */
esp_err_t tm_sensor_indicator_init(void);
void tm_sensor_indicator_update(float x_deg, float y_deg, bool ready,
                                uint64_t now_us);
tm_sensor_indicator_status_t tm_sensor_indicator_status(void);
