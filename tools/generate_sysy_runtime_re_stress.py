#!/usr/bin/env python3

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import subprocess
import sys
import tempfile


MOD = 1000003


def pos_mod(value: int, mod: int) -> int:
    value %= mod
    return value if value >= 0 else value + mod


@dataclass
class Config:
    leaf_count: int
    chain_count: int
    array_size: int
    global_count: int
    global_size: int
    param_count: int
    main_iters: int
    seed: int


class Model:
    def __init__(self, cfg: Config) -> None:
        self.cfg = cfg
        self.globals = [
            [((cfg.seed + g * 97 + i * 31) % MOD) for i in range(cfg.global_size)]
            for g in range(cfg.global_count)
        ]

    def leaf(self, idx: int, args: list[int]) -> int:
        cfg = self.cfg
        local0 = [0] * cfg.array_size
        local1 = [0] * cfg.array_size
        acc = pos_mod(sum(args) + idx * 17 + cfg.seed, MOD)

        for i in range(cfg.array_size):
            slot = pos_mod(acc + args[idx % cfg.param_count] + i * 7 + idx * 3 + cfg.global_size * 8, cfg.global_size)
            g0 = idx % cfg.global_count
            g1 = (idx + 1) % cfg.global_count
            g2 = (idx + 2) % cfg.global_count
            g3 = (idx + 3) % cfg.global_count
            local0[i] = pos_mod(self.globals[g0][slot] + args[(idx + 1) % cfg.param_count] + i + idx + 13, MOD)
            self.globals[g1][pos_mod(slot + i + idx, cfg.global_size)] = pos_mod(
                local0[i] + acc + args[(idx * 3 + 1) % cfg.param_count] + 19,
                MOD,
            )
            local1[i] = pos_mod(
                self.globals[g2][pos_mod(slot * 3 + i + idx + 5, cfg.global_size)]
                + local0[i]
                + args[(idx * 5 + 2) % cfg.param_count]
                + 23,
                MOD,
            )
            self.globals[g3][pos_mod(slot * 5 + i + idx + 7, cfg.global_size)] = pos_mod(
                self.globals[g3][pos_mod(slot * 5 + i + idx + 7, cfg.global_size)] + local1[i] + i + 29,
                MOD,
            )
            acc = pos_mod(acc + local1[i] + self.globals[g1][pos_mod(slot + i + idx, cfg.global_size)] + i, MOD)

        return pos_mod(acc + local0[0] + local1[-1], MOD)

    def chain(self, idx: int, args: list[int]) -> int:
        cfg = self.cfg
        local0 = [0] * cfg.array_size
        local1 = [0] * cfg.array_size
        acc = self.leaf((idx * 7 + 3) % cfg.leaf_count, args)

        for i in range(cfg.array_size):
            slot = pos_mod(acc + args[(idx * 2) % cfg.param_count] + i * 11 + idx * 5 + cfg.global_size * 8, cfg.global_size)
            g0 = idx % cfg.global_count
            g1 = (idx + 1) % cfg.global_count
            local0[i] = pos_mod(self.globals[g0][slot] + acc + i + idx + 31, MOD)
            self.globals[g1][pos_mod(slot + idx + i * 3, cfg.global_size)] = pos_mod(
                self.globals[g1][pos_mod(slot + idx + i * 3, cfg.global_size)] + local0[i] + args[(idx + 2) % cfg.param_count] + 37,
                MOD,
            )
            local1[i] = pos_mod(
                self.globals[g1][pos_mod(slot + idx + i * 3, cfg.global_size)] + local0[i] + args[(idx * 7 + 4) % cfg.param_count] + 41,
                MOD,
            )
            acc = pos_mod(acc + local1[i] + i + idx, MOD)

        if idx > 0:
            next_args = [pos_mod(args[p] + local0[p % cfg.array_size] + idx + p * 3, MOD) for p in range(cfg.param_count)]
            acc = pos_mod(acc + self.chain(idx - 1, next_args), MOD)
        acc = pos_mod(acc + self.leaf((idx * 11 + 5) % cfg.leaf_count, [pos_mod(args[p] + local1[p % cfg.array_size] + idx + p, MOD) for p in range(cfg.param_count)]), MOD)

        for i in range(cfg.array_size):
            slot = pos_mod(acc + local0[i] + i * 13 + idx + cfg.global_size * 8, cfg.global_size)
            g0 = (idx + 2) % cfg.global_count
            g1 = (idx + 4) % cfg.global_count
            self.globals[g0][slot] = pos_mod(self.globals[g0][slot] + local1[i] + acc + 43, MOD)
            acc = pos_mod(acc + self.globals[g0][slot] + self.globals[g1][pos_mod(slot + i + 17, cfg.global_size)], MOD)

        return acc

    def run(self) -> int:
        cfg = self.cfg
        answer = 0
        for it in range(cfg.main_iters):
            args = [
                pos_mod(cfg.seed + it * 53 + p * 97 + self.globals[p % cfg.global_count][pos_mod(it + p * 5, cfg.global_size)], MOD)
                for p in range(cfg.param_count)
            ]
            answer = pos_mod(answer + self.chain(cfg.chain_count - 1, args), MOD)
            for g in range(cfg.global_count):
                pivot = pos_mod(answer + it * 7 + g * 13, cfg.global_size)
                self.globals[g][pivot] = pos_mod(self.globals[g][pivot] + answer + g + it, MOD)
        return answer


