#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


LINE_RE = re.compile(r"^\s+(?P<body>.+)$")
ADDR_LOCAL_RE = re.compile(r"^(?P<dst>\S+)\s*=\s*addr_local\s+local\.(?P<slot>\d+)$")
IMM_RE = re.compile(r"^(?P<dst>\S+)\s*=\s*imm\s+(?P<imm>-?\d+)$")
ALU_RE = re.compile(r"^(?P<dst>\S+)\s*=\s*alu\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<rhs>\S+)$")
ALUI_RE = re.compile(r"^(?P<dst>\S+)\s*=\s*alui\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<imm>-?\d+)$")
LOAD_INDIRECT_RE = re.compile(r"^(?P<dst>\S+)\s*=\s*load_indirect\s+(?P<addr>\S+)$")
STORE_INDIRECT_RE = re.compile(r"^store_indirect\s+(?P<addr>\S+),\s*(?P<value>\S+)$")
BLOCK_RE = re.compile(r"^\s*(?P<label>F\d+\.L\d+):")


def load_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8").splitlines()


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_emit_addr_local.py <machine_emit_dump>")
        return 2

    path = Path(sys.argv[1])
    lines = load_lines(path)

    block = "<unknown>"
    zero_offset_counter: Counter[str] = Counter()
    scaled_index_counter: Counter[str] = Counter()
    root_use_counter: Counter[str] = Counter()
    witness_lines: defaultdict[str, list[str]] = defaultdict(list)

    for i, raw in enumerate(lines):
        block_match = BLOCK_RE.match(raw)
        if block_match:
            block = block_match.group("label")
            continue

        line_match = LINE_RE.match(raw)
        if not line_match:
            continue
        line = line_match.group("body")

        addr_match = ADDR_LOCAL_RE.match(line)
        if not addr_match:
            continue

        base_reg = addr_match.group("dst")
        slot = addr_match.group("slot")

        if i + 2 < len(lines):
            next1 = LINE_RE.match(lines[i + 1])
            next2 = LINE_RE.match(lines[i + 2])
            if next1 and next2:
                imm_match = IMM_RE.match(next1.group("body"))
                alu_match = ALU_RE.match(next2.group("body"))
                if (imm_match and alu_match and imm_match.group("imm") == "0" and
                        alu_match.group("op") == "0" and
                        alu_match.group("lhs") == base_reg and
                        alu_match.group("rhs") == imm_match.group("dst")):
                    key = f"local.{slot}"
                    zero_offset_counter[key] += 1
                    witness_lines[key].append(f"{block}: {line} | {next1.group('body')} | {next2.group('body')}")

        for j in range(i + 1, min(i + 5, len(lines))):
            inner = LINE_RE.match(lines[j])
            if not inner:
                continue
            body = inner.group("body")
            alui_match = ALUI_RE.match(body)
            alu_match = ALU_RE.match(body)
            load_match = LOAD_INDIRECT_RE.match(body)
            store_match = STORE_INDIRECT_RE.match(body)

            if alui_match and alui_match.group("lhs") == base_reg and alui_match.group("op") == "2":
                scaled_index_counter[f"local.{slot}"] += 1
            if alu_match and alu_match.group("op") == "0" and (
                    alu_match.group("lhs") == base_reg or alu_match.group("rhs") == base_reg):
                root_use_counter[f"local.{slot}"] += 1
            if load_match and load_match.group("addr") == base_reg:
                root_use_counter[f"local.{slot}"] += 1
            if store_match and store_match.group("addr") == base_reg:
                root_use_counter[f"local.{slot}"] += 1

    print("zero_offset_roots")
    for key, count in zero_offset_counter.most_common():
        print(f"{key} {count}")

    print("scaled_index_roots")
    for key, count in scaled_index_counter.most_common():
        print(f"{key} {count}")

    print("root_use_counts")
    for key, count in root_use_counter.most_common():
        print(f"{key} {count}")

    print("sample_witnesses")
    for key, samples in witness_lines.items():
        for sample in samples[:5]:
            print(sample)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
