#include "tiltmouse/app_pipeline.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TM_IMU_FRESH_US UINT64_C(100000)
#define TM_TOUCH_FRESH_US UINT64_C(100000)
#define TM_REPORT_GAP_US UINT64_C(50000)
#define TM_BOOT_HOLD_MIN_US UINT64_C(600000)
#define TM_BOOT_HOLD_MAX_US UINT64_C(4000000)
#define TM_CLICK_MOTION_WINDOW_US UINT64_C(100000)

static bool recent(uint64_t last_us, uint64_t now_us, uint64_t limit_us)
{
    return last_us != 0u && now_us >= last_us && now_us - last_us <= limit_us;
}

tm_app_config_t tm_app_default_config(void)
{
    return (tm_app_config_t){
        .imu_fresh_us = TM_IMU_FRESH_US,
        .touch_fresh_us = TM_TOUCH_FRESH_US,
        .max_report_gap_us = TM_REPORT_GAP_US,
        .boot_hold_min_us = TM_BOOT_HOLD_MIN_US,
        .boot_hold_max_us = TM_BOOT_HOLD_MAX_US,
        .click_motion_window_us = TM_CLICK_MOTION_WINDOW_US,
        .startup_quiet_samples = 32,
        .startup_gyro_limit_dps = 8.0f,
        .orientation = tiltmouse_orientation_default_config(),
        .motion = tiltmouse_mouse_motion_default_config(),
        .touch = tiltmouse_touch_buttons_default_config(),
    };
}

static bool output_valid(tm_app_output_t output)
{
    return (output.owner == TM_MOUSE_TRANSPORT_USB ||
            output.owner == TM_MOUSE_TRANSPORT_ESPNOW) &&
           output.publish != NULL && output.service != NULL &&
           output.begin_quiesce != NULL && output.is_quiesced != NULL;
}

static bool config_valid(const tm_app_config_t *cfg)
{
    return cfg != NULL && cfg->imu_fresh_us > 0 &&
           cfg->touch_fresh_us > 0 && cfg->max_report_gap_us > 0 &&
           cfg->max_report_gap_us <= cfg->imu_fresh_us &&
           cfg->boot_hold_min_us > 0 &&
           cfg->boot_hold_max_us >= cfg->boot_hold_min_us &&
           cfg->click_motion_window_us > 0 &&
           cfg->startup_quiet_samples > 0 &&
           isfinite(cfg->startup_gyro_limit_dps) &&
           cfg->startup_gyro_limit_dps > 0.0f &&
           tiltmouse_orientation_config_valid(&cfg->orientation) &&
           tiltmouse_mouse_motion_config_valid(&cfg->motion);
}

bool tm_app_init(tm_app_pipeline_t *app, const tm_app_config_t *config,
                 tm_mouse_transport_id_t selected, tm_app_output_t output)
{
    if (app == NULL || !config_valid(config) ||
        (selected != TM_MOUSE_TRANSPORT_USB &&
         selected != TM_MOUSE_TRANSPORT_ESPNOW) ||
        selected != output.owner || !output_valid(output)) {
        return false;
    }
    tm_app_pipeline_t next = {0};
    next.config = *config;
    next.output = output;
    tiltmouse_orientation_reset(&next.orientation, &config->orientation);
    if (!tiltmouse_touch_buttons_init(&next.touch, &config->touch)) {
        return false;
    }
    next.inhibit_motion_once = true;
    next.initialized = true;
    *app = next;
    return true;
}

static bool quiet_imu(const tm_app_pipeline_t *app,
                      const tiltmouse_imu_sample_t *sample)
{
    const float ax = sample->accel_g[0];
    const float ay = sample->accel_g[1];
    const float az = sample->accel_g[2];
    const float magnitude_sq = ax * ax + ay * ay + az * az;
    const float lo = app->config.orientation.accel_min_g;
    const float hi = app->config.orientation.accel_max_g;
    if (!isfinite(magnitude_sq) || magnitude_sq < lo * lo ||
        magnitude_sq > hi * hi) {
        return false;
    }
    for (size_t i = 0; i < 3u; ++i) {
        if (!isfinite(sample->gyro_dps[i]) ||
            fabsf(sample->gyro_dps[i]) > app->config.startup_gyro_limit_dps) {
            return false;
        }
    }
    return true;
}

static void stop_motion(tm_app_pipeline_t *app)
{
    tiltmouse_mouse_motion_reset(&app->motion);
    app->last_report_us = 0;
    app->inhibit_motion_once = true;
}

