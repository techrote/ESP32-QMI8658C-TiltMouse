#!/usr/bin/env python3
"""Small dependency-free repository text/style sanity check."""

from __future__ import annotations

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {".git", "build", "managed_components", "dependencies"}
TEXT_SUFFIXES = {".c", ".h", ".md", ".py", ".txt", ".yml", ".yaml"}
TEXT_NAMES = {"CMakeLists.txt", "sdkconfig.defaults"}


def iter_text_files():
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file():
            continue
        if any(part in SKIP_DIRS for part in path.relative_to(ROOT).parts):
            continue
        if path.suffix in TEXT_SUFFIXES or path.name in TEXT_NAMES:
            yield path


def main() -> int:
    failures: list[str] = []
    for path in iter_text_files():
        rel = path.relative_to(ROOT)
        data = path.read_bytes()
        if b"\r\n" in data or b"\r" in data:
            failures.append(f"{rel}: CRLF/CR line endings are not allowed")
            continue
        if data and not data.endswith(b"\n"):
            failures.append(f"{rel}: missing final newline")

        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            failures.append(f"{rel}: expected UTF-8 text")
            continue

        for lineno, line in enumerate(text.splitlines(), start=1):
            if line.rstrip(" \t") != line:
                failures.append(f"{rel}:{lineno}: trailing whitespace")

    if failures:
        print("Style check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print("Style check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
