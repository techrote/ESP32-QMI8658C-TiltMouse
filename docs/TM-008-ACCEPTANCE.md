# TM-008 physical acceptance campaign

TM-008 / issue #8 and receiver FNR-008 / issue #9 share session
`joint-TM008-FNR008-2026-10-13`. Each issue retains its own acceptance decision.
Both remain open until their physical criteria and final CI are established.

## Wired telemetry build

Normal firmware exposes only the HID mouse. On the Matrix board its native USB
pins cannot simultaneously expose the hardware USB Serial/JTAG console. The
qualification build adds CDC to the same USB device so the existing TM-007
application telemetry is observable through the board's cable:

```sh
idf.py -B build-usb-diag -D IDF_TARGET=esp32s3 -D SDKCONFIG=sdkconfig.usb-diag \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.usb-diagnostics.defaults' build
```

Run from the pinned ESP-IDF v6.1 environment with ESP32-S3 selected. Diagnostic
USB has three interfaces (HID plus the two CDC interfaces). HID retains its
original report descriptor, interface 0, IN endpoint 0x81 and 8 ms poll interval.
CDC uses separate endpoints and redirects existing application logs through
the pinned nonblocking, fixed-FIFO VFS. There are no diagnostic application
commands, synthetic movement, sensor overrides, or changed motion semantics.
Unconsumed serial output can be truncated; serial logs are not lossless evidence.

CI builds both wired variants and checks their actual linked descriptor bytes:

```sh
python tools/check_usb_descriptors.py \
  --usb build/esp32_qmi8658c_tiltmouse.elf \
  --diagnostic build-usb-diag/esp32_qmi8658c_tiltmouse.elf
```

Normal mouse-only firmware must also be tested physically. Diagnostic USB
enumeration or timing cannot stand in for acceptance of the normal identity.
Wireless instrumentation uses the selected ESP-NOW build's console; no direct
USB mouse is initialized in that mode.

## Session evidence

See [the TM-008 session record](../evidence/TM-008/README.md). Receiver-owned
UART/core/USB observations and the shared RF matrix belong in
[`faikeow-now-reciever/evidence/FNR-008/`](https://github.com/techrote/faikeow-now-reciever/tree/codex/fnr-008-physical-acceptance/evidence/FNR-008).
Source commits, actual build modes and binary hashes must accompany every
result. Repeated USB transfers and ESP-NOW callbacks do not alone establish
host movement or receiver application acceptance.
