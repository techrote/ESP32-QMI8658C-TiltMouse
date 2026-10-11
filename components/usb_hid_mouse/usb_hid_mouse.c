#include "tiltmouse/usb_hid_mouse.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "class/hid/hid_device.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tiltmouse/mouse_output.h"
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

/* Three-byte, mouse-only descriptor: button bits [0..1], constant padding
 * [2..7], signed relative X [8..15], signed relative Y [16..23].
 * The two-button Data Input was missing before TM-005A.
 */
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
    0x81, 0x02,       /*     Input (Data, Variable, Absolute) */
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
static atomic_bool s_link_lost;
static atomic_uint_fast32_t s_completion_event;
static tm_mouse_output_t s_output;

enum {
    USB_EVENT_PRESENT = 0x80000000u,
    USB_EVENT_SUCCESS = 0x40000000u,
    USB_EVENT_PAYLOAD_MASK = 0x00FFFFFFu,
};

_Static_assert(
    sizeof(tiltmouse_usb_hid_mouse_report_t) == 3,
    "TiltMouse HID input report must stay exactly three bytes");

static bool usb_backend_ready(void *ctx)
{
    (void)ctx;
    return s_initialized && tud_mounted() && !atomic_load(&s_suspended) &&
           tud_hid_ready();
}

static bool usb_backend_submit(void *ctx, const tm_mouse_report_t *report)
{
    (void)ctx;
    tiltmouse_usb_hid_mouse_report_t hid;
    return tiltmouse_usb_hid_mouse_from_logical(report, &hid) &&
           tud_hid_report(0, &hid, sizeof(hid));
}

static uint64_t usb_backend_now(void *ctx)
{
    (void)ctx;
    return (uint64_t)esp_timer_get_time();
}

/* TinyUSB callbacks must not mutate the multivariable output state.
 * Only one report is in flight; a single atomic payload/event slot is enough.
 * Worker-side drain checks it against the copied in-flight three-byte report.
 */
static void record_completion(bool success, const uint8_t *report, uint16_t length)
{
    if (report == NULL || length != 3u) {
        atomic_store(&s_link_lost, true);
        return;
    }
    const uint32_t payload = (uint32_t)report[0] |
                             ((uint32_t)report[1] << 8) |
                             ((uint32_t)report[2] << 16);
    const uint32_t event = USB_EVENT_PRESENT |
                           (success ? USB_EVENT_SUCCESS : 0u) | payload;
    atomic_store(&s_completion_event, event);
}

void tud_hid_report_complete_cb(
    uint8_t instance, const uint8_t *report, uint16_t len)
{
    if (instance == TILTMOUSE_USB_HID_INTERFACE) {
        record_completion(true, report, len);
    }
}

void tud_hid_report_failed_cb(
    uint8_t instance, hid_report_type_t report_type,
    const uint8_t *report, uint16_t xferred_bytes)
{
    if (instance == TILTMOUSE_USB_HID_INTERFACE &&
        report_type == HID_REPORT_TYPE_INPUT) {
        record_completion(false, report, xferred_bytes);
    }
}

static void drain_usb_events(void)
{
    if (!s_initialized) {
        return;
    }

    if (atomic_exchange(&s_link_lost, false)) {
        (void)atomic_exchange(&s_completion_event, 0);
        tm_mouse_output_link_lost(&s_output);
    }

    const uint_fast32_t event = atomic_exchange(&s_completion_event, 0);
    if ((event & USB_EVENT_PRESENT) == 0u) {
        return;
    }

    tm_mouse_report_t inflight;
    uint64_t ticket = 0;
    if (!tm_mouse_output_inflight(&s_output, &ticket, &inflight)) {
        return;
    }

    tiltmouse_usb_hid_mouse_report_t hid;
    if (!tiltmouse_usb_hid_mouse_from_logical(&inflight, &hid)) {
        return;
    }
    const uint32_t expected = (uint32_t)hid.buttons |
                              ((uint32_t)(uint8_t)hid.x << 8) |
                              ((uint32_t)(uint8_t)hid.y << 16);
    if ((event & USB_EVENT_PAYLOAD_MASK) != expected) {
        return;
    }

    (void)tm_mouse_output_complete(
        &s_output, ticket, (event & USB_EVENT_SUCCESS) != 0u);
}

static tm_publish_result_t usb_transport_publish(
    void *ctx, const tm_mouse_report_t *report, uint64_t deadline)
{
    (void)ctx;
    drain_usb_events();
    return tm_mouse_output_publish(&s_output, report, deadline);
}

static void usb_transport_service(void *ctx)
{
    (void)ctx;
    drain_usb_events();
    tm_mouse_output_service(&s_output);
}

static void usb_transport_quiesce(void *ctx)
{
    (void)ctx;
    drain_usb_events();
    tm_mouse_output_begin_quiesce(&s_output);
}

static tm_transport_status_t usb_transport_status(void *ctx)
{
    (void)ctx;
    drain_usb_events();
    return tm_mouse_output_status(&s_output);
}

tm_mouse_transport_t tiltmouse_usb_hid_mouse_transport(void)
{
    return (tm_mouse_transport_t){
        .ctx = &s_output,
        .publish_consume = usb_transport_publish,
        .service = usb_transport_service,
        .begin_quiesce = usb_transport_quiesce,
        .status = usb_transport_status,
    };
}

void tiltmouse_usb_hid_mouse_service(void)
{
    usb_transport_service(&s_output);
}

static void tiltmouse_usb_hid_mouse_device_event(
    tinyusb_event_t *event, void *arg)
{
    (void)arg;
    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
    case TINYUSB_EVENT_RESUMED:
        atomic_store(&s_suspended, false);
        break;
    case TINYUSB_EVENT_DETACHED:
        atomic_store(&s_suspended, false);
        atomic_store(&s_link_lost, true);
        break;
    case TINYUSB_EVENT_SUSPENDED:
        atomic_store(&s_suspended, true);
        atomic_store(&s_link_lost, true);
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
    atomic_store(&s_link_lost, false);
    atomic_store(&s_completion_event, 0);

    const tm_mouse_output_backend_t backend = {
        .ctx = &s_output,
        .ready = usb_backend_ready,
        .submit = usb_backend_submit,
        .now_us = usb_backend_now,
    };
    if (!tm_mouse_output_init(&s_output, backend)) {
        return ESP_FAIL;
    }

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
    return s_initialized && atomic_load(&s_suspended);
}

static esp_err_t usb_result_to_legacy(tm_publish_result_t result)
{
    if (result.reason == TM_REASON_OK) {
        return ESP_OK;
    }
    if (result.reason == TM_REASON_INVALID) {
        return ESP_ERR_INVALID_ARG;
    }
    if (result.reason == TM_REASON_SUBMIT_FAILED) {
        return ESP_FAIL;
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t tiltmouse_usb_hid_mouse_send(
    bool left_pressed, bool right_pressed, int8_t x, int8_t y)
{
    tm_mouse_report_t logical;
    if (!tm_mouse_report_make(
            (int)x, (int)y,
            tiltmouse_usb_hid_mouse_button_mask(left_pressed, right_pressed),
            &logical)) {
        return ESP_ERR_INVALID_ARG;
    }
    return usb_result_to_legacy(usb_transport_publish(&s_output, &logical, 0));
}

esp_err_t tiltmouse_usb_hid_mouse_release_all(void)
{
    const tm_mouse_report_t release = tm_mouse_report_release();
    return usb_result_to_legacy(usb_transport_publish(&s_output, &release, 0));
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
    uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
    const uint8_t *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
