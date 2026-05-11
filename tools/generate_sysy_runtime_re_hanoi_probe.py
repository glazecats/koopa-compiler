#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def write_case(path: Path, source: str) -> int:
    c_path = path.with_suffix(".c")
    out_path = path.with_suffix(".out")
    c_path.write_text(source, encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="hanoi-host-") as td:
        tmp = Path(td)
        exe_path = tmp / "host.out"
        wrapper_path = tmp / "host_wrapper.c"
        wrapper_path.write_text(
            "\n".join(
                [
                    "#include <stdio.h>",
                    "int putint(int x) { return printf(\"%d\", x); }",
                    "int putch(int x) { return putchar(x); }",
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        subprocess.run(["gcc", "-O0", str(c_path), str(wrapper_path), "-o", str(exe_path)], check=True)
        result = subprocess.run([str(exe_path)], check=True, capture_output=True, text=True)
    host_stdout = result.stdout
    if host_stdout.endswith("\n"):
        oracle = host_stdout + str(result.returncode) + "\n"
    else:
        oracle = host_stdout + "\n" + str(result.returncode) + "\n"
    out_path.write_text(oracle, encoding="utf-8")
    return 1


def case_header() -> str:
    return "\n".join(
        [
            "int putint(int x);",
            "int putch(int x);",
            "int moves; int checksum; int g0; int g1; int g2; int g3;",
            "int touch(int x){ checksum = checksum + x + moves + g0 - g1 + g2 - g3; return x; }",
            "int mix(int a, int b, int c){ return a + b * 3 - c * 2 + touch((a ^ b) & 7); }",
        ]
    )


def emit_main(call_expr: str) -> str:
    return "\n".join(
        [
            "int main(){",
            "  moves = 0; checksum = 0; g0 = 1; g1 = 2; g2 = 3; g3 = 4;",
            f"  putint({call_expr}); putch(10);",
            "  putint(moves); putch(10);",
            "  putint(checksum); putch(10);",
            "  putint(g0 + g1 + g2 + g3); putch(10);",
            "  return 0;",
            "}",
        ]
    )


def build_cases() -> list[tuple[str, str]]:
    cases: list[tuple[str, str]] = []

    body1 = "\n".join(
        [
            "int h1(int n, int from, int to, int via){",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; checksum = checksum + from * 100 + to * 10 + via; return touch(from + to + via); }",
            "  return h1(n - 1, from, via, to) + touch(from) + h1(n - 1, via, to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_01", "\n".join([case_header(), body1, emit_main("h1(5, 1, 3, 2)")]) + "\n"))

    body2 = "\n".join(
        [
            "int h2(int n, int from, int to, int via){",
            "  int left; int right; int mid;",
            "  if (n <= 0) return 0;",
            "  if (n == 1) { moves = moves + 1; g0 = g0 + from; return mix(from, to, via); }",
            "  left = h2(n - 1, from, via, to);",
            "  mid = touch(from + n);",
            "  right = h2(n - 1, via, to, from);",
            "  return left + mid + right + g0 - g1;",
            "}",
        ]
    )
    cases.append(("hanoi_probe_02", "\n".join([case_header(), body2, emit_main("h2(5, 1, 3, 2)")]) + "\n"))

    body3 = "\n".join(
        [
            "int bump(int x, int y){ g1 = g1 + x - y; return touch(x + y + g1); }",
            "int h3(int n, int from, int to, int via){",
            "  if (!n) return 0;",
            "  if (n == 1) { moves = moves + 1; return bump(from, to); }",
            "  return h3(n - 1, from, via, to) + bump(from, via) + h3(n - 1, via, to, from) + bump(to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_03", "\n".join([case_header(), body3, emit_main("h3(4, 1, 3, 2)")]) + "\n"))

    body4 = "\n".join(
        [
            "int maybe(int x){ if ((x & 1) == 0) g2 = g2 + x; else g3 = g3 - x; return touch(g2 + g3 + x); }",
            "int h4(int n, int from, int to, int via){",
            "  int acc;",
            "  if (n == 0) return 0;",
            "  acc = maybe(from + to);",
            "  if (n == 1) { moves = moves + 1; return acc + maybe(via); }",
            "  return h4(n - 1, from, via, to) + acc + h4(n - 1, via, to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_04", "\n".join([case_header(), body4, emit_main("h4(5, 1, 3, 2)")]) + "\n"))

    body5 = "\n".join(
        [
            "int h5_inner(int n, int a, int b, int c){",
            "  if (n < 2) { moves = moves + n; return touch(a + b + c + n); }",
            "  return h5_inner(n - 1, a, c, b) + h5_inner(n - 2, c, b, a) + touch(a + c);",
            "}",
            "int h5(int n, int from, int to, int via){",
            "  return h5_inner(n, from, to, via) + touch(from) + touch(to);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_05", "\n".join([case_header(), body5, emit_main("h5(6, 1, 3, 2)")]) + "\n"))

    body6 = "\n".join(
        [
            "int h6(int n, int from, int to, int via){",
            "  int l; int r;",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; checksum = checksum + 7; return touch(from * 10 + to); }",
            "  l = h6(n - 1, from, via, to);",
            "  if ((l & 1) == 0) g0 = g0 + from; else g1 = g1 + via;",
            "  r = h6(n - 1, via, to, from);",
            "  return l + r + touch(g0 + g1 + to);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_06", "\n".join([case_header(), body6, emit_main("h6(5, 1, 3, 2)")]) + "\n"))

    body7 = "\n".join(
        [
            "int h7_once(int x, int y, int z){ return touch(x + y) + touch(y + z) - touch(x + z); }",
            "int h7(int n, int from, int to, int via){",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return h7_once(from, to, via); }",
            "  return h7(n - 1, from, via, to) + h7_once(from, to, via) + h7(n - 1, via, to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_07", "\n".join([case_header(), body7, emit_main("h7(5, 1, 3, 2)")]) + "\n"))

    body8 = "\n".join(
        [
            "int h8(int n, int from, int to, int via){",
            "  int t0; int t1; int t2;",
            "  if (n == 0) return 0;",
            "  t0 = touch(from);",
            "  if (n == 1) { moves = moves + 1; return t0 + touch(to) + touch(via); }",
            "  t1 = h8(n - 1, from, via, to);",
            "  t2 = h8(n - 1, via, to, from);",
            "  return t1 + t0 + t2 + touch(n);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_08", "\n".join([case_header(), body8, emit_main("h8(5, 1, 3, 2)")]) + "\n"))

    body9 = "\n".join(
        [
            "int h9(int n, int from, int to, int via){",
            "  int left; int right;",
            "  if (n <= 0) return 0;",
            "  if (n == 1) { moves = moves + 1; checksum = checksum + touch(from + to + via); return mix(from, via, to); }",
            "  left = h9(n - 1, from, via, to);",
            "  right = h9(n - 1, via, to, from);",
            "  if ((left + right) & 1) { g0 = g0 + from + to; } else { g1 = g1 + via; }",
            "  return left + right + touch(g0 + g1 + n);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_09", "\n".join([case_header(), body9, emit_main("h9(6, 1, 3, 2)")]) + "\n"))

    body10 = "\n".join(
        [
            "int h10_gate(int x, int y){ if (x > y) return touch(x - y); return touch(y - x); }",
            "int h10(int n, int from, int to, int via){",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return h10_gate(from, to); }",
            "  return h10(n - 1, from, via, to) + h10_gate(from, via) + h10(n - 1, via, to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_10", "\n".join([case_header(), body10, emit_main("h10(5, 1, 3, 2)")]) + "\n"))

    body11 = "\n".join(
        [
            "int h11_step(int a, int b, int c){",
            "  g2 = g2 + a;",
            "  g3 = g3 - b;",
            "  return touch(a + b + c + g2 + g3);",
            "}",
            "int h11(int n, int from, int to, int via){",
            "  int v;",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return h11_step(from, to, via); }",
            "  v = h11(n - 1, from, via, to);",
            "  if ((v & 3) == 0) { v = v + h11_step(from, via, to); }",
            "  return v + h11(n - 1, via, to, from) + touch(n + from + to + via);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_11", "\n".join([case_header(), body11, emit_main("h11(5, 1, 3, 2)")]) + "\n"))

    body12 = "\n".join(
        [
            "int h12_pick(int n, int x){ if (n & 1) return touch(n + x); return touch(x - n); }",
            "int h12(int n, int from, int to, int via){",
            "  int k;",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return h12_pick(n, from); }",
            "  k = h12_pick(n, to);",
            "  return h12(n - 1, from, via, to) + k + h12(n - 1, via, to, from);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_12", "\n".join([case_header(), body12, emit_main("h12(6, 1, 3, 2)")]) + "\n"))

    body13 = "\n".join(
        [
            "int h13(int n, int from, int to, int via){",
            "  int x; int y;",
            "  if (n <= 0) return 0;",
            "  x = touch(from + via);",
            "  y = touch(to + via);",
            "  if (n == 1) { moves = moves + 1; return x + y + touch(from + to); }",
            "  return h13(n - 1, from, via, to) + x + h13(n - 1, via, to, from) + y;",
            "}",
        ]
    )
    cases.append(("hanoi_probe_13", "\n".join([case_header(), body13, emit_main("h13(5, 1, 3, 2)")]) + "\n"))

    body14 = "\n".join(
        [
            "int h14(int n, int from, int to, int via){",
            "  int r;",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; checksum = checksum + from + to + via; return touch(from * 4 + to * 2 + via); }",
            "  r = h14(n - 1, from, via, to);",
            "  if (r > 10) { g0 = g0 + r; } else { g1 = g1 + r; }",
            "  return r + h14(n - 1, via, to, from) + touch(g0 + g1);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_14", "\n".join([case_header(), body14, emit_main("h14(5, 1, 3, 2)")]) + "\n"))

    body15 = "\n".join(
        [
            "int h15_store(int idx, int val){ if (idx == 0) g2 = val; else g3 = val; return touch(idx + val); }",
            "int h15(int n, int from, int to, int via){",
            "  int left; int right;",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return h15_store(from & 1, to + via); }",
            "  left = h15(n - 1, from, via, to);",
            "  right = h15(n - 1, via, to, from);",
            "  return left + right + h15_store((from + to + via) & 1, n);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_15", "\n".join([case_header(), body15, emit_main("h15(5, 1, 3, 2)")]) + "\n"))

    body16 = "\n".join(
        [
            "int h16(int n, int from, int to, int via){",
            "  if (n == 0) return 0;",
            "  if (n == 1) { moves = moves + 1; return touch(from + to + via + checksum); }",
            "  if ((from < to && to < via) || (via < from && from < to)) { checksum = checksum + n; }",
            "  return h16(n - 1, from, via, to) + touch(from) + h16(n - 1, via, to, from) + touch(to);",
            "}",
        ]
    )
    cases.append(("hanoi_probe_16", "\n".join([case_header(), body16, emit_main("h16(6, 1, 3, 2)")]) + "\n"))

    return cases


def main() -> int:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/runtime_re_hanoi_probe_suite")
    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for name, source in build_cases():
        count += write_case(out_dir / name, source)
    print(f"generated {count} cases in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
