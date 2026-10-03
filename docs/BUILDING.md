# Building and testing

## Pinned toolchain

TM-001 pins the foundation to:

- ESP-IDF **v6.1** (release version 6.1.0);
- `espressif/esp_tinyusb` **2.3.0**;
- `espressif/tinyusb` **0.21.0~1**.

The managed USB components are declared so the USB implementation inherits a known dependency set. TM-002 enables exactly one TinyUSB HID interface and supplies an explicit one-interface mouse configuration/report descriptor. The normal build does not enable CDC or any other USB device class.

The ESP-IDF version is pinned in both `main/idf_component.yml` and the CI container image. Do not update any of these versions independently: dependency/toolchain changes require a deliberate repository decision and a clean firmware build.

## Native USB HID state

TM-002 initializes the ESP32-S3 native USB device peripheral during normal startup after the board safe-state initialization.

The HID transport exposes only:

- left/right button bits;
- relative signed X movement;
- relative signed Y movement.

There is no keyboard interface, wheel/pan report field, CDC interface, wireless transport, or automatic demo movement. Initializing the firmware therefore must not move the host cursor by itself; future integration tasks explicitly call the HID send API when real input is available.

## Firmware build

With ESP-IDF v6.1 activated:

```sh
idf.py set-target esp32s3
idf.py build
```

`idf.py set-target` may create a local `sdkconfig`. It is generated state and should not be committed unless a later task deliberately establishes a project configuration file.

## Host unit tests

The host-test harness deliberately does not depend on ESP-IDF:

```sh
cmake -S tests/host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

TM-002 adds host coverage for HID button masks, relative report construction, and the all-buttons-release report. Later pure algorithm components should extend this harness rather than hiding deterministic logic inside target-only callbacks.

## Style/static sanity

```sh
python3 tools/check_style.py
```

The check is intentionally dependency-free. It enforces UTF-8/LF text, final newlines, and no trailing whitespace. Host tests compile with `-Wall -Wextra -Wpedantic -Werror`, providing an additional static compiler sanity gate for pure C logic.

## CI evidence boundary

The automated firmware build establishes that the HID descriptors, TinyUSB calls and application wiring compile for ESP32-S3 against the pinned SDK/component versions. Host CI establishes the pure report/button semantics.

Neither proves physical USB enumeration or cursor behaviour on the Waveshare board. Those remain target-hardware evidence for TM-008.
