#ifndef TILTMOUSE_USB_HID_MOUSE_REPORT_H
#define TILTMOUSE_USB_HID_MOUSE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_report.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TILTMOUSE_USB_HID_MOUSE_BUTTON_LEFT = 1u << 0,
    TILTMOUSE_USB_HID_MOUSE_BUTTON_RIGHT = 1u << 1,
};

typedef struct {
    uint8_t buttons;
    int8_t x;
    int8_t y;
} tiltmouse_usb_hid_mouse_report_t;

uint8_t tiltmouse_usb_hid_mouse_button_mask(bool left_pressed, bool right_pressed);

tiltmouse_usb_hid_mouse_report_t tiltmouse_usb_hid_mouse_make_report(
    bool left_pressed,
    bool right_pressed,
    int8_t x,
    int8_t y);

tiltmouse_usb_hid_mouse_report_t tiltmouse_usb_hid_mouse_release_report(void);

/* Reject invalid input and clear output to a safe zero report. */
bool tiltmouse_usb_hid_mouse_from_logical(
    const tm_mouse_report_t *logical,
    tiltmouse_usb_hid_mouse_report_t *out);

#ifdef __cplusplus
}
#endif

#endif
