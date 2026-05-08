#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path


LINE_RE = re.compile(r"^\s+(?P<body>.+)$")
BLOCK_RE = re.compile(r"^\s*bb\.(?P<id>\d+):")
ADDR_LOCAL_RE = re.compile(r"^(?P<dst>spill\.\d+|reg\.\d+)\s*=\s*addr_local\s+local\.(?P<slot>\d+)$")
ADDR_GLOBAL_RE = re.compile(r"^(?P<dst>spill\.\d+|reg\.\d+)\s*=\s*addr_global\s+global\.(?P<slot>\d+)$")
SPILL_TOKEN_RE = re.compile(r"\bspill\.(?P<id>\d+)\b")


def load_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8").splitlines()


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_remat_spills.py <machine_select_dump>")
        return 2

    path = Path(sys.argv[1])
    lines = load_lines(path)

    current_block = "<unknown>"
    defs: dict[tuple[str, str], dict[str, object]] = {}
    uses: defaultdict[tuple[str, str], list[str]] = defaultdict(list)

    for raw in lines:
        block_match = BLOCK_RE.match(raw)
        if block_match:
            current_block = f"bb.{block_match.group('id')}"
            continue

        line_match = LINE_RE.match(raw)
        if not line_match:
            continue
        body = line_match.group("body")

        local_match = ADDR_LOCAL_RE.match(body)
        global_match = ADDR_GLOBAL_RE.match(body)

        if local_match:
            key = (current_block, local_match.group("dst"))
            defs[key] = {
                "kind": "local",
                "slot": local_match.group("slot"),
                "line": body,
            }
            continue
        if global_match:
            key = (current_block, global_match.group("dst"))
            defs[key] = {
                "kind": "global",
                "slot": global_match.group("slot"),
                "line": body,
            }
            continue

        for token_match in SPILL_TOKEN_RE.finditer(body):
            spill_name = f"spill.{token_match.group('id')}"
            key = (current_block, spill_name)
            if key in defs:
                uses[key].append(body)

    print("rematerializable_spill_candidates")
    for key, info in sorted(defs.items()):
        block, spill = key
        use_lines = uses.get(key, [])
        if not use_lines:
            continue
        print(f"{block} {spill} {info['kind']}.{info['slot']} uses={len(use_lines)}")
        print(f"  def: {info['line']}")
        for use in use_lines[:8]:
            print(f"  use: {use}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
