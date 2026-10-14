#pragma once
void test_indicator_log(const char *tag, const char *format, ...);
#define ESP_LOGE(...) test_indicator_log(__VA_ARGS__)
