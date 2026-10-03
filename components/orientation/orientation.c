#include "tiltmouse/orientation.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TILTMOUSE_RAD_TO_DEG 57.295779513082320876f

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

static bool finite_sample(const tiltmouse_imu_sample_t *sample)
{
    for (size_t i = 0; i < 3; ++i) {
        if (!isfinite(sample->accel_g[i]) || !isfinite(sample->gyro_dps[i])) {
            return false;
        }
    }
    return true;
}

static bool accel_angles(
    const tiltmouse_orientation_config_t *config,
    const float accel_g[3],
    float *roll_deg,
    float *pitch_deg)
{
    const float ax = accel_g[0];
    const float ay = accel_g[1];
    const float az = accel_g[2];
    const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
    if (!isfinite(magnitude) || magnitude < config->accel_min_g ||
        magnitude > config->accel_max_g) {
        return false;
    }

    *roll_deg = atan2f(ay, az) * TILTMOUSE_RAD_TO_DEG;
    *pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * TILTMOUSE_RAD_TO_DEG;
    *roll_deg = clampf(*roll_deg, -config->angle_limit_deg, config->angle_limit_deg);
    *pitch_deg = clampf(*pitch_deg, -config->angle_limit_deg, config->angle_limit_deg);
    return true;
}

tiltmouse_orientation_config_t tiltmouse_orientation_default_config(void)
{
    return (tiltmouse_orientation_config_t){
        .complementary_tau_s = 0.50f,
        .accel_min_g = 0.75f,
        .accel_max_g = 1.25f,
        .max_dt_s = 0.050f,
        .angle_limit_deg = 85.0f,
    };
}

bool tiltmouse_orientation_config_valid(const tiltmouse_orientation_config_t *config)
{
    return config != NULL && isfinite(config->complementary_tau_s) &&
           config->complementary_tau_s > 0.0f && isfinite(config->accel_min_g) &&
           config->accel_min_g > 0.0f && isfinite(config->accel_max_g) &&
           config->accel_max_g > config->accel_min_g && isfinite(config->max_dt_s) &&
           config->max_dt_s > 0.0f && isfinite(config->angle_limit_deg) &&
           config->angle_limit_deg > 0.0f && config->angle_limit_deg <= 90.0f;
}

void tiltmouse_orientation_reset(
    tiltmouse_orientation_state_t *state,
    const tiltmouse_orientation_config_t *config)
{
    if (state == NULL) {
        return;
    }

    memset(state, 0, sizeof(*state));
    if (tiltmouse_orientation_config_valid(config)) {
        state->config = *config;
    } else {
        state->config = tiltmouse_orientation_default_config();
    }
}

tiltmouse_orientation_status_t tiltmouse_orientation_update(
    tiltmouse_orientation_state_t *state,
    const tiltmouse_imu_sample_t *sample)
{
    if (state == NULL || sample == NULL || !finite_sample(sample)) {
        return TILTMOUSE_ORIENTATION_ERR_INVALID_ARGUMENT;
    }
    if (!tiltmouse_orientation_config_valid(&state->config)) {
        return TILTMOUSE_ORIENTATION_ERR_INVALID_CONFIG;
    }

    float accel_roll_deg = 0.0f;
    float accel_pitch_deg = 0.0f;
    const bool accel_valid = accel_angles(
        &state->config, sample->accel_g, &accel_roll_deg, &accel_pitch_deg);

    if (!state->initialized) {
        if (!accel_valid) {
            return TILTMOUSE_ORIENTATION_ERR_ACCEL_UNUSABLE;
        }

        state->roll_deg = accel_roll_deg;
        state->pitch_deg = accel_pitch_deg;
        state->last_timestamp_us = sample->timestamp_us;
        state->initialized = true;
        return TILTMOUSE_ORIENTATION_INITIALIZED;
    }

    if (sample->timestamp_us <= state->last_timestamp_us) {
        return TILTMOUSE_ORIENTATION_ERR_NON_MONOTONIC_TIME;
    }

    const float dt_s =
        (float)(sample->timestamp_us - state->last_timestamp_us) / 1000000.0f;
    state->last_timestamp_us = sample->timestamp_us;

    if (dt_s > state->config.max_dt_s) {
        if (accel_valid) {
            state->roll_deg = accel_roll_deg;
            state->pitch_deg = accel_pitch_deg;
        }
        return TILTMOUSE_ORIENTATION_GAP_REBASED;
    }

    float roll_deg = state->roll_deg + sample->gyro_dps[0] * dt_s;
    float pitch_deg = state->pitch_deg + sample->gyro_dps[1] * dt_s;

    if (accel_valid) {
        const float alpha = state->config.complementary_tau_s /
                            (state->config.complementary_tau_s + dt_s);
        roll_deg = alpha * roll_deg + (1.0f - alpha) * accel_roll_deg;
        pitch_deg = alpha * pitch_deg + (1.0f - alpha) * accel_pitch_deg;
    }

    state->roll_deg = clampf(
        roll_deg, -state->config.angle_limit_deg, state->config.angle_limit_deg);
    state->pitch_deg = clampf(
        pitch_deg, -state->config.angle_limit_deg, state->config.angle_limit_deg);
    return TILTMOUSE_ORIENTATION_OK;
}

bool tiltmouse_orientation_capture_neutral(tiltmouse_orientation_state_t *state)
{
    if (state == NULL || !state->initialized) {
        return false;
    }

    state->neutral_roll_deg = state->roll_deg;
    state->neutral_pitch_deg = state->pitch_deg;
    state->neutral_valid = true;
    return true;
}

void tiltmouse_orientation_clear_neutral(tiltmouse_orientation_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->neutral_roll_deg = 0.0f;
    state->neutral_pitch_deg = 0.0f;
    state->neutral_valid = false;
}

tiltmouse_orientation_pose_t tiltmouse_orientation_pose(
    const tiltmouse_orientation_state_t *state)
{
    if (state == NULL) {
        return (tiltmouse_orientation_pose_t){0};
    }

    return (tiltmouse_orientation_pose_t){
        .roll_deg = state->roll_deg,
        .pitch_deg = state->pitch_deg,
        .neutral_roll_deg = state->neutral_roll_deg,
        .neutral_pitch_deg = state->neutral_pitch_deg,
        .neutral_valid = state->neutral_valid,
    };
}

const char *tiltmouse_orientation_status_string(tiltmouse_orientation_status_t status)
{
    switch (status) {
    case TILTMOUSE_ORIENTATION_OK:
        return "ok";
    case TILTMOUSE_ORIENTATION_INITIALIZED:
        return "initialized";
    case TILTMOUSE_ORIENTATION_GAP_REBASED:
        return "gap_rebased";
    case TILTMOUSE_ORIENTATION_ERR_INVALID_ARGUMENT:
        return "invalid_argument";
    case TILTMOUSE_ORIENTATION_ERR_INVALID_CONFIG:
        return "invalid_config";
    case TILTMOUSE_ORIENTATION_ERR_ACCEL_UNUSABLE:
        return "accel_unusable";
    case TILTMOUSE_ORIENTATION_ERR_NON_MONOTONIC_TIME:
        return "non_monotonic_time";
    default:
        return "unknown";
    }
}
