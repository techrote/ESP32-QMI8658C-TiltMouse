# Building and testing

## Pinned toolchain

TM-001 pins:

- ESP-IDF **v6.1** (release version 6.1.0);
- `espressif/esp_tinyusb` **2.3.0**;
- `espressif/tinyusb` **0.21.0~1**.

The ESP-IDF version is pinned in the component manifest and CI container. Dependency/toolchain changes require a deliberate repository decision and a clean target build.

## Current component state

Merged work includes:

- native USB mouse-only HID transport;
- QMI8658C acquisition;
- orientation/motion logic;
- capacitive-touch acquisition/button fusion.

TM-005A / #15 will introduce a logical mouse-report seam. TM-005B / #16 will add ESP-NOW support using ESP-IDF's Wi-Fi/ESP-NOW APIs.

## Transport build policy

### Direct USB

Direct USB is the default wired mode.

The HID transport exposes:

- left/right button bits;
- relative signed X;
- relative signed Y.

No keyboard interface, wheel/pan field or automatic demo movement is part of normal operation.

Direct USB mode should not initialize Wi-Fi merely because ESP-NOW support exists.

### ESP-NOW

ESP-NOW is the alternate wireless mode.

Wireless support may initialize the ESP32-S3 Wi-Fi subsystem only as required by ESP-NOW. It does not require:

- infrastructure Wi-Fi association;
- an IP address;
- TCP/UDP sockets;
- an AP service.

The packet contract remains <=250 bytes for compatibility with ESP-NOW v1 receivers such as ESP8266-class hardware.

### Bluetooth

Bluetooth LE HID/HOGP is deferred beyond v0.1. Current builds should not initialize Bluetooth/BLE.

## Firmware build

With ESP-IDF v6.1 activated:

```sh
idf.py set-target esp32s3
idf.py build
```

`idf.py set-target` may create local generated `sdkconfig` state; do not commit it unless a later task deliberately establishes project configuration.

Exact build-time/runtime transport selection will be documented by TM-006 once the transport seam and ESP-NOW implementation are merged.

## Host unit tests

```sh
cmake -S tests/host -B build/host -G Ninja
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Pure logic, logical report construction, USB adaptation and ESP-NOW packet/state semantics should remain testable without target hardware.

## Style/static sanity

```sh
python3 tools/check_style.py
```

The check enforces UTF-8/LF text, final newlines and no trailing whitespace. Host tests compile with `-Wall -Wextra -Wpedantic -Werror`.

## CI evidence boundary

The automated ESP32-S3 build establishes compatibility with the pinned SDK/components.

Host CI establishes deterministic software contracts.

Neither proves:

- physical USB enumeration;
- ESP-NOW RF behavior;
- ESP8266 receiver interoperability;
- physical cursor feel;
- touch reliability.

Those are TM-008 physical evidence.
