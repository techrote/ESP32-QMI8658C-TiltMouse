#pragma once
#include <assert.h>
#include "../fakes/esp_err.h"
#define ESP_ERROR_CHECK(expression) assert((expression) == ESP_OK)
