# Runtime RE / Segfault Plan

## Scope

- This file is the compact active-plan authority for the still-open hidden
  runtime `RE` line when the observed failure mode looks like
  **generated-program segmentation fault**.
- Read this after `AGENTS.md`, `docs/ir-conventions.md`, and
  `docs/NEXT_STEPS.md` when this specific line is active.
- Keep this file synchronized with the higher-level progress memory in
  `docs/NEXT_STEPS.md`.

## Current Ground Truth

- User reminder: the hidden `RE` we still care about is the generated program
  crashing on the judge, and the user specifically remembers the current
  failure mode as **segmentation fault**.
- User reminder: recent hidden red points also include several **Lv8 no-array**
  cases (`short_circuit1`, `func_expr2`, `global_var2`, `hanoi2`), so the
  active root cause cannot be treated as “arrays only”.
- Therefore the current mainline is **not** “general timeout hunting” and
  **not** “pure perf witness cleanup”.
- Stable broad surfaces already revalidated on the current tree:
  - `make test` PASS
  - full course `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`)
  - `compiler2021/function_test2021` PASS (`103/103`)
  - `indigo/test_codes/functional_test` PASS (`111/111`)
  - `minic-test-cases-2021f/functional` PASS (`100/100`)
- Known public timeout/artifact witnesses such as `03_sort2`, `shuffle2`,
  `many_parameters10000`, `matrix-1`, and `register_alloc10000` are useful
  context, but they are **not** the same thing as the hidden segfault root
  cause.

## Closed “looks-like-RE” families already handled

- final text peephole address/base reuse invalidation holes in
  `src/compiler/compiler_driver.c`
- dropped `.sbss/.sdata` during image construction in
  `src/machine/runtime/machine_image/machine_image_build.inc`
- generic bytecode branch/imm payload truncation in
  `src/machine/object/machine_bytes/*`
- runtime decode-chain omission for `LOAD_INDIRECT` / `STORE_INDIRECT`
- selected constant-branch truthiness folding using host-width semantics
  instead of normalized 32-bit SysY semantics

These were worth fixing, but the user has now confirmed that the hidden
segfault is still not closed, so the active root cause is elsewhere.

## Highest-probability remaining segfault families

1. **Bad address formation**
   - `addr_local` / `addr_global`
   - repeated base/index reuse across blocks or transformed materializations
   - wrong local/global root equivalence assumptions

2. **Indirect memory misuse**
   - `load_indirect` / `store_indirect`
   - value-equality being mistaken for address-equality
   - wrong alias/non-alias proof across cleanup boundaries

3. **Stack-frame / call-lowering corruption**
   - spill-slot lifetime overlap
   - stack-passed argument area misuse
   - caller-save / callee-save / return-value path corruption
   - stack-address materialization surviving past unsafe transforms

4. **Late control-flow corruption that leads to bad memory access**
   - successor-edge folding
   - compare/branch lowering
   - fallthrough/jump target shaping that is locally plausible but globally
     wrong

5. **Cross-layer handoff mismatch**
   - selected -> bytes
   - bytes -> text export
   - image/load/runtime entry state
   where the shape stays verifier-legal but the runtime memory model diverges

6. **Lv8 side-effect ordering bugs**
   - short-circuit side effects
   - function argument evaluation order
   - global write/read ordering across nested calls
   - recursive call-state corruption that only shows up after side effects

## Immediate ordered read/recheck spine

Read in this order and keep the focus on “could this directly yield a
segfault?” rather than on generic optimization value:

1. `src/ir/ir_lower_stmt.inc`
2. `src/machine/lowering/machine_select/machine_select_cleanup.inc`
3. `src/machine/lowering/machine_select/machine_select_lower_control.inc`
4. `src/machine/object/machine_bytes/machine_bytes.c`
5. `src/compiler/compiler_driver.c`
6. adjacent tests under:
   - `tests/ir/`
   - `tests/machine/lowering/machine_select/`
   - `tests/machine/object/machine_bytes/`
   - `tests/compiler/`

## What to treat as stronger evidence than public timeout shape

- any transform that can convert one address root into another
- any cleanup that rewrites a memory-producing value across a store/call edge
- any stack-slot/base-pointer reuse that survives beyond one obviously safe
  straight-line window
- any branch/jump fold that changes memory-touching reachability

## Active stress-generation tactic

