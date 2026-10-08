#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tiltmouse/orientation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TILTMOUSE_MOTION_AXIS_ROLL = 0,
    TILTMOUSE_MOTION_AXIS_PITCH,
} tiltmouse_motion_axis_source_t;

typedef struct {
    tiltmouse_motion_axis_source_t source;
    bool invert;
    float deadzone_deg;
    float max_tilt_deg;
    float response_exponent;
    float gain_counts_per_s;
    float max_velocity_counts_per_s;
} tiltmouse_motion_axis_config_t;

typedef struct {
    tiltmouse_motion_axis_config_t x;
    tiltmouse_motion_axis_config_t y;
    float max_report_dt_s;
    uint8_t max_delta_per_report;
} tiltmouse_mouse_motion_config_t;

typedef struct {
    float residual_x;
    float residual_y;
} tiltmouse_mouse_motion_state_t;

typedef struct {
    float x_counts_per_s;
    float y_counts_per_s;
} tiltmouse_mouse_velocity_t;

typedef struct {
    int8_t x;
    int8_t y;
} tiltmouse_mouse_delta_t;

typedef enum {
    TILTMOUSE_MOUSE_MOTION_OK = 0,
    TILTMOUSE_MOUSE_MOTION_NOT_READY,
    TILTMOUSE_MOUSE_MOTION_ERR_INVALID_ARGUMENT,
    TILTMOUSE_MOUSE_MOTION_ERR_INVALID_CONFIG,
} tiltmouse_mouse_motion_status_t;

tiltmouse_mouse_motion_config_t tiltmouse_mouse_motion_default_config(void);
bool tiltmouse_mouse_motion_config_valid(const tiltmouse_mouse_motion_config_t *config);
void tiltmouse_mouse_motion_reset(tiltmouse_mouse_motion_state_t *state);

tiltmouse_mouse_motion_status_t tiltmouse_mouse_motion_velocity(
    const tiltmouse_mouse_motion_config_t *config,
    const tiltmouse_orientation_pose_t *pose,
    tiltmouse_mouse_velocity_t *velocity);

tiltmouse_mouse_motion_status_t tiltmouse_mouse_motion_step(
    tiltmouse_mouse_motion_state_t *state,
    const tiltmouse_mouse_motion_config_t *config,
    const tiltmouse_orientation_pose_t *pose,
    float report_dt_s,
    tiltmouse_mouse_delta_t *delta,
    tiltmouse_mouse_velocity_t *velocity);

#ifdef __cplusplus
}
#endif
