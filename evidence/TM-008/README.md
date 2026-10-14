# TM-008 physical acceptance evidence

Session: `joint-TM008-FNR008-2026-10-13`, started 13 October 2026, Europe/London.
Status: **IN PROGRESS; NOT ACCEPTED**. Issue #8 remains open.

## Latest checkpoint: next touch/indicator build ready, unflashed; operator session stopped

On 14 October 2026 the operator requested an ESPsand touch investigation review,
another TiltMouse test build, then a stop while they sleep. Physical operations
have stopped. No new candidate is flashed; Board A remains unchanged.

[Reference review and next-session procedure](../../docs/TM-008-NEXT-TEST.md)
pins ESPsand `fd509bfdd3e1a84e9cf097635d3f25a98a345759`. Its tested bare-board
row showed broad fingertip coupling and unreliable separate product zones.
That finding motivates measurement here; it establishes no TiltMouse pass/fail
and does not justify importing common-mode cancellation or new thresholds.

The [next candidate manifest](next-touch-candidate.json) identifies clean source
`600df4366bc93fb3a00c64d9855e8c906d1d0113`, all three local builds, configuration
hashes, toolchain, linked-code checks and private archived artifact hashes.
Diagnostic app SHA-256:
`69918e8fc3cad9cc1dfc865ec5f646d71100963756705bbe61add259a060e984`.
All four gates and 17 host tests pass in
[source CI 38489855242](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38489855242).
Local normal USB, HID+CDC diagnostic USB and real-pair ESP-NOW builds pass.
Linked mouse report/interface/endpoint bytes remain equal across USB variants.
Both wired images exclude real/derived pairing keys and radio/BLE initialization
symbols; only the diagnostic ELF contains the opt-in indicator adapter.
Private keys, RF binaries and raw build/CI logs remain outside Git.

The candidate maps GPIO5..7 to left, GPIO1..3 to right at logical report creation
for both transports, retaining canonical wire bits and GPIO4 exclusion. Touch
thresholds/debounce and the physically tried IMU/frame/gyro/X/motion defaults
are unchanged. One-Hz raw/baseline/noise/evidence and full logical button-state
snapshots support the next controlled touch trials. The owner-requested very
dim diagnostic dot uses GPIO14, RGB row-major reference wiring, one channel at
code 1 on one pixel, bounded changed-frame DMA output and no blocking waits.
Kconfig defaults off; normal USB/RF keep LEDs off. Actual colour/orientation,
visibility, loop overhead and touch behaviour remain physically untested.
No click-motion freeze/damping is introduced.

The last installed sender remains X-corrected source `e8cfa99`, diagnostic app
`a63f5c606c3ecebab9ca8ad56c86b3c5d29efd3a52486a82c6e7009e8f8d3abd`.
Earlier physical results below apply to their recorded builds, not to this
unflashed candidate. Both TM-008 and FNR-008 remain open and unaccepted. Resume
with the operator-present procedure, then the remaining USB and joint RF matrix.

## Last physical checkpoint: calibrated neutral, X-corrected control and BOOT recenter

Earlier calibrated source/build `3a33f41b0e2f5016b425bf28a955dfd777912bf8`, diagnostic app
`f10c82308eea69cd9a5ae8137ac863dcfb344385bf91b0839a260c2e7bdde7b0`, passed live
ROM identity, three written-region checks and independent application readback.
The operator cold-booted and left the board flat, LED face up and untouched.
The 30-second capture at 23:10:50 UTC (14 October local time) established
neutral=1, quiet=32/32, touch ready, zero host movement/button events, no USB
removal, serial/observer error or sensor/touch fault, and zero movement drops.
Gyro qualification used 166 samples, rejected 54 earlier candidate windows,
and completed 5.145 seconds after boot. Offset (-2018,10029,-826) mdps stayed
frozen. A two-second startup is not established. Maximum local loop gap was
11337 us. Three pose snapshots differed from neutral by at most 0.288 degrees;
this is not a continuous physical angle bound.

The first 60-second tilt window mostly captured a flat board and is not an
axis/recenter result. A separate 300-second-maximum recording, stopped after
operator completion at 121.156 seconds, captured 11718 selected HID events,
6222 with movement and 22 with button flags. USB and serial remained stable,
gyro offset remained frozen, no sensor faults occurred, and recenter stayed 0.
Logical movement drops rose to 108; maximum local loop gap was 12114 us.
These diagnostics are retained rather than treated as zero-loss evidence.