def emit_header(cfg: Config) -> list[str]:
    lines = [
        "int putint(int x);",
        "int putch(int x);",
        f"const int MOD = {MOD};",
        "",
    ]
    for g in range(cfg.global_count):
        lines.append(f"int g{g}[{cfg.global_size}];")
    lines.append("")
    return lines


def emit_leaf(cfg: Config, idx: int) -> list[str]:
    params = ", ".join(f"int a{i}" for i in range(cfg.param_count))
    lines = [
        f"int leaf_{idx}({params}) {{",
        f"  int local0[{cfg.array_size}];",
        f"  int local1[{cfg.array_size}];",
        "  int acc;",
        "  int i;",
        "  int slot;",
        "  int alt;",
    ]
    sum_expr = " + ".join(f"a{i}" for i in range(cfg.param_count))
    lines.append(f"  acc = ({sum_expr} + {idx * 17 + cfg.seed}) % MOD;")
    lines.append("  i = 0;")
    lines.append(f"  while (i < {cfg.array_size}) {{")
    lines.append(
        f"    slot = (acc + a[(i + {idx}) % {cfg.param_count}] + i * 7 + {idx * 3} + {cfg.global_size * 8}) % {cfg.global_size};".replace("a[", "a")
    )
    # replace expanded parameter indexing explicitly
    lines[-1] = f"    slot = (acc + a{(idx) % cfg.param_count} + i * 7 + {idx * 3} + {cfg.global_size * 8}) % {cfg.global_size};"
    lines.append(
        f"    local0[i] = (g{idx % cfg.global_count}[slot] + a{(idx + 1) % cfg.param_count} + i + {idx + 13}) % MOD;"
    )
    lines.append(
        f"    g{(idx + 1) % cfg.global_count}[(slot + i + {idx}) % {cfg.global_size}] = "
        f"(local0[i] + acc + a{(idx * 3 + 1) % cfg.param_count} + 19) % MOD;"
    )
    lines.append(
        f"    local1[i] = (g{(idx + 2) % cfg.global_count}[(slot * 3 + i + {idx + 5}) % {cfg.global_size}] + "
        f"local0[i] + a{(idx * 5 + 2) % cfg.param_count} + 23) % MOD;"
    )
    lines.append(
        f"    g{(idx + 3) % cfg.global_count}[(slot * 5 + i + {idx + 7}) % {cfg.global_size}] = "
        f"(g{(idx + 3) % cfg.global_count}[(slot * 5 + i + {idx + 7}) % {cfg.global_size}] + local1[i] + i + 29) % MOD;"
    )
    lines.append(f"    alt = (slot + acc + i + {idx * 9 + 47}) % {cfg.global_size};")
    lines.append(f"    if (((acc + i + {idx}) % 3) == 0) {{")
    lines.append(
        f"      local0[i] = (local0[i] + g{(idx + 4) % cfg.global_count}[alt] + a{(idx + 6) % cfg.param_count} + 59) % MOD;"
    )
    lines.append(
        f"      g{(idx + 5) % cfg.global_count}[alt] = (g{(idx + 5) % cfg.global_count}[alt] + local0[i] + local1[i] + 61) % MOD;"
    )
    lines.append("    } else {")
    lines.append(
        f"      local1[i] = (local1[i] + g{(idx + 5) % cfg.global_count}[alt] + a{(idx + 7) % cfg.param_count} + 67) % MOD;"
    )
    lines.append(
        f"      g{(idx + 4) % cfg.global_count}[alt] = (g{(idx + 4) % cfg.global_count}[alt] + local0[i] + local1[i] + 71) % MOD;"
    )
    lines.append("    }")
    lines.append(
        f"    acc = (acc + local1[i] + local0[i] + g{(idx + 1) % cfg.global_count}[(slot + i + {idx}) % {cfg.global_size}] + i + alt) % MOD;"
    )
    lines.append("    i = i + 1;")
    lines.append("  }")
    lines.append(f"  if ((acc & 1) == 0) {{")
    lines.append(f"    acc = (acc + local0[0] + local1[{cfg.array_size - 1}]) % MOD;")
    lines.append("  } else {")
    lines.append(f"    acc = (acc + local1[0] + local0[{cfg.array_size - 1}] + {idx + 73}) % MOD;")
    lines.append("  }")
    lines.append("  return acc;")
    lines.append("}")
    lines.append("")
    return lines


