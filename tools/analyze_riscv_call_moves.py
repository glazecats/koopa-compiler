#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path


LINE_RE = re.compile(r"^\s+(?P<body>.+)$")
LABEL_RE = re.compile(r"^[A-Za-z0-9_.]+:\s*$")
OP_RE = re.compile(r"^(?P<op>[A-Za-z.][A-Za-z0-9_.]*)\b")


def load_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8").splitlines()


def is_instruction(line: str) -> bool:
    if not line:
        return False
    if line.lstrip().startswith("."):
        return False
    if LABEL_RE.match(line.strip()):
        return False
    return OP_RE.match(line.strip()) is not None


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_riscv_call_moves.py <assembly.s>")
        return 2

    path = Path(sys.argv[1])
    lines = load_lines(path)

    instruction_indices: list[int] = []
    instructions: list[str] = []
    for index, raw in enumerate(lines):
        if not is_instruction(raw):
            continue
        instruction_indices.append(index)
        instructions.append(raw.strip())

    pattern_counts: Counter[str] = Counter()
    print("call_move_clusters")
    for inst_pos, raw in enumerate(instructions):
        op_match = OP_RE.match(raw)
        if not op_match:
            continue
        op = op_match.group("op")
        if op not in {"call", "call_void", "calli", "call_voidi"}:
            continue

        before: list[str] = []
        i = inst_pos - 1
        while i >= 0 and len(before) < 8:
            prev = instructions[i]
            prev_op = OP_RE.match(prev)
            if not prev_op:
                break
            if prev_op.group("op") in {"call", "call_void", "calli", "call_voidi", "ret", "retspill", "jmp", "bge", "blt", "beq", "bne", "bgt", "ble", "br", "cmpbr", "cmpbri", "cmpbrft", "cmpbrift"}:
                break
            before.append(prev)
            i -= 1
        before.reverse()

        after: list[str] = []
        j = inst_pos + 1
        while j < len(instructions) and len(after) < 8:
            nxt = instructions[j]
            nxt_op = OP_RE.match(nxt)
            if not nxt_op:
                break
            if nxt_op.group("op") in {"call", "call_void", "calli", "call_voidi", "ret", "retspill", "jmp", "bge", "blt", "beq", "bne", "bgt", "ble", "br", "cmpbr", "cmpbri", "cmpbrft", "cmpbrift"}:
                break
            after.append(nxt)
            j += 1

        pattern = " | ".join(before + [raw] + after)
        pattern_counts[pattern] += 1
        print(f"line {instruction_indices[inst_pos] + 1}:")
        print(f"  before_mv={sum(1 for x in before if x.startswith('mv '))}")
        print(f"  after_mv={sum(1 for x in after if x.startswith('mv '))}")
        print(f"  call={raw}")
        for item in before:
            print(f"    {item}")
        for item in after:
            print(f"    {item}")

    if pattern_counts:
        print("top_patterns")
        for pattern, count in pattern_counts.most_common(10):
            print(f"{count} {pattern}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
