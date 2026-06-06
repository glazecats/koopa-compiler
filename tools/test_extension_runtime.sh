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

run_case "pair_parameter_reads_fields" "9" <<'EOF'
int sum(pair p){ return p.first + p.second; }
int main(){ pair p={4,5}; putint(sum(p)); return 0; }
EOF

run_case "struct_parameter_reads_fields" "13" <<'EOF'
struct Point { int x; int y; };
int sum(struct Point p){ return p.x + p.y; }
int main(){ struct Point p={6,7}; putint(sum(p)); return 0; }
EOF

run_case "nested_struct_parameter_reads_fields" "6" <<'EOF'
struct Mid { pair p; int z; };
int sum(struct Mid m){ return m.p.first + m.p.second + m.z; }
int main(){ struct Mid m={{1,2},3}; putint(sum(m)); return 0; }
EOF

run_case "pair_return_copy_init" "1" "2" <<'EOF'
pair mk(){ pair p={1,2}; return p; }
int main(){ pair q = mk(); putint(q.first); return q.second; }
EOF

run_case "struct_return_copy_init" "1" "2" <<'EOF'
struct Point { int x; int y; };
struct Point mk(){ struct Point p={1,2}; return p; }
int main(){ struct Point q = mk(); putint(q.x); return q.y; }
EOF

run_case "pair_return_assignment" "1" "2" <<'EOF'
pair mk(){ pair p={1,2}; return p; }
int main(){ pair q={7,8}; q = mk(); putint(q.first); return q.second; }
EOF

run_case "struct_return_assignment" "1" "2" <<'EOF'
struct Point { int x; int y; };
struct Point mk(){ struct Point p={1,2}; return p; }
int main(){ struct Point q={7,8}; q = mk(); putint(q.x); return q.y; }
EOF

run_case "pair_parameter_to_return_forwarding" "1" "2" <<'EOF'
pair id(pair p){ return p; }
int main(){ pair q={1,2}; pair r=id(q); putint(r.first); return r.second; }
EOF

run_case "struct_parameter_to_return_forwarding" "1" "2" <<'EOF'
struct Point { int x; int y; };
struct Point id(struct Point p){ return p; }
int main(){ struct Point q={1,2}; struct Point r=id(q); putint(r.x); return r.y; }
EOF

run_case "nested_struct_return_copy_init" "5" <<'EOF'
struct Mid { pair p; int z; };
struct Mid mk(){ struct Mid m={{1,2},3}; return m; }
int main(){ struct Mid q=mk(); putint(q.p.second + q.z); return 0; }
EOF

run_case "pair_call_result_aggregate_call_argument" "" "3" <<'EOF'
pair mk(){ pair p={1,2}; return p; }
int sum(pair p){ return p.first + p.second; }
int main(){ return sum(mk()); }
EOF

run_case "struct_call_result_aggregate_call_argument" "" "3" <<'EOF'
struct Point { int x; int y; };
struct Point mk(){ struct Point p={1,2}; return p; }
int sum(struct Point p){ return p.x + p.y; }
int main(){ return sum(mk()); }
EOF

run_case "nested_struct_call_result_aggregate_call_argument" "" "6" <<'EOF'
struct Mid { pair p; int z; };
struct Mid mk(){ struct Mid m={{1,2},3}; return m; }
int sum(struct Mid m){ return m.p.first + m.p.second + m.z; }
int main(){ return sum(mk()); }
EOF

run_case "pair_multi_hop_return_chain" "" "2" <<'EOF'
pair mk(){ pair p={1,2}; return p; }
pair id(pair p){ return p; }
pair wrap(){ return id(mk()); }
int main(){ pair q=wrap(); return q.second; }
EOF

run_case "nested_struct_multi_hop_return_chain" "" "5" <<'EOF'
struct Mid { pair p; int z; };
struct Mid mk(){ struct Mid m={{1,2},3}; return m; }
struct Mid id(struct Mid m){ return m; }
struct Mid wrap(){ return id(mk()); }
int main(){ struct Mid q=wrap(); return q.p.second + q.z; }
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

run_case "local_function_value_zero_arg_binding" "" "9" <<'EOF'
int next(){ return 9; }
int main(){ int f()=next; return f(); }
EOF

