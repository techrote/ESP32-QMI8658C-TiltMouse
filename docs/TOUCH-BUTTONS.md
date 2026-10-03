# Capacitive touch acquisition and button logic

TM-005 owns the touch subsystem only. It does not emit HID reports and it does not tune physical-board thresholds.

## Hardware edge

`touch_input` uses the ESP-IDF v6.1 `esp_driver_touch_sens` API through `driver/touch_sens.h`.

- Touch channels 1 through 7 are allocated independently and verified against GPIO1 through GPIO7.
- The controller is primed with three one-shot scans, then left in continuous-scan mode.
- Application reads use `TOUCH_CHAN_DATA_TYPE_RAW`; ESP32-S3 touch hardware V2 produces larger values as capacitance/touch increases.
- Each returned frame contains all seven raw channels plus an `esp_timer` timestamp.
- Acquisition failures are returned explicitly. Initialization unwinds partially-created driver state rather than hiding the failure.
- The ESP-IDF channel `active_thresh` required by the driver configuration is not used as the product click threshold. Logical button decisions are entirely outside the target driver.

The initial sample electrical settings match Espressif's ESP32-S2/S3 touch example: 500 charge cycles, 0.5 V low limit, 2.2 V high limit, charge speed 7 and default initial charge voltage. TM-008 may change these only from physical evidence.

## Pure logic pipeline

`touch_buttons` is ordinary C with no ESP-IDF dependency. One raw frame passes through:

1. independent per-channel startup calibration;
2. per-channel baseline and noise estimation;
3. positive-delta normalization using the larger of a relative-baseline floor and a noise-derived floor;
4. left/right fusion over GPIO1-3 and GPIO5-7 respectively;
5. logical press/release hysteresis;
6. temporal debounce;
7. slow baseline/noise adaptation only for evidence considered idle.

A candidate or held logical group freezes baseline adaptation for all three electrodes in that group. A channel with elevated normalized evidence also freezes its own adaptation. This prevents a sustained touch from being quickly learned into the baseline.

GPIO4 is calibrated and normalized exactly like the other channels so its diagnostic signal is available, but it is excluded from every fusion policy. There is no GPIO4 common-mode subtraction in TM-005.

## Fusion policies

The pure logic supports three selectable policies:

- `SUM`: the three normalized evidences are summed.
- `STRONG_OR_TWO_MODERATE`: one strong electrode or at least two moderate electrodes asserts the group.
- `TWO_OF_THREE`: at least two electrodes must exceed the voting threshold.

Each policy has separate press and release thresholds. Left and right state machines are independent, so simultaneous holds are representable.

## Provisional defaults

The checked-in values are deterministic engineering starting points for software validation, not claims about bare-board touch reliability. The default is `STRONG_OR_TWO_MODERATE`, with a 16-frame startup calibration and two-frame press/release debounce. Normalization, fusion thresholds and adaptation rates are centralized in `tiltmouse_touch_buttons_config_t` for TM-008 tuning.

TM-008 must use physical ESP32-S3-Matrix measurements to establish final touch sensitivity, noise margins and ergonomic reliability. CI establishes only the deterministic software behaviour described here.

## Host evidence

`tests/host/test_touch_buttons.c` covers:

- quiet idle noise;
- independent calibration and normalized response across different baselines;
- single-electrode and broad multi-electrode touches;
- a one-frame noisy/outlier electrode;
- GPIO4-only activity under every fusion policy;
- simultaneous left/right state;
- press, hold and release debounce;
- frozen baseline during touch and slow idle baseline adaptation;
- summed, strong-or-two-moderate and 2-of-3 fusion semantics;
- replay of the human-readable `tests/fixtures/touch_replay.csv` trace.
