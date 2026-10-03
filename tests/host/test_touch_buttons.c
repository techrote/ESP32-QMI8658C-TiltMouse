#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tiltmouse/touch_buttons.h"

#ifndef TOUCH_FIXTURE_PATH
#error "TOUCH_FIXTURE_PATH must be defined"
#endif

#define BASELINE 10000U

static void fill_raw(uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT], uint32_t value)
{
    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        raw[i] = value;
    }
}

static tiltmouse_touch_buttons_t make_state(tiltmouse_touch_fusion_policy_t policy)
{
    tiltmouse_touch_buttons_t state;
    tiltmouse_touch_buttons_config_t config = tiltmouse_touch_buttons_default_config();
    config.fusion_policy = policy;
    assert(tiltmouse_touch_buttons_init(&state, &config));
    return state;
}

static void calibrate_constant(
    tiltmouse_touch_buttons_t *state,
    const uint32_t baseline[TILTMOUSE_TOUCH_CHANNEL_COUNT])
{
    tiltmouse_touch_buttons_output_t output;
    for (uint32_t sample = 0; sample < state->config.calibration_samples; ++sample) {
        uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
        for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
            const int offset = ((sample + (uint32_t)i) % 3U == 0U) ? 2 : -2;
            raw[i] = (uint32_t)((int)baseline[i] + offset);
        }
        tiltmouse_touch_buttons_update(state, raw, &output);
    }
    assert(output.calibrated);
    assert(!output.left_pressed);
    assert(!output.right_pressed);
}

static void calibrate_uniform(tiltmouse_touch_buttons_t *state)
{
    const uint32_t baseline[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {
        BASELINE, BASELINE, BASELINE, BASELINE, BASELINE, BASELINE, BASELINE,
    };
    calibrate_constant(state, baseline);
}

static tiltmouse_touch_buttons_output_t step(
    tiltmouse_touch_buttons_t *state,
    const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT],
    unsigned repeat)
{
    tiltmouse_touch_buttons_output_t output = {0};
    for (unsigned i = 0; i < repeat; ++i) {
        tiltmouse_touch_buttons_update(state, raw, &output);
    }
    return output;
}

static void test_idle_noise(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    for (unsigned frame = 0; frame < 200U; ++frame) {
        uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
        for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
            const int offset = (int)((frame * 5U + (unsigned)i * 3U) % 9U) - 4;
            raw[i] = (uint32_t)((int)BASELINE + offset);
        }
        const tiltmouse_touch_buttons_output_t output = step(&state, raw, 1);
        assert(output.calibrated);
        assert(!output.left_pressed);
        assert(!output.right_pressed);
    }
}

static void test_partial_and_broad_touches(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 60U;
    tiltmouse_touch_buttons_output_t output = step(&state, raw, 4);
    assert(!output.left_pressed);

    fill_raw(raw, BASELINE);
    (void)step(&state, raw, 3);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 60U;
    raw[TILTMOUSE_TOUCH_LEFT_2] += 60U;
    output = step(&state, raw, 2);
    assert(output.left_pressed);

    fill_raw(raw, BASELINE);
    output = step(&state, raw, 2);
    assert(!output.left_pressed);

    raw[TILTMOUSE_TOUCH_LEFT_3] += 150U;
    output = step(&state, raw, 2);
    assert(output.left_pressed);
}

static void test_noisy_outlier_is_debounced(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_2] += 400U;
    tiltmouse_touch_buttons_output_t output = step(&state, raw, 1);
    assert(!output.left_pressed);

    fill_raw(raw, BASELINE);
    output = step(&state, raw, 2);
    assert(!output.left_pressed);
}

static void test_gpio4_never_clicks(void)
{
    for (int policy = TILTMOUSE_TOUCH_FUSION_SUM;
         policy <= TILTMOUSE_TOUCH_FUSION_TWO_OF_THREE;
         ++policy) {
        tiltmouse_touch_buttons_t state = make_state((tiltmouse_touch_fusion_policy_t)policy);
        calibrate_uniform(&state);

        uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
        fill_raw(raw, BASELINE);
        raw[TILTMOUSE_TOUCH_REFERENCE] += 1000U;
        const tiltmouse_touch_buttons_output_t output = step(&state, raw, 8);
        assert(output.reference_normalized > 0.0f);
        assert(!output.left_pressed);
        assert(!output.right_pressed);
    }
}

static void test_simultaneous_buttons(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 150U;
    raw[TILTMOUSE_TOUCH_RIGHT_1] += 60U;
    raw[TILTMOUSE_TOUCH_RIGHT_2] += 60U;
    const tiltmouse_touch_buttons_output_t output = step(&state, raw, 2);
    assert(output.left_pressed);
    assert(output.right_pressed);
}

