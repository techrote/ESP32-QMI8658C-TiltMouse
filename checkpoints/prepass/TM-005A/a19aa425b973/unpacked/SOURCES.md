# Source locators and assessed state

All TiltMouse file URLs below are pinned to the assessed commit. Source citations in FINDINGS.md refer to these IDs. Exact originals needed by the focused C probe are retained under `evidence/assessed`; other sources were read through the GitHub connector/web, not copied in full.

## S1 — Owning issue and current application

- [issue #15](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/issues/15) — Required work and acceptance; complete comment list was empty.
- [main/app_main.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/main/app_main.c) — app_main; USB init only.

## S2 — Core architecture

- [RAG.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/RAG.md) — Motion model; logical report and transport model; pinned dependencies.
- [docs/ARCHITECTURE.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/docs/ARCHITECTURE.md) — Scheduler; mouse motion mapping; logical report; transport selection; re-centre.

## S3 — Wireless contract boundary

- [docs/WIRELESS.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/docs/WIRELESS.md) — Latest-first movement; complete button state; ordering; failure semantics.

## S4 — USB baseline

- [components/usb_hid_mouse/usb_hid_mouse.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/usb_hid_mouse/usb_hid_mouse.c) — L27–52 descriptor; L88–109 event callback; L148–166 send; L168–182 release.
- [components/usb_hid_mouse/usb_hid_mouse_report.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/usb_hid_mouse/usb_hid_mouse_report.c) — button_mask, make_report, release_report.
- [components/usb_hid_mouse/include/tiltmouse/usb_hid_mouse.h](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/usb_hid_mouse/include/tiltmouse/usb_hid_mouse.h) — public raw send and release API.
- [components/usb_hid_mouse/include/tiltmouse/usb_hid_mouse_report.h](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/usb_hid_mouse/include/tiltmouse/usb_hid_mouse_report.h) — three-byte payload structure.

## S5 — Motion

- [components/mouse_motion/mouse_motion.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/mouse_motion/mouse_motion.c) — L64–72 reset; L107–127 velocity; L129–140 integration; L142–174 step.
- [components/mouse_motion/include/tiltmouse/mouse_motion.h](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/mouse_motion/include/tiltmouse/mouse_motion.h) — residual state, delta, status, configuration.
- [tests/host/test_orientation_motion.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/tests/host/test_orientation_motion.c) — test_fractional_accumulation_and_recentre_reset; test_report_delta_clamp_discards_excess_backlog.

## S6 — Touch

- [components/touch_buttons/touch_buttons.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/touch_buttons/touch_buttons.c) — update_group; tiltmouse_touch_buttons_update; null guard and full-state output.
- [components/touch_buttons/include/tiltmouse/touch_buttons.h](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/touch_buttons/include/tiltmouse/touch_buttons.h) — group state and calibrated full-state output.
- [tests/host/test_touch_buttons.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/tests/host/test_touch_buttons.c) — test_hold_release_and_baseline_freeze; test_simultaneous_buttons; test_gpio4_never_clicks.

## S7 — Source timestamps

- [components/orientation/include/tiltmouse/orientation.h](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/components/orientation/include/tiltmouse/orientation.h) — timestamped IMU sample and estimator state versus untimestamped pose.

## S8 — Existing USB tests

- [tests/host/test_usb_hid_mouse.c](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/tests/host/test_usb_hid_mouse.c) — test_report_construction preserves INT8_MIN; payload-only tests.

## S9 — Programme and integration

- [docs/PROGRAMME.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/docs/PROGRAMME.md) — Completed foundations; TM-005A/B, TM-006 and TM-007 scope.
- [issue #6](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/issues/6) — Scheduler, neutral/re-centre, drag, single selected output; faikeow boundary.

## S10 — Instructions and gates

- [AGENTS.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/AGENTS.md) — Read-first, single-owner, hardware-evidence and write rules; user read-only instruction governs this turn.
- [docs/VALIDATION.md](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/docs/VALIDATION.md) — Automated checks, transport tests, physical acceptance.
- [tests/host/CMakeLists.txt](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/8d7313216916d8f7d554da20df1b5fb8603d0b21/tests/host/CMakeLists.txt) — C11 strict-warning targets; USB target includes only pure report helper.

## E1 — Upstream stack corroboration, not managed artifact verification

- [TinyUSB 0.21.0 hid_device.c](https://github.com/hathach/tinyusb/blob/0.21.0/src/class/hid/hid_device.c) — tud_hid_n_ready; tud_hid_n_report; completion/failed weak callbacks; five-field convenience mouse report.

## E2 — Espressif primary guidance

- [ESP-NOW API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_now.html) — Send ESP-NOW Data: MAC success vs application delivery, send callback sequencing, bounded callback work. Retrieved 2026-10-04; latest docs are mutable..

## E3 — Primary HID descriptor explanation

- [Linux kernel HID introduction](https://docs.kernel.org/hid/hidintro.html) — Parsing HID report descriptors and mouse report examples: Input items commit button/padding/relative fields. Retrieved 2026-10-04..

## Retrieval and scope limits

Issue #15 was open, with zero comments. Open PRs were empty. Eight visible branches were listed; none was a TM-005A work branch. Main was read at the start and rechecked near packaging with the same SHA. No CI action was taken and no green-CI assertion is made.

Local git network retrieval failed at DNS, so connector text was retained and checked against Git tree/contents blob IDs. Seven originals matched exactly. The full repository was not cloned. The complete original host suite, target toolchain and managed TinyUSB revision were not built or recovered. Touch code was inspected but is not part of the compiled retained-source probe. Faikeow wire bytes and receiver implementation were not independently audited; the current TiltMouse authority identifies that boundary.
