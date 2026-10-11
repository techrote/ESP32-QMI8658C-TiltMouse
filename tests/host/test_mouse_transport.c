#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tiltmouse/mouse_output.h"

typedef struct {
    bool ready;
    bool accepts;
    uint64_t now;
    tm_mouse_report_t submitted[64];
    size_t count;
} fake_backend_t;

static bool fake_ready(void *ctx)
{
    return ((fake_backend_t *)ctx)->ready;
}

static bool fake_submit(void *ctx, const tm_mouse_report_t *report)
{
    fake_backend_t *fake = ctx;
    if (!fake->accepts) {
        return false;
    }
    assert(fake->count < 64);
    fake->submitted[fake->count++] = *report;
    return true;
}

static uint64_t fake_now(void *ctx)
{
    return ((fake_backend_t *)ctx)->now;
}

static void setup(tm_mouse_output_t *output, fake_backend_t *fake)
{
    *fake = (fake_backend_t){.ready = true, .accepts = true};
    const tm_mouse_output_backend_t backend = {
        .ctx = fake,
        .ready = fake_ready,
        .submit = fake_submit,
        .now_us = fake_now,
    };
    assert(tm_mouse_output_init(output, backend));
}

static tm_mouse_report_t make(int dx, int dy, unsigned buttons)
{
    tm_mouse_report_t report;
    assert(tm_mouse_report_make(dx, dy, buttons, &report));
    return report;
}

static void complete(tm_mouse_output_t *output, bool success)
{
    uint64_t ticket = 0;
    assert(tm_mouse_output_inflight(output, &ticket, NULL));
    assert(tm_mouse_output_complete(output, ticket, success));
    assert(!tm_mouse_output_complete(output, ticket, success));
}

static void test_report_contract(void)
{
    tm_mouse_report_t report = {99, 88, 0xff, TM_REPORT_CURRENT};
    assert(!tm_mouse_report_make(-128, 0, 0, &report));
    assert(!tm_mouse_report_valid(&report));
    assert(!tm_mouse_report_make(0, 128, 0, &report));
    assert(!tm_mouse_report_make(0, 0, 4, &report));
    assert(!tm_mouse_report_make(0, 0, 0, NULL));
    assert(tm_mouse_report_make(-127, 127, TM_BUTTON_MASK, &report));
    assert(tm_mouse_report_valid(&report));
    assert(report.dx == -127 && report.dy == 127);
    assert(report.buttons == TM_BUTTON_MASK);
    report.kind = TM_REPORT_RELEASE_ALL;
    assert(!tm_mouse_report_valid(&report));
    report = tm_mouse_report_release();
    assert(tm_mouse_report_valid(&report));
    assert(report.dx == 0 && report.dy == 0 && report.buttons == 0);
    assert(!tm_mouse_report_valid(NULL));
}

static void test_backpressure_latest_only(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    fake.ready = false;
    tm_mouse_report_t r = make(4, 0, 0);
    tm_publish_result_t res = tm_mouse_output_publish(&out, &r, 0);
    assert(res.motion == TM_MOTION_DROPPED && res.reason == TM_REASON_NOT_READY);
    r = make(5, 0, 0);
    res = tm_mouse_output_publish(&out, &r, 0);
    assert(res.motion == TM_MOTION_DROPPED);
    fake.ready = true;
    r = make(6, 0, 0);
    res = tm_mouse_output_publish(&out, &r, 0);
    assert(res.motion == TM_MOTION_ACCEPTED && res.reason == TM_REASON_OK);
    assert(fake.count == 1 && fake.submitted[0].dx == 6);
    complete(&out, true);
    tm_mouse_output_service(&out);
    assert(fake.count == 1);
}

static void test_busy_drag_and_release(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(0, 0, TM_BUTTON_LEFT);
    tm_mouse_output_publish(&out, &r, 0);
    complete(&out, true);
    r = make(7, -2, TM_BUTTON_LEFT);
    assert(tm_mouse_output_publish(&out, &r, 0).motion == TM_MOTION_ACCEPTED);
    r = make(8, -3, TM_BUTTON_LEFT);
    assert(tm_mouse_output_publish(&out, &r, 0).motion == TM_MOTION_DROPPED);
    r = make(0, 0, 0);
    assert(tm_mouse_output_publish(&out, &r, 0).reason == TM_REASON_NOT_READY);
    complete(&out, true);
    tm_mouse_output_service(&out);
    assert(fake.count == 3);
    assert(fake.submitted[2].buttons == 0);
    assert(fake.submitted[2].dx == 0 && fake.submitted[2].dy == 0);
    complete(&out, true);
    assert(!tm_mouse_output_status(&out).release_pending);
}

