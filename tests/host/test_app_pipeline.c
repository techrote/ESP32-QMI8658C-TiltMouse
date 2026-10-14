#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tiltmouse/app_pipeline.h"

typedef struct {
    unsigned calls;
    unsigned serviced;
    unsigned quiesce_calls;
    tm_mouse_report_t reports[256];
    bool drop;
    bool quiesced;
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

static bool fake_begin_quiesce(void *ctx)
{
    fake_transport_t *f = ctx;
    ++f->quiesce_calls;
    return true;
}

static bool fake_is_quiesced(void *ctx)
{
    return ((fake_transport_t *)ctx)->quiesced;
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
        .begin_quiesce = fake_begin_quiesce,
        .is_quiesced = fake_is_quiesced,
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
    /* A fault invalidates the old baseline: fully recalibrate before
     * a new physical press can become logical state again. */
    for (unsigned i = 0; i < 16; ++i) {
        touch(&app, 184000 + UINT64_C(8000) * i, 1000, 1000, 1000);
    }
    touch(&app, 312000, 2000, 1000, 2000);
    touch(&app, 320000, 2000, 1000, 2000);
    r = next(&app, 320000);
    assert(r.kind == TM_MOUSE_REPORT_CURRENT &&
           r.buttons == TM_MOUSE_BUTTON_ALL);

    tm_app_imu_error(&app);
    r = next(&app, 328000);
    assert(r.dx == 0 && r.dy == 0 &&
           r.buttons == TM_MOUSE_BUTTON_ALL);
    assert(!app.orientation.neutral_valid);
}

static void test_operator_button_locations(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    assert(app.config.swap_touch_buttons);
    calibrate_touch(&app);
    calibrate_imu(&app);
    touch(&app, 136000, 2000, 1000, 1000);
    touch(&app, 144000, 2000, 1000, 1000);
    assert(next(&app, 144000).buttons == TM_MOUSE_BUTTON_RIGHT);
    touch(&app, 152000, 1000, 1000, 1000);
    touch(&app, 160000, 1000, 1000, 1000);
    assert(next(&app, 160000).buttons == 0);
    touch(&app, 168000, 1000, 1000, 2000);
    touch(&app, 176000, 1000, 1000, 2000);
    assert(next(&app, 176000).buttons == TM_MOUSE_BUTTON_LEFT);
    touch(&app, 184000, 2000, 1000, 2000);
    touch(&app, 192000, 2000, 1000, 2000);
    assert(next(&app, 192000).buttons == TM_MOUSE_BUTTON_ALL);
    touch(&app, 200000, 1000, 9000, 1000);
    touch(&app, 208000, 1000, 9000, 1000);
    assert(next(&app, 208000).buttons == 0);

    /* Same physical fusion supports the original mapping when explicitly set. */
    tm_app_config_t cfg = tm_app_default_config();
    cfg.swap_touch_buttons = false;
    assert(tm_app_init(&app, &cfg, TM_MOUSE_TRANSPORT_USB, app.output));
    calibrate_touch(&app);
    touch(&app, 136000, 2000, 1000, 1000);
    touch(&app, 144000, 2000, 1000, 1000);
    assert(next(&app, 144000).buttons == TM_MOUSE_BUTTON_LEFT);
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

static void test_touch_fault_requires_recalibration_and_single_release(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    calibrate_touch(&app);
    calibrate_imu(&app);
    assert(app.touch_valid);

    tm_app_touch_error(&app);
    assert(!app.touch_valid && app.touch_recalibrations == 1u);
    tm_mouse_report_t r = next(&app, 140000);
    assert(r.kind == TM_MOUSE_REPORT_RELEASE_ALL);
    assert(!tm_app_make_report(&app, 141000, &r));

    for (unsigned i = 0; i < 15; ++i) {
        touch(&app, 142000 + UINT64_C(8000) * i, 1000, 1000, 1000);
        assert(!app.touch_valid);
    }
    touch(&app, 142000 + UINT64_C(8000) * 15u, 1000, 1000, 1000);
    assert(app.touch_valid);
    assert(app.touch_recoveries == 1u);
}

static void test_stale_sources_count_once_and_recover(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    calibrate_touch(&app);
    calibrate_imu(&app);
    tm_mouse_report_t r = next(&app, 128000);
    (void)r;

    assert(tm_app_make_report(&app, 240001, &r));
    assert(app.imu_stale == 1u && app.touch_stale == 1u);
    assert(!app.imu_valid && !app.touch_valid);
    assert(r.kind == TM_MOUSE_REPORT_RELEASE_ALL);
    assert(!tm_app_make_report(&app, 241000, &r));
    assert(app.imu_stale == 1u && app.touch_stale == 1u);

    sample_imu(&app, 244000, 0.0f);
    assert(app.imu_recoveries == 1u);
}

static tm_app_output_t fake_output(
    fake_transport_t *fake, tm_mouse_transport_id_t owner)
{
    return (tm_app_output_t){
        .context = fake,
        .owner = owner,
        .publish = fake_publish,
        .service = fake_service,
        .begin_quiesce = fake_begin_quiesce,
        .is_quiesced = fake_is_quiesced,
    };
}

static void test_switch_waits_for_quiesce_and_all_up_rearm(void)
{
    tm_app_pipeline_t app;
    fake_transport_t usb, radio;
    setup(&app, &usb, TM_MOUSE_TRANSPORT_USB);
    memset(&radio, 0, sizeof(radio));
    calibrate_touch(&app);
    calibrate_imu(&app);

    tm_app_output_t next_output = fake_output(&radio, TM_MOUSE_TRANSPORT_ESPNOW);
    assert(tm_app_request_output_switch(&app, next_output));
    assert(app.switch_pending && usb.quiesce_calls == 1u);
    tm_mouse_publish_result_t result = {0};
    assert(!tm_app_dispatch(&app, 130000, &result));

    tm_app_service(&app);
    assert(app.output.owner == TM_MOUSE_TRANSPORT_USB);
    usb.quiesced = true;
    tm_app_service(&app);
    assert(app.output.owner == TM_MOUSE_TRANSPORT_ESPNOW);
    assert(!app.switch_pending && app.switch_rearm);
    assert(app.switch_completions == 1u);
    assert(radio.serviced == 1u);

    /* Held input does not migrate to the new host. */
    touch(&app, 136000, 2000, 1000, 1000);
    touch(&app, 144000, 2000, 1000, 1000);
    assert(app.buttons.left_pressed);
    assert(!tm_app_dispatch(&app, 144000, &result));
    assert(radio.calls == 0u);

    /* Fresh all-up input arms the new transport with zero motion only. */
    touch(&app, 152000, 1000, 1000, 1000);
    touch(&app, 160000, 1000, 1000, 1000);
    assert(!app.buttons.left_pressed);
    assert(tm_app_dispatch(&app, 160000, &result));
    assert(radio.calls == 1u);
    assert(radio.reports[0].buttons == 0u);
    assert(radio.reports[0].dx == 0 && radio.reports[0].dy == 0);
    assert(!app.switch_rearm);
}

static void test_diagnostics_snapshot(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);
    calibrate_touch(&app);
    calibrate_imu(&app);
    tm_mouse_publish_result_t result = {0};
    assert(tm_app_dispatch(&app, 130000, &result));

    tm_app_diagnostics_t diag;
    tm_app_diagnostics(&app, 132000, &diag);
    assert(diag.selected == TM_MOUSE_TRANSPORT_USB);
    assert(diag.reports_published == 1u);
    assert(diag.imu_age_us == 8000u);
    assert(diag.touch_age_us == 4000u);
    assert(diag.neutral_valid && diag.touch_valid);
}

static void test_click_motion_window_tracks_post_transition_peak(void)
{
    tm_app_pipeline_t app;
    fake_transport_t fake;
    setup(&app, &fake, TM_MOUSE_TRANSPORT_USB);

    /* Construct a fresh, calibrated logical state without relying on the
     * physical touch fixture; this isolates telemetry arithmetic. */
    app.touch_valid = true;
    app.last_touch_us = 100000;
    app.buttons.calibrated = true;
    app.buttons.left_pressed = true;
    app.imu_valid = true;
    app.last_imu_us = 100000;
    app.orientation.initialized = true;
    app.orientation.neutral_valid = true;
    app.orientation.neutral_roll_deg = 0.0f;
    app.orientation.neutral_pitch_deg = 0.0f;
    app.orientation.roll_deg = 20.0f;
    app.orientation.pitch_deg = 0.0f;
    app.last_report_us = 92000;
    app.inhibit_motion_once = false;

    tm_mouse_report_t r = next(&app, 100000);
    assert(r.buttons == TM_MOUSE_BUTTON_RIGHT);
    assert(app.last_button_transition_us == 100000);
    assert(app.click_motion_window_until_us == 200000);
    assert(app.click_motion_samples == 1u);

    const int8_t first_peak = app.click_peak_dx;
    app.orientation.roll_deg = 35.0f;
    app.last_imu_us = 108000;
    r = next(&app, 108000);
    assert(r.buttons == TM_MOUSE_BUTTON_RIGHT);
    assert(app.click_motion_samples == 2u);
    assert((app.click_peak_dx < 0 ? -app.click_peak_dx : app.click_peak_dx) >=
           (first_peak < 0 ? -first_peak : first_peak));

    /* Reports after the 100-ms observation window leave the retained peak. */
    const int8_t retained = app.click_peak_dx;
    const uint32_t samples = app.click_motion_samples;
    app.orientation.roll_deg = 35.0f;
    app.last_imu_us = 208001;
    app.last_touch_us = 208001;
    (void)next(&app, 208001);
    assert(app.click_motion_samples == samples);
    assert(app.click_peak_dx == retained);

    tm_app_diagnostics_t diag;
    tm_app_diagnostics(&app, 208001, &diag);
    assert(diag.last_button_transition_us == 100000);
    assert(diag.click_motion_window_until_us == 200000);
    assert(diag.click_motion_samples == samples);
    assert(diag.click_peak_dx == retained);
}

int main(void)
{
    test_startup_neutral_and_stale_sources();
    test_tilt_movement_and_recentre();
    test_buttons_reference_failure_and_drag();
    test_operator_button_locations();
    test_single_transport_and_latest_only();
    test_reject_reused_timestamps();
    test_touch_fault_requires_recalibration_and_single_release();
    test_stale_sources_count_once_and_recover();
    test_switch_waits_for_quiesce_and_all_up_rearm();
    test_diagnostics_snapshot();
    test_click_motion_window_tracks_post_transition_peak();
    return 0;
}