- User reminder now promoted into the active plan: use scripted SysY stress
  generation to try to **force hidden segfault shapes** instead of waiting
  only for public suites to stumble onto them.
- Current generator:
  `tools/generate_sysy_runtime_re_stress.py`
- Purpose:
  generate large SysY programs with:
  - deep nested call chains
  - many-argument calls
  - repeated global/local array reads and writes
  - nested helper calls inside memory-heavy functions
  so the backend sees more combinations of call-lowering plus indirect-memory
  traffic than the stock public suites alone provide.
- Current usage note:
  the script can emit both `.sy` and matching `.out` files, so the generated
  case can be run directly through `tools/sweep_sysy_suite.py`.
  The bridge generator now also correctly wraps parameter references when
  `param_count < 11`, so stronger 8-parameter stress bundles no longer fail in
  the host-oracle build step just because the generator emitted undeclared
  names like `a8`/`a9`/`a10`.

## Current progress snapshot

- compact hidden runtime-RE/segfault plan setup: **complete / 100%**
- remaining segfault-family source audit on the current stable tree:
  **in progress / ~66%**
- next concrete source slice:
  post-loop canonical-IR fact handling is now reclosed for the latest
  `while`/`for` local-fact leak family, and condition-side-effect lowering is
  now reclosed too; `if (cond) stmt;` false-path fact merging is now reclosed
  as well, and expression-level ternary/logical joins are now reclosed too.
  The new downstream `34_multi_loop.sy` witness has now also been closed on
  the Value-SSA allocate+rewrite line. Next concrete slice is the remaining
  generated-program repro hunt plus any fresh reopens in late
  `machine_select` / `machine_bytes` / `compiler_driver` only when a new
  witness points there again

## Latest finding

- 2026-05-10: another concrete hidden-runtime-wrong-code family is now
  closed one stage earlier than the late backend line: canonical-IR
  loop-exit local facts in `src/ir/ir_lower_stmt.inc`.
- Root cause:
  the previous `while` scope-restore fix had correctly stopped one
  zero-iteration leak family, but the more general rule was still wrong:
  loop lowering could either
  `1)` leak loop-body facts to the exit path (`for`), or
  `2)` blindly restore pre-loop facts for locals mutated inside the loop
  (`while`/`for`), which is also unsound when the loop definitely/possibly
  executes.
- Concrete witness:
  `for (; x && getint(); x = x - 1) { f = 1; } if (f) ...`
  used to miscompile the zero-iteration input case (`0`) into `111` instead
  of `222`, because `f = 1` leaked from the loop body to the exit path.
- Live repair:
  loop-exit scope restoration is now mutation-aware. On `while`/`for` exits,
  the lowering path restores the pre-loop scope shape but clears
  constant-value facts for any scalar local mutated in the loop condition,
  body, or step expression instead of pretending either the entry value or
  the one-iteration body value is universally valid after the loop.
- Regression coverage:
  `test_ir_keeps_pre_loop_local_fact_after_unknown_calling_while_body`
  (updated to require an explicit post-loop branch rather than an unsound
  fold) plus
  `test_ir_clears_mutated_local_fact_after_unknown_calling_for_loop`
- Rechecks:
  - `make test-ir-regression` PASS
  - `make test` PASS
  - `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`)
  - `compiler2023 hidden_functional` focused reruns:
    `09_BFS.sy` PASS, `10_DFS.sy` PASS (both at `--case-timeout 60`)
- Why it still matters for hidden runtime RE / segfault:
  this family does not only cause WA. By changing post-loop control-flow and
  dataflow facts, it can redirect later address calculations, recursion
  guards, queue/DFS visitation checks, or global-state-dependent branches
  into bad memory access patterns or pathological runtime behavior.
- 2026-05-10: one more concrete canonical-IR wrong-code family is now closed
  in the same `src/ir/ir_lower_stmt.inc` area: **condition side effects were
  being dropped whenever truthiness folding decided the branch direction
  early**.
- Root cause:
  `if` / `while` / `for` lowering would use known truthiness from the local
  fact engine and directly choose the branch/loop shape without lowering the
  condition expression at all. That is only sound for side-effect-free
  conditions. Expressions like `if (i = 1)`, `while (i = 0)`, or nested
  comma/call/update conditions still need their stores/calls emitted even if
  the final truthiness is already known.
- Concrete witness:
  `int i = 0; if (i = 1) {} putint(i);` used to print `0` instead of `1`
  because the condition assignment vanished entirely during lowering.