def emit_chain(cfg: Config, idx: int) -> list[str]:
    params = ", ".join(f"int a{i}" for i in range(cfg.param_count))
    lines = [
        f"int chain_{idx}({params}) {{",
        f"  int local0[{cfg.array_size}];",
        f"  int local1[{cfg.array_size}];",
        "  int acc;",
        "  int i;",
        "  int slot;",
        "  int alt;",
    ]
    lines.append(
        f"  acc = leaf_{(idx * 7 + 3) % cfg.leaf_count}(" +
        ", ".join(f"a{i}" for i in range(cfg.param_count)) +
        ");"
    )
    lines.append("  i = 0;")
    lines.append(f"  while (i < {cfg.array_size}) {{")
    lines.append(
        f"    slot = (acc + a{(idx * 2) % cfg.param_count} + i * 11 + {idx * 5} + {cfg.global_size * 8}) % {cfg.global_size};"
    )
    lines.append(f"    local0[i] = (g{idx % cfg.global_count}[slot] + acc + i + {idx + 31}) % MOD;")
    lines.append(
        f"    g{(idx + 1) % cfg.global_count}[(slot + {idx} + i * 3) % {cfg.global_size}] = "
        f"(g{(idx + 1) % cfg.global_count}[(slot + {idx} + i * 3) % {cfg.global_size}] + local0[i] + a{(idx + 2) % cfg.param_count} + 37) % MOD;"
    )
    lines.append(
        f"    local1[i] = (g{(idx + 1) % cfg.global_count}[(slot + {idx} + i * 3) % {cfg.global_size}] + "
        f"local0[i] + a{(idx * 7 + 4) % cfg.param_count} + 41) % MOD;"
    )
    lines.append(f"    alt = (slot * 7 + acc + i + {idx + 83}) % {cfg.global_size};")
    lines.append(f"    if (((local1[i] + acc + {idx}) % 4) < 2) {{")
    lines.append(
        f"      local0[i] = (local0[i] + g{(idx + 2) % cfg.global_count}[alt] + a{(idx + 8) % cfg.param_count} + 89) % MOD;"
    )
    lines.append("    } else {")
    lines.append(
        f"      local1[i] = (local1[i] + g{(idx + 3) % cfg.global_count}[alt] + a{(idx + 9) % cfg.param_count} + 97) % MOD;"
    )
    lines.append("    }")
    lines.append(f"    acc = (acc + local1[i] + local0[i] + i + {idx} + alt) % MOD;")
    lines.append("    i = i + 1;")
    lines.append("  }")
    if idx > 0:
        lines.append(f"  if (((acc + {idx}) & 1) == 0) {{")
        lines.append(
            f"    acc = (acc + chain_{idx - 1}(" +
            ", ".join(f"(a{i} + local0[{i % cfg.array_size}] + {idx + i * 3}) % MOD" for i in range(cfg.param_count)) +
            ")) % MOD;"
        )
        lines.append("  } else {")
        lines.append(
            f"    acc = (acc + leaf_{(idx * 13 + 7) % cfg.leaf_count}(" +
            ", ".join(f"(a{i} + local0[{(i + 1) % cfg.array_size}] + {idx + i * 5}) % MOD" for i in range(cfg.param_count)) +
            ")) % MOD;"
        )
        lines.append("  }")
    lines.append(f"  if (((acc + {idx}) % 3) == 0) {{")
    lines.append(
        f"    acc = (acc + leaf_{(idx * 11 + 5) % cfg.leaf_count}(" +
        ", ".join(f"(a{i} + local1[{i % cfg.array_size}] + {idx + i}) % MOD" for i in range(cfg.param_count)) +
        ")) % MOD;"
    )
    lines.append("  } else {")
    lines.append(
        f"    acc = (acc + leaf_{(idx * 17 + 9) % cfg.leaf_count}(" +
        ", ".join(f"(a{i} + local0[{(i + 2) % cfg.array_size}] + local1[{i % cfg.array_size}] + {idx + i * 2}) % MOD" for i in range(cfg.param_count)) +
        ")) % MOD;"
    )
    lines.append("  }")
    lines.append("  i = 0;")
    lines.append(f"  while (i < {cfg.array_size}) {{")
    lines.append(
        f"    slot = (acc + local0[i] + i * 13 + {idx} + {cfg.global_size * 8}) % {cfg.global_size};"
    )
    lines.append(
        f"    g{(idx + 2) % cfg.global_count}[slot] = "
        f"(g{(idx + 2) % cfg.global_count}[slot] + local1[i] + acc + 43) % MOD;"
    )
    lines.append(
        f"    acc = (acc + g{(idx + 2) % cfg.global_count}[slot] + "
        f"g{(idx + 4) % cfg.global_count}[(slot + i + 17) % {cfg.global_size}]) % MOD;"
    )
    lines.append(f"    if (((slot + acc + i) & 1) != 0) {{")
    lines.append(
        f"      g{(idx + 5) % cfg.global_count}[(slot + {idx} + i * 5) % {cfg.global_size}] = "
        f"(g{(idx + 5) % cfg.global_count}[(slot + {idx} + i * 5) % {cfg.global_size}] + acc + 101) % MOD;"
    )
    lines.append("    }")
    lines.append("    i = i + 1;")
    lines.append("  }")
    lines.append("  return acc;")
    lines.append("}")
    lines.append("")
    return lines


