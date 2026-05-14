#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


FUNC_RE = re.compile(r"^function\s+(?P<name>\S+)\s+params=")
BLOCK_RE = re.compile(r"^\s+bb\.(?P<id>\d+):$")
LOAD_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_local\s+local\.(?P<id>\d+)$")
ADDR_GLOBAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*addr_global\s+global\.(?P<id>\d+)$")
COPY_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*(?:copy|materialize_imm)\s+(?P<src>.+)$")
ALUI_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*alui\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<imm>-?\d+)$")
ALU_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*alu\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<rhs>\S+)$")
LOAD_INDIRECT_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_indirect\s+(?P<addr>\S+)$")
STORE_INDIRECT_RE = re.compile(r"^\s+store_indirect\s+(?P<addr>\S+),\s*(?P<value>\S+)$")


def is_int(text: str) -> bool:
    return re.fullmatch(r"-?\d+", text) is not None


def describe_value(block_lines: list[str], use_index: int, value: str, depth: int = 0) -> str | None:
    if depth > 12:
        return None
    if is_int(value):
        return f"imm:{value}"

    for scan in range(use_index - 1, -1, -1):
        line = block_lines[scan]

        m = LOAD_LOCAL_RE.match(line)
        if m and m.group("dst") == value:
            return f"local:{m.group('id')}"

        m = ADDR_GLOBAL_RE.match(line)
        if m and m.group("dst") == value:
            return f"addr_global:{m.group('id')}"

        m = COPY_RE.match(line)
        if m and m.group("dst") == value:
            return describe_value(block_lines, scan, m.group("src").strip(), depth + 1)

        m = ALUI_RE.match(line)
        if m and m.group("dst") == value:
            lhs_desc = describe_value(block_lines, scan, m.group("lhs"), depth + 1)
            if lhs_desc is None:
                return None
            return f"alui.{m.group('op')}({lhs_desc},{m.group('imm')})"

        m = ALU_RE.match(line)
        if m and m.group("dst") == value:
            lhs_desc = describe_value(block_lines, scan, m.group("lhs"), depth + 1)
            rhs_desc = describe_value(block_lines, scan, m.group("rhs"), depth + 1)
            if lhs_desc is None or rhs_desc is None:
                return None
            return f"alu.{m.group('op')}({lhs_desc},{rhs_desc})"

        m = LOAD_INDIRECT_RE.match(line)
        if m and m.group("dst") == value:
            addr_desc = describe_value(block_lines, scan, m.group("addr"), depth + 1)
            if addr_desc is None:
                return None
            return f"load_indirect({addr_desc})"

    return None


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_row_base_carriers.py <machine_select_dump>")
        return 2

    lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    current_function = "<unknown>"
    current_block = "<unknown>"
    block_lines: list[str] = []

    carrier_counts = Counter()
    carrier_samples = defaultdict(list)

    def flush_block() -> None:
        if not block_lines:
            return

        for idx, line in enumerate(block_lines):
            m = ALUI_RE.match(line)
            if not m or m.group("op") != "2" or m.group("imm") != "4096":
                continue

            carrier = m.group("dst")
            carrier_root = describe_value(block_lines, idx, m.group("lhs")) or m.group("lhs")
            uses = []

            for scan in range(idx + 1, len(block_lines)):
                body = block_lines[scan]
                m_alu = ALU_RE.match(body)
                if m_alu and m_alu.group("op") == "0" and (
                    m_alu.group("lhs") == carrier or m_alu.group("rhs") == carrier
                ):
                    uses.append(body.strip())
                    continue

                m_load = LOAD_INDIRECT_RE.match(body)
                if m_load and m_load.group("addr") == carrier:
                    uses.append(body.strip())
                    continue

                m_store = STORE_INDIRECT_RE.match(body)
                if m_store and m_store.group("addr") == carrier:
                    uses.append(body.strip())
                    continue

            if len(uses) < 2:
                continue

            key = (current_function, current_block, carrier_root)
            carrier_counts[key] += len(uses)
            if len(carrier_samples[key]) < 6:
                carrier_samples[key].append(f"carrier: {line.strip()}")
                carrier_samples[key].extend(f"use: {u}" for u in uses[:3])

    for raw in lines:
        m = FUNC_RE.match(raw)
        if m:
            flush_block()
            block_lines = []
            current_function = m.group("name")
            current_block = "<unknown>"
            continue

        m = BLOCK_RE.match(raw)
        if m:
            flush_block()
            block_lines = []
            current_block = f"bb.{m.group('id')}"
            continue

        if raw.startswith("  "):
            block_lines.append(raw.strip())

    flush_block()

    print("row_base_carriers")
    if not carrier_counts:
        print("<none>")
        return 0

    for (func, block, root), count in carrier_counts.most_common():
        print(f"{func} {block} root={root} dependent_uses={count}")
        for sample in carrier_samples[(func, block, root)]:
            print(f"  {sample}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
