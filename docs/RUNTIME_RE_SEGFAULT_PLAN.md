# Runtime RE / Segfault Plan

## Scope

- This file is the compact active-plan authority for the still-open hidden
  runtime `RE` line when the observed failure mode looks like
  **generated-program segmentation fault**.
- Read this after `AGENTS.md`, `docs/ENGINEERING_MEMORY.md`, and
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

- allocator split-family spill-slot aliasing in
  `src/value_ssa_alloc/value_ssa_alloc_spill.inc`
  (parent-slot reuse now rechecks interference before inheriting the slot,
  which reclosed the public/hidden-adjacent `register_alloc` WA regression)
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

### Current generated-witness memory

- The first post-fix rotated batch under `/tmp/runtime_re_spill_followup/`
  initially looked promising because `stress_spill_101.sy` came back WA, but
  that witness turned out to be **generator-invalid**, not a real compiler
  lead:
  - the generated program passed `len=array_size=24` into helper calls whose
    first array argument was a global `g9[18]`, so later `a[idx0]` accesses in
    `arr_kernel_2` could legally reach `g9[18..23]` and go out of bounds
  - host/target divergence therefore came from undefined behavior in the
    synthetic testcase rather than from a supported-subset compiler bug
- The generator now guards against that shape directly:
  `tools/generate_sysy_runtime_re_stress.py` rejects
  `global_size < array_size`.
- Revalidated valid follow-up batches are green again:
  - `/tmp/runtime_re_spill_followup_valid/` -> `3/3 PASS`
  - `/tmp/runtime_re_spill_followup_valid2/` -> `6/6 PASS`
- Current authority is therefore: the apparent new stress witness is
  **disqualified**, and this generated nested-call/global-array stress family
  is back in the “adjacent family expanded and currently green” bucket.
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
  **in progress / ~83%**
- simple-backend diagnostic shrink:
  **in progress / ~89%**
  - the dedicated all-spill simple backend remains available as the main
    diagnostic comparison route, but it is no longer the live default
    submission path; current local authority is that the rebuilt default
    `build/compiler -riscv` route is back on the old allocate+rewrite
    mainline, and `COMPILER_SIMPLE_BACKEND=1` is now the explicit knob for
    entering the third-route simple backend
  - the conservative env/profile isolation for this branch now lives in
    `compiler_driver.c`, leaving the simple-backend helper itself as a
    thinner all-spill + emit wrapper
  - the direct simple-text emitter path is now behind
    `COMPILER_USE_DIRECT_SIMPLE_TEXT=1` so the default simple path prefers
    the more mature bytes/text export chain while we debug the new `AE`
  - the default simple-backend profile now also sets
    `MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS=1`, removing one narrower
    selected-side address-root reuse cleanup from the current diagnostic
    submission route
  - the default simple-backend profile now also sets
    `MACHINE_SELECT_SKIP_CLEANUP_PURE=1`, removing one broader
    selected-side pure cleanup layer from the current diagnostic submission
    route as the next hidden-RE shrink step
  - the default simple-backend profile now also skips the two pure-call
    reuse passes in `machine_select`
    (`MACHINE_SELECT_SKIP_REUSE_INTERNAL_PURE_CALLS=1` and
    `MACHINE_SELECT_SKIP_REUSE_UNIQUE_PREDECESSOR_PURE_CALLS=1`)
  - the default simple-backend profile now also skips the two remaining
    indirect-memory cleanup passes in `machine_select`
    (`MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS=1` and
    `MACHINE_SELECT_SKIP_REUSE_SPILL_PURE_EXPR=1`)
  - the default simple-backend profile now also skips the final full
    `machine_select` cleanup via `MACHINE_SELECT_SKIP_FULL_CLEANUP=1`
  - the direct simple-text-first variant was backed out again after it
    widened the hidden red surface; the default route stays on the shared
    bytes/text export chain
  - a later attempt to copy the direct simple-text `_start` startup stub
    into the shared bytes/text export tail was **reverted** again after
    hidden-course feedback widened from the current `1 RE` baseline to
    `3 RE`; current authority is therefore that this default route must not
    currently inject its own `_start`/`la gp, __global_pointer$` sequence
  - one follow-up experiment then temporarily switched the default route back
    to the old allocate+rewrite mainline without its custom `_start` stub so
    the startup path could be isolated; that line is now the kept live
    default again, with local probe confirmation that default output omits
    `_start` while the explicit simple-backend route still keeps its scoped
    startup-stub behavior
  - a later route-audit also found that the simple-backend conservative
    profile had been applied too late: until this fix, the third route still
    built `value_ssa` with the ordinary default bridge/hotspot settings
    before the simple-backend env toggles were installed, so earlier
    route-difference conclusions were partially polluted
  - the shared bytes/text export tail now also has one concrete peephole
    bug closed: `repeated_indexed_addr_sequences` had been hardcoding `sp`
    as the base during a fold that could match non-`sp` carriers, and the
    fold is now restricted back to genuine stack-based sequences
  - that same late text-export area now also has a second safety tightening:
    address-reuse scans no longer treat post-`ret` / post-`j` / post-direct-
    branch text as if it were still one continuous safe straight-line window
  - focused public rechecks after this shrink stayed green:
    default `lv8` `12/12`, simple-backend `lv8` `12/12`, simple-backend
    `lv9` `22/22`
  - one rotated external `indigo/test_codes/functional_test` slice also
    stayed green (`25/25`)
  - `make test-machine-ir` still hits the already-known `spill.3/spill.4`
    dump-shape expectation noise, so it remains a non-clean checkpoint
    surface here rather than evidence of a new regression from the shrink
- next concrete source slice:
  post-loop canonical-IR fact handling is now reclosed for the latest
  `while`/`for` local-fact leak family, and condition-side-effect lowering is
  now reclosed too; `if (cond) stmt;` false-path fact merging is now reclosed
  as well, and expression-level ternary/logical joins are now reclosed too.
  The downstream `34_multi_loop.sy` witness and the later many-args
  allocate+rewrite result-sync family are now both reclosed on the
  Value-SSA allocate+rewrite line. Next concrete slice is the remaining
  generated-program repro hunt **after** the many-args batch closure plus any
  fresh reopens in late `machine_select` / `machine_bytes` /
  `compiler_driver` only when a new witness points there again

## Latest finding

- 2026-05-10: the next active repro-search round finally produced a **new concrete supported-subset runtime wrong-code witness family** instead of another all-green widening batch. Three fresh temporary pools were used:
  - `/tmp/lv8_hidden_like_batch` (`80/80 PASS`): no-array Lv8-style
    `short-circuit + global mutation + nested calls + recursion`
  - `/tmp/array_hidden_like_batch` (`50/50 PASS`): array/global/call recursion mixes
  - `/tmp/many_args_hidden_like_batch` (`0/60 PASS`): **all red**, a new
    wide-arity nested-call family
- The failing family is now reduced further under `/tmp/many_args_reduce3/`:
  - `caseD_callee_side_only.sy` FAIL
  - `caseE_callee_side_plus_caller_globals.sy` FAIL
  - `caseG_original_no_branch_in_caller.sy` FAIL
  - `caseF_callee_side_plus_caller_params.sy` PASS
- Current minimized shape:
  a caller with `12` scalar parameters forwards them into another `12`-arg
  callee in reversed order; the callee is fine when called directly from
  `main`, but miscompiles when its body both
  `1)` performs calls/branches and
  `2)` later recombines a pre-call accumulator with reloaded globals.
