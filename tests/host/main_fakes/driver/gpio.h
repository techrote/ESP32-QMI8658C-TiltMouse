#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    uint64_t pin_bit_mask;
    int mode, pull_up_en, pull_down_en, intr_type;
} gpio_config_t;
enum { GPIO_MODE_INPUT, GPIO_PULLUP_ENABLE, GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE };
esp_err_t gpio_config(const gpio_config_t *config);
int gpio_get_level(int pin);
