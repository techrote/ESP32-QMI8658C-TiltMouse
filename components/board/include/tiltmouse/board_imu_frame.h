#pragma once

/* Matrix LED-face-up estimator frame. Preserve native X and rotate Y/Z by
 * 180 degrees about X. Apply the same proper rotation to accel and gyro;
 * negating gravity Z alone would mirror the frame and corrupt gyro signs. */
static inline void tiltmouse_board_imu_to_mouse_frame(
    const float native[3], float mouse[3])
{
    mouse[0] = native[0];
    mouse[1] = -native[1];
    mouse[2] = -native[2];
}
