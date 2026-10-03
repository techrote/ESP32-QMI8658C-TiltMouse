#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tiltmouse/qmi8658.h"

typedef enum {
    FAKE_READ,
    FAKE_WRITE,
} fake_operation_kind_t;

typedef struct {
    fake_operation_kind_t kind;
    uint8_t address;
    uint8_t reg;
    qmi8658_bus_status_t result;
    uint8_t data[24];
    size_t length;
    size_t transferred;
} fake_operation_t;

typedef struct {
    fake_operation_t operations[64];
    size_t operation_count;
    size_t next_operation;
    uint32_t delays[8];
    size_t delay_count;
    uint64_t now_us;
} fake_bus_t;

static void push_read(
    fake_bus_t *fake,
    uint8_t address,
    uint8_t reg,
    qmi8658_bus_status_t result,
    const uint8_t *data,
    size_t length,
    size_t transferred)
{
    assert(fake->operation_count < sizeof(fake->operations) / sizeof(fake->operations[0]));
    fake_operation_t *operation = &fake->operations[fake->operation_count++];
    *operation = (fake_operation_t){
        .kind = FAKE_READ,
        .address = address,
        .reg = reg,
        .result = result,
        .length = length,
        .transferred = transferred,
    };
    if (data != NULL && transferred > 0) {
        assert(transferred <= sizeof(operation->data));
        memcpy(operation->data, data, transferred);
    }
}

static void push_write(
    fake_bus_t *fake,
    uint8_t address,
    uint8_t reg,
    qmi8658_bus_status_t result,
    uint8_t value,
    size_t transferred)
{
    assert(fake->operation_count < sizeof(fake->operations) / sizeof(fake->operations[0]));
    fake_operation_t *operation = &fake->operations[fake->operation_count++];
    *operation = (fake_operation_t){
        .kind = FAKE_WRITE,
        .address = address,
        .reg = reg,
        .result = result,
        .data = {value},
        .length = 1,
        .transferred = transferred,
    };
}

static fake_operation_t *next_operation(
    fake_bus_t *fake,
    fake_operation_kind_t kind,
    uint8_t address,
    uint8_t reg,
    size_t length)
{
    assert(fake->next_operation < fake->operation_count);
    fake_operation_t *operation = &fake->operations[fake->next_operation++];
    assert(operation->kind == kind);
    assert(operation->address == address);
    assert(operation->reg == reg);
    assert(operation->length == length);
    return operation;
}

static qmi8658_bus_status_t fake_read(
    void *context,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length,
    size_t *transferred)
{
    fake_bus_t *fake = context;
    fake_operation_t *operation = next_operation(fake, FAKE_READ, address, reg, length);
    *transferred = operation->transferred;
    if (operation->transferred > 0) {
        memcpy(data, operation->data, operation->transferred);
    }
    return operation->result;
}

static qmi8658_bus_status_t fake_write(
    void *context,
    uint8_t address,
    uint8_t reg,
    const uint8_t *data,
    size_t length,
    size_t *transferred)
{
    fake_bus_t *fake = context;
    fake_operation_t *operation = next_operation(fake, FAKE_WRITE, address, reg, length);
    assert(memcmp(data, operation->data, length) == 0);
    *transferred = operation->transferred;
    return operation->result;
}

static void fake_delay_ms(void *context, uint32_t delay_ms)
{
    fake_bus_t *fake = context;
    assert(fake->delay_count < sizeof(fake->delays) / sizeof(fake->delays[0]));
    fake->delays[fake->delay_count++] = delay_ms;
}

static uint64_t fake_now_us(void *context)
{
    const fake_bus_t *fake = context;
    return fake->now_us;
}

static qmi8658_bus_t make_bus(fake_bus_t *fake)
{
    return (qmi8658_bus_t){
        .context = fake,
        .read = fake_read,
        .write = fake_write,
        .delay_ms = fake_delay_ms,
        .now_us = fake_now_us,
    };
}

