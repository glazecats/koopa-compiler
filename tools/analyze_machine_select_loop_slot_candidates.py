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
STORE_LOCAL_RE = re.compile(r"^\s+store_local(?:i)?\s+local\.(?P<slot>\d+),\s*(?P<value>\S+)$")
LOAD_LOCAL_RE = re.compile(r"^\s+(?P<dst>\S+)\s*=\s*load_local\s+local\.(?P<slot>\d+)$")


def parse_blocks(lines: list[str]):
    current_function = None
    current_block = None
    blocks = {}
    order = []

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


def build_preds(order, blocks):
    preds = defaultdict(list)
    for key in order:
        function, block_id = key
        ops = blocks[key]
        if not ops:
            continue
        term = ops[-1]
        m = JMP_RE.match(term)
        if m:
            preds[(function, int(m.group("target")))].append(block_id)
            continue
        m = CMPBR_RE.match(term) or BR_RE.match(term)
        if m:
            preds[(function, int(m.group("then")))].append(block_id)
            preds[(function, int(m.group("else")))].append(block_id)
    return preds


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: analyze_machine_select_loop_slot_candidates.py <machine_select_dump>")
        return 2

    order, blocks = parse_blocks(Path(sys.argv[1]).read_text(encoding="utf-8").splitlines())
    preds = build_preds(order, blocks)

    print("loop_slot_candidates")
    found = False

    for key in order:
        function, block_id = key
        ops = blocks[key]
        if len(preds[(function, block_id)]) != 1:
            continue
        pred_block_id = preds[(function, block_id)][0]
        pred_ops = blocks[(function, pred_block_id)]

        pred_stores = []
        for idx, op in enumerate(pred_ops):
            m = STORE_LOCAL_RE.match(op)
            if m:
                pred_stores.append((idx, int(m.group("slot")), m.group("value"), op))

        succ_loads = []
        for idx, op in enumerate(ops):
            m = LOAD_LOCAL_RE.match(op)
            if m:
                succ_loads.append((idx, int(m.group("slot")), m.group("dst"), op))

        if not pred_stores or not succ_loads:
            continue

        for _store_idx, slot, value, store_op in pred_stores:
            for load_idx, load_slot, dst, load_op in succ_loads:
                if slot != load_slot:
                    continue
                found = True
                print(f"{function} pred=bb.{pred_block_id} succ=bb.{block_id} slot=local.{slot}")
                print(f"  pred_store: {store_op}")
                print(f"  succ_load : {load_op}")
                print(f"  succ_load_index: {load_idx}")
                print(f"  carried_value: {value}")
                print()

    if not found:
        print("<none>")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
