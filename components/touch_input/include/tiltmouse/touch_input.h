#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/touch_sens.h"
#include "esp_err.h"
#include "tiltmouse/touch_channels.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t timestamp_us;
    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
} tiltmouse_touch_raw_sample_t;

typedef struct {
    touch_sensor_handle_t sensor;
    touch_channel_handle_t channels[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    bool enabled;
    bool scanning;
} tiltmouse_touch_input_t;

/**
 * Configure GPIO1..GPIO7 as independent touch channels and start continuous
 * raw-data scanning. No logical button thresholds are applied here.
 */
esp_err_t tiltmouse_touch_input_init(tiltmouse_touch_input_t *input);

/** Read one timestamped raw frame from all seven independent channels. */
esp_err_t tiltmouse_touch_input_read(
    tiltmouse_touch_input_t *input,
    tiltmouse_touch_raw_sample_t *sample);

/** Stop scanning and release all touch-driver resources. */
esp_err_t tiltmouse_touch_input_deinit(tiltmouse_touch_input_t *input);

#ifdef __cplusplus
}
#endif
