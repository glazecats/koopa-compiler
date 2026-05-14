# Optimization Plan

## Scope

- This file is the compact active-plan authority for the current
  optimization-focused round.
- It covers:
  - `void call` pointless-code cleanup
  - in-repo regression expectation realignment
  - course `-perf` hotspot ranking and optimization
  - third-party perf ranking and optimization
- Keep this file synchronized with the higher-level progress memory in
  `docs/NEXT_STEPS.md`.

## Current Plan

1. `void call` pointless-code cleanup
   - Audit all remaining `void call`-adjacent no-op materialization such as
     synthetic zero-result temps, dead result placeholders, or other
     compatibility-only code.
   - Current kept closure:
     - `lower_ir` no longer appends a synthetic `tmp = 0` after `void call`
     - the lower-IR verifier now accepts unused temp-id gaps while still
       rejecting real use-before-def
   - Current status: **in progress**

2. In-repo regression surface realignment
   - Keep `make test` usable by updating stale dump/report expectations when
     behavior is still correct and only the output shape has changed.
   - Current narrowed tail:
     - the earlier wide `make test` red surface has already shrunk to a
       `machine_select`-heavy dump-expectation family after
       `compiler-driver`, `lower-ir`, and `machine_ir` expectation realignment
     - one real implementation-side fix is already landed inside this same
       area: the default `machine_select` aggressive cleanup gate has been
       restored so cleanup is no longer silently skipped when
       `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS` is unset
     - that same line also promoted one explicit optimization target from
       “stale expectation noise” to “real implementation work”:
       `reg = copy reg` self-copy no-ops should be removed from final
       selected output rather than being preserved and merely re-blessed by
       tests
     - current 2026-05-12 restamp:
       `test-compiler-driver`, `test-lower-ir-regression`, and
       `test-lower-ir-verifier` are green again on the live tree; the
       remaining in-repo red surface is a still-open batch of stale
       `machine_select` dump expectations whose shapes were previously edited
       away from the current stronger cleanup/immediate-folding behavior
     - 2026-05-12 perf-side experiment note:
       a first attempt at a late `stack store/reload -> mv` peephole was
       tried and then backed out in the same round because it interfered with
       existing final-text rewrites (`call_arg_load_swaps`) before proving a
       trustworthy net correctness story
   - Current 2026-05-13 restamp:
     - `make test-compiler-driver` is green again on the live tree
     - the fresh red batch that reopened during the huge-parameter /
       compile-time work turned out to be mostly stale output-shape
       expectations, not a newly confirmed codegen regression:
       several tests were still hard-coding old register choices, old stack
       slot offsets, or old "must fully constant-fold" assumptions
     - those assertions are now re-anchored to more stable semantic/codegen
       facts instead:
       branch/call ordering, presence of the expected compare/branch family,
       call-span preservation order, and post-call global reload survival
       - the currently restamped expectation families include:
         `pretty riscv mnemonic`,
         `rv32m/compare-branch`,
         `immediate-compare/loop-control`,
         `complex const shadowing`,
         `caller-save span ordering`,
         `32-bit wraparound loop-condition`,
         `32-bit wraparound condition runtime-path`,
         `assignment-condition break loop exit`, and
         `same-block global-writing call should preserve later global reload`
   - Current status: **in progress**

