# Firmware architecture

## Principles

- Native USB HID, not Bluetooth.
- Relative mouse motion.
- Hardware drivers at the edges; deterministic logic in the middle.
- Minimal shared mutable state.
- Fixed-rate sampling/reporting with explicit timestamps.
- Tunables centralized rather than scattered.
- Normal build should enumerate as a mouse only.

## Proposed tree

```text
CMakeLists.txt
sdkconfig.defaults
main/
  CMakeLists.txt
  app_main.c
  board.c
  board.h
  config.h
components/
  qmi8658/
  orientation/
  mouse_motion/
  touch_input/
  touch_buttons/
  usb_hid_mouse/
tests/
  host/
  fixtures/
```

Exact layout may evolve, but module boundaries should remain.

## Data flow

```text
QMI8658C -> qmi8658 -> orientation -> mouse_motion --+
                                                     +-> USB HID reports
GPIO1..7 -> touch_input -> touch_buttons -----------+
GPIO0 ----------------------------------------------> recentre command
```

## Scheduler

Initial target rates are hypotheses, not acceptance requirements:
- IMU sample: ~250 Hz.
- motion estimator: IMU rate.
- HID report: ~125 Hz.
- touch scan: fast enough for responsive click/drag, likely 100–200 Hz.

Use timestamps and bounded queues/latest-sample semantics so delayed tasks do not generate bursts of stale motion.

## Orientation estimator

Estimate pitch and roll only.

A complementary-filter implementation is sufficient for v0.1:
- integrate gyro rates over measured `dt`;
- derive pitch/roll from normalized acceleration;
- low-pass gravity correction / high-pass gyro contribution;
- reject or reduce accelerometer correction during implausible acceleration magnitude if needed;
- express final orientation relative to a stored neutral pose.

Yaw is neither observable from gravity nor needed for the initial pointing model.

## Mouse motion mapping

For each axis:
1. subtract neutral orientation;
2. apply configurable sign/axis mapping;
3. apply deadzone;
4. map remaining tilt to velocity with a monotonic nonlinear curve;
5. clamp velocity;
6. integrate velocity over report interval;
7. retain fractional X/Y residual;
8. emit bounded integer HID deltas.

This makes tilt act like a small analog joystick.

## Touch acquisition

Read GPIO1–GPIO7 independently through the current ESP-IDF capacitive-touch driver.

Maintain:
- baseline;
- noise estimate;
- normalized activation;
- active/inactive state;
- calibration validity.

Do not update baseline aggressively while touched.

## Touch fusion

Left group = GPIO1,2,3.
Right group = GPIO5,6,7.
GPIO4 = reference/diagnostic only.

Fusion is a pure function over normalized channel evidence plus state. Begin with a configurable policy that can support summed evidence and one-strong-or-two-moderate evidence. Hysteresis and temporal debounce are applied to the logical group state.

Simultaneous left/right must be representable, and held state must survive movement for drag.

## USB HID

Use native ESP32-S3 USB D+/D- via TinyUSB / `esp_tinyusb`.

The HID layer owns:
- report descriptor;
- device/config/string descriptors as required;
- mount/suspend state;
- button bitmask;
- relative X/Y reports;
- safe all-buttons-release path.

Normal release build should not require CDC. A diagnostic composite HID+CDC build is allowed if it materially improves bring-up/tuning.

## Safe startup

At the earliest practical point:
- keep RGB data GPIO14 low/quiescent;
- do not initialize Wi-Fi/Bluetooth;
- initialize USB/board state deterministically;
- initialize IMU/touch with explicit error handling;
- do not emit cursor motion until neutral calibration is valid.

## Re-centre

Use runtime BOOT/GPIO0 input for a deliberate re-centre action without breaking the reset+BOOT flashing path. Debounce it and require a clear action; the exact short/long-press policy can be chosen in TM-006.

Re-centre should:
- clear orientation neutral offset;
- clear fractional motion residuals;
- avoid generating a click;
- preserve USB connection where possible.
