#include <stddef.h>
#include <string.h>

#include "tiltmouse/touch_buttons.h"

static float abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

static float max_float(float a, float b)
{
    return a > b ? a : b;
}

static float clamp_float(float value, float low, float high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static bool valid_config(const tiltmouse_touch_buttons_config_t *config)
{
    if (config == NULL || config->calibration_samples == 0U) {
        return false;
    }
    if (config->minimum_delta_ratio <= 0.0f || config->noise_multiplier <= 0.0f ||
        config->maximum_normalized_evidence <= 0.0f) {
        return false;
    }
    if (config->baseline_alpha <= 0.0f || config->baseline_alpha > 1.0f ||
        config->noise_alpha <= 0.0f || config->noise_alpha > 1.0f ||
        config->baseline_freeze_evidence <= 0.0f) {
        return false;
    }
    if (config->sum_release > config->sum_press ||
        config->strong_release > config->strong_press ||
        config->moderate_release > config->moderate_press ||
        config->vote_release > config->vote_press) {
        return false;
    }
    if (config->press_debounce_samples == 0U || config->release_debounce_samples == 0U) {
        return false;
    }
    return config->fusion_policy >= TILTMOUSE_TOUCH_FUSION_SUM &&
           config->fusion_policy <= TILTMOUSE_TOUCH_FUSION_TWO_OF_THREE;
}

tiltmouse_touch_buttons_config_t tiltmouse_touch_buttons_default_config(void)
{
    return (tiltmouse_touch_buttons_config_t){
        .calibration_samples = 16,
        .minimum_delta_ratio = 0.005f,
        .noise_multiplier = 6.0f,
        .maximum_normalized_evidence = 8.0f,
        .baseline_alpha = 0.01f,
        .noise_alpha = 0.05f,
        .baseline_freeze_evidence = 0.50f,
        .fusion_policy = TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE,
        .sum_press = 2.0f,
        .sum_release = 1.2f,
        .strong_press = 2.0f,
        .strong_release = 1.2f,
        .moderate_press = 0.9f,
        .moderate_release = 0.6f,
        .vote_press = 1.0f,
        .vote_release = 0.7f,
        .press_debounce_samples = 2,
        .release_debounce_samples = 2,
    };
}

bool tiltmouse_touch_buttons_init(
    tiltmouse_touch_buttons_t *state,
    const tiltmouse_touch_buttons_config_t *config)
{
    if (state == NULL || !valid_config(config)) {
        return false;
    }

    memset(state, 0, sizeof(*state));
    state->config = *config;
    return true;
}

static void calibrate_channel(
    tiltmouse_touch_channel_state_t *channel,
    uint32_t raw,
    uint32_t target_samples)
{
    const float value = (float)raw;
    const uint32_t count = channel->calibration_count + 1U;

    if (count == 1U) {
        channel->baseline = value;
        channel->noise = 0.0f;
    } else {
        const float previous_baseline = channel->baseline;
        channel->baseline += (value - channel->baseline) / (float)count;
        const float deviation = abs_float(value - previous_baseline);
        channel->noise += (deviation - channel->noise) / (float)count;
    }

    channel->calibration_count = count;
    channel->calibrated = count >= target_samples;
}

static bool all_channels_calibrated(const tiltmouse_touch_buttons_t *state)
{
    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        if (!state->channels[i].calibrated) {
            return false;
        }
    }
    return true;
}

static float normalized_evidence(
    const tiltmouse_touch_buttons_t *state,
    size_t channel_index,
    uint32_t raw)
{
    const tiltmouse_touch_channel_state_t *channel = &state->channels[channel_index];
    const float delta = (float)raw - channel->baseline;
    if (delta <= 0.0f) {
        return 0.0f;
    }

    const float relative_floor = channel->baseline * state->config.minimum_delta_ratio;
    const float noise_floor = channel->noise * state->config.noise_multiplier;
    const float scale = max_float(relative_floor, noise_floor);
    return clamp_float(
        delta / scale,
        0.0f,
        state->config.maximum_normalized_evidence);
}

