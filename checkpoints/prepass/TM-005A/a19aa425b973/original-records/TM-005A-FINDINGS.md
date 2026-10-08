# TM-005A prepass: consumable motion, persistent button state

**Assessment:** `techrote/ESP32-QMI8658C-TiltMouse` #15 at
`8d7313216916d8f7d554da20df1b5fb8603d0b21`, 4 October 2026.
**Disposition:** design contribution and executable host evidence; not implementation acceptance.
GitHub was read-only. No issue, comment, branch, PR, workflow or settings write was made.
Source locators are in `SOURCES.md`; retained originals have verified Git blob identities.

## 1. Decision

Use a **one-shot publication operation** for each generated integer delta, together with
**retained current button state and a sticky release obligation**. Do not make a logical
mouse report a cacheable current value in its entirety: its button fields are repeatable
state; its X/Y fields are single-use movement. Periodic state repair always contains zero
movement. The proposed public operation is `publish_consume`, not an ambiguous Boolean
`send` and not a retryable queue insertion.

For TM-005A, choose **bounded drop with no pending movement queue**. A transport that
cannot accept a delta immediately discards that delta, retains valid current buttons and
reports both dispositions. The motion engine keeps its own fractional remainder; the
transport never adds rejected or ambiguously delivered integer counts back into it.

This is a proposed resolution of an unspecified seam, not a claim about an existing
integrated application's policy. At the assessed commit, `main/app_main.c` initializes
safe board state and USB only; IMU and touch are explicitly disabled. The components
are merged but their producer-to-transport scheduler does not yet exist [S1–S10].

## 2. What the merged interfaces actually do

### Motion ownership begins before transport submission

`tiltmouse_mouse_motion_step` calls `integrate_axis` for each axis. For finite inputs:

```text
bounded_dt = min(report_dt, configured_max_report_dt)
exact      = clamp(velocity * bounded_dt + residual, -max_delta, +max_delta)
whole      = trunc(exact)
residual   = exact - whole
output     = whole
```

The default maximum interval is 50 ms and maximum delta is 127. Clamping discards
excess, rather than leaving an integer backlog in `residual`. The existing test is even
named `test_report_delta_clamp_discards_excess_backlog`. Fractional conversion is
committed when the motion step returns, before any transport has seen the output [S5].

A synthetic 0.25-count contribution over four steps emits `0, 0, 0, 1`; residuals are
`0.25, 0.5, 0.75, 0`. If the last integer report is rejected, residual is still zero.
Putting that integer back changes the algorithm into a delivery accumulator. Re-running
the same integration interval also double-integrates time. Neither is appropriate here.

**Important invalid-output trap:** the early invalid-argument branch in `step` returns
before clearing `*delta`. The retained-source probe demonstrates that sentinel values
`77,66` remain after a negative-dt call. Not-neutral and configuration failures take a
later path which does clear the delta, but do not necessarily clear residuals. Therefore
TM-006 must initialize output locals each cycle and use outputs only after successful
status checks. The pose-to-velocity entry point does not independently establish source
age or all finite pose values. Validate finite inputs before conversion, not afterward.

### Touch owns persistent state, but not freshness

`tiltmouse_touch_buttons_t.left/right.pressed` are debounced persistent states;
`tiltmouse_touch_buttons_output_t` publishes complete booleans and calibration status.
GPIO1–3 and GPIO5–7 are fused separately; GPIO4 is diagnostic-only. The existing tests
cover hold/release, simultaneous buttons and GPIO4 exclusion [S6].

The output has no acquisition timestamp or hardware-error status. The update function's
null guard also precedes its output clearing. A previous successful output must not be
mistaken for a new hardware observation. The integration layer needs successful touch
acquisition age and calibration status separately from these booleans.

### USB is currently a stateless submission adapter

`tiltmouse_usb_hid_mouse_send` checks initialization, mount, suspension and
`tud_hid_ready()`. Failure returns `ESP_ERR_INVALID_STATE` without calling the outbound
stack API. It constructs a stack-local three-byte `{buttons,x,y}` payload and calls
`tud_hid_report(0, &report, sizeof(report))`; false returns `ESP_FAIL`, true returns
`ESP_OK`. There is no retained application report, retry queue or completion ledger [S4].