- Live repair:
  condition-truthiness pruning is now gated by a recursive side-effect check.
  When the condition contains assignment/update/call side effects (directly or
  nested), lowering keeps the full condition-evaluation path instead of using
  the early known-truthiness shortcut.
- Regression coverage:
  `test_ir_lowers_if_condition_side_effect_even_when_truthiness_known`
  plus the loop siblings
  `test_ir_lowers_while_condition_side_effect_even_when_truthiness_known` and
  `test_ir_lowers_for_condition_side_effect_even_when_truthiness_known`
- Rechecks:
  - `make test-ir-regression` PASS
  - `make test` PASS
  - `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
  - local runtime minis:
    `if_side_effect.sy`, `while_side_effect.sy`, `for_side_effect.sy` PASS
  - rotated external hidden-like batch:
    `compiler2023 hidden_functional` cases
    `21_union_find`, `22_matrix_multiply`, `23_json`, `24_array_only`,
    `25_scope3`, `26_scope4`, `27_scope5`, `28_side_effect2`,
    `30_many_dimensions`, `31_many_indirections` all PASS
- Why it still matters for hidden runtime RE / segfault:
  dropping a condition-side-effect store/call can corrupt loop guards,
  recursion guards, visited-state writes, or later array indices/pointers.
  That can manifest as segfault/RE just as plausibly as plain WA.
- 2026-05-10: one more adjacent canonical-IR wrong-code family is now closed
  in `src/ir/ir_lower_stmt.inc`: **no-else `if` joins were keeping only the
  then-path facts and forgetting the false path entirely**.
- Root cause:
  on `if (cond) stmt;` with unknown condition, when the then-branch
  fallthrough reached the join block, lowering restored the post-`if` scope
  from `then_scopes` alone instead of merging `then_scopes` with the false
  path. That made later truthiness pruning behave as if `stmt` had executed
  unconditionally.
- Concrete witness:
  `int i = 1; if (a) i = 2; if (i == 1) ...` used to fold the second `if`
  to false and return/print `0` even when `a == 0`.
- Side-effectful sibling witness:
  `if ((i = 1) && 0) {} if (i) ...` used to keep the stale false-path fact
  and fold the second `if` to false even though the condition-side assignment
  had already executed.
- Live repair:
  the no-else continue scope now merges the then-path facts with the false
  path instead of taking the then-path snapshot alone, and the condition-side
  side-effect clearing logic is reused so side-effectful conditions do not
  reintroduce stale pre-condition facts during that merge.
- Regression coverage:
  `test_ir_merges_no_else_branch_facts_with_false_path`
  and
  `test_ir_merges_no_else_condition_side_effect_fact_to_continue`
- Rechecks:
  - `make test-ir-regression` PASS
  - `make test` PASS
  - local runtime minis:
    `if_no_else_merge.sy`, `if_cond_scope_follow.sy`,
    `if_side_effect.sy`, `while_side_effect.sy`, `for_side_effect.sy` PASS
  - `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`)
- Why it still matters for hidden runtime RE / segfault:
  once a join inherits “then definitely executed” facts incorrectly, later
  loop/visited/index guards can collapse to the wrong side, which can
  cascade into bad traversal state, invalid indices, or other runtime memory
  misuse rather than only a simple WA.
- 2026-05-10: one more adjacent canonical-IR wrong-code family is now closed,
  this time on the **expression-level** branch join line in
  `src/ir/ir_lower_expr.inc`.
- Root cause:
  ternary value lowering (`cond ? then : else`) and logical-value lowering
  (`&&` / `||` used as expressions) were leaving one branch's mutated-local
  facts alive after the join. That let later conditions fold as if one
  expression branch had executed unconditionally.
- Concrete witnesses:
  - `x = a ? (i = 1) : (i = 2); if (i == 1) ...`
  - `x = a && (i = 1); if (i == 1) ...`
  The first one reproduced directly as a runtime mismatch in a minimized
  local witness (`ternary_scope_follow.sy`).
- Live repair:
  after logical-value and ternary-expression joins, lowering now clears
  mutated-local constant facts conservatively instead of letting one branch's
  fact snapshot dominate the join.
- Regression coverage:
  `test_ir_clears_ternary_branch_mutation_facts_after_join`
  and
  `test_ir_clears_logical_value_branch_mutation_facts_after_join`
- Rechecks:
  - `make test-ir-regression` PASS
  - `make test` PASS
  - `autotest -riscv -s lv9 /workspaces/compiler_lab` rerun started from the
    same kept tree while the focused local/external repro work advanced, with
    the targeted local minis already green
  - local expression-join minis:
    `ternary_scope_follow.sy`, `logical_unknown_scope_follow.sy`,
    `comma_scope_follow.sy` PASS (`3/3`)
- Fresh external bug lead still open after this closure:
- 2026-05-10: the downstream `compiler2023 hidden_functional/34_multi_loop.sy`
  witness is now closed on the **Value-SSA allocate+rewrite** line.
- Root cause:
  after stage-by-stage localization ruled out canonical IR / lower IR /
  value-SSA mainline shape, the first real divergence came from multi-round
  spill rewriting. `value_ssa_alloc_rewrite_block_local_spill_split_for_value`
  was still willing to split a value family that had already been rewritten
  through spill-named locals in an earlier round. In a deep nested-loop
  witness, that produced a fresh split child inside the already spilled
  family, and the next rewrite round then mixed an older spill-local family
  with a newer allocation result.
- Concrete bad shape:
  the rewritten Value-SSA dump could end up with both an old outer spill
  family like `store_local spill.0.16, ssa.14` and a later inner family
  partially redirected through the same historical spill lineage, which then
  surfaced in final code as corrupted nested loop counters and the runtime
  mismatch `expected 42`, actual exit `162`.
- Live repair:
  block-local spill splitting now skips values that are already materially
  represented by spill-named locals from an earlier rewrite round (either as a
  spill-local load definition or a spill-local store use). That prevents the
  allocator rewrite loop from re-splitting an already spilled family and keeps
  spill families distinct across rewrite rounds.
- Regression coverage:
  `test_value_ssa_allocate_and_rewrite_program_keeps_distinct_nested_spill_families`
- Rechecks:
  - `make test-value-ssa-alloc` PASS
  - `make test` PASS
  - `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`)
  - focused witness rerun:
    `34_multi_loop.sy` PASS
  - rotated external hidden-like batch:
    `21_union_find`, `22_matrix_multiply`, `23_json`, `24_array_only`,
    `28_side_effect2`, `30_many_dimensions`, `31_many_indirections`,
    `32_many_params3`, `34_multi_loop` all PASS (`9/9`)

- 2026-05-10: after closing `34_multi_loop.sy`, the next fresh supported-
  subset search rounds stayed green rather than immediately exposing another
  runtime witness. Fresh external batches now additionally include:
  - `compiler2025 functional` heavy subset:
    `62_percolation`, `63_big_int_mul`, `64_calculator`, `65_color`,
    `68_brainfk`, `69_expr_eval`, `70_dijkstra`, `71_full_conn`,
    `75_max_flow`, `76_n_queens` -> all PASS (`10/10`)
  - `compiler2026 functional` supported-subset upper tail:
    `77_substr`..`94_nested_loops` (excluding float cases) -> all PASS
    (`18/18`)
  - `compiler2021 h_functional` upper tail:
    `110_many_params3`..`117_nested_loops` -> all PASS (`8/8`)
  - stronger generated stress bundle (`stress_a/b/c`) -> all PASS (`3/3`)
  Current authority therefore is that the external/runtime-heavy search line
  has widened materially again, but no new supported-subset runtime mismatch
  is presently reproduced after the `34_multi_loop.sy` closure.

- 2026-05-10: one concrete **segfault-plausible wrong-code** bug is now
  closed in `machine_select_cleanup.inc`.
- Root cause:
  same-block indirect-load forwarding was proving a repeated load safe across
  `store_local local.N` too aggressively when the loaded address came from
  `addr_local local.0 + const`. The address-root helper remembered only the
  base local slot and discarded the offset, so a load from
  `addr_local local.0 + 4` could be treated as disjoint from
  `store_local local.1, ...` even though those two names can denote the same
  contiguous local-slot family element.
- Why it matters for hidden runtime RE:
  this can preserve a stale value after an in-block store through a sibling
  local slot, which is exactly the kind of wrong indirect-memory result that
  can later become a bad pointer / bad index / segfault instead of only WA.
- Live repair:
  local-slot alias proofs now distinguish exact slot addresses from
  offset-derived addresses and refuse the “different local id => non-alias”
  shortcut once an address has been offset from its base root.
- Regression coverage:
  `test_machine_select_does_not_forward_indirect_load_across_aliasing_sibling_local_store`
- Rechecks:
  - `make test-machine-select` PASS
  - `make test` PASS
  - `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
  - rotated third-party slice:
    `compiler2021/function_test2021 --filter array` PASS (`3/3`)
