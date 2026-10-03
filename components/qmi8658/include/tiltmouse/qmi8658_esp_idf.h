#pragma once

#include "esp_err.h"
#include "tiltmouse/qmi8658.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create the ESP32-S3 I2C master adapter used by qmi8658.
 *
 * The adapter owns I2C port 0, GPIO11/GPIO12, and two device handles for the
 * QMI8658C's possible 7-bit addresses. Call qmi8658_esp_idf_bus_deinit() when
 * the bus is no longer required.
 */
esp_err_t qmi8658_esp_idf_bus_init(qmi8658_bus_t *bus);

/** Release resources allocated by qmi8658_esp_idf_bus_init(). */
void qmi8658_esp_idf_bus_deinit(qmi8658_bus_t *bus);

#ifdef __cplusplus
}
#endif