`release_all` follows the same gates. A failed release is not retained. Device lifecycle
callbacks only maintain suspension state; resume does not automatically replay a release.
The host C probe compiles these actual adapter functions against a deliberately fake
backend and demonstrates this behavior. It is not a TinyUSB or USB-hardware test.

Upstream TinyUSB 0.21.0 claims an endpoint, copies report bytes into its own endpoint
buffer, and calls `usbd_edpt_xfer`. Its completion and failure callbacks are distinct from
the Boolean submission return [E1]. This supports a synchronous copy/ownership boundary
on successful submission. The precise managed `espressif/tinyusb` **0.21.0~1** artifact
was not retrieved; verify that revision and its device-controller behavior during the
actual refactor. `ready()` is advisory: submission can still fail after a readiness check.

## 3. Exact publication contract

The two headers in `proposed_tree/` are a compilable API sketch, not a production adapter.
The patch adds only those headers. No Wi-Fi, BLE, sensor or USB initialization is added.

```c
typedef struct {
    int8_t dx, dy;
    uint8_t buttons; /* left=1, right=2; no other bits */
    uint8_t kind;    /* INVALID=0, CURRENT=1, RELEASE_ALL=2 */
} tm_mouse_report_t;

tm_publish_result_t publish_consume(
    void *ctx, const tm_mouse_report_t *report,
    uint64_t motion_deadline_us);
```

`kind` is logical validity/release intent, not a wire message type. This structure is not
serialized by copying its memory. The constructor accepts wide integer arguments and
checks `[-127,127]` and the button mask **before narrowing**. Zero-initialized storage is
invalid rather than accidentally usable. `RELEASE_ALL` canonicalizes all fields to zero,
even if a caller supplied contaminated movement or button fields.

Every candidate passed to `publish_consume` is terminal for that publication. The caller
must not offer it again, send it to a second transport, or refund its movement. The
transport never retains the caller's pointer. C cannot enforce linear ownership: a
single serialized router plus tests enforces this contract. Audit-only origin labels in
the Python model expose violations; they are not proposed wire or application metadata.

The result has three independent fields:

| Field | Meaning |
|---|---|
| `motion = NONE` | Canonical input has no movement. This says nothing about button delivery. |
| `motion = ACCEPTED` | All X/Y were copied into this transport's accepted work. No partial-axis acceptance. Not a host-delivery acknowledgement. |
| `motion = DROPPED` | No ownership-preserving replay is allowed. Includes rejection, expiry, invalidity and suppressed movement. |
| `state = BUTTONS_RETAINED` | Valid complete button state has replaced local desired state, including while busy. |
| `state = SAFE_RELEASE_RETAINED` | A neutralization obligation is retained, not necessarily transmitted yet. |
| `state = UNCHANGED` | Inactive route did not update state or emit anything. |
| `reason` | OK, not-ready, submission-failed, invalid, release-first, expired or inactive; diagnostic, never retry permission. |

For invalid input, `DROPPED` means any supplied candidate was discarded, even when its
untrusted values happened to be zero. A canonical explicit release returns `NONE` and
`SAFE_RELEASE_RETAINED`: its success means the obligation was recorded, not that USB
accepted it. Poll `status().release_pending` for local progress; it is not end-to-end proof.

| Situation | State and movement outcome | Subsequent action |
|---|---|---|
| Valid, ready, backend accepts | Buttons retained; entire fresh delta accepted | Caller retires candidate immediately. |
| Not initialized/mounted, suspended, busy | Buttons retained; nonzero movement dropped | Service latest state with zero movement when possible. |
| Backend refuses before acceptance | Buttons retained; movement dropped; submission-failed | No integer retry; later state repair is independent. |
| Later failure, with or without hidden host delivery | Previously accepted movement remains consumed | Never refund/replay; repair only current button state. |
| Expired motion, independently fresh buttons | Movement dropped; buttons retained | Zero-motion state may still be submitted. |
| Malformed/null/invalid active input | Ignore unsafe fields; latch all-up; movement dropped | Service neutralization, never a previous report. |
| Release/re-press conflict | Retain latest desired state and release debt; candidate movement dropped | Zero-motion release barrier must precede re-press. |
| Wrong/inactive transport | State unchanged; candidate discarded | Router error/diagnostic, not fallback duplicate send. |

