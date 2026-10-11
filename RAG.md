# RAG — ESP32-QMI8658C-TiltMouse

This file is the compact authoritative context for implementation agents. If an issue prompt conflicts with this file, reconcile live repository state and prefer the newest explicit repository decision.

## Mission

Create firmware for the Waveshare ESP32-S3-Matrix that behaves as a no-solder tilt mouse. Cursor movement is controlled by board tilt and mouse buttons by grouped capacitive-touch channels.

v0.1 supports two output paths over one transport-neutral logical mouse-report stream:

- direct native USB HID;
- ESP-NOW wireless using the generic `faikeow-now-reciever` platform and its `relative_mouse` profile.

Bluetooth LE HID/HOGP is deferred beyond v0.1 rather than removed as a future design option.

## Fixed product constraints

- Target board: Waveshare ESP32-S3-Matrix.
- Firmware framework: ESP-IDF.
- Wired transport: ESP32-S3 native USB device peripheral using TinyUSB / `esp_tinyusb`.
- Wireless transport: ESP-NOW using the ESP32-S3 Wi-Fi radio.
- Direct USB remains the default wired path.
- ESP-NOW is an alternate mode, not an IP-network transport.
- Do not add infrastructure Wi-Fi association, sockets, AP services, or unrelated network features for v0.1.
- Do not initialize Bluetooth/BLE in v0.1.
- No host-side driver should be required for normal mouse use in either path: wireless hosts see the companion receiver as ordinary USB HID.
- Motion sensor: onboard QMI8658C 6-axis IMU.
- Left click: software fusion of touch channels GPIO1, GPIO2, GPIO3.
- GPIO4: never a mouse click; reserve as separator/reference/diagnostic channel.
- Right click: software fusion of touch channels GPIO5, GPIO6, GPIO7.
- Do not electrically tie the touch pins together. Group them only in software.
- Do not use the LED matrix in the initial release. Keep LED data quiescent.
- Preserve a reliable BOOT/RESET flashing recovery path.
- Do not claim physical-device or RF acceptance without physical evidence.

## Board facts

From the Waveshare schematic and board documentation:

| Function | ESP32-S3 GPIO / resource |
| --- | --- |
| Native USB D- | GPIO19 |
| Native USB D+ | GPIO20 |
| QMI8658C SDA | GPIO11 |
| QMI8658C SCL | GPIO12 |
| QMI8658C INT1 | GPIO10 |
| QMI8658C INT2 | GPIO13 |
| RGB matrix data input | GPIO14 |
| BOOT button | GPIO0 |
| Touch left group | GPIO1, GPIO2, GPIO3 |
| Touch separator/reference | GPIO4 |
| Touch right group | GPIO5, GPIO6, GPIO7 |

ESP32-S3 touch hardware maps TOUCH1–TOUCH7 to GPIO1–GPIO7. The S3 uses touch hardware version 2, for which raw measurement values increase with added capacitance/touch.

The QMI8658C `WHO_AM_I` register is 0x00 and should read 0x05. QMI8658C rev. 0.9 documents the 7-bit I2C address as 0x6A when SA0 is pulled high or left unconnected and 0x6B when SA0 is pulled low. Firmware verifies identity and probes 0x6A then 0x6B rather than relying on a board-address assumption.

## Motion model

Do not use free-running gyro integration as the tilt estimate.

Use:
1. accelerometer-derived gravity direction as the long-term reference;
2. gyroscope angular rate for low-latency short-term motion;
3. a bounded complementary filter for pitch/roll;
4. a neutral orientation captured at startup and on explicit re-centre;
5. tilt angle mapped to **relative cursor velocity**, not absolute screen position.

TM-004 established the host-testable estimator/motion layer. Physical axis mapping and tuning remain TM-008 evidence.

Preserve sub-report motion with fractional residual accumulators so small motions are not lost by integer reports.

## Touch-button model

Every touch channel is calibrated independently. Maintain per-channel baseline and noise estimates.

Logical button fusion supports configurable evidence strategies with hysteresis and debounce. GPIO4 remains diagnostic/reference-only and never emits a click.

## Logical report and transport model

