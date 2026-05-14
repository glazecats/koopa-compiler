#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path


FUNC_RE = re.compile(r"^function\s+(?P<name>\S+)\s+params=")
BLOCK_RE = re.compile(r"^\s+bb\.(?P<id>\d+):$")
JMP_RE = re.compile(r"^\s+jmp\s+bb\.(?P<target>\d+)$")
CMPBR_RE = re.compile(r"^\s+cmpbr(?:i)?\.\d+\s+.+,\s+bb\.(?P<then>\d+),\s+bb\.(?P<else>\d+)$")
BR_RE = re.compile(r"^\s+br\s+.+,\s+bb\.(?P<then>\d+),\s+bb\.(?P<else>\d+)$")
LOAD_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_local\s+local\.(?P<slot>\d+)$")
STORE_LOCAL_RE = re.compile(r"^\s+store_local(?:i)?\s+local\.(?P<slot>\d+),\s*(?P<value>\S+)$")


def parse_blocks(lines: list[str]):
    current_function = None
    current_block = None
    blocks: dict[tuple[str, int], list[str]] = {}
    order: list[tuple[str, int]] = []

    for raw in lines:
        m = FUNC_RE.match(raw)
        if m:
            current_function = m.group("name")
            current_block = None
            continue

        m = BLOCK_RE.match(raw)
        if m and current_function is not None:
            current_block = (current_function, int(m.group("id")))
            blocks[current_block] = []
            order.append(current_block)
            continue

        if current_block is not None and raw.startswith("    "):
            blocks[current_block].append(raw.strip())

    return order, blocks


def parse_successors(ops: list[str]) -> list[int]:
    if not ops:
        return []
    term = ops[-1]
    m = JMP_RE.match(term)
    if m:
        return [int(m.group("target"))]
    m = CMPBR_RE.match(term) or BR_RE.match(term)
    if m:
        return [int(m.group("then")), int(m.group("else"))]
    return []


def build_preds(order, blocks):
    preds: dict[tuple[str, int], list[int]] = defaultdict(list)
    for func, block_id in order:
        for succ in parse_successors(blocks[(func, block_id)]):
            preds[(func, succ)].append(block_id)
    return preds


def first_head_loads(ops: list[str], budget: int = 4) -> list[tuple[int, int, str]]:
    result = []
    for idx, op in enumerate(ops[:budget]):
        m = LOAD_LOCAL_RE.match(op)
        if m:
            result.append((idx, int(m.group("slot")), m.group("dst")))
    return result


def trailing_stores(ops: list[str], budget: int = 4) -> list[tuple[int, int, str]]:
    result = []
    start = max(0, len(ops) - 1 - budget)
    for idx in range(start, len(ops) - 1):
        m = STORE_LOCAL_RE.match(ops[idx])
        if m:
            result.append((idx, int(m.group("slot")), m.group("value")))
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_loop_phi_slots.py <machine_select_dump>")
        return 2

    lines = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    order, blocks = parse_blocks(lines)
    preds = build_preds(order, blocks)

    print("loop_phi_slot_candidates")
    found = False

    for func, block_id in order:
        key = (func, block_id)
        head_loads = first_head_loads(blocks[key])
        if len(preds[key]) < 2 or not head_loads:
            continue

        pred_store_map: dict[int, list[tuple[int, int, str]]] = {}
        for pred_id in preds[key]:
            pred_key = (func, pred_id)
            pred_store_map[pred_id] = trailing_stores(blocks[pred_key])

        for load_index, slot, dst in head_loads:
            matching_preds = []
            for pred_id, stores in pred_store_map.items():
                for store_index, store_slot, value in stores:
                    if store_slot == slot:
                        matching_preds.append((pred_id, store_index, value))
                        break

            if len(matching_preds) < 2:
                continue

            found = True
            print(f"{func} bb.{block_id} head_load=local.{slot} -> {dst}")
            print(f"  head_load_index: {load_index}")
            for pred_id, store_index, value in matching_preds:
                print(f"  pred bb.{pred_id} trailing_store_index={store_index} value={value}")
            print()

    if not found:
        print("<none>")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
