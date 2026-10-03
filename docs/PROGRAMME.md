# Programme — TiltMouse v0.1

## Goal

Deliver a no-solder USB HID tilt mouse on the Waveshare ESP32-S3-Matrix with QMI8658C motion sensing and grouped capacitive-touch buttons.

## Dependency graph

```text
TM-001 foundation
├── TM-002 USB HID
├── TM-003 QMI8658C
│   └── TM-004 orientation + motion
└── TM-005 touch buttons

TM-002 + TM-004 + TM-005
          │
          ▼
      TM-006 integration
          │
          ▼
      TM-007 hardening
          │
          ▼
      TM-008 physical acceptance
          │
          ▼
      TM-009 v0.1 release
```

## Work packages

### TM-001 — Foundation and CI

Create the ESP-IDF project skeleton, pin the framework/toolchain, establish board definitions, host-test infrastructure and GitHub Actions. The firmware must build for ESP32-S3 before feature work proceeds.

### TM-002 — USB HID transport

Implement a minimal native-USB mouse device with explicit descriptors/report handling, mount state and synthetic report tests. No IMU or touch dependency.

### TM-003 — QMI8658C driver

Implement I2C initialization, identity verification, configuration, scaled accel/gyro samples and deterministic driver tests around a mocked bus abstraction. Expose data without mouse behaviour.

### TM-004 — Orientation and motion engine

Implement complementary-filter pitch/roll, neutral transform, deadzone/nonlinear velocity curve, axis configuration, clamps and fractional output accumulation. Make this pure logic with replayable tests.

### TM-005 — Touch acquisition and grouped buttons

Implement GPIO1–GPIO7 touch acquisition, independent channel calibration, normalization, logical left/right fusion, GPIO4 diagnostic/reference semantics, hysteresis and debounce. Keep group-fusion logic host-testable.

### TM-006 — Integrated mouse

Join HID, IMU, orientation, motion and touch into the main scheduler. Add startup neutral calibration, BOOT-button runtime re-centre and optional diagnostic telemetry mode. LEDs/radios remain unused.

### TM-007 — Robustness and usability hardening

Handle USB suspend/resume/reconnect, IMU failures/recovery, task timing, stale samples, safe button release on faults and click-induced motion behaviour. Add deterministic fault/replay tests.

### TM-008 — Physical acceptance and tuning

Use the real ESP32-S3-Matrix to measure USB enumeration, axes, neutral drift, deadzone, cursor feel, touch distributions, grouped-button reliability, GPIO4 behaviour and click-induced jitter. Tune only from evidence and record the dataset/results.

### TM-009 — v0.1 release consolidation

Freeze defaults proven in TM-008, document flashing/use/re-centre/recovery, produce a reproducible release build and close remaining documentation gaps.

## Plan review and improvements

The first outline was technically viable but too linear and hardware-dependent. The programme adopts these improvements:

1. **Parallel feature tracks after foundation.** USB, IMU and touch can progress independently.
2. **Pure logic boundaries.** Orientation, motion mapping and touch fusion are host-testable rather than hidden inside ESP-IDF callbacks.
3. **Explicit hardware-evidence boundary.** CI does not pretend to prove USB enumeration or touch performance.
4. **Address/identity robustness.** QMI8658C identity is verified and address selection is not hard-coded without proof.
5. **Software-only touch grouping.** The no-solder requirement is preserved while retaining per-electrode calibration.
6. **GPIO4 is experimentally useful but never a button.** Common-mode compensation is optional, evidence-driven.
7. **Click-induced movement is an acceptance item.** It is likely to matter for a hand-held tilt mouse and should be measured.
8. **Release HID identity remains simple.** Diagnostic CDC can exist as a build mode without becoming a host dependency.
9. **Framework version is pinned.** TM-001 must select one exact stable ESP-IDF version rather than allowing CI/API drift.
10. **LED/radio non-use is a testable scope rule.** Feature work must not accidentally initialize them.