static void invalidate_imu(tm_app_pipeline_t *app, bool driver_error)
{
    if (driver_error) {
        ++app->imu_errors;
    } else {
        ++app->imu_stale;
    }
    app->imu_valid = false;
    app->imu_faulted = true;
    app->quiet_samples = 0;
    tiltmouse_orientation_clear_neutral(&app->orientation);
    stop_motion(app);
}

void tm_app_imu_error(tm_app_pipeline_t *app)
{
    if (app == NULL || !app->initialized) {
        return;
    }
    invalidate_imu(app, true);
}

bool tm_app_on_imu(tm_app_pipeline_t *app,
                   const tiltmouse_imu_sample_t *sample)
{
    if (app == NULL || !app->initialized || sample == NULL ||
        sample->timestamp_us == 0 ||
        (app->last_imu_us != 0 && sample->timestamp_us <= app->last_imu_us)) {
        return false;
    }
    const tiltmouse_orientation_status_t status =
        tiltmouse_orientation_update(&app->orientation, sample);
    if (status != TILTMOUSE_ORIENTATION_OK &&
        status != TILTMOUSE_ORIENTATION_INITIALIZED &&
        status != TILTMOUSE_ORIENTATION_GAP_REBASED) {
        tm_app_imu_error(app);
        return false;
    }
    app->last_imu_us = sample->timestamp_us;
    if (app->imu_faulted) {
        ++app->imu_recoveries;
        app->imu_faulted = false;
    }
    app->imu_valid = true;
    if (status == TILTMOUSE_ORIENTATION_GAP_REBASED) {
        app->quiet_samples = 0;
        tiltmouse_orientation_clear_neutral(&app->orientation);
        stop_motion(app);
    }
    if (!app->orientation.neutral_valid) {
        if (quiet_imu(app, sample)) {
            if (app->quiet_samples < app->config.startup_quiet_samples) {
                ++app->quiet_samples;
            }
            if (app->quiet_samples == app->config.startup_quiet_samples &&
                tiltmouse_orientation_capture_neutral(&app->orientation)) {
                stop_motion(app);
            }
        } else {
            app->quiet_samples = 0;
        }
    }
    return true;
}

static void reset_touch_after_fault(tm_app_pipeline_t *app, bool driver_error)
{
    if (driver_error) {
        ++app->touch_errors;
    } else {
        ++app->touch_stale;
    }
    ++app->touch_recalibrations;
    app->touch_valid = false;
    app->touch_faulted = true;
    app->buttons = (tiltmouse_touch_buttons_output_t){0};
    (void)tiltmouse_touch_buttons_init(&app->touch, &app->config.touch);
    app->safety_release_latched = false;
}

void tm_app_touch_error(tm_app_pipeline_t *app)
{
    if (app != NULL && app->initialized) {
        reset_touch_after_fault(app, true);
    }
}

bool tm_app_on_touch(tm_app_pipeline_t *app,
                     const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT],
                     uint64_t timestamp_us)
{
    if (app == NULL || !app->initialized || raw == NULL ||
        timestamp_us == 0 ||
        (app->last_touch_us != 0 && timestamp_us <= app->last_touch_us)) {
        return false;
    }
    tiltmouse_touch_buttons_output_t output = {0};
    tiltmouse_touch_buttons_update(&app->touch, raw, &output);
    app->buttons = output;
    app->last_touch_us = timestamp_us;
    app->touch_valid = output.calibrated;
    if (output.calibrated && app->touch_faulted) {
        ++app->touch_recoveries;
        app->touch_faulted = false;
    }
    return true;
}

void tm_app_boot_sample(tm_app_pipeline_t *app, bool pressed, uint64_t now_us)
{
    if (app == NULL || !app->initialized) {
        return;
    }
    if (!app->boot_armed) {
        if (!pressed) {
            app->boot_armed = true;
        }
        return;
    }
    if (pressed) {
        if (!app->boot_down) {
            app->boot_down = true;
            app->boot_down_us = now_us;
        }
        return;
    }
    if (!app->boot_down) {
        return;
    }
    app->boot_down = false;
    if (now_us < app->boot_down_us) {
        return;
    }
    const uint64_t held_us = now_us - app->boot_down_us;
    if (held_us < app->config.boot_hold_min_us ||
        held_us > app->config.boot_hold_max_us) {
        return;
    }
    ++app->recenters;
    stop_motion(app);
    if (!app->imu_valid ||
        !recent(app->last_imu_us, now_us, app->config.imu_fresh_us) ||
        !tiltmouse_orientation_capture_neutral(&app->orientation)) {
        tiltmouse_orientation_clear_neutral(&app->orientation);
        app->quiet_samples = 0;
    }
}