- Stage localization so far:
  canonical IR, lower IR, Value-SSA, and `valueopt` dumps for
  `/tmp/many_args_reduce3/caseD_callee_side_only.sy` still show the correct
  `t + g2 + g3` structure, but the first machine allocate/rewrite dump is
  already wrong in `wide0 bb.1`:
  it becomes `load global.2; add spill.3, reg; load global.3; add reg, reg`,
  i.e. effectively `g3 + g3` instead of preserving `t + g2 + g3`.
- Current working conclusion:
  this is no longer only a front-end/IR-side hypothesis. There is now a real
  post-`valueopt` / machine-stage wrong-code lead in a **wide-arity nested
  call + side-effectful callee** family, and that family is plausibly related
  to hidden runtime `RE` because in larger programs the same corruption can
  hit address/index/call-state values instead of only printing the wrong
  integer.

- 2026-05-10: that many-args family is now partially closed with **two kept fixes** on the allocate+rewrite line rather than on the front-end or final text line.
- First fix:
  when post-rewrite canonicalization runs after one function introduced spill
  locals, later allocation/layout rebuilding must treat **all body functions**
  as rewritten for the next allocation round. Otherwise untouched sibling
  functions can keep pre-canonicalization placement ids against a
  post-canonicalization Value-SSA program. The minimized witness was
  `/tmp/many_args_reduce3/caseD_callee_side_only.sy`, where `wide0` needed
  `t + g2 + g3` but stale placements lowered it like `g3 + g3`.
- Second fix:
  once a function already owns concrete spill locals from an earlier rewrite
  round, the later rewriteable-spill stage must not materialize **another**
  spill-rewrite round into that same function blindly. The minimized witness
  was `/tmp/many_args_reduce3/caseG_original_no_branch_in_caller.sy`: after a
  correct first rewrite round, a second spill rewrite started reusing the
  existing `spill.0`/`spill.1` locals for newly spilled values and corrupted a
  12-arg nested call into duplicated arguments. The live rule is now more
  conservative: later rounds still reallocate/recolor such functions, but the
  rewriteable-spill materialization step skips functions that already have
  earlier spill locals.
- Focused rechecks after these closures:
  - `make test-value-ssa-alloc` PASS
  - `make test-value-ssa-machine` PASS
  - `make test-machine-ir` PASS
  - `autotest -riscv -s lv8` PASS (`12/12`)
  - `autotest -riscv -s lv9` PASS (`22/22`)
  - `/tmp/many_args_reduce3` runtime witnesses now reclose on the `.sy` cases
    (`caseD`, `caseE`, `caseF`, `caseG`, `caseH`, `caseI`, `caseJ` all PASS;
    the `.c` host-helper files in that temp dir are irrelevant harness noise).
- Current status of the broader generated many-args search:
  the broad `/tmp/many_args_hidden_like_batch` line no longer represents an
  immediately minimized runtime wrong-code lead; its latest rerun is still a
  long-running / compile-pressure-heavy sweep rather than a closed result, so
  keep treating it as an adjacent widening probe instead of proof that the
  whole many-args family is exhausted.

- 2026-05-10: one more adjacent allocator/rewrite pressure family is now narrowed too, and this time the first visible symptom was **compile timeout** rather than direct wrong-code. After the earlier many-args runtime fixes landed, the widened generated batch `/tmp/many_args_hidden_like_batch` started failing first at `many_args_hidden_like_3001.sy -> COMPILE_TIMEOUT`. Trace-guided profiling showed the sink was a split-only rewrite loop on the recursive `rec` function: coalesce analysis kept getting rerun on a slightly larger split-expanded SSA body even though that iteration had not performed any spill-materialization rewrite. The live rule is now more conservative there: once a rewrite-loop iteration rewrote only via block-local spill splitting and then completed one successful reallocation, the allocator accepts that reallocated result as converged instead of insisting on another split-only round. Focused checks after this closure are green (`make test-value-ssa-alloc`, `make test-value-ssa-machine`, `make test-machine-ir`), and `many_args_hidden_like_3001.sy` now moves from `COMPILE_TIMEOUT` to `PASS`. The next active similar witnesses in that generated family are now `many_args_hidden_like_3002.sy` and `many_args_hidden_like_3003.sy`, both runtime mismatches rather than compile-time stalls.

- 2026-05-10: the **actual** remaining `3002/3003` bug turned out to be one step later than the earlier rewrite-stage guess. Fresh reduction first narrowed it to `/tmp/ma3002_reduce/P_postpred_bool.sy`: the fully rewritten Value-SSA program was already correct again (`wide1` still stored `a11..a8` into `spill.0..3` before the `wide0(...)` call), but the final machine-IR path still lowered the call incorrectly because the **final allocation result had drifted away from the rewritten spill shape**. Concretely, the rewritten program still explicitly spilled `ssa.0..ssa.3`, while the post-rewrite reallocation wanted to spill `ssa.4..ssa.7`; machine lowering then paired the rewritten program with that mismatched newer allocation and rebuilt the call with the wrong argument carriers. The kept repair is now on the allocate+rewrite loop handoff itself: after a rewrite round has already materialized spill locals and the current function still has the same value-count shape, a later reallocation is no longer allowed to replace that function's allocation result with a different spill family. Instead the loop preserves the earlier spill-compatible allocation result for that function, so the rewritten program and final allocation stay in sync. Focused rechecks after this closure are green:
  - reduced witnesses:
    `P_postpred_bool.sy`, `K_postcall_side_only.sy`, `N_postcall_pred_only.sy`
    -> all PASS
  - generated seeds:
    `many_args_hidden_like_3001.sy`, `3002.sy`, `3003.sy` -> PASS
  - widened generated family:
    `/tmp/many_args_hidden_like_batch` -> **`60/60 PASS`**
  - regression surfaces:
    `make test-value-ssa-alloc` PASS,
    `make test-value-ssa-machine` PASS,
    `make test-machine-ir` PASS,
    `autotest -riscv -s lv8` PASS (`12/12`),
    `autotest -riscv -s lv9` PASS (`22/22`)
- Current authority after this closure:
  the earlier many-args / allocator / late-text lines stay closed, but the
  still-open hidden runtime-RE hunt has now produced one fresh concrete
  **non-array / global-side-effect** public synthetic witness again on the
  current conservative tree, so the active line has moved back up to
  `machine_ir` canonicalization rather than staying only on final export.

