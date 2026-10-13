#include "tiltmouse/mouse_motion.h"

#include <math.h>
#include <stddef.h>

static float clampf(float value, float lower, float upper)
{
    if (value < lower) {
        return lower;
    }
    if (value > upper) {
        return upper;
    }
    return value;
}

static bool axis_config_valid(const tiltmouse_motion_axis_config_t *axis)
{
    return axis != NULL &&
           (axis->source == TILTMOUSE_MOTION_AXIS_ROLL ||
            axis->source == TILTMOUSE_MOTION_AXIS_PITCH) &&
           isfinite(axis->deadzone_deg) && axis->deadzone_deg >= 0.0f &&
           isfinite(axis->max_tilt_deg) && axis->max_tilt_deg > axis->deadzone_deg &&
           isfinite(axis->response_exponent) && axis->response_exponent > 0.0f &&
           isfinite(axis->gain_counts_per_s) && axis->gain_counts_per_s >= 0.0f &&
           isfinite(axis->max_velocity_counts_per_s) &&
           axis->max_velocity_counts_per_s >= 0.0f;
}

tiltmouse_mouse_motion_config_t tiltmouse_mouse_motion_default_config(void)
{
    return (tiltmouse_mouse_motion_config_t){
        .x = {
            .source = TILTMOUSE_MOTION_AXIS_ROLL,
            .invert = false,
            .deadzone_deg = 3.0f,
            .max_tilt_deg = 35.0f,
            .response_exponent = 1.6f,
            .gain_counts_per_s = 900.0f,
            .max_velocity_counts_per_s = 1000.0f,
        },
        .y = {
            .source = TILTMOUSE_MOTION_AXIS_PITCH,
            .invert = false,
            .deadzone_deg = 3.0f,
            .max_tilt_deg = 35.0f,
            .response_exponent = 1.6f,
            .gain_counts_per_s = 900.0f,
            .max_velocity_counts_per_s = 1000.0f,
        },
        .max_report_dt_s = 0.050f,
        .max_delta_per_report = 127,
    };
}

bool tiltmouse_mouse_motion_config_valid(const tiltmouse_mouse_motion_config_t *config)
{
    return config != NULL && axis_config_valid(&config->x) &&
           axis_config_valid(&config->y) && isfinite(config->max_report_dt_s) &&
           config->max_report_dt_s > 0.0f && config->max_delta_per_report > 0 &&
           config->max_delta_per_report <= 127;
}

void tiltmouse_mouse_motion_reset(tiltmouse_mouse_motion_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->residual_x = 0.0f;
    state->residual_y = 0.0f;
}

static float relative_angle(
    const tiltmouse_orientation_pose_t *pose,
    tiltmouse_motion_axis_source_t source)
{
    if (source == TILTMOUSE_MOTION_AXIS_ROLL) {
        return pose->roll_deg - pose->neutral_roll_deg;
    }
    return pose->pitch_deg - pose->neutral_pitch_deg;
}

static float map_axis(
    const tiltmouse_motion_axis_config_t *config,
    const tiltmouse_orientation_pose_t *pose)
{
    float angle_deg = relative_angle(pose, config->source);
    if (config->invert) {
        angle_deg = -angle_deg;
    }

    const float magnitude = fabsf(angle_deg);
    if (magnitude <= config->deadzone_deg) {
        return 0.0f;
    }

    const float span = config->max_tilt_deg - config->deadzone_deg;
    const float normalized =
        clampf((magnitude - config->deadzone_deg) / span, 0.0f, 1.0f);
    const float response = powf(normalized, config->response_exponent);
    float velocity = response * config->gain_counts_per_s;
    velocity = fminf(velocity, config->max_velocity_counts_per_s);
    return copysignf(velocity, angle_deg);
}

tiltmouse_mouse_motion_status_t tiltmouse_mouse_motion_velocity(
    const tiltmouse_mouse_motion_config_t *config,
    const tiltmouse_orientation_pose_t *pose,
    tiltmouse_mouse_velocity_t *velocity)
{
    if (config == NULL || pose == NULL || velocity == NULL) {
        return TILTMOUSE_MOUSE_MOTION_ERR_INVALID_ARGUMENT;
    }

    *velocity = (tiltmouse_mouse_velocity_t){0};
    if (!tiltmouse_mouse_motion_config_valid(config)) {
        return TILTMOUSE_MOUSE_MOTION_ERR_INVALID_CONFIG;
    }
    if (!pose->neutral_valid) {
        return TILTMOUSE_MOUSE_MOTION_NOT_READY;
    }

    velocity->x_counts_per_s = map_axis(&config->x, pose);
    velocity->y_counts_per_s = map_axis(&config->y, pose);
    return TILTMOUSE_MOUSE_MOTION_OK;
}

static int8_t integrate_axis(
    float velocity,
    float dt_s,
    float *residual,
    uint8_t max_delta)
{
    float exact = velocity * dt_s + *residual;
    exact = clampf(exact, -(float)max_delta, (float)max_delta);
    const float whole = truncf(exact);
    *residual = exact - whole;
    return (int8_t)whole;
}

tiltmouse_mouse_motion_status_t tiltmouse_mouse_motion_step(
    tiltmouse_mouse_motion_state_t *state,
    const tiltmouse_mouse_motion_config_t *config,
    const tiltmouse_orientation_pose_t *pose,
    float report_dt_s,
    tiltmouse_mouse_delta_t *delta,
    tiltmouse_mouse_velocity_t *velocity)
{
    if (state == NULL || config == NULL || pose == NULL || delta == NULL ||
        velocity == NULL || !isfinite(report_dt_s) || report_dt_s <= 0.0f) {
        return TILTMOUSE_MOUSE_MOTION_ERR_INVALID_ARGUMENT;
    }

    *delta = (tiltmouse_mouse_delta_t){0};
    const tiltmouse_mouse_motion_status_t status =
        tiltmouse_mouse_motion_velocity(config, pose, velocity);
    if (status != TILTMOUSE_MOUSE_MOTION_OK) {
        return status;
    }

    const float bounded_dt = fminf(report_dt_s, config->max_report_dt_s);
    delta->x = integrate_axis(
        velocity->x_counts_per_s,
        bounded_dt,
        &state->residual_x,
        config->max_delta_per_report);
    delta->y = integrate_axis(
        velocity->y_counts_per_s,
        bounded_dt,
        &state->residual_y,
        config->max_delta_per_report);
    return TILTMOUSE_MOUSE_MOTION_OK;
}
