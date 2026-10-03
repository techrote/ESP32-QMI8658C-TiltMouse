#include <stdint.h>

#include "driver/gpio.h"
#include "tiltmouse/board.h"

esp_err_t tiltmouse_board_init_safe(void)
{
    const gpio_config_t rgb_data_config = {
        .pin_bit_mask = UINT64_C(1) << TILTMOUSE_GPIO_RGB_DATA,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&rgb_data_config);
    if (err != ESP_OK) {
        return err;
    }

    return gpio_set_level((gpio_num_t)TILTMOUSE_GPIO_RGB_DATA, 0);
}
