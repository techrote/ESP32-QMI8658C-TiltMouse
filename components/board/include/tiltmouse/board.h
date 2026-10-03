#pragma once

#include "esp_err.h"
#include "tiltmouse/board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Apply the minimal safe board state owned by the foundation layer.
 *
 * TM-001 deliberately configures only the RGB matrix data pin, driving GPIO14
 * low so no LED data stream is emitted. USB, IMU, touch, BOOT, Wi-Fi, and
 * Bluetooth resources are left untouched for their owning tasks.
 */
esp_err_t tiltmouse_board_init_safe(void);

#ifdef __cplusplus
}
#endif
