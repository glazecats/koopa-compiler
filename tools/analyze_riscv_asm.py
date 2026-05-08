#!/usr/bin/env python3

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize opcode frequencies in generated RISC-V assembly.")
    parser.add_argument("asm", type=Path)
    args = parser.parse_args()

    text = args.asm.read_text(encoding="utf-8")
    counts: Counter[str] = Counter()

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith(".") or line.endswith(":"):
            continue
        opcode = line.split()[0]
        counts[opcode] += 1

    total = sum(counts.values())
    print(f"total_instructions={total}")
    for opcode, count in counts.most_common():
        print(f"{opcode} {count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
