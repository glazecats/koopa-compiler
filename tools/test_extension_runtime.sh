#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPILER_BIN="$ROOT_DIR/build/compiler"

if [[ ! -x "$COMPILER_BIN" ]]; then
  echo "[extension-runtime] FAIL: missing compiler binary at $COMPILER_BIN" >&2
  exit 1
fi

tmpdir="$(mktemp -d)"
COMPILER_SNAPSHOT="$tmpdir/compiler"
cleanup() {
  rm -rf "$tmpdir"
}
trap cleanup EXIT

cp "$COMPILER_BIN" "$COMPILER_SNAPSHOT"
chmod +x "$COMPILER_SNAPSHOT"

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

  "$COMPILER_SNAPSHOT" -extension "$source_path" -o "$asm_path"
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

run_case "fndefer_runs_at_function_exit" "2" <<'EOF'
int main(){ int x=1; fndefer putint(x); x=2; return 0; }
EOF

run_case "fndefer_is_lifo_and_after_scope_defer" "132" <<'EOF'
int main(){ defer putint(1); fndefer putint(2); fndefer putint(3); return 0; }
EOF

run_case "fndefer_return_value_is_frozen_before_unwind" "" "1" <<'EOF'
int main(){ int x=1; fndefer x=2; return x; }
EOF

run_case "fndefer_replays_each_loop_registration" "33333333" <<'EOF'
int a[8]={1,2,3,4,5,6,7,8};
int main(){ for(int i=0;i<8;i=i+1){ fndefer putint(a[2]); } return 0; }
EOF

run_case "capdefer_snapshot_value_is_used_at_function_exit" "1" <<'EOF'
int main(){ int x=1; capdefer(y=x) putint(y); x=2; return 0; }
EOF

run_case "capdefer_multiple_snapshot_values_are_used_at_function_exit" "3" <<'EOF'
int main(){ int x=1; int z=2; capdefer(y=x, w=z) putint(y+w); x=3; z=4; return 0; }
EOF

run_case "function_value_param_unused_top_level_function_arg" "" "41" <<'EOF'
int add1(int x){ return x+1; }
int apply(int f(int), int x){ return x; }
int main(){ return apply(add1, 41); }
EOF

run_case "function_value_param_direct_call_specialization" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int apply(int f(int), int x){ return f(x); }
int main(){ return apply(add1, 41); }
EOF

run_case "function_value_param_direct_call_specialization_two_bindings" "" "16" <<'EOF'
int add1(int x){ return x+1; }
int double1(int x){ return x*2; }
int apply(int f(int), int x){ return f(x); }
int main(){ return apply(add1, 3) + apply(double1, 6); }
EOF

run_case "function_value_param_forwarding_specialization" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int apply(int f(int), int x){ return f(x); }
int wrapper(int f(int), int x){ return apply(f, x); }
int main(){ return wrapper(add1, 41); }
EOF

run_case "function_value_param_multiple_bindings_compose" "" "41" <<'EOF'
int add1(int x){ return x+1; }
int double1(int x){ return x*2; }
int compose(int f(int), int g(int), int x){ return f(g(x)); }
int main(){ return compose(add1, double1, 20); }
EOF

run_case "function_value_param_void_builtin_binding" "7" <<'EOF'
void apply(void f(int), int x){ f(x); }
int main(){ apply(putint, 7); return 0; }
EOF

run_case "function_value_param_zero_arg_binding" "" "9" <<'EOF'
int next(){ return 9; }
int apply0(int f()){ return f(); }
int main(){ return apply0(next); }
EOF

run_case "function_value_param_zero_arg_void_binding" "1" <<'EOF'
void ping(){ putint(1); }
void apply0(void f()){ f(); }
int main(){ apply0(ping); return 0; }
EOF

run_case "pair_local_fields_and_initializer" "7" "4" <<'EOF'
int main(){ pair p={3,4}; p.first = p.first + p.second; putint(p.first); return p.second; }
EOF

run_case "pair_local_copy_initializer_and_assignment" "1" "2" <<'EOF'
int main(){ pair a={1,2}; pair b=a; b=a; putint(b.first); return b.second; }
EOF

run_case "struct_local_copy_initializer_and_assignment" "13" "2" <<'EOF'
struct Point { int x; int y; int z; };
int main(){ struct Point a={1,2,3}; struct Point b=a; b=a; putint(b.x); putint(b.z); return b.y; }
EOF

echo "[extension-runtime] All extension runtime regressions passed."
