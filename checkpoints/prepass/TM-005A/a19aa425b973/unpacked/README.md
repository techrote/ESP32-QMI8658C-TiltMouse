# TM-005A logical-report seam prepass

Start with [HANDOFF.md](HANDOFF.md), then [FINDINGS.md](FINDINGS.md).
Source locators are in [SOURCES.md](SOURCES.md). This packet is a local, read-only
research/prototype contribution, not a firmware release or issue completion.

The decisive distinction is consumable integer motion versus persistent button state.
The proposed default is one-shot bounded motion drop, retained buttons and release debt,
with zero-motion repair and no replay after ambiguous acceptance.

## Layout

| Path | Content |
|---|---|
| `proposed_tree/`, `proposals/` | Compilable headers and an API-only patch; no production adapter installation. |
| `prototype/` | Executable synthetic state model, C baseline probe, fake backend, descriptor checker, policy arithmetic. |
| `fixtures/` | Labelled synthetic backpressure/button/failure traces. |
| `evidence/` | Seven exact Git-blob-verified baseline files, source inventory and reconciliation. |
| `results/` | Actual outputs, traces, executable host probe, environment and check records. |
| `run_checks.sh` | Reproduce finite checks offline; output directory argument recommended. |
| `verify_packet.py`, `MANIFEST.json` | Verify SHA-256, original Git blob identities and text sanity. |

```sh
python3 verify_packet.py
sh run_checks.sh /tmp/tm005a-recheck
```

Requires Python >=3.10, C11/C++11 compilers, UBSan and a POSIX shell. The bundled ELF
is a Linux x86-64 host test, not ESP32 firmware. Rebuild it rather than using it on
another platform. Test instrumentation stores traces; proposed firmware policy state
is bounded. Verification establishes integrity/reproducibility, not USB/RF acceptance.
