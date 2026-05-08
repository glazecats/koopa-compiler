#!/usr/bin/env python3

from __future__ import annotations

import argparse
import subprocess
import tempfile
import time
from pathlib import Path


def run(cmd: list[str]) -> tuple[int, str, str, float]:
    start = time.perf_counter()
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    elapsed = time.perf_counter() - start
    return proc.returncode, proc.stdout, proc.stderr, elapsed


def main() -> int:
    parser = argparse.ArgumentParser(description="Profile compiler front/middle/back stages with small standalone probes.")
    parser.add_argument("input", type=Path)
    parser.add_argument("--workdir", type=Path, default=Path("/workspaces/compiler_lab"))
    parser.add_argument("--compiler", type=Path, default=Path("/workspaces/compiler_lab/build/compiler"))
    args = parser.parse_args()

    workdir = args.workdir.resolve()
    compiler = args.compiler.resolve()
    source = args.input.resolve()

    with tempfile.TemporaryDirectory(prefix="compile-profile-") as td:
        tmp = Path(td)
        asm = tmp / "out.s"

        commands = [
            ("full-compiler", [str(compiler), "-riscv", str(source), "-o", str(asm)]),
            ("full-perf", [str(compiler), "-perf", str(source), "-o", str(asm)]),
        ]

        for label, cmd in commands:
            code, _stdout, stderr, elapsed = run(cmd)
            print(f"{label}: status={code} elapsed={elapsed:.3f}s")
            if code != 0 and stderr:
                print(stderr.strip())

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
