#!/usr/bin/env python3
import subprocess
import sys
import tempfile
from pathlib import Path


def write_case(path: Path, source: str) -> None:
    c_path = path.with_suffix(".c")
    out_path = path.with_suffix(".out")
    c_path.write_text(source, encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="callframe-host-") as td:
        tmp = Path(td)
        wrapper = tmp / "host_wrapper.c"
        exe = tmp / "host.out"
        wrapper.write_text(
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
        subprocess.run(["gcc", "-O0", str(c_path), str(wrapper), "-o", str(exe)], check=True)
        result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
    host_stdout = result.stdout
    if host_stdout.endswith("\n"):
        oracle = host_stdout + str(result.returncode) + "\n"
    else:
        oracle = host_stdout + "\n" + str(result.returncode) + "\n"
    out_path.write_text(oracle, encoding="utf-8")


def header() -> str:
    return "\n".join(
        [
            "int putint(int x);",
            "int putch(int x);",
            "int g0; int g1; int g2; int g3; int gm; int hist[64];",
            "int touch(int x){ gm = gm + x + g0 - g1 + g2 - g3; return x; }",
            "int stamp(int idx, int val){ hist[idx & 63] = hist[idx & 63] + val + gm; return hist[idx & 63]; }",
            "int mix4(int a, int b, int c, int d){ return touch(a + b * 2 - c + d * 3); }",
            "int mix8(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7){",
            "  return touch(a0 + a1 - a2 + a3 - a4 + a5 - a6 + a7);",
            "}",
            "int mix12(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){",
            "  return touch(a0 + a1 + a2 - a3 + a4 - a5 + a6 - a7 + a8 - a9 + a10 - a11);",
            "}",
        ]
    )


def emit_main(expr: str) -> str:
    return "\n".join(
        [
            "int main(){",
            "  int i;",
            "  g0 = 1; g1 = 2; g2 = 3; g3 = 4; gm = 0;",
            "  i = 0; while (i < 64) { hist[i] = i * 3 + 1; i = i + 1; }",
            f"  putint({expr}); putch(10);",
            "  putint(gm); putch(10);",
            "  putint(hist[0] + hist[1] + hist[7] + hist[31]); putch(10);",
            "  return 0;",
            "}",
        ]
    )


def cases() -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []

    body1 = "\n".join(
        [
            "int f1(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9){",
            "  int t;",
            "  t = mix8(a0,a1,a2,a3,a4,a5,a6,a7);",
            "  return t + stamp(a8, a9) + touch(a8 + a9);",
            "}",
        ]
    )
    out.append(("callframe_probe_01", "\n".join([header(), body1, emit_main("f1(1,2,3,4,5,6,7,8,9,10)")]) + "\n"))

    body2 = "\n".join(
        [
            "int f2_inner(int n,int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10,int a11){",
            "  if (n == 0) return mix12(a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11);",
            "  return f2_inner(n-1,a11,a10,a9,a8,a7,a6,a5,a4,a3,a2,a1,a0) + touch(n + a0 + a11);",
            "}",
            "int f2(){ return f2_inner(3,1,2,3,4,5,6,7,8,9,10,11,12); }",
        ]
    )
    out.append(("callframe_probe_02", "\n".join([header(), body2, emit_main("f2()")]) + "\n"))

    body3 = "\n".join(
        [
            "int f3b(int x,int y,int z,int p,int q,int r,int s,int t,int u){",
            "  return stamp(x + y, mix8(x,y,z,p,q,r,s,t) + u);",
            "}",
            "int f3a(int x){",
            "  int k;",
            "  k = f3b(x, x+1, x+2, x+3, x+4, x+5, x+6, x+7, x+8);",
            "  return k + touch(hist[x & 7]);",
            "}",
        ]
    )
    out.append(("callframe_probe_03", "\n".join([header(), body3, emit_main("f3a(5)")]) + "\n"))

    body4 = "\n".join(
        [
            "int f4(int n,int a,int b,int c){",
            "  int x; int y;",
            "  if (n <= 0) return touch(a + b + c);",
            "  x = mix4(a,b,c,n);",
            "  y = f4(n-1, c, a + n, b - n);",
            "  return x + y + stamp(n, x - y);",
            "}",
        ]
    )
    out.append(("callframe_probe_04", "\n".join([header(), body4, emit_main("f4(6, 1, 2, 3)")]) + "\n"))

    body5 = "\n".join(
        [
            "int f5g(int x){ g0 = g0 + x; return touch(g0 + x); }",
            "int f5(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8,int a9,int a10){",
            "  int u;",
            "  u = mix12(a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,1);",
            "  if ((u & 1) != 0) { u = u + f5g(a8); } else { u = u + f5g(a10); }",
            "  return u + stamp(a9, a10);",
            "}",
        ]
    )
    out.append(("callframe_probe_05", "\n".join([header(), body5, emit_main("f5(1,2,3,4,5,6,7,8,9,10,11)")]) + "\n"))

    body6 = "\n".join(
        [
            "int f6h(int n,int a,int b,int c,int d,int e,int f,int g,int h,int i){",
            "  if (n == 0) return mix8(a,b,c,d,e,f,g,h) + i;",
            "  return f6h(n-1, i,h,g,f,e,d,c,b,a) + touch(a + i + n);",
            "}",
            "int f6(){ return f6h(4,1,2,3,4,5,6,7,8,9); }",
        ]
    )
    out.append(("callframe_probe_06", "\n".join([header(), body6, emit_main("f6()")]) + "\n"))

    body7 = "\n".join(
        [
            "int f7p(int x){ return hist[x & 15] + touch(x); }",
            "int f7q(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j){",
            "  int r0; int r1;",
            "  r0 = f7p(i) + mix8(a,b,c,d,e,f,g,h);",
            "  r1 = f7p(j) + stamp(i, j);",
            "  return r0 + r1 + touch(a + j);",
            "}",
        ]
    )
    out.append(("callframe_probe_07", "\n".join([header(), body7, emit_main("f7q(1,2,3,4,5,6,7,8,9,10)")]) + "\n"))

    body8 = "\n".join(
        [
            "int f8r(int x,int y){ return touch(x * 7 + y * 5); }",
            "int f8(int n,int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k){",
            "  if (n == 0) return mix12(a,b,c,d,e,f,g,h,i,j,k,1);",
            "  return f8(n-1,k,j,i,h,g,f,e,d,c,b,a) + f8r(a,k) + stamp(n, a + k);",
            "}",
        ]
    )
    out.append(("callframe_probe_08", "\n".join([header(), body8, emit_main("f8(3,1,2,3,4,5,6,7,8,9,10,11)")]) + "\n"))

    body9 = "\n".join(
        [
            "int f9s(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l){",
            "  int arr[8];",
            "  arr[0]=a; arr[1]=b; arr[2]=c; arr[3]=d; arr[4]=e; arr[5]=f; arr[6]=g; arr[7]=h;",
            "  return arr[(i + j) & 7] + touch(k + l + arr[(k + l) & 7]);",
            "}",
        ]
    )
    out.append(("callframe_probe_09", "\n".join([header(), body9, emit_main("f9s(1,2,3,4,5,6,7,8,9,10,11,12)")]) + "\n"))

    body10 = "\n".join(
        [
            "int f10u(int n,int x){ if (n == 0) return touch(x); return f10u(n-1, x+1) + touch(x-n); }",
            "int f10v(int a0,int a1,int a2,int a3,int a4,int a5,int a6,int a7,int a8){",
            "  return f10u(5, a8) + mix8(a0,a1,a2,a3,a4,a5,a6,a7);",
            "}",
        ]
    )
    out.append(("callframe_probe_10", "\n".join([header(), body10, emit_main("f10v(1,2,3,4,5,6,7,8,9)")]) + "\n"))

    body11 = "\n".join(
        [
            "int f11w(int x,int y,int z){ return stamp(x + y, z) + touch(x - y + z); }",
            "int f11(int n,int a,int b,int c,int d,int e,int f,int g,int h,int i){",
            "  int r;",
            "  r = f11w(a, h, i);",
            "  if (n == 0) return r;",
            "  return f11(n-1, i,h,g,f,e,d,c,b,a) + r + touch(n + a + i);",
            "}",
        ]
    )
    out.append(("callframe_probe_11", "\n".join([header(), body11, emit_main("f11(4,1,2,3,4,5,6,7,8,9)")]) + "\n"))

    body12 = "\n".join(
        [
            "int f12x(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l){",
            "  return touch(a+b+c+d+e+f+g+h+i+j+k+l);",
            "}",
            "int f12(){ return f12x(1,2,3,4,5,6,7,8,9,10,11,12) + f12x(12,11,10,9,8,7,6,5,4,3,2,1); }",
        ]
    )
    out.append(("callframe_probe_12", "\n".join([header(), body12, emit_main("f12()")]) + "\n"))

    return out


def main() -> int:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/tmp/runtime_re_callframe_probe_suite")
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, source in cases():
        write_case(out_dir / name, source)
    print(f"generated {len(cases())} cases in {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