run_case "local_function_value_zero_arg_void_binding" "1" <<'EOF'
void ping(){ putint(1); return; }
int main(){ void f()=ping; f(); return 0; }
EOF

run_case "local_function_value_builtin_binding" "7" <<'EOF'
int main(){ void f(int)=putint; f(7); return 0; }
EOF

run_case "local_function_value_forward_into_function_param" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int apply(int f(int), int x){ return f(x); }
int main(){ int g(int)=add1; return apply(g, 41); }
EOF

run_case "dynamic_local_function_value_forward_into_function_param" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int g(int)=add1; if((putint(0), c)) g=add2; return apply(g, 40); }
EOF

run_case "dynamic_returned_function_value_forward_into_function_param" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int pick(int c)(int){ int f(int)=add1; if(c) f=add2; return f; }
int main(){ int g(int)=pick(1); return apply(g, 40); }
EOF

run_case "dynamic_local_zero_arg_function_value_forward_into_function_param" "0" "2" <<'EOF'
int next1(){ return 1; }
int next2(){ return 2; }
int apply0(int f()){ return f(); }
int main(){ int c=1; int g()=next1; if((putint(0), c)) g=next2; return apply0(g); }
EOF

run_case "dynamic_local_zero_arg_void_function_value_forward_into_function_param" "02" <<'EOF'
void ping1(){ putint(1); }
void ping2(){ putint(2); }
void apply0(void f()){ f(); }
int main(){ int c=1; void g()=ping1; if((putint(0), c)) g=ping2; apply0(g); return 0; }
EOF

run_case "dynamic_returned_zero_arg_function_value_forward_into_function_param" "" "2" <<'EOF'
int next1(){ return 1; }
int next2(){ return 2; }
int apply0(int f()){ return f(); }
int pick(int c)(){ int g()=next1; if(c) g=next2; return g; }
int main(){ int g()=pick(1); return apply0(g); }
EOF

run_case "dynamic_function_value_assignment_then_direct_call" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; h=f; return h(40); }
EOF

run_case "ternary_function_value_assignment_then_direct_call" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=add1; h=((putint(0), c) ? f : g); return h(40); }
EOF

run_case "ternary_closure_assignment_then_direct_call" "0" "7" <<'EOF'
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; int h(int)=f; h=((putint(0), c) ? f : g); return h(4); }
EOF

run_case "dynamic_function_value_assignment_then_forward_into_function_param" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; h=f; return apply(h, 40); }
EOF

run_case "dynamic_function_value_assignment_then_return" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; h=f; return h; }
int main(){ return pick(1)(40); }
EOF

run_case "ternary_function_value_local_initializer" "" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=c ? f : g; return h(40); }
EOF

run_case "comma_wrapped_function_value_direct_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int main(){ int g(int)=add1; return (0, g)(41); }
EOF

run_case "assignment_result_function_value_direct_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int f(int)=add1; int h(int)=add1; return (h=f)(41); }
EOF

run_case "dynamic_comma_wrapped_function_value_forward_into_parameter" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int g(int)=add1; if((putint(0), c)) g=add2; return apply((0, g), 40); }
EOF

run_case "dynamic_assignment_result_function_value_forward_into_parameter" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int f(int)=add1; int h(int)=add1; if((putint(0), c)) f=add2; return apply((h=f), 40); }
EOF

run_case "ternary_function_value_actual_argument" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(((putint(0), c) ? f : g), 40); }
EOF

run_case "ternary_closure_actual_argument" "0" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }
EOF

run_case "ternary_function_value_callee" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; int g(int)=add2; return (((putint(0), c) ? f : g))(40); }
EOF

run_case "ternary_two_arg_function_value_callee" "0" "13" <<'EOF'
int add(int x,int y){ return x+y; }
int sub(int x,int y){ return x-y; }
int main(){ int c=1; int f(int,int)=add; int g(int,int)=sub; return (((putint(0), c) ? f : g))(9,4); }
EOF

run_case "ternary_closure_callee" "0" "7" <<'EOF'
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return (((putint(0), c) ? f : g))(4); }
EOF