def emit_twist(cfg: Config, idx: int) -> list[str]:
    lines = [
        f"int twist_{idx}(int x, int y, int z, int w) {{",
        "  int t0;",
        "  int t1;",
        "  int t2;",
        f"  t0 = (x + y + {idx * 19 + 107}) % MOD;",
        f"  t1 = (z + w + {idx * 23 + 113}) % MOD;",
        "  if (((t0 + t1) & 1) == 0) {",
        f"    t2 = (t0 + t1 + g{idx % cfg.global_count}[(t0 + {idx * 7 + 3} + {cfg.global_size * 8}) % {cfg.global_size}]) % MOD;",
        "  } else {",
        f"    t2 = (t0 + MOD - t1 + g{(idx + 1) % cfg.global_count}[(t1 + {idx * 5 + 9} + {cfg.global_size * 8}) % {cfg.global_size}]) % MOD;",
        "  }",
        "  if ((t2 % 3) == 0) {",
        f"    t2 = (t2 + g{(idx + 2) % cfg.global_count}[(t2 + {idx * 11 + 15} + {cfg.global_size * 8}) % {cfg.global_size}] + 127) % MOD;",
        "  } else {",
        f"    t2 = (t2 + g{(idx + 3) % cfg.global_count}[(t2 * 3 + {idx * 13 + 17} + {cfg.global_size * 8}) % {cfg.global_size}] + 131) % MOD;",
        "  }",
        "  return t2;",
        "}",
        "",
    ]
    return lines


