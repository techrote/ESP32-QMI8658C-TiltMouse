# TM-008 next test build: touch placement and dim tilt observation

Prepared after the operator's 14 October 2026 request to review ESPsand, build
the next candidate and stop until they are available. This is software/build
evidence only. Do not flash or continue physical trials without the operator.
The installed sender remains source `e8cfa99c3780df1296eb0fe76678224f67a33436`,
diagnostic application SHA-256
`a63f5c606c3ecebab9ca8ad56c86b3c5d29efd3a52486a82c6e7009e8f8d3abd`.
Board A and both open physical acceptance issues retain their previous status.

## Reference reviewed

Pinned ESPsand main: `fd509bfdd3e1a84e9cf097635d3f25a98a345759`.

- [ES-003 touch characterization](https://github.com/techrote/ESPsand/blob/fd509bfdd3e1a84e9cf097635d3f25a98a345759/docs/hardware/TOUCH_CHARACTERIZATION.md):
  GPIO1..7 are exposed touch-capable pins; ESP32-S3 readings rise with added
  capacitance. The tested bare-board fingertip affected most/all of the row.
  Edge pinching gave repeatable broad common-mode response, while local zones
  showed spurious activity. ESPsand consequently declined independent A/B
  product buttons, exposing broad contact and a gated coarse slider instead.
- [Acquisition adapter](https://github.com/techrote/ESPsand/blob/fd509bfdd3e1a84e9cf097635d3f25a98a345759/src/board/touch_zones.cpp)
  primes Arduino `touchRead()` once at startup, then reads one channel every
  4 ms, processing a seven-channel scan about every 28 ms. TiltMouse already
  primes its ESP-IDF continuous controller and consumes all seven channels;
  copying Arduino timing/initialization would not be a justified correction.
- [Normalizer](https://github.com/techrote/ESPsand/blob/fd509bfdd3e1a84e9cf097635d3f25a98a345759/lib/espsand_core/src/touch_normalizer.cpp)
  tracks baseline/noise, signed raw z and common-mode-corrected local z.
  These differ from TiltMouse's bounded positive evidence. The reference doc
  describes 75% common-mode cancellation, while the pinned normalizer header
  defaults to gain 1.0 and the board default-constructs it. Neither number is
  transplanted into TiltMouse. GPIO4 remains a measurement, never a click.
- [Physical matrix calibration](https://github.com/techrote/ESPsand/blob/fd509bfdd3e1a84e9cf097635d3f25a98a345759/docs/hardware/CALIBRATION_2026-09-14.md)
  and [adapter](https://github.com/techrote/ESPsand/blob/fd509bfdd3e1a84e9cf097635d3f25a98a345759/src/board/matrix_output.cpp)
  establish RGB byte order and ordinary row-major `y*8+x` traversal on its
  tested Matrix, viewed LED face forward with USB at the top. GPIO14 is matrix
  data, consistent with the Waveshare schematic. This informs the candidate;
  the exact TiltMouse unit's visible colour/orientation remains untested.

No ESPsand implementation, framework or dependency is imported. Its physical
touch result motivates a coupling investigation here; it cannot establish
this TiltMouse's button acceptance. Swapping locations also cannot establish
that the two groups are physically separable.

## Candidate behaviour

The operator chose GPIO5..7 for canonical left click and GPIO1..3 for right.
Application placement is applied once before USB or ESP-NOW encoding. The
fusion API's historical physical group names remain documented. Canonical
wire bits, both-held/release semantics, GPIO4 exclusion, thresholds, baseline
freezing and two-sample debounce are unchanged. Original placement can be
explicitly selected through application configuration for comparison.

The corrected IMU frame, learned stationary gyro offset, X inversion, Y sign,
3-degree deadzone, exponent 1.6 and gain 900 remain the last physically tried
motion defaults. No click freeze/damping or touch threshold change is added.

USB qualification telemetry adds fixed one-Hz `touch-observe` snapshots:

- `t` and `sample`: observation and last successful acquisition device time;
- `ready`, `err`, `swap`: touch validity, last acquisition error and placement;
- `groups`: debounced GPIO1..3 / GPIO5..7 physical states;
- `buttons`: last logical full-state mask, not proof of host receipt;
- `raw`, `baseline`, `noise`, `evidence_milli`: seven entries ordered GPIO1..7.

Floats are converted with saturation. Evidence is positive, clipped at the
existing 8.0 limit and scaled by 1000; it is not ESPsand z. Faults can leave an
older raw frame; `sample`, `ready` and `err` make that limitation explicit.
One-Hz snapshots can miss short taps or spikes. Use sustained representative
contacts next session and correlate with the existing transition timestamps
and measurement-only 100-ms peak-motion windows. No extra acquisitions occur.

## Very dim diagnostic sensor indicator

The operator explicitly requested this next-build exception to the normal
LED-off rule. Kconfig `TILTMOUSE_SENSOR_INDICATOR` defaults off and depends on
USB diagnostics. `sdkconfig.usb-diagnostics.defaults` opts in; normal USB and
ESP-NOW defaults remain off. There are no runtime commands or extra networking.

The dot shows recentered angles with the mouse axes' configured source/sign,
5 degrees per cell, rounded and clamped to +/-3 cells from (3,3). The view is
LED face up with USB toward the operator, rotated 180 degrees from ESPsand's
USB-top reference. Pending/stale/invalid IMU or neutral calibration displays a
centre blue dot; valid tilt is green. Only one pixel's one colour channel is
ever nonzero, at byte code 1/255 (aggregate frame code sum 1). Visible brightness
and orientation on this unit require physical confirmation; byte code is not
a measured luminance or electrical power claim.

GPIO14 starts low. A fixed RGB frame is pre-encoded into static RMT symbols
and transmitted with DMA, a one-entry queue and zero-wait/nonblocking calls.
Only changed frames are submitted, at most once per 100 ms; the payload is
not modified until completion. A 300-us low latch terminates each frame.
Allocation occurs at initialization only. Initialization failure leaves the
indicator off; a runtime error or 500-ms stuck transfer disables it once.
No mouse-path retries, blocking LED delay or animation is introduced.
Ten-second logs retain indicator submitted/busy/error status beside loop gaps
and logical drops so any observer overhead can be measured next session.

Host tests check button placement, releases/both-held/GPIO4, bounded pixel
brightness/coordinates/invalid inputs, and actual RMT adapter ownership,
nonblocking submission, unchanged idle frames, pending-payload immutability,
bounded busy handling and stop-on-error. Target builds and linked descriptor
checks verify compilation and the preserved mouse descriptor. These are not
physical LED, USB, touch or RF results.

## Next operator session

1. Confirm ROM identity, archive/current installed image and exact candidate
   manifest before flashing only the TiltMouse; retain existing recovery path.
2. Cold-start LED face up, USB toward the operator, untouched until the
   calibration telemetry is ready; verify very dim blue/green behaviour and
   unchanged directions/neutral/BOOT recenter. Stop for heat, instability or
   uncontrolled USB/reset behaviour under the campaign's stop conditions.
3. Capture separate sustained GPIO5..7 left and GPIO1..3 right contacts,
   releases, GPIO4-only, both groups, stationary clicks and moving drags.
   Compare each pin and cross-row response with host full-state button evidence
   and 100-ms motion peaks. Do not claim independent buttons from RawInput
   transition flags alone. Include baseline recalibration/fault recovery.
4. Compare local loop gaps/logical drops with the installed LED-off baseline;
   brightness, touch coupling and logging overhead remain observations to test.
5. Resume remaining USB lifecycle and joint authenticated RF/receiver matrix
   only after this candidate's physical behaviour is understood. Keep TM-008
   and FNR-008 conclusions separate; neither issue is accepted by this build.