- 2026-05-10: a new conservative-path public synthetic witness,
  `/tmp/noarray_hidden2/case_001.c`, exposed another concrete wrong-code
  family that is plausibly adjacent to the hidden segfault line:
  `global mutation + branch-gated later return reload`.
  - Symptom:
    host expected `124`, generated program returned `130`; an instrumented
    variant showed the first divergence already inside `f3(...)`, where the
    `if (g1 > 2 && g0 != 3) g0 = g0 + 3; return a1 + g0;` tail skipped the
    `g0 = g0 + 3` update on the taken path.
  - Stage localization:
    the raw conservative `machine_ir` allocate+rewrite dump was still correct,
    but the **canonicalized machine-ir** dump was already wrong. The key bad
    rewrite came from
    `src/machine/lowering/machine_ir/machine_ir_slot_cleanup.inc`:
    `machine_ir_try_fold_known_slot_return_tails(...)` /
    `machine_ir_try_fold_known_slot_branch_tails(...)` were allowed to replace
    a non-empty jump block wholesale with a successor's rewritten inlineable
    return/branch wrapper, even when the current block's own instructions were
    still needed to define the rewritten operand or to preserve side effects
    such as `store_global`.
  - Kept repair:
    known-slot inlineable tail folding now distinguishes
    `1)` blocks that are safe to replace wholesale from
    `2)` non-empty predecessor blocks that must keep their body.
    When the rewritten inlineable instruction cannot safely replace the whole
    block, the cleanup now appends the rewritten instruction and new
    terminator to the existing block instead of deleting the predecessor body.
    The same safety rule was then widened across the sibling
    known-slot inlineable fold sites too (shared return-successor and shared
    branch-successor variants), because the same “replace whole non-empty
    predecessor block” unsoundness exists there as well.
  - Focused rechecks after the fix:
    `/tmp/case001_dbg.c` PASS,
    `08_global_arr_init.sy` PASS,
    `28_side_effect2.sy` PASS,
    `32_many_params3.sy` PASS,
    `34_multi_loop.sy` PASS,
    `09_BFS.sy` PASS,
    `10_DFS.sy` PASS,
    `lava_test/short_circuit1.sy` PASS.
  - Adjacent widening after the fix:
    a new host-oracle-backed no-array/global-side-effect batch under
    `/tmp/runtime_re_globaltail_batch` now stays fully green
    (`100/100 PASS`). That batch intentionally stresses
    `global mutation + branch-gated later return/global reload + nested helper
    calls` around the same repaired family, so current authority is that the
    immediately adjacent witness family is reclosed on the live tree.
    Another sibling batch under `/tmp/runtime_re_sharedtail_batch`,
    targeting `shared tail / wrapper fold / global side-effect / nested call`
    shapes, also stays fully green (`100/100 PASS`).
    A third sibling batch under `/tmp/runtime_re_wrappermerge_batch`,
    targeting duplicated then/else wrapper-merging opportunities with helper
    calls and global stores, also stays fully green (`100/100 PASS`).
    A fourth sibling batch under `/tmp/runtime_re_callwrapper_batch`,
    targeting duplicated call-wrapper tails plus surrounding global mutation
    and branch gating, also stays fully green (`100/100 PASS`).
    A fifth sibling batch under `/tmp/runtime_re_branchwrapper_batch`,
    targeting duplicated branch-wrapper tails with surrounding global mutation,
    helper calls, and branch gating, also stays fully green (`100/100 PASS`).
    The first attempt to widen farther into a “linear-merge-heavy” family with
    `/tmp/runtime_re_linearmerge_batch` produced a few red points, but that
    batch is now **disqualified as generator UB** rather than kept as a real
    compiler lead: the generated arithmetic let signed `int` expressions grow
    into large overflow territory, so host-oracle `.c` behavior was no longer
    a reliable reference. A rebuilt overflow-safe version under
    `/tmp/runtime_re_linearmerge_safe_batch` now stays fully green
    (`100/100 PASS`).
    A separate bounded array/indirect-memory widening under
    `/tmp/runtime_re_arrmerge2_batch`, targeting `bounded array traffic +
    merge-heavy control flow + helper-call interleaving`, also stays green
    (`50/50 PASS`).
    Another bounded indirect-memory/control-flow batch under
    `/tmp/runtime_re_indirectphi_batch`, targeting
    `indirect memory + merge-heavy branch joins + helper-call interleaving`,
    also stays green (`60/60 PASS`).
    A compare-branch-heavy bounded batch under
    `/tmp/runtime_re_cmpbranch_batch`, targeting
    `compare-branch immediate/control-flow density + helper-call interleaving`,
    also stays green (`80/80 PASS`).
  - Outstanding local validation note:
    the repository's broad golden-output unit surfaces (`test-machine-ir`,
    `test-compiler-driver`) are not currently a clean confidence signal in
    this worktree, so the immediate evidence for this closure is the direct
    witness plus the rotated course/external runtime-facing rechecks above.
  the earlier many-args batch is no longer an open runtime-wrong-code lead.
  The active runtime-RE hunt should now widen **away from** this closed
  family instead of continuing to treat `3002/3003` as the front-most
  witness.

- 2026-05-11: follow-up continuation after that `machine_ir` closure:
  - regression maintenance:
    the sibling guard
    `test_machine_ir_cleanup_keeps_nonempty_known_slot_branch_tail_local_store`
    is now wired into the live `machine_ir` test mainline instead of being an
    unused static helper; `make build/machine_ir/machine_ir_test` rebuilds
    cleanly after that change.
  - local validation status:
    `make test-machine-ir` is still **not** a clean checkpoint surface for
    this line because it continues to stop earlier on the pre-existing
    allocate+rewrite dump-expectation noise (`spill.3/spill.4` shape), so the
    current evidence remains the focused runtime-facing rechecks rather than
    that whole target.
  - rotated runtime-facing rechecks:
    `/tmp/recheck_runtime_re_globaltail_rotA` => `25/25 PASS`,
    `/tmp/recheck_runtime_re_cmpbranch_rotA` => `16/16 PASS`,
    direct witness rerun `case_001.c` => PASS.
    One ad-hoc symlinked c-only `noarray_hidden2` subset produced many SKIPs
    because the subset lost standalone oracle context for several generated
    cases; treat that as harness noise, not as new compiler evidence.
  - reread/audit boundary advanced:
    `src/machine/lowering/machine_select/machine_select_lower_control.inc`,
    `src/machine/lowering/machine_select/machine_select_cleanup.inc`,
    `src/machine/object/machine_bytes/machine_bytes.c`, and
    `src/compiler/compiler_driver.c`
    were reread again under the generated-program runtime-RE lens after the
    latest `machine_ir` fix.
  - current conclusion:
    no fresh concrete runtime-RE bug is localized in that downstream reread
    span yet, so the active front-most kept closure still remains the
    `machine_ir_slot_cleanup` known-slot-tail family while the search
    continues through rotated witness generation/rechecks.

- 2026-05-11: next continuation after that downstream reread:
  - sibling `machine_ir` cleanup reread:
    I re-audited the always-on generic canonicalization folds in
    `src/machine/lowering/machine_ir/machine_ir_cleanup.inc`
    (`inlineable/call/thin wrapper tails`, `same-single-instruction branch
    successors`, `equivalent inlineable successor folds`, and linear jump-block
    merge) under the same “non-empty predecessor / side-effect / could this
    directly produce wrong generated code or segfault?” lens.
  - current code-read conclusion:
    still conservative: no fresh concrete wrong-code bug is proven in that
    sibling reread span yet.
  - rotated no-array/runtime-facing widening:
    two fresh host-oracle-backed families were added and both stayed green:
    - `/tmp/runtime_re_noarray_callmix_batch` => `40/40 PASS`
      (`scalar globals + recursion + nested calls + short-circuit + branch
      gating + constant-heavy locals`)
    - `/tmp/runtime_re_func_exprish_batch` => `80/80 PASS`
      (denser `func_expr2/global_var2`-adjacent
      `global mutation + nested call expressions + later global reload`)
- current authority after this chunk:
  the active front-most kept closure is still the
  `machine_ir_slot_cleanup` known-slot-tail family; the sibling generic
  `machine_ir_cleanup` folds have now also been reread without exposing the
  next concrete runtime-RE witness; and the search should keep rotating to
  nearby always-on runtime-risk families instead of reopening these now-green
  no-array/generated batches immediately again.

- 2026-05-11: a separate downstream preview-path bug was then localized in
  `machine_bytes` while validating the same always-on tail. `CMP_IMM` had a
  fallback path for non-optimized compare-immediate cases that prepended the
  left-operand preparation and then, after falling through to the general
  path, emitted that same left operand a second time. That made large
  spill-heavy blocks overrun their predicted byte counts and surfaced as
  `MACHINE-BYTES-341` on `83_long_array.sy`, `091_long_func.c`, and
  `94_nested_loops.sy`. The fix is to reset the offset before falling back so
  the left operand is written once. This is a separate compile-stage bug
  from the runtime-RE `machine_ir_slot_cleanup` witness, but it mattered for
  checkpoint stability on the current tree.
