#ifndef TILTMOUSE_USB_HID_MOUSE_H
#define TILTMOUSE_USB_HID_MOUSE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "tiltmouse/mouse_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t tiltmouse_usb_hid_mouse_init(void);

bool tiltmouse_usb_hid_mouse_is_mounted(void);

bool tiltmouse_usb_hid_mouse_is_suspended(void);

esp_err_t tiltmouse_usb_hid_mouse_send(
    bool left_pressed,
    bool right_pressed,
    int8_t x,
    int8_t y);

esp_err_t tiltmouse_usb_hid_mouse_release_all(void);

/* Application-facing logical-report USB adapter, for TM-006 integration.
 * Call from one serialized owner; service drains asynchronous completions. */
tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_publish(
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us);
tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_service(void);

#ifdef __cplusplus
}
#endif

#endif
