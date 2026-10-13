# TM-007 — Transmitter lifecycle and fault hardening

## Invariants

- Relative X/Y is terminal at publication: never refund, retransmit or migrate movement after endpoint/API/callback failure.
- Complete button state is retained separately. Safety release and switch barriers contain zero movement.
- There is one in-flight logical transport report and no unbounded queue.
- Runtime switching is opt-in and local-fence based; normal v0.1 startup still selects one transport only.
- No click-motion freeze/damping is enabled without TM-008 physical evidence.

## USB lifecycle

Detach and suspend callbacks latch an atomic lifecycle reset. The serialized application owner consumes that event, fences/discards the old local in-flight ledger and re-activates USB with an all-buttons-up obligation. Resume/reconnect must service that neutral barrier before current buttons can be restored. If a lifecycle reset occurs while a deliberate transport quiesce is active, that quiesce intent is restored on the new local generation rather than cancelling the handoff. ESP-NOW recovery preserves the same quiesce intent. Pre-disconnect relative movement is never replayed.

Diagnostics count attach, detach, suspend, resume, successful completion, failed completion and lifecycle resets. The production USB adapter is compiled against a fake TinyUSB backend in host tests, exercising suspend, detach, reconnect, failed transfer repair and quiescence.

## ESP-NOW lifecycle

The FNR-006 sender session/sequence are application state, not Wi-Fi-driver state. Radio recovery tears down and rebuilds ESP-NOW/Wi-Fi/peer configuration while retaining the existing sender object. Sequence numbers continue monotonically, including u32 wrap, and a local radio recovery never pretends to be an application restart.

Three consecutive immediate/callback failures schedule recovery. Initial delay is 50 ms and doubles after failed recovery to a 1 s cap. One service call can perform at most one due setup attempt. A successful MAC callback cancels a pending recovery streak. On successful reinitialization the mouse transport starts behind a fresh release barrier. No stale movement backlog exists.

Initial selected-radio startup remains fail-closed. app_main performs at most three initialization attempts with a 100 ms delay; invalid pairing input is not retried. There is no USB fallback, plaintext mode, AP/IP service or BLE.

## Sensor and touch faults

QMI read errors invalidate neutral/motion immediately. Repeated target-driver errors trigger at most one re-probe/reconfiguration attempt per 100 ms after three failures. A prolonged `NO_DATA` condition is separately detected after the IMU freshness interval and feeds the same bounded reinitialization path, so a sensor that stops asserting data-ready cannot remain permanently inert without recovery attempts. A later valid sample counts as acquisition recovery but movement remains gated until neutral is captured again. Pure application staleness is separately counted once per fault episode.

Touch read failure or freshness expiry releases buttons and recreates the software calibration state. Fresh input remains invalid until the complete calibration sample count is observed again. The target touch controller is rate-limited similarly after repeated hardware read failures.

## Transport switching

The old route rejects new normal reports after quiesce begins but continues zero-motion state service. Older completion tickets cannot clear the newer release revision. The application waits for local quiescence, installs an already-initialized alternate output, clears motion residual/timing, then blocks normal reports while buttons are held. A fresh all-up observation primes the new route. Motion and held gestures do not duplicate across host transports.

No automatic cable-triggered switching is enabled by v0.1 main firmware. The API exists to make any deliberate later switch deterministic rather than an implicit fallback.

## TM-008 instrumentation

Every 10 seconds the application emits bounded diagnostics: generated/movement/dropped report counts; the last logical button transition; a provisional 100 ms post-transition window with peak signed X/Y and sample count for click-motion correlation; IMU/touch ages, errors, stale episodes, recoveries and touch recalibration; driver retry counts; maximum loop gap, collapsed timer slots and stack high-water mark; plus USB lifecycle counters or ESP-NOW send/recovery/session/sequence counters. The 100 ms window is measurement only and does not alter movement.

These are software observations, not USB/RF/ergonomic acceptance. TM-008 and faikeow FNR-008 must correlate them with actual receiver/host evidence.