Application integration must not couple sensor/touch logic directly to TinyUSB or ESP-NOW.

TM-005A / #15 establishes `components/mouse_report/`: a validated four-byte logical report with bounded signed X/Y, full two-bit button state and explicit current/invalid/release kind. The bounded publisher retains desired buttons plus release debt but never queues integer movement. It sends through one selected sink, returning separate movement/state/reason outcomes. Transport-specific metadata remains outside the core report. See `docs/LOGICAL-REPORT.md`.

The completed TM-005A preparatory packet is [archived with its original ZIP, models, fixtures, results and API proposal](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/c9d813b0e313b9cd1f7017c596a7ce0404073a3c/checkpoints/prepass/TM-005A/a19aa425b973/PRESERVATION.md) at assessed commit `8d7313216916d8f7d554da20df1b5fb8603d0b21`. Its original bytes and Git modes were verified during publication on 8 October 2026. This is preserved historical prepass evidence. Its API-only sketch was reviewed rather than blindly installed; the production implementation and regression tests are separate.

Normal application report ownership is single-transport:

- USB mode -> logical report -> USB HID adapter;
- ESP-NOW mode -> logical report -> ESP-NOW packet encoder/TX.

Do not duplicate the same motion report across both transports during normal operation.

## ESP-NOW policy

TM-005B / #16 owns the TiltMouse sender implementation. The generic ESP-NOW platform envelope, profile IDs and `relative_mouse` receiver contract are owned by `techrote/faikeow-now-reciever` (not by TiltMouse).

The wireless protocol must:

- remain small enough for ESP-NOW v1 interoperability: application packet <=250 bytes;
- carry full current button state in every report;
- carry a monotonically advancing sequence number outside the core logical report;
- reject/ignore duplicate and stale/out-of-order reports deterministically;
- prefer newest movement over retransmitting stale movement;
- avoid unbounded retransmission queues;
- define link-loss timeout behavior that releases all buttons at the receiver;
- keep RF channel and peer/key provisioning explicit;
- prefer unicast for normal operation;
- keep Wi-Fi callbacks short and bounded.

The intended first receiver implementation is `techrote/faikeow-now-reciever` on clone Pico-W-class hardware containing RP2040 + ESP8266/ESP8285:

```text
ESP32-S3-Matrix
  logical mouse report
        |
     ESP-NOW
        v
ESP8266 receiver
        |
 internal serial/SPI-style board link
        v
RP2040
        |
     USB HID
        v
       PC
```

This repository owns the ESP32-S3 sender-side adaptation from TiltMouse logical reports into faikeow's generic platform envelope + `relative_mouse` profile. `techrote/faikeow-now-reciever` owns the generic envelope/profile contracts, receiver ordering/freshness semantics, clone-board interconnect, ESP radio firmware and RP2040 USB HID implementation.

## Usability detail: click-induced motion

Touching the board can mechanically perturb its orientation. TM-008 must measure this. Any mitigation remains evidence-driven and narrowly bounded.

## Firmware decomposition

Keep hardware access separate from pure logic so most behavior can be tested on a normal CI host.

Expected/established modules:

- `board`: pin/resource definitions and safe startup.
- `qmi8658`: I2C register driver and scaled samples.
- `orientation`: pitch/roll estimator and neutral transform.
- `mouse_motion`: deadzone/curve/velocity/residual logic.
- `touch_input`: raw touch acquisition, calibration, normalization.
- `touch_buttons`: left/right fusion, hysteresis, debounce.
- `mouse_report`: implemented transport-neutral logical report, serialized publisher and release debt.
- `usb_hid_mouse`: USB descriptor/lifecycle and logical-report adapter.
- `espnow_mouse`: ESP-NOW packet/TX transport.
- `app`: scheduling, neutral/re-centre and selected-transport integration.
- optional diagnostic transport/build mode for telemetry.

## Build and diagnostics strategy

TM-001 pinned ESP-IDF **v6.1** (6.1.0), `espressif/esp_tinyusb` **2.3.0**, and `espressif/tinyusb` **0.21.0~1**.

Normal direct-USB identity remains mouse-only. A diagnostic build may expose HID+CDC if useful.

