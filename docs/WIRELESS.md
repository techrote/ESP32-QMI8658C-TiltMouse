# Wireless transport — ESP-NOW pivot

## Decision

TiltMouse v0.1 keeps **direct USB HID** as its default wired transport and adds **ESP-NOW** as an alternate wireless transport.

Bluetooth LE HID/HOGP is not removed from the architecture permanently, but it is deferred beyond v0.1.

The pivot was made before TM-006 application integration, so completed IMU, touch and motion logic remains unchanged.

## Why ESP-NOW

The available spare receiver boards are clone Pico-W-style boards containing:

- genuine RP2040;
- ESP8266-class Wi-Fi hardware instead of CYW43439.

They therefore cannot provide the Bluetooth-controller functionality expected from a genuine Pico W, but the ESP8266 can still participate in ESP-NOW. The RP2040 can then present standard USB HID to the PC.

The intended path is:

```text
Waveshare ESP32-S3-Matrix
  IMU + touch
      |
 orientation / motion
      |
 logical mouse report
      |
   ESP-NOW
      v
ESP8266 on clone Pico-W board
      |
 board-internal link
      v
RP2040
      |
 USB HID
      v
PC
```

The PC does not need Bluetooth, ESP-NOW support or a custom driver. It sees only the RP2040 USB HID mouse.

## Repository boundary

This repository owns:

- transport-neutral logical mouse semantics;
- ESP32-S3 ESP-NOW TX;
- packet version/format;
- ordering/state semantics;
- transmitter-side fault handling;
- interoperability vectors/tests.

The companion receiver project owns:

- identification of the clone board's ESP8266↔RP2040 link;
- ESP8266 flashing/firmware;
- ESP-NOW RX;
- receiver ordering/timeout behavior;
- RP2040-side USB HID firmware;
- receiver physical acceptance.

TM-008 links the exact receiver implementation used for end-to-end acceptance.

## Protocol requirements

TM-005B / #16 freezes the exact byte layout. The protocol must satisfy these invariants.

### Compatibility

Keep every application packet within **250 bytes**. This preserves ESP-NOW v1 interoperability with ESP8266-class receivers even when the ESP32-S3 stack supports newer ESP-NOW framing.

### Logical contents

A wireless packet carries at least:

- protocol version;
- monotonically advancing sequence number;
- bounded relative X;
- bounded relative Y;
- complete current left/right button state;
- any flags required for safe evolution.

Transport metadata remains outside the core logical mouse-report type.

### Latest-first movement

Mouse motion is time-sensitive. A missing movement packet should not cause old cursor deltas to be replayed later.

Therefore:

- do not create an unbounded retransmission queue;
- do not request/fill old motion purely to make sequence numbers contiguous;
- deliver the newest valid movement;
- use sequence gaps primarily for diagnostics/order rejection.

### Complete button state

Buttons are state, not edge-only events.

Every report repeats the full current left/right state. This means a later successful packet repairs a lost press/release packet.

If the receiver loses the transmitter for longer than the configured timeout, it must emit an all-buttons-released USB state.

### Ordering

The receiver rejects duplicate/stale/out-of-order reports according to a wrap-safe sequence rule defined and tested by TM-005B.

### RF/peer configuration

Normal operation should prefer unicast. RF channel and peer identity must be explicit.

Encryption should be supported where practical with the pinned stack, but key/provisioning behavior must be explicit; never embed one project-wide secret as the only production path.

ESP-NOW mode does not require infrastructure Wi-Fi or IP networking.

## Transport selection

v0.1 requires:

- USB as the default mode;
- ESP-NOW as an alternate mode;
- exactly one normal report-emission owner at a time.

Automatic switching based on cable state is optional. If introduced, it must be deterministic and must not duplicate or replay reports across transitions.

## Failure semantics

Wireless behavior must fail safely:

- no stale movement backlog;
- duplicate/stale packets ignored;
- sequence gaps observable;
- receiver link timeout releases all buttons;
- link recovery starts from fresh state;
- transport changes clear or otherwise safely reconcile pending state.

## Power and coexistence

ESP-NOW uses the ESP32-S3 Wi-Fi radio. Power-saving may be considered later if it does not compromise pointer latency/reliability.

BLE coexistence is out of v0.1 scope because BLE/HOGP itself is deferred.

## Physical evidence

CI can validate packet encoding, ordering logic and state machines. It cannot validate RF performance or the clone receiver.

TM-008 must establish the complete physical chain:

```text
TiltMouse -> ESP-NOW RF -> ESP8266 receiver -> RP2040 -> USB HID -> host
```

Evidence must identify exact firmware commits/artifacts on both sides.

## References

- ESP-IDF ESP-NOW API:
  https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_now.html
- Espressif ESP-NOW interoperability FAQ:
  https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html
- ESP-NOW SDK introduction, including ESP8266 family support:
  https://docs.espressif.com/projects/esp-now/en/latest/esp32/introduction.html
