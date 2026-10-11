#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/mouse_report.h"
#include "tiltmouse/mouse_transport.h"

typedef struct {
    bool ready;
    bool accepts;
    unsigned count;
    tm_mouse_report_t output[64];
} fake_t;

static bool ready(void *context)
{
    return ((fake_t *)context)->ready;
}

static bool submit(void *context, const tm_mouse_report_t *report)
{
    fake_t *fake = (fake_t *)context;
    assert(tm_mouse_report_is_valid(report));
    assert(report->kind == TM_MOUSE_REPORT_CURRENT);
    if (!fake->accepts) {
        return false;
    }
    assert(fake->count < 64u);
    fake->output[fake->count++] = *report;
    return true;
}

static void activate(tm_mouse_transport_t *publisher, fake_t *fake,
                     tm_mouse_transport_id_t id)
{
    const tm_mouse_transport_sink_t sink = {
        .ready = ready, .submit = submit, .context = fake,
    };
    assert(tm_mouse_transport_activate(publisher, id, sink));
}

static tm_mouse_report_t report(int32_t x, int32_t y, uint32_t buttons)
{
    tm_mouse_report_t r;
    assert(tm_mouse_report_make_current(x, y, buttons, &r));
    return r;
}

static void complete(tm_mouse_transport_t *publisher, bool success)
{
    const uint64_t token = tm_mouse_transport_pending_ticket(publisher);
    assert(token != 0u);
    assert(tm_mouse_transport_complete(publisher, token, success));
    assert(tm_mouse_transport_pending_ticket(publisher) == 0u);
}

static void test_report(void)
{
    tm_mouse_report_t r = {0};
    assert(sizeof(r) == 4u && !tm_mouse_report_is_valid(&r));
    assert(tm_mouse_report_make_current(-127, 127, 3u, &r));
    assert(r.dx == -127 && r.dy == 127 && tm_mouse_report_is_valid(&r));
    assert(!tm_mouse_report_make_current(-128, 0, 0u, &r));
    assert(r.kind == TM_MOUSE_REPORT_INVALID && r.dx == 0);
    assert(!tm_mouse_report_make_current(0, 128, 0u, &r));
    assert(!tm_mouse_report_make_current(0, 0, 4u, &r));
    assert(!tm_mouse_report_make_current(0, 0, 0u, NULL));
    assert(tm_mouse_report_make_release(&r));
    assert(r.buttons == 0u && r.dx == 0 && r.dy == 0);
    assert(tm_mouse_report_is_valid(&r));
    r.dx = 5;
    assert(!tm_mouse_report_is_valid(&r));
    assert(!tm_mouse_report_is_valid(NULL));
    assert(!tm_mouse_report_make_release(NULL));
}

static void test_busy_and_no_stale_replay(void)
{
    fake_t fake = {.ready = true, .accepts = true};
    tm_mouse_transport_t pub = {0};
    activate(&pub, &fake, TM_MOUSE_TRANSPORT_USB);
    tm_mouse_report_t r = report(12, -7, 0u);
    tm_mouse_publish_result_t result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &r, 100u, 200u);
    assert(result.motion == TM_MOUSE_MOTION_ACCEPTED);
    assert(fake.count == 1u);
    assert(fake.output[0].dx == 12 && fake.output[0].dy == -7);
    r = report(19, 3, 0u);
    result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &r, 100u, 200u);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    assert(result.reason == TM_MOUSE_REASON_BUSY);
    complete(&pub, true);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.count == 1u);
}

static void test_button_repair_and_release_barrier(void)
{
    fake_t fake = {.ready = false, .accepts = true};
    tm_mouse_transport_t pub = {0};
    activate(&pub, &fake, TM_MOUSE_TRANSPORT_USB);
    tm_mouse_report_t left = report(24, 0, TM_MOUSE_BUTTON_LEFT);
    tm_mouse_publish_result_t result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &left, 0u, 0u);
    assert(result.reason == TM_MOUSE_REASON_NOT_READY);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    fake.ready = true;
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.count == 1u);
    assert(fake.output[0].buttons == 0u && fake.output[0].dx == 0);
    complete(&pub, true);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.output[1].buttons == TM_MOUSE_BUTTON_LEFT);
    assert(fake.output[1].dx == 0 && fake.output[1].dy == 0);
    complete(&pub, true);
    fake.ready = false;
    tm_mouse_report_t release;
    assert(tm_mouse_report_make_release(&release));
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &release, 0u, 0u);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &left, 0u, 0u);
    fake.ready = true;
    result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &left, 0u, 0u);
    assert(result.reason == TM_MOUSE_REASON_RELEASE_FIRST);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    assert(fake.output[2].buttons == 0u && fake.output[2].dx == 0);
    complete(&pub, true);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.output[3].buttons == TM_MOUSE_BUTTON_LEFT);
    assert(fake.output[3].dx == 0 && fake.output[3].dy == 0);
    complete(&pub, true);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.count == 4u);
}

