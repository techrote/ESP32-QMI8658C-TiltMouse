#ifndef TEST_FAKE_TINYUSB_H
#define TEST_FAKE_TINYUSB_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    TINYUSB_EVENT_ATTACHED = 1,
    TINYUSB_EVENT_DETACHED,
    TINYUSB_EVENT_SUSPENDED,
    TINYUSB_EVENT_RESUMED,
} tinyusb_event_id_t;
typedef struct {
    tinyusb_event_id_t id;
} tinyusb_event_t;
typedef void (*tinyusb_event_callback_t)(tinyusb_event_t *event, void *arg);
typedef struct {
    tinyusb_event_callback_t event_cb;
    void *event_arg;
    struct {
        const void *device;
        const uint8_t *full_speed_config;
        const uint8_t *high_speed_config;
        const char **string;
        size_t string_count;
    } descriptor;
} tinyusb_config_t;

esp_err_t tinyusb_driver_install(const tinyusb_config_t *config);

#endif