static void test_release_retry_repress_barrier(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(0, 0, TM_BUTTON_LEFT);
    tm_mouse_output_publish(&out, &r, 0);
    complete(&out, true);

    fake.accepts = false;
    r = make(0, 0, 0);
    assert(tm_mouse_output_publish(&out, &r, 0).reason == TM_REASON_SUBMIT_FAILED);
    assert(tm_mouse_output_status(&out).release_pending);

    fake.accepts = true;
    r = make(9, 0, TM_BUTTON_LEFT);
    const tm_publish_result_t barrier = tm_mouse_output_publish(&out, &r, 0);
    assert(barrier.reason == TM_REASON_RELEASE_FIRST);
    assert(barrier.motion == TM_MOTION_DROPPED);
    assert(fake.submitted[1].buttons == 0 && fake.submitted[1].dx == 0);
    complete(&out, true);
    tm_mouse_output_service(&out);
    assert(fake.submitted[2].buttons == TM_BUTTON_LEFT);
    assert(fake.submitted[2].dx == 0);
    complete(&out, true);
    assert(fake.count == 3);
}

static void test_partial_release_keeps_drag(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(1, 2, TM_BUTTON_MASK);
    tm_mouse_output_publish(&out, &r, 0);
    complete(&out, true);
    r = make(3, 4, TM_BUTTON_RIGHT);
    assert(tm_mouse_output_publish(&out, &r, 0).motion == TM_MOTION_ACCEPTED);
    assert(fake.submitted[1].buttons == TM_BUTTON_RIGHT);
    assert(fake.submitted[1].dx == 3 && fake.submitted[1].dy == 4);
    complete(&out, true);
    assert(!tm_mouse_output_status(&out).release_pending);
}

static void test_invalid_and_expiry(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(0, 0, TM_BUTTON_LEFT);
    tm_mouse_output_publish(&out, &r, 0);
    complete(&out, true);
    fake.now = 30;
    r = make(20, -10, TM_BUTTON_RIGHT);
    tm_publish_result_t res = tm_mouse_output_publish(&out, &r, 20);
    assert(res.motion == TM_MOTION_DROPPED && res.reason == TM_REASON_EXPIRED);
    assert(fake.submitted[1].buttons == TM_BUTTON_RIGHT);
    assert(fake.submitted[1].dx == 0 && fake.submitted[1].dy == 0);
    complete(&out, true);

    res = tm_mouse_output_publish(&out, NULL, 0);
    assert(res.state == TM_SAFE_RELEASE_RETAINED);
    assert(res.motion == TM_MOTION_DROPPED);
    assert(fake.submitted[2].buttons == 0 && fake.submitted[2].dx == 0);
    complete(&out, true);
    assert(!tm_mouse_output_status(&out).release_pending);
}

static void test_failure_never_replays_movement(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(12, 6, TM_BUTTON_LEFT);
    tm_mouse_output_publish(&out, &r, 0);
    complete(&out, false);
    tm_mouse_output_service(&out);
    assert(fake.count == 2);
    assert(fake.submitted[1].buttons == TM_BUTTON_LEFT);
    assert(fake.submitted[1].dx == 0 && fake.submitted[1].dy == 0);
    complete(&out, true);
    assert(fake.count == 2);
}

static void test_stale_ticket_and_link_reset(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_report_t r = make(1, 0, TM_BUTTON_LEFT);
    tm_mouse_output_publish(&out, &r, 0);
    uint64_t ticket = 0;
    assert(tm_mouse_output_inflight(&out, &ticket, NULL));
    tm_mouse_output_link_lost(&out);
    assert(!tm_mouse_output_complete(&out, ticket, true));
    assert(tm_mouse_output_status(&out).release_pending);
    tm_mouse_output_service(&out);
    assert(fake.submitted[1].buttons == 0 && fake.submitted[1].dx == 0);
    uint64_t next = 0;
    assert(tm_mouse_output_inflight(&out, &next, NULL) && next != ticket);
    assert(!tm_mouse_output_complete(&out, ticket, true));
    assert(tm_mouse_output_complete(&out, next, true));
    assert(!tm_mouse_output_status(&out).release_pending);
}

static void test_router_and_quiesce(void)
{
    tm_mouse_output_t out;
    fake_backend_t fake;
    setup(&out, &fake);
    tm_mouse_router_t router;
    assert(tm_mouse_router_init(
        &router, TM_ROUTE_USB, tm_mouse_output_transport(&out)));
    tm_mouse_report_t r = make(5, 2, TM_BUTTON_LEFT);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_ESPNOW, &r, 0).reason == TM_REASON_INACTIVE);
    assert(fake.count == 0);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).motion == TM_MOTION_ACCEPTED);
    complete(&out, true);
    tm_mouse_router_begin_quiesce(&router);
    assert(tm_mouse_router_publish(
        &router, TM_ROUTE_USB, &r, 0).reason == TM_REASON_INACTIVE);
    tm_mouse_router_service(&router);
    assert(fake.count == 2);
    assert(fake.submitted[1].buttons == 0 && fake.submitted[1].dx == 0);
    complete(&out, true);
    assert(tm_mouse_router_status(&router).quiesced);
}

int main(void)
{
    test_report_contract();
    test_backpressure_latest_only();
    test_busy_drag_and_release();
    test_release_retry_repress_barrier();
    test_partial_release_keeps_drag();
    test_invalid_and_expiry();
    test_failure_never_replays_movement();
    test_stale_ticket_and_link_reset();
    test_router_and_quiesce();
    return 0;
}
