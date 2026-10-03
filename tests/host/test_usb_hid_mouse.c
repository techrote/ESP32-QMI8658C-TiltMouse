#include <assert.h>
#include <stdint.h>

#include "tiltmouse/usb_hid_mouse_report.h"

static void test_button_masks(void)
{
    assert(tiltmouse_usb_hid_mouse_button_mask(false, false) == 0u);
    assert(tiltmouse_usb_hid_mouse_button_mask(true, false) ==
           TILTMOUSE_USB_HID_MOUSE_BUTTON_LEFT);
    assert(tiltmouse_usb_hid_mouse_button_mask(false, true) ==
           TILTMOUSE_USB_HID_MOUSE_BUTTON_RIGHT);
    assert(tiltmouse_usb_hid_mouse_button_mask(true, true) ==
           (TILTMOUSE_USB_HID_MOUSE_BUTTON_LEFT |
            TILTMOUSE_USB_HID_MOUSE_BUTTON_RIGHT));
}

static void test_report_construction(void)
{
    const tiltmouse_usb_hid_mouse_report_t report =
        tiltmouse_usb_hid_mouse_make_report(true, true, INT8_MIN, INT8_MAX);

    assert(report.buttons ==
           (TILTMOUSE_USB_HID_MOUSE_BUTTON_LEFT |
            TILTMOUSE_USB_HID_MOUSE_BUTTON_RIGHT));
    assert(report.x == INT8_MIN);
    assert(report.y == INT8_MAX);
    assert(sizeof(report) == 3u);
}

static void test_release_report(void)
{
    const tiltmouse_usb_hid_mouse_report_t report =
        tiltmouse_usb_hid_mouse_release_report();

    assert(report.buttons == 0u);
    assert(report.x == 0);
    assert(report.y == 0);
}

int main(void)
{
    test_button_masks();
    test_report_construction();
    test_release_report();
    return 0;
}
