#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


FUNC_RE = re.compile(r"^function\s+(?P<name>\S+)\s+params=")
BLOCK_RE = re.compile(r"^\s+bb\.(?P<id>\d+):$")
ADDR_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*addr_local\s+local\.(?P<id>\d+)$")
ADDR_GLOBAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*addr_global\s+global\.(?P<id>\d+)$")
LOAD_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_local\s+local\.(?P<id>\d+)$")
LOAD_GLOBAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_global\s+global\.(?P<id>\d+)$")
COPY_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*(?:copy|materialize_imm)\s+(?P<src>.+)$")
ALUI_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*alui\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<imm>-?\d+)$")
ALU_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*alu\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<rhs>\S+)$")
CMP_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*cmp\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<rhs>\S+)$")
CMPI_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*cmpi\.(?P<op>\d+)\s+(?P<lhs>\S+),\s*(?P<imm>-?\d+)$")
CALL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*calli?\s+(?P<callee>\S+)\((?P<args>.*)\)$")
CALL_VOID_RE = re.compile(r"^\s+call_voidi?\s+(?P<callee>\S+)\((?P<args>.*)\)$")
STORE_LOCAL_RE = re.compile(r"^\s+store_local(?:i)?\s+local\.(?P<id>\d+),\s*(?P<value>\S+)$")
STORE_GLOBAL_RE = re.compile(r"^\s+store_global(?:i)?\s+global\.(?P<id>\d+),\s*(?P<value>\S+)$")
STORE_INDIRECT_RE = re.compile(r"^\s+store_indirect\s+(?P<addr>\S+),\s*(?P<value>\S+)$")
LOAD_INDIRECT_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_indirect\s+(?P<addr>\S+)$")


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

        m = LOAD_GLOBAL_RE.match(line)
        if m and m.group("dst") == value:
            return f"global:{m.group('id')}"

        m = ADDR_LOCAL_RE.match(line)
        if m and m.group("dst") == value:
            return f"addr_local:{m.group('id')}"

        m = ADDR_GLOBAL_RE.match(line)
        if m and m.group("dst") == value:
            return f"addr_global:{m.group('id')}"

        m = COPY_RE.match(line)
        if m and m.group("dst") == value:
            src = m.group("src").strip()
            if is_int(src):
                return f"imm:{src}"
            return describe_value(block_lines, scan, src, depth + 1)

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

        m = CMP_RE.match(line)
        if m and m.group("dst") == value:
            lhs_desc = describe_value(block_lines, scan, m.group("lhs"), depth + 1)
            rhs_desc = describe_value(block_lines, scan, m.group("rhs"), depth + 1)
            if lhs_desc is None or rhs_desc is None:
                return None
            return f"cmp.{m.group('op')}({lhs_desc},{rhs_desc})"

        m = CMPI_RE.match(line)
        if m and m.group("dst") == value:
            lhs_desc = describe_value(block_lines, scan, m.group("lhs"), depth + 1)
            if lhs_desc is None:
                return None
            return f"cmpi.{m.group('op')}({lhs_desc},{m.group('imm')})"

        m = LOAD_INDIRECT_RE.match(line)
        if m and m.group("dst") == value:
            addr_desc = describe_value(block_lines, scan, m.group("addr"), depth + 1)
            if addr_desc is None:
                return None
            return f"load_indirect({addr_desc})"

    return None


def classify_blocker(block_lines: list[str], idx: int) -> str:
    line = block_lines[idx].strip()

    m = CALL_RE.match(block_lines[idx])
    if m:
        return f"call:{m.group('callee')}"

    m = CALL_VOID_RE.match(block_lines[idx])
    if m:
        return f"call_void:{m.group('callee')}"

    m = STORE_LOCAL_RE.match(block_lines[idx])
    if m:
        return f"store_local:{m.group('id')}"

    m = STORE_GLOBAL_RE.match(block_lines[idx])
    if m:
        return f"store_global:{m.group('id')}"

    m = STORE_INDIRECT_RE.match(block_lines[idx])
    if m:
        addr_desc = describe_value(block_lines, idx, m.group("addr"))
        return f"store_indirect:{addr_desc or m.group('addr')}"

    return line


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_expr_reuse_blockers.py <machine_select_dump>")
        return 2

    lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    current_function = "<unknown>"
    current_block = "<unknown>"
    block_lines: list[str] = []
    blocker_counts: Counter[tuple[str, str]] = Counter()
    samples: defaultdict[tuple[str, str], list[str]] = defaultdict(list)

    def flush_block() -> None:
        if not block_lines:
            return

        seen_exprs: defaultdict[str, list[int]] = defaultdict(list)
        for idx, line in enumerate(block_lines):
            body = line.strip()
            if not (
                COPY_RE.match(line)
                or ALUI_RE.match(line)
                or ALU_RE.match(line)
                or CMP_RE.match(line)
                or CMPI_RE.match(line)
            ):
                continue
            dst = body.split("=", 1)[0].strip()
            desc = describe_value(block_lines, idx + 1, dst)
            if desc is not None:
                seen_exprs[desc].append(idx)

        for desc, uses in seen_exprs.items():
            if len(uses) < 2:
                continue
            for left, right in zip(uses, uses[1:]):
                blockers = [classify_blocker(block_lines, i) for i in range(left + 1, right)]
                key = (current_function, desc)
                if blockers:
                    for blocker in blockers:
                        blocker_counts[(desc, blocker)] += 1
                    if len(samples[key]) < 4:
                        samples[key].append(
                            f"{current_block} between expr#{left} and expr#{right}: " + " | ".join(blockers)
                        )
                else:
                    blocker_counts[(desc, "<none>")] += 1
                    if len(samples[key]) < 4:
                        samples[key].append(f"{current_block} between expr#{left} and expr#{right}: <none>")

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
            block_lines.append(raw)

    flush_block()

    print("expr_reuse_blockers")
    for (desc, blocker), count in blocker_counts.most_common():
        print(f"repeats={count} expr={desc} blocker={blocker}")

    print("samples")
    for (func, desc), lines_ in samples.items():
        print(f"{func} {desc}")
        for sample in lines_:
            print(f"  {sample}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
