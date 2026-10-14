#pragma once
#include "freertos/FreeRTOS.h"
TaskHandle_t xTaskGetCurrentTaskHandle(void);
void xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(int clear, uint32_t timeout);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task);
