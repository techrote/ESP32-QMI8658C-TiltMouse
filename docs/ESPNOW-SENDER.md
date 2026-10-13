# TM-005B — faikeow ESP-NOW sender

## Sender wire compatibility

- Receiver authority: techrote/faikeow-now-reciever FNR-006,
  commit 136ef3b612f83fd9cad74a5dd50c2e5f629b44de.
- Provisioned radio authority: FNR-007, commit
  a49d95e61dc910c7db139db0280f80a31d10bfc8.
- Platform envelope v1: 18-byte little-endian header, profile ID 1,
  PROFILE_DATA = 1, flags = 0, fresh nonzero u64 session, u32 sequence.
- Relative-mouse v1: exact four bytes, schema 1, complete left/right
  button mask, signed relative X/Y in [-127,+127]. -128 is invalid.
- Radio authenticated wrapper v1: FNA1 + length LE16 + platform packet
  + first 16 bytes of HMAC-SHA256(KA, M). Complete radio packet is
  exactly 44 bytes. No unwrapped production path.
- KA = HMAC-SHA256(K, ASCII FNR-radio-auth-key-v1).
  M = ASCII FNR-radio-auth-v1 + config_id LE64 + generation LE32
  + actual sender station MAC + provisioned receiver station MAC
  + six wrapper-header bytes + complete 22-byte platform packet.
- Sender sequence starts at zero on a fresh nonzero hardware-RNG
  session and advances modulo u32 for every encoded attempt, even
  when lower-layer transmission is subsequently refused.
- Repeated movement is never queued or retransmitted after loss.
  Button state is full-state; TM-005A's sticky release debt is
  serviced using new zero-motion messages, not stale deltas.

## Security boundary and provisioning

Production wireless mode requires a **per-device 16-byte key**, exact
sender and receiver station MACs, channel 1..14, nonzero public config ID and
generation. The sender must compare its actual station MAC to the public
FNR-007 provisioned sender MAC and fail closed on mismatch. The app supplies
these credentials only from a local, ignored per-device configuration header. Supply these at application initialization from an
operator-specific source that matches the FNR-007 paired receiver
configuration. No example key, secret value or default universal key
is linked into TiltMouse.

The sender config API is defined in espnow_mouse.h. Factory/unprovisioned
application builds must not call init and must remain wired-only.
Configuration validation rejects all-zero keys, invalid unicast MACs
and absent public identities. The ESP32-S3 station MAC is taken from
its Wi-Fi driver after startup and becomes part of the HMAC.

Only the explicitly selected wireless transport initializes Wi-Fi
in STA mode and ESP-NOW. No infrastructure association, DHCP, sockets,
AP service or BLE are started. A paired unicast peer is configured
with the same LMK as the receiver; a distinct local PMK is derived.
The application-level HMAC is mandatory even with CCMP link encryption:
encrypted peer registration alone did not prove each packet encrypted
or authenticated on the receiver's pinned SDK.

HMAC implementation uses ESP-IDF v6.1's Mbed TLS 4 PSA Crypto HMAC-SHA256 provider.
No key or digest is printed. Temporary derived material is wiped.

## Lifecycle and evidence

An ESP-NOW send callback only records a bounded atomic status; the
single foreground owner drains it and completes the TM-005A state
machine. MAC-level success does not prove receiver application
delivery or perfect short button-release reconstruction.

TM-007 adds transmitter-local recovery without altering FNR bytes:
immediate esp_now_send refusal and MAC callback failure are counted,
and three consecutive failures schedule Wi-Fi/ESP-NOW reinitialization
after a 50 ms backoff. Failed recovery doubles that delay to a maximum
of 1 s. Each service iteration performs at most one due recovery attempt.
The already-created FNR sender object is retained, so a local radio reset
does not fabricate a new application session or rewind its u32 sequence.
On recovery the local mouse transport is re-activated behind a zero-motion
all-buttons-up barrier. Movement that failed or was in flight is never
retransmitted.

The public status snapshot exposes session/next sequence, send/API/callback
failure counters and recovery state but no key/digest material. Receiver
timeout/ordering/neutralization remains faikeow-owned; physical recovery
and interoperability remain TM-008.

The native OpenSSL host test checks FNR-006 envelope golden values,
FNR-007's published public HMAC vector, an independently calculated
full 44-byte mouse vector, validation/bounds, neutral state, sequence
wrap and session rejection. ESP32-S3 build validates the real Wi-Fi
adapter against pinned ESP-IDF 6.1. Neither establishes physical
ESP-NOW transmission or receiver interoperability; that is TM-008.
