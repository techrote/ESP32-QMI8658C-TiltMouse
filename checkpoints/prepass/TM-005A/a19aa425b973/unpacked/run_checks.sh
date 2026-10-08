#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${1:-"$ROOT/results"}
mkdir -p "$OUT"
export PYTHONDONTWRITEBYTECODE=1
python3 "$ROOT/prototype/run_model.py" "$OUT" > "$OUT/model_stdout.json"
python3 "$ROOT/prototype/descriptor_check.py" "$OUT" > "$OUT/descriptor_stdout.json"
python3 "$ROOT/prototype/compare_policies.py" "$OUT"
S="$ROOT/evidence/assessed/components"
I="$ROOT/proposed_tree/components/mouse_report/include"
cc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror \
  -fsanitize=undefined -fno-sanitize-recover=all \
  -I"$ROOT/prototype/stubs" -I"$S/usb_hid_mouse/include" \
  -I"$S/mouse_motion/include" -I"$S/orientation/include" -I"$I" \
  "$ROOT/prototype/baseline_probe.c" "$S/usb_hid_mouse/usb_hid_mouse.c" \
  "$S/usb_hid_mouse/usb_hid_mouse_report.c" "$S/mouse_motion/mouse_motion.c" \
  -lm -o "$OUT/baseline_probe"
"$OUT/baseline_probe" > "$OUT/baseline_results.json"
c++ -std=c++11 -Wall -Wextra -Wpedantic -Werror -I"$I" \
  -fsyntax-only "$ROOT/prototype/header_smoke.cpp"
printf 'All local checks passed.\n'
