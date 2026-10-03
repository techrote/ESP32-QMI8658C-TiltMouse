# ESP32-QMI8658C-TiltMouse

No-solder wired/wireless tilt-mouse firmware for the Waveshare **ESP32-S3-Matrix**.

The product uses the onboard **QMI8658C** accelerometer/gyroscope for tilt-driven cursor motion and the ESP32-S3 capacitive-touch channels as two logical mouse buttons:

- GPIO1–GPIO3: left click
- GPIO4: no click; reserved as separator/reference channel
- GPIO5–GPIO7: right click

v0.1 now targets two transport modes:

- **direct USB HID** — the ESP32-S3 enumerates as a standard USB mouse;
- **ESP-NOW wireless** — the ESP32-S3 encodes the generic faikeow `relative_mouse` profile and sends it to a compatible faikeow receiver, which presents standard USB HID to the PC.

The receiver platform is maintained separately in [`techrote/faikeow-now-reciever`](https://github.com/techrote/faikeow-now-reciever). Its first accepted hardware target is a clone "Pico W" containing RP2040 plus ESP8266/ESP8285-class Wi-Fi hardware. TiltMouse is the first reference sender for faikeow's generic `relative_mouse` profile.

Bluetooth LE HID/HOGP remains a possible future transport, but it is explicitly deferred beyond v0.1. The onboard 8×8 RGB matrix also remains out of scope.

## Project authority

Read these before implementation:

- [RAG.md](RAG.md) — compact authoritative project context and decisions.
- [AGENTS.md](AGENTS.md) — execution rules for issue-driven work.
- [docs/PROGRAMME.md](docs/PROGRAMME.md) — staged implementation plan and dependencies.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — firmware architecture and algorithms.
- [docs/VALIDATION.md](docs/VALIDATION.md) — CI, host tests, and physical-device evidence.
- [docs/WIRELESS.md](docs/WIRELESS.md) — USB/ESP-NOW transport policy and receiver boundary.
- [docs/BUILDING.md](docs/BUILDING.md) — pinned toolchain and local build/test commands.
- [docs/PLAN-REVIEW.md](docs/PLAN-REVIEW.md) — initial plan review plus the later ESP-NOW pivot.

## Current implementation

The project is pinned to ESP-IDF **v6.1**, with `espressif/esp_tinyusb` **2.3.0** and `espressif/tinyusb` **0.21.0~1**.

Completed components now include:

- board-safe startup and CI foundation;
- native mouse-only USB HID transport;
- QMI8658C I2C acquisition with deterministic failure handling;
- host-testable pitch/roll estimation and tilt-to-relative-motion mapping;
- capacitive-touch acquisition and grouped logical buttons.

These components are not yet joined into the final application scheduler. The next steps are:

1. **TM-005A / #15** — introduce the logical mouse-report / transport seam;
2. **TM-005B / #16** — add ESP-NOW TX and encode faikeow's generic platform envelope + `relative_mouse` profile;
3. **TM-006 / #6** — integrate the complete input pipeline with selectable USB or ESP-NOW output.

The default wired path must not initialize Wi-Fi unnecessarily. ESP-NOW mode may initialize the ESP32-S3 Wi-Fi subsystem only as required for connectionless ESP-NOW; it does not require infrastructure Wi-Fi or IP networking. Bluetooth/BLE remains uninitialized for v0.1.

See [docs/BUILDING.md](docs/BUILDING.md) for exact commands.

## v0.1 scope

The first usable release should provide:

- relative X/Y motion derived from bounded pitch/roll estimation;
- grouped capacitive-touch left/right buttons;
- startup neutral calibration and runtime re-centre;
- direct USB HID with no host driver;
- ESP-NOW wireless operation through a compatible `faikeow-now-reciever` implementation of the generic `relative_mouse` profile;
- no soldering on the TiltMouse board;
- no LED features;
- no infrastructure Wi-Fi/IP networking;
- no Bluetooth requirement.

Implementation work is tracked through GitHub issues under the **TM-###** task IDs.
