# TM-006 — Integration implementation and dependency hold

## Status

This branch provides dependency-ready TM-006 integration but **does not meet
the full issue #6 acceptance contract**. The genuine ESP-NOW sender belongs to
TM-005B / issue #16, which has not yet merged. This branch must not merge,
and issue #6 must remain open, until that predecessor is accepted and wired in.

## Implemented on this branch

- A hardware-independent controller (components/app_pipeline) combining the
  accepted QMI8658 sample contract, TM-004 orientation/velocity engine,
  TM-005 GPIO1–GPIO7 touch logic and TM-005A logical mouse reports.
- Source freshness is based on last successful acquisition, never the report
  timestamp. IMU and touch have independent provisional 100-ms age limits.
- Startup motion is gated on valid IMU plus 32 consecutive quiet, gravity-
  plausible samples. The first report after neutral capture is zero-motion.
- A real ESP-IDF application loop with a 4-ms esp_timer notification samples
  IMU at most once per tick and touch/reports at approximately 8-ms intervals.
  Delayed timer notifications are collapsed; there is no catch-up queue.
- BOOT/GPIO0 re-centres only after a deliberate 600–4000-ms press/release and
  only after BOOT was seen released after startup. This preserves flashing
  recovery and does not synthesize button presses/releases.
- Fractional motion residual is discarded on re-centre, IMU failure, lack of
  neutral, stale input or a delayed report gap longer than 50 ms.
- Touch invalidity latches a single explicit safety release, with no stale
  pressed-button state published. Fresh touch can retain held/drag state even
  when IMU/motion is temporarily unavailable.
- Output is initialized once with a single selected sink. The physical
  application binds USB; host tests instantiate USB and mock ESP-NOW sinks to
  check ownership without inventing a radio implementation.
- Diagnostic logs contain only counters and calibration state, not secrets.
  The existing USB descriptor/identity remains the accepted mouse-only HID.
- No infrastructure Wi-Fi/IP, BLE, RGB or ESP-NOW initialization is introduced.

Values/rates are hypotheses for TM-008 physical tuning, not final usability
acceptance. Actual ESP-NOW binding is explicitly absent.

## TM-005B prerequisite

TM-005B must merge first with the exact accepted faikeow contracts:

- FNR-006 platform v1 / relative_mouse v1 at receiver commit
  136ef3b612f83fd9cad74a5dd50c2e5f629b44de, 18-byte envelope header,
  four-byte mouse profile and sender session/sequence behavior;
- FNR-007 authenticated radio wrapper v1 and paired provisioning at receiver
  commit a49d95e61dc910c7db139db0280f80a31d10bfc8, with encrypted peer
  admission and per-device HMAC authentication.

Once TM-005B is merged, replace the fixed USB startup binding with explicit
USB-default or provisioned ESP-NOW startup selection, retaining one selected
output owner. Integrate TM-005B's real sender API, preserve its session/sequence
behavior, and add exact cross-repo compatibility and mode-selection tests.

The receiver owns ordering, timeout-neutralization, ESP8266–RP2040 transport
and USB receiver; none are reimplemented here.

## Evidence policy

The host suite checks timestamp freshness, startup calibration, re-centre,
button fusion, GPIO4 isolation, safe fault release, USB/wireless-mock output
routing, and delayed-motion drop. The required ESP32-S3 cross-build must also
pass. Neither hosted test nor target build establishes physical USB
enumeration, IMU orientation, touch ergonomics or RF receiver compatibility.
TM-007 owns additional lifecycle hardening; TM-008 owns physical acceptance.
