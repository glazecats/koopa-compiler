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
    branch_kind: str
    tail_kind: str


def run_cmd(args: list[str], *, timeout_s: int = 30, stdin_bytes: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        args,
        input=stdin_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_s,
        check=False,
    )


def build_host_oracle(source_text: str) -> str:
    wrapper = (
        "#include <stdio.h>\n"
        "int putint(int x) { printf(\"%d\", x); return 0; }\n"
        "int putch(int x) { putchar(x); return 0; }\n"
    )

    with tempfile.TemporaryDirectory(prefix="sysy-tail-probe-") as td:
        tmp = Path(td)
        src = tmp / "case.c"
        exe = tmp / "case.out"
        src.write_text(wrapper + source_text, encoding="utf-8")
        compile_proc = run_cmd(["cc", "-O0", str(src), "-o", str(exe)])
        if compile_proc.returncode != 0 or not exe.exists():
            raise RuntimeError(f"host oracle compile failed:\n{compile_proc.stderr.decode('utf-8', errors='replace')}")
        run_proc = run_cmd([str(exe)])
        if run_proc.returncode != 0:
            raise RuntimeError(f"host oracle run failed:\n{run_proc.stderr.decode('utf-8', errors='replace')}")
        return run_proc.stdout.decode("utf-8", errors="replace")


