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
- capacitive-touch acquisition/button fusion;
- TM-005A transport-neutral report/publisher and native USB adapter.

TM-005A / #15 introduced that logical mouse-report seam. TM-005B / #16 adds a selectable authenticated ESP-NOW sender API without changing default USB startup. It must be bound by TM-006 to a matching operator-provisioned receiver configuration; no secrets or universal key are embedded in a factory image. See docs/ESPNOW-SENDER.md.

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

The host suite includes pure logical report construction, bounded transport policy, USB adaptation and a parser of the exact production HID report descriptor. ESP-NOW packet/state tests will be introduced by TM-005B; no wireless implementation is present yet.

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

## TM-006 output selection and private provisioning

Normal builds use the USB HID mouse only and do not initialize Wi-Fi.
To build the provisioned ESP-NOW mode, select TILTMOUSE_OUTPUT_ESPNOW
with idf.py menuconfig, and create an untracked local header at
main/tiltmouse_sender_config.local.h defining TM_ESPNOW_LOCAL_CONFIG.
The macro expands to a tm_espnow_mouse_config_t struct initializer with
sender_mac (this ESP32-S3 station MAC), receiver_mac (paired radio MAC),
peer_key (the exact per-device 16-byte FNR-007 LMK), channel (1..14),
config_id (nonzero u64), and generation (nonzero u32). Source values
must match the same FNR-007 JSON used to build the receiver pair;
peer_mac in that JSON is the TiltMouse sender station MAC.

The header is gitignored because it contains the pairing key. Do not
commit, paste, or print real key material. Wireless builds without it
fail at compile time. Runtime rejects station-MAC mismatch and invalid
pairing, without switching to USB or sending plaintext/unwrapped data.
See docs/TM-006-INTEGRATION.md and docs/ESPNOW-SENDER.md.

## TM-007 runtime bounds and diagnostics

Normal scheduling remains a single serialized application owner driven by a
4 ms timer notification. IMU polling is nominally 4 ms; touch and logical
report cadence are nominally 8 ms. Notifications collapse rather than queue,
so delayed work produces a measured gap and dropped interval instead of a
catch-up cursor burst. The motion layer caps accepted report dt at 50 ms and
the application source freshness hypothesis remains 100 ms pending TM-008.

Repeated QMI or touch-driver failures cause at most one reinitialization
attempt per 100 ms after a three-error threshold. Persistent QMI `NO_DATA`
beyond the configured IMU freshness interval enters that same bounded recovery
path without treating ordinary empty polls as errors. ESP-NOW recovery likewise
performs one bounded radio setup attempt when due, with exponential 50 ms to
1 s backoff. There is still only one logical report in flight and no relative
movement retry queue.

The 10-second diagnostic snapshot records application report/movement/drop
counts, IMU/touch errors/staleness/recovery/recalibration, maximum observed
loop gap, collapsed timer slots and FreeRTOS stack high-water mark. USB mode
also records mount/suspend and lifecycle-reset/completion failures. Wireless
mode records send attempts, immediate/callback failures, recovery attempts,
sender session and next platform sequence. Button transitions open a provisional 100 ms observation window; the retained
peak signed X/Y and number of generated reports in that window are logged for
TM-008 click-motion correlation. This is instrumentation only: it does not
freeze, damp or otherwise modify cursor motion.
No secret/key material is exposed.