ESP-NOW mode may depend on ESP-IDF `esp_wifi` / `esp_now`; direct USB mode should not initialize Wi-Fi merely because wireless support exists in the binary.

The onboard LEDs remain unused unless a later issue explicitly changes scope.

## Acceptance principles

Hosted CI can establish:

- clean ESP32-S3 build;
- host unit/replay tests;
- USB report contracts;
- ESP-NOW packet/sequence/state contracts;
- transport ownership and fault-state logic.

Hosted CI cannot establish:

- physical USB enumeration;
- real ESP-NOW RF behavior;
- compatibility with a particular clone receiver board;
- physical axes, drift or cursor feel;
- touch sensitivity/ergonomics;
- click-induced physical jitter.

TM-008 requires physical evidence for both direct USB and the complete ESP-NOW -> receiver -> USB HID path.

## v0.1 definition

v0.1 is complete when a physical ESP32-S3-Matrix can:

- enumerate directly as a standard USB mouse in wired mode;
- operate wirelessly through a compatible ESP-NOW receiver that enumerates as standard USB HID;
- remain stationary in a comfortable neutral pose;
- move cursor smoothly and predictably by tilt;
- left-click/hold/drag using GPIO1–3;
- right-click/hold using GPIO5–7;
- ignore GPIO4 as a click;
- re-centre without reflashing;
- survive ordinary USB and wireless link-loss/recovery paths safely;
- recover safely from IMU read failures;
- run with the LED matrix unused;
- run without Bluetooth/BLE.

## Non-goals for v0.1

- Bluetooth LE HID/HOGP;
- Bluetooth/Wi-Fi coexistence tuning;
- infrastructure Wi-Fi, TCP/IP or socket networking;
- RGB feedback or animations;
- scrolling gestures;
- middle click;
- persistent multi-profile GUI;
- full quaternion/yaw pointing;
- magnetometer support;
- elaborate AHRS;
- absolute pointing;
- soldered touch electrodes.

## Authoritative external documentation

1. Waveshare ESP32-S3-Matrix:
   https://docs.waveshare.com/ESP32-S3-Matrix
2. Waveshare schematic:
   https://files.waveshare.com/wiki/ESP32-S3-Matrix/ESP32-S3-Matrix-Sch.pdf
3. QMI8658C datasheet:
   https://files.waveshare.com/wiki/common/QMI8658C.pdf
4. ESP32-S3 capacitive touch:
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/cap_touch_sens.html
5. ESP32-S3 USB device stack:
   https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html
6. ESP-NOW API:
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_now.html
7. ESP-NOW v1/v2 interoperability:
   https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html

## Programme order

Completed foundation/feature work:

- TM-001: repository/ESP-IDF/CI foundation.
- TM-002: native USB HID mouse transport.
- TM-003: QMI8658C driver and sample acquisition.
- TM-004: orientation estimator and tilt-to-motion engine.
- TM-005: capacitive-touch acquisition and grouped buttons.

Pivot/integration work:

- TM-005A / #15: logical mouse-report and transport abstraction (implemented).
- TM-005B / #16: ESP-NOW sender implementing faikeow platform envelope + `relative_mouse` profile.
- TM-006 / #6: integrated tilt mouse with selectable USB/ESP-NOW output.
- TM-007 / #7: wired/wireless fault tolerance and hardening.
- TM-008 / #8: physical USB + ESP-NOW acceptance and tuning.
- TM-009 / #9: v0.1 wired/wireless release consolidation.

TM-005A depends on TM-002/TM-004/TM-005. TM-005B depends on TM-005A and must reconcile an accepted faikeow platform/profile v1 contract before freezing bytes. TM-006 depends on both pivot tasks plus the completed sensor/input work.

## Receiver platform authority

Generic receiver platform: https://github.com/techrote/faikeow-now-reciever

Relevant faikeow authority:
- `RAG.md`
- `docs/02-PROTOCOLS.md`
- `docs/08-HID-PROFILES.md`
- FNR-006 / issue #7 — generic receiver core + `relative_mouse` profile
- FNR-008 / issue #9 — physical platform/reference-sender acceptance
- FNR-009 / issue #10 — v0.1 receiver release