- 2026-05-11 follow-up tail-probe round: three new multi-structure probe suites were generated around the user's suggested tail pattern (`if (cond) x = new_value; return use(x);`) with direct if / if-else / nested-if / short-circuit / loop / call-chain / sequential-if variants. All of them stayed green on the live tree: `/tmp/runtime_re_tail_probe_suite` (`16/16 PASS`), `/tmp/runtime_re_tail_probe_suite2` (`32/32 PASS`), and `/tmp/runtime_re_tail_probe_suite3` (`36/36 PASS`). Current authority is therefore that this nearby family is not currently reproducing the remaining hidden runtime-RE line; the search should continue rotating to a different adjacent family rather than staying on this one.
- 2026-05-11 index-tail follow-up: one more generated family then moved from plain value tail-reads to address/index carrier tail-reads, specifically `if (cond) idx = new_idx; return arr[idx];`-adjacent shapes across local/global arrays, nested branches, short-circuit guards, and helper-call updates. The generated suite `/tmp/runtime_re_index_tail_suite` stayed fully green too (`16/16 PASS`). Current authority after this extra round is therefore stronger again: neither plain tail value-readbacks nor nearby tail index/address-readback families are currently exposing the remaining hidden runtime-RE line.
- 2026-05-11 fold-probe follow-up: one further generated family then targeted exactly the user-specified “optimizable but side-effectful” neighborhood with 96 cases of constant-true / constant-false-else / algebraically-constant / nested-constant / short-circuit-side-effect / sequential-if / address-index / global-index / call-chain / store-then-branch shapes, crossed with local/global/array/index/call/global-index/helper-store/array-global tails. The generated suite `/tmp/runtime_re_fold_probe_suite` stayed fully green too (`96/96 PASS`). Current authority after this round is therefore stronger again: this nearby side-effect-preservation-under-folding family is not currently reproducing the remaining hidden runtime-RE line either, so the next search step should rotate farther toward address-root/base-carrier corruption families.
- 2026-05-11 lv8 no-array follow-up: a new focused probe suite then targeted exactly the remaining user-listed lv8 cluster (`short_circuit1 / func_expr2 / global_var2 / hanoi2`) without arrays, using globals, short-circuit, nested function-expression calls, global shadowing, and hanoi-style recursion. The generated suite `/tmp/runtime_re_lv8_probe_suite` came back with a clean split: `short_circuit / global_shadow / hanoi` stayed green, but `func_expr` failed `5/5` (`lv8probe_05..09`). Current minimization already shrank that family to a much smaller nested-call-argument trigger: `m(f(1), h(2), f(3))`-style shapes fail, while simpler neighbors such as `f(1)+h(2)+f(3)`, `m(f(1), h(2), 3)`, `m(1, 2, f(3))`, and `m(1, h(2), f(3))` stay green. Current authority after this chunk is therefore that the front-most remaining lv8 hidden-like lead is now a **nested function-expression / call-argument lowering** family rather than broad short-circuit or hanoi behavior.
- 2026-05-11 clarification follow-up: the `m(f(1), h(2), f(3))` shape is now treated as a **求值顺序歧义伪 witness** rather than a stable compiler bug signal. The repository's actual call-argument lowering is left-to-right, so a host-oracle comparison against a different evaluation order is not a clean bug oracle. To avoid that ambiguity, I switched to a deterministic call/side-effect family where each full-expression keeps only one side-effectful call, and the new `/tmp/runtime_re_deterministic_funcexpr_safe` batch is fully green (`48/48 PASS`). The active search should therefore continue on the broader later-call / stack-arg / address-carrier family instead of the ambiguous nested-argument shape itself.
- 2026-05-11 compiler-driver isolation follow-up: an attempt to make the driver default conservative exposed that `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS` is currently an overly broad umbrella, not a pure final-peephole toggle. Flipping it changes at least:
  `.sbss/.sdata` vs `.bss/.data`,
  default SSA-build entry,
  perf-hotspot pass enablement,
  caller-save text wrapping around calls,
  and the final RISC-V text peepholes.
  A judge-side `AE` appeared under that broad conservative-default experiment, but local checks still passed (`make`, `autotest -riscv -s lv8`). To turn that into a useful diagnosis rather than a noisy rollback, the live tree now keeps the historical aggressive-default baseline while adding narrower env toggles in `compiler_driver.c`:
  `COMPILER_USE_SMALL_DATA_SECTIONS`,
  `COMPILER_USE_DEFAULT_SSA_BUILD`,
  `COMPILER_USE_PERF_HOTSPOTS`,
  `COMPILER_USE_CALLER_SAVE_TEXT`,
  `COMPILER_USE_FINAL_TEXT_PEEPHOLES`.
  The immediate next step is to use those one at a time against the judge so we can tell whether the `AE` is really tied to final text peepholes, caller-save text emission, section naming, or an earlier SSA-side divergence.
- 2026-05-11 caller-save follow-up: `COMPILER_USE_CALLER_SAVE_TEXT` is now confirmed as a separate live bug line. The first restore-placement error was fixed by moving the caller-save restore point to after the explicit `jalr` in caller-save mode, and the external/internal call split was also tightened so external helpers still print as symbolic `call foo`. That eliminated the judge-side `AE`, but the caller-save mode still reproduces runtime corruption locally on caller-heavy `lv9` shapes (`05_global_arr_init`, `08_arr_access`, `09_const_arr_read`, `12_more_arr_params`, `13_complex_arr_params` show `-11` in the caller-save sweep). The current narrowed suspicion is therefore live-value / frame-layout handling around some call sites, not assembler syntax anymore.

- 2026-05-11: conservative/default pipeline boundary clarification plus next
  always-on family widening:
  - traced current default path:
    `compiler_compile_source_text_with_options(...)` currently uses
    plain `value_ssa_build_from_lower_ir(...)` when
    `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS` is unset, skips
    `value_ssa_optimize_perf_hotspots(...)`, builds
    `machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(...)`,
    runs only raw `machine_select` lowering + verifier (later cleanup skipped),
    and skips the final preview-text peepholes in `compiler_driver`.
  - implication:
    the already-kept `machine_ir_slot_cleanup` canonicalization fix remains a
    valid/aggressive-path closure, but by itself it is **not** an always-on
    explanation for the still-open conservative default hidden runtime-RE line.
    The remaining search should therefore bias harder toward always-on
    surfaces:
    `machine_ir` lower without canonicalize,
    raw `machine_select` lower,
    `machine_bytes`,
    preview-text emission,
    and global/runtime layout handoff.
  - rotated always-on array/global widening:
    a fresh host-oracle-backed batch
    `/tmp/runtime_re_globalobjmix_batch` targeted
    `mixed initialized/uninitialized global arrays + multi-global declaration
    style + helper calls + 1d/2d array traffic + later global reloads`,
    and it stayed fully green (`60/60 PASS`).
  - current authority after this chunk:
    the hidden runtime-RE root cause is increasingly likely to sit in one of
    the **always-on** downstream surfaces above rather than only in the
    already-fixed aggressive cleanup/peephole layers.

