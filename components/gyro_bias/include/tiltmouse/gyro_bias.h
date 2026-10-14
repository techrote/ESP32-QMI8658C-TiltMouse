#ifndef TILTMOUSE_GYRO_BIAS_H
#define TILTMOUSE_GYRO_BIAS_H

#include <stdbool.h>
#include <stdint.h>

/* Fixed, bounded startup policy for the QMI8658C target adapter. */
#define TM_GYRO_BIAS_MIN_SAMPLES 128u
#define TM_GYRO_BIAS_MAX_SAMPLES 1024u
#define TM_GYRO_BIAS_WINDOW_US UINT64_C(1000000)
#define TM_GYRO_BIAS_MAX_GAP_US UINT64_C(100000)

typedef enum {
    TM_GYRO_BIAS_PENDING = 0,
    TM_GYRO_BIAS_READY,
    TM_GYRO_BIAS_INVALID_SAMPLE,
} tm_gyro_bias_result_t;

typedef struct {
    uint64_t first_us;
    uint64_t last_us;
    uint64_t qualified_us;
    uint32_t samples;
    uint32_t rejected_windows;
    float accel_min[3];
    float accel_max[3];
    float gyro_min[3];
    float gyro_max[3];
    float gyro_sum[3];
    float bias_dps[3];
    bool ready;
} tm_gyro_bias_t;

/* Zero initialization is valid. Reset after a sensor error or reinitialization. */
void tm_gyro_bias_reset(tm_gyro_bias_t *state);

/* Native sensor frame. READY returns gyro minus a frozen stationary mean.
 * PENDING/INVALID return zero correction output; callers must withhold motion.
 * A stale gap discards qualification. This heuristic requires a still board:
 * a six-axis IMU cannot distinguish constant yaw from a zero-rate offset. */
tm_gyro_bias_result_t tm_gyro_bias_update(
    tm_gyro_bias_t *state, const float accel_g[3], const float gyro_dps[3],
    uint64_t timestamp_us, float corrected_dps[3]);

#endif