Operator report with LED face up and USB toward the operator: "Feels good,
x axis swapped". The narrow correction in this branch inverts default cursor
X only, preserving Y, the original 1.6/900 response and 3-degree deadzone.
The default-response regression retains the magnitude table and checks both
corrected X signs with unchanged Y. Source/build
`e8cfa99c3780df1296eb0fe76678224f67a33436` passes all 14 tests and all four gates
in [CI 38483762002](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38483762002).
Local USB/diagnostic/real-pair ESP-NOW builds and linked HID checks pass.
[Exact X-correction artifacts](x-sign-candidate.json) were flashed at
23:28:18 UTC with live identity, all region checks and independent readback,
then operator cold-booted. Diagnostic app is
`a63f5c606c3ecebab9ca8ad56c86b3c5d29efd3a52486a82c6e7009e8f8d3abd`.
The [repeated 30-second stationary capture](x-sign-stationary-result.json) has
neutral=1, quiet=32/32, zero host motion/faults/removal and frozen gyro offset
(-2499,11059,-838) mdps. Qualification completed 8.637 seconds after boot
after 73 rejected candidate windows. Maximum local loop gap is 11127 us;
three pose snapshots differ from neutral by at most 0.252 degrees. No
two-second startup or full button-neutral state is inferred from this record.
The [closed 300-second operator capture](x-sign-operator-result.json) confirms
recenter 0 -> 1 and a changed neutral pose after the reported two-second BOOT
gesture. Gyro offsets stayed frozen and USB/sensors remained fault-free.
The operator reports control "Seems pretty good" and LEDs dark. It records
9676 moving host events and logical movement drops 0 -> 235, with local loop
maximum 12020 us. Sampled diagnostic logical cadence is about 100 reports/s;
this is not RF loss or host-pointer latency. Mixed button events remain
unassigned; stationary-click and individual electrode acceptance are open.

On 14 October local time, the operator explicitly chose to swap click locations
and requested a very dim sensor representation in the next build. Next trial:
GPIO5-7 left / GPIO1-3 right, sensitivity unchanged; diagnostic touch readouts
and a dim tilt indicator. This explicit diagnostic request updates the earlier
dark-matrix diagnostic constraint. Normal release/radio LED defaults remain off.
The new layout/indicator require implementation, CI and physical qualification.

[Derived capture results](gyro-bias-physical-results.json) identify exact source,
artifact, timing and limits. [Mixed click windows](gyro-bias-mixed-click-windows.csv)
retain transition timestamps/peaks but are not controlled stationary-click or
touch acceptance. BOOT recenter, stopping-flat, heat and LED behavior were not
confirmed by the latest operator reply. No click freeze/damping is added.
Implementation 3a33f41 passes all 14 tests and four gates in CI 38480200839;
evidence head 228ac4b passes CI 38481644096. Receiver head 4078604 passes
CI 38481643674. Board A remains unchanged on config 2 and RF is untested.

Following sections retain earlier chronological checkpoints; this checkpoint
takes precedence. Both issues remain open.

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


## Baseline restored; control trials stopped for diagnosis

The archived 5f07be2 diagnostic image was restored after fresh ROM identity,
three written-region digests and independent application readback matched.
Operator cold unplug/replug established healthy 303a:4005 HID plus CDC again.
The original exponent 1.6 / gain 900 and original axis directions are running.

The 12 second `restored-baseline-startup` capture lost serial with Windows
ClearCommError error 22. The operator reported it disconnected by itself and
no unusual heat. Its 913 retained serial bytes show neutral not ready, touch
ready, no input errors and successful USB transfers before interruption. The
original wrapper failed to save the Raw Input result on this exception, so
whole-device removal versus CDC-only failure is not established by that run.
Movement/recenter/RF experiments were stopped; no reset or reflash followed.

The separate 30 second `restored-baseline-usb-passive` diagnostic observation
retained both channels even if serial were to fail. It had no serial/observer
errors, no HID removal or target motion, and unchanged serial-port presence.
Device uptime advanced from 170994 to 191004 ms in retained snapshots; USB
attach=1, detach/suspend/resume/fail/reset=0 throughout. This bounded interval
does not explain or erase the earlier interruption.

The same snapshots expose a distinct input problem: neutral=0, IMU stale=1,
recovery=1, driver reinitialization=1/1, maximum loop gap 279667 us, collapsed
slots=68 and dropped logical movement=208. No additional increments occurred
within the captured snapshots. Recovery's existing 200 ms plus 80 ms delays
are consistent with the large loop gap; the initiating no-data event and
failed subsequent quiet calibration are not yet attributed. No threshold,
workload, axis, curve or recovery timing is changed to force a result.

A new optional diagnostic-only observer retains the very same successful IMU
sample already used by the pipeline and logs raw/scaled values, quiet progress,
pose/neutral and BOOT state at the existing ten-second cadence. It adds no I2C
reads or application commands. Qualify and deploy it through the confirmed
cold ROM path, then diagnose stationary samples before resuming control tests.
The register-address bit-7 check agrees with section 12.2 of the existing
QMI8658C rev. 0.9 reference; no adapter addressing change is justified.

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

## Confirmed face-up frame and stationary gyro observations

