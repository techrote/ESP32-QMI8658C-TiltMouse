# Orientation and tilt-motion contract

TM-004 is pure host-testable logic. It does not call ESP-IDF, TinyUSB, touch sensing, or target hardware. TM-006 will adapt successful QMI8658C samples into this layer.

## Input contract

`tiltmouse_imu_sample_t` intentionally mirrors the parts of TM-003's `qmi8658_sample_t` used here:

- `timestamp_us`: monotonic host timestamp in microseconds;
- `accel_g[3]`: estimator-frame X/Y/Z acceleration in g;
- `gyro_dps[3]`: estimator-frame X/Y/Z angular rate in degrees per second.

The pure estimator expects a consistent right-handed frame with positive Z
gravity in the intended flat pose. It estimates roll about frame +X and pitch
about frame +Y. Gyro Z is ignored by orientation integration. Yaw and
magnetometer support are out of scope.

TM-008 physical capture with the Matrix flat, LED face up and untouched showed
native QMI gravity near (0,0,-1) g. Passing it directly to the estimator drove
roll to the 85 degree bound. The target adapter now applies a fixed 180 degree
rotation about native X: (X,Y,Z) becomes (X,-Y,-Z), for both acceleration and
gyro. This is a proper rotation, not a Z-only reflection. The QMI driver still
returns native data; portable estimator fixtures already use estimator-frame
data. Physical axis signs and usable tilt remain acceptance work.

## Target gyro zero-rate calibration

The confirmed stationary Matrix capture after frame correction showed native
gyro means near (-1.9, 10.3, -0.83) dps. The Y offset exceeds the unchanged
8-dps startup quiet gate. Three roughly 1500-sample stationary windows had
per-axis gyro spans at most 3.609 dps and acceleration spans at most 0.062 g.
The actual-main regression with this offset reproduces failed neutral capture.

The target adapter now learns a bounded stationary mean before forwarding IMU
samples to the pipeline. It uses at least 128 samples spanning at least one
second, with no gap over 100 ms. Each window requires acceleration magnitude
0.75â€“1.25 g, per-axis acceleration span at most 0.10 g, per-axis gyro span at
most 4 dps, and raw gyro magnitude at most 16 dps per axis. The last bound is
an application guard covering the measured offset, not a QMI manufacturing
tolerance. The window contains only fixed sums/min/max and at most 1024 samples;
movement or excess count restarts it without queuing samples.

After qualification the native gyro mean is subtracted and then rotated into
the estimator frame. The offset remains frozen through normal motion and BOOT
recenter. Sensor errors/reinitialization, invalid/non-monotonic samples, or a
gap over 100 ms discard qualification. Motion remains withheld while a fresh
stationary window is pending, followed by the existing 32-sample/8-dps neutral
gate. Touch acquisition and fresh full-state button reports continue.

The board must be still during startup and after sensor recovery. A six-axis
IMU cannot distinguish constant yaw rotation from a gyro zero-rate offset;
these stationarity checks are a bounded heuristic. No offset is persisted or
hard-coded for this board. Diagnostic USB logs retain native samples and add
qualification, frozen offset and corrected gyro values for physical review.
This implementation still requires physical calibration/drift/axes acceptance.

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
| usable acceleration magnitude | 0.75â€“1.25 g |
| largest gyro-integrated sample gap | 50 ms |
| roll/pitch bound | Â±85Â° |

A non-monotonic timestamp is rejected without changing estimator state. A gap larger than 50 ms is never gyro-integrated: the timestamp is rebased and, when gravity is usable, roll/pitch are re-established from the accelerometer. This prevents a stale interval from becoming a large synthetic rotation.

Acceleration outside 0.75â€“1.25 g is treated as a dynamic disturbance and is not used for gravity correction on that update. Gyro prediction continues for normal-sized time steps.

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
| invert | yes | no |
| deadzone | 3Â° | 3Â° |
| max useful tilt | 35Â° | 35Â° |
| response exponent | 1.6 | 1.6 |
| gain | 900 counts/s | 900 counts/s |
| velocity clamp | 1000 counts/s | 1000 counts/s |

Axis source and inversion are explicit so physical acceptance can change board-to-pointer mapping without changing estimator mathematics.

TM-008 candidate 1 tried exponent 0.9 and gain 450 counts/s after the operator
reported difficult initiation and high-speed control. The operator rejected
the physical candidate as substantially less controllable. The original
1.6/900 response is restored; it remains provisional rather than accepted v0.1
tuning. Original axis signs are retained after the operator corrected an
initial Y-inversion report when holding the board with the cable in hand.
Neutral capture in the actual grip and usable orientation range require
controlled measurement before another curve change. The corrected 25 second
baseline idle was quiet. Click-transition measurement continues without
freezing or damping motion. Full candidate results remain in TM-008 evidence.

After frame and gyro-bias correction, the operator reported "Feels good,
x axis swapped" with LED face up and the USB cable toward the operator.
TM-008 therefore inverts only the default cursor X mapping; Y and all response
magnitudes remain unchanged. The deterministic default-response test checks
both X signs and the preserved Y sign across the original response table.
Physical confirmation of the corrected X mapping is still required.

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
- a CSV replay that tilts from neutral to +20Â° pitch, holds through a dynamic acceleration disturbance, and returns to neutral.

These tests establish deterministic algorithm behavior only. Final board axes, neutral drift, deadzone, gain, response curve, and cursor feel remain physical TM-008 acceptance work.