def emit_array_kernel(cfg: Config, idx: int, twist_count: int) -> list[str]:
    twist_a = idx % twist_count
    twist_b = (idx * 3 + 1) % twist_count
    lines = [
        f"int arr_kernel_{idx}(int a[], int b[], int c[], int len, int seed, int bias, int step, int extra) {{",
        "  int i;",
        "  int idx0;",
        "  int idx1;",
        "  int idx2;",
        "  int acc;",
        "  int tmp;",
        "  int noise0;",
        "  int noise1;",
        f"  acc = (seed + bias + step + extra + {idx * 29 + 137}) % MOD;",
        "  i = 0;",
        "  while (i < len) {",
        f"    idx0 = (acc + i * (step + 1) + bias + {cfg.global_size * 8 + idx * 5}) % len;",
        f"    idx1 = (idx0 + step + i + {idx + 3}) % len;",
        f"    idx2 = (idx1 + bias + extra + {idx * 7 + 5}) % len;",
        f"    noise0 = (({idx * 41 + 191} - {idx * 41 + 191}) + 5) * 1;",
        "    noise1 = noise0;",
        "    noise1 = noise1 + 0;",
        f"    tmp = twist_{twist_a}(a[idx0], b[idx1], c[idx2], acc);",
        "    if ((noise1 - 5) != 0) {",
        "      tmp = (tmp + 777777) % MOD;",
        "    } else {",
        "      tmp = (tmp + 0) % MOD;",
        "    }",
        "    a[idx0] = (a[idx0] + tmp + bias + i) % MOD;",
        "    if (((tmp + i + seed) & 1) == 0) {",
        f"      b[idx1] = (b[idx1] + a[idx0] + g{idx % cfg.global_count}[(idx1 + {cfg.global_size * 8 + idx}) % {cfg.global_size}] + 139) % MOD;",
        "    } else {",
        f"      c[idx2] = (c[idx2] + b[idx1] + g{(idx + 1) % cfg.global_count}[(idx2 + {cfg.global_size * 8 + idx * 3}) % {cfg.global_size}] + 149) % MOD;",
        "    }",
        "    if (((a[idx0] + b[idx1] + c[idx2]) % 3) == 0) {",
        f"      acc = (acc + twist_{twist_b}(a[idx0], b[idx1], c[idx2], tmp) + 151) % MOD;",
        "    } else {",
        "      acc = (acc + a[idx0] + b[idx1] + c[idx2] + tmp + 157) % MOD;",
        "    }",
        "    i = i + 1;",
        "  }",
        "  return acc;",
        "}",
        "",
    ]
    return lines


def emit_alias_kernel(cfg: Config, idx: int, twist_count: int) -> list[str]:
    twist_a = (idx * 5 + 1) % twist_count
    twist_b = (idx * 7 + 3) % twist_count
    lines = [
        f"int alias_kernel_{idx}(int x[], int y[], int z[], int w[], int len, int seed, int extra) {{",
        "  int i;",
        "  int p;",
        "  int q;",
        "  int r;",
        "  int acc;",
        "  int t;",
        "  int dead;",
        f"  acc = (seed + extra + {idx * 43 + 211}) % MOD;",
        "  i = 0;",
        "  while (i < len) {",
        f"    p = (acc + i * 3 + {cfg.global_size * 8 + idx}) % len;",
        f"    q = (p + seed + i + {idx + 5}) % len;",
        f"    r = (q + extra + {idx * 9 + 7}) % len;",
        "    dead = x[p];",
        "    dead = y[q];",
        f"    t = twist_{twist_a}(x[p], y[q], z[r], acc);",
        "    if (((t + seed + i) % 3) == 0) {",
        "      x[p] = (x[p] + t + z[r]) % MOD;",
        "      y[q] = (y[q] + x[p] + 193) % MOD;",
        "    } else {",
        "      z[r] = (z[r] + t + w[p]) % MOD;",
        "      w[p] = (w[p] + z[r] + 197) % MOD;",
        "    }",
        "    if (((x[p] + y[q] + z[r] + w[p]) & 1) == 0) {",
        f"      acc = (acc + twist_{twist_b}(x[p], z[r], y[q], w[p])) % MOD;",
        "    } else {",
        "      acc = (acc + x[p] + y[q] + z[r] + w[p] + 199) % MOD;",
        "    }",
        "    i = i + 1;",
        "  }",
        "  return acc;",
        "}",
        "",
    ]
    return lines


