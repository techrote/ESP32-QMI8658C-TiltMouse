# TM-005A handoff

Assessed repository: `techrote/ESP32-QMI8658C-TiltMouse`, issue #15.
Assessed and rechecked main: **`8d7313216916d8f7d554da20df1b5fb8603d0b21`**.
This is a read-only prepass. Nothing was published, implemented on target, or accepted
on behalf of the issue. No GitHub mutations or workflow actions were made.

## Decision to carry forward

Publish each integer delta once. Return separate movement and button-state dispositions.
Drop unavailable/expired movement; retain complete current buttons plus sticky release
debt. Never restore accepted movement after a later failure. Zero-motion state service
repairs buttons. Keep wire session/sequence in the transport and input ages in integration.

## Contents and reproduction

Read `FINDINGS.md` (contract, diagrams, traces, compatibility, refactor map), then
`SOURCES.md`. Execute from the extracted packet with Python >=3.10, C11/C++11 compilers and
UBSan support:

```sh
python3 verify_packet.py
sh run_checks.sh /tmp/tm005a-recheck
```

`prototype/` contains the executable Python state model, synthetic policy arithmetic,
a focused C probe using seven Git-blob-verified originals, fake backend headers and a
static descriptor checker. `fixtures/scenarios.json` is explicitly synthetic. `results/`
contains actual run outputs and compiler/runtime identity. The ELF in results is a host
test executable, not firmware. The original complete host suite and target build were
not run. No physical USB/RF evidence is claimed.

## Intended repository paths

`proposals/0001-logical-seam-headers.patch` adds only:

- `components/mouse_report/include/tiltmouse/mouse_report.h`
- `components/mouse_report/include/tiltmouse/mouse_transport.h`

It is an API sketch, not the complete TM-005A change. `proposed_tree/` contains the exact
post-patch files. Apply only after review. The Python policy is not already wired to them.
Port policy into pure C in `components/mouse_report/`; refactor
`components/usb_hid_mouse/usb_hid_mouse.c` and report helpers; add host adapter/state tests
and CMake registration. TM-006 later connects source ages, neutral/dt and single-owner
routing in `main/`. Detailed sequencing is in FINDINGS section 11. These research files
could be preserved under `docs/prepasses/TM-005A/` without installing proposed code.

## Actual findings and remaining questions

Ten fixture scenarios and 6,400 seeded stress steps passed; all five deliberate defects
were detected. The C probe passed 260,100 valid-domain field comparisons and demonstrated
forgotten failed release and stale output on an early motion error. Static parsing found
an existing missing button Input item: descriptor declares 22 bits, zero button data,
X/Y at bit 6. A separate two-byte correction is evaluated but **not applied**. Resolve
that defect explicitly before asserting ordinary mouse semantics. New logical validation
rejects -128 although the legacy raw helper preserves it.

Still resolve exact managed TinyUSB completion/fencing behavior; callback synchronization;
input freshness/service/watchdog thresholds; startup-only versus runtime switching; and
faikeow's strength of release delivery acknowledgement. MAC-level ESP-NOW success cannot
prove a brief release survived before re-press. Stronger guarantees require receiver
contract support or fail-closed timeout/rearm, not another logical-report field.
