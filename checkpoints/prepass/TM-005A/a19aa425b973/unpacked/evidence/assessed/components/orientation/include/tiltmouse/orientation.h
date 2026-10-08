#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t timestamp_us;
    float accel_g[3];
    float gyro_dps[3];
} tiltmouse_imu_sample_t;

typedef struct {
    float complementary_tau_s;
    float accel_min_g;
    float accel_max_g;
    float max_dt_s;
    float angle_limit_deg;
} tiltmouse_orientation_config_t;

typedef enum {
    TILTMOUSE_ORIENTATION_OK = 0,
    TILTMOUSE_ORIENTATION_INITIALIZED,
    TILTMOUSE_ORIENTATION_GAP_REBASED,
    TILTMOUSE_ORIENTATION_ERR_INVALID_ARGUMENT,
    TILTMOUSE_ORIENTATION_ERR_INVALID_CONFIG,
    TILTMOUSE_ORIENTATION_ERR_ACCEL_UNUSABLE,
    TILTMOUSE_ORIENTATION_ERR_NON_MONOTONIC_TIME,
} tiltmouse_orientation_status_t;

typedef struct {
    float roll_deg;
    float pitch_deg;
    float neutral_roll_deg;
    float neutral_pitch_deg;
    bool neutral_valid;
} tiltmouse_orientation_pose_t;

typedef struct {
    tiltmouse_orientation_config_t config;
    uint64_t last_timestamp_us;
    float roll_deg;
    float pitch_deg;
    float neutral_roll_deg;
    float neutral_pitch_deg;
    bool initialized;
    bool neutral_valid;
} tiltmouse_orientation_state_t;

tiltmouse_orientation_config_t tiltmouse_orientation_default_config(void);
bool tiltmouse_orientation_config_valid(const tiltmouse_orientation_config_t *config);

void tiltmouse_orientation_reset(
    tiltmouse_orientation_state_t *state,
    const tiltmouse_orientation_config_t *config);

tiltmouse_orientation_status_t tiltmouse_orientation_update(
    tiltmouse_orientation_state_t *state,
    const tiltmouse_imu_sample_t *sample);

bool tiltmouse_orientation_capture_neutral(tiltmouse_orientation_state_t *state);
void tiltmouse_orientation_clear_neutral(tiltmouse_orientation_state_t *state);

tiltmouse_orientation_pose_t tiltmouse_orientation_pose(
    const tiltmouse_orientation_state_t *state);

const char *tiltmouse_orientation_status_string(tiltmouse_orientation_status_t status);

#ifdef __cplusplus
}
#endif