- 2026-05-11: one more always-on widening round after that boundary
  clarification also stayed green:
  - `/tmp/runtime_re_runtimeinit_batch` => `40/40 PASS`
    (runtime-global-initializer / startup-helper family with declaration-order
    dependent scalar globals plus 1d/2d globals initialized through helper
    calls/branches)
  - `/tmp/runtime_re_indirect_globalmix_batch` => `50/50 PASS`
    (always-on indirect/global-array traffic with array-parameter helpers,
    repeated indexed reads/writes, helper calls, and later global reloads)
  - current authority after this chunk:
    the still-open hidden runtime-RE root cause looks even less like the
    already-expanded runtime-global-init or indirect-global-array families and
    even more like a narrower always-on downstream lowering/bytes/preview
    handoff issue.

- 2026-05-11: follow-up precision note on that boundary plus one more widened
  family:
  - default-path nuance:
    the conservative/default path still reaches `machine_ir` canonicalization
    **conditionally** through `machine_select_build_program_from_machine_ir_report(...)`
    whenever any function in the report still carries phi nodes; only fully
    phi-free programs stay on the raw machine-ir -> machine_select path.
  - reread span:
    `machine_select_lower_memory.inc`,
    `machine_bytes` global load/store/addr emit,
    and the matching compiler preview global/load/store/address pretty-printer
    path were reread again under the generated-program runtime-RE lens.
  - current code-read result:
    still conservative: no fresh concrete wrong-code bug proven from that
    reread span yet.
  - rotated conditional-canonicalization widening:
    `/tmp/runtime_re_phianchor_globaltail_batch` => `60/60 PASS`
    (`unrelated phi-bearing anchor function + no-array global-tail /
    branch-gated later reload family`)
  - current authority after this chunk:
    machine-ir canonicalization is still a conditionally-always-on surface on
    the default path, but this newly widened `phi-anchor + global-tail` family
    is green too, so the remaining hidden runtime-RE root cause should still
    be searched in a narrower downstream handoff issue rather than in this
    now-expanded slice alone.

- 2026-05-11: next downstream handoff follow-up after that:
  - reread span:
    `machine_layout_lower.inc`,
    `machine_emit_lower.inc`,
    and the corresponding `machine_bytes` compare-branch / fallthrough
    emission path were reread under the generated-program runtime-RE lens with
    emphasis on branch polarity, fallthrough target shaping, and global/call
    operand handoff.
  - current code-read result:
    still conservative: no fresh concrete wrong-code bug is proven directly
    from that reread span yet.
  - rotated downstream handoff widening:
    `/tmp/runtime_re_cmpfall_batch` => `80/80 PASS`
    (`compare-branch + fallthrough-heavy control + helper calls + global
    mutations + loop anchors`)
  - discarded line:
    one attempted stronger `phi + runtime-global-init + later global-tail`
    synthetic family is **disqualified as generator/oracle-invalid** rather
    than kept as a compiler lead; the first emitted `.out` values were wrong.
  - current authority after this chunk:
    the compare/fallthrough-heavy downstream handoff slice is widened and
    still green, so the remaining hidden runtime-RE root cause still looks
    narrower than these now-expanded control-handoff families.

- 2026-05-11: one more adjacent downstream-control widening round after that:
  - invalid line kept discarded:
    the heavier `phi + runtime-init + later global-tail` synthetic family
    remains **disqualified as generator/oracle-invalid** after host-equivalent
    checking; do not treat it as compiler evidence.
  - valid neighboring widening:
    `/tmp/runtime_re_localcmp_batch` => `60/60 PASS`
    (`local arrays + compare-heavy control + helper calls + later
    local/global interaction`)
  - current authority after this chunk:
    another nearby stack/local/control combination is green too, so the
    remaining hidden runtime-RE root cause still appears narrower than the
    already-expanded compare/fallthrough and local-array/control downstream
    handoff families.

- 2026-05-11: follow-up on the conditional-canonicalization + array/global
  angle:
  - valid widened family:
    `/tmp/runtime_re_phi_arraymix_batch` => `60/60 PASS`
    (`unrelated phi-bearing anchor function + array/global-side-effect-heavy
    helper code with local arrays, 2D locals, compare-heavy control, and later
    global reloads`)
  - current authority after this chunk:
    even the `phi present elsewhere` + `array/global hidden-family` combination
    is green on the live tree, so the remaining hidden runtime-RE root cause
    looks narrower than these now-expanded conditional-canonicalization +
    array/global combinations too.

- 2026-05-11: matching no-array follow-up on that same conditional-canonicalization
  line:
  - valid widened family:
    `/tmp/runtime_re_phi_funcexpr_batch` => `70/70 PASS`
    (`unrelated phi-bearing anchor function + func_expr2/global_var2-adjacent
    family with nested call expressions, global mutation, later global reload,
    short-circuit, and helper side effects`)
  - discarded first draft:
    the initial draft of this batch had a generator/source bug
    (`v` referenced before declaration in some cases) and was regenerated
    before treating the results as evidence.
  - current authority after this chunk:
    neither the array/global hidden family nor the lv8-style
    `func_expr/global-side-effect` hidden family reproduces when the default
    path is forced into the conditional `machine_ir` canonicalization branch by
    an unrelated phi-bearing function elsewhere in the same program.

- 2026-05-11: BFS/DFS-like follow-up on that same conditional-canonicalization
  line:
  - valid widened family:
    `/tmp/runtime_re_phi_bfsmix_batch` => `40/40 PASS`
    (`unrelated phi-bearing anchor function + BFS/DFS-like global-array,
    queue/visited, short-circuit recursion/queue gating family`)
  - current authority after this chunk:
    even the earlier BFS/DFS-style control/state-corruption suspicion is not
    reproducing when the default path is forced into the conditional
    `machine_ir` canonicalization branch elsewhere in the same program.

- 2026-05-11: address-carrier/array-parameter follow-up on that same
  conditional-canonicalization line:
  - valid widened family:
    `/tmp/runtime_re_phi_addrcarrier_batch` => `60/60 PASS`
    (`unrelated phi-bearing anchor + local arrays, 2D locals, array-parameter
    helper calls, repeated address-carrier style read/write traffic, global
    side effects, and later reload/useback`)
  - current authority after this chunk:
    even the address-carrier / array-parameter neighborhood is not currently
    exposing the hidden runtime-RE line when the default path is forced into
    the conditional `machine_ir` canonicalization branch elsewhere in the same
    program.

- 2026-05-11: hanoi-like recursion follow-up on that same
  conditional-canonicalization line:
  - valid widened family:
    `/tmp/runtime_re_phi_hanoi_batch` => `50/50 PASS`
    (`unrelated phi-bearing anchor + hanoi-like recursion with global peg
    arrays, global counters, short-circuit move gating, and later
    score/useback`)
  - current authority after this chunk:
    even the `phi elsewhere + hanoi-like recursive global-state family`
    combination is not currently exposing the hidden runtime-RE line on the
    live tree either.

- 2026-05-11: separate always-on caller-save suspicion follow-up:
  - discarded first draft:
    `/tmp/runtime_re_voidcall_save_batch` is **disqualified as
    generator/oracle-invalid** because the draft allowed negative `%`-based
    final results, making host exit-code oracles unstable/ambiguous.
  - corrected valid family:
    `/tmp/runtime_re_voidcall_save_batch2` => `70/70 PASS`
    (`void calls + live caller values + stack arguments + later reuse`)
  - current authority after this chunk:
    the preview-text caller-save/void-call suspicion did not produce a valid
    compiler witness once the oracle was repaired, so it should not be treated
    as the hidden runtime-RE explanation either.

