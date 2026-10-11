#include "tiltmouse/usb_hid_mouse.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "class/hid/hid_device.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tiltmouse/usb_hid_mouse_report.h"
#include "tiltmouse/mouse_report.h"
#include "tiltmouse/mouse_transport.h"

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
    0x81, 0x02,       /*     Input (Data, Variable, Absolute) BUTTONS */
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
static atomic_bool s_suspended;
/* TinyUSB runs callbacks on its own task. Handoff is atomic; all publisher
 * state mutation is confined to the calling application task. */
static atomic_int s_completion; /* 0=pending, 1=completed, 2=failed */
static tm_mouse_transport_t s_usb_publisher;

static bool usb_sink_ready(void *context)
{
    (void)context;
    return s_initialized && tud_mounted() &&
           !atomic_load(&s_suspended) && tud_hid_ready();
}

static bool usb_sink_submit(void *context, const tm_mouse_report_t *report)
{
    (void)context;
    tiltmouse_usb_hid_mouse_report_t usb_report;
    if (!tiltmouse_usb_hid_mouse_from_logical(report, &usb_report)) {
        return false;
    }

    return tud_hid_report(0, &usb_report, sizeof(usb_report));
}

static void usb_process_completion(void)
{
    /* Call only from the single owner of the publisher, never from TinyUSB. */
    const int event = atomic_exchange(&s_completion, 0);
    if (event != 0) {
        const uint64_t ticket =
            tm_mouse_transport_pending_ticket(&s_usb_publisher);
        if (ticket != 0u) {
            (void)tm_mouse_transport_complete(
                &s_usb_publisher, ticket, event == 1);
        }
    }
}

_Static_assert(
    sizeof(tiltmouse_usb_hid_mouse_report_t) == 3,
    "TiltMouse HID input report must stay exactly three bytes");

static void tiltmouse_usb_hid_mouse_device_event(
    tinyusb_event_t *event,
    void *arg)
{
    (void)arg;

    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
    case TINYUSB_EVENT_DETACHED:
        atomic_store(&s_suspended, false);
        break;
    case TINYUSB_EVENT_SUSPENDED:
        atomic_store(&s_suspended, true);
        break;
    case TINYUSB_EVENT_RESUMED:
        atomic_store(&s_suspended, false);
        break;
    default:
        break;
    }
}

esp_err_t tiltmouse_usb_hid_mouse_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    atomic_store(&s_suspended, false);

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.event_cb = tiltmouse_usb_hid_mouse_device_event;
    tusb_cfg.event_arg = NULL;
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = s_configuration_descriptor;
    tusb_cfg.descriptor.string = s_string_descriptor;
    tusb_cfg.descriptor.string_count =
        sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_configuration_descriptor;
#endif

    atomic_store(&s_completion, 0);
    const esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err == ESP_OK) {
        s_initialized = true;
        const tm_mouse_transport_sink_t sink = {
            .ready = usb_sink_ready,
            .submit = usb_sink_submit,
            .context = NULL,
        };
        /* The zero-initialized static publisher is owned by the app task. */
        if (!tm_mouse_transport_activate(
                &s_usb_publisher, TM_MOUSE_TRANSPORT_USB, sink)) {
            return ESP_FAIL;
        }
    }

    return err;
}

bool tiltmouse_usb_hid_mouse_is_mounted(void)
{
    return s_initialized && tud_mounted();
}

bool tiltmouse_usb_hid_mouse_is_suspended(void)
{
    return s_initialized && atomic_load(&s_suspended);
}

tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_publish(
    const tm_mouse_report_t *report, uint64_t now_us, uint64_t deadline_us)
{
    usb_process_completion();
    return tm_mouse_transport_publish(
        &s_usb_publisher, TM_MOUSE_TRANSPORT_USB,
        report, now_us, deadline_us);
}

tm_mouse_publish_result_t tiltmouse_usb_hid_mouse_service(void)
{
    usb_process_completion();
    return tm_mouse_transport_service(
        &s_usb_publisher, TM_MOUSE_TRANSPORT_USB);
}

static esp_err_t usb_legacy_result(tm_mouse_publish_result_t result)
{
    if (result.reason == TM_MOUSE_REASON_INVALID) {
        return ESP_ERR_INVALID_ARG;
    }
    if (result.reason == TM_MOUSE_REASON_NOT_READY ||
        result.reason == TM_MOUSE_REASON_INACTIVE ||
        result.reason == TM_MOUSE_REASON_BUSY) {
        return ESP_ERR_INVALID_STATE;
    }
    if (result.reason == TM_MOUSE_REASON_SUBMIT_FAILED) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t tiltmouse_usb_hid_mouse_send(
    bool left_pressed, bool right_pressed, int8_t x, int8_t y)
{
    tm_mouse_report_t logical;
    const uint32_t buttons =
        (left_pressed ? TM_MOUSE_BUTTON_LEFT : 0u) |
        (right_pressed ? TM_MOUSE_BUTTON_RIGHT : 0u);

    if (!tm_mouse_report_make_current(x, y, buttons, &logical)) {
        return ESP_ERR_INVALID_ARG;
    }

    return usb_legacy_result(
        tiltmouse_usb_hid_mouse_publish(&logical, 0u, 0u));
}

esp_err_t tiltmouse_usb_hid_mouse_release_all(void)
{
    tm_mouse_report_t release;
    (void)tm_mouse_report_make_release(&release);
    return usb_legacy_result(
        tiltmouse_usb_hid_mouse_publish(&release, 0u, 0u));
}

/* TinyUSB asynchronous signals contain no movement-retry permission. Do not
 * call the pure-C state machine from callback context. */
void tud_hid_report_complete_cb(
    uint8_t instance, uint8_t const *report, uint16_t len)
{
    (void)instance;
    (void)report;
    (void)len;
    atomic_store(&s_completion, 1);
}

void tud_hid_report_failed_cb(
    uint8_t instance, hid_report_type_t report_type,
    uint8_t const *report, uint16_t transferred_bytes)
{
    (void)instance;
    (void)report_type;
    (void)report;
    (void)transferred_bytes;
    atomic_store(&s_completion, 2);
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