run_case "returned_closure_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ int f(int)=make(3); return apply(f, 4); }
EOF

run_case "mixed_returned_closure_function_args_runtime" "" "8" <<'EOF'
int compose(int f(int), int g(int), int x){ return f(g(x)); }
int pick(int c, int f(int))(int){ int a(int)=closure [f] int (int y){ return f(y); }; int b(int)=closure [f] int (int y){ return f(f(y)); }; if(c) a=b; return a; }
int make(int x)(int){ return closure [x] int (int y){ return x+y; }; }
int add1(int x){ return x+1; }
int main(){ return compose(pick(0, add1), make(3), 4); }
EOF

run_case "returned_closure_zero_arg_forward_into_function_param" "" "3" <<'EOF'
int apply0(int f()){ return f(); }
int make(int x)() { return closure [x] int () { return x; }; }
int main(){ int f()=make(3); return apply0(f); }
EOF

run_case "returned_closure_void_forward_into_function_param" "7" <<'EOF'
void apply0(void f()){ f(); }
void make(int x)() { return closure [x] void () { putint(x); return; }; }
int main(){ void f()=make(7); apply0(f); return 0; }
EOF

run_case "direct_returned_noncapturing_function_value_arg" "" "5" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int add1(int x){ return x+1; }
int pick()(int){ return add1; }
int main(){ return apply(pick(), 4); }
EOF

run_case "returned_function_value_parameter_immediate_call" "" "42" <<'EOF'
int id(int f(int))(int) { return f; }
int add1(int x){ return x+1; }
int main(){ return id(add1)(41); }
EOF

run_case "returned_function_value_parameter_bind_and_call" "" "42" <<'EOF'
int id(int f(int))(int) { return f; }
int add1(int x){ return x+1; }
int main(){ int g(int)=id(add1); return g(41); }
EOF

run_case "dynamic_returned_function_value_parameter_immediate_call" "" "42" <<'EOF'
int id(int f(int))(int) { return f; }
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; if(c) f=add2; return id(f)(40); }
EOF

run_case "dynamic_returned_function_value_parameter_bind_and_call" "" "42" <<'EOF'
int id(int f(int))(int) { return f; }
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; if(c) f=add2; int g(int)=id(f); return g(40); }
EOF

run_case "returned_closure_backed_function_value_parameter_immediate_call" "" "7" <<'EOF'
int id(int f(int))(int) { return f; }
int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(4); }
EOF

run_case "returned_closure_backed_function_value_parameter_bind_and_call" "" "7" <<'EOF'
int id(int f(int))(int) { return f; }
int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=id(f); return g(4); }
EOF

run_case "dynamic_returned_closure_backed_function_value_parameter_immediate_call" "" "9" <<'EOF'
int id(int f(int))(int) { return f; }
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(4); }
EOF

run_case "dynamic_returned_closure_backed_function_value_parameter_bind_and_call" "" "9" <<'EOF'
int id(int f(int))(int) { return f; }
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; int h(int)=id(f); return h(4); }
EOF

run_case "direct_returned_noncapturing_zero_arg_function_value_arg" "" "9" <<'EOF'
int apply0(int f()){ return f(); }
int next(){ return 9; }
int pick()(){ return next; }
int main(){ return apply0(pick()); }
EOF

run_case "direct_returned_noncapturing_zero_arg_void_function_value_arg" "7" <<'EOF'
void apply0(void f()){ f(); }
void ping(){ putint(7); }
void pick()(){ return ping; }
int main(){ apply0(pick()); return 0; }
EOF

run_case "direct_returned_closure_function_value_arg" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ return apply(make(3), 4); }
EOF

run_case "direct_returned_closure_zero_arg_function_value_arg" "" "3" <<'EOF'
int apply0(int f()){ return f(); }
int make(int x)() { return closure [x] int () { return x; }; }
int main(){ return apply0(make(3)); }
EOF

run_case "direct_returned_closure_zero_arg_void_function_value_arg" "7" <<'EOF'
void apply0(void f()){ f(); }
void make(int x)() { return closure [x] void () { putint(x); return; }; }
int main(){ apply0(make(7)); return 0; }
EOF

