#include "tiltmouse/qmi8658.h"

#include <string.h>

enum {
    QMI8658_REG_WHO_AM_I = 0x00,
    QMI8658_REG_CTRL1 = 0x02,
    QMI8658_REG_CTRL2 = 0x03,
    QMI8658_REG_CTRL3 = 0x04,
    QMI8658_REG_CTRL5 = 0x06,
    QMI8658_REG_CTRL7 = 0x08,
    QMI8658_REG_STATUS0 = 0x2E,
    QMI8658_REG_TIMESTAMP_L = 0x30,
    QMI8658_REG_RESET = 0x60,
};

enum {
    QMI8658_CTRL1_AUTO_INCREMENT_LITTLE_ENDIAN = 0x40,
    QMI8658_CTRL2_4G_235HZ = 0x15,
    QMI8658_CTRL3_512DPS_235HZ = 0x55,
    QMI8658_CTRL5_FILTERS_DISABLED = 0x00,
    QMI8658_CTRL7_SENSORS_DISABLED = 0x00,
    QMI8658_CTRL7_ACCEL_GYRO_ENABLED = 0x03,
    QMI8658_RESET_COMMAND = 0xB0,
    QMI8658_STATUS0_ACCEL_DATA_AVAILABLE = 0x01,
    QMI8658_STATUS0_GYRO_DATA_AVAILABLE = 0x02,
    QMI8658_SAMPLE_BLOCK_LENGTH = 17,
    QMI8658_RESET_WAIT_MS = 200,
    QMI8658_SENSOR_ENABLE_WAIT_MS = 80,
};

static qmi8658_status_t map_bus_status(qmi8658_bus_status_t status)
{
    switch (status) {
    case QMI8658_BUS_OK:
        return QMI8658_OK;
    case QMI8658_BUS_NACK:
        return QMI8658_ERR_NACK;
    case QMI8658_BUS_TIMEOUT:
        return QMI8658_ERR_TIMEOUT;
    case QMI8658_BUS_IO:
    default:
        return QMI8658_ERR_IO;
    }
}

static qmi8658_status_t read_bytes(
    const qmi8658_bus_t *bus,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length)
{
    size_t transferred = 0;
    const qmi8658_bus_status_t bus_status =
        bus->read(bus->context, address, reg, data, length, &transferred);
    if (bus_status != QMI8658_BUS_OK) {
        return map_bus_status(bus_status);
    }
    if (transferred != length) {
        return QMI8658_ERR_SHORT_READ;
    }
    return QMI8658_OK;
}

static qmi8658_status_t write_bytes(
    const qmi8658_bus_t *bus,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length)
{
    size_t transferred = 0;
    const qmi8658_bus_status_t bus_status =
        bus->write(bus->context, address, reg, data, length, &transferred);
    if (bus_status != QMI8658_BUS_OK) {
        return map_bus_status(bus_status);
    }
    if (transferred != length) {
        return QMI8658_ERR_SHORT_WRITE;
    }
    return QMI8658_OK;
}

static qmi8658_status_t read_identity(const qmi8658_bus_t *bus, uint8_t address)
{
    uint8_t identity = 0;
    const qmi8658_status_t status =
        read_bytes(bus, address, QMI8658_REG_WHO_AM_I, &identity, sizeof(identity));
    if (status != QMI8658_OK) {
        return status;
    }
    if (identity != QMI8658_WHO_AM_I_VALUE) {
        return QMI8658_ERR_IDENTITY_MISMATCH;
    }
    return QMI8658_OK;
}

static qmi8658_status_t select_address(const qmi8658_bus_t *bus, uint8_t *address)
{
    const qmi8658_status_t primary = read_identity(bus, QMI8658_I2C_ADDRESS_PRIMARY);
    if (primary == QMI8658_OK) {
        *address = QMI8658_I2C_ADDRESS_PRIMARY;
        return QMI8658_OK;
    }
    if (primary != QMI8658_ERR_NACK && primary != QMI8658_ERR_IDENTITY_MISMATCH) {
        return primary;
    }

    const qmi8658_status_t alternate = read_identity(bus, QMI8658_I2C_ADDRESS_ALTERNATE);
    if (alternate == QMI8658_OK) {
        *address = QMI8658_I2C_ADDRESS_ALTERNATE;
        return QMI8658_OK;
    }
    if (alternate != QMI8658_ERR_NACK && alternate != QMI8658_ERR_IDENTITY_MISMATCH) {
        return alternate;
    }
    if (primary == QMI8658_ERR_IDENTITY_MISMATCH ||
        alternate == QMI8658_ERR_IDENTITY_MISMATCH) {
        return QMI8658_ERR_IDENTITY_MISMATCH;
    }
    return QMI8658_ERR_NOT_FOUND;
}

static qmi8658_status_t write_register(
    const qmi8658_bus_t *bus,
    uint8_t address,
    uint8_t reg,
    uint8_t value)
{
    return write_bytes(bus, address, reg, &value, sizeof(value));
}

