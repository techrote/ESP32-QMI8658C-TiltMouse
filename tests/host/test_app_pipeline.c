#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tiltmouse/app_pipeline.h"

typedef struct {
    unsigned calls;
    unsigned serviced;
    tm_mouse_report_t reports[256];
    bool drop;
} fake_transport_t;

static tm_mouse_publish_result_t fake_publish(
    void *ctx, const tm_mouse_report_t *report, uint64_t now, uint64_t deadline)
{
    fake_transport_t *f = ctx;
    assert(now == deadline);
    assert(tm_mouse_report_is_valid(report));
    assert(f->calls < 256u);
    f->reports[f->calls++] = *report;
    return (tm_mouse_publish_result_t){
        .motion = f->drop ? TM_MOUSE_MOTION_DROPPED : TM_MOUSE_MOTION_ACCEPTED,
        .state = TM_MOUSE_STATE_BUTTONS_RETAINED,
        .reason = f->drop ? TM_MOUSE_REASON_NOT_READY : TM_MOUSE_REASON_OK,
    };
}

static tm_mouse_publish_result_t fake_service(void *ctx)
{
    ++((fake_transport_t *)ctx)->serviced;
    return (tm_mouse_publish_result_t){0};
}

static void setup(tm_app_pipeline_t *app, fake_transport_t *fake,
                  tm_mouse_transport_id_t route)
{
    memset(fake, 0, sizeof(*fake));
    const tm_app_output_t sink = {
        .context = fake,
        .owner = route,
        .publish = fake_publish,
        .service = fake_service,
    };
    tm_app_config_t config = tm_app_default_config();
    config.startup_quiet_samples = 4;
    assert(tm_app_init(app, &config, route, sink));
    assert(!tm_app_init(app, &config, TM_MOUSE_TRANSPORT_ESPNOW,
                        (tm_app_output_t){.owner = TM_MOUSE_TRANSPORT_USB,
                                          .publish = fake_publish,
                                          .service = fake_service}));
}

static tiltmouse_imu_sample_t imu(uint64_t time, float roll_rate)
{
    return (tiltmouse_imu_sample_t){
        .timestamp_us = time,
        .accel_g = {0.0f, 0.0f, 1.0f},
        .gyro_dps = {roll_rate, 0.0f, 0.0f},
    };
}

static void sample_imu(tm_app_pipeline_t *app, uint64_t time, float roll_rate)
{
    const tiltmouse_imu_sample_t sample = imu(time, roll_rate);
    assert(tm_app_on_imu(app, &sample));
}

static void calibrate_imu(tm_app_pipeline_t *app)
{
    for (unsigned i = 1; i <= 4; ++i) {
        sample_imu(app, UINT64_C(108000) + UINT64_C(4000) * i, 0.0f);
    }
    assert(app->orientation.neutral_valid);
}

static void touch(tm_app_pipeline_t *app, uint64_t time,
                  uint32_t left, uint32_t ref, uint32_t right)
{
    const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {
        left, left, left, ref, right, right, right
    };
    assert(tm_app_on_touch(app, raw, time));
}

static void calibrate_touch(tm_app_pipeline_t *app)
{
    for (unsigned i = 1; i <= 16; ++i) {
        touch(app, UINT64_C(8000) * i, 1000, 1000, 1000);
    }
    assert(app->touch_valid);
}

static tm_mouse_report_t next(tm_app_pipeline_t *app, uint64_t now_us)
{
    tm_mouse_report_t report = {77, 66, 0xffu, TM_MOUSE_REPORT_CURRENT};
    assert(tm_app_make_report(app, now_us, &report));
    assert(tm_mouse_report_is_valid(&report));
    return report;
}

static void test_startup_neutral_and_stale_sources(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    tm_mouse_report_t r = next(&app, 1000);
    assert(r.kind == TM_MOUSE_REPORT_RELEASE_ALL);
    assert(!tm_app_make_report(&app, 2000, &r));
    assert(r.kind == TM_MOUSE_REPORT_INVALID);

    calibrate_touch(&app);
    r = next(&app, 130000);
    assert(r.kind == TM_MOUSE_REPORT_CURRENT && r.dx == 0 && r.dy == 0);
    calibrate_imu(&app);
    r = next(&app, 138000);
    assert(r.dx == 0 && r.dy == 0);

    /* Creating a report does not refresh old sensor data. */
    r = next(&app, 250000);
    assert(r.kind == TM_MOUSE_REPORT_RELEASE_ALL);
    assert(!tm_app_make_report(&app, 258000, &r));
    assert(r.kind == TM_MOUSE_REPORT_INVALID);
}