static void push_successful_configuration(fake_bus_t *fake, uint8_t address, bool fallback)
{
    static const uint8_t identity = QMI8658_WHO_AM_I_VALUE;
    if (fallback) {
        push_read(fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x00, QMI8658_BUS_NACK, NULL, 1, 0);
    }
    push_read(fake, address, 0x00, QMI8658_BUS_OK, &identity, 1, 1);
    push_write(fake, address, 0x60, QMI8658_BUS_OK, 0xB0, 1);
    push_read(fake, address, 0x00, QMI8658_BUS_OK, &identity, 1, 1);
    push_write(fake, address, 0x02, QMI8658_BUS_OK, 0x40, 1);
    push_write(fake, address, 0x08, QMI8658_BUS_OK, 0x00, 1);
    push_write(fake, address, 0x03, QMI8658_BUS_OK, 0x15, 1);
    push_write(fake, address, 0x04, QMI8658_BUS_OK, 0x55, 1);
    push_write(fake, address, 0x06, QMI8658_BUS_OK, 0x00, 1);
    push_write(fake, address, 0x08, QMI8658_BUS_OK, 0x03, 1);
}

static qmi8658_device_t initialize(fake_bus_t *fake, uint8_t address, bool fallback)
{
    push_successful_configuration(fake, address, fallback);
    const qmi8658_bus_t bus = make_bus(fake);
    qmi8658_device_t device;
    assert(qmi8658_init(&device, &bus) == QMI8658_OK);
    assert(device.initialized);
    assert(device.address == address);
    assert(fake->delay_count == 2);
    assert(fake->delays[0] == 200);
    assert(fake->delays[1] == 80);
    assert(fake->next_operation == fake->operation_count);
    return device;
}

static void test_primary_identity_and_configuration(void)
{
    fake_bus_t fake = {0};
    (void)initialize(&fake, QMI8658_I2C_ADDRESS_PRIMARY, false);
}

static void test_alternate_address_fallback(void)
{
    fake_bus_t fake = {0};
    (void)initialize(&fake, QMI8658_I2C_ADDRESS_ALTERNATE, true);
}

static void test_sample_decode_and_scaling(void)
{
    fake_bus_t fake = {.now_us = UINT64_C(1234567)};
    qmi8658_device_t device = initialize(&fake, QMI8658_I2C_ADDRESS_PRIMARY, false);

    const uint8_t status0 = 0x03;
    push_read(&fake, device.address, 0x2E, QMI8658_BUS_OK, &status0, 1, 1);

    const uint8_t raw[17] = {
        0x03, 0x02, 0x01,
        0x00, 0x00,
        0x00, 0x20,
        0x00, 0xF0,
        0x00, 0x40,
        0x40, 0x00,
        0x80, 0xFF,
        0x80, 0x0C,
    };
    push_read(&fake, device.address, 0x30, QMI8658_BUS_OK, raw, sizeof(raw), sizeof(raw));

    qmi8658_sample_t sample = {0};
    assert(qmi8658_read_sample(&device, &sample) == QMI8658_OK);
    assert(sample.host_time_us == UINT64_C(1234567));
    assert(sample.sensor_timestamp == UINT32_C(0x010203));
    assert(sample.accel_g[0] == 1.0f);
    assert(sample.accel_g[1] == -0.5f);
    assert(sample.accel_g[2] == 2.0f);
    assert(sample.gyro_dps[0] == 1.0f);
    assert(sample.gyro_dps[1] == -2.0f);
    assert(sample.gyro_dps[2] == 50.0f);
    assert(fake.next_operation == fake.operation_count);
}

