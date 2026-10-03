# AGENTS.md

These instructions apply to repository work unless a newer issue explicitly overrides them.

## Read first

Before editing:
1. read `RAG.md`;
2. read `docs/PROGRAMME.md`;
3. read `docs/ARCHITECTURE.md`;
4. read `docs/VALIDATION.md`;
5. read `docs/WIRELESS.md` for transport-related work;
6. read the owning GitHub issue and all of its comments;
7. reconcile live `main`, open PRs and relevant branches.

## Execution rules

- One TM issue owns one bounded implementation scope.
- Resume an existing legitimate branch/PR for the issue instead of duplicating it.
- Use a short issue-specific branch.
- Do not force-push `main`.
- Do not alter repository security/settings unless the owner explicitly asks.
- Do not broaden scope into LEDs, unrelated networking, scrolling or unrelated features.
- Wi-Fi is permitted only in issues that explicitly own ESP-NOW work or integrated ESP-NOW behavior.
- ESP-NOW mode must not grow into infrastructure Wi-Fi/IP networking unless a later issue explicitly changes scope.
- Bluetooth/BLE HID is deferred beyond v0.1; do not initialize or implement it unless a later issue explicitly owns that scope.
- Preserve the no-solder constraint.
- Group GPIO1–3 and GPIO5–7 only in software; never assume they are electrically connected.
- GPIO4 must not emit a mouse click.
- Keep GPIO14 / LED data quiescent for the initial product.
- Keep hardware access separated from pure logic and add host tests for pure logic.
- Normal application report emission has one selected transport owner at a time; avoid duplicate movement through USB and ESP-NOW.
- Do not fabricate target-hardware or RF evidence. If a task requires physical proof unavailable in the current environment, record the exact missing evidence and stop at a clean handoff rather than claiming completion.

## PR and merge protocol

For an implementation issue:
1. implement only the owned scope;
2. add or update tests and documentation;
3. run the required checks from `docs/VALIDATION.md`;
4. commit and push the issue branch;
5. open or update a PR that references the issue;
6. inspect automated checks and fix legitimate failures;
7. merge only when required automated checks are green and the issue's acceptance criteria are actually satisfied;
8. update/close the issue after merge;
9. update programme/RAG documentation if an architectural decision changed.

Use squash merge unless repository history or the issue gives a reason not to.

## Evidence discipline

Distinguish:

- **CI-established** facts;
- **host-test-established** facts;
- **target-hardware-established** facts;
- **RF/receiver-established** facts;
- assumptions awaiting physical confirmation.

A green firmware build proves compilation, not actual USB enumeration, ESP-NOW RF interoperability, receiver behavior, touch ergonomics or IMU orientation correctness on physical hardware.
