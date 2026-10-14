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
   - logical reports -> exact faikeow generic-envelope + `relative_mouse` sender encoding and compatibility vectors.

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

TM-005A logical report construction, bounded one-owner transport policy, USB report adaptation and the **production** HID descriptor layout are exercised by host tests without USB or ESP-NOW. The descriptor test verifies exactly two data button bits, six padding bits and two relative signed axes.

TM-005B sender logic must separate faikeow envelope/profile encoding from target Wi-Fi callbacks so host tests can cover:

- exact platform/profile version and ID;
- envelope/payload length bounds;
- golden-vector encoding;
- sender session/sequence advance and wrap;
- complete current button-state encoding;
- latest-first/no stale retransmission backlog.

Receiver-side duplicate/stale/out-of-order rejection and timeout neutralization are owned and tested in `techrote/faikeow-now-reciever`, not duplicated here.

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
- the exact accepted `techrote/faikeow-now-reciever` build receives the generic platform envelope + `relative_mouse` profile;
- the receiver presents normal USB HID to the host;
- joint evidence confirms packet loss/sequence gaps do not replay stale motion;
- joint evidence confirms duplicate/out-of-order handling matches faikeow's profile contract;
- wireless link loss reaches faikeow's safe neutral/released-button state;
- recovery does not emit a stale movement burst;
- RF channel/peer/provisioning assumptions are recorded.

### Shared physical behavior

- X/Y axes correspond to intended board tilt;
- neutral drift is acceptable;
- cursor has no objectionable stepping/hunting;
- GPIO5–7 reliably produces left state (TM-008 placement);
- GPIO1–3 reliably produces right state (TM-008 placement);
- GPIO4 never clicks;
- hold/drag works;
- clicking does not cause unacceptable cursor jumps.

TM-008 must record the exact TiltMouse commit/artifact and, for wireless acceptance, the exact `faikeow-now-reciever` commit, RP2040/ESP-radio artifact identities, generic platform version and `relative_mouse` profile version.

## Failure handling tests

TM-007 automated qualification establishes:

- QMI driver NACK/timeout/error propagation plus application invalidation; repeated target-driver errors trigger a rate-limited reinitialization attempt and successful samples require neutral recovery before movement;
- stale IMU is counted once per fault episode and stops motion without replay;
- touch driver/staleness faults force all-buttons release and reset the touch calibration state; normal input is withheld until the configured calibration sequence completes again;
- the production USB adapter is host-exercised through suspend/resume and detach/reconnect; pre-lifecycle movement is consumed, a new neutral barrier is established, failed completion repairs only zero-motion state, and no unbounded queue exists;
- ESP-NOW immediate/API failures and callback failures feed a bounded recovery controller (three consecutive failures; 50 ms exponential backoff capped at 1 s). Local radio reinitialization preserves the application sender session and monotonic sequence while re-activating output behind a neutral barrier;
- generic handoff quiesces the old route behind all-up, refuses normal reports during the fence, installs the new already-initialized output only after local quiescence, and requires a fresh all-buttons-up observation before re-arming. Held gestures/movement never migrate between hosts;
- reconnect/recovery never refunds or retries accepted relative movement;
- re-centre/fault/switch paths clear fractional residual motion.

Receiver timeout/order/HID behavior remains faikeow-owned and is not duplicated here.

## BLE boundary

BLE/HOGP is not a v0.1 acceptance target. CI or physical work must not claim Bluetooth support simply because the ESP32-S3 hardware contains BLE.

## Merge rule

A PR may merge only when required automated checks are green and all acceptance criteria owned by that issue are established.

If an issue owns a physical USB/RF/receiver criterion and the current worker lacks that hardware, leave a precise handoff instead of weakening the criterion.
