# Building and testing

## Pinned toolchain

TM-001 pins the foundation to:

- ESP-IDF **v6.1** (release version 6.1.0);
- `espressif/esp_tinyusb` **2.3.0**;
- `espressif/tinyusb` **0.21.0~1**.

The managed USB components are declared now so the USB implementation task inherits a known dependency set. TM-001 does not initialize TinyUSB or expose a USB HID interface.

The ESP-IDF version is pinned in both `main/idf_component.yml` and the CI container image. Do not update any of these versions independently: dependency/toolchain changes require a deliberate repository decision and a clean firmware build.

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

Later pure algorithm components should extend this harness rather than hiding deterministic logic inside target-only callbacks.

## Style/static sanity

```sh
python3 tools/check_style.py
```

The check is intentionally dependency-free. It enforces UTF-8/LF text, final newlines, and no trailing whitespace. Host tests compile with `-Wall -Wextra -Wpedantic -Werror`, providing an additional static compiler sanity gate for pure C logic.

## CI evidence boundary

The foundation CI proves that the source builds for ESP32-S3 under the pinned SDK and that host-only logic tests execute. It does **not** prove physical USB enumeration, IMU operation, capacitive-touch performance, or ergonomics; those require their owning issues and, where specified, physical-device evidence.