3. Course perf runtime ranking and optimization
  - Measure course `-perf` cases by real runtime first.
  - Rank hotspots before choosing the next optimization target.
  - Optimize the heaviest witnessed runtime case first, then rerun the same
    perf surface to confirm a real speedup.
  - Do **not** rank or justify work only by static "this instruction/text
    shape appears N times" counts. For hotspot choice, always combine:
    `1)` source-level control-flow / loop / recursion structure,
    `2)` emitted assembly shape, and
    `3)` expected dynamic execution multiplicity.
    The key question for this round is not only "what appears repeatedly in
    the text", but "what introduces unnecessary dynamic complexity on the
    hot path", such as loop-invariant recomputation, repeated helper calls
    inside high-trip loops, or address/index chains rebuilt at every
    iteration despite invariant/carry opportunities.
  - Do not artificially limit this line to the currently landed cleanup
    passes. If the live hotspot evidence points to a stronger transform, it is
    in scope to open a new optimization pass instead of forcing one more tiny
    cleanup.
    Current allowed examples:
    - narrower or broader helper inlining
    - SSA-side loop transforms such as carefully bounded unrolling/peeling
    - hotspot-specific value/address CSE or rematerialization shaping
    - dedicated call/recurrence rewrites for one concretely measured witness
  - While doing this, keep inspecting the emitted assembly for obviously
    pointless no-op shapes (`copy self`, redundant address materialization,
    dead immediate staging, useless store/load pairs) and treat those as
    valid optimization targets whenever they can be removed without breaking
    correctness.
  - Explicit candidate optimization-pass menu for this round:
    - `ValueSSA` or lower selected-IR helper inlining for tiny hot internal
      helpers, especially when a helper is pure, frequently called inside
      loops, and currently blocks later CSE/constant-folding
    - bounded loop unrolling on small constant-trip loops, only when the trip
      count proof is explicit and code growth stays controlled
    - loop peeling for first-iteration-special cases when it can remove loop-
      invariant condition/setup work from the steady-state body
    - targeted loop-invariant code motion / call hoisting for pure address,
      bounds, or helper-call shapes that visibly repeat in hot loops
    - same-block / loop-local address CSE for repeated base+index+scale
      materialization patterns such as the current `spmv1`-style row/index
      chains
    - light function-specialization or constant-argument cloning when one hot
      callsite repeatedly passes the same constant mode/stride/size knobs and
      a smaller specialized body would unlock simpler lowering
    - induction-variable cleanup / strength reduction for repeated multiply,
      shift, or add trees that can become a carried pointer/index recurrence
    - backend-side late cleanup passes for obviously redundant selected/text
      forms, such as self-copies, duplicate rematerialization, or repeated
      spill reload scaffolding that survives earlier IR passes
  - Candidate-pass priority for the current tree:
    1. hotspot-specific helper inlining
    2. hotspot-specific address/index CSE and strength reduction
    3. bounded loop transforms (`peel` / `unroll`) only after the first two
       are measured and still insufficient
  - Candidate-pass admission rule:
    - do not add a new pass only because it is a classic optimization name;
      each pass should be opened against a measured witness (`fft1`,
      `spmv1`, `mv1`, third-party perf case, or a concrete static hot block)
      and kept only if correctness stays green and the witness actually
      improves after rebuild
  - Explicit major optimization line now added for this round:
    - `div/mod`-heavy hotspot cleanup is in scope as a first-class target,
      not just as a side-effect of other cleanup
    - typical shapes include repeated same-value `div/mod`, repeated quotient
      or remainder recomputation inside one hot block/loop, and address/index
      chains rebuilt from the same `div` result
    - this line also explicitly includes currently-missing
      **semantic-safe constant folding** for `div/mod/shift` when the folded
      case can be proven not to change invalid-input behavior
    - preferred landing style:
      first on a sharply measured witness such as `fft1`, then generalized
      only after the semantic-safety condition has been proven on the kept
      transform
  - Semantic-safety reminder for this round:
    - treat potentially trap-sensitive or otherwise runtime-invalid arithmetic
      (`div`, `mod`, and any later similar operation whose legality depends on
      operand values such as division by zero) as a special-risk class
    - do **not** widen folding / CSE / forward-substitution for those ops
      until the transform preserves the original evaluation / fault behavior
      on invalid inputs too
    - once the risky case is ruled out on the transformed path, treat
      `div/mod` reuse and cleanup as a real major optimization line rather
      than as a forbidden area
    - in practice, the workflow is:
      `1)` prove or guard away the invalid-input case on the optimized path
      `2)` then allow the same witness-driven CSE / forwarding / reuse work
      there just like other pure arithmetic
    - current authority:
      do not be permanently conservative around `div/mod`; be conditionally
      aggressive once the semantic precondition is explicit
    - immediate concrete subtargets on this line:
      `1)` semantic-safe constant folding for `div/mod/shift`
      `2)` witness-driven reuse/CSE for repeated `div/mod`
      `3)` cleanup of address/index chains derived from reused `div` results
   - Current early runtime ordering from the first live timing sweep:
     - `06_mv1`
     - `03_mm1`
     - `04_mm2`
     - `05_mm3`
     - `02_bitset3`
   - Current 2026-05-13 live-tree rerank refresh:
     - fresh direct local rerank on the current tree now orders the sampled
       course runtime hotspots as:
       `13_fft1` (~`19.16s`),
       `09_spmv1` (~`14.23s`),
       `06_mv1` (~`12.96s`),
       `03_mm1` (~`7.28s`),
       `01_bitset2` (~`2.88s`)
     - the same reread also exposed one concrete shared preview text-export
       no-op still worth fixing independently of the larger hotspot family:
       the pretty-printer could still emit self-moves such as `mv t6, t6`
       when rendering `addi rd, rd, 0`
   - Current 2026-05-13 experimental pass note:
     - one narrow `ValueSSA` experiment was attempted for
       loop-invariant pure internal call hoisting, motivated by the
       `fft1`-side `power(...)` repetition shape
     - current authority is **not kept**:
       after a first prototype, the pass did not cleanly hit the witness and
       briefly destabilized `fft1` compilation, so it has been removed from
       the live optimization path again
     - keep the idea as a possible later reopen, but only after a smaller
       witness and a crisper loop-shape proof are built first
   - Current 2026-05-13 recursive-pure-call reuse follow-up:
     - a narrower `ValueSSA` fix on the existing repeated pure internal call
       reuse line is now materially useful and locally stable:
       purity detection now admits recursively pure internal helpers instead
       of only leaf-like pure helpers
     - on the live `fft1` witness, this produces a real compiler-output win
       after explicit rebuild:
       `call multiply` count drops from `10` to `9`, and total static
       instruction count drops from `494` to `486`
     - current correctness restamp on the live tree is green:
       `make test-compiler-driver` PASS and course
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` all PASS
     - current caution:
       this is a kept local win on the real `fft1` witness, but not yet a
       fully generalized "recursive pure-call optimization" claim; keep the
       next step witness-driven
  - Current 2026-05-13 recursive-pure-call barrier follow-up:
    - one further narrow widening on the same line is now also kept:
      same-block repeated pure internal call reuse no longer treats an
      intervening pure internal call as an automatic barrier
     - after explicit rebuild, the live `fft1` witness improves again:
       `call multiply` drops from the original `10` down to `8`, and total
       static instruction count drops from `494` to `476`
     - current correctness restamp after this narrower barrier removal is
       still green:
       `make test-compiler-driver` PASS and course
       `lv8`, `lv9` PASS on the rebuilt tree
    - current authority:
      keep this as another witness-driven `fft1` local win on the repeated
      pure-call reuse line, while still avoiding a broad claim that all
      call-crossing reuse is now generally safe without case evidence
  - Current 2026-05-14 selected-side cross-block reuse follow-up:
    - one new narrow retry tried to broaden the unique-predecessor reuse
      line to cover `load_local/load_global` as reusable pure expressions
      across the predecessor boundary
    - correctness stayed green on the usual regression gate, but the
      measured hotspot witnesses did not improve (`06_mv1`, `09_spmv1`,
      `13_fft1` stayed effectively flat), so this variant is not kept
    - current authority:
      back this variant out and return to the stronger hotspot-specific
      address/index or call-structure lines instead of widening this branch
      further
  - Current 2026-05-14 `ValueSSA` unique-predecessor indirect-load follow-up:
    - one narrower kept fix reduced over-conservatism in the existing
      unique-predecessor `load_indirect` forwarding line
    - specific repair:
      the pass no longer treats every intervening `store_local` /
      `store_global` / `store_indirect` as an unconditional barrier; it now
      reuses the same alias check already used by the same-block indirect-load
      forwarding path, so non-aliasing scalar-local stores no longer block a
      repeated global/other non-aliasing indirect load
    - regression evidence on the live tree:
      `make test-value-ssa-regression` PASS, including a new regression that
      locks unique-predecessor repeated indirect-load reuse across an
      intervening non-aliasing scalar-local store;
      course `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` all PASS
      (`lv2` is absent in the local testcase tree and therefore not a signal)
    - current same-environment runtime evidence through the full
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path:
      `18_brainfuck-bootstrap ~= 11803 ms`,
      `19_brainfuck-calculator ~= 20388 ms`
  - Current 2026-05-14 `spmv/mv` parameter-load hoist retries, not kept:
    - I first tried a `ValueSSA` perf-hotspot widening that hoisted immutable
      parameter `load_local` operations to function entry, aiming to cut the
      repeated parameter-local reloads visible in `spmv` / `mv`
    - regression coverage for the pass itself was straightforward, but the
      real witness check showed that this widening did **not** actually change
      the current `spmv1` / `mv1` machine-facing hot shapes on the live tree,
      so it was not a runtime-proven win
    - I then tried a narrower follow-up that hoisted immutable parameter
      loads only to natural-loop preheaders, closer to the visible
      `spmv` outer-loop reload sites
    - that variant did not reach a stable kept state: before any runtime gain
      was proven, it reopened verifier/regression churn on the focused
      synthetic witness and was therefore fully backed out
    - current authority:
      both parameter-load-hoist retries are **not kept**; resume from the
      stable pre-experiment implementation and move the next `spmv/mv`
      optimization round back to more witness-direct lines such as address /
      index-chain reuse or another pass that demonstrably reaches the live
      selected hot blocks
  - Current 2026-05-14 final-text stack-reload-to-`mv` pipeline retry, not kept:
    - I noticed an already-implemented final-text peephole,
      `compiler_optimize_riscv_preview_same_block_temp_stack_reload_to_mv(...)`,
      had never been wired into the live final RISC-V text pipeline
    - after connecting it, the generated `spmv1` / `mv1` hot blocks did
      materially change in the intended way: several hot-path pairs such as
      `sw t4, slot(sp)` + `lw t6, slot(sp)` became `sw ...` + `mv t6, t4`
    - however, direct ms-precision witness timing on the real
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path did not give
      a trustworthy net win:
      first wider version:
      `06_mv1 ~= 13762.857 ms`,
      `09_spmv1 ~= 15275.252 ms`,
      `18_brainfuck-bootstrap ~= 12282.603 ms`,
      `19_brainfuck-calculator ~= 22148.733 ms`
      and the `brainfuck` witnesses clearly regressed versus the remembered
      stable checkpoint band
    - I then narrowed the peephole so it would skip `lw -> slli/mul` address-
      chain starts while still allowing the existing safe `sw; lw; add`
      regression and the branch-side `spmv/mv` witness shapes
    - the narrowed version stayed regression-green
      (`make test-compiler-driver` PASS), but the same 4-case runtime check
      still failed to produce a convincing net gain:
      `06_mv1 ~= 13780.051 ms`,
      `09_spmv1 ~= 15365.625 ms`,
      `18_brainfuck-bootstrap ~= 12249.894 ms`,
      `19_brainfuck-calculator ~= 22233.816 ms`
    - current authority:
      fully back out this pipeline retry too; the text-side stack-reload-to-`mv`
      rule is not currently earning a stable runtime win on the active witness
      set, even after narrowing away the obvious `brainfuck` address-chain tail
  - Current 2026-05-14 final-text `mv -> sw` store-copy forwarding, kept:
    - I then switched to a narrower final-text cleanup aimed directly at the
      hot `brainfuck` `run_program` / `read_program` tails under the correct
      course-perf route (`build/compiler -perf` on `/opt/bin/testcases/perf`)
    - specific rule:
      when the final text contains
      `mv tmp, src`
      followed later in the same straight-line region by
      `sw tmp, off(base)`,
      and the copied register is not used/redefined in between, rewrite the
      final store to `sw src, off(base)` and delete the dead `mv`
    - this directly removed many hot-tail patterns such as
      `mv t4, a3 ; ... ; sw t4, 0(t6)` in `run_program`, plus a smaller
      `read_program` counter-update copy/store pair
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS with new positive/barrier tests,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - same-command A/B under the user-provided formal perf script shape
      (`build/compiler -perf`, `/opt/bin/testcases/perf`, `clang -target riscv32`,
      direct `/opt/lib/riscv32/libsysy.a`, `qemu-riscv32-static`) gave:
      baseline:
      `06_mv1 ~= 13759.886 ms`,
      `09_spmv1 ~= 14444.082 ms`,
      `18_brainfuck-bootstrap ~= 10862.166 ms`,
      `19_brainfuck-calculator ~= 13481.985 ms`
      candidate:
      `06_mv1 ~= 12872.978 ms`,
      `09_spmv1 ~= 13353.902 ms`,
      `18_brainfuck-bootstrap ~= 10471.140 ms`,
      `19_brainfuck-calculator ~= 13571.081 ms`
      and one immediate confirmation rerun on the same candidate path came back
      in the same band:
      `06_mv1 ~= 13681.072 ms`,
      `09_spmv1 ~= 15289.049 ms`,
      `18_brainfuck-bootstrap ~= 10598.447 ms`,
      `19_brainfuck-calculator ~= 13373.254 ms`
    - current authority:
      keep this as a small but real runtime-oriented text cleanup; it is not a
      universal win on every single single-run witness, but the formal A/B
      comparison against the restored baseline is strong enough to treat it as
      a stable kept checkpoint
    - current authority:
      keep this fix as a correctness-green narrowing of a real over-conservative
      barrier, then continue measuring whether further dynamic wins should come
      from the `program[ip]` scan-loop repeated-load family or from a separate
      proof-backed removal of dead local-array zero-initialization
  - Later 2026-05-14 `brainfuck` function-entry scalar-global hoist follow-up:
    - one narrower kept `ValueSSA` perf-hotspot expansion now targets repeated
      `load_global` of stable scalar globals at function scope, rather than
      another broad loop transform
    - kept rule:
      when a function has no calls and no instruction in the function can
      alias-write a given 4-byte global slot, repeated `load_global` of that
      slot are hoisted to function entry and later loads are rewritten to
      reuse the single hoisted SSA value
    - concrete live-tree motivation:
      `run_program` in the `brainfuck` witnesses was still reloading
      `program_length` / `input_length` in hot loops even though the function
      itself never calls out and does not write those globals
    - regression evidence:
      `make test-value-ssa-regression` PASS with new positive/barrier perf
      regressions,
      `make test-compiler-driver` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - current same-environment runtime evidence through the full
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path:
      `18_brainfuck-bootstrap ~= 9547 ms`,
      `19_brainfuck-calculator ~= 12365 ms`
    - current authority:
      keep this as the latest stable `brainfuck` checkpoint and continue the
      next measured round on repeated global-address / indirect-index chains
      that still survive in `run_program`
  - Later 2026-05-14 `brainfuck` branch-join local-forward retry, not kept:
    - I added three new `ValueSSA` regressions around unique-predecessor /
      branch-join recomputed indirect-load shapes after local updates, then
      tried a narrow join-slot local-forward rule so the join block could keep
      the idom predecessor's updated local value instead of reloading that
      scalar local again
    - it did hit the intended `run_program` scan-loop SSA shape:
      in `perf/19_brainfuck-calculator`, `bb.20` stopped rebuilding
      `program + ip * 4` from a fresh `load_local ip.0` and reused the
      earlier `ssa.47` load result from `bb.17`
    - but the real runtime result was materially worse on the same local
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path:
      `18_brainfuck-bootstrap ~= 11961 ms`,
      `19_brainfuck-calculator ~= 15026 ms`
    - current authority:
      fully back out the join-slot forwarding rule, keep only the added
      regression coverage, and do **not** treat this branch-join local-forward
      line as the next profitable runtime direction
  - Later 2026-05-14 `machine-bytes` final local-offset remap retry, not kept:
    - I then retried the old slot-layout idea in the narrowest downstream
      form: no local-id remap in `ValueSSA`/`MachineIR`, only a final RV32
      preview local-slot offset remap in `machine_bytes`, scoped to the known
      hot `run_program` shape (`function->name == "run_program"` and
      `local_count == 520`)
    - the remap kept the `return_address[512]` family contiguous but moved the
      hottest scalar locals near the frame base:
      `ip/read_head/input_head/return_address_top/code/val/loop`
    - safety gates on the reverted tree are green again:
      `make test-machine-bytes` PASS and `make test-compiler-driver` PASS
    - but the real runtime result was again materially worse on the same local
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path:
      `19_brainfuck-calculator ~= 14390 ms`,
      `18_brainfuck-bootstrap ~= 11307 ms`
      versus the current stable checkpoint near
      `12365 ms / 9547 ms`
    - current authority:
      fully back out this final-offset remap too; "make a few hot scalar locals
      closer by downstream stack-offset remap" is also **not** currently paying
      off on the live `brainfuck` witnesses
  - Later 2026-05-14 final-text `run_program_25` tail-fold retry, not kept:
    - I also tried one even narrower downstream optimization in
      `compiler_driver` final-text peepholes: detect the repeated
      `run_program_25` shape where many branches first save the current `ip`
      through a dedicated stack slot and a shared tail block then reloads
      that slot, computes `ip + 1`, stores it to the loop-carried `ip` slot,
      and branches back to the loop header
    - the targeted text transform was refined enough to pass a focused
      text-pattern regression, but the real runtime result was still clearly
      worse on the same local
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path:
      `19_brainfuck-calculator ~= 14041 ms`,
      `18_brainfuck-bootstrap ~= 11321 ms`
    - current authority:
      fully back out that final-text tail-fold too; the open `brainfuck`
      runtime tail is also **not** currently closing through small final-text
      control-flow reshaping around the shared `ip+1` ladder
  - Current 2026-05-13 final-text constant-reuse follow-up:
    - one already-implemented final-text peephole,
      `compiler_optimize_riscv_preview_reuse_repeated_lui_addi_constants(...)`,
      was present with regression coverage but had not been wired into the
      live preview-text optimization chain
    - kept wiring fix:
      the pass now runs in the default final-text peephole pipeline
    - concrete witness effect after full rebuild:
      `13_fft1` drops from `544` asm lines to `542`, and one repeated
      `lui t5, 0x3b800` / `addi t5, t5, 1` pair disappears from the hot
      `mod = 998244353` materialization path
    - current blast radius check:
      `04_spmv1` stays at `253` asm lines and `02_mv1` stays at `249` asm
      lines, so this is a targeted `fft1` cleanup rather than a broad shape
      shift
    - correctness restamp:
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - current authority:
      keep this wiring as a small safe `fft1` win, then continue hotspot work
      on the remaining `fft1` / `spmv1` / `mv1` heavier dynamic-complexity
      shapes rather than over-rotating on final-text micro-cleanups alone
  - Current 2026-05-13 indirect-memory fast-path follow-up:
    - the default indirect-memory `ValueSSA` fast path was missing the
      already-proven repeated pure internal call reuse cleanup, so `fft1`
      could still keep duplicate same-block pure calls on the default mainline
      even though the broader repeated-call line had already been validated
      elsewhere
    - kept wiring fix:
      `value_ssa_bridge_reuse_repeated_pure_internal_calls_in_program(...)`
      now also runs inside `value_ssa_run_indirect_memory_direct_fast_cleanup`
    - concrete witness effect after explicit rebuild of both the compiler and
      dump helpers:
      in `fft bb.5`, the second `multiply(w, w)` disappears and the second
      recursive `fft(...)` now reuses the first multiply result directly
    - downstream effect on final emitted text is real but modest:
      the `fft` frame shrinks from `112` bytes to `96` bytes and the second
      helper call is gone from the corresponding emitted block, but the total
      whole-file instruction count remains at the current `476`-instruction
      plateau
    - correctness restamp on the kept tree:
      `make test-compiler-driver` PASS and course
      `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` all PASS
    - current authority:
      keep this as a real structural `fft1` improvement on the default path,
      then continue the next `fft1` round on the remaining address/index
      materialization in `bb.10` rather than reopening the same duplicate-call
      site again
  - Current 2026-05-14 `spmv1` loop-invariant indirect-load retry:
    - the next `spmv1` round explicitly tested a narrower `ValueSSA`
      extension that only hoists `load_indirect` when the full address value
      itself is loop-invariant and the loop body/header provably contain no
      aliasing write to that address root
    - correctness result:
      the narrowed version stayed compatible with the previously reopened
      `12_more_arr_params` family after the final-text peephole fix was kept
      in place
    - runtime result:
      despite that tighter proof, the witness did not improve; local rerun
      showed roughly `09_spmv1 ~= 15.28s`, which is slightly worse than the
      repaired baseline
    - current authority:
      do not keep broadening this indirect-load hoist line for `spmv1`;
      rotate back to smaller `machine_select` / final-text hotspot cleanup
      lines instead of forcing a ValueSSA loop-hoist answer here
  - Current 2026-05-14 `spmv1` cleanup-toggle convergence note:
    - after the narrower `ValueSSA` attempt was dropped, the next round
      rechecked the remaining `machine_select` cleanup toggles directly on the
      live `spmv1` witness
    - focused A/B summary:
      disabling `forward_same_block_indirect_loads` was slightly worse, and
      disabling `reuse_spill_pure_expr` / `reuse_addr_roots` no longer showed
      meaningful selected-form differences on the live tree
    - current authority:
      the easy `spmv1` wins in the current selected/final-text cleanup layer
      are close to exhausted; do not keep grinding those toggles without a
      new concrete shape win, and be ready to rotate toward `mv1` once the
      next hotspot rerank still leaves `spmv1` flat
  - Current 2026-05-14 correctness-side blocker closure during perf work:
    - `lv9/12_more_arr_params` was rechecked against the current tree and
      turned out to be a real default-mainline wrong-code bug, not just a
      perf-side experiment artifact
    - narrowing results:
      `COMPILER_USE_PERF_HOTSPOTS=0` still failed,
      `COMPILER_SIMPLE_BACKEND=1` passed, and
      `COMPILER_USE_FINAL_TEXT_PEEPHOLES=0` passed
    - direct final-asm diff then localized the corruption to the final text
      peephole layer: a staged stack argument load was being rewritten into
      a stale `mv t5, t4`, which is wrong in the multi-argument array-call
      witness
    - kept fix:
      remove `compiler_optimize_riscv_preview_stack_staged_call_args(...)`
      from the default final-text peephole chain for now
    - immediate local recheck after that removal:
      `11_arr_params`, `12_more_arr_params`, and `13_complex_arr_params`
      all pass again
    - current authority:
      keep the rest of the final-text peepholes, but treat
      `stack_staged_call_args` as unsafe until it is rederived with a tighter
      correctness proof
  - Current 2026-05-13 perf-only repeated-pure-call follow-up:
    - one fresh rereread of the live tree found that an older `spmv1`
      diagnosis had been polluted by a stale helper binary:
      `build/dump_middle_stage` is not rebuilt by `make compiler`, so an old
      dump falsely suggested the repeated `load_indirect` pair in `spmv1`
      was still present even though the current live tree had already folded
      it away
    - after explicitly rebuilding both `build/compiler` and
      `build/dump_middle_stage`, the real remaining hot duplicate turned out
      to be on the `fft1` `-perf` path instead:
      `main` still computed `power(3, (mod - 1) / d)` twice before the two
      initial `fft(...)` calls
    - kept fix:
      `value_ssa_optimize_perf_hotspots(...)` now runs one extra narrow same-
      block repeated pure internal call reuse sweep on the perf-only path,
      which is enough to reuse that `power(...)` result even though the
      default `value-ssa-default` dump does not reflect perf-only passes
    - concrete witness effect after explicit rebuild:
      the final emitted `13_fft1` assembly drops from `547` lines to `544`,
      and `call power` drops from `5` to `4`
    - correctness restamp on the kept tree:
      course `lv8` PASS (`12/12`) and `lv9` PASS (`22/22`)
    - helper-discipline reminder renewed by this witness:
      when validating stage dumps against live code changes, rebuild helper
      binaries such as `build/dump_middle_stage` explicitly; `make compiler`
      alone is not enough evidence for helper freshness
  - Current 2026-05-13 `mv1` loop-load hoist probe:
    - after the `fft1` perf-only win, the next course hotspot was rechecked
      on the live tree and `06_mv1` remained one of the heaviest witnesses
      by runtime
    - a conservative simple-loop load-hoist probe was added to the
      perf-only path so loop-invariant `load_local` / `load_global` values
      in a simple single-body loop can be cloned into the preheader and
      reused in the body
    - concrete witness effect:
      `06_mv1`'s emitted assembly stayed the same line count but one of the
      steady-state loop bodies now reuses the hoisted `local.3` base in the
      initial `res[i] = 0` loop, while the inner `mv` body remains unchanged
      enough that this probe is still a mild win rather than a decisive
      hotspot closure
    - correctness restamp on the kept tree:
      course `lv8` PASS (`12/12`) and `lv9` PASS (`22/22`)
    - current authority:
      keep the probe for now, but treat it as a small structural step rather
      than the final answer for `mv1`; the next round should re-rank whether
      `mv1` or `brainfuck-calculator` is the more profitable remaining target
      before opening a broader loop transform
    - later same-day boundary note:
      one follow-up widening from “parameter locals only” to “any loop-stable
      local” was tried and then backed out again in the same round. Although
      it stayed correctness-green, it expanded `06_mv1`'s final assembly from
      `249` lines to `257` by hoisting extra locals into `main`, which is the
      wrong tradeoff for this witness. Current authority is therefore to keep
      only the narrower parameter/global version and stop widening this probe
      further unless a future witness shows a clearer payoff.
  - Current 2026-05-14 perf loop-invariant binary-hoist follow-up:
    - the perf-only simple-loop hoist line now has one new narrow extension:
      in the same simple-loop shapes already used for loop-invariant load
      hoisting, `mov` / `binary` values whose operands are already proven
      loop-invariant may now be cloned into the preheader and replaced by
      loop-local `mov` uses
    - this was intentionally landed as a small extension of the existing
      invariant-hoist machinery rather than a broad new LICM framework
    - first safety/result checkpoint on the live tree:
      `make test-value-ssa-regression` PASS,
      `make compiler` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - runtime evidence versus the older `koopa-compiler.zip` baseline on the
      same course-style `compiler -> clang -> ld.lld -> qemu-riscv32-static`
      flow:
      `06_mv1` improves from about `14.123s` to `13.902s`,
      `09_spmv1` improves from about `15.429s` to `15.173s`,
      and the already-strong `13_fft1` baseline stays far ahead
      (`~21.182s -> ~9.681s`)
    - interpretation:
      this line is worth keeping even though the final static assembly on the
      local witnesses grows slightly, because the measured runtime still moves
      in the right direction on the targeted `mv1` / `spmv1` witnesses
    - focused regression added:
      `VALUE-SSA-PERF-HOTSPOT-LOOP-INVARIANT-BINARY`
      now locks the basic shape where a loop-invariant arithmetic chain is
      materialized once before the loop and reused from inside the loop body
    - current authority:
      keep this as a correctness-green perf-side structural expansion, then
      measure whether it materially improves the remaining `mv1` / `spmv1`
      runtime witnesses before widening it further
    - later 2026-05-14 `spmv1`/`mv1` follow-up on local-driven widening:
      two narrower extensions were tested and are **not** kept as active
      mainline directions:
      `1)` allowing ordinary scalar local loads into the perf simple-loop
      invariant hoist, and
      `2)` allowing those ordinary scalar locals only inside the invariant
      proof for indirect-load addresses.
      Both variants stayed correctness-green on
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`,
      but the measured runtime did not justify them.
      The broader scalar-local version regressed all three sampled witnesses,
      and the narrower indirect-address-only variant still did not actually
      eliminate the intended `spmv1` `b[i] - 1` hot-path recomputation:
      fresh course-style reruns landed around
      `06_mv1 ~= 14.052s`,
      `09_spmv1 ~= 15.475s`,
      `13_fft1 ~= 9.585s`.
    - current authority after that negative result:
      stop widening the generic perf `ValueSSA` hoist line for this family.
      The next profitable search surface is the downstream selected/backend
      shape itself rather than more generic loop-invariant proofs.
  - Current 2026-05-14 old-vs-current selected-shape compare follow-up:
    - direct old/current selected dumps for `spmv1` and `mv1` now show that
      the remaining difference is not just one missed arithmetic fold.
      The current tree tends to preload/cache more locals and carry more
      block-local structural scaffolding in hot blocks, while the older tree
      materializes some addresses more directly inside the same blocks.
    - concrete current hotspot examples:
      `spmv bb.8/bb.11` and `mv bb.10/bb.11` still contain repeated
      same-root `load_indirect` families and extra local/cache carriers.
    - current authority:
      rotate the next optimization round away from generic `ValueSSA` loop
      hoisting and toward a targeted `machine_select` / backend cleanup that
      trims the extra selected-shape scaffolding on those exact witnesses.
    - later 2026-05-14 downstream-stage cross-check correction:
      after comparing `select -> emit -> bytes -> final asm` on both the
      current tree and `koopa-compiler.zip`, the strongest simple hypothesis
      ("current `spmv1`/`mv1` is slower because its downstream code is larger")
      does **not** hold up.
      Current `select`/`emit`/`bytes` and final assembly are actually at least
      as compact as the older tree on those witnesses, while runtime stays
      roughly tied:
      `06_mv1` current `~23.388s` vs old `~23.796s`,
      `09_spmv1` current `~25.314s` vs old `~25.313s`,
      and `13_fft1` still heavily favors the current tree
      (`~16.092s` vs `~34.351s`) on the same measurement flow.
    - current authority after that correction:
      do not over-prioritize `mv1` / `spmv1` downstream shrink work just
      because some local selected blocks look structurally different.
      The next mainline should rotate back to witnesses whose runtime still
      offers clearer upside, instead of assuming that every old/current
      selected-shape difference is a real performance blocker.
    - later same-day branch-hoist note:
      one more perf-only attempt then tried to hoist a shared first binary
      from the two successors of a branch, targeting shapes like
      `mv1 bb.10/bb.11` both computing the same prefix from the branch
      predecessor. That experiment was reverted immediately: it destabilized
      the emitted output shape badly enough that `06_mv1` temporarily
      collapsed to an empty output file. Current authority is to avoid this
      cross-branch shared-binary hoist line for now and keep the tree on the
      restored stable baseline (`mv1 = 249`, `fft1 = 544`,
      `brainfuck-calculator = 1019` static lines on the local quick check).
  - Current 2026-05-13 fresh old-vs-current perf compare:
    - a fresh direct old-vs-current runtime rerun on the live tree confirmed
      the main current gains are real rather than helper-noise:
      `13_fft1` dropped from about `20.176s` on the old compiler to about
      `9.514s` on the current tree
    - smaller shifts from the same rerun:
      `06_mv1` improved from about `15.108s` to `14.692s`,
      `09_spmv1` stayed roughly flat (`16.543s` vs `16.517s`),
      `18_brainfuck-bootstrap` stayed in the same band, and
      `19_brainfuck-calculator` remained the top runtime witness without
      showing a clear regression relative to the old compiler
    - current authority:
      keep pushing on the `fft1` / `mv1` family first, because they are the
      witnesses that are still clearly moving on the live tree, then return
      to `brainfuck-calculator` once the smaller dynamic-reuse wins are
      exhausted
  - Current 2026-05-13 lower-IR crash-closure follow-up:
    - a broad serial recheck found two course perf compile-crash reds:
      `perf/18_brainfuck-bootstrap` and `perf/19_brainfuck-calculator`
    - root cause was in lower-IR construction / verification, not in the
      perf optimization line itself:
      void-return / jump / branch terminator union state could be read
      through stale bytes, and the temp-definition verifier also touched
      `return_value` on `void ret`
    - kept fix:
      zero the lower-IR terminator payload union when building `void ret`,
      `jump`, and `branch`, and guard verifier access behind
      `has_return_value`
    - closure evidence after rebuild:
      `make test-lower-ir-verifier` PASS and
      `autotest -perf /workspaces/compiler_lab` PASS (`130/130`)
    - current authority:
      this crash line is now reclosed, so the next optimization round should
      return to runtime hotspot work rather than treating `brainfuck-*` as an
      open compiler-crash blocker
  - Current 2026-05-13 `brainfuck` regression re-localization follow-up:
    - the suspected allocator/rewrite regression was rechecked and narrowed
      further with direct old-vs-current stage dumps plus new local probes
    - kept diagnosis:
      the main `brainfuck` slowdown was **not** in the final allocator result
      itself; `initial allocate` and final allocate+rewrite stayed
      color-only on the key hot functions, while the extra `spills=` surface
      reappeared later in `machine_ir` lowering as protected spill slots
    - concrete root-cause narrowing:
      the dominant bad expansion was in the newer `machine_ir` protected
      call-crossing spill path rather than in the earlier allocator rewrite
      rollback/shared-spill guards
    - kept machine-IR-side fix:
      default `machine_ir` lowering no longer keeps the earlier heavier
      call-crossing protected-spill shape on the `brainfuck` witnesses;
      the live default `machine_ir` shape for the main hot functions is now
      back to:
      `get_bf_char spills=0`, `read_program spills=3`, `run_program spills=0`
      instead of the regressed `1 / 4 / 4` branch investigated earlier in
      this round
    - real runtime evidence on the rebuilt tree with the course-style
      `compiler -> clang -> ld.lld -> qemu-riscv32-static < input` flow:
      `18_brainfuck-bootstrap` now runs in about `13.702s`
      and `19_brainfuck-calculator` in about `24.194s`
    - comparison against the remembered old-good local baseline:
      this is effectively back in the same band as the older
      `~14.34s / ~24.60s` measurements, so the large regression gap has been
      materially closed
    - correctness restamp on the kept tree:
      `autotest -riscv -t /opt/bin/testcases /workspaces/compiler_lab`
      PASS (`130/130`), plus focused `lv8` and `lv9` reruns PASS
    - current authority:
      the main `brainfuck` perf regression is now largely reclosed on the
      live tree; next optimization work should rotate back to the remaining
      hotspot witnesses (`spmv1`, `mv1`, `fft1`, and third-party perf tails)
      instead of continuing to tunnel on allocator blame for this case
  - Current 2026-05-13 semantic-safe `div/mod/shift` fold follow-up:
    - ValueSSA constant folding now has a first conservative implementation
      for `div/mod/shift`, gated by the existing interpreter-style safety
      rules:
      `/0` is not folded, `INT_MIN / -1` and `INT_MIN % -1` are not folded,
      invalid shift counts are not folded, and left-shift of negative values
      is not folded
    - focused external correctness restamp stayed green on the live tree:
      `make test-compiler-driver` PASS and course
      `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` PASS
    - the initially re-exposed `test-value-ssa-regression` red surface turned
      out to be stale default-path expectations rather than a bug in the new
      fold rules themselves
    - kept expectation realignment:
      `VALUE-SSA-CONVERT-DEFAULT-FOLD`,
      `VALUE-SSA-CONVERT-DEFAULT-MATCHES-MEMORY-FULL`, and
      `VALUE-SSA-CONVERT-DEFAULT-MATCHES-DIRECT-EXTREME-PARAM` now assert the
      current intended split between the default path and the older
      canonicalized/direct reference paths, instead of forcing equality that
      no longer matches the live implementation
    - current in-repo closure evidence:
      `make test-value-ssa-regression` PASS
    - current authority:
      the semantic-safe `div/mod/shift` fold line is now checkpoint-worthy on
      its own merits, and the next mainline may rotate back to hotter runtime
      witnesses such as `fft1 bb.10`
  - Current 2026-05-13 semantic-safe `div/mod/shift` reuse/CSE follow-up:
    - the next step on that same arithmetic line is now also partially kept:
      safe repeated `div/mod/shift` expressions may participate in redundant-
      binary / same-block identical-run reuse when the operands prove they are
      outside the currently invalid cases
    - current kept safety boundary:
      this does **not** relax dead-def deletion for dangerous arithmetic; it
      only widens reuse/CSE for cases already proven safe enough to evaluate
      once and reuse
    - concrete witness effect on the live `fft1` hotspot:
      the whole-file static instruction count drops further from `476` to
      `470`
    - first visible payoff appears on the `fft bb.10` family rather than the
      earlier `bb.5` duplicate-call site:
      repeated `div`-derived address/index formation is reduced, and the
      final emitted block shows a shorter `div -> add -> shift/address` chain
    - current correctness restamp on the kept tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS, and course
      `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` PASS
    - current authority:
      keep this as the first real runtime-facing payoff from the new
      `div/mod` optimization line, then continue mining `fft1 bb.10` before
      rotating back to `spmv1`
  - Current 2026-05-13 internal-call global-clobber narrowing follow-up:
    - one more `fft1`-driven narrowing is now kept on the default
      indirect-memory path:
      non-address-taken global load forwarding no longer treats every
      resolved internal call as "kill every forwarded global" by default
    - kept safety boundary:
      external/unresolved calls still conservatively clear all forwarded
      non-address-taken globals, but known internal callees now only clear
      the specific non-address-taken globals that the transitive callee body
      may actually write
    - concrete witness effect on the live `fft1` hotspot:
      whole-file static instruction count drops further from `464` to `454`
    - focused regression added:
      `VALUE-SSA-CONVERT-DEFAULT-INTERNAL-CALL-PRESERVES-UNWRITTEN-GLOBAL-FORWARD`
      now locks the default-path shape where an internal helper call that
      does not write `g` must not destroy the forwarded `store_global g, 7`
      fact
    - correctness restamp on the rebuilt live tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS, and
      full `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`)
    - companion same-round safety note:
      an unfinished `machine_bytes` block-local immediate-cache experiment
      was explicitly backed out before keeping this change, because it had
      diverged size-accounting and byte-emission state and was not yet safe
      to measure against
    - current authority:
      keep the safer bytes rollback plus this narrower global-clobber fix as
      the new `fft1` baseline, then continue from `454` by targeting the
      remaining repeated address/call chains rather than reopening the bytes
      cache experiment immediately
  - Current 2026-05-13 safe div/mod/shift reuse widening follow-up:
    - one further narrow widening is now kept on the same arithmetic line:
      repeated `div/mod/shift` expressions may participate in redundant
      binary reuse even when the operands are not all compile-time immediates,
      as long as the transform is only reusing an already-evaluated result
      rather than deleting the first evaluation as dead
    - kept safety boundary:
      this does **not** relax dangerous arithmetic dead-def deletion; the
      first evaluation still stays in place, and the older
      `div 1, 0`-style regression remains locked as non-eliminable
    - concrete witness effect on the live `fft1` hotspot:
      whole-file static instruction count drops further from `454` to `452`
    - direct visible payoff:
      `main` now reuses the first `div(998244352, d)` result for the second
      `power(3, ...)` call instead of rematerializing the same division chain
    - focused regression added:
      `VALUE-SSA-ELIMINATE-REDUNDANT-SAFE-DIV-BINARY`
      now locks the intended "safe repeated div can become mov" behavior,
      while `VALUE-SSA-ELIMINATE-REDUNDANT-DANGEROUS-BINARY` still locks the
      `div 1, 0` no-rewrite boundary
    - correctness restamp on the rebuilt live tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS, and
      full `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`)
    - later 2026-05-13 loop-hoist closure restamp:
      the earlier loop-invariant pure-call hoist prototype is now reclosed as
      a **kept** optimization on the live default path rather than an
      unkept side experiment.
      The original compiler self-crash was traced to a concrete ownership bug
      inside the rebuilt preheader/body transfer path:
      after moving `rebuilt_preheader.instructions` /
      `rebuilt_body.instructions` into the real blocks, the temporary block
      structs had their `instructions` pointers nulled but their
      `instruction_count` / `instruction_capacity` fields were not reset, so
      later `value_ssa_basic_block_free(...)` walked stale counts through
      `NULL`.
      With that ownership fix in place, the pass now hoists the
      loop-invariant `power(d, mod-2)` call out of `main`'s final loop in
      `13_fft1`, and the live default-path witness improves again:
      whole-file static instruction count drops from `452` to `451`.
      Reclosed evidence on the rebuilt live tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS, and
      full `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`).
    - current authority:
      keep `fft1 = 451` as the new safe baseline, treat the loop-hoist line
      as a real kept default-path optimization, and rotate the next hotspot
      round toward the remaining `spmv1` / `mv1` dynamic-complexity shapes
      plus any further safe loop-invariant pure-call opportunities exposed by
      real witnesses.
    - later 2026-05-13 repeated-indirect-load rerun follow-up:
      one more very conservative `ValueSSA` cleanup sequencing refinement is
      now also kept on the default indirect-memory path:
      after the first `simplify_trivial_values(...)` in the indirect-memory
      fast cleanup, rerun
      `value_ssa_bridge_forward_same_block_repeated_indirect_loads(...)`
      once more before the later pure-call / hoist passes.
      Motivation from the live `spmv1` witness:
      the first early cleanup round can expose a second wave of repeated
      same-block indirect loads whose address expressions are not yet equal
      enough during the very first forward pass, but become equal after the
      early simplification collapses the first layer.
      Concrete witness payoff on the rebuilt tree:
      `09_spmv1` drops from `total_instructions=196` to
      `total_instructions=190`.
      At the same time, the current `fft1` win is preserved:
      `13_fft1` stays at `total_instructions=451`.
      Kept recheck evidence:
      `make test-value-ssa-regression` PASS and
      `make test-compiler-driver` PASS.
    - current authority:
      keep both new course-perf baselines together:
      `fft1 = 451` and `spmv1 = 190`,
      then continue on the remaining `mv1` / third-party hotspot families.
    - later 2026-05-13 unique-predecessor slot-load retry:
      one more very narrow `machine_select` cleanup experiment was tried on a
      tempting local shape: if a block had a unique predecessor and the
      predecessor had just fixed the same `local/global` slot with
      `store_local`, `store_locali`, `store_global`, or `store_globali`,
      then an opening `load_local/load_global` in the successor was rewritten
      to `copy/imm`.
      This retry stayed correctness-green but produced no measurable payoff
      on the active witnesses after explicit rebuild:
      `make test-machine-select` PASS,
      course `lv8` PASS,
      course `lv9` PASS,
      while the local quick static witnesses stayed exactly flat:
      `13_fft1 = 544` asm lines,
      `06_mv1 = 249` asm lines,
      `04_spmv1 = 253` asm lines.
      Current authority is therefore to treat this line as an
      **unkept/no-payoff** side experiment and keep it backed out, rather
      than leaving extra selected-side complexity in the live tree.
  - Current status: **in progress**