run_case "direct_dynamic_returned_noncapturing_function_value_arg" "" "42" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int){ int f(int)=add1; if(c) f=add2; return f; }
int main(){ return apply(pick(1), 40); }
EOF

run_case "direct_dynamic_returned_noncapturing_two_arg_function_value_arg" "" "5" <<'EOF'
int apply(int f(int,int), int x, int y){ return f(x,y); }
int add(int x,int y){ return x+y; }
int sub(int x,int y){ return x-y; }
int pick(int c)(int,int){ int f(int,int)=add; if(c) f=sub; return f; }
int main(){ return apply(pick(1), 9, 4); }
EOF

run_case "direct_dynamic_returned_noncapturing_zero_arg_function_value_arg" "" "2" <<'EOF'
int apply0(int f()){ return f(); }
int next1(){ return 1; }
int next2(){ return 2; }
int pick(int c)(){ int g()=next1; if(c) g=next2; return g; }
int main(){ return apply0(pick(1)); }
EOF

run_case "direct_dynamic_returned_noncapturing_zero_arg_void_function_value_arg" "2" <<'EOF'
void apply0(void f()){ f(); }
void ping1(){ putint(1); }
void ping2(){ putint(2); }
void pick(int c)(){ void g()=ping1; if(c) g=ping2; return g; }
int main(){ apply0(pick(1)); return 0; }
EOF

run_case "returned_closure_alias_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ int f(int)=make(3); int g(int)=f; return apply(g, 4); }
EOF

run_case "closure_assignment_then_direct_call" "" "7" <<'EOF'
int main(){ int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; g=f; return g(4); }
EOF

run_case "closure_assignment_then_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int y=5; int x=3; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; g=f; return apply(g, 4); }
EOF

run_case "returned_closure_assignment_then_direct_call" "" "7" <<'EOF'
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ int f(int)=make(3); int g(int)=make(5); g=f; return g(4); }
EOF

run_case "returned_closure_assignment_then_return" "" "7" <<'EOF'
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int pick()(int){ int f(int)=make(3); int g(int)=make(5); g=f; return g; }
int main(){ return pick()(4); }
EOF

run_case "dynamic_closure_local_dispatch" "" "9" <<'EOF'
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return f(4); }
EOF

run_case "ternary_closure_local_initializer" "" "7" <<'EOF'
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; int h(int)=c ? f : g; return h(4); }
EOF

run_case "dynamic_closure_local_forward_into_function_param" "" "9" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return apply(f, 4); }
EOF

run_case "dynamic_runtime_closure_local_forward_into_function_param" "0" "9" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if((putint(0), c)) f=g; return apply(f, 4); }
EOF

run_case "dynamic_runtime_zero_arg_void_closure_local_forward_into_function_param" "09" <<'EOF'
void apply0(void f()){ f(); }
int main(){ int c=1; int x=7; int y=9; void f()=closure [x] void () { putint(x); return; }; void g()=closure [y] void () { putint(y); return; }; if((putint(0), c)) f=g; apply0(f); return 0; }
EOF

run_case "local_function_value_alias_chain" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int main(){ int f(int)=add1; int g(int)=f; return g(41); }
EOF

run_case "local_function_value_reassignment" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int f(int)=add1; f=add2; return f(40); }
EOF

run_case "dynamic_local_function_value_dispatch_else" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=0; int f(int)=add1; if((putint(0), c)) f=add2; return f(40); }
EOF

run_case "dynamic_local_function_value_dispatch_then" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; if((putint(0), c)) f=add2; return f(40); }
EOF

run_case "dynamic_local_two_arg_function_value_dispatch_then" "0" "5" <<'EOF'
int add(int x,int y){ return x+y; }
int sub(int x,int y){ return x-y; }
int main(){ int c=1; int f(int,int)=add; if((putint(0), c)) f=sub; return f(9,4); }
EOF

run_case "dynamic_local_function_value_alias_dispatch_else" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=0; int f(int)=add1; if((putint(0), c)) f=add2; int g(int)=f; return g(40); }
EOF

run_case "dynamic_local_function_value_alias_dispatch_then" "0" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int main(){ int c=1; int f(int)=add1; if((putint(0), c)) f=add2; int g(int)=f; return g(40); }
EOF