static void test_probe_and_identity_failures(void)
{
    {
        fake_bus_t fake = {0};
        push_read(&fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x00, QMI8658_BUS_TIMEOUT, NULL, 1, 0);
        const qmi8658_bus_t bus = make_bus(&fake);
        qmi8658_device_t device;
        assert(qmi8658_init(&device, &bus) == QMI8658_ERR_TIMEOUT);
    }

    {
        fake_bus_t fake = {0};
        const uint8_t wrong_identity = 0x99;
        push_read(
            &fake,
            QMI8658_I2C_ADDRESS_PRIMARY,
            0x00,
            QMI8658_BUS_OK,
            &wrong_identity,
            1,
            1);
        push_read(&fake, QMI8658_I2C_ADDRESS_ALTERNATE, 0x00, QMI8658_BUS_NACK, NULL, 1, 0);
        const qmi8658_bus_t bus = make_bus(&fake);
        qmi8658_device_t device;
        assert(qmi8658_init(&device, &bus) == QMI8658_ERR_IDENTITY_MISMATCH);
    }

    {
        fake_bus_t fake = {0};
        push_read(&fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x00, QMI8658_BUS_NACK, NULL, 1, 0);
        push_read(&fake, QMI8658_I2C_ADDRESS_ALTERNATE, 0x00, QMI8658_BUS_NACK, NULL, 1, 0);
        const qmi8658_bus_t bus = make_bus(&fake);
        qmi8658_device_t device;
        assert(qmi8658_init(&device, &bus) == QMI8658_ERR_NOT_FOUND);
    }
}

static void test_initialization_write_failures(void)
{
    {
        fake_bus_t fake = {0};
        static const uint8_t identity = QMI8658_WHO_AM_I_VALUE;
        push_read(
            &fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x00, QMI8658_BUS_OK, &identity, 1, 1);
        push_write(&fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x60, QMI8658_BUS_OK, 0xB0, 0);
        const qmi8658_bus_t bus = make_bus(&fake);
        qmi8658_device_t device;
        assert(qmi8658_init(&device, &bus) == QMI8658_ERR_SHORT_WRITE);
        assert(!device.initialized);
    }

    {
        fake_bus_t fake = {0};
        static const uint8_t identity = QMI8658_WHO_AM_I_VALUE;
        push_read(
            &fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x00, QMI8658_BUS_OK, &identity, 1, 1);
        push_write(&fake, QMI8658_I2C_ADDRESS_PRIMARY, 0x60, QMI8658_BUS_IO, 0xB0, 0);
        const qmi8658_bus_t bus = make_bus(&fake);
        qmi8658_device_t device;
        assert(qmi8658_init(&device, &bus) == QMI8658_ERR_IO);
        assert(!device.initialized);
    }
}

static void test_sample_failures_do_not_modify_output(void)
{
    fake_bus_t fake = {0};
    qmi8658_device_t device = initialize(&fake, QMI8658_I2C_ADDRESS_PRIMARY, false);

    qmi8658_sample_t sample = {.host_time_us = UINT64_C(0xA5A5A5A5)};
    const qmi8658_sample_t sentinel = sample;

    const uint8_t status0 = 0x03;
    push_read(&fake, device.address, 0x2E, QMI8658_BUS_OK, &status0, 1, 1);
    const uint8_t partial[17] = {0};
    push_read(&fake, device.address, 0x30, QMI8658_BUS_OK, partial, sizeof(partial), 8);
    assert(qmi8658_read_sample(&device, &sample) == QMI8658_ERR_SHORT_READ);
    assert(memcmp(&sample, &sentinel, sizeof(sample)) == 0);

    push_read(&fake, device.address, 0x2E, QMI8658_BUS_NACK, NULL, 1, 0);
    assert(qmi8658_read_sample(&device, &sample) == QMI8658_ERR_NACK);
    assert(memcmp(&sample, &sentinel, sizeof(sample)) == 0);

    push_read(&fake, device.address, 0x2E, QMI8658_BUS_TIMEOUT, NULL, 1, 0);
    assert(qmi8658_read_sample(&device, &sample) == QMI8658_ERR_TIMEOUT);
    assert(memcmp(&sample, &sentinel, sizeof(sample)) == 0);

    const uint8_t no_data = 0x01;
    push_read(&fake, device.address, 0x2E, QMI8658_BUS_OK, &no_data, 1, 1);
    assert(qmi8658_read_sample(&device, &sample) == QMI8658_NO_DATA);
    assert(memcmp(&sample, &sentinel, sizeof(sample)) == 0);
}

int main(void)
{
    test_primary_identity_and_configuration();
    test_alternate_address_fallback();
    test_sample_decode_and_scaling();
    test_probe_and_identity_failures();
    test_initialization_write_failures();
    test_sample_failures_do_not_modify_output();
    return 0;
}
