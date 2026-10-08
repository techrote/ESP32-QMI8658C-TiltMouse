#ifndef FAKE_HID_DEVICE_H
#define FAKE_HID_DEVICE_H
#include <stdbool.h>
#include <stdint.h>
#define TUD_CONFIG_DESC_LEN 9
#define TUD_HID_DESC_LEN 25
#define HID_ITF_PROTOCOL_MOUSE 2
/* Config macros are inert stubs; this probe does not validate enumeration. */
#define TUD_CONFIG_DESCRIPTOR(...) 0
#define TUD_HID_DESCRIPTOR(...) 0
typedef int hid_report_type_t;
bool tud_mounted(void);
bool tud_hid_ready(void);
bool tud_hid_report(uint8_t report_id, const void *report, uint16_t size);
#endif