run_case "closure_direct_local_call" "" "7" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return f(4); }
EOF

run_case "closure_capture_snapshot_value" "" "7" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; x=10; return f(4); }
EOF

run_case "closure_zero_arg_direct_local_call" "" "3" <<'EOF'
int main(){ int x=3; int f()=closure [x] int () { return x; }; return f(); }
EOF

run_case "closure_multi_capture_direct_local_call" "" "7" <<'EOF'
int main(){ int x=3; int y=4; int f()=closure [x,y] int () { return x + y; }; return f(); }
EOF

run_case "closure_multi_capture_two_arg_direct_local_call" "" "18" <<'EOF'
int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f(5,6); }
EOF

run_case "closure_multi_capture_two_arg_forward_into_function_param" "" "18" <<'EOF'
int apply(int f(int,int), int a, int b){ return f(a,b); }
int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return apply(f,5,6); }
EOF

run_case "closure_void_direct_local_call" "7" <<'EOF'
int main(){ int x=7; void f()=closure [x] void () { putint(x); return; }; f(); return 0; }
EOF

run_case "closure_int_expression_statement_prefix" "3" "7" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { putint(x); return x + y; }; return f(4); }
EOF

run_case "closure_int_local_decl_prefix" "" "7" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = x + y; return z; }; return f(4); }
EOF

run_case "closure_int_parameter_assignment_prefix" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { y = y + 1; return x + y; }; return f(4); }
EOF

run_case "closure_int_local_assignment_prefix" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z = z + 1; return x + z; }; return f(4); }
EOF

run_case "closure_int_uninitialized_local_prefix" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int z; z = y + 1; return x + z; }; return f(4); }
EOF

run_case "closure_int_multi_declarator_local_prefix" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int a, b; a = y; b = a + 1; return x + b; }; return f(4); }
EOF

run_case "closure_int_parameter_compound_assignment" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { y += 1; return x + y; }; return f(4); }
EOF

run_case "closure_int_local_compound_assignment" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z += 1; return x + z; }; return f(4); }
EOF

run_case "closure_int_parameter_prefix_increment" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { ++y; return x + y; }; return f(4); }
EOF

run_case "closure_int_local_postfix_increment" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z++; return x + z; }; return f(4); }
EOF

run_case "closure_int_bitwise_expression" "" "3" <<'EOF'
int main(){ int x=6; int f(int)=closure [x] int (int y) { return (x & y) ^ 1; }; return f(3); }
EOF

run_case "closure_int_logical_expression" "" "1" <<'EOF'
int main(){ int x=0; int f(int)=closure [x] int (int y) { return (x || y) && 1; }; return f(4); }
EOF

run_case "closure_int_shift_expression" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return (x << 1) + (y >> 1); }; return f(4); }
EOF

run_case "closure_int_bitwise_compound_assignment" "" "7" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { y &= 7; return x + y; }; return f(12); }
EOF

run_case "closure_int_bitwise_not_expression" "" "0" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return ~x + y; }; return f(4); }
EOF

run_case "closure_int_return_assignment_expression" "" "5" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return (y = y + 1); }; return f(4); }
EOF

run_case "closure_int_return_prefix_increment_expression" "" "5" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return ++y; }; return f(4); }
EOF

run_case "closure_int_return_comma_expression" "" "8" <<'EOF'
int main(){ int x=3; int f(int)=closure [x] int (int y) { return (y = y + 1, x + y); }; return f(4); }
EOF

run_case "closure_alias_chain_direct_call" "" "7" <<'EOF'
int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; return g(4); }
EOF

run_case "closure_alias_chain_two_hops" "" "7" <<'EOF'
int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; int h(int)=g; return h(4); }
EOF

run_case "closure_zero_arg_alias_chain" "" "3" <<'EOF'
int main(){ int x=3; int f()=closure [x] int () { return x; }; int g()=f; return g(); }
EOF

run_case "closure_void_alias_chain" "7" <<'EOF'
int main(){ int x=7; void f()=closure [x] void () { putint(x); return; }; void g()=f; g(); return 0; }
EOF