static void test_invalid_failed_and_expired(void)
{
    fake_t fake = {.ready = false, .accepts = true};
    tm_mouse_transport_t pub = {0};
    activate(&pub, &fake, TM_MOUSE_TRANSPORT_USB);
    tm_mouse_report_t bad = {
        .dx = 50, .dy = 60, .buttons = 7u,
        .kind = TM_MOUSE_REPORT_CURRENT,
    };
    tm_mouse_publish_result_t result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &bad, 0u, 0u);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    assert(result.state == TM_MOUSE_STATE_SAFE_RELEASE_RETAINED);
    assert(fake.count == 0u);
    fake.ready = true;
    fake.accepts = false;
    tm_mouse_report_t move = report(25, 0, 0u);
    result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &move, 0u, 0u);
    assert(result.reason == TM_MOUSE_REASON_SUBMIT_FAILED);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    fake.accepts = true;
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(fake.count == 1u);
    assert(fake.output[0].buttons == 0u &&
           fake.output[0].dx == 0 && fake.output[0].dy == 0);
    complete(&pub, true);
    move = report(80, -3, 0u);
    result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &move, 501u, 500u);
    assert(result.reason == TM_MOUSE_REASON_EXPIRED);
    assert(result.motion == TM_MOUSE_MOTION_DROPPED);
    assert(fake.count == 1u);
}

static void test_revision_and_exclusive_route(void)
{
    fake_t usb = {.ready = true, .accepts = true};
    fake_t radio = {.ready = true, .accepts = true};
    tm_mouse_transport_t pub = {0};
    activate(&pub, &usb, TM_MOUSE_TRANSPORT_USB);
    tm_mouse_report_t left = report(0, 0, TM_MOUSE_BUTTON_LEFT);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &left, 0u, 0u);
    complete(&pub, true);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    complete(&pub, true);

    tm_mouse_report_t released = report(0, 0, 0u);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &released, 0u, 0u);
    const uint64_t stale = tm_mouse_transport_pending_ticket(&pub);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &left, 0u, 0u);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &released, 0u, 0u);
    assert(tm_mouse_transport_complete(&pub, stale, true));
    assert(pub.release_debt != 0u);
    (void)tm_mouse_transport_service(&pub, TM_MOUSE_TRANSPORT_USB);
    assert(usb.output[usb.count - 1u].buttons == 0u);
    complete(&pub, true);
    tm_mouse_report_t move = report(13, 1, 0u);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &move, 0u, 0u);
    const uint64_t old_ticket = tm_mouse_transport_pending_ticket(&pub);

    activate(&pub, &radio, TM_MOUSE_TRANSPORT_ESPNOW);
    assert(!tm_mouse_transport_complete(&pub, old_ticket, true));
    const unsigned old_usb_count = usb.count;
    tm_mouse_publish_result_t result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_USB, &move, 0u, 0u);
    assert(result.reason == TM_MOUSE_REASON_INACTIVE);
    assert(result.state == TM_MOUSE_STATE_UNCHANGED);
    assert(usb.count == old_usb_count && radio.count == 0u);
    move = report(0, 0, 0u);
    (void)tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_ESPNOW, &move, 0u, 0u);
    assert(radio.count == 1u && radio.output[0].dx == 0);
    assert(tm_mouse_transport_pending_ticket(&pub) != old_ticket);
    tm_mouse_transport_deactivate(&pub);
    result = tm_mouse_transport_publish(
        &pub, TM_MOUSE_TRANSPORT_ESPNOW, &move, 0u, 0u);
    assert(result.reason == TM_MOUSE_REASON_INACTIVE);
}

int main(void)
{
    test_report();
    test_busy_and_no_stale_replay();
    test_button_repair_and_release_barrier();
    test_invalid_failed_and_expired();
    test_revision_and_exclusive_route();
    return 0;
}
