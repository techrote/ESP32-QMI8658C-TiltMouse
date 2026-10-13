#include "tiltmouse/gyro_bias.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TM_BIAS_ACCEL_MIN_G 0.75f
#define TM_BIAS_ACCEL_MAX_G 1.25f
#define TM_BIAS_ACCEL_SPAN_G 0.10f
#define TM_BIAS_GYRO_SPAN_DPS 4.0f
#define TM_BIAS_RAW_LIMIT_DPS 16.0f

void tm_gyro_bias_reset(tm_gyro_bias_t *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static void reject_window(tm_gyro_bias_t *state)
{
    uint32_t rejected = state->rejected_windows;
    if ((state->samples != 0u || state->ready) && rejected != UINT32_MAX) {
        ++rejected;
    }
    tm_gyro_bias_reset(state);
    state->rejected_windows = rejected;
}

static bool finite_sample(const float accel_g[3], const float gyro_dps[3])
{
    for (size_t i = 0; i < 3u; ++i) {
        if (!isfinite(accel_g[i]) || !isfinite(gyro_dps[i])) {
            return false;
        }
    }
    return true;
}

tm_gyro_bias_result_t tm_gyro_bias_update(
    tm_gyro_bias_t *state, const float accel_g[3], const float gyro_dps[3],
    uint64_t timestamp_us, float corrected_dps[3])
{
    if (corrected_dps != NULL) {
        for (size_t i = 0; i < 3u; ++i) {
            corrected_dps[i] = 0.0f;
        }
    }
    if (state == NULL) {
        return TM_GYRO_BIAS_INVALID_SAMPLE;
    }
    if (accel_g == NULL || gyro_dps == NULL || corrected_dps == NULL ||
        timestamp_us == 0u || !finite_sample(accel_g, gyro_dps) ||
        (state->last_us != 0u && timestamp_us <= state->last_us)) {
        reject_window(state);
        return TM_GYRO_BIAS_INVALID_SAMPLE;
    }
    if (state->last_us != 0u &&
        timestamp_us - state->last_us > TM_GYRO_BIAS_MAX_GAP_US) {
        reject_window(state);
    }
    state->last_us = timestamp_us;

    if (!state->ready) {
        const float norm_sq = accel_g[0] * accel_g[0] +
            accel_g[1] * accel_g[1] + accel_g[2] * accel_g[2];
        bool usable = isfinite(norm_sq) &&
            norm_sq >= TM_BIAS_ACCEL_MIN_G * TM_BIAS_ACCEL_MIN_G &&
            norm_sq <= TM_BIAS_ACCEL_MAX_G * TM_BIAS_ACCEL_MAX_G;
        for (size_t i = 0; i < 3u; ++i) {
            usable = usable && fabsf(gyro_dps[i]) <= TM_BIAS_RAW_LIMIT_DPS;
        }
        if (!usable) {
            reject_window(state);
            state->last_us = timestamp_us;
            return TM_GYRO_BIAS_PENDING;
        }

        bool restart = state->samples >= TM_GYRO_BIAS_MAX_SAMPLES;
        for (size_t i = 0; i < 3u && state->samples != 0u; ++i) {
            const float accel_lo = fminf(state->accel_min[i], accel_g[i]);
            const float accel_hi = fmaxf(state->accel_max[i], accel_g[i]);
            const float gyro_lo = fminf(state->gyro_min[i], gyro_dps[i]);
            const float gyro_hi = fmaxf(state->gyro_max[i], gyro_dps[i]);
            restart = restart || accel_hi - accel_lo > TM_BIAS_ACCEL_SPAN_G ||
                gyro_hi - gyro_lo > TM_BIAS_GYRO_SPAN_DPS;
        }
        if (restart) {
            reject_window(state);
            state->last_us = timestamp_us;
        }
        if (state->samples == 0u) {
            state->first_us = timestamp_us;
            for (size_t i = 0; i < 3u; ++i) {
                state->accel_min[i] = state->accel_max[i] = accel_g[i];
                state->gyro_min[i] = state->gyro_max[i] = gyro_dps[i];
            }
        }
        for (size_t i = 0; i < 3u; ++i) {
            state->accel_min[i] = fminf(state->accel_min[i], accel_g[i]);
            state->accel_max[i] = fmaxf(state->accel_max[i], accel_g[i]);
            state->gyro_min[i] = fminf(state->gyro_min[i], gyro_dps[i]);
            state->gyro_max[i] = fmaxf(state->gyro_max[i], gyro_dps[i]);
            state->gyro_sum[i] += gyro_dps[i];
        }
        ++state->samples;
        if (state->samples < TM_GYRO_BIAS_MIN_SAMPLES ||
            timestamp_us - state->first_us < TM_GYRO_BIAS_WINDOW_US) {
            return TM_GYRO_BIAS_PENDING;
        }
        for (size_t i = 0; i < 3u; ++i) {
            state->bias_dps[i] = state->gyro_sum[i] / (float)state->samples;
        }
        state->ready = true;
        state->qualified_us = timestamp_us;
    }
    for (size_t i = 0; i < 3u; ++i) {
        corrected_dps[i] = gyro_dps[i] - state->bias_dps[i];
    }
    return TM_GYRO_BIAS_READY;
}