- 2026-05-10: the scripted stress-generation tactic is now live too. A new
  generator, `tools/generate_sysy_runtime_re_stress.py`, can emit nested-call
  / nested-read-write SysY stress cases plus matching oracle files. Focused
  smoke runs are green so far:
  - one ~1.5k-line generated case PASS via `tools/sweep_sysy_suite.py`
  - one ~10k-line generated case PASS via `tools/sweep_sysy_suite.py`
  - a newer more varied branch-heavy generator shape now also passes on a
    two-case local stress bundle
  - an even more call/branch-heavy oversized variant also re-exposed the
    existing `MACHINE-BYTES-344: riscv preview branch target out of range`
    backend boundary during compile, which is not the hidden segfault itself
    but is useful as a reminder that “make it more varied” can hit older
    compile-stage target-range limits before it reaches runtime
  - the generator now also includes intentionally foldable / overwritable
    noise plus stronger alias-style relations (`alias_kernel_*`, repeated
    overwritten locals, constant-false branches, same-array multi-role calls),
    and one fresh ~2.5k-line “more relations” sample passes cleanly
  This does **not** mean the hidden segfault is gone; it means the repository
  now has a reusable way to probe bigger call/memory combinations while the
  remaining segfault-family audit continues.

## Current side-effect-order audit note

