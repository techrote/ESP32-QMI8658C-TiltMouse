#include "esp_err.h"
#include "esp_log.h"
#include "tiltmouse/board.h"
#include "tiltmouse/usb_hid_mouse.h"

static const char *TAG = "tiltmouse";

void app_main(void)
{
    ESP_ERROR_CHECK(tiltmouse_board_init_safe());
    ESP_ERROR_CHECK(tiltmouse_usb_hid_mouse_init());
    ESP_LOGI(
        TAG,
        "Native USB HID mouse initialized; IMU and touch input remain disabled");
}
