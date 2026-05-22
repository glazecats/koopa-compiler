#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPILER_BIN="$ROOT_DIR/build/compiler"

if [[ ! -x "$COMPILER_BIN" ]]; then
  echo "[extension-runtime] FAIL: missing compiler binary at $COMPILER_BIN" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

run_case() {
  local case_id="$1"
  local expected="$2"
  local expected_status="${3:-0}"
  local source_path="$tmpdir/$case_id.sy"
  local asm_path="$tmpdir/$case_id.s"
  local obj_path="$tmpdir/$case_id.o"
  local exe_path="$tmpdir/$case_id.out"
  local stdout_path="$tmpdir/$case_id.stdout"

  cat >"$source_path"

  "$COMPILER_BIN" -extension "$source_path" -o "$asm_path"
  clang "$asm_path" -c -o "$obj_path" -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
  ld.lld "$obj_path" -L/opt/lib/riscv32 -lsysy -o "$exe_path"
  local actual_status
  set +e
  qemu-riscv32-static "$exe_path" >"$stdout_path"
  actual_status=$?
  set -e

  local actual
  actual="$(cat "$stdout_path")"
  if [[ "$actual" != "$expected" ]]; then
    echo "[extension-runtime] FAIL: $case_id expected '$expected' got '$actual'" >&2
    exit 1
  fi
  if [[ "$actual_status" != "$expected_status" ]]; then
    echo "[extension-runtime] FAIL: $case_id expected exit '$expected_status' got '$actual_status'" >&2
    exit 1
  fi

  echo "[extension-runtime] PASS: $case_id"
}

run_case "defer_return_order" "132" <<'EOF'
int main(){ putint(1); defer putint(2); putint(3); return 0; }
EOF

run_case "defer_compound_body_order" "123" <<'EOF'
int main(){ putint(1); defer { putint(2); putint(3); } return 0; }
EOF

run_case "defer_lifo_order" "321" <<'EOF'
int main(){ defer putint(1); defer putint(2); defer putint(3); return 0; }
EOF

run_case "defer_nested_order" "321" <<'EOF'
int main(){ defer putint(1); { defer putint(2); defer putint(3); } return 0; }
EOF

run_case "defer_break_order" "1" <<'EOF'
int main(){ while(1){ defer putint(1); break; } return 0; }
EOF

run_case "defer_continue_order" "12" <<'EOF'
int main(){ int i=0; while(i<2){ { defer putint(i); i=i+1; continue; } } return 0; }
EOF

run_case "defer_break_outer_then_inner_order" "21" <<'EOF'
int main(){ defer putint(1); while(1){ defer putint(2); break; } return 0; }
EOF

run_case "defer_continue_nested_outer_order" "121" <<'EOF'
int main(){ defer putint(1); int i=0; while(i<2){ { defer putint(i); i=i+1; continue; } } return 0; }
EOF

run_case "defer_loop_body_fallthrough_order" "12" <<'EOF'
int main(){ int i=0; while(i<2){ { defer putint(i); i=i+1; } } return 0; }
EOF

run_case "defer_continue_multi_inner_lifo" "2121" <<'EOF'
int main(){ int i=0; while(i<2){ { defer putint(1); defer putint(2); i=i+1; continue; } } return 0; }
EOF

run_case "defer_break_nested_outer_inner_lifo" "321" <<'EOF'
int main(){ defer putint(1); while(1){ { defer putint(2); defer putint(3); break; } } return 0; }
EOF

run_case "defer_body_internal_break_loop" "12" <<'EOF'
int main(){ defer { while(1){ putint(1); break; } putint(2); } return 0; }
EOF

run_case "defer_body_internal_continue_loop" "2" <<'EOF'
int main(){ defer { int i=0; while(i<2){ i=i+1; if(i<2) continue; putint(i); } } return 0; }
EOF

run_case "defer_nested_defer_body" "21" <<'EOF'
int main(){ defer { defer putint(1); putint(2); } return 0; }
EOF

run_case "defer_nested_multi_defer_body" "231" <<'EOF'
int main(){ defer { defer putint(1); { defer putint(2); } putint(3); } return 0; }
EOF

run_case "defer_nested_defer_body_lifo" "3421" <<'EOF'
int main(){ defer { defer putint(1); defer putint(2); { defer putint(3); } putint(4); } return 0; }
EOF

run_case "defer_reads_exit_time_local_value" "2" <<'EOF'
int main(){ int x=1; defer putint(x); x=2; return 0; }
EOF

run_case "defer_reads_exit_time_inner_scope_local_value" "2" <<'EOF'
int main(){ int x=1; { defer putint(x); x=2; } return 0; }
EOF

run_case "defer_if_else_uses_exit_time_condition_value" "1" <<'EOF'
int main(){ int x=0; defer if(x) putint(1); else putint(2); x=1; return 0; }
EOF

run_case "defer_if_else_compound_uses_exit_time_condition_value" "1" <<'EOF'
int main(){ int x=0; defer if(x){ putint(1); } else { putint(2); } x=1; return 0; }
EOF

run_case "defer_return_unwinds_inner_then_outer_scope" "21" <<'EOF'
int main(){ defer putint(1); { defer putint(2); return 0; } }
EOF

run_case "defer_return_unwinds_branch_inner_then_outer_scope" "21" <<'EOF'
int main(){ defer putint(1); if(1){ defer putint(2); return 0; } return 0; }
EOF

run_case "defer_return_expression_value_is_frozen_before_defer" "" "1" <<'EOF'
int main(){ int x=1; defer x=2; return x; }
EOF

run_case "defer_return_expression_and_deferred_read_both_use_pre_defer_value" "1" "1" <<'EOF'
int main(){ int x=1; defer x=2; defer putint(x); return x; }
EOF

run_case "defer_return_expression_side_effect_happens_before_defer" "" "2" <<'EOF'
int main(){ int x=1; defer x=3; return x=2; }
EOF

run_case "defer_return_expression_side_effect_visible_to_later_defer" "3" "2" <<'EOF'
int main(){ int x=1; defer putint(x); defer x=3; return x=2; }
EOF

run_case "defer_for_body_runs_before_for_step" "01" <<'EOF'
int main(){ int i=0; for(;i<2;i=i+1){ defer putint(i); } return 0; }
EOF

run_case "defer_for_return_unwinds_body_then_outer_without_step" "21" <<'EOF'
int main(){ defer putint(1); for(int i=0;i<3;i=i+1){ defer putint(2); return 0; } return 0; }
EOF

run_case "defer_for_break_unwinds_body_then_outer_without_step" "21" <<'EOF'
int main(){ defer putint(1); for(int i=0;i<3;i=i+1){ defer putint(2); break; } return 0; }
EOF

run_case "defer_with_global_init_runtime_regression" "1122334455667788" <<'EOF'
int a[8]={1,2,3,4,5,6,7,8};
int main() {
    for(int i=0;i<8;i++) {
        putint(a[i]);
        defer putint(a[i]);
    }
    int x=1;
    int t=7;
    while(a[0]) {
        a[t--]--;
        t=(t+8)%8;
    }
    return 0;
}
EOF

run_case "unless_executes_when_false" "1" <<'EOF'
int main(){ unless(0) putint(1); return 0; }
EOF

run_case "unless_skips_when_true" "" <<'EOF'
int main(){ int x=1; unless(x) putint(1); return 0; }
EOF

echo "[extension-runtime] All extension runtime regressions passed."
