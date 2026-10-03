# Initial plan review

## What was retained

- ESP-IDF + native TinyUSB HID is the appropriate wired base.
- QMI8658C accel+gyro should be fused rather than using gyro integration alone.
- Tilt should control relative velocity rather than absolute position.
- GPIO1–3 / GPIO5–7 grouping is appropriate for the no-solder experiment.
- GPIO4 should remain non-clicking.
- LEDs should stay out of initial scope.
- startup calibration, hysteresis, debounce and runtime re-centre are important.

## Gaps found in the initial plan

### Too much hardware coupling

The revised architecture requires pure host-testable estimator, motion and touch logic instead of hiding it inside ESP-IDF callbacks.

### Framework/API drift was not bounded

TM-001 pinned an exact ESP-IDF/toolchain/dependency baseline.

### Physical acceptance was mixed with compilation success

USB enumeration, RF behavior and touch ergonomics require physical evidence. CI proves software contracts, not real-device behavior.

### Touch grouping needed a clearer electrical rule

GPIO1–3 and GPIO5–7 remain independent electrodes and are fused only in software.

### QMI address assumptions were unnecessary

The driver verifies `WHO_AM_I=0x05` and probes both documented addresses.

### Click-induced cursor motion deserved first-class treatment

This remains a measured TM-008 acceptance item.

### GPIO4 reference use was too speculative

GPIO4 stays non-clicking and diagnostic/reference-only unless evidence justifies compensation.

## ESP-NOW pivot after TM-004

After TM-002 through TM-005 were completed, the available hardware changed the practical transport plan.

The spare "Pico W" boards intended for other experiments were identified as clones containing a genuine RP2040 but ESP8266-class Wi-Fi hardware instead of CYW43439. They cannot provide the intended Bluetooth controller function, but they remain useful as ESP-NOW receiver hardware.

At the same time, TiltMouse had not yet reached TM-006 application integration. This made it possible to add wireless support without rewriting the completed IMU/touch/motion layers.

### Decision

v0.1 now supports:

- direct native USB HID as the default wired path;
- ESP-NOW as an alternate wireless path through a companion receiver.

Bluetooth LE HID/HOGP is deferred beyond v0.1.

### Architectural consequences

1. Insert TM-005A / #15 before integration to define a transport-neutral logical mouse report.
2. Insert TM-005B / #16 to implement ESP-NOW TX and the receiver wire contract.
3. Keep sensor/touch/motion algorithms transport-agnostic.
4. Keep ESP-NOW application packets <=250 bytes for ESP8266 interoperability.
5. Repeat full current button state in each wireless report.
6. Prefer newest cursor movement over retransmitting stale movement.
7. Require receiver link timeout to release all buttons.
8. Use Wi-Fi only as required for ESP-NOW; do not add IP networking.
9. Do not carry BLE pairing/HOGP/coexistence complexity in v0.1.
10. Keep the companion RP2040+ESP8266 receiver as a separate project boundary.

## Result

The revised programme is now:

```text
sensor/input feature tracks
        |
TM-005A logical report seam
        |
TM-005B ESP-NOW transport
        |
TM-006 dual-transport integration
        |
TM-007 hardening
        |
TM-008 physical USB + wireless acceptance
        |
TM-009 v0.1 release
```

This preserves the completed work while adding wireless support at the cleanest remaining architectural boundary.
