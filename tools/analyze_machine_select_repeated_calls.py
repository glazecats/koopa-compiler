#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


FUNC_RE = re.compile(r"^function\s+(?P<name>\S+)\s+params=")
BLOCK_RE = re.compile(r"^\s+bb\.(?P<id>\d+):$")
CALL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*calli?\s+(?P<callee>\S+)\((?P<args>.*)\)$")
LOAD_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_local\s+local\.(?P<id>\d+)$")
LOAD_GLOBAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_global\s+global\.(?P<id>\d+)$")
COPY_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*(?:copy|materialize_imm)\s+(?P<src>.+)$")


def split_args(raw: str) -> list[str]:
    raw = raw.strip()
    if not raw:
        return []
    return [part.strip() for part in raw.split(",")]


def describe_register_value(block_lines: list[str], use_line_index: int, reg: str) -> str | None:
    if re.fullmatch(r"-?\d+", reg):
        return f"imm:{reg}"
    for scan in range(use_line_index - 1, -1, -1):
        line = block_lines[scan]
        m = LOAD_LOCAL_RE.match(line)
        if m and m.group("dst") == reg:
            return f"local:{m.group('id')}"
        m = LOAD_GLOBAL_RE.match(line)
        if m and m.group("dst") == reg:
            return f"global:{m.group('id')}"
        m = COPY_RE.match(line)
        if m and m.group("dst") == reg:
            src = m.group("src").strip()
            if re.fullmatch(r"-?\d+", src):
                return f"imm:{src}"
            reg = src
            continue
    return None


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_repeated_calls.py <machine_select_dump>")
        return 2

    text = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    current_function = "<unknown>"
    current_block = "<unknown>"
    block_lines: list[str] = []
    repeated: Counter[tuple[str, str, str]] = Counter()
    examples: defaultdict[tuple[str, str, str], list[str]] = defaultdict(list)

    def flush_block() -> None:
        if not block_lines:
            return
        seen: Counter[str] = Counter()
        for idx, line in enumerate(block_lines):
            m = CALL_RE.match(line)
            if not m:
                continue
            callee = m.group("callee")
            arg_descs: list[str] = []
            ok = True
            for arg in split_args(m.group("args")):
                desc = describe_register_value(block_lines, idx, arg)
                if desc is None:
                    ok = False
                    break
                arg_descs.append(desc)
            if not ok:
                continue
            signature = f"{callee}({', '.join(arg_descs)})"
            seen[signature] += 1
            if seen[signature] >= 2:
                key = (current_function, current_block, signature)
                repeated[key] += 1
                if len(examples[key]) < 4:
                    examples[key].append(line.strip())

    for raw in text:
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

    print("repeated_call_signatures")
    for (func, block, sig), count in repeated.most_common():
        print(f"{func} {block} repeats={count} {sig}")
        for sample in examples[(func, block, sig)]:
            print(f"  {sample}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
