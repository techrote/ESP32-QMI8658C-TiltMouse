#include "tiltmouse/usb_hid_mouse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "class/hid/hid_device.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tiltmouse/usb_hid_mouse_report.h"

#if CONFIG_TINYUSB_HID_COUNT != 1
#error "TiltMouse requires exactly one TinyUSB HID interface"
#endif

enum {
    TILTMOUSE_USB_INTERFACE_COUNT = 1,
    TILTMOUSE_USB_HID_INTERFACE = 0,
    TILTMOUSE_USB_HID_STRING_INDEX = 4,
    TILTMOUSE_USB_HID_ENDPOINT_IN = 0x81,
    TILTMOUSE_USB_HID_ENDPOINT_SIZE = 8,
    TILTMOUSE_USB_HID_POLL_INTERVAL_MS = 8,
    TILTMOUSE_USB_CONFIG_DESC_TOTAL_LEN = TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
};

static const uint8_t s_hid_report_descriptor[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x02,       /* Usage (Mouse) */
    0xA1, 0x01,       /* Collection (Application) */
    0x09, 0x01,       /*   Usage (Pointer) */
    0xA1, 0x00,       /*   Collection (Physical) */
    0x05, 0x09,       /*     Usage Page (Button) */
    0x19, 0x01,       /*     Usage Minimum (Button 1) */
    0x29, 0x02,       /*     Usage Maximum (Button 2) */
    0x15, 0x00,       /*     Logical Minimum (0) */
    0x25, 0x01,       /*     Logical Maximum (1) */
    0x95, 0x02,       /*     Report Count (2) */
    0x75, 0x01,       /*     Report Size (1) */
    0x95, 0x01,       /*     Report Count (1) */
    0x75, 0x06,       /*     Report Size (6) */
    0x81, 0x03,       /*     Input (Constant, Variable, Absolute) */
    0x05, 0x01,       /*     Usage Page (Generic Desktop) */
    0x09, 0x30,       /*     Usage (X) */
    0x09, 0x31,       /*     Usage (Y) */
    0x15, 0x81,       /*     Logical Minimum (-127) */
    0x25, 0x7F,       /*     Logical Maximum (127) */
    0x75, 0x08,       /*     Report Size (8) */
    0x95, 0x02,       /*     Report Count (2) */
    0x81, 0x06,       /*     Input (Data, Variable, Relative) */
    0xC0,             /*   End Collection */
    0xC0,             /* End Collection */
};

static const char *s_string_descriptor[] = {
    (char[]){0x09, 0x04},
    "techrote",
    "TiltMouse",
    "TM-002",
    "TiltMouse mouse",
};

static const uint8_t s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        TILTMOUSE_USB_INTERFACE_COUNT,
        0,
        TILTMOUSE_USB_CONFIG_DESC_TOTAL_LEN,
        0,
        100),
    TUD_HID_DESCRIPTOR(
        TILTMOUSE_USB_HID_INTERFACE,
        TILTMOUSE_USB_HID_STRING_INDEX,
        HID_ITF_PROTOCOL_MOUSE,
        sizeof(s_hid_report_descriptor),
        TILTMOUSE_USB_HID_ENDPOINT_IN,
        TILTMOUSE_USB_HID_ENDPOINT_SIZE,
        TILTMOUSE_USB_HID_POLL_INTERVAL_MS),
};

static bool s_initialized;
static volatile bool s_suspended;

_Static_assert(
    sizeof(tiltmouse_usb_hid_mouse_report_t) == 3,
    "TiltMouse HID input report must stay exactly three bytes");

esp_err_t tiltmouse_usb_hid_mouse_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_suspended = false;

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = s_configuration_descriptor;
    tusb_cfg.descriptor.string = s_string_descriptor;
    tusb_cfg.descriptor.string_count =
        sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_configuration_descriptor;
#endif

    const esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err == ESP_OK) {
        s_initialized = true;
    }

    return err;
}

bool tiltmouse_usb_hid_mouse_is_mounted(void)
{
    return s_initialized && tud_mounted();
}

bool tiltmouse_usb_hid_mouse_is_suspended(void)
{
    return s_initialized && s_suspended;
}

esp_err_t tiltmouse_usb_hid_mouse_send(
    bool left_pressed,
    bool right_pressed,
    int8_t x,
    int8_t y)
{
    if (!s_initialized || !tud_mounted() || s_suspended || !tud_hid_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    const tiltmouse_usb_hid_mouse_report_t report =
        tiltmouse_usb_hid_mouse_make_report(left_pressed, right_pressed, x, y);

    if (!tud_hid_report(0, &report, sizeof(report))) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t tiltmouse_usb_hid_mouse_release_all(void)
{
    const tiltmouse_usb_hid_mouse_report_t report =
        tiltmouse_usb_hid_mouse_release_report();

    if (!s_initialized || !tud_mounted() || s_suspended || !tud_hid_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_report(0, &report, sizeof(report))) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

void tud_mount_cb(void)
{
    s_suspended = false;
}

void tud_umount_cb(void)
{
    s_suspended = false;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    s_suspended = true;
}

void tud_resume_cb(void)
{
    s_suspended = false;
}