- 2026-05-10: I rechecked the obvious front-end lowering surfaces first:
  `ir_lower_expr.inc` still lowers call arguments left-to-right, and logical
  `&&` / `||` branch lowering still descends left operand before right operand,
  so the **first obvious AST->IR side-effect ordering contract still looks
  aligned**. That does not clear the bug family; it narrows suspicion toward
  later layers such as call/result handling, caller-saved state, late
  call-argument peepholes, or global-state-sensitive cleanup instead of the
  first direct short-circuit lowering skeleton itself.
- 2026-05-10: I then tried the first focused **Lv8 side-effect repro bundle**
  instead of only reading code. The simplest deterministic short-circuit /
  global-side-effect cases and a pair of deterministic nested-single-argument /
  recursive side-effect cases are currently green under
  `tools/sweep_sysy_suite.py`, so the most obvious “front-end short-circuit is
  simply reversed / later call lowering completely ignores side effects”
  hypothesis has not reproduced yet. Current working interpretation:
  if the hidden segfault really involves side effects, it is likely a more
  selective later-layer interaction (for example late call-argument rewriting,
  caller-save misuse, global-state-sensitive cleanup, or a deeper nested shape)
  rather than the first trivial short-circuit skeleton.
- 2026-05-10: I also pulled the extra third-party `compiler2023` suite the
  user suggested and checked the most relevant Lv8-style surfaces first.
  Current targeted results are still green:
  `50_short_circuit.sy`, `51_short_circuit3.sy`, `72_hanoi.sy`,
  `89_many_globals.sy`, `93_nested_calls.sy`, plus hidden-like
  `28_side_effect2.sy` and `32_many_params3.sy` all PASS locally. A small
  `hidden_functional` prefix sweep (`12` cases) only reopened `09_BFS.sy` and
  `10_DFS.sy` as runtime timeouts, not a new segfault-style Lv8 repro. Current
  interpretation: this extra suite usefully widens confidence that the most
  obvious Lv8 side-effect/call-order shapes are still green on the current
  tree, but it has not yet exposed the hidden runtime-RE root cause.
- 2026-05-10: Those `compiler2023 hidden_functional` BFS/DFS timeouts now look
  much more suspicious than generic “heavy case” noise. `09_BFS.sy` and
  `10_DFS.sy` both use only `n=1000`, `m=5000`, and about `4.5k` query lines;
  when translated to a host C++ executable with trivial runtime wrappers, both
  finish in roughly `0.009s`, yet the current compiled RISC-V outputs still
  hit `RUN_TIMEOUT` even at `--case-timeout 60`. Current authority therefore
  treats these two as **bug leads first**, not “probably just too slow”:
  something in the generated code is likely changing control flow or state
  enough to explode the runtime cost beyond what the source algorithm alone
  justifies.
