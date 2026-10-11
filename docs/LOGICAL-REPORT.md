# TM-005A logical mouse report and transport contract

## Established code

- `components/mouse_report/include/tiltmouse/mouse_report.h` — canonical 4-byte logical report and constructors.
- `components/mouse_report/include/tiltmouse/mouse_transport.h` — bounded, serialized one-owner publisher contract.
- `components/usb_hid_mouse/` — native USB adapter and mouse-only USB identity.

Implementation follows the archived [TM-005A prepass](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/blob/c9d813b0e313b9cd1f7017c596a7ce0404073a3c/checkpoints/prepass/TM-005A/a19aa425b973/unpacked/HANDOFF.md) after review. The archived API sketch was not blindly applied.

## Logical report

`tm_mouse_report_t` carries signed `dx`/`dy`, complete current left/right button bits and an explicit `kind`:

- `CURRENT` is valid only for signed motion values in [-127,+127] and button bits 0/1.
- `RELEASE_ALL` must be canonical zero motion/buttons.
- `INVALID` is zero-initialized/default and must not emit motion.

Constructors validate wide integer inputs before narrowing to int8 and leave failed output invalid. Do not serialize the C layout: the USB adapter emits its defined three-byte `{buttons,x,y}` report; TM-005B will encode faikeow's separate versioned envelope/profile.

The legacy USB-only three-byte construction helper remains available, including its historical int8 domain. The logical contract deliberately rejects -128 because the USB HID descriptor advertises a -127 lower bound.

## Terminal publication and button safety

`tm_mouse_transport_t` is zero-initialized before first use and has exactly one selected owner. An active report is offered to the sink once. The publisher retains no integer movement queue: expiry, backpressure, rejection and later failure cannot refund or replay old movement. `service()` emits only zero-motion state repair. Ordinary valid `publish()` calls still permit periodic zero-delta full-button-state reports.

The publisher retains desired complete button state and sticky release debt. If a newly requested press conflicts with owed release, a zero-motion release barrier must first complete before the new pressed state is emitted. The three result axes (`motion`, `state`, `reason`) distinguish accepted movement from retained buttons; state retention is not delivery evidence.

Activation/switching fences the prior route, uses monotonically advancing local tickets and starts with an all-buttons-up obligation on the new route. Wrong-owner publications neither mutate state nor submit anything. Callers must not offer a dropped motion candidate to an alternate sink as a fallback.

The backend copies its report during successful `submit()` and must not retain its pointer. `complete()` is a transport endpoint signal, not proof of host application delivery. Matching tickets and release/state revisions prevent old completion signals from clearing newer release obligations.

## USB adapter

Boot still initializes only board-safe state and native TinyUSB mouse-only transport. TM-005A adds no sensor sampling, Wi-Fi, ESP-NOW, BLE or LED features. `tiltmouse_usb_hid_mouse_publish()` and `tiltmouse_usb_hid_mouse_service()` provide the TM-006 application integration seam.

TinyUSB callbacks set a bounded atomic outcome. Only the serialized application-side publish/service calls consume it and mutate publisher state; the application must not invoke publisher methods from an asynchronous callback.

The historical descriptor accidentally described button padding without emitting the two data button bits. The corrected production descriptor has 24 bits: two button data bits, six padding bits and relative signed X/Y. The host test parses that exact production descriptor.

## Evidence and future work

Host tests exercise report validation, USB conversion, busy/rejected/expired movement drops, invalid-input neutralization, owed release-before-repress, stale completion fencing, single-owner routing and the descriptor layout. The firmware CI target build verifies compatibility with the pinned ESP-IDF/TinyUSB stack; neither proves enumeration on the actual board.

TM-006 owns independent input freshness, neutral calibration, scheduling and routing. TM-007 owns fully hardened USB lifecycle, concurrent callback/event fencing and reconnect/suspend behavior. TM-008 owns physical USB, IMU/touch, cursor feel and cross-repository RF acceptance.

Future ESP-NOW send success is only a MAC-level signal, not an acknowledgement that the receiver applied the event. Repeating full current button state cannot reconstruct a release wholly lost between a press and re-press; stronger guarantees require the eventual accepted receiver contract or fail-closed timeout/rearm handling.
