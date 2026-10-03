# RAG — ESP32-QMI8658C-TiltMouse

This file is the compact authoritative context for implementation agents. If an issue prompt conflicts with this file, reconcile live repository state and prefer the newest explicit repository decision.

## Mission

Create firmware for the Waveshare ESP32-S3-Matrix that behaves as a standard USB HID mouse. Cursor movement is controlled by tilting the board. Mouse buttons are implemented with the ESP32-S3 capacitive-touch channels using only the existing exposed board contacts; the project must remain no-solder.

## Fixed product constraints

- Target board: Waveshare ESP32-S3-Matrix.
- Firmware framework: ESP-IDF.
- USB transport: ESP32-S3 native USB device peripheral using TinyUSB / `esp_tinyusb`.
- No host-side driver should be required for normal mouse use.
- Motion sensor: onboard QMI8658C 6-axis IMU.
- Left click: software fusion of touch channels GPIO1, GPIO2, GPIO3.
- GPIO4: never a mouse click; reserve as separator/reference/diagnostic channel.
- Right click: software fusion of touch channels GPIO5, GPIO6, GPIO7.
- Do not electrically tie the touch pins together. Group them only in software.
- Do not use the LED matrix in the initial release. Do not initialize LED effects. Keep LED data quiescent.
- Do not initialize Wi-Fi or Bluetooth in the initial release.
- Preserve a reliable BOOT/RESET flashing recovery path.
- Do not claim physical-device acceptance without physical evidence.

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

The QMI8658C `WHO_AM_I` register is 0x00 and should read 0x05. QMI8658C rev. 0.9 documents the 7-bit I2C address as 0x6A when SA0 is pulled high or left unconnected and 0x6B when SA0 is pulled low. Firmware should verify identity and probe 0x6A then 0x6B instead of relying on a board-address assumption.

## Motion model

Do not use free-running gyro integration as the tilt estimate.

Use:
1. accelerometer-derived gravity direction as the long-term reference;
2. gyroscope angular rate for low-latency short-term motion;
3. a lightweight complementary filter or equivalent bounded estimator for pitch/roll;
4. a neutral orientation captured at startup and on explicit re-centre;
5. tilt angle mapped to **relative cursor velocity**, not absolute screen position.

Initial tunables should be centralized and documented:
- IMU sample rate;
- HID report rate;
- deadzone;
- maximum useful tilt;
- X/Y gain and axis inversion;
- response exponent / curve;
- output clamp;
- complementary-filter time constant.

Preserve sub-report motion with fractional residual accumulators so small motions are not lost by integer HID reports.

## Touch-button model

Every touch channel is calibrated independently. Maintain per-channel baseline and noise estimates.

Logical button fusion must be testable and configurable. The implementation should support or experimentally compare:
- normalized summed evidence;
- one-strong-or-two-moderate evidence;
- 2-of-3 voting.

Use separate press/release thresholds (hysteresis), debounce, and baseline adaptation only while a channel/group is not actively touched.

GPIO4 is not a button. Initially collect it for diagnostics. Use it as common-mode/environmental compensation only if evidence shows that it improves false-positive/false-negative behaviour.

## Usability detail: click-induced motion

Touching the board can mechanically perturb its orientation. Integration must measure this. If physical evidence shows objectionable cursor jumps on press/release, add a narrowly bounded mitigation such as a short transition damping/freeze window or estimator gating. Keep this configurable and do not introduce it speculatively without evidence.

## Firmware decomposition

Keep hardware access separate from pure logic so most behaviour can be tested on a normal CI host.

Expected modules:

- `board`: pin/resource definitions and safe startup.
- `qmi8658`: I2C register driver and scaled samples.
- `orientation`: pitch/roll estimator and neutral transform.
- `mouse_motion`: deadzone/curve/velocity/residual logic.
- `touch_input`: raw touch acquisition, calibration, normalization.
- `touch_buttons`: left/right fusion, hysteresis, debounce.
- `usb_hid_mouse`: descriptors, mount state, reports, suspend/resume.
- `app`: scheduling and integration.
- optional diagnostic transport/build mode for telemetry.

Pure estimator, motion and button-fusion logic must be host-testable without ESP32 hardware.

## Build and diagnostics strategy

TM-001 selected and pinned the stable ESP-IDF **v6.1** release (6.1.0), `espressif/esp_tinyusb` **2.3.0**, and `espressif/tinyusb` **0.21.0~1**. The ESP-IDF pin is enforced by the component manifest and CI container; managed USB dependencies are exact rather than floating.

ESP-IDF v6.1 provides the modern `esp_driver_touch_sens` / `driver/touch_sens.h` touch API required by TM-005. The pinned `esp_tinyusb` release supports ESP32-S3 and HID and is reserved for TM-002; TM-001 does not initialize USB HID.

Normal release identity: HID mouse only.

A diagnostic build may expose TinyUSB CDC + HID as a composite device if useful for raw touch/IMU telemetry. It must not become a requirement for normal use.

The onboard LEDs remain unused in both normal and diagnostic builds unless a later issue explicitly changes scope.

## Acceptance principles

Hosted CI can establish:
- clean ESP-IDF build for `esp32s3`;
- host unit tests for pure logic;
- deterministic replay tests;
- static/format checks where practical;
- descriptor/report compile-time checks.

Hosted CI cannot establish:
- touch sensitivity on the physical board;
- ergonomics;
- actual USB enumeration on the target board;
- real cursor feel;
- click-induced physical jitter.

Never substitute simulated evidence for those physical claims.

## v0.1 definition

v0.1 is complete when a physical ESP32-S3-Matrix can:
- enumerate as a standard USB mouse;
- remain stationary in a comfortable neutral pose;
- move cursor smoothly and predictably by tilt;
- left-click/hold/drag using GPIO1–3 group;
- right-click/hold using GPIO5–7 group;
- ignore GPIO4 as a click;
- re-centre without reflashing;
- survive USB reconnect and ordinary suspend/resume;
- recover safely from IMU read failures;
- run with LED matrix, Wi-Fi and Bluetooth unused.

## Non-goals for v0.1

- RGB feedback or animations;
- wireless HID;
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
2. Waveshare resources:
   https://docs.waveshare.com/ESP32-S3-Matrix/Resources-And-Documents
3. Waveshare schematic:
   https://files.waveshare.com/wiki/ESP32-S3-Matrix/ESP32-S3-Matrix-Sch.pdf
4. QMI8658C datasheet:
   https://files.waveshare.com/wiki/common/QMI8658C.pdf
5. ESP32-S3 capacitive touch:
   https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/cap_touch_sens.html
6. ESP32-S3 USB device stack:
   https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html
7. Espressif TinyUSB HID device example:
   https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_hid

## Programme order

- TM-001: repository/ESP-IDF/CI foundation.
- TM-002: native USB HID mouse transport.
- TM-003: QMI8658C driver and sample acquisition.
- TM-004: orientation estimator and tilt-to-motion engine.
- TM-005: capacitive-touch acquisition and grouped buttons.
- TM-006: integrated tilt mouse and re-centre/diagnostics.
- TM-007: robustness and usability hardening.
- TM-008: physical acceptance and tuning.
- TM-009: v0.1 release consolidation.

Parallelism after TM-001: TM-002, TM-003 and TM-005 may proceed independently. TM-004 depends on TM-003. TM-006 depends on TM-002, TM-004 and TM-005.
