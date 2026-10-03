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

TiltMouse owns:

- transport-neutral logical mouse semantics;
- ESP32-S3 ESP-NOW TX;
- mapping TiltMouse logical reports into the generic faikeow platform envelope + `relative_mouse` profile;
- transmitter-side scheduling/fault handling;
- sender compatibility vectors/tests.

`techrote/faikeow-now-reciever` owns:

- the generic ESP-NOW application envelope and platform versioning;
- HID profile IDs and the `relative_mouse` profile contract;
- ESP-NOW RX;
- generic peer/session/order/freshness behavior;
- clone-board ESP8266/ESP8285↔RP2040 transport;
- RP2040 profile dispatch and USB HID implementation;
- receiver provisioning/recovery and physical acceptance.

TiltMouse must not invent a private receiver wire format. TM-005B consumes the accepted faikeow contract. TM-008 records the exact faikeow commit/artifacts used for end-to-end acceptance.

## Protocol requirements

faikeow FNR-006 / issue #7 freezes the generic platform envelope and `relative_mouse` profile byte contracts. TM-005B / #16 must encode those contracts exactly and add no incompatible private framing.

### Compatibility

Keep every application packet within **250 bytes**. This preserves ESP-NOW v1 interoperability with ESP8266-class receivers even when the ESP32-S3 stack supports newer ESP-NOW framing.

### Logical contents

The ESP-NOW application payload is the faikeow generic envelope plus the `relative_mouse` profile payload.

The generic envelope carries routing/order metadata such as platform version, profile ID, session/restart discriminator, sequence, flags/type and payload length.

The `relative_mouse` profile carries bounded relative X/Y and complete current left/right button state.

Transport/profile metadata remains outside TiltMouse's core logical mouse-report type.

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

faikeow owns receiver-side session/order/freshness semantics. TM-005B must generate envelope session/sequence fields consistent with the accepted faikeow contract and provide compatibility vectors.

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

TM-008, coordinated with faikeow FNR-008 / issue #9, must establish the complete physical chain:

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

## Receiver platform references

- faikeow repository: https://github.com/techrote/faikeow-now-reciever
- generic protocol authority: https://github.com/techrote/faikeow-now-reciever/blob/main/docs/02-PROTOCOLS.md
- HID profile authority: https://github.com/techrote/faikeow-now-reciever/blob/main/docs/08-HID-PROFILES.md
- generic receiver/profile implementation: https://github.com/techrote/faikeow-now-reciever/issues/7
