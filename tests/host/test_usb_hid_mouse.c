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

static void test_logical_adapter(void)
{
    tm_mouse_report_t logical = {0};
    tiltmouse_usb_hid_mouse_report_t usb = {0};

    assert(tm_mouse_report_make_current(-127, 127, TM_MOUSE_BUTTON_ALL,
                                        &logical));
    assert(tiltmouse_usb_hid_mouse_from_logical(&logical, &usb));
    assert(usb.x == -127 && usb.y == 127 && usb.buttons == 3u);

    assert(tm_mouse_report_make_release(&logical));
    assert(tiltmouse_usb_hid_mouse_from_logical(&logical, &usb));
    assert(usb.x == 0 && usb.y == 0 && usb.buttons == 0u);

    logical.kind = TM_MOUSE_REPORT_INVALID;
    usb = tiltmouse_usb_hid_mouse_make_report(true, true, 20, -10);
    assert(!tiltmouse_usb_hid_mouse_from_logical(&logical, &usb));
    assert(usb.x == 0 && usb.y == 0 && usb.buttons == 0u);
    assert(!tiltmouse_usb_hid_mouse_from_logical(NULL, &usb));
    assert(!tiltmouse_usb_hid_mouse_from_logical(&logical, NULL));
}

int main(void)
{
    test_button_masks();
    test_report_construction();
    test_release_report();
    test_logical_adapter();
    return 0;
}
