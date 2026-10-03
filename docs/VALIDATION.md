# Validation strategy

## Required automated checks

TM-001 should establish named CI checks that later PRs preserve. At minimum:

1. **ESP32-S3 firmware build**
   - clean ESP-IDF build for target `esp32s3`;
   - pinned exact ESP-IDF version/toolchain.

2. **Host unit tests**
   - standard host compiler;
   - tests for all pure logic components.

3. **Deterministic replay tests**
   - IMU traces -> orientation/motion outputs;
   - touch traces -> logical button transitions.

4. **Static/format sanity**
   - apply a lightweight formatter/linter policy that is reproducible in CI;
   - avoid checks that require target hardware.

## Driver testability

QMI8658 I2C transactions should sit behind a narrow bus abstraction so register-sequence/identity/error cases can be tested with a fake bus.

Touch acquisition hardware calls may be target-only, but normalization, fusion, hysteresis, debounce and baseline-update decisions must be testable on the host.

USB report construction/button masks should be testable without requiring an enumerated device.

## Fixture requirements

Store small, human-readable fixtures where practical:
- stationary IMU;
- slow tilt;
- return-to-neutral;
- short dynamic acceleration perturbation;
- single-channel touch;
- broad 3-channel touch;
- noisy idle;
- GPIO4-only touch;
- simultaneous left/right;
- press-hold-release.

Physical TM-008 runs may add captured traces as fixtures after reviewing size/privacy/reproducibility.

## Physical-device acceptance

The following require real hardware evidence and must not be inferred from CI:

- host sees a normal USB mouse;
- X/Y axes correspond to intended board tilt;
- neutral drift is acceptable;
- cursor has no objectionable stepping/hunting;
- GPIO1–3 reliably produces left state;
- GPIO5–7 reliably produces right state;
- GPIO4 does not click;
- hold/drag works;
- touching/clicking does not cause unacceptable cursor jumps;
- reconnect and suspend/resume behave correctly.

TM-008 should document host OS, firmware commit, build configuration, calibration/tuning values and observed results.

## Failure handling tests

Before v0.1:
- I2C NACK/timeouts do not wedge the main loop;
- stale IMU data stops motion safely;
- touch subsystem failure releases button state;
- USB unmount/suspend does not build an unbounded report backlog;
- reconnect does not emit a large stale movement;
- re-centre clears residual motion.

## Merge rule

A PR may merge only when its required automated checks are green and all acceptance criteria owned by that issue are established.

If an issue explicitly owns a physical acceptance criterion and the current worker cannot access the board, leave a precise handoff instead of reducing the criterion.