Do not use the API's reason code to infer delivery. A future backend that can report
partial or ambiguous *submission* must conservatively classify any possibly accepted
movement as consumed, or expose an additional consumed-unknown outcome. It must not
call such an outcome safely retryable. The present fake has explicit pre-acceptance refusal.

## 4. Ownership and state diagrams

```text
orientation + dt       touch successful acquisition
       |                         |
 motion step                 debounced buttons
 residual retained                | current state
       | integer delta            |
       +------------- application snapshot + source-age checks
                               |
                       single selected router
                               |
                       publish_consume once
                               |
              +----------------+-----------------+
              |                                  |
         backend accepts                    cannot accept
      copied, immutable flight             delta discarded
              |                                  |
   completion / failure / unknown          NO pending movement
              |                                  |
              +----------- no delta refund ------+
                               |
        desired buttons + sticky release debt persist independently
                               |
                  service: ZERO-motion state reports
```

```text
button state: desired + release_debt + internal revision

falling bits      -> debt |= old_desired & ~new_desired
                    revision advances; desired becomes latest full state
re-press in debt  -> send desired & ~debt first, with zero movement
completion       -> clear only the debt snapshot belonging to this revision
failure/stale cb  -> do not erase newer debt; never restore old movement
```

The transport may keep one accepted in-flight report and constant-sized state. The Python
history arrays are test instrumentation, not a proposed unbounded firmware allocation.
Callbacks must be serialized with publication/service through a bounded event handoff or
proper synchronization. A `volatile` flag alone is not a multi-field ownership protocol.
Copy any needed callback metadata before returning; do not retain stack-owned pointers.

## 5. Release priority and its limits

A button release must not disappear when latest-state replacement subsequently observes
another press. Retain a per-button debt mask plus an internal revision. A falling left
button while right remains held records left debt without releasing right. If desired
state still has debt bits clear, a normal fresh report can include its own X/Y unchanged;
healthy press-drag-release behavior is preserved. There is no stale movement queue ahead
of a release.

If a button is re-pressed before its release obligation completes, send a zero-motion
barrier with the owed bits clear. Do not combine that barrier with re-press movement.
After completion, subsequent fresh input or a zero-motion state report may reassert the
held button. Global invalidity/explicit safety release owes all-up. An already accepted
transfer cannot be retroactively preempted; priority means the **next eligible submission**.

Completion clears only the debt snapshot associated with the same internal release
revision. A release, re-press and second release during one in-flight transfer must not
let the first completion clear the second obligation. The global revision used by this
model is conservative: unrelated changes may cause another harmless zero-motion release.
Do not repeatedly call global neutralization every scheduler tick while waiting, because
that needlessly advances its revision; enter the fault state once, then service it.

**Guarantees are conditional.** With a functioning service loop and a backend that
provides sufficiently strong completion evidence, a stable release eventually reaches the
fake host; the model verifies this under recovery. A permanently unavailable link cannot
be forced to deliver by software. A press and release entirely between successful state
reports may be missed; this is not a reliable click-event queue.

**Wireless counterexample:** `ESP_NOW_SEND_SUCCESS` is MAC-level evidence, not proof of
application delivery [E2]. The executable trace delivers press, hides release despite
successful lower-layer completion, then delivers re-press. The host sees `[pressed,
pressed]`. Repeating *current pressed state* cannot reconstruct the missed release.
Do not claim that full state plus a sequence number makes every click/release reliable.
Stronger release-before-repress guarantees need an accepted receiver-level state ACK,
or a fail-closed timeout/rearm protocol that stops refreshing the old pressed state and
actually establishes neutralization. These belong to TM-005B/TM-007 and faikeow's accepted
contract, not a private byte format invented here. Ordinary stable-state repair and
receiver link timeout remain useful, but are weaker guarantees.

## 6. Policy comparison

