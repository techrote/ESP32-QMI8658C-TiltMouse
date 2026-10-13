# TM-008 physical acceptance evidence

Session: `joint-TM008-FNR008-2026-10-13`, started 13 October 2026, Europe/London.
Status: **IN PROGRESS; NOT ACCEPTED**. Issue #8 remains open.

## Reconciled authority and baseline

- TiltMouse main: `2be851985fefb55bc0df3063b66896c4ec900092`.
- Prerequisites merged: TM-005A PR #20, TM-005B PR #23, TM-006 PR #22,
  TM-007 PR #24. Owning issue #8 had no comments at intake.
- Exact-main [CI 38442701195](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38442701195)
  passed host tests, wired build, public-fixture wireless build, and style.
- Receiver main: `a49d95e61dc910c7db139db0280f80a31d10bfc8`; FNR-005/006/007
  accepted. Exact-main CI 38322804818 passed all four receiver gates.
- Platform/internal protocol v1, relative_mouse ID/schema 1, FNA1 wrapper v1,
  250 ms receiver freshness. Full-state buttons; no movement retransmission.

## Hardware and first wired artifact

Operator identified the connected ESP32-S3 as TiltMouse and requested flashing.
Live ROM identity: ESP32-S3 QFN56 revision v0.2, embedded 4 MiB XMC flash,
2 MiB PSRAM, 40 MHz crystal. Flash JEDEC manufacturer/device: 20 / 4016.
Two complete original 4 MiB reads match and remain private for recovery.

Host: native Windows 11 Pro, 64-bit, version 10.0.26300. Board A is the sole
receiver DUT; operator confirmed it remains on its previous USB port. Historical
board names or COM numbers alone are not target identity.

First wired build was clean baseline source above, ESP-IDF v6.1 (6.1.0), IDF
commit `fff9895c82d744c7237be8847347bdd1b07c6643`, Xtensa esp-15.2.0_20251204,
esp_tinyusb 2.3.0 and tinyusb 0.21.0~1. Supplemental native Windows build:
`idf.py -B build/usb set-target esp32s3`, then `idf.py -B build/usb build`.
Default 2 MiB/DIO/80 MHz flash configuration fits the observed 4 MiB device.
Selected output: USB. Application BIN SHA-256:
`9228f2ade4b33da021d83462615d5bd9c26ce8a65baff02b2a37b7b55be63f29`.
Private manifest separately records bootloader/partition/ELF/config hashes.
esptool 4.8.1 verified all three written regions by digest; this is separate
from observing successful application boot. No real deployment key is linked
into this wired artifact.

## Observations so far

| Case | Observation | Scope |
| --- | --- | --- |
| Normal USB enumeration | 303a:4004; Windows mouse and USB input children healthy | PASS: physical enumeration only |
| Parsed HID caps | Usage page 1 / usage 2; two button bits, two axes; 4-byte Windows input buffer including report-ID prefix; no output/feature reports | PASS: parsed descriptor subset |
| Initial 20 s idle | Zero motion, no removal, no observer error | PASS: bounded host idle subset; no neutral-report claim |
| GET_REPORT | Windows error 1, no input report returned | LIMITATION: cannot establish neutral through this probe |
| LED after warm flash | Operator observed the old firmware's last frame retained | Retained observation; not evidence of new LED writes |
| Cold unplug/replug | Operator reported no LEDs lit after reconnection | Supports retained LED-latch explanation; electrical data-pin activity not measured |
| 60 s active-axis capture | 230 zero-motion Raw Input events, no button transitions or removal | Motion not established; correlate operator timing and diagnose with telemetry |

Raw Input captures filter the TiltMouse device only; unrelated host input is
neither suppressed nor saved. The accepted receiver host observer was reused
unchanged; its private HID probe selector alone was changed to 303a:4004.
Raw captures, original flash, full device identities, deployment keys and
generated source remain outside Git. No RF result or final usability tuning is
claimed by this record. No click-motion freeze/damping has been added.

## Outstanding acceptance

The [campaign instructions](../../docs/TM-008-ACCEPTANCE.md) and issue #8 remain
the gates: axes/signs, neutral/recenter, full left/right/GPIO4 touch matrix,
calibration/recalibration and faults, normal USB lifecycle, bounded click-motion
correlation, authenticated receiver interoperability, RF/lifecycle recovery,
cadence/loss/latency/soak and evidence-backed final defaults. Untested cases must
remain open; compilation is not physical acceptance.
