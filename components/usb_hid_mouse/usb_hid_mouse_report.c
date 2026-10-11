#include "tiltmouse/usb_hid_mouse_report.h"

#include <stddef.h>

uint8_t tiltmouse_usb_hid_mouse_button_mask(bool left_pressed, bool right_pressed)
{
    uint8_t buttons = 0;

    if (left_pressed) {
        buttons |= TILTMOUSE_USB_HID_MOUSE_BUTTON_LEFT;
    }
    if (right_pressed) {
        buttons |= TILTMOUSE_USB_HID_MOUSE_BUTTON_RIGHT;
    }

    return buttons;
}

tiltmouse_usb_hid_mouse_report_t tiltmouse_usb_hid_mouse_make_report(
    bool left_pressed, bool right_pressed, int8_t x, int8_t y)
{
    const tiltmouse_usb_hid_mouse_report_t report = {
        .buttons = tiltmouse_usb_hid_mouse_button_mask(left_pressed, right_pressed),
        .x = x,
        .y = y,
    };

    return report;
}

tiltmouse_usb_hid_mouse_report_t tiltmouse_usb_hid_mouse_release_report(void)
{
    return tiltmouse_usb_hid_mouse_make_report(false, false, 0, 0);
}

bool tiltmouse_usb_hid_mouse_from_logical(
    const tm_mouse_report_t *logical, tiltmouse_usb_hid_mouse_report_t *hid)
{
    if (hid == NULL) {
        return false;
    }
    *hid = tiltmouse_usb_hid_mouse_release_report();
    if (!tm_mouse_report_valid(logical)) {
        return false;
    }
    if (logical->kind == TM_REPORT_CURRENT) {
        hid->buttons = logical->buttons;
        hid->x = logical->dx;
        hid->y = logical->dy;
    }
    return true;
}
