#!/usr/bin/env python3
"""Check that README.md and README.zh-CN.md stay structurally in sync.

Compares the `^#` ATX heading sequences (count and level order, not text) of
the two READMEs. The zh-CN README is a translation mirror; when a section is
added/removed/reordered in one file, the other must follow. Heading *level*
must match exactly — a `###` in one file vs `##` in the other is treated as
drift so hierarchy changes can't hide behind a same-count sequence.

Run:
    python tools/check_readme_sync.py          # exit 1 on drift
    python tools/check_readme_sync.py --check  # same (accepted for CI symmetry)
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
README_EN = REPO_ROOT / "README.md"
README_ZH = REPO_ROOT / "README.zh-CN.md"

# ATX heading: 1-6 `#` at line start, then a space (or end of line).
HEADING_RE = re.compile(r"^(#{1,6})(?=\s|$)", re.MULTILINE)


def heading_levels(text: str) -> list:
    """Return the heading level sequence of a Markdown text, e.g. [2, 2, 3]."""
    return [len(m.group(1)) for m in HEADING_RE.finditer(text)]


def heading_lines(text: str) -> list:
    """Return (line_number, level, text) for every ATX heading."""
    out = []
    for m in re.finditer(r"^(#{1,6})([^\n]*)", text, re.MULTILINE):
        out.append((text[: m.start()].count("\n") + 1,
                    len(m.group(1)), m.group(2).strip()))
    return out


def check(en_path: Path = README_EN, zh_path: Path = README_ZH) -> int:
    for p in (en_path, zh_path):
        if not p.is_file():
            print(f"ERROR: {p} not found", file=sys.stderr)
            return 1
    en = en_path.read_text(encoding="utf-8")
    zh = zh_path.read_text(encoding="utf-8")
    en_levels = heading_levels(en)
    zh_levels = heading_levels(zh)

    print(f"{en_path.name}: {len(en_levels)} headings")
    print(f"{zh_path.name}: {len(zh_levels)} headings")

    if en_levels == zh_levels:
        print("heading sequences match (count and level order)")
        return 0

    # Point at the first divergence for an actionable message.
    en_lines = heading_lines(en)
    zh_lines = heading_lines(zh)
    for i, ((en_ln, en_lv, en_tx), (zh_ln, zh_lv, zh_tx)) in enumerate(
            zip(en_lines, zh_lines)):
        if en_lv != zh_lv:
            print(
                f"first divergence at heading #{i + 1}: "
                f"{en_path.name}:L{en_ln} is {'#' * en_lv} {en_tx!r}, "
                f"{zh_path.name}:L{zh_ln} is {'#' * zh_lv} {zh_tx!r}",
                file=sys.stderr,
            )
            break
    print("heading sequences differ — mirror the section structure in both files",
          file=sys.stderr)
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Check README.md / README.zh-CN.md heading-structure sync.")
    ap.add_argument("--check", action="store_true",
                    help="accepted no-op flag for CI symmetry")
    args = ap.parse_args()
    return check()


if __name__ == "__main__":
    sys.exit(main())
