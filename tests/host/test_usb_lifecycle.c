#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "class/hid/hid_device.h"
#include "tinyusb.h"
#include "tiltmouse/mouse_report.h"
#include "tiltmouse/usb_hid_mouse.h"

extern void tud_hid_report_complete_cb(
    uint8_t instance, const uint8_t *report, uint16_t len);
extern void tud_hid_report_failed_cb(
    uint8_t instance, hid_report_type_t report_type,
    const uint8_t *report, uint16_t transferred_bytes);

typedef struct {
    bool mounted;
    bool ready;
    bool accept;
    uint32_t sends;
    uint8_t last[3];
    tinyusb_config_t config;
} fake_usb_t;
static fake_usb_t g;

bool tud_mounted(void) { return g.mounted; }
bool tud_hid_ready(void) { return g.ready; }

bool tud_hid_report(uint8_t id, const void *report, uint16_t len)
{
    assert(id == 0 && len == 3u);
    if (!g.accept) {
        return false;
    }
    memcpy(g.last, report, 3u);
    ++g.sends;
    return true;
}

esp_err_t tinyusb_driver_install(const tinyusb_config_t *config)
{
    g.config = *config;
    return ESP_OK;
}

static void event(tinyusb_event_id_t id)
{
    tinyusb_event_t e = {.id = id};
    assert(g.config.event_cb != NULL);
    g.config.event_cb(&e, g.config.event_arg);
}

static tm_mouse_report_t current(int x, int y, uint8_t buttons)
{
    tm_mouse_report_t r;
    assert(tm_mouse_report_make_current(x, y, buttons, &r));
    return r;
}

static void complete_last(bool success)
{
    if (success) {
        tud_hid_report_complete_cb(0, g.last, 3);
    } else {
        tud_hid_report_failed_cb(
            0, HID_REPORT_TYPE_INPUT, g.last, 3);
    }
}

static void test_suspend_resume_neutral_fence(void)
{
    tm_mouse_report_t r = current(17, -3, TM_MOUSE_BUTTON_LEFT);
    tm_mouse_publish_result_t result =
        tiltmouse_usb_hid_mouse_publish(&r, 100, 100);
    assert(result.motion == TM_MOUSE_MOTION_ACCEPTED);
    assert(g.sends == 1u && g.last[1] == 17u);

    /* Accepted pre-suspend movement is never offered again. */
    event(TINYUSB_EVENT_SUSPENDED);
    tm_usb_hid_mouse_status_t status = tiltmouse_usb_hid_mouse_status();
    assert(status.suspended && status.suspends == 1u);
    assert(status.lifecycle_resets == 1u);

    event(TINYUSB_EVENT_RESUMED);
    assert(!tiltmouse_usb_hid_mouse_is_suspended());
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.sends == 2u);
    assert(g.last[0] == 0u && g.last[1] == 0u && g.last[2] == 0u);
    complete_last(true);
    status = tiltmouse_usb_hid_mouse_status();
    assert(status.completions == 2u);

    r = current(5, 1, TM_MOUSE_BUTTON_LEFT);
    result = tiltmouse_usb_hid_mouse_publish(&r, 200, 200);
    assert(result.motion == TM_MOUSE_MOTION_ACCEPTED);
    assert(g.sends == 3u && g.last[1] == 5u);
    complete_last(true);
    (void)tiltmouse_usb_hid_mouse_status();
    assert(g.sends == 3u);
}

static void test_detach_reconnect_and_failed_completion(void)
{
    tm_mouse_report_t r = current(9, 2, TM_MOUSE_BUTTON_RIGHT);
    (void)tiltmouse_usb_hid_mouse_publish(&r, 300, 300);
    event(TINYUSB_EVENT_DETACHED);
    g.mounted = false;
    tm_usb_hid_mouse_status_t status = tiltmouse_usb_hid_mouse_status();
    assert(status.detaches == 1u && status.lifecycle_resets == 2u);
    const uint32_t before = g.sends;
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.sends == before);

    g.mounted = true;
    event(TINYUSB_EVENT_ATTACHED);
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.sends == before + 1u);
    assert(g.last[0] == 0u && g.last[1] == 0u && g.last[2] == 0u);
    complete_last(false);
    (void)tiltmouse_usb_hid_mouse_status();
    status = tiltmouse_usb_hid_mouse_status();
    assert(status.transfer_failures == 1u);

    /* Failure repairs only zero-motion state, never old +9/+2 movement. */
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.last[1] == 0u && g.last[2] == 0u);
}

static void test_quiesce(void)
{
    complete_last(true);
    (void)tiltmouse_usb_hid_mouse_status();
    assert(tiltmouse_usb_hid_mouse_begin_quiesce());

    /* A disconnect during handoff is a new USB generation but must not
     * silently cancel the application's quiesce intent. */
    event(TINYUSB_EVENT_DETACHED);
    g.mounted = false;
    tm_usb_hid_mouse_status_t status = tiltmouse_usb_hid_mouse_status();
    assert(status.quiescing && !status.quiesced);
    assert(!tiltmouse_usb_hid_mouse_is_quiesced());

    g.mounted = true;
    event(TINYUSB_EVENT_ATTACHED);
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.last[0] == 0u && g.last[1] == 0u && g.last[2] == 0u);
    complete_last(true);
    assert(tiltmouse_usb_hid_mouse_is_quiesced());
}

int main(void)
{
    g = (fake_usb_t){.mounted = true, .ready = true, .accept = true};
    assert(tiltmouse_usb_hid_mouse_init() == ESP_OK);
    /* Initial activation itself is release-first; complete that barrier so
     * lifecycle tests start from ordinary ready state. */
    (void)tiltmouse_usb_hid_mouse_service();
    assert(g.sends == 1u && g.last[0] == 0u && g.last[1] == 0u);
    complete_last(true);
    (void)tiltmouse_usb_hid_mouse_status();
    g.sends = 0u;
    test_suspend_resume_neutral_fence();
    test_detach_reconnect_and_failed_completion();
    test_quiesce();
    const tm_usb_hid_mouse_status_t status = tiltmouse_usb_hid_mouse_status();
    assert(status.attaches == 1u && status.resumes == 1u);
    return 0;
}
