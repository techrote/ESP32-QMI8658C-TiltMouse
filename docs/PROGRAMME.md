# Programme — TiltMouse v0.1

## Goal

Deliver a no-solder wired/wireless tilt mouse on the Waveshare ESP32-S3-Matrix with QMI8658C motion sensing, grouped capacitive-touch buttons, direct USB HID, and ESP-NOW as an alternate wireless path through a companion USB receiver.

Bluetooth LE HID/HOGP is deferred beyond v0.1.

## Dependency graph

```text
TM-001 foundation
├── TM-002 USB HID ──────────────┐
├── TM-003 QMI8658C              │
│   └── TM-004 orientation       ├── TM-005A logical report seam
└── TM-005 touch buttons ────────┘          │
                                            ▼
                                  TM-005B ESP-NOW TX/protocol
                                            │
                                            ▼
                                       TM-006 integration
                                            │
                                            ▼
                                       TM-007 hardening
                                            │
                                            ▼
                              TM-008 physical USB + wireless
                                            │
                                            ▼
                                  TM-009 v0.1 release
```

TM-005A is issue #15. TM-005B is issue #16. Existing TM-006 through TM-009 retain their original issue numbers #6–#9.

## Work packages

### TM-001 — Foundation and CI — complete

Established the pinned ESP-IDF project, board definitions, host-test infrastructure and CI.

### TM-002 — USB HID transport — complete

Implemented the direct mouse-only native USB HID transport independently of sensor/input logic.

### TM-003 — QMI8658C driver — complete

Implemented deterministic I2C identity/configuration/acquisition and scaled timestamped samples.

### TM-004 — Orientation and motion engine — complete

Implemented bounded complementary-filter pitch/roll, neutral transform, deadzone/nonlinear velocity mapping, clamps and fractional output accumulation as pure logic.

### TM-005 — Touch acquisition and grouped buttons — complete

Implemented independent GPIO1–GPIO7 touch acquisition/calibration/fusion with GPIO4 permanently non-clicking.

### TM-005A / #15 — Logical mouse report and transport abstraction — implemented

Introduced a transport-neutral logical report and single-owner bounded publication seam. Refactored native USB HID to consume validated logical reports, preserved the mouse-only interface, and corrected the previously missing button-data Input descriptor item. Added host verification for release/re-press barriers, movement drops, adapter mapping and the exact production HID descriptor. No Wi-Fi or ESP-NOW is initialized by this task.

**The original preparation remains archived as historical evidence.** The [TM-005A preservation record](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/c9d813b0e313b9cd1f7017c596a7ce0404073a3c/checkpoints/prepass/TM-005A/a19aa425b973/PRESERVATION.md) links the exact original ZIP, complete unpacked packet and publication manifest. Read the original [findings](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/c9d813b0e313b9cd1f7017c596a7ce0404073a3c/checkpoints/prepass/TM-005A/a19aa425b973/unpacked/FINDINGS.md) and [handoff](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/c9d813b0e313b9cd1f7017c596a7ce0404073a3c/checkpoints/prepass/TM-005A/a19aa425b973/unpacked/HANDOFF.md) before implementation. The checkpoint preserves executable prototypes, fixtures, original source extracts, results and an unapplied API-only patch.

The retained historical evidence reports 10 fixture scenarios plus separate extra checks, 6,400 seeded stress steps, five detected deliberate defects and 260,100 focused C field comparisons. The original full host suite, target firmware build and physical USB/RF acceptance were not established by that prepass. Archive integrity alone did not satisfy issue #15 acceptance or install the proposed policy; the new production code and CI-tested regression suite do that separately. Physical-device evidence remains TM-008. The preservation record reconciles the earlier conversation's scenario-count discrepancy without changing the original files.

### TM-005B / #16 — ESP-NOW sender for the faikeow relative-mouse profile

Implement ESP-NOW TX on the ESP32-S3 and map the TM-005A logical mouse report into the accepted `techrote/faikeow-now-reciever` generic platform envelope + `relative_mouse` profile. Do not define a competing private receiver protocol.

The application packet must remain <=250 bytes for ESP-NOW v1 interoperability with ESP8266-class receivers. Movement uses latest-first semantics; full button state is repeated so loss cannot permanently stick a button.

### TM-006 / #6 — Integrated mouse

Join IMU, orientation, motion and touch into one logical report stream, then route reports through exactly one selected output: direct USB or ESP-NOW. Add startup neutral calibration, BOOT-button re-centre and optional diagnostics.

### TM-007 / #7 — Robustness and usability hardening

Harden USB lifecycle, ESP-NOW link/sequence/timeout behavior, transport transitions, IMU failures, touch failures, stale data and safe button release. Add deterministic fault/replay tests and physical acceptance telemetry.

### TM-008 / #8 — Physical USB + faikeow ESP-NOW acceptance and tuning

Use the real ESP32-S3-Matrix to establish USB enumeration, axes, neutral drift, cursor feel and touch behavior. Coordinate with faikeow FNR-008 / #9 to establish the full generic-envelope + `relative_mouse` ESP-NOW -> receiver -> USB HID path with exact cross-repo artifacts.

### TM-009 / #9 — v0.1 wired/wireless release consolidation with faikeow compatibility

Freeze evidence-backed defaults, document wired/wireless operation and receiver assumptions, produce reproducible artifacts and explicitly record BLE/HOGP as deferred.

## Wireless pivot rationale

The original programme deliberately excluded wireless HID. After the input, motion and USB layers were independently completed, the available receiver hardware changed the practical design:

- the intended spare Pico W boards were identified as clones using RP2040 + ESP8266 rather than CYW43439;
- those boards cannot satisfy the separate Bluetooth-HCI project, but the ESP8266 remains useful as an ESP-NOW radio;
- the RP2040 remains well suited to presenting standard USB HID to a PC;
- the ESP32-S3-Matrix already contains the Wi-Fi radio needed for ESP-NOW;
- the pivot occurred before TM-006 application integration, so a transport seam can be inserted without rewriting the completed sensor/input algorithms.

The first receiver architecture is therefore:

```text
TiltMouse ESP32-S3
   |
 ESP-NOW
   |
ESP8266 on clone Pico-W board
   |
 board-internal link
   |
RP2040
   |
 USB HID
   |
 PC
```

The receiver is a companion project. This repository owns the transmitter and interoperable wire contract.

## Plan review and improvements

The programme retains the original principles:

1. independent feature tracks before integration;
2. pure host-testable logic boundaries;
3. explicit hardware-evidence boundaries;
4. robust QMI identity/address handling;
5. software-only touch grouping;
6. GPIO4 never clicks;
7. click-induced movement is measured rather than guessed;
8. exact framework/toolchain pinning;
9. LED non-use is preserved.

The pivot adds:

10. **Transport neutrality before integration.** Input/motion logic must emit one logical report rather than call USB directly.
11. **ESP-NOW as the v0.1 wireless path.** Direct USB stays the default.
12. **ESP8266 compatibility.** Keep application packets within the ESP-NOW v1 250-byte interoperability limit.
13. **Latest-first motion.** Do not retransmit stale cursor movement to fill packet gaps.
14. **Complete button state.** Repeat full button state in every wireless report and release all buttons on receiver timeout.
15. **No hidden IP networking.** Wi-Fi is initialized only as required for ESP-NOW.
16. **BLE remains a future transport.** Do not carry BLE/HOGP or coexistence complexity in v0.1.
