# Firmware architecture

## Principles

- Relative mouse motion.
- Hardware drivers at the edges; deterministic logic in the middle.
- One transport-neutral logical mouse-report stream.
- Direct native USB HID is the default wired output.
- ESP-NOW is the alternate wireless output.
- Bluetooth LE HID/HOGP is deferred beyond v0.1.
- Minimal shared mutable state.
- Fixed-rate sampling/reporting with explicit timestamps.
- Tunables centralized rather than scattered.
- Exactly one normal output transport owns each report.

## Proposed tree

```text
CMakeLists.txt
sdkconfig.defaults
main/
  CMakeLists.txt
  app_main.c
  config.h
components/
  board/
  qmi8658/
  orientation/
  mouse_motion/
  touch_input/
  touch_buttons/
  mouse_report/
  usb_hid_mouse/
  espnow_mouse/
tests/
  host/
  fixtures/
```

Exact layout may evolve, but module boundaries should remain.

## Data flow

```text
QMI8658C -> qmi8658 -> orientation -> mouse_motion ----+
                                                       |
GPIO1..7 -> touch_input -> touch_buttons --------------+-> logical mouse report
GPIO0 -------------------------------------------------+-> re-centre/state control
                                                               |
                                            +------------------+------------------+
                                            |                                     |
                                      USB HID adapter                       ESP-NOW TX
                                            |                                     |
                                            v                                     v
                                         direct PC                       companion receiver
                                                                                  |
                                                                           ESP8266 -> RP2040
                                                                                  |
                                                                               USB HID
                                                                                  |
                                                                                  v
                                                                                 PC
```

Sensor, orientation, motion and button logic do not know which transport is selected.

## Scheduler

Initial target rates remain hypotheses pending TM-008:

- IMU sample: QMI8658C 235 Hz 6DOF configuration;
- motion estimator: IMU rate;
- logical mouse/report cadence: approximately 125 Hz;
- touch scan: approximately 100–200 Hz subject to evidence.

Use timestamps and bounded latest-sample semantics so delayed tasks do not generate bursts of stale motion.

Transport scheduling must not create unbounded backlogs. Wireless motion is latest-first rather than reliable replay of old deltas.

## Orientation estimator

TM-004 estimates pitch and roll only using measured-dt gyro integration with accelerometer gravity correction. It rejects/reduces gravity correction during implausible acceleration and rebases over stale gaps.

Yaw is neither observable from gravity nor needed for v0.1.

## Mouse motion mapping

For each axis:

1. subtract neutral orientation;
2. apply configurable sign/axis mapping;
3. apply deadzone;
4. map remaining tilt to velocity with a monotonic nonlinear curve;
5. clamp velocity;
6. integrate velocity over report interval;
7. retain fractional X/Y residual;
8. emit bounded integer relative deltas.

## Touch acquisition and fusion

Read GPIO1–GPIO7 independently.

- Left group = GPIO1,2,3.
- Right group = GPIO5,6,7.
- GPIO4 = reference/diagnostic only and never a button.

Calibration, normalization, fusion, hysteresis and debounce remain independent of output transport.

## Logical mouse report

TM-005A / #15 owns the seam between application logic and transport.

The logical report carries mouse semantics only:

- bounded relative X;
- bounded relative Y;
- complete current left/right button state;
- validity/release semantics required for safe fault handling.

Transport-specific metadata, including ESP-NOW sequence/version information, must not contaminate the core input/motion model.

During normal operation exactly one selected transport consumes reports. This prevents duplicate cursor motion when direct USB is available while wireless support is compiled in.

## Direct USB HID

Use native ESP32-S3 USB D+/D- via TinyUSB / `esp_tinyusb`.

The USB layer owns:

- descriptors;
- mount/suspend state;
- adaptation of logical reports into relative mouse HID reports;
- safe all-buttons-release.

Normal wired identity remains mouse-only. CDC is optional diagnostic functionality only.

## ESP-NOW transport

TM-005B / #16 owns ESP-NOW.

Wireless mode:

- initializes Wi-Fi only as required for ESP-NOW;
- does not require joining an access point;
- does not use TCP/IP or sockets;
- prefers unicast normal operation;
- uses a small versioned packet <=250 bytes for ESP8266 interoperability;
- includes full current button state and a sequence number;
- rejects stale/duplicate ordering deterministically;
- does not build a stale movement retransmission backlog.

Wi-Fi/ESP-NOW callbacks run in high-priority Wi-Fi context and therefore perform only bounded work before handing data/state to lower-priority application code.

## Companion receiver boundary

The intended receiver platform is [`techrote/faikeow-now-reciever`](https://github.com/techrote/faikeow-now-reciever), initially targeting clone Pico-W-style RP2040 + ESP8266/ESP8285 hardware.

This repository does **not** own receiver firmware or the generic platform/profile wire contract. It owns the TiltMouse sender-side mapping into faikeow's accepted generic envelope + `relative_mouse` profile.

The companion receiver is responsible for:

1. receiving and validating ESP-NOW packets on ESP8266;
2. forwarding newest valid reports across the board's internal ESP8266↔RP2040 link;
3. rejecting duplicate/stale reports;
4. releasing all buttons after link timeout;
5. presenting ordinary USB HID mouse reports from RP2040 to the PC.

The exact internal link, ESP radio flashing path, profile dispatch and receiver USB implementation belong to faikeow.

## Transport selection

v0.1 policy:

- USB = default wired transport;
- ESP-NOW = explicit alternate wireless transport;
- one active report owner at a time.

Automatic cable-detection/switching may be added only if TM-006/TM-007 can make it deterministic and prove that transitions cannot duplicate or replay movement. It is not required merely because both transports exist.

## Safe startup

At the earliest practical point:

- keep RGB data GPIO14 low/quiescent;
- do not initialize Bluetooth/BLE;
- initialize USB deterministically;
- initialize Wi-Fi only when selected ESP-NOW mode requires it;
- initialize IMU/touch with explicit error handling;
- do not emit cursor motion until neutral calibration is valid.

## Re-centre

Use runtime BOOT/GPIO0 for deliberate re-centre without breaking reset+BOOT flashing.

Re-centre should:

- capture/replace orientation neutral;
- clear fractional motion residuals;
- avoid generating a click;
- preserve selected transport where possible.

## Deferred BLE transport

The ESP32-S3 can support BLE, and a future transport may use HID over GATT. v0.1 deliberately avoids that complexity:

- no BLE HID/HOGP implementation;
- no pairing/bonding UI;
- no ESP-NOW/BLE coexistence tuning;
- no BLE acceptance gate.

The transport-neutral report seam should allow BLE to be added later without rewriting IMU, touch or motion logic.
