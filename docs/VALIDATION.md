# Validation strategy

## Required automated checks

TM-001 established these CI checks; later PRs preserve them:

1. **ESP32-S3 firmware build**
   - clean build for target `esp32s3`;
   - pinned exact ESP-IDF/toolchain.

2. **Host unit tests**
   - standard host compiler;
   - tests for pure logic and transport-independent contracts.

3. **Deterministic replay/protocol tests**
   - IMU traces -> orientation/motion outputs;
   - touch traces -> logical button transitions;
   - logical reports -> USB report adaptation;
   - logical reports -> ESP-NOW packet encode/decode/order behavior.

4. **Text and style sanity**
   - UTF-8/LF text;
   - final newlines;
   - no trailing whitespace;
   - no target hardware requirement.

The host build uses `-Wall -Wextra -Wpedantic -Werror`.

## Driver and transport testability

QMI8658 I2C transactions sit behind a fakeable bus abstraction.

Touch hardware acquisition may be target-only, but normalization/fusion/debounce logic must be host-testable.

USB report construction/button masks must be host-testable without enumeration.

TM-005A logical reports must be host-testable without USB or ESP-NOW.

TM-005B ESP-NOW protocol logic must separate packet/state semantics from target Wi-Fi callbacks so host tests can cover:

- version/size validation;
- encode/decode;
- sequence advance/wrap;
- duplicate/stale/out-of-order rejection;
- complete button-state recovery;
- link-timeout release policy;
- latest-first movement/no stale backlog.

## Fixture requirements

Store small, human-readable fixtures where practical:

- stationary IMU;
- slow tilt;
- return-to-neutral;
- dynamic acceleration perturbation;
- quiet/noisy touch;
- GPIO4-only touch;
- simultaneous left/right;
- press-hold-release;
- transport/report traces for packet ordering and loss scenarios.

Physical TM-008 runs may add captured traces after reviewing size/privacy/reproducibility.

## Physical-device acceptance

The following require real hardware evidence and must not be inferred from CI.

### Direct USB

- host sees a normal USB mouse;
- reconnect/suspend behavior is safe;
- no stale movement burst after recovery.

### ESP-NOW wireless

- ESP32-S3 transmits in the selected wireless mode without requiring infrastructure Wi-Fi;
- a compatible receiver actually receives/interprets the documented packet contract;
- the receiver presents normal USB HID to the host;
- packet loss/sequence gaps do not replay stale motion;
- duplicate/out-of-order packets are rejected as designed;
- wireless link loss eventually releases all buttons;
- recovery does not emit a stale movement burst;
- RF channel/peer/provisioning assumptions are recorded.

### Shared physical behavior

- X/Y axes correspond to intended board tilt;
- neutral drift is acceptable;
- cursor has no objectionable stepping/hunting;
- GPIO1–3 reliably produces left state;
- GPIO5–7 reliably produces right state;
- GPIO4 never clicks;
- hold/drag works;
- clicking does not cause unacceptable cursor jumps.

TM-008 must record the exact TiltMouse commit/artifact and, for wireless acceptance, the exact companion receiver hardware and firmware identity.

## Failure handling tests

Before v0.1:

- I2C NACK/timeouts do not wedge the main loop;
- stale IMU data stops motion safely;
- touch failure releases button state;
- USB unmount/suspend does not build an unbounded report backlog;
- ESP-NOW failure does not build an unbounded retransmission backlog;
- receiver timeout releases all buttons;
- transport transitions cannot duplicate reports;
- reconnect/recovery does not emit large stale movement;
- re-centre clears residual motion.

## BLE boundary

BLE/HOGP is not a v0.1 acceptance target. CI or physical work must not claim Bluetooth support simply because the ESP32-S3 hardware contains BLE.

## Merge rule

A PR may merge only when required automated checks are green and all acceptance criteria owned by that issue are established.

If an issue owns a physical USB/RF/receiver criterion and the current worker lacks that hardware, leave a precise handoff instead of weakening the criterion.
