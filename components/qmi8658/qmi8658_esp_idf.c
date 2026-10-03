#include "tiltmouse/qmi8658_esp_idf.h"

#include <stdlib.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tiltmouse/board_pins.h"

enum {
    QMI8658_I2C_CLOCK_HZ = 400000,
    QMI8658_I2C_TIMEOUT_MS = 20,
    QMI8658_I2C_AUTO_INCREMENT = 0x80,
};

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t primary_device;
    i2c_master_dev_handle_t alternate_device;
} qmi8658_esp_idf_context_t;

static i2c_master_dev_handle_t device_for_address(
    const qmi8658_esp_idf_context_t *context,
    uint8_t address)
{
    if (address == QMI8658_I2C_ADDRESS_PRIMARY) {
        return context->primary_device;
    }
    if (address == QMI8658_I2C_ADDRESS_ALTERNATE) {
        return context->alternate_device;
    }
    return NULL;
}

static qmi8658_bus_status_t map_esp_error(esp_err_t error)
{
    if (error == ESP_OK) {
        return QMI8658_BUS_OK;
    }
    if (error == ESP_ERR_INVALID_RESPONSE) {
        return QMI8658_BUS_NACK;
    }
    if (error == ESP_ERR_TIMEOUT) {
        return QMI8658_BUS_TIMEOUT;
    }
    return QMI8658_BUS_IO;
}

static qmi8658_bus_status_t esp_idf_read(
    void *opaque_context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length,
    size_t *transferred)
{
    qmi8658_esp_idf_context_t *context = opaque_context;
    i2c_master_dev_handle_t device = device_for_address(context, address);
    if (device == NULL || data == NULL || transferred == NULL || length == 0) {
        return QMI8658_BUS_IO;
    }

    uint8_t register_address = reg;
    if (length > 1) {
        register_address |= QMI8658_I2C_AUTO_INCREMENT;
    }

    *transferred = 0;
    const esp_err_t error = i2c_master_transmit_receive(
        device,
        &register_address,
        sizeof(register_address),
        data,
        length,
        QMI8658_I2C_TIMEOUT_MS);
    if (error == ESP_OK) {
        *transferred = length;
    }
    return map_esp_error(error);
}

static qmi8658_bus_status_t esp_idf_write(
    void *opaque_context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length,
    size_t *transferred)
{
    qmi8658_esp_idf_context_t *context = opaque_context;
    i2c_master_dev_handle_t device = device_for_address(context, address);
    if (device == NULL || data == NULL || transferred == NULL || length == 0) {
        return QMI8658_BUS_IO;
    }

    uint8_t *transaction = malloc(length + 1);
    if (transaction == NULL) {
        return QMI8658_BUS_IO;
    }
    transaction[0] = reg;
    if (length > 1) {
        transaction[0] |= QMI8658_I2C_AUTO_INCREMENT;
    }
    memcpy(&transaction[1], data, length);

    *transferred = 0;
    const esp_err_t error = i2c_master_transmit(
        device, transaction, length + 1, QMI8658_I2C_TIMEOUT_MS);
    free(transaction);

    if (error == ESP_OK) {
        *transferred = length;
    }
    return map_esp_error(error);
}

static void esp_idf_delay_ms(void *context, uint32_t delay_ms)
{
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static uint64_t esp_idf_now_us(void *context)
{
    (void)context;
    return (uint64_t)esp_timer_get_time();
}

static esp_err_t add_device(
    i2c_master_bus_handle_t bus_handle,
    uint8_t address,
    i2c_master_dev_handle_t *device_handle)
{
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = QMI8658_I2C_CLOCK_HZ,
    };
    return i2c_master_bus_add_device(bus_handle, &device_config, device_handle);
}

esp_err_t qmi8658_esp_idf_bus_init(qmi8658_bus_t *bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(bus, 0, sizeof(*bus));

    qmi8658_esp_idf_context_t *context = calloc(1, sizeof(*context));
    if (context == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TILTMOUSE_GPIO_IMU_SDA,
        .scl_io_num = TILTMOUSE_GPIO_IMU_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t error = i2c_new_master_bus(&bus_config, &context->bus_handle);
    if (error != ESP_OK) {
        free(context);
        return error;
    }

    error = add_device(
        context->bus_handle, QMI8658_I2C_ADDRESS_PRIMARY, &context->primary_device);
    if (error != ESP_OK) {
        i2c_del_master_bus(context->bus_handle);
        free(context);
        return error;
    }

    error = add_device(
        context->bus_handle, QMI8658_I2C_ADDRESS_ALTERNATE, &context->alternate_device);
    if (error != ESP_OK) {
        i2c_master_bus_rm_device(context->primary_device);
        i2c_del_master_bus(context->bus_handle);
        free(context);
        return error;
    }

    *bus = (qmi8658_bus_t){
        .context = context,
        .read = esp_idf_read,
        .write = esp_idf_write,
        .delay_ms = esp_idf_delay_ms,
        .now_us = esp_idf_now_us,
    };
    return ESP_OK;
}

void qmi8658_esp_idf_bus_deinit(qmi8658_bus_t *bus)
{
    if (bus == NULL || bus->context == NULL) {
        return;
    }

    qmi8658_esp_idf_context_t *context = bus->context;
    if (context->alternate_device != NULL) {
        i2c_master_bus_rm_device(context->alternate_device);
    }
    if (context->primary_device != NULL) {
        i2c_master_bus_rm_device(context->primary_device);
    }
    if (context->bus_handle != NULL) {
        i2c_del_master_bus(context->bus_handle);
    }
    free(context);
    memset(bus, 0, sizeof(*bus));
}
