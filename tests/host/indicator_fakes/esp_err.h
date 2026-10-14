#pragma once
typedef int esp_err_t;
enum { ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_TIMEOUT = 0x107 };
const char *esp_err_to_name(esp_err_t error);