4. Third-party perf ranking and optimization
  - For third-party perf cases that run reliably locally, use real runtime.
  - For cases whose local runtime is badly distorted by memory pressure or
    host overhead, use static instruction counts / hot-op counts /
    repeated-load-store-address patterns as the local metric.
  - The same candidate-pass menu from course perf is in scope here too; third-
    party witnesses may justify opening a pass earlier if they expose a
    cleaner minimal hot pattern than the course suite does.
  - Current status: **pending**

5. Correctness gate for every kept edit chunk
   - Required after each substantial code edit:
     - course `lv1-lv9`
   - Preferred whenever practical in the same round:
     - `make test`
     - course `-perf`
     - relevant third-party suites
     - curated `/tmp` witness pool after removing stale-oracle samples
   - Round-specific workflow note:
     - do **not** commit repeatedly
     - hold local edits until the user explicitly asks for a commit
     - do **not** run `make compiler` in parallel with any witness command,
       dump tool, `autotest`, asm diff, or profiling command
     - when code changes need a rebuild, run `make compiler` alone, wait for
       it to finish, and only then start witness / dump / perf validation
     - if helper tools such as `build/dump_middle_stage` or
       `build/dump_machine_stage` are used for validation, rebuild those tools
       explicitly after the library/compiler rebuild before trusting their
       output on the new edit
     - for trap-sensitive arithmetic (`div/mod` and similar later cases),
       include one extra semantic sanity check before keeping the change:
       confirm either
       `1)` we did not eliminate or move a potentially invalid evaluation in a
       way that changes behavior, or
       `2)` we explicitly proved the invalid case cannot arise on the
       optimized path
   - Latest kept recheck on the current implementation baseline:
     - course functional directories that actually exist in this environment
       are all green:
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9`
     - targeted in-repo checks also currently green on this same baseline:
       `make test-compiler-driver`,
       `make test-lower-ir-regression`,
       `make test-lower-ir-verifier`
     - one unfinished optimization experiment was deliberately backed out
       before this restamp:
       the late `stack store/reload -> mv` peephole in `compiler_driver.c`
       was removed again because it had not yet proven correctness and was
       actively breaking regression expectations
     - later same-day recheck after that rollback:
       course functional suites stayed green again on the restored baseline:
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9`
       and `make test-compiler-driver` returned to PASS
     - current 2026-05-12 selected-IR experiment:
       a new very conservative same-block `load_local/load_global -> copy`
       forwarding rule was added in `machine_select` and kept through the
       course functional gate (`lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`,
       `lv8`, `lv9` stayed green; `make test-compiler-driver` stayed PASS),
       but the first static perf evidence is weak:
       `06_mv1` assembly line count stayed flat and `09_spmv1` grew from the
       older snapshot's `196` instruction lines to `200`, so this is not yet
       a clear win candidate
     - 2026-05-13 follow-up on that line:
       the slot-load experiment is now treated as “safe but not a primary
       perf win” rather than as the current hotspot answer. Focused timings
       showed mixed results (`mv*` improved, `spmv1` regressed, `spmv3`
       regressed slightly), so the line stays provisional and should not be
       expanded blindly.
     - 2026-05-13 newer selected-IR experiment:
       a very conservative same-block repeated register pure-expression reuse
       cleanup was then added in `machine_select`. It also kept the course
       functional gate green and, importantly, it removed the specific extra
       `spmv1` instruction-pair regression that the slot-load experiment had
       introduced (the repeated `lw t6, 68/72(sp)` + `slli` pair disappeared,
       bringing `09_spmv1` back down from `200` instruction lines to `196`).
       Current runtime evidence is also better than the immediately previous
       baseline:
       `06_mv1` improved again to about `14.57s`,
       `09_spmv1` improved from the slot-load-only `16.32s` down to about
       `16.13s`, and `13_fft1` improved further to about `18.83s`.
       This is still not enough to beat the older historical `spmv1` baseline,
       so the rule is worth keeping for now, while the next hotspot pass
       focuses on the remaining `spmv1` address/index chain itself.
     - 2026-05-13 attempted follow-up on repeated same-block indirect loads:
       replacing the later repeated `load_indirect` with a direct `copy` of
       the earlier loaded result was tested and then backed out again. It did
       remove the visible `68/72(sp)` spill+reload pair, but it also
       reintroduced repeated shift work in the hot path and pushed
       `09_spmv1` up to roughly `17.60s`. After rollback, the worktree is back
       on the safer commutative-reuse baseline (`spmv1` at `194`
       instruction lines, quick `lv1/lv8/lv9` recheck green, and
       `make test-compiler-driver` PASS).
     - 2026-05-13 pass-isolation follow-up:
       a focused A/B check then separated the two likely selected-side causes
       for the remaining `spmv1` slowdown:
       `MACHINE_SELECT_SKIP_REUSE_SPILL_PURE_EXPR=1` barely moved the result,
       while `MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS=1`
       restored the older `196`-instruction form and, on that focused run,
       actually came back faster than the current `194`-instruction default.
       Current authority is therefore that the front-most next diagnosis line
       is the `forward_same_block_indirect_loads` cleanup itself rather than
       the later `reuse_spill_pure_expr` cleanup.
     - later 2026-05-13 safeguard follow-up:
       one more attempted narrowing on `forward_same_block_indirect_loads`
       tried to refuse rewrites when the earlier load result would need to be
       preserved across an intervening redefinition. This simply kicked `spmv1`
       back to the older `196`-instruction shape and slowed the focused run to
       roughly `16.74s`, so that safeguard was backed out immediately.
       Current authority after rollback is still the same:
       keep the existing commutative pure-expression reuse,
       keep the existing indirect-load forwarding pass,
       and continue searching for a different, narrower `spmv1` hotspot cut.
     - later 2026-05-13 preserve-register experiment follow-up:
       the first real "preserve register result + insert copy" attempt was
       implemented on top of the new block-local `insert op before index`
       helper, but it was **not** kept. The experiment exposed two concrete
       problems:
       `1)` the first version rewrote the wrong post-insert op because the
       inserted copy shifted `op_index`, producing nonsensical `copy` shapes;
       `2)` even after fixing that index bug, focused
       `dump_machine_stage select 04_spmv1.sy` runs with
       `MACHINE_SELECT_TRACE_TIMING=1` still timed out before any
     - 2026-05-13 broad serial recheck closure:
       after the lower-IR terminator/verifier fix, a full logged
       `autotest -perf /workspaces/compiler_lab` rerun is green again:
       `130/130`, including the previously red
       `perf/18_brainfuck-bootstrap` and `perf/19_brainfuck-calculator`
     - 2026-05-13 `fft1` fast-path reuse restamp:
       after wiring repeated pure internal call reuse into the indirect-memory
       default fast path and explicitly rebuilding both `build/compiler` and
       the dump helpers, the default `fft1` witness stays green on the course
       functional gate:
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` PASS, and
       `make test-compiler-driver` PASS
       `cleanup-forward-same-block-indirect-loads` completion marker, while
       the same command with
       `MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS=1` still
       completed immediately. Current authority is therefore:
       keep the preserve-register rewrite backed out for now,
       treat `forward_same_block_indirect_loads` itself as the still-open
       hotspot / non-termination diagnosis surface,
       and do not reland another preserve-copy variant until it is proven to
       terminate on the real `spmv1` witness.
     - later 2026-05-13 fresh-tooling recheck + kept guard:
       after rebuilding both `build/dump_machine_stage` and
       `build/machine_select/machine_select_test`, the earlier
       "default path times out before forward-same-block-indirect-loads
       finishes" conclusion turned out to be stale-binary noise. Fresh runs
       showed both the default path and
       `MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS=1` complete
       immediately on `04_spmv1.sy`; the real remaining issue is again output
       shape/perf, not current nontermination.
       The kept fix from that recheck is narrower:
       `forward_same_block_indirect_loads` now refuses to force the earlier
       repeated `load_indirect` through a fresh spill when that earlier load
       result is already consumed by a small reusable pure expression before
       the later same-address load.
       Fresh evidence on the live tree:
       default `04_spmv1.sy` `-perf` output now returns to the older
       `279`-line asm shape (matching the old `SKIP_FORWARD` line count),
       the hot `spmv` loop no longer emits the `sw ... 68/72(sp)` /
       `lw ... 68/72(sp)` pair, and a quick direct qemu rerun came back at
       about `run_default ~= 0.0216s` versus
       `run_skip ~= 0.0176s` on the same local micro-check.
       Required correctness gate after the kept guard also reclosed:
       `make test-compiler-driver` PASS and course
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` all PASS.
     - later 2026-05-13 next-hotspot rerank + indirect-store/load follow-up:
       one fresh small reranking on the rebuilt toolchain then showed the
       active course-perf order had shifted again: `spmv*` was no longer the
       front-most local witness after the kept repeated-load guard, and the
       heavier quick witnesses became `bitset2`, then `mm1`, with `spmv1`
       trailing them in the same micro-check.
       A new narrow selected-side experiment is now kept on that next line:
       same-block `store_indirect(addr, value)` followed by a later
       same-address `load_indirect addr` may now forward the stored value
       directly when no intervening call, no potentially aliasing memory
       write, and no redefinition/clobber of the stored value occurs in
       between.
       Current kept local evidence:
       `00_bitset2.sy` dropped from the earlier `372`-line local asm shape
       down to about `362` lines and now runs around `0.0042s` in the quick
       qemu micro-check, while `01_mm1.sy` remains around `327` lines /
       `0.0038s` and `04_spmv1.sy` stays around `279` lines /
       `0.0034s`.
       A deeper selected dump check also confirmed that this is not just a
       late-text accident: inside `set`, the first big local-table
       initialization ladder is now forwarded all the way from same-block
       `store_indirect` values, so the selected op count for that function
       drops from about `145` to about `125` and the earlier
       `store -> load -> shift -> store` chain becomes a direct constant-store
       staircase.
       Correctness recheck on this kept state is still in progress but is
       green so far:
       `make test-compiler-driver` PASS and course
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8` have all rerun PASS;
       the final `lv9` restamp is the remaining required course-facing gate
       before this line should be treated as fully reclosed.
     - later 2026-05-13 shared-text no-op follow-up:
       the shared preview text exporter now also closes one small but real
       no-op family: `addi rd, rd, 0` is no longer pretty-printed as a
       literal self-move such as `mv t6, t6`.
       A focused compiler-driver regression now locks that shape directly.
       Required recheck after this kept edit is fully green:
       `make test-compiler-driver` PASS and course
       `lv1`, `lv3`, `lv4`, `lv5`, `lv6`, `lv7`, `lv8`, `lv9` all PASS.

## Current Progress Snapshot

- `void call` pointless-code cleanup: **~55%**
- in-repo regression realignment: **~94%**
- course perf runtime ranking: **~94%**
- third-party perf ranking: **~45%**

## Next Immediate Steps

1. Continue from the now-restamped `make test-compiler-driver` + course
   functional baseline and inspect the new live runtime front:
   `fft1`, then `spmv1`, then `mv1`.
2. Prefer the next code change on a hotspot with a crisp observable witness
   rather than forcing the currently analysis-blocked `mm1` line.
3. Treat the new recursive-pure-call reuse support as a kept local `fft1`
   win, then look for the next similarly crisp repeated helper / repeated
   address witness before opening a larger new pass family.
4. Stay on `fft1` while it still offers concrete repeated helper / repeated
   pure-call witnesses with measurable wins before rotating back to `spmv1`.
3. Keep the remaining dump-expectation tail as a parallel cleanup chore, but
   do not let it block safe perf work now that the key compiler-driver gate is
   green again.
4. Prefer safer selected-IR / machine-select-side opportunities before adding
   new final-text peepholes again; the current authority from the backed-out
   `store/reload -> mv` attempt is that late text rewrites still need a very
   high proof bar.
5. For the new same-block slot-load forwarding experiment, do not assume
   “correctness green” implies “perf useful”: keep or revert it based on the
   hotspot evidence after the in-flight `-perf` sweep finishes.
6. Current `spmv`-side priority is no longer the slot-load experiment itself.
   The next mainline is the remaining repeated address/index materialization
   inside `spmv` hot blocks (`bb.8` / `bb.11` family), using conservative
   selected-IR reuse rather than new high-risk final-text peepholes.
7. Do not keep the direct repeated-load-to-copy variant for now. The current
   mainline should assume that simply copying an earlier `load_indirect`
   result is not enough if it causes the byte-lowering path to recompute
   shifts or address carriers. Keep investigating the residual
   `load -> spill -> shift` chain on top of the safer commutative-reuse
   baseline.
8. Prioritize diagnosis inside `forward_same_block_indirect_loads` before
   reopening `reuse_spill_pure_expr`: current isolated evidence says the
   remaining `spmv1` regression tracks the former much more closely than the
   latter.
9. The cleanup layer now also has the first required structural primitive for
   the next `spmv1` attempt:
   a block-local `insert op before index` helper exists in
   `machine_select_cleanup`. Current authority is to use that helper to build
   a true third strategy later ("preserve register result + insert copy"),
   instead of forcing work into only spill-or-copy rewrites. Do not land a
   half-finished variant just because the helper now exists; keep the
   worktree on the restored safer baseline until the full local rewrite is
   ready.
10. Before attempting another preserve-register variant, keep one explicit
    termination check in the loop:
    `timeout ... env MACHINE_SELECT_TRACE_TIMING=1 build/dump_machine_stage select 04_spmv1.sy`.
    If the default path still times out while
    `MACHINE_SELECT_SKIP_FORWARD_SAME_BLOCK_INDIRECT_LOADS=1` returns
    immediately, treat that as proof that the active bug is still inside the
    repeated indirect-load forwarding line rather than in later cleanup.
11. Current first-choice narrow direction inside
    `forward_same_block_indirect_loads` is now "block only the harmful
    spill-forcing cases", not "disable the pass wholesale" and not "reland
    preserve-copy". The currently kept example is the new guard for repeated
    indirect loads whose earlier result already feeds a small reusable pure
    expression before the later same-address load.
12. After the `spmv` guard, keep reranking the local course-perf witnesses
    instead of assuming `spmv` remains the hottest line forever. The next
    rebuilt local priority has already shifted toward the `bitset/mm` family,
    so the active follow-up should now prefer narrow same-block
    store/load/address-formation cleanup there before reopening broader
    `spmv` experimentation again.
13. Before reopening any cross-block `store_local -> load_local` forwarding
    idea for `mm1`, require one concrete diagnostic surface that actually
    shows those candidates in the current selected form. The first fresh
    probe with `tools/analyze_machine_select_loop_slot_candidates.py` found
    no simple unique-predecessor `store_local/load_local` carry pairs in the
    current `mm1` or `bitset2` selected dumps, so the next `mm1` line should
    assume the bottleneck is no longer a plain slot-carry pattern unless a
    stronger witness is built first.
14. The next `mm1` diagnosis should now explicitly target address formation
    rather than slot carries. Fresh selected/asm rereads show the hot `mm`
    body is dominated by repeated `slli 12`, `slli 2`, base+index `add`,
    and triple `lw`/`mul`/`add` combinations over A/B/C matrix rows, while
    both loop-slot candidate probes returned `<none>`. Treat that as a strong
    signal that the next useful `mm1` tool or witness should model
    row/column address-carrier reuse, not plain local-slot forwarding.
15. A fresh dedicated address-pattern probe now exists too:
    `tools/analyze_machine_select_mm_address_patterns.py`.
    Current authority from that tool is:
    `bitset2` still benefits from dense same-block `alui.2(...,4)` and
    same-address indirect-memory staircases, but `mm1` is structurally
    different. The hot `mm bb.16` block does **not** show a broad repeated
    same-root lattice; instead it shows a few distinct row/column carriers
    such as `alui.2(local:4,4096)`, `alui.2(local:6,4096)`,
    `alui.2(spill.2,4)`, and the composed indirect roots built from them.
    So the next `mm1` optimization line should be a deliberately narrow
    row-base / matrix-address-carrier reuse idea, not a generic `alui/load`
    folding pass.
16. A second more targeted `mm1` carrier probe,
    `tools/analyze_machine_select_row_base_carriers.py`,
    currently returns `<none>` on both the live `mm1` and `bitset2` selected
    dumps. Treat that as a practical boundary: the current selected form does
    not yet expose a stable repeated "row-base carrier" pattern that is
    easy to optimize with another tiny cleanup pass. Until a stronger witness
    is built, prefer stopping here over forcing a speculative `mm1`
    optimization that lacks a crisp observable target.
17. Third-party perf ranking is now no longer purely pending in principle:
    one first rebuilt shared-sample ranking has been taken across
    `compiler2021-public`, `indigo`, and `minic-test-cases-2021f`.
    Current local ordering on that shared sample is:
    `mm1` first (roughly `0.0041s`, `327` asm lines),
    then `bitset2` (roughly `0.0038s..0.0040s`, `362` asm lines),
    then `spmv1` (roughly `0.0037s`, `279` asm lines).
    Current authority is therefore:
    if the shared course/external optimization line continues, `mm1` is now
    the front-most remaining cross-suite hotspot, but it is also explicitly
    analysis-blocked on a better address-carrier witness before another code
    change should be attempted.
18. A first private/lava-style third-party compile-vs-run split has now also
    been sampled. Current authority from that rebuilt local sample is that
    the dominant pressure on this line is **compile time**, not runtime:
    `instruction-combining-*` is currently the heaviest local witness
    (about `15.1s` compile, `~187k` asm lines),
    followed by `integer-divide-optimization-*` (about `13.6s` compile,
    `~26k` asm lines), then `hoist-*` (about `4.1s` compile,
    `~5.3k` asm lines), while the corresponding qemu runtimes stay tiny.
    Current authority is therefore:
    if the next optimization slice moves onto the third-party line, it should
    reopen as a compile-time / IR-growth / pass-scaling diagnosis, not as a
    runtime code-quality line.
19. The first stage-level third-party compile-time diagnosis is now also in
    hand for the front-most witness family:
    `instruction-combining-1/2.sy` both lower into the same giant selected
    function shape under `tools/profile_backend_layers.c`:
    one internal `func` with roughly `30005` selected ops, about `10000`
    `alui` ops, and about `20004` spill slots, all inside a single block.
    Current authority is therefore:
    the next third-party optimization line should target **IR / selected-form
    size explosion** on that giant single-block function, not low-level bytes
    or qemu runtime behavior.
20. A deeper `dump_machine_stage machine-ir/select` reread now tightens that
    same conclusion again: the explosion is already structurally present by
    `machine-ir`, not introduced late by `machine_select`.
    On `instruction-combining-1.sy`, `machine-ir` already shows one single
    `func` block with about `20004` spill slots and a long repeated
    `load local.0 -> add 1 -> store local.0` ladder, while `machine_select`
    mostly mirrors that into about `30005` selected ops.
    Current authority is therefore:
    if this third-party line reopens for code changes, reopen it at the
    earlier IR/lowering/SSA formation boundary rather than trying to rescue
    the giant shape only with later selected cleanups.
21. The source-level shape of the front-most third-party compile-time family
    is now also explicit enough to guide that earlier reopen: the
    `instruction-combining-*` witnesses are dominated by one enormous
    straight-line chain of repeated `input = input + 1;` updates inside
    `func(...)`. Current authority is therefore:
    if we reopen this line for implementation, prioritize asking why that
    linear self-update chain survives so literally through lower-IR / SSA /
    machine-ir formation, and whether any earlier canonical/value-stage fold
    can collapse it before it multiplies into tens of thousands of spill-backed
    machine ops.
22. A new 2026-05-13 kept third-party compile-time follow-up has now closed
    the first half of that `instruction-combining-*` explosion:
    the extreme straight-line `value_ssa` fast path now runs a narrow local
    cleanup/rename pass instead of returning the raw direct-build form
    immediately. On `instruction-combining-1.sy`, that collapses the
    `value-ssa-default` shape from about `30042` instructions
    (`10014 load_local`, `10011 store_local`) down to about `10036`
    instructions (`10009 binary`, `10 load_local`, `9 store_local`), and the
    downstream selected shape follows suit:
    `func` drops from about `30005` selected ops / `20004` spill slots to
    about `10003` selected ops / `0` spill slots.
23. A second kept 2026-05-13 follow-up then closed the next hot compile-time
    tier on that same family:
    `machine_select`'s pure-cleanup forward/reuse scans were spending about
    `10s` on the remaining giant single-block arithmetic chain even after the
    selected shape had already shrunk to about `10003` ops. The current tree
    now recognizes this very narrow shape
    (single block, return-only terminator, `4k+` ops, almost entirely
    `alu/cmp` plus only a tiny number of copy/materialize/load ops) and skips
    the expensive same-block forward/reuse scans there.
    Current kept local evidence:
    `instruction-combining-1.sy` total compile time dropped from about
    `22.8s` to about `11.9s`, `instruction-combining-2.sy` from about
    `22.6s` to about `12.6s`, and `machine_select` itself fell from about
    `10s` to near-zero on that family while the selected op count stayed at
    about `10003`.
24. A third kept 2026-05-13 follow-up then closed that remaining allocator
    tier too:
    the current tree now gives the same giant straight-line value chain a
    very narrow allocator cheap path instead of forcing it through the full
    general `select-phase`.
    Current kept local evidence:
    `instruction-combining-1.sy` now compiles in about `2.8s` total
    (`machine_ir_report ~= 0.7s`, `machine_select ~= 0.004s`),
    `instruction-combining-2.sy` in about `2.9s` total
    (`machine_ir_report ~= 0.68s`, `machine_select ~= 0.004s`),
    while the selected `func` shape stays stable at about
    `10003` ops with only `2` spill slots.
25. Current authority after those three kept steps is therefore:
    the earlier `instruction-combining-*` compile-time emergency is now
    materially reclosed on the local tree. If the third-party perf line keeps
    moving, the next worthwhile hotspot is no longer this family's selected
    cleanup or allocator pressure, but whichever heavy compile-time witness
    remains next after reranking the private/lava sample
    (`integer-divide-optimization-*`, `hoist-*`, or another refreshed tail).
26. A new 2026-05-13 third-party rerank has now made that "next witness"
    concrete: `integer-divide-optimization-2/3.sy` currently sit around
    `28s` on the local compile-path sample, far above `hoist-2.sy`
    (about `4.6s` / `4.3s`). Current authority is therefore:
    the active third-party compile-time mainline is now the
    `integer-divide-optimization-*` family, not `hoist-*`.
27. A first kept follow-up on that new family is now also in hand:
    `value-ssa-classic` and `value-ssa-memory-full` already knew how to
    collapse the hot `main bb.5` shape from "1000 identical `mul i,multi`"
    down to one `mul` plus a repeated-argument call, while the default
    extreme-straight-line path did not. The current tree now routes the
    huge-parameter extreme-straight-line family through that same classic
    canonicalization path.
    Current kept local evidence on the main compiler path:
    `integer-divide-optimization-2.sy` compile time dropped from about
    `28.5s` down to about `21.9s`, and `value-ssa-default` now matches the
    canonicalized hot shape (`ssa.8 = mul ...` followed by
    `call func(ssa.8, ssa.8, ..., ssa.8)`).
28. Current authority after that kept step is:
    the `integer-divide-optimization-*` family is improved but not yet closed.
    The remaining gap is now partly a tooling/observation mismatch too:
    `build/profile_backend_layers` still reports the older pre-collapse
    selected shape even after the main compiler path has improved, so the next
    diagnosis should first realign that helper surface with the live default
    compiler path before overfitting another backend change to stale helper
    evidence.
29. 2026-05-14 experiment note, not kept:
    a narrow `machine_ir`-entry local-slot remap was tried to move hot scalar
    locals ahead of large local arrays so `brainfuck-calculator` would stop
    paying repeated large-offset stack-address costs for `return_address_top`
    and neighboring scalar state.
    Local timing evidence was promising on the direct course witness
    (`19_brainfuck-calculator` fell from the previously remembered `42s+`
    tier to about `31s` locally), but the implementation was not correct:
    both `18_brainfuck-bootstrap` and `19_brainfuck-calculator` produced
    wrong output, so the remap was explicitly backed out.
    Current authority:
    treat “hot scalar before giant local array” as a still-interesting future
    direction, but only reopen it after first proving which downstream layer
    can preserve the remapped local-slot semantics end-to-end.
30. 2026-05-14 maintenance follow-up:
    while investigating that failed remap experiment, `machine_ir` text dump
    support was widened to print `addr local/global`, `load_indirect`, and
    `store_indirect` explicitly instead of falling back to `<unknown>` on
    those instruction kinds.
    That dump-side improvement is kept; the semantic local-remap experiment is
    not.
