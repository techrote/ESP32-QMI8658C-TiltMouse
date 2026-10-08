#ifndef FAKE_TINYUSB_H
#define FAKE_TINYUSB_H
#include "esp_err.h"
enum { TINYUSB_EVENT_ATTACHED, TINYUSB_EVENT_DETACHED,
       TINYUSB_EVENT_SUSPENDED, TINYUSB_EVENT_RESUMED };
typedef struct { int id; } tinyusb_event_t;
typedef struct {
    void (*event_cb)(tinyusb_event_t *, void *);
    void *event_arg;
    struct { const void *device, *full_speed_config; const char **string;
             unsigned string_count; } descriptor;
} tinyusb_config_t;
esp_err_t tinyusb_driver_install(const tinyusb_config_t *config);
#endif
