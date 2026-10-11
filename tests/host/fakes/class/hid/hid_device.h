#ifndef TEST_FAKE_TINYUSB_HID_DEVICE_H
#define TEST_FAKE_TINYUSB_HID_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HID_REPORT_TYPE_INVALID = 0,
    HID_REPORT_TYPE_INPUT = 1,
    HID_REPORT_TYPE_OUTPUT = 2,
    HID_REPORT_TYPE_FEATURE = 3,
} hid_report_type_t;
enum { HID_ITF_PROTOCOL_MOUSE = 2 };
enum { TUD_CONFIG_DESC_LEN = 9, TUD_HID_DESC_LEN = 9 };

/* Test shell records the actual production HID report-descriptor length. */
#define TUD_CONFIG_DESCRIPTOR(...) 0x09, 0x02
#define TUD_HID_DESCRIPTOR(itf, str, protocol, report_len, ep, ep_size, poll) \
    0x09, 0x21, (uint8_t)((report_len) & 0xffu), \
    (uint8_t)(((report_len) >> 8) & 0xffu)

bool tud_mounted(void);
bool tud_hid_ready(void);
bool tud_hid_report(uint8_t report_id, const void *report, uint16_t len);

#endif
