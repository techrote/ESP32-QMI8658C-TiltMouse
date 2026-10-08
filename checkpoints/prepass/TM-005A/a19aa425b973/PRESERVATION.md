# TM-005A prepass preservation

This checkpoint preserves the completed preparatory work for [issue #15](https://github.com/techrote/ESP32-QMI8658C-TiltMouse/issues/15). The original work assessed commit `8d7313216916d8f7d554da20df1b5fb8603d0b21`. Archive preservation does not implement the logical-report seam or establish issue acceptance.

## Original artifact and layout

- Original filename: `TM-005A-prepass-8d731321.zip`.
- Original ZIP: [ORIGINAL-PACKET.zip](ORIGINAL-PACKET.zip), 74,516 bytes.
- ZIP SHA-256: `9fc446a0739cbb8d48f1ad226e1da8a51d024a0080163e893c14073e515f143a`.
- Original Library locator: `libfile_482166a062388191a6cfde83bd22d658`; file identity: `file_000000001ff081f49d506dede4366bc5`.
- Historical parent: `8d7313216916d8f7d554da20df1b5fb8603d0b21`; parent tree: `dfdba897d73ce6e3c8c64bdb647296b3ea7aceca`.
- Planned archival ref retained: `checkpoint/prepass-TM-005A-a19aa425b973`. The suffix preserves the earlier intended destination label; it is not the original ZIP or internal manifest hash.
- Original internal manifest SHA-256: `eb35aa387e86e3d5292d3cd598e2c021a6796499abc95790c0a7aa632db6c34f`.

All 45 original archive members are preserved under `unpacked/`, removing only the ZIP's enclosing `TM-005A-prepass/` directory from the unpacked path. The original ZIP retains its exact names and bytes. Its 44 payload files remain covered by the unchanged [MANIFEST.json](unpacked/MANIFEST.json); the manifest itself is the 45th member. Original executable modes are retained for `unpacked/results/baseline_probe` and `unpacked/run_checks.sh`; the other packet files use mode 100644.

The separately saved handoff, findings, archive-verification receipt and SHA-256 sidecar are retained unchanged under `original-records/`. [PRESERVATION-MANIFEST.json](PRESERVATION-MANIFEST.json) inventories the actual preserved files, their byte sizes, SHA-256 identities and Git modes; it excludes itself. This new record is separate from the immutable original packet and its original verification claims.

## Work preserved

Start with the original [handoff](unpacked/HANDOFF.md), [findings](unpacked/FINDINGS.md) and [source locators](unpacked/SOURCES.md). The packet includes the executable Python policy model, synthetic fixtures and worked traces, policy comparisons, C probe and fake-backend headers, descriptor checker, seven assessed source files, observed outputs and environment records, API-only patch, proposed headers and implementation order.

The proposed policy consumes integer motion once, drops unavailable or expired motion, retains complete button state and sticky release debt, and uses zero-motion state service to repair buttons. It preserves the distinction between movement acceptance and button-state delivery. Transport metadata remains outside the core logical report.

The recorded findings include a forgotten failed release, stale output on an early motion error, a missing button Input descriptor item (22-bit layout with X/Y at bit 6), and the -128 domain discrepancy between proposed logical validation and the legacy helper. These are preparatory findings at the assessed commit, not fixes installed by this checkpoint.

The proposed `mouse_report.h`, `mouse_transport.h` and `proposals/0001-logical-seam-headers.patch` remain archival. No packet code or source export is installed in active component paths, and no patch is applied.

## Scenario-count reconciliation and historical evidence

The actual `fixtures/scenarios.json` contains **10 named fixture scenarios**. The retained `results/model_results.json`, `results/model_stdout.json`, `results/verification.json`, FINDINGS and HANDOFF agree on that count; `prototype/run_model.py` derives it from the fixture array length. The conversation's separate reference to 18 model scenarios is not an 18-fixture inventory in this packet. This publication uses the verifiable categories below and does not invent an explanation or rewrite historical files.

- 10 fixture scenarios reported passing.
- Additional checks recorded separately: release barrier retry, inactive route, four invalid/release candidates, and fenced switching with fresh-all-up rearm.
- 64 seeded stress runs, totaling 6,400 steps.
- Five deliberate defects reported detected, with their messages retained.
- 260,100 valid-domain C field comparisons, using the focused fake-backend probe.
- Recorded descriptor, header-syntax, isolated patch and sanitizer checks remain historical evidence.

The historical archive receipt's note that a rebuilt binary differed because of debug paths is retained unchanged. This preservation keeps the original bundled executable bytes; it does not substitute a rebuilt executable.

The original complete host suite, target firmware build and physical USB/RF evidence were not established by this prepass. TinyUSB completion/fencing, synchronization, thresholds, switching policy and receiver release-delivery guarantees remain implementation questions. A future implementation must reconcile current repository authority and establish its own acceptance evidence.

## Preservation verification and current status

On 8 October 2026 the recovered original ZIP matched its recorded size and SHA-256; CRCs, safe paths and exact member inventory passed. All 44 original manifest payloads matched their sizes and SHA-256 values, and the seven retained source files matched their recorded Git blob identities. Every unpacked byte matched the corresponding ZIP member. These are preservation checks; the research models and historical probe were not rerun.

The archival tree adds only this checkpoint directory to the assessed parent. Repository navigation is harmonized separately on current main. The issue progress record and its focused documentation PR provide the publication commit and remote read-back results after the branch becomes reachable; no pre-publication check is presented here as proof of a remote write.

The original packet's dated statements that it was local/read-only describe the producing prepass. This checkpoint and later issue/documentation records describe its subsequent publication. Issue #15's implementation acceptance remains separate.