- 2026-05-11: matching non-void caller-save/stack-arg follow-up:
  - valid widened family:
    `/tmp/runtime_re_nonvoid_stackcall_batch` => `80/80 PASS`
    (`non-void calls + stack arguments + live caller values after the call +
    later reuse`)
  - current authority after this chunk:
    neither the void-call nor the non-void-call version of the
    caller-save/stack-arg suspicion is currently reproducing the hidden
    runtime-RE line on the live tree.

- 2026-05-11: cross-product follow-up with conditional canonicalization:
  - valid widened family:
    `/tmp/runtime_re_phi_nonvoid_stackcall_batch` => `60/60 PASS`
    (`unrelated phi-bearing anchor + non-void calls + stack arguments + live
    caller values after the call + later reuse`)
  - current authority after this chunk:
    neither the plain path nor the phi-triggered conditional-canonicalization
    path is reproducing the non-void caller-save/stack-arg suspicion on the
    live tree.

- 2026-05-11: return-value / local-array pressure follow-up on that same
  line:
  - valid widened family:
    `/tmp/runtime_re_phi_returnspill_batch` => `80/80 PASS`
    (`unrelated phi-bearing anchor + non-void helper returns carried through
    local-array-heavy callers, many arguments, and later reuse`)
  - current authority after this chunk:
    even the return-value / local-array pressure variant of this always-on
    downstream suspicion is not currently exposing the hidden runtime-RE line
    on the live tree either.

- 2026-05-11: runtime-global-init + many-arg-call follow-up:
  - discarded first draft:
    `/tmp/runtime_re_rtinit_manyargs_batch` is **disqualified as
    oracle-invalid** because the expected outputs came from an incorrect
    handwritten runtime-init simulation.
  - corrected valid family:
    `/tmp/runtime_re_rtinit_manyargs_valid_batch` => `30/30 PASS`
    (`runtime global initializer helper + many-argument non-void calls + later
    array/global readback`)
  - current authority after this chunk:
    even the runtime-global-init + many-argument non-void-call family is not
    currently exposing the hidden runtime-RE line on the live tree once the
    oracle is made trustworthy.

- 2026-05-11: cross-product follow-up with conditional canonicalization on that
  same runtime-init line:
  - valid widened family:
    `/tmp/runtime_re_phi_rtinit_manyargs_batch` => `30/30 PASS`
    (`unrelated phi-bearing anchor + runtime global initializer helper +
    many-argument non-void calls + later array/global readback`)
  - current authority after this chunk:
    neither the plain path nor the phi-triggered conditional-canonicalization
    path is reproducing the runtime-init + many-arg non-void-call suspicion on
    the live tree.

- 2026-05-11: broad mixed always-on downstream follow-up:
  - discarded first drafts:
    `/tmp/runtime_re_megamix_batch` is **disqualified as generator-invalid**
    because it included incompatible 2D array parameter dimensions, and the
    intermediate `/tmp/runtime_re_megamix_valid_batch` is also disqualified
    because it still allowed invalid negative-index / negative-modulo oracle
    behavior.
  - corrected valid family:
    `/tmp/runtime_re_megamix_nonneg_batch` => `100/100 PASS`
    (`broad mixed always-on downstream family combining globals, local arrays,
    2D locals, scan helpers, recursion, call chains, stack-arg calls, and
    later global/local readback`)
  - current authority after this chunk:
    even a broader mixed downstream family is not currently exposing the
    hidden runtime-RE line once oracle-invalid UB is removed from the
    generated surface.

- 2026-05-10: the first widening step **after** closing the many-args family
  is now recorded too, and it produced one reopened lead plus one useful
  non-lead classification.
  - New generated adjacent stress direction:
    `/tmp/runtime_re_adjacent_batch3`
    (`many-args + arrays + alias-style kernels + branch nesting`, but kept
    below the earlier wide-branch cliff as much as practical).
    Result so far:
    `adj2_a`, `adj2_b`, `adj2_c`, `adj2_e` -> PASS;
    `adj2_d`, `adj2_f` -> `COMPILE_FAIL` with the already-known
    `MACHINE-BYTES-344: riscv preview branch target out of range` boundary.
    Current interpretation: this batch did **not** reproduce a new runtime RE
    yet; the red points here are compile-stage size/range limits rather than
    generated-program wrong-code.
  - Rotated external follow-up:
    `compiler2023 hidden_functional`
    `09_BFS.sy` PASS, `10_DFS.sy` PASS, `28_side_effect2.sy` PASS,
    `32_many_params3.sy` PASS.
  - Fresh reopened lead:
    `compiler2023 hidden_functional/34_multi_loop.sy`
    is now timing out again under focused reruns (`--case-timeout 120` still
    `RUN_TIMEOUT` on the current tree). Because this witness had previously
    been reclosed during the allocator work, current authority is to treat it
    as the **next active runtime/logic lead** instead of assuming the new
    result is harmless noise.

- 2026-05-10: that reopened `34_multi_loop.sy` lead is now reclosed too, and
  the actual root cause was one stage later than the earlier Value-SSA fix:
  **machine-ir lowering was reusing the pre-phi-elimination allocation across
  branch-carried live-through values**.
- Root cause:
  in deep nested-loop CFGs, raw machine-ir with explicit phi nodes could still
  look locally plausible, but once phi elimination materialized edge copies,
  some outer loop counters were still living through inner loop regions while
  sharing the same physical register with inner phi/branch values. That meant
  the no-phi program could clobber an outer carrier (for example `j`) inside
  the inner subtree, then later increment the wrong value on loop exit. The
  first visible symptom on this witness was `RUN_TIMEOUT`; after one partial
  protection pass it sharpened into a direct `MISMATCH`.
- Live repair:
  `src/machine/lowering/machine_ir/machine_ir_lower.inc` now allocates
  protected spill slots not only for call-crossing caller-clobbered values,
  but also for branch-sensitive / phi-sensitive live-through values:
  - phi results that would alias a live-out register on the same block
  - branch conditions that would alias a live-out register
  - and, more conservatively, register-colored values that are live-out of a
    branch block, so phi elimination no longer depends on the pre-phi
    allocation keeping those branch-carried carriers intact through nested
    regions
- Focused rechecks after this closure:
  - `34_multi_loop.sy` PASS
  - `make test-machine-ir` PASS
  - `autotest -riscv -s lv8` PASS (`12/12`)
  - `autotest -riscv -s lv9` PASS (`22/22`)
  - `compiler2023` focused follow-up:
    `09_BFS.sy`, `10_DFS.sy`, `28_side_effect2.sy`, `32_many_params3.sy`,
    `34_multi_loop.sy` -> PASS
  - post-fix generated widening:
    `/tmp/runtime_re_post34_batch` -> `4/4 PASS`
- Remaining adjacent note after this closure:
  the earlier `/tmp/runtime_re_adjacent_batch3` still has two reds
  (`adj2_d`, `adj2_f`), but those remain the already-known compile-stage
  `MACHINE-BYTES-344` branch-range boundary rather than a fresh runtime-RE
  family.

- 2026-05-10: after re-closing `34_multi_loop.sy`, the next widening step did
  find **one new similar runtime wrong-code lead** instead of another all-green
  batch:
  - new loop/phi-heavy generated batch:
    `/tmp/runtime_re_loopphi_batch`
    -> `7/8 PASS`
  - fresh failing witness:
    `loopphi_e.sy` -> `MISMATCH`
    (`expected 445`, actual `5898`)
- Current localization on `loopphi_e.sy`:
  - raw/rewritten Value-SSA still shows distinct values such as
    `ssa.91 = load_local acc.0` and `ssa.92 = load_local mix.1`
  - the final allocation result colors both to the **same register color**
    even though the interference graph still says they interfere
  - machine-ir lowering therefore emits bad shapes like
    `load local.0 ; load local.1 ; add reg, reg`, i.e. the first operand is
    lost before the binary op