def emit_source(case: Case) -> str:
    s = case.seed
    branch_cond = {
        "if": f"((a + b + {s}) & 1)",
        "if_else": f"((a + b + {s}) & 1)",
        "nested": f"((a + b + {s}) & 1)",
        "short_circuit": f"(((a + {s}) & 1) && side_guard(b))",
        "while": f"((a + b + {s}) & 1)",
        "for": f"((a + b + {s}) & 1)",
        "branch_global": f"((a + b + {s}) & 1)",
        "call_chain": f"((a + b + {s}) & 1)",
        "seq_if": f"((a + b + {s}) & 1)",
    }[case.branch_kind]
    inner_cond = f"((c + {s}) & 1)"

    branch = []
    if case.branch_kind == "if":
        branch += [
            f"  if ({branch_cond}) {{",
            f"    x = x + y + g0 + {s % 7 + 3};",
            "  }",
        ]
    elif case.branch_kind == "if_else":
        branch += [
            f"  if ({branch_cond}) {{",
            f"    x = x + y + g1 + {s % 11 + 5};",
            "  } else {",
            f"    x = x + z + g2 + {s % 13 + 7};",
            "  }",
        ]
    elif case.branch_kind == "nested":
        branch += [
            f"  if ({branch_cond}) {{",
            f"    if ({inner_cond}) {{",
            f"      x = x + y + {s % 9 + 4};",
            "    } else {",
            f"      x = x + z + {s % 10 + 6};",
            "    }",
            "  }",
        ]
    elif case.branch_kind == "while":
        branch += [
            f"  int k = ((a + b + {s}) & 3) + 1;",
            "  while (k > 0) {",
            f"    if (((x + k + {s}) & 1) == 0) {{",
            f"      x = x + y + g2 + {s % 7 + 2};",
            "    } else {",
            f"      x = x + z + g1 + {s % 11 + 3};",
            "    }",
            "    k = k - 1;",
            "  }",
        ]
    elif case.branch_kind == "for":
        branch += [
            "  int i;",
            "  for (i = 0; i < 3; i = i + 1) {",
            f"    if (((a + i + {s}) & 1) == 0) {{",
            f"      x = x + y + i + {s % 5 + 2};",
            "    } else {",
            f"      x = x + z + i + {s % 7 + 4};",
            "    }",
            "  }",
        ]
    elif case.branch_kind == "branch_global":
        branch += [
            f"  if ({branch_cond}) {{",
            f"    g0 = g0 + x + {s % 7 + 2};",
            "  } else {",
            f"    g1 = g1 + y + {s % 11 + 3};",
            "  }",
        ]
    elif case.branch_kind == "call_chain":
        branch += [
            f"  if ({branch_cond}) {{",
            "    x = side_mix(side_guard(y) ? y : z) + x;",
            "  } else {",
            "    x = side_mix(x + z) + y;",
            "  }",
        ]
    elif case.branch_kind == "seq_if":
        branch += [
            f"  if (((a + {s}) & 1) == 0) {{",
            f"    x = x + y + {s % 7 + 2};",
            "  }",
            f"  if (((b + {s}) & 1) == 0) {{",
            f"    y = y + z + x + {s % 11 + 3};",
            "  }",
            f"  if (((c + {s}) & 1) == 0) {{",
            f"    g0 = g0 + x + y + {s % 13 + 4};",
            "  }",
        ]
    else:
        branch += [
            f"  if ({branch_cond}) {{",
            f"    x = x + side_mix(y) + {s % 7 + 2};",
            "  }",
        ]

    tail = {
        "local": f"return x + y + g0 + {s % 17 + 1};",
        "global": f"return x + g0 + g1 + {s % 19 + 2};",
        "array": f"return x + slots[idx] + {s % 23 + 3};",
        "call": f"return side_mix(x) + y + {s % 29 + 4};",
    }[case.tail_kind]

    return "\n".join(
        [
            f"int g0 = {s % 13 + 1};",
            f"int g1 = {s % 17 + 2};",
            f"int g2 = {s % 19 + 3};",
            "",
            "int side_guard(int v) {",
            f"  g1 = g1 + v + {s % 7 + 1};",
            "  return (g1 & 1) != 0;",
            "}",
            "",
            "int side_mix(int v) {",
            f"  g0 = g0 + v + {s % 11 + 5};",
            "  return g0 + v;",
            "}",
            "",
            "int solve(int a, int b, int c) {",
            f"  int slots[8];",
            "  int x;",
            "  int y;",
            "  int z;",
            "  int idx;",
            f"  x = a + {s % 23 + 3};",
            f"  y = b + {s % 29 + 4};",
            f"  z = c + {s % 31 + 5};",
            "  idx = (a + b + c + g0) & 7;",
            "  slots[idx] = x + y + z + g2;",
            *branch,
            f"  slots[idx] = slots[idx] + x + {s % 7 + 6};",
            f"  if (((x + y + z + {s}) & 1) == 0) {{",
            f"    g2 = g2 + slots[idx] + {s % 5 + 2};",
            "  } else {",
            f"    g2 = g2 + x + {s % 5 + 3};",
            "  }",
            f"  if (((g2 + x + {s}) & 1) == 0) {{",
            f"    g1 = g1 + slots[idx] + {s % 11 + 3};",
            "  }",
            f"  {tail}",
            "}",
            "",
            "int main() {",
            f"  int ans = solve({s % 7 + 1}, {s % 11 + 2}, {s % 13 + 3});",
            "  putint(ans);",
            "  putch(10);",
            "  return 0;",
            "}",
            "",
        ]
    )


def make_cases(seed: int) -> list[Case]:
    branch_kinds = ["if", "if_else", "nested", "short_circuit", "while", "for", "branch_global", "call_chain", "seq_if"]
    tail_kinds = ["local", "global", "array", "call"]
    cases: list[Case] = []
    idx = 0
    for b in branch_kinds:
        for t in tail_kinds:
            cases.append(Case(name=f"tailprobe_{idx:02d}_{b}_{t}", seed=seed + idx * 37, branch_kind=b, tail_kind=t))
            idx += 1
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a multi-structure tail-read SysY probe suite.")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=17)
    args = parser.parse_args()

    out_dir = args.output_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = make_cases(args.seed)
    for case in cases:
        source = emit_source(case)
        expected = build_host_oracle(source)
        sy_path = out_dir / f"{case.name}.sy"
        out_path = out_dir / f"{case.name}.out"
        sy_path.write_text(source, encoding="utf-8")
        out_path.write_text(expected, encoding="utf-8")
        print(f"wrote {sy_path.name}")

    print(f"generated {len(cases)} cases in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