static void test_tilt_movement_and_recentre(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    calibrate_touch(&app);
    calibrate_imu(&app);
    tm_mouse_report_t r = next(&app, 128000);
    assert(r.dx == 0 && r.dy == 0);
    bool moved = false;
    for (unsigned i = 0; i < 24; ++i) {
        const uint64_t t = UINT64_C(132000) + UINT64_C(4000) * i;
        sample_imu(&app, t, 300.0f);
        touch(&app, t, 1000, 1000, 1000);
        if ((i & 1u) != 0u) {
            r = next(&app, t);
            moved |= r.dx != 0 || r.dy != 0;
        }
    }
    assert(moved);

    /* GPIO0 is ignored while held at boot until first released. */
    tm_app_boot_sample(&app, true, 230000);
    tm_app_boot_sample(&app, false, 900000);
    assert(app.recenters == 0);
    tm_app_boot_sample(&app, true, 910000);
    tm_app_boot_sample(&app, false, 910100);
    assert(app.recenters == 0);

    tm_app_boot_sample(&app, true, 920000);
    sample_imu(&app, 1620000, 0.0f);
    touch(&app, 1620000, 1000, 1000, 1000);
    tm_app_boot_sample(&app, false, 1620000);
    assert(app.recenters == 1);
    r = next(&app, 1620000);
    assert(r.dx == 0 && r.dy == 0);
    assert(app.motion.residual_x == 0.0f && app.motion.residual_y == 0.0f);
}

static void test_buttons_reference_failure_and_drag(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_ESPNOW);
    calibrate_imu(&app);
    calibrate_touch(&app);

    touch(&app, 136000, 1000, 9000, 1000);
    touch(&app, 144000, 1000, 9000, 1000);
    tm_mouse_report_t r = next(&app, 144000);
    assert(r.buttons == 0);

    touch(&app, 152000, 2000, 1000, 2000);
    touch(&app, 160000, 2000, 1000, 2000);
    r = next(&app, 160000);
    assert(r.buttons == TM_MOUSE_BUTTON_ALL);

    tm_app_touch_error(&app);
    r = next(&app, 168000);
    assert(r.kind == TM_MOUSE_REPORT_RELEASE_ALL);
    assert(!tm_app_make_report(&app, 176000, &r));
    touch(&app, 184000, 2000, 1000, 2000);
    r = next(&app, 184000);
    assert(r.kind == TM_MOUSE_REPORT_CURRENT &&
           r.buttons == TM_MOUSE_BUTTON_ALL);

    tm_app_imu_error(&app);
    r = next(&app, 192000);
    assert(r.dx == 0 && r.dy == 0 &&
           r.buttons == TM_MOUSE_BUTTON_ALL);
    assert(!app.orientation.neutral_valid);
}

static void test_single_transport_and_latest_only(void)
{
    tm_app_pipeline_t usb_app, wireless_app;
    fake_transport_t usb = {0}, wireless = {0};
    setup(&usb_app, &usb, TM_MOUSE_TRANSPORT_USB);
    setup(&wireless_app, &wireless, TM_MOUSE_TRANSPORT_ESPNOW);
    calibrate_touch(&usb_app);
    calibrate_touch(&wireless_app);
    calibrate_imu(&usb_app);
    calibrate_imu(&wireless_app);

    tm_app_service(&usb_app);
    assert(usb.serviced == 1 && wireless.serviced == 0);
    tm_mouse_publish_result_t result = {0};
    assert(tm_app_dispatch(&usb_app, 130000, &result));
    assert(usb.calls == 1 && wireless.calls == 0);
    usb.drop = true;
    assert(tm_app_dispatch(&usb_app, 138000, &result));
    assert(usb.calls == 2 && wireless.calls == 0);
    assert(usb_app.dropped_motion == 0);
    usb.drop = false;
    assert(tm_app_dispatch(&usb_app, 146000, &result));
    assert(usb.calls == 3 && wireless.calls == 0);
    assert(usb.reports[2].dx == 0 && usb.reports[2].dy == 0);

    assert(tm_app_dispatch(&wireless_app, 130000, &result));
    assert(wireless.calls == 1 && usb.calls == 3);
}

static void test_reject_reused_timestamps(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    calibrate_touch(&app);
    calibrate_imu(&app);
    const tiltmouse_imu_sample_t stale = imu(app.last_imu_us, 10.0f);
    const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {0};
    assert(!tm_app_on_imu(&app, &stale));
    assert(!tm_app_on_touch(&app, raw, app.last_touch_us));
    assert(app.last_imu_us == stale.timestamp_us);
    assert(app.last_touch_us == 128000);
}

int main(void)
{
    test_startup_neutral_and_stale_sources();
    test_tilt_movement_and_recentre();
    test_buttons_reference_failure_and_drag();
    test_single_transport_and_latest_only();
    test_reject_reused_timestamps();
    return 0;
}
