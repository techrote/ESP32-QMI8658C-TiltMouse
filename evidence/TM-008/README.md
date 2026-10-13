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

## Retained target-loop timing failure

The zero-motion observation prompted inspection of the real application loop.
It captured `now_us` before acquisition, while successful IMU/touch reads stamp
their completion. Dispatch then judged those fresh samples using the earlier
time, so the deliberately strict freshness predicate treated them as future
input and repeatedly invalidated calibration/motion.

Regression-first commit `c558ded0faca1eb6dec811a742e35d2e91a53e62` includes the
actual `main/app_main.c` with bounded fake timer/driver edges and the real
pipeline/orientation/motion/touch logic. [CI 38461938110](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38461938110)
reproduced the defect: 600 loop iterations, 19 publications, zero motion,
300 IMU stale episodes and 18 touch stale episodes. The other 11 host tests
passed. This is deterministic software evidence, not invented sensor evidence.

The correction samples the output clock after acquisition and transport service.
Poll-slot scheduling and the freshness limits stay unchanged. Dispatch and
diagnostics use the new time; freshness checks still reject future timestamps.
Corrected CI and repeated physical evidence remain required. The defect is
TiltMouse-owned; no receiver or wire/authentication change is justified.

## Corrected firmware physical results and tuning candidate

Source `5f07be236680d6232081923615c0433d643c4fdf` passes all four gates in
[CI 38462974058](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38462974058),
including all 12 host tests and linked wired/diagnostic descriptor checks.
Normal USB, diagnostic USB and the private paired ESP-NOW candidates are
archived with [exact hashes](candidate-artifacts.json).

Manual RESET established the earlier diagnostic USB boot (303a:4005 plus CDC).
Its pre-fix physical telemetry reproduced false freshness failures: IMU stale
3557 rising to 4163 and touch stale 384 rising to 447 over ten seconds, with
zero movement and successful USB transfers. This supports the deterministic
actual-main regression; it does not indicate an electrically failed sensor.

The corrected diagnostic image was flashed after a fresh ROM MAC match. All
three written regions passed digest checks, and independent application
readback matched SHA-256
`28395ec4c79d33eede13504cd3be3113f8f0dca3a3ad5f0b42c410133e2399af`.
After operator RESET it enumerated as 303a:4005 with CDC. In the 25 second
`corrected-diag-idle` capture there was no host movement/removal/error.
Telemetry established neutral and touch readiness, zero IMU/touch errors,
zero stale/recovery episodes, zero dropped logical reports, and approximately
104-108 logical reports/s. Recorded maximum loop gap was 5708 us. This is a
bounded diagnostic-firmware idle result, not acceptance of normal USB firmware.

The 60 second `corrected-diag-axes-recenter` capture contained 6389 target-only
Raw Input events, 1736 with movement, and no observer errors or removal.
Movement reached both host axes. Nine button transitions occurred during
handling; this was not a controlled touch or stationary-click trial. Completed
click-window peaks reached (10,12) counts while moving; no artefact or damping
requirement is inferred. Recenter counters remained zero in captured periodic
snapshots, so BOOT recenter is not established by this trial. Raw Input can
coalesce reports; event counts are not a packet-loss measurement.

The operator initially reported Y inverted, then corrected that assessment:
directions are correct when held the intended way with the cable in hand.
The response was difficult to start, then hard to control in motion.
TM-008 tuning candidate 1 preserved original axis signs, reduced gain from
900 to 450 counts/s and changed exponent from 1.6 to 0.9. Source
`b8bac0e0d3aadb99c9a12fc570f9bfba7f07aebf` passed all four required gates in
[CI 38468595024](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38468595024).
Both wired builds and linked HID descriptor checks passed. Diagnostic app
SHA-256 `e1b49551c43b1fc2b1cfeab5f72c325e2fc96ddd2711819d6ed9de2ed7776af2`
passed live target identity, all three flash digests and independent readback.
After reported RESET the downloader remained visible, with GPIO0 read high.
A subsequent operator cold unplug/replug produced healthy diagnostic USB and
CDC. This does not establish the reason the earlier RESET did not start it.

The 15 second `tuning1b-diag-idle` capture had 1556 Raw Input events, 1505 with
movement and six button events. Inputs were ready; stale/error/drop counters
were zero, USB mounted with no resets/failures, maximum loop gap 5648 us.
Despite its planned idle name, it does not establish quiet idle: handling was
not confirmed. The operator reported the new curve far less controllable than
before, comparing it to flying a quadcopter in acro mode while drunk.
**Candidate 1 is rejected.** Restore exponent 1.6 / gain 900 and verify deliberate
BOOT recenter in the actual grip before further response tuning. Original
axis signs remain unchanged. No click-motion freeze or damping is added.
[Candidate 1 hashes/results](tuning-candidate-1.json) retain this failed trial.

Board A remains on its prior accepted config-2 production pair and received
no flash/reset in this session. RF, receiver internal transport and joint
recovery acceptance remain untested. The first normal baseline BIN was
superseded in a working build directory; its exact manifest/hash/logs remain.
Subsequent firmware candidates and application readbacks are separately
archived. No acceptance relies on reconstructing a missing earlier BIN.

Capture digests are in [the private-capture index](capture-index.json).
Both issues and both draft PRs remain open.

## Outstanding acceptance

The [campaign instructions](../../docs/TM-008-ACCEPTANCE.md) and issue #8 remain
the gates: axes/signs, neutral/recenter, full left/right/GPIO4 touch matrix,
calibration/recalibration and faults, normal USB lifecycle, bounded click-motion
correlation, authenticated receiver interoperability, RF/lifecycle recovery,
cadence/loss/latency/soak and evidence-backed final defaults. Untested cases must
remain open; compilation is not physical acceptance.