| Policy | Benefit | Cost / compatibility | Recommendation |
|---|---|---|---|
| One-shot bounded drop | Constant state; no delayed motion backlog; transparent ownership | Drops distance during busy intervals; missed short clicks remain possible | Choose for TM-005A; aligns with latest-first and existing clamp-discard intent. |
| Unbounded accumulation | Conserves unsent integer sums in ideal arithmetic | Old movement bursts, unbounded age/overflow, release ordering and cross-host replay risks | Reject. |
| Bounded sum coalescing | Recovers some brief congestion without unlimited storage | Requires maximum age, widened saturating arithmetic, provenance and button-epoch boundaries; clipping loses counts | Possible later measured policy, not the minimum seam. |
| Latest unsent replacement slot | Preserves newest candidate while rejecting old candidates | Needs explicit valid/consumed bit and deadline; never replay slot as state | Feasible but additional state without demonstrated need here. |

`compare_policies.py` calculates a synthetic sequence `+4,+5,+6`, unavailable for the
first two intervals. Immediate drop sends `+6`; unbounded accumulation sends `+15`; a
latest replacement sends `+6`; bounded summation with an illustrative age window keeps
only the last two and sends `+11`. None is a measurement of cursor feel or screen pixels.
The 8-unit window is a fixture assumption, not a tuned firmware default.

Even exact count conservation does not preserve report timing or the path. Coalescing
`+8 while left held` with `-8 after release` into zero destroys the drag excursion.
Only combine **unaccepted**, source-valid deltas in the same button epoch and bounded
age interval; flush/drop rather than cross a button/fault/re-centre/transport boundary.
Never coalesce an accepted delta back into unsent work. Because the project maps tilt to
velocity rather than absolute position, loss should stop or reduce movement instead of
creating a later catch-up jump [S2, S3, S5].

## 7. Worked traces

All numbers are synthetic counts. Full ordered records are in `results/worked_traces.json`.

### Backpressure

| Time | Candidate | Backend | Result / host movement |
|---|---|---|---|
| 0 | +4, buttons 0 | Busy | Drop +4, retain state 0. |
| 8 | +5, buttons 0 | Busy | Drop +5, retain state 0. |
| 16 | +6, buttons 0 | Accept + complete | Host gets +6, not +15. |
| 24 | State service | Accept + complete | Host gets 0; total remains +6. |

### Press, drag and release during congestion

| Action | Delivered state / movement |
|---|---|
| Press | left=1, (0,0). |
| Drag | left=1, (+7,-2). |
| Busy drag (+8,-3) | Movement discarded, left remains desired=1. |
| Busy release | desired=0, left release debt retained. |
| Recovery service | left=0, (0,0); final movement (+7,-2), no stale drag. |

### Ambiguous later failure

An accepted `(9,0,left=1)` can fail with hidden delivery either false or true. The fake
host therefore ends at X=0 or X=9. Both paths repair button state using `(0,0,left=1)`.
Neither emits another +9. Exactly-once **delivery** is not claimed; at-most-once
application submission is the invariant that avoids creating duplicates.

### Release, failed submission, re-press

Host first sees left=1. A release submission fails before acceptance; debt remains.
A new `(9,0,left=1)` cannot erase that debt: its movement is dropped and a `(0,0,left=0)`
barrier is accepted. After completion, service emits `(0,0,left=1)`. Delivered sequence
is `1,0,1`, with zero movement from the re-press candidate. If the release completion
itself fails, debt remains and only the zero-motion barrier is repeated.

### Partial release

From both buttons held, a normal fresh report `(3,4,right=1,left=0)` can be accepted
unchanged. It clears left without dropping right. The fixture's total after its preceding
`(1,2,both)` report is `(4,6)` and final buttons=2.

### Stale completion

First release enters flight. Re-press and another release advance debt revision before
the first completes. The older completion cannot erase the new debt. A subsequent
zero-motion service establishes the latest release. Tickets from a fenced prior
activation are ignored by the new activation's state machine.

## 8. Freshness belongs at two different layers

