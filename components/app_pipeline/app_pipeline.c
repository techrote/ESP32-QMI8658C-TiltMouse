#include "tiltmouse/app_pipeline.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TM_IMU_FRESH_US UINT64_C(100000)
#define TM_TOUCH_FRESH_US UINT64_C(100000)
#define TM_REPORT_GAP_US UINT64_C(50000)
#define TM_BOOT_HOLD_MIN_US UINT64_C(600000)
#define TM_BOOT_HOLD_MAX_US UINT64_C(4000000)

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
        .startup_quiet_samples = 32,
        .startup_gyro_limit_dps = 8.0f,
        .orientation = tiltmouse_orientation_default_config(),
        .motion = tiltmouse_mouse_motion_default_config(),
        .touch = tiltmouse_touch_buttons_default_config(),
    };
}

static bool config_valid(const tm_app_config_t *cfg)
{
    return cfg != NULL && cfg->imu_fresh_us > 0 &&
           cfg->touch_fresh_us > 0 && cfg->max_report_gap_us > 0 &&
           cfg->max_report_gap_us <= cfg->imu_fresh_us &&
           cfg->boot_hold_min_us > 0 &&
           cfg->boot_hold_max_us >= cfg->boot_hold_min_us &&
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
        selected != output.owner || output.publish == NULL ||
        output.service == NULL) {
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

void tm_app_imu_error(tm_app_pipeline_t *app)
{
    if (app == NULL || !app->initialized) {
        return;
    }
    app->imu_valid = false;
    app->quiet_samples = 0;
    tiltmouse_orientation_clear_neutral(&app->orientation);
    stop_motion(app);
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

void tm_app_touch_error(tm_app_pipeline_t *app)
{
    if (app != NULL && app->initialized) {
        app->touch_valid = false;
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

bool tm_app_make_report(tm_app_pipeline_t *app, uint64_t now_us,
                        tm_mouse_report_t *report)
{
    if (app == NULL || !app->initialized || report == NULL) {
        return false;
    }
    *report = (tm_mouse_report_t){0};
    if (!app->touch_valid ||
        !recent(app->last_touch_us, now_us, app->config.touch_fresh_us)) {
        stop_motion(app);
        if (!app->safety_release_latched) {
            app->safety_release_latched = true;
            *report = (tm_mouse_report_t){.kind = TM_MOUSE_REPORT_RELEASE_ALL};
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
        recent(app->last_imu_us, now_us, app->config.imu_fresh_us) &&
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
    return tm_mouse_report_make_current(x, y, buttons, report);
}

bool tm_app_dispatch(tm_app_pipeline_t *app, uint64_t now_us,
                     tm_mouse_publish_result_t *result)
{
    if (app == NULL || !app->initialized || result == NULL) {
        return false;
    }
    tm_mouse_report_t candidate = {0};
    if (!tm_app_make_report(app, now_us, &candidate)) {
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
    return true;
}

void tm_app_service(tm_app_pipeline_t *app)
{
    if (app != NULL && app->initialized) {
        (void)app->output.service(app->output.context);
    }
}
