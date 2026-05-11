#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Case:
    name: str
    seed: int
    shape: str
    tail: str


def run_cmd(args: list[str], *, timeout_s: int = 30) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout_s,
        check=False,
    )


def build_host_oracle(source_text: str) -> str:
    wrapper = (
        "#include <stdio.h>\n"
        "int putint(int x) { printf(\"%d\", x); return 0; }\n"
        "int putch(int x) { putchar(x); return 0; }\n"
    )
    with tempfile.TemporaryDirectory(prefix="sysy-fold-probe-") as td:
        td = Path(td)
        src = td / "case.c"
        exe = td / "case.out"
        src.write_text(wrapper + source_text, encoding="utf-8")
        cp = run_cmd(["cc", "-O0", str(src), "-o", str(exe)])
        if cp.returncode != 0 or not exe.exists():
            raise RuntimeError(cp.stderr)
        rp = run_cmd([str(exe)])
        if rp.returncode != 0:
            raise RuntimeError(rp.stderr)
        return rp.stdout


def emit_header(seed: int) -> list[str]:
    return [
        f"int g0 = {seed % 7 + 1};",
        f"int g1 = {seed % 11 + 2};",
        f"int g2 = {seed % 13 + 3};",
        "int garr[8];",
        "",
        "int side(int x) {",
        f"  g1 = g1 + x + {seed % 5 + 1};",
        "  return g1 + x;",
        "}",
        "",
    ]


def emit_tail(tail: str, seed: int) -> list[str]:
    if tail == "local":
        return [
            "  return x + y + z + local0;",
        ]
    if tail == "global":
        return [
            "  return x + y + z + g0;",
        ]
    if tail == "array":
        return [
            "  return x + y + z + slots[idx];",
        ]
    if tail == "index":
        return [
            "  idx = (idx + x + y + z) & 7;",
            "  return slots[idx] + g0;",
        ]
    if tail == "call":
        return [
            "  return side(x + y + z) + g0;",
        ]
    if tail == "global_idx":
        return [
            "  gidx = (gidx + x + y + z) & 7;",
            "  return garr[gidx] + g1;",
        ]
    if tail == "helper_store":
        return [
            "  g0 = g0 + x + y + z + 1;",
            "  return g0 + side(local0);",
        ]
    if tail == "array_global":
        return [
            "  slots[idx] = slots[idx] + g0 + x;",
            "  return slots[idx] + g1;",
        ]
    return [
        f"  return x + y + z + {seed % 17 + 1};",
    ]


def emit_shape(shape: str, seed: int) -> list[str]:
    if shape == "if_true_local":
        return [
            "  if (1) {",
            f"    local0 = local0 + x + {seed % 3 + 1};",
            "  }",
        ]
    if shape == "if_true_global":
        return [
            "  if ((1 + 0) == 1) {",
            f"    g0 = g0 + x + {seed % 5 + 2};",
            "  }",
        ]
    if shape == "if_false_else_global":
        return [
            "  if ((0 + 1) != 1) {",
            f"    g0 = g0 + x + {seed % 5 + 2};",
            "  } else {",
            f"    g1 = g1 + y + {seed % 7 + 3};",
            "  }",
        ]
    if shape == "if_const_cmp_local":
        return [
            "  if (((x - x) == 0)) {",
            f"    local0 = local0 + y + {seed % 7 + 1};",
            "  }",
        ]
    if shape == "if_const_cmp_global":
        return [
            "  if (((y - y) != 1)) {",
            f"    g0 = g0 + y + {seed % 7 + 1};",
            "  }",
        ]
    if shape == "short_circuit_side":
        return [
            "  if (((1 && side(y)) || 0)) {",
            f"    g0 = g0 + side(x) + {seed % 9 + 1};",
            "  }",
        ]
    if shape == "nested_const":
        return [
            "  if (1) {",
            "    if (1) {",
            f"      g0 = g0 + x + {seed % 11 + 1};",
            "    }",
            "  }",
        ]
    if shape == "sequential_if":
        return [
            "  if (1) {",
            f"    local0 = local0 + x + {seed % 3 + 1};",
            "  }",
            "  if (((x - x) == 0)) {",
            f"    g0 = g0 + y + {seed % 5 + 2};",
            "  }",
            "  if (((y - y) != 1)) {",
            f"    g1 = g1 + z + {seed % 7 + 3};",
            "  }",
        ]
    if shape == "address_idx":
        return [
            "  if (1) {",
            f"    idx = (idx + x + {seed % 5 + 1}) & 7;",
            "  }",
        ]
    if shape == "global_idx":
        return [
            "  if (1) {",
            f"    gidx = (gidx + y + {seed % 7 + 2}) & 7;",
            "  }",
        ]
    if shape == "call_chain":
        return [
            "  if (1) {",
            "    local0 = side(side(x + y));",
            "  }",
        ]
    if shape == "store_then_branch":
        return [
            f"  g0 = g0 + x + {seed % 5 + 1};",
            "  if (1) {",
            f"    g1 = g1 + y + {seed % 7 + 2};",
            "  }",
        ]
    return ["  if (1) { g0 = g0 + 1; }"]