- Working conclusion:
  this is now the next active runtime-risk family after the `34_multi_loop`
  closure: an allocator/select/mainline correctness bug where interfering
  non-spilled values can still receive the same color in a rewrite-heavy
  function. The currently attempted machine-ir-side protection work fixed the
  reopened `34_multi_loop` path but does **not** close this new direct
  `loopphi_e` witness yet, so the next implementation slice should pivot onto
  the allocator/mainline color-conflict root cause itself.

- 2026-05-10: that `loopphi_e.sy` lead is now reclosed too, and the final root
  cause turned out to be **one stage later than the allocator-color guess**:
  `machine_ir` lowering still needed to protect same-register multi-operand
  uses after allocation. The critical raw bad shapes were things like
  `load local.0; load local.1; add reg.0, reg.0` and similar nested-loop
  sibling forms where two distinct SSA operands had the same colored machine
  register and the second lowered use clobbered the first before the binary op
  consumed both values.
- Live repair:
  `src/machine/lowering/machine_ir/machine_ir_lower.inc` now extends protected
  spill-slot assignment beyond the earlier branch/phi live-through cases:
  - binary operands that would lower from two distinct SSA values onto the same
    colored register are protected through spill slots
  - call arguments with the same problem are protected the same way
  - `store_indirect(addr, value)` now also protects same-register addr/value
    pairs
  This keeps machine-ir lowering from collapsing two-input value operations
  into one clobbered register operand before any later cleanup or text export.
- Focused rechecks after this closure:
  - `loopphi_e.sy` PASS
  - widened loop/phi batch:
    `/tmp/runtime_re_loopphi_batch` -> **`8/8 PASS`**
  - post-fix adjacent widening:
    `/tmp/runtime_re_loopphi_postfix_batch` -> **`4/4 PASS`**
  - regression/kept-closure surfaces:
    `make test-machine-ir` PASS,
    `autotest -riscv -s lv8` PASS (`12/12`),
    `autotest -riscv -s lv9` PASS (`22/22`),
    `/tmp/ma300x` -> `3/3 PASS`,
    `/tmp/compiler2023_focus2` ->
    `28_side_effect2.sy`, `32_many_params3.sy`, `34_multi_loop.sy` all PASS
- Current authority after this closure:
  the loop-/phi-heavy family that reopened after `34_multi_loop` is now closed
  again. The active runtime-RE hunt should rotate to a different adjacent
  family instead of continuing to treat this loop-carried/multi-operand line
  as the front-most red witness.

- 2026-05-10: the next rotated post-fix widening step moved off the closed
  loop-/phi carrier family and onto **indirect-memory / array-address /
  repeated-index** shapes. The fresh generated batch
  `/tmp/runtime_re_indirect_batch` stayed all green (`8/8 PASS`), and the
  targeted external `compiler2023` array/indirection neighbors
  (`24_array_only`, `30_many_dimensions`, `31_many_indirections`) also stayed
  green when rerun through the local focused bundle. Current authority is
  therefore that this adjacent indirect-memory family did **not** immediately
  reproduce a new runtime wrong-code/RE after the latest machine-ir repair,
  so the next search step should rotate again instead of reopening this now
  all-green slice.

- 2026-05-10: one more rotated widening step then probed the next adjacent
  family: **array-parameter aliasing + nested calls + branch-heavy control**.
  The fresh generated batch `/tmp/runtime_re_aliascall_batch` stayed all green
  too (`8/8 PASS`). Current authority is therefore that this alias/call-heavy
  family also does **not** currently reproduce a new runtime wrong-code/RE on
  the current tree, so the next search turn should rotate again instead of
  sitting on this all-green slice.

- 2026-05-10: the next rotated widening step after that moved onto
  **indirect-store addr/value overlap + same-array aliasing through helper
  calls**. The fresh generated batch `/tmp/runtime_re_storealias_batch` stayed
  all green too (`8/8 PASS`). Current authority is therefore that this
  store-/alias-heavy adjacent family also does **not** currently reproduce a
  new runtime wrong-code/RE on the current tree, so the search should keep
  rotating instead of reopening these now-all-green synthetic slices.

- 2026-05-10: one more rotated widening step then probed
  **recursion + global-array mutation + short-circuit / branch gating**.
  The fresh generated batch `/tmp/runtime_re_recur_short_batch` also stayed
  all green (`8/8 PASS`). Current authority is therefore that this
  recursion/side-effect-heavy adjacent family does **not** currently reproduce
  a new runtime wrong-code/RE on the current tree either, so the search should
  keep rotating instead of reopening this now-all-green slice.

- 2026-05-10: the next rotated widening step then moved onto
  **local-array stack traffic + same-base multi-parameter aliasing + helper
  mutation**. The fresh generated batch `/tmp/runtime_re_localalias_batch`
  also stayed all green (`8/8 PASS`). Current authority is therefore that this
  local-array/stack-alias adjacent family does **not** currently reproduce a
  new runtime wrong-code/RE on the current tree either, so the search should
  keep rotating instead of reopening this now-all-green slice.

- 2026-05-10: the next rotated widening step after that moved onto
  **global-scalar/global-array side effects + branch-gated helper calls**.
  The fresh generated batch `/tmp/runtime_re_globalside_batch` also stayed all
  green (`8/8 PASS`). Current authority is therefore that this global-side-
  effect adjacent family does **not** currently reproduce a new runtime
  wrong-code/RE on the current tree either, so the search should keep rotating
  instead of reopening this now-all-green slice.

- 2026-05-10: the next rotated widening step after that moved onto
  **helper return values reused as array indices / branch gates**. The fresh
  generated batch `/tmp/runtime_re_retindex_batch` also stayed all green
  (`8/8 PASS`). Current authority is therefore that this return-value-as-index
  adjacent family does **not** currently reproduce a new runtime wrong-code/RE
  on the current tree either, so the search should keep rotating instead of
  reopening this now-all-green slice.

- 2026-05-10: after the invalid stress-witness line was disqualified, the next
  valid rotated widening step moved onto **multi-dimensional local/global
  array indexing + helper-returned bounded indices + same-base write/read
  recombination**. The fresh generated batch
  `/tmp/runtime_re_mdindex_batch` also stayed all green (`8/8 PASS`).
  Current authority is therefore that this multidimensional
  address-formation/index-reuse adjacent family does **not** currently
  reproduce a new runtime wrong-code/RE on the current tree either, so the
  search should keep rotating instead of reopening this now-all-green slice.

- 2026-05-10: the next valid rotated widening step after that moved onto
  **deep recursion + 12-argument calls + local-array stack traffic +
  branch-gated parameter permutation**. The fresh generated batch
  `/tmp/runtime_re_widerecur_batch` also stayed all green (`8/8 PASS`).
  Current authority is therefore that this wide-recursive stack/argument
  adjacent family does **not** currently reproduce a new runtime
  wrong-code/RE on the current tree either, so the search should keep
  rotating instead of reopening this now-all-green slice.

- 2026-05-10: the next two valid rotated widening steps after that moved onto
  **many scalar globals + nested short-circuit gating + helper side effects**
  and then a heavier follow-up with the same scalar-global/short-circuit idea.
  The fresh generated batches `/tmp/runtime_re_scalarshort_batch` and
  `/tmp/runtime_re_scalarshort2_batch` both stayed all green (`8/8 PASS`
  each). Current authority is therefore that this scalar-global /
  short-circuit-heavy adjacent family does **not** currently reproduce a new
  runtime wrong-code/RE on the current tree either, so the search should keep
  rotating instead of reopening this now-all-green slice.

