#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def normalize_text_bytes(data: bytes) -> str:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n").decode("utf-8", errors="replace")


def normalize_oracle_text(text: str) -> str:
    lines = text.split("\n")
    return "\n".join(line.rstrip(" \t") for line in lines)


def trim_trailing_blank_lines(text: str) -> str:
    return text.rstrip("\n")


def output_variants(stdout_text: str, exit_code: int) -> set[str]:
    base_stdout = stdout_text.rstrip("\n")
    exit_text = str(exit_code)
    return {
        stdout_text,
        base_stdout,
        stdout_text + exit_text,
        stdout_text + exit_text + "\n",
        base_stdout + exit_text,
        base_stdout + "\n" + exit_text,
        base_stdout + exit_text + "\n",
        base_stdout + "\n" + exit_text + "\n",
    }


def detect_cases(root: Path) -> list[Path]:
    cases: list[Path] = []
    for suffix in (".c", ".sy"):
        cases.extend(root.rglob(f"*{suffix}"))
    cases = [p for p in cases if p.is_file()]
    cases.sort()
    return cases


def companion(path: Path, suffix: str) -> Path:
    return path.with_suffix(suffix)


def detect_suite_root(case_root: Path) -> Path:
    current = case_root.resolve()
    for candidate in [current, *current.parents]:
        if (candidate / "Makefile").is_file():
            return candidate
    return case_root.resolve()


def find_runtime_lib_dir(suite_root: Path) -> Path | None:
    for name in ("sysy-runtime-lib", "sysyruntimelibrary", "sysy-runtime-libary"):
        candidate = suite_root / name
        if candidate.is_dir():
            return candidate
    return None


def find_native_runtime_lib() -> Path | None:
    candidate = Path("/opt/lib/native/libsysy.a")
    if candidate.is_file():
        return candidate
    return None


def suite_build_out_path(suite_root: Path, case: Path) -> Path:
    rel = case.relative_to(suite_root)
    return (suite_root / "build" / rel).with_suffix(".out")


def build_host_oracle(case: Path, suite_root: Path, stdin_bytes: bytes | None, timeout_s: int) -> tuple[bool, str]:
    runtime_dir = find_runtime_lib_dir(suite_root)
    native_runtime_lib = find_native_runtime_lib()
    if case.suffix != ".c":
        return False, ""

    link_inputs: list[str]
    if runtime_dir is not None:
        runtime_candidates = list(runtime_dir.glob("*.c"))
        if runtime_candidates:
            link_inputs = [str(runtime_candidates[0])]
        elif native_runtime_lib is not None:
            link_inputs = [str(native_runtime_lib)]
        else:
            return False, ""
    elif native_runtime_lib is not None:
        link_inputs = [str(native_runtime_lib)]
    else:
        return False, ""
    with tempfile.TemporaryDirectory(prefix="sysy-host-oracle-") as td:
        tmp = Path(td)
        exe = tmp / "host.out"
        compile_args = [
            "clang",
            "-Wall",
            "-Wno-implicit-function-declaration",
            "-Wno-unused-variable",
            "-Wno-dangling-else",
            "-Wno-unused-value",
            "-Wno-empty-body",
            "-Wno-missing-braces",
            "-Wno-logical-op-parentheses",
            "-Wno-tautological-compare",
            "-Wno-constant-logical-operand",
            "-Dstarttime=_sysy_starttime",
            "-Dstoptime=_sysy_stoptime",
            "-O2",
            str(case),
            *link_inputs,
            "-o",
            str(exe),
        ]
        try:
            compile_proc = run_cmd(compile_args, timeout_s=timeout_s)
        except subprocess.TimeoutExpired:
            return False, ""
        if compile_proc.returncode != 0 or not exe.exists():
            return False, ""

        try:
            run_proc = run_cmd([str(exe)], timeout_s=timeout_s, stdin_bytes=stdin_bytes)
        except subprocess.TimeoutExpired:
            return False, ""

        host_stdout = normalize_text_bytes(run_proc.stdout)
        if host_stdout.endswith("\n"):
            return True, host_stdout + str(run_proc.returncode)
        return True, host_stdout + "\n" + str(run_proc.returncode)