static bool group_candidate(
    const tiltmouse_touch_buttons_t *state,
    const float evidence[TILTMOUSE_TOUCH_CHANNEL_COUNT],
    const size_t indices[3],
    bool currently_pressed)
{
    const tiltmouse_touch_buttons_config_t *config = &state->config;
    float sum = 0.0f;
    float strong_threshold;
    float moderate_threshold;
    float vote_threshold;
    unsigned moderate_count = 0U;
    unsigned vote_count = 0U;

    if (currently_pressed) {
        strong_threshold = config->strong_release;
        moderate_threshold = config->moderate_release;
        vote_threshold = config->vote_release;
    } else {
        strong_threshold = config->strong_press;
        moderate_threshold = config->moderate_press;
        vote_threshold = config->vote_press;
    }

    for (size_t i = 0; i < 3U; ++i) {
        const float value = evidence[indices[i]];
        sum += value;
        if (value >= moderate_threshold) {
            ++moderate_count;
        }
        if (value >= vote_threshold) {
            ++vote_count;
        }
    }

    switch (config->fusion_policy) {
    case TILTMOUSE_TOUCH_FUSION_SUM:
        return sum >= (currently_pressed ? config->sum_release : config->sum_press);
    case TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE:
        for (size_t i = 0; i < 3U; ++i) {
            if (evidence[indices[i]] >= strong_threshold) {
                return true;
            }
        }
        return moderate_count >= 2U;
    case TILTMOUSE_TOUCH_FUSION_TWO_OF_THREE:
        return vote_count >= 2U;
    default:
        return false;
    }
}

static void update_group(
    tiltmouse_touch_group_state_t *group,
    bool candidate,
    const tiltmouse_touch_buttons_config_t *config)
{
    if (candidate == group->pressed) {
        group->pending_count = 0U;
        return;
    }

    ++group->pending_count;
    const uint32_t required = candidate
                                  ? config->press_debounce_samples
                                  : config->release_debounce_samples;
    if (group->pending_count >= required) {
        group->pressed = candidate;
        group->pending_count = 0U;
    }
}

static void adapt_channel(
    tiltmouse_touch_buttons_t *state,
    size_t index,
    uint32_t raw,
    float evidence,
    bool group_frozen)
{
    if (group_frozen || evidence >= state->config.baseline_freeze_evidence) {
        return;
    }

    tiltmouse_touch_channel_state_t *channel = &state->channels[index];
    const float value = (float)raw;
    const float previous_baseline = channel->baseline;
    const float deviation = abs_float(value - previous_baseline);

    channel->baseline += state->config.baseline_alpha * (value - channel->baseline);
    channel->noise += state->config.noise_alpha * (deviation - channel->noise);
}

void tiltmouse_touch_buttons_update(
    tiltmouse_touch_buttons_t *state,
    const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT],
    tiltmouse_touch_buttons_output_t *output)
{
    if (state == NULL || raw == NULL || output == NULL) {
        return;
    }

    memset(output, 0, sizeof(*output));

    if (!all_channels_calibrated(state)) {
        for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
            if (!state->channels[i].calibrated) {
                calibrate_channel(
                    &state->channels[i],
                    raw[i],
                    state->config.calibration_samples);
            }
            output->baseline[i] = state->channels[i].baseline;
            output->noise[i] = state->channels[i].noise;
        }
        output->calibrated = all_channels_calibrated(state);
        return;
    }

    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        output->normalized[i] = normalized_evidence(state, i, raw[i]);
    }
    output->reference_normalized = output->normalized[TILTMOUSE_TOUCH_REFERENCE];

    static const size_t left_indices[3] = {
        TILTMOUSE_TOUCH_LEFT_1,
        TILTMOUSE_TOUCH_LEFT_2,
        TILTMOUSE_TOUCH_LEFT_3,
    };
    static const size_t right_indices[3] = {
        TILTMOUSE_TOUCH_RIGHT_1,
        TILTMOUSE_TOUCH_RIGHT_2,
        TILTMOUSE_TOUCH_RIGHT_3,
    };

    const bool left_candidate =
        group_candidate(state, output->normalized, left_indices, state->left.pressed);
    const bool right_candidate =
        group_candidate(state, output->normalized, right_indices, state->right.pressed);

    update_group(&state->left, left_candidate, &state->config);
    update_group(&state->right, right_candidate, &state->config);

    const bool freeze_left = left_candidate || state->left.pressed;
    const bool freeze_right = right_candidate || state->right.pressed;

    for (size_t i = 0; i < TILTMOUSE_TOUCH_CHANNEL_COUNT; ++i) {
        bool group_frozen = false;
        if (i <= TILTMOUSE_TOUCH_LEFT_3) {
            group_frozen = freeze_left;
        } else if (i >= TILTMOUSE_TOUCH_RIGHT_1) {
            group_frozen = freeze_right;
        }
        adapt_channel(state, i, raw[i], output->normalized[i], group_frozen);
        output->baseline[i] = state->channels[i].baseline;
        output->noise[i] = state->channels[i].noise;
    }

    output->calibrated = true;
    output->left_pressed = state->left.pressed;
    output->right_pressed = state->right.pressed;
}