run_case "closure_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply(f, 4); }
EOF

run_case "closure_zero_arg_forward_into_function_param" "" "3" <<'EOF'
int apply0(int f()){ return f(); }
int main(){ int y=3; int f()=closure [y] int () { return y; }; return apply0(f); }
EOF

run_case "closure_void_forward_into_function_param" "7" <<'EOF'
void apply0(void f()){ f(); }
int main(){ int y=7; void f()=closure [y] void () { putint(y); return; }; apply0(f); return 0; }
EOF

run_case "closure_alias_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; return apply(g, 4); }
EOF

run_case "closure_two_hop_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; int h(int)=g; return apply(h, 4); }
EOF

run_case "closure_zero_alias_forward_into_function_param" "" "3" <<'EOF'
int apply0(int f()){ return f(); }
int main(){ int y=3; int f()=closure [y] int () { return y; }; int g()=f; return apply0(g); }
EOF

run_case "closure_void_alias_forward_into_function_param" "7" <<'EOF'
void apply0(void f()){ f(); }
int main(){ int y=7; void f()=closure [y] void () { putint(y); return; }; void g()=f; apply0(g); return 0; }
EOF

run_case "direct_closure_literal_argument_to_function_value_parameter" "" "7" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int main(){ int y=3; return apply(closure [y] int (int z) { return y + z; }, 4); }
EOF

run_case "direct_zero_arg_closure_literal_argument_to_function_value_parameter" "" "3" <<'EOF'
int apply0(int f()){ return f(); }
int main(){ int y=3; return apply0(closure [y] int () { return y; }); }
EOF

run_case "direct_void_closure_literal_argument_to_function_value_parameter" "7" <<'EOF'
void apply0(void f()){ f(); }
int main(){ int y=7; apply0(closure [y] void () { putint(y); return; }); return 0; }
EOF

run_case "returned_closure_local_init_direct_call" "" "7" <<'EOF'
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ int f(int)=make(3); return f(4); }
EOF

run_case "returned_closure_immediate_call" "" "7" <<'EOF'
int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }
int main(){ return make(3)(4); }
EOF

run_case "returned_local_closure_bind_and_call" "" "7" <<'EOF'
int pick(int x)(int) { int f(int)=closure [x] int (int y) { return x + y; }; return f; }
int main(){ int h(int)=pick(3); return h(4); }
EOF

run_case "returned_multi_capture_local_closure_bind_and_call" "" "18" <<'EOF'
int pick(int x,int y)(int,int) { int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f; }
int main(){ int h(int,int)=pick(3,4); return h(5,6); }
EOF

run_case "returned_local_closure_immediate_call" "" "7" <<'EOF'
int pick(int x)(int) { int f(int)=closure [x] int (int y) { return x + y; }; return f; }
int main(){ return pick(3)(4); }
EOF

run_case "dynamic_returned_local_closure_bind_and_call" "" "255" <<'EOF'
int pick(int x, int c)(int){ int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }
int main(){ int h(int)=pick(3, 1); return h(4); }
EOF

run_case "dynamic_returned_local_closure_immediate_call" "" "255" <<'EOF'
int pick(int x, int c)(int){ int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }
int main(){ return pick(3, 1)(4); }
EOF

run_case "dynamic_returned_local_closure_forward_into_function_param" "" "2" <<'EOF'
int apply(int f(int), int x){ return f(x); }
int pick(int x, int c)(int){ int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }
int main(){ return apply(pick(5, 1), 3); }
EOF

run_case "dynamic_returned_multi_capture_local_closure_forward_into_function_param" "" "7" <<'EOF'
int apply(int f(int,int), int a, int b){ return f(a,b); }
int pick(int x, int y, int c)(int,int){ int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; if(c) f=g; return f; }
int main(){ return apply(pick(5, 7, 1), 3, 2); }
EOF

run_case "noncapturing_function_valued_return_bind_and_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int pick()(int) { return add1; }
int main(){ int f(int)=pick(); return f(41); }
EOF

run_case "noncapturing_function_valued_return_immediate_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int pick()(int) { return add1; }
int main(){ return pick()(41); }
EOF

