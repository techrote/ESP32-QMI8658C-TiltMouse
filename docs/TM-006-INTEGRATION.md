# TM-006 — Integrated tilt and touch application

## Dependencies and scope

TM-006 is stacked onto the TM-005B sender implementation. Merge the
TM-005B PR for issue 16 and verify exact main before accepting this PR.
No firmware-to-firmware physical interchange is claimed.

The application has one explicit startup transport: native USB HID
(default) or authenticated ESP-NOW. A selected radio that cannot initialize
does not silently fall back to wired HID. The firmware never runs both
outputs normally and does not perform automatic switching.

## Input and safety

- Single serialized ESP-IDF application loop: 4-ms timer, nominal 4-ms
  QMI8658 polls, 8-ms touch and report slots. Missed slots are dropped.
- Host-testable IMU orientation, tilt velocity mapping, seven independent
  touch channels, and logical current/release reports.
- TM-008 target adaptation first learns a bounded stationary gyro mean
  (at least one second/128 samples), frozen during use and discarded
  after a sensor error or stale gap. See `ORIENTATION-MOTION.md`.
- Startup neutral capture only after 32 quiet valid sensor samples. No
  relative motion before neutral, after stale IMU, or on an excessive
  scheduling gap. Provisional limits: 100-ms input freshness, 50-ms
  maximum report gap, 8-dps gyro stillness limit.
- BOOT/GPIO0 must be seen released after startup. A deliberate
  600–4000-ms held-then-released gesture re-centres orientation and
  resets fractional X/Y motion without generating a click or dropping
  independently fresh held buttons.
- Touch acquisition/calibration failures trigger a sticky safety release
  once; no stale pressed state is replayed. Failed IMU only suppresses
  movement while fresh touch may still update buttons.

## Wireless compatibility

The radio consumes the same validated TM-005A logical reports through
TM-005B and emits exact FNR-006 platform-v1 relative_mouse-v1 bytes.
FNR-007 requires the authenticated FNA1 44-byte payload, encrypted peer
and per-device key. Actual sender station MAC, receiver station MAC,
public config identity/generation, RF channel and 16-byte LMK must match
the provisioned receiver pair.

Normal USB builds never initialize Wi-Fi; wireless builds require an
ignored private header main/tiltmouse_sender_config.local.h defining
TM_ESPNOW_LOCAL_CONFIG as a tm_espnow_mouse_config_t initializer. The
header MUST NOT be committed. No universal key, plaintext fallback,
infrastructure Wi-Fi/IP, Bluetooth or RGB feature is introduced.

## Acceptance boundary

Deterministic host tests validate neutral, touch fusion, deliberate
re-centre, freshness, button and transport ownership. TM-005B host tests
validate faikeow golden vectors, authenticated HMAC and sequence wrap.
The ESP32-S3 CI build validates SDK compilation. Physical USB, RF link,
click-induced movement and complete reference receiver acceptance remain
TM-007/TM-008 gates, not claims of this PR.

Receiver FNR-006 frozen at 136ef3b612f83fd9cad74a5dd50c2e5f629b44de.
FNR-007 provisioning/wrapper at a49d95e61dc910c7db139db0280f80a31d10bfc8.

## Dual-mode CI

The normal firmware CI job cross-builds the default USB-only application.
An independent wireless-mode job compiles the real ESP-NOW selected application
against the same pinned ESP-IDF with the FNR-007 PUBLIC interoperability key
and synthetic test MACs in a transient gitignored local header. Those values
are intentionally not production credentials. Both paths must compile; neither
requires physical hardware or claims real pairing.
