#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "class/hid/hid_device.h"
#include "tinyusb.h"
#include "tiltmouse/mouse_report.h"
#include "tiltmouse/usb_hid_mouse.h"
#include "tiltmouse/usb_hid_mouse_report.h"

extern const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance);
extern void tud_hid_report_complete_cb(
    uint8_t instance, const uint8_t *report, uint16_t len);
extern void tud_hid_report_failed_cb(
    uint8_t instance, hid_report_type_t report_type,
    const uint8_t *report, uint16_t xferred_bytes);

typedef struct {
    bool mounted;
    bool ready;
    bool accept;
    int64_t now;
    unsigned send_count;
    uint8_t last_report[3];
    tinyusb_config_t config;
} fake_usb_t;
static fake_usb_t g;

bool tud_mounted(void)
{
    return g.mounted;
}

bool tud_hid_ready(void)
{
    return g.ready;
}

bool tud_hid_report(uint8_t report_id, const void *report, uint16_t len)
{
    assert(report_id == 0 && len == 3);
    if (!g.accept) {
        return false;
    }
    memcpy(g.last_report, report, 3);
    ++g.send_count;
    return true;
}

esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    g.config = *config;
    return ESP_OK;
}

int64_t esp_timer_get_time(void)
{
    return g.now;
}

static tm_mouse_report_t make(int x, int y, unsigned buttons)
{
    tm_mouse_report_t r;
    assert(tm_mouse_report_make(x, y, buttons, &r));
    return r;
}

static void finish(bool ok)
{
    if (ok) {
        tud_hid_report_complete_cb(0, g.last_report, 3);
    } else {
        tud_hid_report_failed_cb(0, HID_REPORT_TYPE_INPUT, g.last_report, 3);
    }
}

static void emit_event(tinyusb_event_id_t id)
{
    tinyusb_event_t event = {.id = id};
    assert(g.config.event_cb != NULL);
    g.config.event_cb(&event, g.config.event_arg);
}

/* HID short-item parser for the exact production descriptor, not a duplicated
 * hand-written descriptor fixture. Verify bit positions and report identity.
 */
static void test_descriptor(void)
{
    const uint8_t *config = g.config.descriptor.full_speed_config;
    assert(config != NULL && config[0] == 9 && config[1] == 2);
    assert(config[2] == 9 && config[3] == 0x21);
    const size_t length = (size_t)config[4] | ((size_t)config[5] << 8);
    const uint8_t *d = tud_hid_descriptor_report_cb(0);
    size_t bit_offset = 0;
    unsigned report_size = 0;
    unsigned report_count = 0;
    unsigned usage_page = 0;
    unsigned items = 0;
    unsigned data_buttons = 0;
    unsigned padding_bits = 0;
    unsigned relative_bits = 0;

    assert(length >= 40 && length < 120);
    for (size_t i = 0; i < length;) {
        const uint8_t tag = d[i++];
        assert(tag != 0xfe);
        const size_t width = (tag & 3u) == 3u ? 4u : (tag & 3u);
        assert(i + width <= length);
        unsigned value = 0;
        for (size_t b = 0; b < width; ++b) {
            value |= (unsigned)d[i + b] << (8u * b);
        }
        i += width;
        if (tag == 0x05) {
            usage_page = value;
        } else if (tag == 0x75) {
            report_size = value;
        } else if (tag == 0x95) {
            report_count = value;
        } else if (tag == 0x81) {
            const unsigned bits = report_size * report_count;
            ++items;
            if (value == 0x02) {
                assert(usage_page == 0x09 && bit_offset == 0);
                assert(report_size == 1 && report_count == 2);
                data_buttons += bits;
            } else if (value == 0x03) {
                assert(bit_offset == 2 && report_size == 6 && report_count == 1);
                padding_bits += bits;
            } else if (value == 0x06) {
                assert(usage_page == 0x01 && bit_offset == 8);
                assert(report_size == 8 && report_count == 2);
                relative_bits += bits;
            } else {
                assert(false && "Unexpected HID Input item");
            }
            bit_offset += bits;
        }
    }
    assert(items == 3 && bit_offset == 24);
    assert(data_buttons == 2 && padding_bits == 6 && relative_bits == 16);
    assert(g.config.descriptor.string_count == 5);
    assert(strcmp(g.config.descriptor.string[1], "techrote") == 0);
    assert(strcmp(g.config.descriptor.string[2], "TiltMouse") == 0);
}

