#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/touch_channels.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TILTMOUSE_TOUCH_FUSION_SUM = 0,
    TILTMOUSE_TOUCH_FUSION_STRONG_OR_TWO_MODERATE = 1,
    TILTMOUSE_TOUCH_FUSION_TWO_OF_THREE = 2,
} tiltmouse_touch_fusion_policy_t;

typedef struct {
    uint32_t calibration_samples;
    float minimum_delta_ratio;
    float noise_multiplier;
    float maximum_normalized_evidence;
    float baseline_alpha;
    float noise_alpha;
    float baseline_freeze_evidence;

    tiltmouse_touch_fusion_policy_t fusion_policy;
    float sum_press;
    float sum_release;
    float strong_press;
    float strong_release;
    float moderate_press;
    float moderate_release;
    float vote_press;
    float vote_release;
    uint32_t press_debounce_samples;
    uint32_t release_debounce_samples;
} tiltmouse_touch_buttons_config_t;

typedef struct {
    float baseline;
    float noise;
    uint32_t calibration_count;
    bool calibrated;
} tiltmouse_touch_channel_state_t;

typedef struct {
    bool pressed;
    uint32_t pending_count;
} tiltmouse_touch_group_state_t;

typedef struct {
    tiltmouse_touch_buttons_config_t config;
    tiltmouse_touch_channel_state_t channels[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    tiltmouse_touch_group_state_t left;
    tiltmouse_touch_group_state_t right;
} tiltmouse_touch_buttons_t;

typedef struct {
    bool calibrated;
    bool left_pressed;
    bool right_pressed;
    float normalized[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    float baseline[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    float noise[TILTMOUSE_TOUCH_CHANNEL_COUNT];
    float reference_normalized;
} tiltmouse_touch_buttons_output_t;

/** Provisional logic defaults; physical threshold tuning belongs to TM-008. */
tiltmouse_touch_buttons_config_t tiltmouse_touch_buttons_default_config(void);

/** Initialize a pure, host-testable touch logic state machine. */
bool tiltmouse_touch_buttons_init(
    tiltmouse_touch_buttons_t *state,
    const tiltmouse_touch_buttons_config_t *config);

/**
 * Consume one raw GPIO1..GPIO7 frame.
 *
 * ESP32-S3 V2 raw values rise with touch. GPIO4 is normalized and exposed for
 * diagnostics but is never included in either logical button group.
 */
void tiltmouse_touch_buttons_update(
    tiltmouse_touch_buttons_t *state,
    const uint32_t raw[TILTMOUSE_TOUCH_CHANNEL_COUNT],
    tiltmouse_touch_buttons_output_t *output);

#ifdef __cplusplus
}
#endif