run_case "local_noncapturing_function_value_return_immediate_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int pick()(int) { int f(int)=add1; return f; }
int main(){ return pick()(41); }
EOF

run_case "rebound_local_noncapturing_function_value_return_immediate_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick()(int) { int f(int)=add1; f=add2; return f; }
int main(){ return pick()(40); }
EOF

run_case "dynamic_noncapturing_function_value_return_immediate_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }
int main(){ return pick(1)(40); }
EOF

run_case "dynamic_noncapturing_function_value_return_bind_and_call" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }
int main(){ int g(int)=pick(1); return g(40); }
EOF

run_case "wrapped_function_value_local_initializer" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int main(){ int f(int)=add1; int h(int)=(0, f); return h(41); }
EOF

run_case "wrapped_function_value_return" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick()(int){ int f(int)=add1; int h(int)=add1; return (h=f); }
int main(){ return pick()(41); }
EOF

run_case "dynamic_wrapped_function_value_return" "" "42" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; return (h=f); }
int main(){ return pick(1)(40); }
EOF

run_case "ternary_noncapturing_function_value_return_immediate_call" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }
int main(){ return pick(1)(40); }
EOF

run_case "ternary_noncapturing_function_value_return_bind_and_call" "0" "41" <<'EOF'
int add1(int x){ return x+1; }
int add2(int x){ return x+2; }
int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }
int main(){ int h(int)=pick(1); return h(40); }
EOF

run_case "ternary_closure_function_value_return_immediate_call" "0" "7" <<'EOF'
int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }
int main(){ return pick(1)(4); }
EOF

run_case "ternary_closure_function_value_return_bind_and_call" "0" "7" <<'EOF'
int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }
int main(){ int h(int)=pick(1); return h(4); }
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

run_case "nested_pair_field_lookup" "12" "3" <<'EOF'
struct Outer { pair p; int z; };
int main(){ struct Outer o={{1,2},3}; putint(o.p.first); putint(o.p.second); return o.z; }
EOF

run_case "nested_struct_field_lookup" "12" "3" <<'EOF'
struct Point { int x; int y; };
struct Outer { struct Point p; int z; };
int main(){ struct Outer o={{1,2},3}; putint(o.p.x); putint(o.p.y); return o.z; }
EOF

run_case "nested_pair_copy_init" "1" "2" <<'EOF'
struct Outer { pair p; int z; };
int main(){ struct Outer o={{1,2},3}; pair q=o.p; putint(q.first); return q.second; }
EOF

run_case "nested_struct_copy_init" "1" "2" <<'EOF'
struct Point { int x; int y; };
struct Outer { struct Point p; int z; };
int main(){ struct Outer o={{1,2},3}; struct Point q=o.p; putint(q.x); return q.y; }
EOF

run_case "deep_nested_struct_field_lookup" "12" "7" <<'EOF'
struct Point { int x; int y; };
struct Mid { struct Point p; int z; };
struct Outer { struct Mid m; int w; };
int main(){ struct Outer o={{{1,2},3},4}; putint(o.m.p.x); putint(o.m.p.y); return o.m.z + o.w; }
EOF

run_case "deep_nested_pair_copy_init" "1" "5" <<'EOF'
struct Mid { pair p; int z; };
struct Outer { struct Mid m; int w; };
int main(){ struct Outer o={{{1,2},3},4}; struct Mid m=o.m; putint(m.p.first); return m.p.second + m.z; }
EOF

run_case "deep_nested_struct_assignment_from_member" "1" "5" <<'EOF'
struct Point { int x; int y; };
struct Mid { struct Point p; int z; };
struct Outer { struct Mid m; int w; };
int main(){ struct Mid m={{7,8},9}; struct Outer o={{{1,2},3},4}; m=o.m; putint(m.p.x); return m.p.y + m.z; }
EOF

run_case "deep_nested_pair_assignment_from_member" "1" "5" <<'EOF'
struct Mid { pair p; int z; };
struct Outer { struct Mid m; int w; };
int main(){ struct Mid m={{7,8},9}; struct Outer o={{{1,2},3},4}; m=o.m; putint(m.p.first); return m.p.second + m.z; }
EOF

echo "[extension-runtime] All extension runtime regressions passed."
