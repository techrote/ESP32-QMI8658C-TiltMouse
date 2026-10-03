#include "esp_err.h"
#include "esp_log.h"
#include "tiltmouse/board.h"

static const char *TAG = "tiltmouse";

void app_main(void)
{
    ESP_ERROR_CHECK(tiltmouse_board_init_safe());
    ESP_LOGI(TAG, "TiltMouse foundation initialized; feature subsystems are disabled");
}