static void test_logical_adapter(void)
{
    tiltmouse_usb_hid_mouse_report_t hid = {0xff, 77, 66};
    const tm_mouse_report_t r = make(-127, 127, TM_BUTTON_MASK);
    assert(tiltmouse_usb_hid_mouse_from_logical(&r, &hid));
    assert(hid.buttons == 3 && hid.x == -127 && hid.y == 127);
    const tm_mouse_report_t release = tm_mouse_report_release();
    assert(tiltmouse_usb_hid_mouse_from_logical(&release, &hid));
    assert(hid.buttons == 0 && hid.x == 0 && hid.y == 0);
    assert(!tiltmouse_usb_hid_mouse_from_logical(NULL, &hid));
    assert(!tiltmouse_usb_hid_mouse_from_logical(&r, NULL));
}

static void test_production_transport(void)
{
    g = (fake_usb_t){.mounted = true, .ready = true, .accept = true};
    assert(tiltmouse_usb_hid_mouse_init() == ESP_OK);
    assert(tiltmouse_usb_hid_mouse_init() == ESP_ERR_INVALID_STATE);
    test_descriptor();
    test_logical_adapter();

    tm_mouse_transport_t usb = tiltmouse_usb_hid_mouse_transport();
    tm_mouse_router_t router;
    assert(tm_mouse_router_init(&router, TM_ROUTE_USB, usb));
    tm_mouse_report_t r = make(7, -4, TM_BUTTON_MASK);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_ESPNOW, &r, 0).reason == TM_REASON_INACTIVE);
    assert(g.send_count == 0);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).motion == TM_MOTION_ACCEPTED);
    assert(g.send_count == 1);
    assert(g.last_report[0] == 3 && g.last_report[1] == 7);
    assert(g.last_report[2] == (uint8_t)-4);
    finish(true);
    assert(tm_mouse_router_status(&router).link == TM_LINK_READY);

    g.ready = false;
    r = make(9, 4, TM_BUTTON_MASK);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).reason == TM_REASON_NOT_READY);
    assert(g.send_count == 1);
    g.ready = true;

    r = make(0, 0, 0);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).motion == TM_MOTION_NONE);
    assert(g.send_count == 2 && g.last_report[0] == 0);
    finish(true);
    assert(!tm_mouse_router_status(&router).release_pending);

    r = make(11, 1, TM_BUTTON_LEFT);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).motion == TM_MOTION_ACCEPTED);
    finish(false);
    tm_mouse_router_service(&router);
    assert(g.last_report[0] == TM_BUTTON_LEFT);
    assert(g.last_report[1] == 0 && g.last_report[2] == 0);
    finish(true);
    assert(tm_mouse_router_status(&router).link == TM_LINK_READY);

    assert(tiltmouse_usb_hid_mouse_send(true, false, INT8_MIN, 0) ==
           ESP_ERR_INVALID_ARG);
    assert(tiltmouse_usb_hid_mouse_release_all() == ESP_OK);
    assert(g.last_report[0] == 0 && g.last_report[1] == 0);
    finish(true);
    (void)tm_mouse_router_status(&router);
    assert(!tm_mouse_router_status(&router).release_pending);

    r = make(3, 1, TM_BUTTON_LEFT);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).motion == TM_MOTION_ACCEPTED);
    emit_event(TINYUSB_EVENT_SUSPENDED);
    assert(tiltmouse_usb_hid_mouse_is_suspended());
    (void)tm_mouse_router_status(&router);
    emit_event(TINYUSB_EVENT_RESUMED);
    assert(!tiltmouse_usb_hid_mouse_is_suspended());
    tm_mouse_router_service(&router);
    assert(g.last_report[0] == 0 && g.last_report[1] == 0);
    finish(true);
    assert(!tm_mouse_router_status(&router).release_pending);
}

int main(void)
{
    test_production_transport();
    return 0;
}
