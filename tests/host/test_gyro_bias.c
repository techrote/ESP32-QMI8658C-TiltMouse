#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "tiltmouse/gyro_bias.h"

static const float flat[3] = {0.043f, 0.032f, -0.999f};
static const float measured[3] = {-1.9f, 10.3f, -0.83f};

static uint64_t qualify(tm_gyro_bias_t *state, uint64_t start)
{
    float corrected[3];
    for (unsigned n = 0; n <= 250u; ++n) {
        const uint64_t t = start + (uint64_t)n * 4000u;
        const tm_gyro_bias_result_t result =
            tm_gyro_bias_update(state, flat, measured, t, corrected);
        assert(result == (n == 250u ? TM_GYRO_BIAS_READY : TM_GYRO_BIAS_PENDING));
        for (size_t i = 0; i < 3u; ++i) {
            assert(fabsf(corrected[i]) < 0.001f);
        }
    }
    assert(state->ready && state->samples == 251u);
    for (size_t i = 0; i < 3u; ++i) {
        assert(fabsf(state->bias_dps[i] - measured[i]) < 0.001f);
    }
    return start + TM_GYRO_BIAS_WINDOW_US;
}

static void offset_freezes_and_gap_requalifies(void)
{
    tm_gyro_bias_t state = {0};
    uint64_t t = qualify(&state, 1000u);
    const uint64_t qualified = state.qualified_us;
    const float dynamic_accel[3] = {2.0f, 0.0f, -1.0f};
    const float rotation[3] = {50.0f, 80.0f, -30.0f};
    float corrected[3];
    assert(tm_gyro_bias_update(&state, dynamic_accel, rotation, t + 4000u,
        corrected) == TM_GYRO_BIAS_READY);
    for (size_t i = 0; i < 3u; ++i) {
        assert(fabsf(corrected[i] - (rotation[i] - measured[i])) < 0.001f);
        assert(fabsf(state.bias_dps[i] - measured[i]) < 0.001f);
    }
    assert(state.qualified_us == qualified && state.samples == 251u);
    t += 4000u + TM_GYRO_BIAS_MAX_GAP_US + 1u;
    assert(tm_gyro_bias_update(&state, flat, measured, t, corrected) ==
        TM_GYRO_BIAS_PENDING);
    assert(!state.ready && state.samples == 1u && state.rejected_windows == 1u);
    tm_gyro_bias_reset(&state);
    assert(!state.ready && state.samples == 0u && state.last_us == 0u);
    (void)qualify(&state, t + 4000u);
}

static void stationary_guards(void)
{
    tm_gyro_bias_t state = {0};
    float corrected[3];
    for (unsigned n = 1u; n < 600u; ++n) {
        const float changing_gravity[3] = {n % 2u ? 0.3f : -0.3f, 0, -0.95f};
        assert(tm_gyro_bias_update(&state, changing_gravity, measured,
            (uint64_t)n * 4000u, corrected) == TM_GYRO_BIAS_PENDING);
    }
    assert(!state.ready && state.rejected_windows > 0u);
    tm_gyro_bias_reset(&state);
    for (unsigned n = 1u; n < 600u; ++n) {
        const float variable_gyro[3] = {-1.9f, n % 2u ? 13.0f : 8.0f, -0.83f};
        assert(tm_gyro_bias_update(&state, flat, variable_gyro,
            (uint64_t)n * 4000u, corrected) == TM_GYRO_BIAS_PENDING);
    }
    tm_gyro_bias_reset(&state);
    const float too_fast[3] = {0, 0, 30};
    const float no_gravity[3] = {0, 0, 0};
    for (unsigned n = 1u; n < 300u; ++n) {
        assert(tm_gyro_bias_update(&state, flat, too_fast,
            (uint64_t)n * 4000u, corrected) == TM_GYRO_BIAS_PENDING);
    }
    assert(state.samples == 0u);
    assert(tm_gyro_bias_update(&state, no_gravity, measured, 2000000u,
        corrected) == TM_GYRO_BIAS_PENDING);
    assert(state.samples == 0u);
}