static unsigned magnitude_i8(int8_t value)
{
    return value < 0 ? (unsigned)(-(int)value) : (unsigned)value;
}

static uint64_t saturating_add_u64(uint64_t a, uint64_t b)
{
    return UINT64_MAX - a < b ? UINT64_MAX : a + b;
}

static void update_click_peak(tm_app_pipeline_t *app, int8_t dx, int8_t dy)
{
    if (magnitude_i8(dx) > magnitude_i8(app->click_peak_dx)) {
        app->click_peak_dx = dx;
    }
    if (magnitude_i8(dy) > magnitude_i8(app->click_peak_dy)) {
        app->click_peak_dy = dy;
    }
    if (app->click_motion_samples != UINT32_MAX) {
        ++app->click_motion_samples;
    }
}

static void record_candidate(
    tm_app_pipeline_t *app, const tm_mouse_report_t *report, uint64_t now_us)
{
    const bool current = report->kind == TM_MOUSE_REPORT_CURRENT;
    const uint8_t buttons = current ? report->buttons : 0u;
    if (current && buttons != app->last_buttons) {
        app->last_button_transition_us = now_us;
        app->click_motion_window_until_us =
            saturating_add_u64(now_us, app->config.click_motion_window_us);
        app->click_motion_samples = 0u;
        app->click_peak_dx = 0;
        app->click_peak_dy = 0;
    }
    app->last_buttons = buttons;
    app->last_dx = current ? report->dx : 0;
    app->last_dy = current ? report->dy : 0;
    if (app->last_dx != 0 || app->last_dy != 0) {
        ++app->movement_reports;
    }
    if (current && app->last_button_transition_us != 0u &&
        now_us <= app->click_motion_window_until_us) {
        update_click_peak(app, app->last_dx, app->last_dy);
    }
}

bool tm_app_make_report(tm_app_pipeline_t *app, uint64_t now_us,
                        tm_mouse_report_t *report)
{
    if (app == NULL || !app->initialized || report == NULL ||
        app->switch_pending || app->switch_rearm) {
        return false;
    }
    *report = (tm_mouse_report_t){0};
    if (app->imu_valid &&
        !recent(app->last_imu_us, now_us, app->config.imu_fresh_us)) {
        invalidate_imu(app, false);
    }
    if (app->touch_valid &&
        !recent(app->last_touch_us, now_us, app->config.touch_fresh_us)) {
        reset_touch_after_fault(app, false);
    }
    if (!app->touch_valid) {
        stop_motion(app);
        if (!app->safety_release_latched) {
            app->safety_release_latched = true;
            *report = (tm_mouse_report_t){.kind = TM_MOUSE_REPORT_RELEASE_ALL};
            record_candidate(app, report, now_us);
            return true;
        }
        return false;
    }
    app->safety_release_latched = false;
    const uint32_t buttons =
        (app->buttons.left_pressed ? TM_MOUSE_BUTTON_LEFT : 0u) |
        (app->buttons.right_pressed ? TM_MOUSE_BUTTON_RIGHT : 0u);
    int8_t x = 0;
    int8_t y = 0;
    const bool fresh_imu = app->imu_valid &&
        app->orientation.neutral_valid;
    const bool new_clock = app->last_report_us != 0 &&
        now_us > app->last_report_us;
    const uint64_t dt_us = new_clock ? now_us - app->last_report_us : 0;
    if (!fresh_imu || !new_clock || app->inhibit_motion_once ||
        dt_us > app->config.max_report_gap_us) {
        tiltmouse_mouse_motion_reset(&app->motion);
    } else {
        const tiltmouse_orientation_pose_t pose =
            tiltmouse_orientation_pose(&app->orientation);
        tiltmouse_mouse_delta_t delta = {0};
        tiltmouse_mouse_velocity_t velocity = {0};
        const tiltmouse_mouse_motion_status_t status =
            tiltmouse_mouse_motion_step(
                &app->motion, &app->config.motion, &pose,
                (float)dt_us / 1000000.0f, &delta, &velocity);
        if (status == TILTMOUSE_MOUSE_MOTION_OK) {
            x = delta.x;
            y = delta.y;
        } else {
            tiltmouse_mouse_motion_reset(&app->motion);
        }
    }
    app->last_report_us = now_us;
    app->inhibit_motion_once = false;
    const bool made = tm_mouse_report_make_current(x, y, buttons, report);
    if (made) {
        record_candidate(app, report, now_us);
    }
    return made;
}