static void test_hold_release_and_baseline_freeze(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 200U;
    tiltmouse_touch_buttons_output_t output = step(&state, raw, 2);
    assert(output.left_pressed);
    const float held_baseline = output.baseline[TILTMOUSE_TOUCH_LEFT_1];

    output = step(&state, raw, 100);
    assert(output.left_pressed);
    assert(output.baseline[TILTMOUSE_TOUCH_LEFT_1] - held_baseline < 0.1f);

    fill_raw(raw, BASELINE);
    output = step(&state, raw, 1);
    assert(output.left_pressed);
    output = step(&state, raw, 1);
    assert(!output.left_pressed);
}

static void test_baseline_adaptation(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    calibrate_uniform(&state);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    fill_raw(raw, BASELINE + 20U);
    tiltmouse_touch_buttons_output_t output = step(&state, raw, 300);
    assert(!output.left_pressed);
    assert(output.baseline[TILTMOUSE_TOUCH_LEFT_1] > (float)BASELINE + 15.0f);

    const float before_touch = output.baseline[TILTMOUSE_TOUCH_LEFT_1];
    raw[TILTMOUSE_TOUCH_LEFT_1] = BASELINE + 220U;
    output = step(&state, raw, 100);
    assert(output.left_pressed);
    assert(output.baseline[TILTMOUSE_TOUCH_LEFT_1] - before_touch < 0.1f);
}

static void test_independent_calibration_and_normalization(void)
{
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
    const uint32_t baseline[TILTMOUSE_TOUCH_CHANNEL_COUNT] = {
        8000U, 9000U, 10000U, 11000U, 12000U, 13000U, 14000U,
    };
    calibrate_constant(&state, baseline);

    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        raw[i] = baseline[i] + baseline[i] / 100U;
    }
    const tiltmouse_touch_buttons_output_t output = step(&state, raw, 1);

    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        assert(output.normalized[i] > 1.8f);
        assert(output.normalized[i] < 2.2f);
    }
}

static void test_fusion_policies(void)
{
    uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];

    tiltmouse_touch_buttons_t sum_state = make_state(TILTMOUSE_TOUCH_FUSION_SUM);
    calibrate_uniform(&sum_state);
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 40U;
    raw[TILTMOUSE_TOUCH_LEFT_2] += 40U;
    raw[TILTMOUSE_TOUCH_LEFT_3] += 40U;
    assert(step(&sum_state, raw, 2).left_pressed);

    tiltmouse_touch_buttons_t vote_state = make_state(TILTMOUSE_TOUCH_FUSION_TWO_OF_THREE);
    calibrate_uniform(&vote_state);
    fill_raw(raw, BASELINE);
    raw[TILTMOUSE_TOUCH_LEFT_1] += 150U;
    assert(!step(&vote_state, raw, 3).left_pressed);
    raw[TILTMOUSE_TOUCH_LEFT_2] += 60U;
    assert(step(&vote_state, raw, 2).left_pressed);
}

static void test_fixture_replay(void)
{
    FILE *fixture = fopen(TOUCH_FIXTURE_PATH, "r");
    assert(fixture != NULL);

    char line[512];
    tiltmouse_touch_buttons_t state =
        make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);

    while (fgets(line, sizeof(line), fixture) != NULL) {
        if (line[0] == '#' || strncmp(line, "scenario,", 9) == 0) {
            continue;
        }

        char scenario[64];
        unsigned reset;
        unsigned repeat;
        unsigned values[TILTMOUSE_TOUCH_CHANNEL_COUNT];
        unsigned expected_left;
        unsigned expected_right;
        const int parsed = sscanf(
            line,
            "%63[^,],%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
            scenario,
            &reset,
            &repeat,
            &values[0],
            &values[1],
            &values[2],
            &values[3],
            &values[4],
            &values[5],
            &values[6],
            &expected_left,
            &expected_right);
        assert(parsed == 12);
        assert(repeat > 0U);

        if (reset != 0U) {
            state = make_state(TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE);
        }

        uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT];
        for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
            raw[i] = values[i];
        }
        const tiltmouse_touch_buttons_output_t output = step(&state, raw, repeat);
        assert(output.left_pressed == (expected_left != 0U));
        assert(output.right_pressed == (expected_right != 0U));
        (void)scenario;
    }

    assert(fclose(fixture) == 0);
}

int main(void)
{
    test_idle_noise();
    test_partial_and_broad_touches();
    test_noisy_outlier_is_debounced();
    test_gpio4_never_clicks();
    test_simultaneous_buttons();
    test_hold_release_and_baseline_freeze();
    test_baseline_adaptation();
    test_independent_calibration_and_normalization();
    test_fusion_policies();
    test_fixture_replay();
    return 0;
}