qmi8658_status_t qmi8658_init(qmi8658_device_t *device, const qmi8658_bus_t *bus)
{
    if (device == NULL || bus == NULL || bus->read == NULL || bus->write == NULL ||
        bus->delay_ms == NULL || bus->now_us == NULL) {
        return QMI8658_ERR_INVALID_ARGUMENT;
    }

    memset(device, 0, sizeof(*device));
    device->bus = *bus;

    qmi8658_status_t status = select_address(bus, &device->address);
    if (status != QMI8658_OK) {
        return status;
    }

    status = write_register(bus, device->address, QMI8658_REG_RESET, QMI8658_RESET_COMMAND);
    if (status != QMI8658_OK) {
        return status;
    }
    bus->delay_ms(bus->context, QMI8658_RESET_WAIT_MS);

    status = read_identity(bus, device->address);
    if (status != QMI8658_OK) {
        return status;
    }

    static const struct {
        uint8_t reg;
        uint8_t value;
    } configuration[] = {
        {QMI8658_REG_CTRL1, QMI8658_CTRL1_AUTO_INCREMENT_LITTLE_ENDIAN},
        {QMI8658_REG_CTRL7, QMI8658_CTRL7_SENSORS_DISABLED},
        {QMI8658_REG_CTRL2, QMI8658_CTRL2_4G_235HZ},
        {QMI8658_REG_CTRL3, QMI8658_CTRL3_512DPS_235HZ},
        {QMI8658_REG_CTRL5, QMI8658_CTRL5_FILTERS_DISABLED},
        {QMI8658_REG_CTRL7, QMI8658_CTRL7_ACCEL_GYRO_ENABLED},
    };

    for (size_t i = 0; i < sizeof(configuration) / sizeof(configuration[0]); ++i) {
        status = write_register(
            bus, device->address, configuration[i].reg, configuration[i].value);
        if (status != QMI8658_OK) {
            return status;
        }
    }

    bus->delay_ms(bus->context, QMI8658_SENSOR_ENABLE_WAIT_MS);
    device->initialized = true;
    return QMI8658_OK;
}

static int16_t decode_i16_le(const uint8_t *bytes)
{
    const uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return (int16_t)value;
}

qmi8658_status_t qmi8658_read_sample(qmi8658_device_t *device, qmi8658_sample_t *sample)
{
    if (device == NULL || sample == NULL) {
        return QMI8658_ERR_INVALID_ARGUMENT;
    }
    if (!device->initialized) {
        return QMI8658_ERR_NOT_INITIALIZED;
    }

    uint8_t status0 = 0;
    qmi8658_status_t status = read_bytes(
        &device->bus, device->address, QMI8658_REG_STATUS0, &status0, sizeof(status0));
    if (status != QMI8658_OK) {
        return status;
    }

    const uint8_t required_data =
        QMI8658_STATUS0_ACCEL_DATA_AVAILABLE | QMI8658_STATUS0_GYRO_DATA_AVAILABLE;
    if ((status0 & required_data) != required_data) {
        return QMI8658_NO_DATA;
    }

    uint8_t raw[QMI8658_SAMPLE_BLOCK_LENGTH] = {0};
    status = read_bytes(
        &device->bus,
        device->address,
        QMI8658_REG_TIMESTAMP_L,
        raw,
        sizeof(raw));
    if (status != QMI8658_OK) {
        return status;
    }

    qmi8658_sample_t decoded = {0};
    decoded.host_time_us = device->bus.now_us(device->bus.context);
    decoded.sensor_timestamp =
        (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16);

    for (size_t axis = 0; axis < 3; ++axis) {
        const int16_t accel_raw = decode_i16_le(&raw[5 + axis * 2]);
        const int16_t gyro_raw = decode_i16_le(&raw[11 + axis * 2]);
        decoded.accel_g[axis] = (float)accel_raw / QMI8658_ACCEL_LSB_PER_G;
        decoded.gyro_dps[axis] = (float)gyro_raw / QMI8658_GYRO_LSB_PER_DPS;
    }

    *sample = decoded;
    return QMI8658_OK;
}

const char *qmi8658_status_string(qmi8658_status_t status)
{
    switch (status) {
    case QMI8658_OK:
        return "ok";
    case QMI8658_NO_DATA:
        return "no_data";
    case QMI8658_ERR_INVALID_ARGUMENT:
        return "invalid_argument";
    case QMI8658_ERR_NOT_INITIALIZED:
        return "not_initialized";
    case QMI8658_ERR_NOT_FOUND:
        return "not_found";
    case QMI8658_ERR_IDENTITY_MISMATCH:
        return "identity_mismatch";
    case QMI8658_ERR_NACK:
        return "nack";
    case QMI8658_ERR_TIMEOUT:
        return "timeout";
    case QMI8658_ERR_SHORT_READ:
        return "short_read";
    case QMI8658_ERR_SHORT_WRITE:
        return "short_write";
    case QMI8658_ERR_IO:
    default:
        return "io_error";
    }
}
