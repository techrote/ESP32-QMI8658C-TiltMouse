# ESP32-QMI8658C-TiltMouse

No-solder USB tilt-mouse firmware for the Waveshare **ESP32-S3-Matrix**.

The initial product uses the onboard **QMI8658C** accelerometer/gyroscope for tilt-driven cursor motion and the ESP32-S3 capacitive-touch channels as two logical mouse buttons:

- GPIO1–GPIO3: left click
- GPIO4: no click; reserved as separator/reference channel
- GPIO5–GPIO7: right click

The onboard 8×8 RGB matrix is deliberately out of scope for the first release.

## Project authority

Read these before implementation:

- [RAG.md](RAG.md) — compact authoritative project context and decisions.
- [AGENTS.md](AGENTS.md) — execution rules for issue-driven work.
- [docs/PROGRAMME.md](docs/PROGRAMME.md) — staged implementation plan and dependencies.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — firmware architecture and algorithms.
- [docs/VALIDATION.md](docs/VALIDATION.md) — CI, host tests, and physical-device evidence.
- [docs/BUILDING.md](docs/BUILDING.md) — pinned toolchain and local build/test commands.
- [docs/PLAN-REVIEW.md](docs/PLAN-REVIEW.md) — review of the initial plan and improvements adopted.

## Foundation

The project is pinned to ESP-IDF **v6.1**, with `espressif/esp_tinyusb` **2.3.0** and `espressif/tinyusb` **0.21.0~1** reserved for the native-USB implementation track.

The foundation firmware currently does one target-specific thing: it drives RGB matrix data GPIO14 to a quiescent low state. USB HID, IMU, touch sensing, Wi-Fi, Bluetooth, and LED effects are intentionally not initialized until their owning tasks.

See [docs/BUILDING.md](docs/BUILDING.md) for exact commands.

## Initial scope

The first usable release is a standard USB HID mouse with:

- relative X/Y motion derived from pitch/roll;
- accelerometer-corrected gyro orientation estimation;
- grouped capacitive-touch left/right buttons;
- startup neutral calibration and runtime re-centre;
- no host driver;
- no soldering;
- no LED features, Wi-Fi, or Bluetooth.

Implementation work is tracked through GitHub issues under the **TM-###** task IDs.