Application needs the **last successful IMU acquisition time/status**, last successful
touch acquisition time/status, calibration/neutral validity, finite configuration/pose,
actual report interval, report deadline and discontinuity state. Estimator state has a
sample timestamp, but its exported pose does not. Touch output has no timestamp [S5–S7].
Maintain ages alongside source snapshots. A new report timestamp must not refresh an
old pose or a stuck touch reading.

The optional `motion_deadline_us` argument is local monotonic scheduling information,
outside the core report. The backend checks it using its injected clock. Buttons must
already have independent freshness validation: an expired **combined input snapshot**
does not permit treating its buttons as fresh. The expired-motion fixture explicitly
assumes a separate fresh button observation. A synchronous integration may simply make
the deadline one bounded scheduling interval; the exact interval remains untuned.

On no valid neutral/IMU, emit no movement; independently valid touch may still produce
state. On stale or failed touch, stop using its old pressed state and request release.
Fault transitions, re-centre and transport changes clear fractional residual and reset
report-time accounting. Short ordinary endpoint contention need not clear fractional
sub-count state, but never stores whole rejected counts. Periodic retained-state service
also needs an application-health watchdog: it must not perpetually refresh a dead
producer's pressed state.

Wire session IDs, sequence numbers, version/profile identifiers, serialization, receiver
ordering rules and callback-generation tickets remain inside the transport. The
application needs readiness/fault/quiescence status, not ownership of packet counters.
faikeow owns its accepted envelope and `relative_mouse` profile; this prepass does not
freeze or duplicate those bytes [S2, S3, S9].

## 9. Selected-transport transition

Prefer explicit startup selection in v0.1; automatic cable-trigger switching is not
required. For optional runtime switching, the model chooses a deliberately conservative
policy that interrupts a drag rather than moving it between hosts.

```text
RUN(old)
   | request switch: gate new reports, discard candidate, clear residual/dt
   v
QUIESCING(old)
   | finish/cancel old accepted work; send neutral; establish a real fence
   | unavailable or no proof of fence -> stay blocked, report incomplete switch
   v
ACTIVATE(new private transport generation/session)
   | initial ZERO-motion neutral and completion
   v
REARM
   | fresh valid all-buttons-up observation; discard its movement
   v
RUN(new) with newly generated movement only
```

An accepted old delta is never transferred to the new transport. Cleanup release on the
old transport is not duplicate normal movement. Old callbacks cannot mutate the new
transport's state. An epoch counter rejects stale callbacks locally, but **does not by
itself stop a radio packet already in the air or a queued endpoint transfer**. A real
backend fence or accepted receiver timeout contract is required before declaring the
transition quiesced. The synthetic model represents a fence by completing all old work;
it does not prove hardware cancellation or remote neutrality.

The chosen all-up rearm rule avoids importing a held gesture to another host. Reasserting
fresh held buttons on the new host is an alternative product choice, not equivalent
behavior. Keep runtime switching unavailable until that choice and fencing are tested.
Re-centre, unlike switching, should normally preserve fresh held button state while
clearing motion residual; do not turn every re-centre into an unsolicited release.

## 10. Two compatibility issues already in the baseline

### Descriptor omits button data

The verified `s_hid_report_descriptor` sets two one-bit buttons, then overwrites count
and size with six padding bits **without an intervening `Input` item**. Its first Input
is constant padding. Static short-item parsing establishes **zero button data bits**,
X/Y starting at bit 6, and 22 declared input bits, whereas the software emits 24 bits.
The Linux HID documentation explains why the Input item commits those fields [E3].

`descriptor_check.py` asserts the assessed layout and evaluates a separate two-byte
`81 02` insertion after `95 02 75 01`. That candidate declares 2 button bits + 6 padding
bits + 16 X/Y bits = 24. It is **not applied**. Existing pure payload tests do not parse
the descriptor and therefore cannot establish intended button behavior on a real host.
Treat this as a separately reviewed correction or an explicit acceptance dependency;
do not silently change it under a promise of unchanged descriptor bytes.

### Raw -128 versus logical -127

The raw payload helper and its existing test preserve `INT8_MIN`, but the descriptor
and motion clamp define -127..127 [S4, S5, S8]. The new seam rejects -128; the legacy raw
helper can remain byte-preserving for compatibility until a deliberate deprecation.
The host probe checks all **260,100** combinations in the shared valid domain. It
compares new logical fields with old helper bytes; it does not claim a completed new
USB adapter or resolve the descriptor defect.

