# Orientation and tilt-motion contract

TM-004 is pure host-testable logic. It does not call ESP-IDF, TinyUSB, touch sensing, or target hardware. TM-006 will adapt successful QMI8658C samples into this layer.

## Input contract

`tiltmouse_imu_sample_t` intentionally mirrors the parts of TM-003's `qmi8658_sample_t` used here:

- `timestamp_us`: monotonic host timestamp in microseconds;
- `accel_g[3]`: native sensor X/Y/Z acceleration in g;
- `gyro_dps[3]`: native sensor X/Y/Z angular rate in degrees per second.

TM-004 estimates roll about sensor +X and pitch about sensor +Y. Gyro Z is ignored. Yaw and magnetometer support are out of scope.

## Estimator

The estimator uses a bounded complementary filter:

1. the first usable sample initializes roll/pitch from gravity;
2. subsequent updates integrate gyro X/Y using measured timestamp delta;
3. the accelerometer supplies long-term gravity correction when its magnitude is plausible;
4. the correction coefficient is derived from the configured time constant and measured `dt`;
5. estimated roll and pitch are clamped to a configured finite range.

Default estimator hypotheses are:

| Setting | Default |
| --- | ---: |
| complementary-filter time constant | 0.50 s |
| usable acceleration magnitude | 0.75–1.25 g |
| largest gyro-integrated sample gap | 50 ms |
| roll/pitch bound | ±85° |

A non-monotonic timestamp is rejected without changing estimator state. A gap larger than 50 ms is never gyro-integrated: the timestamp is rebased and, when gravity is usable, roll/pitch are re-established from the accelerometer. This prevents a stale interval from becoming a large synthetic rotation.

Acceleration outside 0.75–1.25 g is treated as a dynamic disturbance and is not used for gravity correction on that update. Gyro prediction continues for normal-sized time steps.

## Neutral pose

Neutral capture stores the current filtered roll and pitch. Motion is not considered ready until a neutral pose exists.

A runtime re-centre in TM-006 should perform both operations together:

1. capture the current orientation as neutral;
2. call `tiltmouse_mouse_motion_reset()` to clear fractional cursor residuals.

A full estimator reset clears initialization and neutral state. Capturing neutral does not reset the complementary filter itself.

## Tilt-to-velocity mapping

Each pointer axis independently selects roll or pitch and an optional sign inversion. The mapped angle first subtracts the stored neutral angle.

For magnitude `m` above the deadzone, the normalized response is:

`n = clamp((m - deadzone) / (max_tilt - deadzone), 0, 1)`

Velocity magnitude is `gain * n^exponent`, then clamped by `max_velocity`. Sign is restored after the nonlinear response. This is monotonic for every accepted positive response exponent.

The provisional defaults are deliberately tuning hypotheses for TM-008, not claims about physical cursor feel:

| Setting | X | Y |
| --- | ---: | ---: |
| source | roll | pitch |
| invert | no | no |
| deadzone | 3° | 3° |
| max useful tilt | 35° | 35° |
| response exponent | 1.6 | 1.6 |
| gain | 900 counts/s | 900 counts/s |
| velocity clamp | 1000 counts/s | 1000 counts/s |

Axis source and inversion are explicit so physical acceptance can change board-to-pointer mapping without changing estimator mathematics.

## Report integration and bounds

`tiltmouse_mouse_motion_step()` integrates velocity over the supplied report interval. Report intervals are clipped to a configurable maximum; the default is 50 ms.

Sub-count motion is retained in independent X/Y fractional residuals. Integer output uses truncation toward zero, so repeated small motion eventually produces a report count instead of being lost.

Per-report deltas are independently clamped to a configurable positive limit, default 127 to match the existing signed 8-bit HID X/Y transport. Motion beyond that per-report bound is discarded rather than retained as a stale multi-report backlog.

## Deterministic evidence

Host tests cover:

- stationary gyro bias remaining bounded by gravity correction;
- slow tilt and opposite/return direction;
- measured variable `dt`;
- stale-gap rebasing and non-monotonic timestamp rejection;
- bounded dynamic-acceleration disturbance;
- neutral capture/clear/reset;
- configurable roll/pitch axis selection and inversion;
- deadzone and nonlinear curve boundaries;
- velocity and report-delta clamps;
- fractional accumulation and residual clearing on re-centre;
- a CSV replay that tilts from neutral to +20° pitch, holds through a dynamic acceleration disturbance, and returns to neutral.

These tests establish deterministic algorithm behavior only. Final board axes, neutral drift, deadzone, gain, response curve, and cursor feel remain physical TM-008 acceptance work.