def run_cmd(
    args: list[str],
    *,
    timeout_s: int,
    stdin_bytes: bytes | None = None,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        args,
        input=stdin_bytes,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_s,
        check=False,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile/link/run a SysY suite with normalized oracle checks.")
    parser.add_argument("suite_dir", type=Path)
    parser.add_argument("--compiler", type=Path, default=Path("build/compiler"))
    parser.add_argument("--case-timeout", type=int, default=20)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--filter", default="")
    parser.add_argument("--verbose-failures", action="store_true")
    args = parser.parse_args()

    suite_dir = args.suite_dir.resolve()
    compiler = args.compiler.resolve()
    suite_root = detect_suite_root(suite_dir)

    if not suite_dir.is_dir():
        print(f"SUITE_NOT_FOUND {suite_dir}")
        return 2
    if not compiler.is_file():
        print(f"COMPILER_NOT_FOUND {compiler}")
        return 2

    cases = detect_cases(suite_dir)
    if args.filter:
        cases = [p for p in cases if args.filter in str(p)]
    if args.limit > 0:
        cases = cases[: args.limit]

    if not cases:
        print(f"NO_CASES {suite_dir}")
        return 2

    ok_count = 0
    fail_count = 0
    skip_count = 0
    failures: list[str] = []

    for idx, case in enumerate(cases, start=1):
        rel = case.relative_to(suite_dir)
        out_path = companion(case, ".out")
        in_path = companion(case, ".in")
        build_out_path = suite_build_out_path(suite_root, case) if case.is_relative_to(suite_root) else out_path

        expected = None
        if out_path.is_file():
            expected = normalize_text_bytes(out_path.read_bytes())
        elif build_out_path.is_file():
            expected = normalize_text_bytes(build_out_path.read_bytes())

        stdin_bytes = in_path.read_bytes() if in_path.is_file() else None
        if expected is None:
            built, host_expected = build_host_oracle(case, suite_root, stdin_bytes, args.case_timeout)
            if built:
                expected = host_expected

        if expected is None:
            skip_count += 1
            print(f"[{idx}/{len(cases)}] SKIP {rel} (missing .out)")
            continue

        with tempfile.TemporaryDirectory(prefix="sysy-sweep-") as td:
            tmp = Path(td)
            asm = tmp / "out.s"
            obj = tmp / "out.o"
            exe = tmp / "out"

            try:
                compile_proc = run_cmd(
                    [str(compiler), "-riscv", str(case), "-o", str(asm)],
                    timeout_s=args.case_timeout,
                )
            except subprocess.TimeoutExpired:
                fail_count += 1
                failures.append(f"{rel}: COMPILE_TIMEOUT")
                print(f"[{idx}/{len(cases)}] FAIL {rel} COMPILE_TIMEOUT")
                continue

            if compile_proc.returncode != 0 or not asm.exists():
                fail_count += 1
                failures.append(f"{rel}: COMPILE_FAIL")
                print(f"[{idx}/{len(cases)}] FAIL {rel} COMPILE_FAIL")
                if args.verbose_failures:
                    sys.stdout.write(normalize_text_bytes(compile_proc.stderr))
                continue

            try:
                asm_proc = run_cmd(
                    [
                        "clang",
                        str(asm),
                        "-c",
                        "-o",
                        str(obj),
                        "-target",
                        "riscv32-unknown-linux-elf",
                        "-march=rv32im",
                        "-mabi=ilp32",
                    ],
                    timeout_s=args.case_timeout,
                )
            except subprocess.TimeoutExpired:
                fail_count += 1
                failures.append(f"{rel}: ASSEMBLE_TIMEOUT")
                print(f"[{idx}/{len(cases)}] FAIL {rel} ASSEMBLE_TIMEOUT")
                continue

            if asm_proc.returncode != 0 or not obj.exists():
                fail_count += 1
                failures.append(f"{rel}: ASSEMBLE_FAIL")
                print(f"[{idx}/{len(cases)}] FAIL {rel} ASSEMBLE_FAIL")
                if args.verbose_failures:
                    sys.stdout.write(normalize_text_bytes(asm_proc.stderr))
                continue

            try:
                link_proc = run_cmd(
                    [
                        "ld.lld",
                        str(obj),
                        "-L/opt/lib/riscv32",
                        "-lsysy",
                        "-o",
                        str(exe),
                    ],
                    timeout_s=args.case_timeout,
                )
            except subprocess.TimeoutExpired:
                fail_count += 1
                failures.append(f"{rel}: LINK_TIMEOUT")
                print(f"[{idx}/{len(cases)}] FAIL {rel} LINK_TIMEOUT")
                continue

            if link_proc.returncode != 0 or not exe.exists():
                fail_count += 1
                failures.append(f"{rel}: LINK_FAIL")
                print(f"[{idx}/{len(cases)}] FAIL {rel} LINK_FAIL")
                if args.verbose_failures:
                    sys.stdout.write(normalize_text_bytes(link_proc.stderr))
                continue

            try:
                run_proc = run_cmd(
                    ["qemu-riscv32-static", str(exe)],
                    timeout_s=args.case_timeout,
                    stdin_bytes=stdin_bytes,
                )
            except subprocess.TimeoutExpired:
                fail_count += 1
                failures.append(f"{rel}: RUN_TIMEOUT")
                print(f"[{idx}/{len(cases)}] FAIL {rel} RUN_TIMEOUT")
                continue

            actual_stdout = normalize_text_bytes(run_proc.stdout)
            actual_exit = run_proc.returncode
            variants = output_variants(actual_stdout, actual_exit)
            normalized_expected = normalize_oracle_text(expected)
            normalized_variants = {normalize_oracle_text(v) for v in variants}
            if expected in variants or trim_trailing_blank_lines(expected) in {
                trim_trailing_blank_lines(v) for v in variants
            } or normalized_expected in normalized_variants or trim_trailing_blank_lines(normalized_expected) in {
                trim_trailing_blank_lines(v) for v in normalized_variants
            }:
                ok_count += 1
                print(f"[{idx}/{len(cases)}] PASS {rel}")
            else:
                fail_count += 1
                failures.append(f"{rel}: MISMATCH")
                print(f"[{idx}/{len(cases)}] FAIL {rel} MISMATCH")
                if args.verbose_failures:
                    print("EXPECTED_REPR", repr(expected))
                    print("ACTUAL_STDOUT_REPR", repr(actual_stdout))
                    print("ACTUAL_EXIT", actual_exit)

    print(
        f"SUMMARY suite={suite_dir} total={len(cases)} passed={ok_count} failed={fail_count} skipped={skip_count}"
    )
    if failures:
        print("FAILURES")
        for item in failures:
            print(item)
    return 1 if fail_count else 0


if __name__ == "__main__":
    raise SystemExit(main())