- 2026-05-10: after that, I biased the next valid rotated widening step more
  explicitly toward optimization-sensitive shapes: **constant-heavy local
  assignments + trivially foldable arithmetic + branch-heavy control +
  scalar-global side effects**. The fresh generated batch
  `/tmp/runtime_re_constbranch_batch` also stayed all green (`8/8 PASS`).
  Current authority is therefore that this constant-heavy
  optimization-trigger adjacent family does **not** currently reproduce a new
  runtime wrong-code/RE on the current tree either, so the search should keep
  rotating instead of only restating the same scalar-global/short-circuit
  family.

- 2026-05-10: after that, I tried a first **splay-style tree rotation**
  family in response to the user hint. The larger direct batch
  `/tmp/runtime_re_splay_batch` only hit the already-known preview
  compare-branch span limit (`MACHINE-BYTES-345`) rather than a runtime lead,
  so I shrank it into a compile-safe version that still preserves the core
  tree-shape risks: global pointer arrays, parent/child rewiring, rotations,
  `find`/`kth`/`splay`, lazy tags, and branch-heavy update logic. That
  compile-safe batch `/tmp/runtime_re_splay_small_batch` stayed all green
  (`4/4 PASS`). Current authority is therefore that this first splay-style
  family does **not** currently reproduce a new runtime wrong-code/RE on the
  current tree, while the larger version only reiterates the existing branch-
  span compile boundary.

- 2026-05-10: after that, I pushed another explicitly optimization-sensitive
  widening round in the direction the user asked for: **constant-heavy locals
  + trivially foldable predicates + nested branch storms + scalar-global
  helper side effects**. The fresh valid batch
  `/tmp/runtime_re_optstress_batch` also stayed all green (`8/8 PASS`).
  Current authority is therefore that this optimization-stress adjacent
  family does **not** currently reproduce a new runtime wrong-code/RE on the
  current tree either, so the search should keep rotating instead of staying
  on only one constant/branch recipe.

- 2026-05-10: after that, I moved one step closer to the existing pass names
  instead of only writing broad optimization-stress code. The fresh valid
  batch `/tmp/runtime_re_exprreuse_batch` targeted **repeated arithmetic
  expressions + branch-merge joins + helper side-effect barriers**, and it
  stayed all green (`8/8 PASS`). The next valid batch
  `/tmp/runtime_re_purecall_batch` targeted **repeated pure-call-looking
  helpers + branch joins + impure barriers**, and it also stayed all green
  (`8/8 PASS`). Current authority is therefore that neither this
  expression-reuse nor pure-call-reuse adjacent family currently reproduces a
  new runtime wrong-code/RE on the current tree.

- 2026-05-10: after that, I completed the earlier “maybe try LCT after
  splay” suggestion instead of stopping at one tree-rotation checkpoint. A
  first `LCT-lite` batch with global arrays, `makeroot/access/link/cut`,
  pathsum queries, lazy reverse tags, and branch-heavy operation gating
  (`/tmp/runtime_re_lct_batch`) stayed all green (`4/4 PASS`). Current
  authority is therefore that this first link-cut-tree-style structural
  family also does **not** currently reproduce a new runtime wrong-code/RE on
  the current tree.

- 2026-05-10: after that, I pushed one more pass-shaped differential family
  rather than only adding new semantic stories. A direct raw-pointer version
  was discarded as invalid SysY syntax, and the corrected valid replacement
  `/tmp/runtime_re_addrreuse_batch2` used **array parameters + repeated
  same-base indexing + pure-expression reuse + side-effect barriers** to
  pressure `reuse_addr_roots` / `cleanup-pure`. That valid batch stayed all
  green (`8/8 PASS`), and there was no observed output drift between the
  normal compiler and skip-flag variants with
  `MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS=1` or
  `MACHINE_SELECT_SKIP_CLEANUP_PURE=1` on those witnesses. Current authority
  is therefore that this first differential addr-root / pure-cleanup family
  also does **not** currently reproduce a new runtime wrong-code/RE on the
  current tree.

- 2026-05-10: I also prepared one **conservative diagnostic submission mode**
  at the user's request so we can check whether the still-open hidden red
  points materially depend on optimization layers. In the current tree,
  aggressive optimization is now disabled by default unless
  `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS=1` is set. The conservative path
  keeps parsing/semantic/lower-IR intact but then:
  - uses plain `value_ssa_build_from_lower_ir(...)` instead of the default
    canonicalized / memory-SSA-heavy build
  - skips `value_ssa_optimize_perf_hotspots(...)`
  - returns early from `machine_select` immediately after the initial verify,
    skipping the later cleanup stack
  - skips final preview-text peepholes in `compiler_driver`
  This is intentionally a **diagnostic build**, not a claimed fix. Focused
  smoke rechecks on the conservative build stayed viable (`34_multi_loop.sy`
  PASS, `92_register_alloc.sy` PASS, `short_circuit1.sy` PASS), so the tree
  is ready if we want to compare judge results against this lower-
  optimization version.

- 2026-05-10: after the all-spill conservative submission still appeared
  unchanged on the judge, I shifted the active narrowing focus away from
  “optimizer main culprit” and into downstream non-optimization surfaces.
  The first library/ABI-adjacent batch on that line,
  `/tmp/runtime_re_libabi_batch2`, targeted **`getarray`/`putarray` + local/
  global arrays + 2D accumulation + scalar-global side effects**, and it
  stayed all green (`8/8 PASS`). Current authority is therefore that this
  first library-function / array-ABI adjacent family does **not** currently
  reproduce a new runtime wrong-code/RE on the current tree either.

- 2026-05-10: I also prepared one more downstream diagnostic variant to test
  whether small-data section policy itself could be part of the hidden
  runtime issue. In addition to the existing conservative/all-spill defaults,
  global-object emission now falls back to **plain `.bss/.data`** instead of
  `.sbss/.sdata` unless `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS=1` is set.
  This is intentionally diagnostic-only. Focused smoke rechecks on the
  relevant array/global surfaces stayed viable (`07_arr_init_nd.sy` PASS,
  `08_global_arr_init.sy` PASS, `24_array_only.sy` PASS), so this less
  small-data-specific backend configuration is ready if we want to compare
  judge results against it.

- 2026-05-10: after that, I added one more always-on-backend witness family
  instead of only toggling diagnostic modes. The fresh valid batch
  `/tmp/runtime_re_bigframe_batch` targeted **large stack frames + many local
  slots + repeated local/indirect memory traffic + helper-call mixing**, and
  it stayed all green (`8/8 PASS`). Current authority is therefore that this
  big-frame/local-slot adjacent family does **not** currently reproduce a new
  runtime wrong-code/RE on the current tree either, which weakens the older
  suspicion that the still-open hidden RE is just a generic large-frame /
  many-locals backend failure.

- 2026-05-10: after that, I prepared one more **calling-convention
  diagnostic variant** instead of only shrinking optimizers or writing new
  witnesses. In the current conservative mode, functions with calls now also
  reserve an explicit caller-save area and save/restore the caller-clobbered
  register set around each call site. This is intentionally diagnostic-only
  and specifically tests whether the still-open hidden RE is really a
  residual call-clobber / caller-save hole that the existing `machine_ir`
  call-crossing protections failed to cover. Focused smoke rechecks stayed
  viable (`34_multi_loop.sy` PASS, `92_register_alloc.sy` PASS,
  `short_circuit1.sy` PASS), so this caller-save diagnostic variant is ready
  if we want another judge-side comparison.

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