Observer source `62c17de5aaf3e41baf6384ff6cf19bd5b84071ae` passed all four gates
in [CI 38472932806](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38472932806).
App SHA-256 `167fb83c9ce1182a1eff8e07f06f38c7cf2f0d070adaf2a1b2df4af4943b043a`
passed live ROM identity, three flash-region digests and independent readback,
then cold-booted as healthy diagnostic HID plus CDC. Original motion settings
were retained. Exact artifacts are in [observer hashes](imu-observer-candidate.json).

The operator explicitly confirmed flat, LED face up, still and untouched during
the 30-second `imu-observer-stationary` capture. It had no serial/observer errors,
HID removal or target movement. The [three periodic native snapshots](faceup-stationary-snapshots.csv)
show acceleration approximately (44,-5,-1001) mg and native gyro Y between
8781 and 9437 mdps. Quiet progress stayed 0/32 against the unchanged 8 dps
startup limit. Roll stayed near -85 degrees rather than a flat pose near zero.
These are ten-second snapshots, not a full-rate trace or precise noise estimate.

This establishes an inappropriate native-to-estimator frame for the confirmed
face-up pose, and an observed stationary gyro offset that blocks startup quiet
calibration. The raw samples explain why changing the cursor curve alone could
not qualify usability. No gyro-bias compensation or threshold relaxation has
been applied. A dense fixed-capacity mean/spread window is needed before
selecting a bias correction.

Regression-first source `617dd2f0d642db0329057daf69e31f2a82bdeafc` supplies the
confirmed negative native Z mounting to the actual target main loop, with zero
gyro to isolate the frame defect. [CI 38474100635](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38474100635)
fails the flat-neutral assertion while the other 11 tests pass: 286 publications,
216 movement reports and no false freshness episodes. Small native Y changes
across the -Z branch can therefore create large false roll transitions.

The focused correction rotates both acceleration and gyro by 180 degrees
about native X before the portable estimator: (X,Y,Z) -> (X,-Y,-Z). Driver bytes
and native sample contract stay unchanged. Tests cover flat neutral in actual
main, matching accel/gyro signs, proper rotation and quiet output under small
native Y noise. Diagnostic-only bounded native mean/spread windows add no I2C
reads, calibration decisions or movement semantics. Corrected CI, stationary
bias observations and repeated physical axes/neutral tests remain required.

## Frame candidate software qualification; downloader diagnosis pending

Frame correction source `2ee4f77d0a6e5f4b1fa18a1110e0596720acb38f` passes all
four gates in [CI 38474705997](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/actions/runs/38474705997),
including the now-passing actual-main regression and all 12 host tests.
Supplemental normal/diagnostic builds and linked descriptor checks pass.
[Frame observer artifacts](frame-observer-candidate.json) are archived; diagnostic
app SHA-256 is `e9695c98bcd56af1ddff01a5c46ef86163c36c80e957fcf9709320da38ec4299`.
These candidates are not flashed or physically qualified.

After operator reported cold downloader entry ready, fresh port/PnP inventories
still showed the running 303a:4005 application rather than 303a:1001 ROM.
No flash attempt followed. A bounded button-input diagnosis was requested:
hold BOOT for 12 seconds and release without reset/unplug. It deliberately
exceeds the 4-second recenter limit. The 60-second observation and separate
30-second later window were USB/serial stable, with no target motion,
no reset/recenter and BOOT down=0 in their periodic snapshots. Operator
completion/timing confirmation is pending; this does not establish a failed
button or wrong GPIO. Do not repeat automatic resets or flash the application
serial port. Verify the physical button input/downloader path first.

Current installed image remains source 62c17de / app 167fb83c, with the
uncorrected frame and raw observer. Gyro-bias compensation is not implemented.
Both issues/PRs remain open, with control/RF trials stopped for diagnosis.

## BOOT completion report and bounded recording

The operator reported completing the requested 12-second hold. The separate
`boot-hold-bounded-recording` on installed observer source 62c17de ran for
114.563 seconds, retained 6779 serial bytes without errors and closed its
serial handle on a host stop flag. It issued no target commands or resets.
Recenter=1 and neutral=1 were already present in its first retained snapshot;
prior bounded windows had recenter=0. This establishes that the firmware has
received a valid BOOT gesture at some point. It does not establish which
gesture caused it, the 12-second hold waveform, or long-hold rejection:
periodic snapshots did not capture BOOT down. Neutral remained the old
incorrect frame's saturated roll (85 degrees), so this is not recenter or
usability acceptance. Do not infer a failed button or wrong GPIO.

The ROM-entry diagnostic now keeps BOOT physically held through host mode
verification, eliminating early release as an uncontrolled timing variable.
No write is permitted unless 303a:1001 ROM and the live preserved MAC match.
At this checkpoint the board still runs 303a:4005 source 62c17de; the user has
been asked to reconnect while keeping BOOT held and reply holding. All serial
captures are closed. Frame observer 2ee4f77 remains archived and unflashed.
Default branches remain 2be8519 (TiltMouse) and a49d95e (receiver). Exact
evidence heads 379c611 and d9a27b8 pass CI 38476221230 and 38476226922.