def emit_source(case: Case) -> str:
    seed = case.seed
    lines = emit_header(seed)
    lines.extend(
        [
            "int solve(int a, int b, int c) {",
            "  int slots[8];",
            "  int x;",
            "  int y;",
            "  int z;",
            "  int idx;",
            "  int gidx;",
            "  int local0;",
            f"  x = a + {seed % 19 + 3};",
            f"  y = b + {seed % 23 + 5};",
            f"  z = c + {seed % 29 + 7};",
            "  idx = (a + b + c + g0) & 7;",
            "  gidx = (a + b + c + g1) & 7;",
            "  local0 = x + y + z + g2;",
            "  slots[idx] = local0 + g0;",
            "  garr[gidx] = g0 + x;",
        ]
    )
    lines.extend(emit_shape(case.shape, seed))
    lines.extend(
        [
            f"  slots[idx] = slots[idx] + local0 + {seed % 11 + 4};",
            "  if (((x + y + z + g0) & 1) == 0) {",
            "    local0 = local0 + slots[idx];",
            "  } else {",
            "    local0 = local0 + g1;",
            "  }",
        ]
    )
    lines.extend(emit_tail(case.tail, seed))
    lines.extend(["}", "", "int main() {", f"  int ans = solve({seed % 5 + 1}, {seed % 7 + 2}, {seed % 9 + 3});", "  putint(ans);", "  putch(10);", "  return 0;", "}", ""])
    return "\n".join(lines)


def make_cases(seed: int) -> list[Case]:
    shapes = [
        "if_true_local",
        "if_true_global",
        "if_false_else_global",
        "if_const_cmp_local",
        "if_const_cmp_global",
        "short_circuit_side",
        "nested_const",
        "sequential_if",
        "address_idx",
        "global_idx",
        "call_chain",
        "store_then_branch",
    ]
    tails = ["local", "global", "array", "index", "call", "global_idx", "helper_store", "array_global"]
    cases: list[Case] = []
    idx = 0
    for shape in shapes:
        for tail in tails:
            cases.append(Case(name=f"foldprobe_{idx:02d}_{shape}_{tail}", seed=seed + idx * 19, shape=shape, tail=tail))
            idx += 1
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate side-effect fold probe SysY programs.")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=37)
    args = parser.parse_args()

    out_dir = args.output_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = make_cases(args.seed)
    for case in cases:
        source = emit_source(case)
        expected = build_host_oracle(source)
        (out_dir / f"{case.name}.sy").write_text(source, encoding="utf-8")
        (out_dir / f"{case.name}.out").write_text(expected, encoding="utf-8")
        print(f"wrote {case.name}.sy")
    print(f"generated {len(cases)} cases in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