bool tm_app_dispatch(tm_app_pipeline_t *app, uint64_t now_us,
                     tm_mouse_publish_result_t *result)
{
    if (app == NULL || !app->initialized || result == NULL ||
        app->switch_pending) {
        return false;
    }
    tm_mouse_report_t candidate = {0};
    if (app->switch_rearm) {
        if (!app->touch_valid ||
            !recent(app->last_touch_us, now_us, app->config.touch_fresh_us) ||
            app->buttons.left_pressed || app->buttons.right_pressed ||
            !tm_mouse_report_make_current(0, 0, 0u, &candidate)) {
            return false;
        }
        record_candidate(app, &candidate, now_us);
    } else if (!tm_app_make_report(app, now_us, &candidate)) {
        return false;
    }
    /* A candidate is terminal: never retry it on a different transport. */
    *result = app->output.publish(
        app->output.context, &candidate, now_us, now_us);
    if (result->motion == TM_MOUSE_MOTION_DROPPED &&
        (candidate.dx != 0 || candidate.dy != 0)) {
        ++app->dropped_motion;
    }
    ++app->reports_published;
    if (app->switch_rearm &&
        result->reason != TM_MOUSE_REASON_NOT_READY &&
        result->reason != TM_MOUSE_REASON_BUSY &&
        result->reason != TM_MOUSE_REASON_SUBMIT_FAILED &&
        result->reason != TM_MOUSE_REASON_INACTIVE) {
        app->switch_rearm = false;
    }
    return true;
}

bool tm_app_request_output_switch(tm_app_pipeline_t *app, tm_app_output_t next)
{
    if (app == NULL || !app->initialized || app->switch_pending ||
        app->switch_rearm || !output_valid(next) ||
        next.owner == app->output.owner ||
        !app->output.begin_quiesce(app->output.context)) {
        return false;
    }
    app->pending_output = next;
    app->switch_pending = true;
    ++app->switch_requests;
    stop_motion(app);
    return true;
}

void tm_app_service(tm_app_pipeline_t *app)
{
    if (app == NULL || !app->initialized) {
        return;
    }
    (void)app->output.service(app->output.context);
    if (app->switch_pending &&
        app->output.is_quiesced(app->output.context)) {
        app->output = app->pending_output;
        app->pending_output = (tm_app_output_t){0};
        app->switch_pending = false;
        app->switch_rearm = true;
        ++app->switch_completions;
        stop_motion(app);
        (void)app->output.service(app->output.context);
    }
}

void tm_app_diagnostics(
    const tm_app_pipeline_t *app, uint64_t now_us, tm_app_diagnostics_t *out)
{
    if (out == NULL) {
        return;
    }
    *out = (tm_app_diagnostics_t){0};
    if (app == NULL || !app->initialized) {
        return;
    }
    out->selected = app->output.owner;
    out->imu_age_us = app->last_imu_us != 0 && now_us >= app->last_imu_us
        ? now_us - app->last_imu_us : UINT64_MAX;
    out->touch_age_us = app->last_touch_us != 0 && now_us >= app->last_touch_us
        ? now_us - app->last_touch_us : UINT64_MAX;
    out->last_button_transition_us = app->last_button_transition_us;
    out->click_motion_window_until_us = app->click_motion_window_until_us;
    out->click_motion_samples = app->click_motion_samples;
    out->reports_published = app->reports_published;
    out->movement_reports = app->movement_reports;
    out->dropped_motion = app->dropped_motion;
    out->recenters = app->recenters;
    out->imu_errors = app->imu_errors;
    out->imu_stale = app->imu_stale;
    out->imu_recoveries = app->imu_recoveries;
    out->touch_errors = app->touch_errors;
    out->touch_stale = app->touch_stale;
    out->touch_recalibrations = app->touch_recalibrations;
    out->touch_recoveries = app->touch_recoveries;
    out->switch_requests = app->switch_requests;
    out->switch_completions = app->switch_completions;
    out->last_dx = app->last_dx;
    out->last_dy = app->last_dy;
    out->click_peak_dx = app->click_peak_dx;
    out->click_peak_dy = app->click_peak_dy;
    out->last_buttons = app->last_buttons;
    out->neutral_valid = app->orientation.neutral_valid;
    out->touch_valid = app->touch_valid;
    out->switch_pending = app->switch_pending;
    out->switch_rearm = app->switch_rearm;
}