Preserve current identity/configuration strings, report ID 0, three-byte payload, two
button mask and 8 ms polling in the seam refactor. Do not replace the raw three-byte
send with TinyUSB's convenience mouse helper: the inspected upstream helper includes
wheel and pan fields [E1]. Optional descriptor repair is the explicit exception above.

## 11. Focused implementation order

| Stage / intended path | Narrow change and reason |
|---|---|
| TM-005A: `components/mouse_report/include/tiltmouse/mouse_report.h` | Land canonical logical type, range/mask validation and explicit release construction. Header sketch supplied. |
| TM-005A: `.../mouse_transport.h` | Result types and narrow function table; document consume-once and no host-ACK claim. Header sketch supplied. |
| TM-005A: `components/mouse_report/mouse_report.c` or `mouse_output.c`, plus CMake | Port retained desired/debt/revision policy and injected monotonic/backend interfaces into pure C. No allocation or motion queue. Keep naming decision explicit. |
| TM-005A: `components/usb_hid_mouse/usb_hid_mouse_report.c` and header | Add checked logical-to-three-byte mapping; preserve legacy helper until callers migrate. Do not serialize logical struct directly. |
| TM-005A: `components/usb_hid_mouse/usb_hid_mouse.c` and public header | Add transport adapter and completion/lifecycle event handoff; retain descriptor/identity unless separate fix approved. Low-level raw send becomes private or clearly legacy; integration must not bypass ownership. |
| TM-005A: USB release entry point | Preserve callable all-buttons-release behavior. Add persistent neutralization through new seam; do not redefine legacy `ESP_OK` from stack acceptance to merely latching without documenting an API change. |
| TM-005A: `tests/host/test_mouse_report.c`, `test_mouse_output.c`, `test_usb_hid_mouse_adapter.c`, host CMake | Port fixture assertions to C, inject backend readiness and completions, exercise actual adapter rather than only byte helper. Preserve existing tests and add descriptor-layout test separately. |
| TM-005A: `docs/ARCHITECTURE.md`, `WIRELESS.md`, `VALIDATION.md`, `RAG.md` | Record terminal delta ownership, conditional release guarantee, callback serialization and unchanged hardware scope. |
| TM-005B: `components/espnow_mouse/` | Implement same seam using exact accepted faikeow bytes. Keep session/sequence/callback ordering private. Decide MAC versus application ACK meaning; no stale retries. |
| TM-006: `main/app_main.c`, scheduling/config module and integration tests | Initialize actual inputs; track last-success ages, neutral, dt and finite status; one motion step per interval, one publication, independent bounded zero-state service; select one transport. |
| TM-006/TM-007: transition/fault orchestration | Startup-only selection first; optional runtime fencing/all-up rearm, producer watchdog and receiver timeout evidence before claiming safe handover. |

Keep TM-004 orientation/motion and TM-005 touch algorithms unchanged in the seam task.
Integration guards handle their early-error outputs; any algorithmic hardening should be
separately scoped. USB target build, original complete host suite and physical USB/RF
acceptance remain required later; this packet does not replace those gates.

## 12. Evidence and remaining decisions

Executed: ten labelled fixture scenarios; 6,400 seeded stress steps; focused release,
invalid-input and switch cases; five deliberate defects detected by assertions; original
USB/motion source compiled with strict warnings and undefined-behavior sanitizer; exhaustive
valid-domain field compatibility; static descriptor layout check. See `results/`.
These are finite host checks, not a proof over all firmware interleavings or USB/RF paths.

Before implementation acceptance, settle: separate descriptor correction; exact managed
TinyUSB callbacks and fencing; backend event synchronization; application freshness and
service/watchdog thresholds; whether runtime switching is in scope; reliable release
versus best-effort full-state semantics in the accepted faikeow protocol. The recommended
defaults here are immediate motion drop, sticky local releases, startup-only selection
and fail-closed optional switching. None of the unresolved receiver questions justifies
placing wireless session/sequence fields in the logical mouse report.
