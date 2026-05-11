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
    with tempfile.TemporaryDirectory(prefix="sysy-lv8-probe-") as td:
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
        f"int g3 = {seed % 17 + 4};",
        f"int moves = {seed % 5};",
        "",
        "int tick(int x) {",
        f"  g0 = g0 + x + {seed % 3 + 1};",
        "  return (g0 & 1) != 0;",
        "}",
        "",
        "int side(int x) {",
        f"  g1 = g1 + x + {seed % 5 + 2};",
        "  return g1 + x;",
        "}",
        "",
        "int mix(int a, int b, int c) {",
        f"  g2 = g2 + a + b + c + {seed % 7 + 3};",
        "  return g2 + a + b + c;",
        "}",
        "",
        "void move(int x, int y) {",
        f"  moves = moves + x + y + {seed % 3 + 1};",
        "  g3 = g3 + x + y;",
        "}",
        "",
        "void hanoi(int n, int one, int two, int three) {",
        "  if (n == 1) {",
        "    move(one, three);",
        "  } else {",
        "    hanoi(n - 1, one, three, two);",
        "    move(one, three);",
        "    hanoi(n - 1, two, one, three);",
        "  }",
        "}",
        "",
    ]


def emit_shape(shape: str, seed: int) -> list[str]:
    if shape == "short_circuit":
        return [
            f"  if (((tick(a) && side(b)) || (!tick(c) && side(a)))) {{",
            f"    g1 = g1 + x + {seed % 5 + 1};",
            "  } else {",
            f"    g2 = g2 + y + {seed % 7 + 2};",
            "  }",
        ]
    if shape == "func_expr":
        return [
            f"  x = mix(side(a), side(b), tick(c)) + side(d);",
            f"  if ((mix(tick(a), side(b), tick(c)) & 1) == 0) {{",
            f"    g0 = g0 + x + {seed % 11 + 3};",
            "  }",
        ]
    if shape == "global_shadow":
        return [
            "  {",
            "    int g0;",
            f"    g0 = a + {seed % 3 + 1};",
            f"    if (((g0 + b + {seed % 5 + 1}) & 1) == 0) {{",
            f"      g1 = g1 + g0 + {seed % 7 + 2};",
            "    } else {",
            f"      g2 = g2 + g0 + {seed % 11 + 3};",
            "    }",
            "  }",
            f"  if (((g0 + b + {seed % 5 + 2}) & 1) == 0) {{",
            f"    g1 = g1 + a + {seed % 7 + 2};",
            "  } else {",
            f"    g2 = g2 + c + {seed % 11 + 3};",
            "  }",
        ]
    if shape == "hanoi":
        return [
            f"  hanoi((a & 3) + 1, 1, 2, 3);",
            f"  if (((moves + a + {seed % 5 + 1}) & 1) == 0) {{",
            f"    g3 = g3 + moves + {seed % 7 + 2};",
            "  } else {",
            f"    g1 = g1 + moves + {seed % 11 + 3};",
            "  }",
        ]
    return ["  g0 = g0 + 1;"]


def emit_tail(tail: str, seed: int) -> list[str]:
    if tail == "local":
        return [
            "  return x + y + z + g0 + g1 + g2 + g3;",
        ]
    if tail == "global":
        return [
            "  g0 = g0 + x + y + z;",
            "  return g0 + g1 + g2 + g3;",
        ]
    if tail == "call":
        return [
            "  return mix(x, y, z) + side(x);",
        ]
    if tail == "reload":
        return [
            "  g1 = g1 + x + y;",
            "  g2 = g2 + z + g1;",
            "  return g0 + g1 + g2 + g3;",
        ]
    if tail == "branch":
        return [
            f"  if (((x + y + z + {seed}) & 1) == 0) {{",
            "    g3 = g3 + x + y;",
            "  } else {",
            "    g3 = g3 + z + x;",
            "  }",
            "  return g0 + g1 + g2 + g3;",
        ]
    return [
        f"  return x + y + z + {seed % 17 + 1};",
    ]


def emit_source(case: Case) -> str:
    seed = case.seed
    lines = emit_header(seed)
    lines.extend(
        [
            "int solve(int a, int b, int c, int d) {",
            "  int x;",
            "  int y;",
            "  int z;",
            "  x = a + g0;",
            "  y = b + g1;",
            "  z = c + d + g2;",
        ]
    )
    lines.extend(emit_shape(case.shape, seed))
    lines.extend(
        [
            f"  if (((x + y + z + g0 + {seed % 9 + 1}) & 1) == 0) {{",
            f"    g3 = g3 + x + {seed % 7 + 2};",
            "  } else {",
            f"    g0 = g0 + y + {seed % 5 + 3};",
            "  }",
            "  x = x + g0 + g1;",
            "  y = y + g2 + g3;",
            "  z = z + x + y;",
        ]
    )
    lines.extend(emit_tail(case.tail, seed))
    lines.extend(["}", "", "int main() {", f"  int ans = solve({seed % 5 + 1}, {seed % 7 + 2}, {seed % 9 + 3}, {seed % 11 + 4});", "  putint(ans);", "  putch(10);", "  return 0;", "}", ""])
    return "\n".join(lines)


def make_cases(seed: int) -> list[Case]:
    shapes = ["short_circuit", "func_expr", "global_shadow", "hanoi"]
    tails = ["local", "global", "call", "reload", "branch"]
    cases: list[Case] = []
    idx = 0
    for shape in shapes:
        for tail in tails:
            cases.append(Case(name=f"lv8probe_{idx:02d}_{shape}_{tail}", seed=seed + idx * 23, shape=shape, tail=tail))
            idx += 1
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate lv8 no-array probe programs.")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=41)
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