def emit_bridge(cfg: Config, idx: int, kernel_count: int, twist_count: int) -> list[str]:
    kernel_a = idx % kernel_count
    kernel_b = (idx * 5 + 2) % kernel_count
    twist_a = (idx * 7 + 1) % twist_count
    alias_a = idx % max(6, kernel_count // 2)
    def param_name(i: int) -> str:
        return f"a{i % cfg.param_count}"
    params = ", ".join(f"int a{i}" for i in range(cfg.param_count))
    lines = [
        f"int bridge_{idx}({params}) {{",
        f"  int loc0[{cfg.array_size}];",
        f"  int loc1[{cfg.array_size}];",
        f"  int loc2[{cfg.array_size}];",
        f"  int loc3[{cfg.array_size}];",
        "  int i;",
        "  int acc;",
        "  int seed;",
        "  int dead;",
        f"  acc = ({' + '.join(f'a{i}' for i in range(min(cfg.param_count, 6)))} + {idx * 31 + 163}) % MOD;",
        f"  seed = twist_{twist_a}({param_name(0)}, {param_name(1)}, {param_name(2)}, {param_name(3)});",
        "  i = 0;",
        f"  while (i < {cfg.array_size}) {{",
        "    dead = i;",
        f"    loc0[i] = (g{idx % cfg.global_count}[(seed + i * 3 + {cfg.global_size * 8 + idx}) % {cfg.global_size}] + acc + i + 167) % MOD;",
        f"    loc0[i] = (g{idx % cfg.global_count}[(seed + i * 3 + {cfg.global_size * 8 + idx}) % {cfg.global_size}] + acc + i + 167) % MOD;",
        f"    loc1[i] = (g{(idx + 1) % cfg.global_count}[(seed + i * 5 + {cfg.global_size * 8 + idx * 3}) % {cfg.global_size}] + loc0[i] + 173) % MOD;",
        f"    loc2[i] = (g{(idx + 2) % cfg.global_count}[(seed + i * 7 + {cfg.global_size * 8 + idx * 5}) % {cfg.global_size}] + loc1[i] + 179) % MOD;",
        f"    loc3[i] = (loc0[i] + loc1[i] + loc2[i] + {idx * 13 + 181}) % MOD;",
        "    if ((0 + 1 - 1) != 0) {",
        "      loc3[i] = (loc3[i] + 123456) % MOD;",
        "    }",
        "    i = i + 1;",
        "  }",
        f"  acc = (acc + arr_kernel_{kernel_a}(loc0, loc1, loc2, {cfg.array_size}, seed, {param_name(4)}, {param_name(5)}, {param_name(6)})) % MOD;",
        "  if (((acc + seed) & 1) == 0) {",
        f"    acc = (acc + arr_kernel_{kernel_b}(loc2, loc0, g{(idx + 3) % cfg.global_count}, {cfg.array_size}, acc, {param_name(7)}, {param_name(8)}, {param_name(9)})) % MOD;",
        "  } else {",
        f"    acc = (acc + arr_kernel_{(idx * 7 + 3) % kernel_count}(g{(idx + 4) % cfg.global_count}, loc1, loc2, {cfg.array_size}, acc, {param_name(7)}, {param_name(8)}, {param_name(9)})) % MOD;",
        "  }",
        f"  acc = (acc + alias_kernel_{alias_a}(loc0, loc1, loc2, loc3, {cfg.array_size}, acc, seed)) % MOD;",
        f"  if (((acc + seed + {idx}) % 4) == 0) {{",
        f"    acc = (acc + alias_kernel_{(alias_a + 1) % max(6, kernel_count // 2)}(loc2, loc0, loc3, g{(idx + 5) % cfg.global_count}, {cfg.array_size}, acc, {param_name(10)})) % MOD;",
        "  }",
    ]
    if idx > 0:
        lines.extend([
            "  if ((acc % 3) == 0) {",
            f"    acc = (acc + bridge_{idx - 1}(" +
            ", ".join(f"(a{i} + loc0[{i % cfg.array_size}] + loc1[{(i + 1) % cfg.array_size}] + {idx + i}) % MOD" for i in range(cfg.param_count)) +
            ")) % MOD;",
            "  }",
        ])
    lines.extend([
        f"  acc = (acc + leaf_{(idx * 19 + 7) % cfg.leaf_count}(" +
        ", ".join(f"(a{i} + loc2[{i % cfg.array_size}] + {idx + i * 3}) % MOD" for i in range(cfg.param_count)) +
        ")) % MOD;",
        "  return acc;",
        "}",
        "",
    ])
    return lines


def emit_main(cfg: Config) -> list[str]:
    lines = ["int main() {", "  int ans;", "  int i;"]
    for g in range(cfg.global_count):
        lines.append(f"  i = 0; while (i < {cfg.global_size}) {{ g{g}[i] = ({cfg.seed + g * 97} + i * 31) % MOD; i = i + 1; }}")
    lines.append("  ans = 0;")
    lines.append("  i = 0;")
    lines.append(f"  while (i < {cfg.main_iters}) {{")
    call_args = ", ".join(
        f"(g{p % cfg.global_count}[(i + {p * 5}) % {cfg.global_size}] + {cfg.seed + p * 97 + 53}) % MOD"
        for p in range(cfg.param_count)
    )
    lines.append(f"    if (((ans + i + {cfg.seed}) & 1) == 0) {{")
    lines.append(f"      ans = (ans + chain_{cfg.chain_count - 1}({call_args})) % MOD;")
    lines.append("    } else {")
    lines.append(f"      ans = (ans + chain_{cfg.chain_count // 2}({call_args})) % MOD;")
    lines.append("    }")
    lines.append(f"    if (((ans + i) % 3) == 0) {{")
    lines.append(
        f"      ans = (ans + leaf_{(cfg.leaf_count * 7 + 5) % cfg.leaf_count}(" +
        ", ".join(f"(g{(p + 1) % cfg.global_count}[(ans + i + {p * 3}) % {cfg.global_size}] + {cfg.seed + p * 11}) % MOD" for p in range(cfg.param_count)) +
        ")) % MOD;"
    )
    lines.append("    }")
    for g in range(cfg.global_count):
        lines.append(
            f"    g{g}[(ans + i * 7 + {g * 13} + {cfg.global_size * 8}) % {cfg.global_size}] = "
            f"(g{g}[(ans + i * 7 + {g * 13} + {cfg.global_size * 8}) % {cfg.global_size}] + ans + {g} + i) % MOD;"
        )
    lines.append("    i = i + 1;")
    lines.append("  }")
    lines.append("  putint(ans);")
    lines.append("  putch(10);")
    lines.append("  return 0;")
    lines.append("}")
    return lines


def generate_source(cfg: Config) -> str:
    twist_count = max(6, cfg.leaf_count // 3)
    kernel_count = max(6, cfg.chain_count // 4)
    alias_count = max(6, kernel_count // 2)
    bridge_count = max(6, cfg.chain_count // 5)
    lines: list[str] = []
    lines.extend(emit_header(cfg))
    for idx in range(twist_count):
        lines.extend(emit_twist(cfg, idx))
    for idx in range(kernel_count):
        lines.extend(emit_array_kernel(cfg, idx, twist_count))
    for idx in range(alias_count):
        lines.extend(emit_alias_kernel(cfg, idx, twist_count))
    for idx in range(cfg.leaf_count):
        lines.extend(emit_leaf(cfg, idx))
    for idx in range(cfg.chain_count):
        lines.extend(emit_chain(cfg, idx))
    for idx in range(bridge_count):
        lines.extend(emit_bridge(cfg, idx, kernel_count, twist_count))
    lines.append("")
    # inject bridge usage before final output
    main_lines = emit_main(cfg)
    insert_at = main_lines.index("  putint(ans);")
    bridge_args = ", ".join(
        f"(g{p % cfg.global_count}[(ans + i + {p * 7}) % {cfg.global_size}] + {cfg.seed + p * 13 + 181}) % MOD"
        for p in range(cfg.param_count)
    )
    main_lines[insert_at:insert_at] = [
        f"  if (((ans + {cfg.seed}) % 5) <= 2) {{",
        f"    ans = (ans + bridge_{bridge_count - 1}({bridge_args})) % MOD;",
        "  } else {",
        f"    ans = (ans + bridge_{bridge_count // 2}({bridge_args})) % MOD;",
        "  }",
    ]
    lines.extend(main_lines)
    return "\n".join(lines) + "\n"


def build_host_oracle(source_text: str) -> str:
    wrapper = (
        "#include <stdio.h>\n"
        "int putint(int x) { printf(\"%d\", x); return 0; }\n"
        "int putch(int x) { putchar(x); return 0; }\n"
    )

    with tempfile.TemporaryDirectory(prefix="sysy-runtime-re-stress-") as td:
        tmp = Path(td)
        src = tmp / "stress.c"
        exe = tmp / "stress.out"
        src.write_text(wrapper + source_text, encoding="utf-8")
        compile_proc = subprocess.run(
            ["cc", "-O0", str(src), "-o", str(exe)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if compile_proc.returncode != 0:
            raise RuntimeError(f"host oracle compile failed:\n{compile_proc.stderr}")
        run_proc = subprocess.run(
            [str(exe)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if run_proc.returncode != 0:
            raise RuntimeError(f"host oracle run failed:\n{run_proc.stderr}")
        return run_proc.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate large SysY stress programs for runtime-RE/segfault hunting.")
    parser.add_argument("--leaf-count", type=int, default=32)
    parser.add_argument("--chain-count", type=int, default=48)
    parser.add_argument("--array-size", type=int, default=8)
    parser.add_argument("--global-count", type=int, default=6)
    parser.add_argument("--global-size", type=int, default=64)
    parser.add_argument("--params", type=int, default=12)
    parser.add_argument("--main-iters", type=int, default=6)
    parser.add_argument("--seed", type=int, default=17)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--out-file", type=Path)
    args = parser.parse_args()

    if args.leaf_count <= 0 or args.chain_count <= 0 or args.array_size <= 0 or args.global_count <= 0 or args.global_size <= 1 or args.params <= 0:
        print("all size/count arguments must be positive and global-size > 1", file=sys.stderr)
        return 2
    if args.global_size < args.array_size:
        print(
            "global-size must be >= array-size so generated global-array helper calls stay in-bounds",
            file=sys.stderr,
        )
        return 2

    cfg = Config(
        leaf_count=args.leaf_count,
        chain_count=args.chain_count,
        array_size=args.array_size,
        global_count=args.global_count,
        global_size=args.global_size,
        param_count=args.params,
        main_iters=args.main_iters,
        seed=args.seed,
    )

    source_text = generate_source(cfg)
    args.output.write_text(source_text, encoding="utf-8")

    if args.out_file is not None:
        args.out_file.write_text(build_host_oracle(source_text), encoding="utf-8")

    line_count = args.output.read_text(encoding="utf-8").count("\n")
    print(
        f"generated {args.output} lines={line_count} leafs={cfg.leaf_count} chains={cfg.chain_count} "
        f"array_size={cfg.array_size} params={cfg.param_count}"
    )
    if args.out_file is not None:
        print(f"oracle {args.out_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
