#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QMI8658_I2C_ADDRESS_PRIMARY UINT8_C(0x6A)
#define QMI8658_I2C_ADDRESS_ALTERNATE UINT8_C(0x6B)
#define QMI8658_WHO_AM_I_VALUE UINT8_C(0x05)

#define QMI8658_ACCEL_LSB_PER_G 8192.0f
#define QMI8658_GYRO_LSB_PER_DPS 64.0f
#define QMI8658_NOMINAL_ODR_HZ 235.0f

typedef enum {
    QMI8658_BUS_OK = 0,
    QMI8658_BUS_NACK,
    QMI8658_BUS_TIMEOUT,
    QMI8658_BUS_IO,
} qmi8658_bus_status_t;

typedef qmi8658_bus_status_t (*qmi8658_bus_read_fn)(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length,
    size_t *transferred);

typedef qmi8658_bus_status_t (*qmi8658_bus_write_fn)(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length,
    size_t *transferred);

typedef void (*qmi8658_bus_delay_ms_fn)(void *context, uint32_t delay_ms);
typedef uint64_t (*qmi8658_bus_now_us_fn)(void *context);

typedef struct {
    void *context;
    qmi8658_bus_read_fn read;
    qmi8658_bus_write_fn write;
    qmi8658_bus_delay_ms_fn delay_ms;
    qmi8658_bus_now_us_fn now_us;
} qmi8658_bus_t;

typedef enum {
    QMI8658_OK = 0,
    QMI8658_NO_DATA,
    QMI8658_ERR_INVALID_ARGUMENT,
    QMI8658_ERR_NOT_INITIALIZED,
    QMI8658_ERR_NOT_FOUND,
    QMI8658_ERR_IDENTITY_MISMATCH,
    QMI8658_ERR_NACK,
    QMI8658_ERR_TIMEOUT,
    QMI8658_ERR_SHORT_READ,
    QMI8658_ERR_SHORT_WRITE,
    QMI8658_ERR_IO,
} qmi8658_status_t;

/** One coherent QMI8658C accelerometer/gyroscope acquisition.
 *
 * accel_g and gyro_dps are ordered X, Y, Z in the sensor's native
 * right-handed frame. host_time_us is captured only after a successful burst.
 * sensor_timestamp contains the zero-extended 24-bit circular sensor count.
 */
typedef struct {
    uint64_t host_time_us;
    uint32_t sensor_timestamp;
    float accel_g[3];
    float gyro_dps[3];
} qmi8658_sample_t;

typedef struct {
    qmi8658_bus_t bus;
    uint8_t address;
    bool initialized;
} qmi8658_device_t;

/** Probe, reset, identity-check, and configure the QMI8658C. */
qmi8658_status_t qmi8658_init(qmi8658_device_t *device, const qmi8658_bus_t *bus);

/**
 * Acquire the latest complete accel+gyro sample when both data-ready bits are set.
 *
 * On every non-QMI8658_OK result the caller-provided sample is unchanged.
 */
qmi8658_status_t qmi8658_read_sample(qmi8658_device_t *device, qmi8658_sample_t *sample);
const char *qmi8658_status_string(qmi8658_status_t status);

#ifdef __cplusplus
}
#endif
