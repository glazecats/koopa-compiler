#!/usr/bin/env python3

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


REPEATS_RE = re.compile(
    r"^repeats=(?P<count>\d+)\s+"
    r"(?:(?P<kind>(?:addr|expr))=)?(?P<desc>.+?)\s+"
    r"blocker=(?P<blocker>.+)$"
)
SAMPLE_HEAD_RE = re.compile(r"^(?P<func>\S+)\s+(?P<desc>.+)$")


def categorize_blocker(text: str) -> str:
    if text == "<none>":
        return "no_blocker"
    if text.startswith("call:") or text.startswith("call_void:"):
        return "call_barrier"
    if text.startswith("store_indirect:"):
        return "indirect_store_barrier"
    if text.startswith("store_local:") or text.startswith("store_global:"):
        return "direct_store_barrier"
    if "load_local" in text:
        return "reload_same_local"
    if "load_global" in text:
        return "reload_same_global"
    if "addr_local" in text or "addr_global" in text:
        return "rebuild_address_root"
    if "alui." in text or "alu." in text or "cmp." in text or "cmpi." in text:
        return "rebuild_small_expr"
    if "load_indirect" in text:
        return "nested_load_indirect"
    return "other"


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: classify_machine_select_hotspots.py "
            "<expr_or_addr_blocker_report> <kind:expr|addr>"
        )
        return 2

    report = Path(sys.argv[1]).read_text(encoding="utf-8").splitlines()
    expected_kind = sys.argv[2]
    if expected_kind not in {"expr", "addr"}:
        print("kind must be expr or addr")
        return 2

    summary: Counter[tuple[str, str]] = Counter()
    per_desc: defaultdict[str, Counter[str]] = defaultdict(Counter)
    samples: defaultdict[str, list[str]] = defaultdict(list)
    current_sample_desc: str | None = None

    for raw in report:
        m = REPEATS_RE.match(raw)
        if m:
            kind = m.group("kind") or expected_kind
            if kind != expected_kind:
                continue
            desc = m.group("desc")
            blocker = m.group("blocker")
            bucket = categorize_blocker(blocker)
            count = int(m.group("count"))
            summary[(desc, bucket)] += count
            per_desc[desc][bucket] += count
            current_sample_desc = None
            continue

        m = SAMPLE_HEAD_RE.match(raw)
        if m and not raw.startswith("  "):
            current_sample_desc = m.group("desc")
            continue

        if raw.startswith("  ") and current_sample_desc is not None:
            if len(samples[current_sample_desc]) < 4:
                samples[current_sample_desc].append(raw.strip())

    print("hotspot_categories")
    for desc, counter in per_desc.items():
        cats = ", ".join(f"{name}={count}" for name, count in counter.most_common())
        print(f"{desc}: {cats}")

    print("raw_category_totals")
    totals: Counter[str] = Counter()
    for (_, bucket), count in summary.items():
        totals[bucket] += count
    for bucket, count in totals.most_common():
        print(f"{bucket} {count}")

    print("samples")
    for desc, lines in samples.items():
        print(desc)
        for line in lines:
            print(f"  {line}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