static void stationary_noise_is_averaged(void)
{
    tm_gyro_bias_t state = {0};
    float corrected[3];
    for (unsigned n = 0u; n <= 200u; ++n) {
        const float sign = n % 2u ? 1.0f : -1.0f;
        const float accel[3] = {0.043f + sign * 0.005f, 0.032f,
            -0.999f + sign * 0.03f};
        const float gyro[3] = {-1.9f + sign * 0.4f,
            10.3f + sign * 1.7f, -0.83f + sign * 0.25f};
        assert(tm_gyro_bias_update(&state, accel, gyro,
            1000u + (uint64_t)n * 5000u, corrected) ==
            (n == 200u ? TM_GYRO_BIAS_READY : TM_GYRO_BIAS_PENDING));
    }
    assert(state.rejected_windows == 0u);
    for (size_t i = 0; i < 3u; ++i) {
        assert(fabsf(state.bias_dps[i] - measured[i]) < 0.01f);
    }
}

static void both_duration_and_sample_count_are_required(void)
{
    tm_gyro_bias_t state = {0};
    float corrected[3];
    for (unsigned n = 1u; n <= 127u; ++n) {
        assert(tm_gyro_bias_update(&state, flat, measured,
            (uint64_t)n * 8000u, corrected) == TM_GYRO_BIAS_PENDING);
    }
    assert(state.last_us - state.first_us >= TM_GYRO_BIAS_WINDOW_US);
    assert(tm_gyro_bias_update(&state, flat, measured, 128u * 8000u,
        corrected) == TM_GYRO_BIAS_READY);

    tm_gyro_bias_reset(&state);
    for (unsigned n = 1u; n <= TM_GYRO_BIAS_MAX_SAMPLES; ++n) {
        assert(tm_gyro_bias_update(&state, flat, measured, n,
            corrected) == TM_GYRO_BIAS_PENDING);
    }
    assert(state.samples == TM_GYRO_BIAS_MAX_SAMPLES);
    assert(tm_gyro_bias_update(&state, flat, measured,
        TM_GYRO_BIAS_MAX_SAMPLES + 1u, corrected) == TM_GYRO_BIAS_PENDING);
    assert(state.samples == 1u && state.rejected_windows == 1u);
}

static void invalid_samples_discard_qualification(void)
{
    tm_gyro_bias_t state = {0};
    float corrected[3] = {1, 2, 3};
    uint64_t t = qualify(&state, 1000u);
    assert(tm_gyro_bias_update(&state, flat, measured, t,
        corrected) == TM_GYRO_BIAS_INVALID_SAMPLE);
    assert(!state.ready && state.samples == 0u);
    assert(corrected[0] == 0 && corrected[1] == 0 && corrected[2] == 0);
    const float nan_sample[3] = {NAN, 0, 0};
    assert(tm_gyro_bias_update(&state, nan_sample, measured, t + 4000u,
        corrected) == TM_GYRO_BIAS_INVALID_SAMPLE);
    assert(tm_gyro_bias_update(&state, flat, measured, 0,
        corrected) == TM_GYRO_BIAS_INVALID_SAMPLE);
    assert(tm_gyro_bias_update(&state, NULL, measured, t + 8000u,
        corrected) == TM_GYRO_BIAS_INVALID_SAMPLE);
    assert(tm_gyro_bias_update(&state, flat, measured, t + 8000u,
        NULL) == TM_GYRO_BIAS_INVALID_SAMPLE);
    assert(tm_gyro_bias_update(NULL, flat, measured, t + 8000u,
        corrected) == TM_GYRO_BIAS_INVALID_SAMPLE);
}

int main(void)
{
    offset_freezes_and_gap_requalifies();
    stationary_guards();
    stationary_noise_is_averaged();
    both_duration_and_sample_count_are_required();
    invalid_samples_discard_qualification();
    puts("gyro bias: stationary offset, frozen correction, guards and recovery pass");
    return 0;
}
