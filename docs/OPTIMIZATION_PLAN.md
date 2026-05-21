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
- For the broader reusable-pass backlog beyond the current witness-driven
  optimization round, see `docs/GENERAL_OPT_PASSES_TODO.md`.

## Current Plan

- 2026-05-20 correctness checkpoint:
  the latest third-party rerun uncovered one default `-riscv` final-text
  peephole regression on the `array_concat` cluster. That specific crash is
  now closed without disabling the whole peephole lane. Continue correctness
  work from the remaining stable rerun failure clusters rather than
  re-opening this already-fixed witness.
- 2026-05-20 later correctness checkpoint:
  the same rerun also exposed a wider `sort_test*` family tied to
  over-aggressive loop-hoist work inside the active perf-hotspot line.
  Current kept safety boundary is now narrower:
  header-side loop hoist is still allowed,
  body-side loop hoist is not currently trusted on the live tree.
- 2026-05-20 later same-day default-path checkpoint:
  the normal default `-riscv` line also had one remaining stale loop-hoist
  proof on `lv9/21_sort7`. Current kept closure is now stronger than the
  earlier "just stop body-side perf hoist" checkpoint:
  the general LICM helper and the default bridge-side simple-loop
  load/pure-call hoist helpers now reason over the full natural loop, not
  only `header + backedge body`. That same chunk also reconnected the
  already-written bridge-side simple loop-invariant load hoist into the
  default indirect-memory direct cleanup path.
- 2026-05-20 later same-day tiny-inline checkpoint:
  the current tiny internal-helper inliner is now widened one more step on
  the live mainline: small helper bodies with `store_global` or
  `store_indirect` can now inline too, not only pure load/address/binary
  helpers. Keep the scope narrow for now: this is still the existing
  one-block / two-block return-tail family with the same body-size and
  callsite budget gates, and the next work should broaden pass coverage only
  after a real perf witness asks for it.
- 2026-05-21 later default-path checkpoint:
  the default indirect-memory direct cleanup path now also runs the existing
  dominated repeated-indirect-load forwarding helper, not only the older
  same-block and single-predecessor-edge slices. This is a reusable
  correctness-closed pass-restore checkpoint, not a witness-specific rewrite.
- 2026-05-21 later machine-select checkpoint:
  one previously disabled selected-cleanup helper is now restored in a narrow
  correctness-closed form:
  adjacent same-block `cmp/cmpi` plus `br` fusion is live again inside the
  selected pure-cleanup fixed-point, while the broader visible-compare sibling
  remains intentionally disabled until it has its own proof and regression
  coverage.
- 2026-05-21 later tiny-inline checkpoint:
  the public `ValueSSA` tiny-helper inliner now also closes one remaining
  pass-completeness gap:
  nested/helper-order-sensitive opportunities are no longer limited by a
  single forward scan over the function table. The pass now runs to a small
  fixed-point while preserving its existing per-function inline budget across
  rounds, so newly simplified tiny helpers may unlock later callers in the
  same invocation without silently over-expanding code size.

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
  - Current 2026-05-18 reusable-pass follow-up:
    - the repository is no longer at the "only discuss generic passes" stage
      on the induction line: a first narrow reusable `ValueSSA`
      induction-strength-reduction slice is now landed locally and locked by a
      focused regression witness
    - current scope of that first slice:
      simple zero-seed single-step induction plus derived `shl` recurrence
    - current limitation:
      rebuilt `value-ssa-perf` rereads show that this first cut has not yet
      hit the real `mm1` / `spmv1` address trees, so it should be treated as
      an opening foothold rather than as the completed general answer
    - current authority:
      continue broadening this reusable induction line toward affine
      `shl/mul + add` address-carrier recurrence before spending more time on
      another fresh generic-pass family
  - later same-day reusable-pass follow-up:
    - the current kept reusable induction line has now advanced past the
      pure-immediate seed case:
      preheader-provided seed values are now accepted too, and a simple
      use-count gate is in place so we only rewrite `shl` values that look
      structurally worth carrying
    - rebuilt `value-ssa-perf` rereads now confirm real witness reach:
      `03_mm1` still keeps the inner `+4` carrier, and `09_spmv1`
      now also carries `+4` recurrences inside the main update loops rather
      than only the tiny regression witness
    - focused A/B after that gate tightening currently reads:
      `03_mm1` run ms `7727.860 -> 7152.769`,
      `09_spmv1` `12484.632 -> 12418.484`,
      `10_spmv2` `7343.576 -> 7429.339`,
      `11_spmv3` `9670.617 -> 9777.935`,
      `13_fft1` `7854.056 -> 7833.304`
    - current authority:
      this reusable induction line is now materially useful on `mm1` and no
      longer obviously harmful on `spmv1`, but it still needs one more
      narrowing round before it becomes a safe keep/commit candidate for the
      full `spmv1/2/3` cluster
  - later same-day gate-tightening follow-up:
    - the reusable induction line now also has one more explicit guard:
      the current `shl` recurrence rewrite requires at least three uses of the
      scaled value, not merely two
    - the focused regression witness was restamped to match that stronger
      policy
    - focused A/B after that tighter gate currently remains:
      `03_mm1` positive,
      `13_fft1` slightly positive,
      `09_spmv1` roughly flat/slightly positive,
      but `10_spmv2` / `11_spmv3` still slightly negative
    - current authority:
      do not broaden the pass again yet; the next refinement should narrow by
      structural/use-role evidence so the line keeps the `mm1` gain without
      continuing to drag `spmv2/3`
  - later same-day focused A/B restamp:
    - after the first role-aware/address-chain gate, the current focused A/B
      still reads:
      `03_mm1` strongly positive,
      `09_spmv1` roughly flat/slightly positive,
      `10_spmv2` / `11_spmv3` still slightly negative,
      `13_fft1` effectively flat
    - current authority:
      stop spending turns on another raw numeric threshold retune.
      The next reusable induction refinement should use a stronger structural
      distinction between worthwhile `mm1`-style address carriers and the
      still-problematic `spmv2/3` candidates
  - later same-day structural-gate restamp:
    - the stronger structure/use-role gate is now landed, and rebuilt
      `value-ssa-perf` rereads show that `spmv1/2/3` light candidates have
      been pushed back to baseline-style shape while the useful `mm1` inner
      `+4` carrier still remains
    - focused A/B after that restamp currently reads:
      `03_mm1` strongly positive,
      `09_spmv1` slightly positive,
      `10_spmv2` roughly flat/slightly negative,
      `11_spmv3` slightly positive,
      and `13_fft1` slightly negative
    - current authority:
      this reusable induction line is now much closer to checkpointable
      territory. The next step should be a confirmation rerun before commit,
      with special attention to compile-time cost and whether the slight
      `fft1` regression is just noise
  - later same-day confirmation rerun:
    - the confirmation rerun over
      `03_mm1`, `09_spmv1`, `10_spmv2`, `11_spmv3`, `13_fft1`, and `14_fft2`
      still shows the broad intended tradeoff:
      `03_mm1` remains strongly positive,
      `11_spmv3` and `14_fft2` are slightly positive,
      `10_spmv2` is only slightly negative,
      while `09_spmv1` and `13_fft1` drift slightly negative/flat
    - rebuilt `value-ssa-perf` rereads now also show `09/10/11_spmv*`
      back at a baseline-style shape, which suggests the remaining runtime
      drift there is more likely noise or a second-order backend/codegen
      effect than a large remaining mid-IR overfire
    - current authority:
      this reusable induction line is now close enough to a keep/backout
      checkpoint decision that the next step should be judgment-based rather
      than another big transform expansion
  - later same-day wider confirmation:
    - a wider guard sample over
      `03_mm1`, `09_spmv1`, `13_fft1`,
      `18_brainfuck-bootstrap`, and `19_brainfuck-calculator`,
      plus a reversed-order confirmation over
      `14_fft2`, `13_fft1`, `11_spmv3`, `10_spmv2`, `09_spmv1`, and `03_mm1`,
      now sharpens the decision:
      `mm1` stays strongly positive, but the line is still not stable enough
      across `spmv1` and `fft1/2`, and compile time is consistently higher
    - current authority:
      do **not** checkpoint the reusable induction line in its current form.
      If this line continues immediately, it should continue as a refinement
      branch; otherwise rotate to a different optimization family instead of
      pretending this version is already a stable base
  - 2026-05-19 reusable-pass pivot:
    - the earlier tiny-helper inlining branch was backed out again because it
      kept tangling with existing call/text backend contracts
    - reusable-pass work has now pivoted to a fresh `ValueSSA` SCCP foothold
    - current SCCP slice scope:
  - 2026-05-20 later perf-restoration checkpoint:
    - one already-implemented reusable induction slice is now safely back on
      the live `-perf` path:
      `value_ssa_perf_strength_reduce_simple_induction_shifts(...)`
    - repository-local status:
      `make test-value-ssa-regression` is green after restamping the focused
      induction-shift witness to the new carried-phi dump
    - targeted rebuilt-compiler external `-perf` status:
      `sort_test7`, `many_locals2`, `matrix_rank_1`, and `matrix_rank_2`
      stayed green, while the known `82_long_func` compile-time tail remains
      a `COMPILE_TIMEOUT`
    - current authority:
      keep this restoration and continue from the compile-time tail rather
      than treating this pass as the current blocker
  - 2026-05-20 later compile-time diagnosis checkpoint:
    - refreshed stage profiling on `many_parameters10000.c` now reads about
      `value_ssa 11.091s`, `value_ssa_perf_hotspots 0.006s`,
      `machine_ir_report 3.562s`, `machine_select 6.622s`,
      `full_riscv 23.093s`, and `full_perf 62.490s`
    - an extra middle-stage timing split sharpens that diagnosis:
      `dump_middle_stage value-ssa-default` takes about `10.25s`, while
      `dump_middle_stage value-ssa-perf` takes about `50.96s`
    - current authority:
      the next `many_parameters10000` compile-time step should focus first on
      splitting/containing the `value-ssa-perf` path, especially the
      MemorySSA scalar-replacement line, before spending another round on
      machine text-export guesses
  - 2026-05-20 later huge-parameter containment checkpoint:
    - step-level MemorySSA timing now explicitly identifies the local-slot
      scalar-replacement line as the dominant `many_parameters10000` perf-path
      compile-time sink:
      `promote-local-slots ~= 5.92s`,
      `forward-local-loads ~= 27.32s`,
      versus only `~1.1s`-scale global-slot steps
    - the current live `-perf` driver therefore now skips only
      `memory_ssa_pass_scalar_replace_local_slots(...)` for the already-known
      huge-parameter hotspot family (`parameter_count > 512`), while keeping
      global-slot scalar replacement plus the later perf-hotspot ValueSSA
      passes enabled
    - immediate rebuilt-compiler result:
      `many_parameters10000.c -perf` compile time dropped from roughly `62s`
      to roughly `28.37s`
    - targeted smoke rechecks stayed green on the sensitive correctness
      witnesses (`sort_test7`, `many_locals2`, `matrix_rank_1`, `matrix_rank_2`)
    - current authority:
      keep this targeted huge-parameter containment path and continue from the
      remaining compile-time and pass-restoration tails, instead of reopening
      the now-reduced `many_parameters10000` line blindly
  - 2026-05-20 later local-MemorySSA containment follow-up:
    - `82_long_func` profiling now confirms that the earlier driver-only skip
      was not enough because higher MemorySSA consumers still re-entered the
      same local-heavy path
    - the dominant local-only timings on that witness now read roughly:
      first heavy local round:
      `promote-local-slots ~= 110.44s`,
      `forward-local-loads ~= 104.90s`,
      `eliminate-redundant-local-stores ~= 112.99s`,
      `eliminate-dead-local-stores ~= 112.11s`
      and one later still-heavy local round:
      `promote-local-slots ~= 72.96s`,
      `forward-local-loads ~= 70.91s`,
      `eliminate-redundant-local-stores ~= 82.26s`,
      `eliminate-dead-local-stores ~= 82.48s`
    - current containment now therefore moved inward too:
      `memory_ssa_pass_scalar_replace_local_slots(...)` itself short-circuits
      on the same huge-parameter / extreme-straight-line hotspot family, so
      higher MemorySSA canonicalizers no longer re-enter the local-heavy
      fixed-point path on those cases
    - current authority:
      the next compile-time step should keep narrowing this local-only
      MemorySSA family rather than reopening unrelated perf passes first
      integer constant lattice, executable-edge discovery, simple phi merge,
      and constant use-site rewrite followed by existing cleanup
    - current status:
      the new SCCP slice is green on `test-value-ssa-regression` and
      `test-compiler-driver`
    - clarified boundary:
      keep SCCP conservative on globals for now; do not fold from global
      initializer metadata unless a stronger program-wide immutability proof
      is added later
  - 2026-05-20 LICM follow-up:
    - the current `ValueSSA` LICM slice now also hoists loop-invariant
      `load_indirect` values when the address root is itself loop invariant
      and the loop body/header do not contain an aliasing barrier for that
      address root
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` are green after
      the extension
  - 2026-05-20 pure-value CSE follow-up:
    - the dominator-scoped redundant-binary family now also reuses repeated
      `mov` pure values, not only repeated `binary` / `addr_*` / readonly
      `load_global` forms
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` are green after
      the extension
  - 2026-05-20 LICM witness follow-up:
    - added a direct positive and a direct alias-barrier negative witness for
      loop-invariant `load_indirect`
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 CSE follow-up:
    - the pure-value CSE family kept its current `mov` reuse shape and no
      larger cross-block expansion was promoted in this chunk
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
    - trial note:
      a `load_indirect` extension was explored and backed out; keep the
      current family boundary and move `load_indirect` into the future
      dedicated cross-block GVN/CSE design instead
  - 2026-05-20 SCCP follow-up:
    - a local-slot constant-environment retry was explored for
      `store_local -> load_local -> branch` style cases
    - current authority:
      not kept; return to the simpler SCCP slice and treat any stronger
      local-slot reasoning as a future dedicated follow-up rather than as an
      underbaked expansion of the current pass
  - 2026-05-20 pure-value CSE follow-up:
    - a dedicated redundant-indirect-load pass is now landed in
      `value_ssa_pass`, covering same-block repeated `load_indirect` reuse
      with alias/call barriers
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 cross-block GVN/CSE follow-up:
    - the old dominator-scoped `redundant_binaries` cleanup now has a first
      real cross-block available-value / GVN-style slice instead of only
      per-dominator-scope reuse
    - current kept scope:
      pure scalar values only:
      `mov`, safe pure `binary`, `addr_local`, `addr_global`, and readonly
      `load_global`
    - current join behavior:
      if all reachable predecessors carry the same available pure-value key
      but through different predecessor result ids, the pass now inserts a
      join phi for that key and rewrites repeated successor computations to
      reuse that phi
    - current boundary:
      do not merge `load_indirect` into this framework yet; keep indirect
      memory reuse on the separate same-block barrier-aware pass until the
      alias proof story is strong enough for cross-block memory values
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 later cross-block GVN/CSE follow-up:
    - the same available-value / GVN slice now also absorbs `load_indirect`
      reuse instead of leaving it only on the older same-block sibling pass
    - current kept extension:
      `load_indirect` joins the same cross-block available-value framework,
      including join-time phi reuse when the loaded address is available on
      every reachable predecessor
    - current safety closure:
      the framework now carries a first unified kill rule for available
      indirect-load entries, so `call`, aliasing stores, and
      address-dependence-invalidating local/global stores clear those entries
      before later reuse
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 later pipeline-cleanup follow-up:
    - now that unified GVN already covers `load_indirect`, the
      canonicalize/direct `ValueSSA` pipelines no longer run the old dedicated
      `redundant_indirect_loads` sibling pass immediately after GVN
    - current authority:
      keep the standalone entrypoint and focused tests for now, but treat the
      unified GVN path as the pipeline authority
    - same-day SCCP cleanup tightening:
      SCCP's post-rewrite cleanup tail now also hands off into
      `value_ssa_eliminate_redundant_binaries(...)`, so SCCP-fed constant
      rewrite can immediately expose the stronger GVN cleanup before later
      CFG/DCE passes
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 later bridge cleanup follow-up:
    - the bridge-side same-block pure-address reuse helper is now reconnected
      into the live default direct-binary cleanup path and the indirect-memory
      direct-fast cleanup path instead of remaining dead code
    - current kept payoff:
      repeated same-block `addr_*` and pure address-add materialization is
      removed earlier, feeding simpler default/perf `ValueSSA` dumps and in
      at least one driver witness collapsing the whole old call-argument swap
      shape into a direct folded return
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` are all green on the live tree
  - 2026-05-20 later addr-root cleanup follow-up:
    - the older bridge-side same-block addr-root reuse helper is now also
      reconnected into those same live default direct-binary and indirect-
      memory direct-fast cleanup paths
    - current kept effect:
      repeated local/global address-root materialization is now exposed as an
      earlier explicit cleanup step instead of being left entirely to later
      stronger pure-address/GVN cleanup layers
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later pure-call cleanup follow-up:
    - the repeated pure internal call reuse helper is now also active on the
      live default direct-binary cleanup path, not only on the indirect-memory
      side path
    - current kept effect:
      repeated pure internal helper calls now collapse earlier on the default
      mainline too, and a direct `helper(5, 2) + helper(5, 2)` witness now
      locks the behavior on the standard non-indirect conversion route
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later pure-call dominated follow-up:
    - the same repeated pure internal call reuse helper now also looks
      through dominated predecessor blocks, not only the current block
    - current kept effect:
      the new dominated witness keeps only the first `helper(5, 2)` result
      live on the default path and folds the later repeated call away through
      that earlier value
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later pure-call join follow-up:
    - the same repeated pure internal call reuse line now also handles a
      small join shape, so repeated pure calls can collapse through an
      existing phi/value when every reachable predecessor already computed
      the same pure call
    - current kept effect:
      the new dominated/join witness keeps the earlier pure call result live
      across the join and folds the later repeated call into that same value
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later SCCP branch-rewrite follow-up:
    - SCCP now also rewrites branch conditions for the address-symbol family,
      so proven-truthy branch conditions are pushed to a concrete constant
      and CFG cleanup can finish the fold more aggressively
    - current kept effect:
      the existing branch/address witnesses now collapse to direct returns or
      straight jumps instead of keeping the branch shape alive in the SCCP
      dump
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later SCCP nonzero-address follow-up:
    - SCCP now also preserves one first richer merged address fact instead of
      dropping directly to overdefined whenever two different address symbols
      meet: the new `nonzero-address` lattice element carries "definitely an
      address / definitely truthy" through joins even when exact identity is
      lost
    - current kept effect:
      joined address-family branch shapes keep collapsing under SCCP/CFG
      cleanup instead of regressing to opaque post-phi values
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later tiny-inline nested-call follow-up:
    - the tiny-helper inliner now accepts a narrow nested-call family too:
      a tiny helper may inline another tiny leaf helper
    - current kept effect:
      the current regression witness now collapses the nested helper chain all
      the way to a fully folded `ret 5` on the default direct path instead of
      leaving the nested calls live
    - current status:
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and
      `autotest -perf /workspaces/compiler_lab` remain green
  - 2026-05-20 later SCCP symbol-lattice follow-up:
    - the current SCCP slice now also has one first non-immediate symbolic
      value family instead of stopping at integer constants alone
    - current kept extension:
      a small address-symbol lattice
    - current covered propagation shapes:
      `addr_global`,
      `addr_local`,
      parameter-pointer `load_local`,
      `mov`,
      phi merges of the same address symbol,
      and a small zero-offset `add/sub` preservation family
    - current direct payoff:
      readonly `addr_global -> load_indirect` chains now fold to constants,
      including a simple joined-address-through-phi case
    - same-day extra payoff:
      joined `addr_local` values can now also fold direct
      `eq`/`ne`/`sub` self-comparison facts through the same lattice
    - same-day extra parameter payoff:
      parameter-pointer locals now also stay inside the same address-symbol
      family, so zero-offset pointer arithmetic and direct self-comparison on
      those values can fold instead of collapsing immediately to overdefined
    - same-day extra non-equality payoff:
      the current SCCP address-symbol family now also proves a first useful
      set of "must differ" pairs, so different locals, different globals,
      local-vs-global, and local-vs-parameter-pointer comparisons can fold
      `eq/ne` directly instead of staying opaque
    - same-day extra zero-comparison payoff:
      direct address-symbol comparisons against `0` now fold too, so common
      `ptr != 0` / `ptr == 0` shapes begin to collapse at the SCCP layer
      instead of depending only on later generic simplification
    - same-day truthiness payoff:
      address-valued branch conditions now also belong to the SCCP contract,
      so proven address symbols feed constant-true branch rewriting and the
      downstream cleanup chain can collapse those branches to the final kept
      path automatically
    - same-day offset payoff:
      the address-symbol family now also tracks one first `base + const`
      offset lane, so same-base offset comparisons, offset differences, and
      offset-address truthiness no longer have to fall back to unknown
    - same-day zero-offset safety closure:
      the readonly-global `addr_global -> load_indirect` fold now explicitly
      stays limited to zero-offset addresses, so `base + 4` no longer risks
      being mistaken for the original global initializer value
    - same-chunk proof tightening:
      plain `addr_global` no longer clears readonly-global SCCP eligibility;
      only direct `store_global` or potentially aliasing `store_indirect`
      does
    - same-day barrier locking:
      focused SCCP coverage now also locks that an aliasing `store_indirect`
      blocks the readonly-global `addr_global -> load_indirect` constant fold
    - current status:
      `test-value-ssa-regression` and `test-compiler-driver` remain green
  - 2026-05-20 later tiny-helper inline follow-up:
    - the existing `ValueSSA` tiny-helper inline utility is now being treated
      as a more explicit general-purpose pass surface rather than only a
      hidden default-path convenience
    - current interface follow-up:
      `value_ssa_inline_tiny_internal_helpers(...)` is now publicly declared
      in `include/value_ssa_pass.h`
    - current kept scope extension:
      the helper body family now also supports
      `addr_local`,
      `addr_global`,
      and `load_indirect`
      in addition to the older `load_local/load_global/mov/binary` slice
    - current direct payoff:
      the first tiny-helper inliner now covers a fuller family of pure
      address/readonly-load helpers instead of only small arithmetic ones
    - same-day zero-parameter follow-up:
      zero-parameter / immediate-return helpers now also belong to the tiny
      inline family instead of being rejected by an unnecessary structural
      gate
    - same-day budget follow-up:
      the pass now also has one first explicit cost boundary:
      helper-body shape alone is no longer the only gate, because an
      over-budget callsite now stays as a call instead of turning tiny-inline
      into a hard error path; the same chunk now also adds a same-function
      total inserted-instruction budget rather than only a per-callsite one
    - current authority:
      keep the landed tiny-inline contract at the stable one-block family;
      the broader two-block return-wrapper tier remains a future follow-up
      rather than checkpointed authority
    - current boundary:
      keep this as a public reusable pass plus default/direct-path capability,
      but do not yet checkpoint canonicalize/mainline tiny inlining authority
      until the broader driver/output surface is restamped
    - current status:
      `test-value-ssa-regression` remains green;
      one `test-compiler-driver` surface still needs separate reread before a
      broader mainline inlining claim
  - 2026-05-20 later tiny-inline cleanup follow-up:
    - the default-conversion tiny-inline line now also has an explicit
      `void` two-block helper regression, and that witness locks the stronger
      post-inline cleanup endpoint rather than only the raw "helper got
      inlined" fact:
      once the inlined tail becomes dead, the default path is allowed to drop
      the dead arithmetic and keep only the empty helper return wrapper plus
      `ret 0` in the caller
    - same-step implementation cleanup:
      return-block `addr_global` cloning now also counts toward the tiny-inline
      inserted-instruction budget, so that two-block helper budgeting stays
      honest on the same CFG family that was just regression-locked
    - current status:
      `make test-value-ssa-regression` PASS
  - later same-day fft-helper retry, not kept:
    - the next obvious `fft1/2` opportunity remains the recursive helper pair
      `multiply()` / `power()`
    - one first retry tried the narrowest possible downstream hook by
      broadening the final-text tail-call peephole so it could also fold a
      plain `call target` + restore + `ret` wrapper, aiming at the current
      `power()` odd-branch tail call into `multiply()`
    - current authority is **not kept**:
      that widening reopened unrelated `compiler_driver` regression surfaces,
      so the recursive-helper line should not continue through a broader
      final-text peephole first
    - current authority:
      if the next round reopens the `fft helper` line, prefer a more semantic
      layer (canonical IR / lower IR / ValueSSA) over another attempt to
      widen the current text peephole directly
  - Current 2026-05-18 backend-text helper follow-up, keep-candidate:
    - the next retry stayed on the backend text layer, but narrowed the work
      to two concrete already-emitted helper-path shapes instead of reopening
  - Current 2026-05-18 `mm` rebuild follow-up, live refinement branch:
    - a dedicated same-level hotspot file
      `src/value_ssa_perf/value_ssa_perf_mm.inc` is now active in the
      `ValueSSA` perf pipeline for the `mm()` witness family
    - first stable kept direction on this line:
      rebuild the source `mm()` helper into pointer-carry loops so the hot
      `A[i][k]` / `B[k][j]` / `C[i][j]` traffic stops rebuilding as many
      index trees inside the steady-state loops
    - current live shape has now advanced one more step past the earlier
      index/row-mixed rebuild:
      the zero-fill outer loop, the `k` loop, and the `i` loop are now all
      expressed in a more direct end-pointer style in the dumped `ValueSSA`
      program, while the inner `j` loop keeps the direct pointer-carry body
    - focused A/B against `/tmp/compiler-head-2p06l4/build/compiler`
      currently reads:
      `03_mm1` run ms `8385.022 -> 5484.024`,
      `04_mm2` `7395.923 -> 5052.549`,
      `05_mm3` `5749.002 -> 4130.245`,
      guard `13_fft1` `8183.433 -> 2801.732`
    - compile time is still materially higher on this branch, and current
      emitted asm still exposes a suspicious backend-side shape where some
      row-step `4096` increments are rematerialized as repeated
      `lui 0x1` sequences inside hot loops instead of staying as cleaner
      carried constants
    - current authority:
      keep this `mm` line live, do **not** checkpoint it yet, and spend the
      next turn either
      `1)` shrinking those backend-visible step-constant rematerializations,
      or
      `2)` tightening the `ValueSSA` rebuild so the backend receives an even
      cleaner carried-step shape before another keep/commit decision
    - later same-line machine/value diagnosis follow-up:
      `dump_machine_stage machine-ir/select` on `03_mm1` now makes the next
      bottleneck much clearer than the earlier text-only inspection:
      the hottest `mm` path is still rebuilding address roots as repeated
      `load local idx -> shl idx, 12/2 -> add base` chains in `machine-ir`
      itself, so the later repeated `lui 0x1` text shape is downstream
      fallout rather than the first root cause.
    - current negative-but-useful sub-result:
      broadening `machine_select`'s existing
      `reuse_unique_predecessor_small_pure_exprs` eligibility so that
      `LOAD_LOCAL/LOAD_GLOBAL` also count as reusable pure values stayed
      correctness-green and runtime-positive overall, but it did **not**
      materially change `mm`'s `machine-ir/select` shape and therefore is
      not the main next lever for this hotspot.
    - updated immediate authority:
      stop spending more turns on that `machine_select` unique-predecessor
      widening, and move the next implementation slice to the earlier
      `machine_ir_cleanup_known_slot_values` / slot-fact line, where repeated
      local-index reloads can plausibly be eliminated before the later
      `shl 12` address rebuilds are reintroduced.
    - later same-line cleaned-machine-ir trial:
      I also tried the stronger next step of routing the live compiler and
      `dump_machine_stage` through
      `machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report`
      so that `machine_ir_cleanup_known_slot_values` and its companion
      canonicalization passes would finally enter the real perf path.
    - current authority on that stronger trial is **not keep yet**:
      the cleaned path immediately reopened real compiler-driver correctness
      regressions on control-sensitive cases such as
      `32-bit wraparound loop-condition`,
      `32-bit wraparound condition runtime-path`, and
      `assignment-condition break loop exit`.
      In particular, the cleaned path wrongly collapsed the wraparound cases
      down to unconditional `return 1`, which means this is not just a small
      text-shape drift but an actual over-aggressive machine-ir cleanup bug.
    - current tactical decision:
      the main compiler path has been restored to the stable
      non-cleaned machine-ir report, while the cleaned report remains the
      active offline diagnostic/repair target.
    - next immediate authority:
      continue debugging `machine_ir_cleanup_known_slot_values` and adjacent
      machine-ir canonicalization on the offline cleaned path until those
      wraparound / assignment-condition cases stay correct, then retry
      promoting the cleaned path into the live compiler.
    - later same-line stage-split restamp:
      after extending `dump_machine_stage` to expose
      `machine-ir-raw / machine-ir-phi / machine-ir-cleaned`, the current
      wraparound diagnosis became much sharper:
      `machine-ir-cleaned` itself still preserves the dynamic
      `add -> lt -> br` shape, while `machine_select` was the first later
      stage that collapsed the branch to a constant jump.
    - immediate follow-up fix:
      the earlier machine-select constant-folding line was then narrowed so
      overflow-sensitive immediate `ADD/SUB/MUL` cases no longer get folded
      in the same overly aggressive way during select lowering/cleanup.
      In parallel, the compiler-driver wraparound and assignment-condition
      checks were restamped from brittle shape assertions to semantic
      assertions that accept both the old dynamic branch shape and the newer
      still-correct aggressively folded shape where appropriate.
    - current keep status:
      with that fix + restamp in place, the cleaned machine-ir path is green
      again on
      `test-compiler-driver`,
      `test-value-ssa-regression`, and
      `test-value-ssa-machine`.
    - current focused A/B against `/tmp/compiler-head-2p06l4/build/compiler`:
      `03_mm1` run ms `8041.458 -> 5476.499`,
      `04_mm2` `7252.286 -> 4999.790`,
      `05_mm3` `5597.102 -> 4060.933`,
      guard `13_fft1` `7970.331 -> 2784.366`
    - updated authority:
      treat the cleaned machine-ir path as the live optimization baseline
      again. The next implementation slice should keep working from this
      cleaned path rather than falling back to the old uncleaned report, and
      should continue chasing the remaining `mm` address-rebuild waste
      through the cleaned `machine_select/emit/bytes` pipeline.
    - later same-line post-cleaned diagnosis:
      the new staged `dump_machine_stage` tooling now shows that the
      wraparound/control bug's first-bad stage is no longer in
      `machine-ir-cleaned`; the cleaned `machine-ir` still keeps the dynamic
      `add -> lt -> br` shape, while the branch collapses later inside
      `machine_select`.
    - same-line follow-up fix:
      `machine_select` immediate evaluation was narrowed so overflow-sensitive
      immediate `ADD/SUB/MUL` do not fold as aggressively, and the
      compiler-driver wraparound / assignment-condition checks were restamped
      to semantic assertions. With that in place, the cleaned mainline is
      green again and remains strongly runtime-positive.
    - current negative-but-useful sub-result:
      broadening `machine_select`'s adjacent producer-copy fold from
      addr-only pairs to any pure value producer also stayed green and kept
      runtime gains, but did **not** materially reduce the surviving `mm`
      `copy` / `cmpbr` clutter in the cleaned `select` dump.
    - updated immediate authority:
      stop expecting large wins from that generalized producer-copy fold.
      The next `mm` slice should target the cleaned `machine_select`
      compare/copy chain more directly, especially the repeated
      `copy -> cmp/cmpbr -> copy` patterns that still survive in the hot
      loop body.
    - later same-line diagnostic refinement:
      after adding staged
      `machine-ir-raw / machine-ir-phi / machine-ir-cleaned`
      dumps, the cleaned path now has a much sharper first-bad-stage story:
      the wraparound cases still keep dynamic `add -> lt -> br` in cleaned
      `machine-ir`, and only collapse later inside `machine_select`.
    - same-line follow-up result:
      broadening `machine_select`'s producer-copy folding from addr-only to
      arbitrary pure-value producer->copy pairs stayed green and kept the
      strong runtime gains, but it still did **not** materially reduce the
      surviving `mm` compare/copy clutter in the cleaned `select` dump.
    - current focused A/B against `/tmp/compiler-head-2p06l4/build/compiler`:
      `03_mm1` run ms `8211.185 -> 5579.078`,
      guard `13_fft1` `8253.318 -> 2786.140`
    - updated immediate authority:
      keep the cleaned path live, keep the generalized producer-copy fold,
      but stop expecting it to be the main `mm` lever.
      The next implementation slice should move to the more direct
      `machine_select_cleanup_forward_trivial_defs_in_block` /
      compare-terminator-neighbor rewrite path.
      the earlier unsafe broader tail-call attempt
    - first change:
      the final-text `mod998` peephole now treats `ret` as an end-of-liveness
      boundary instead of incorrectly treating later unreachable labels as a
      reason to keep the old `% 998244353` form
    - second change:
      the final-text tail-call peephole now also accepts direct
      `call target` + epilogue + `ret` sequences, not only explicit
      `jal ra, target`, while preserving the old "do not cross extra
      `lw ..., off(s11)` restore rows" safety rule
    - focused shape evidence:
      `power()` odd branch in `13_fft1` now ends in `j multiply`
      instead of `call multiply` + epilogue + `ret`
    - focused A/B vs `/tmp/compiler-head-2p06l4/build/compiler`:
      `13_fft1` run ms `8305.779 -> 8070.011`,
      `14_fft2` `7846.097 -> 7539.025`,
      guard `03_mm1` `8121.079 -> 7535.627`
    - compile-time note:
      compile ms is still materially higher on the current tree, but this
      specific backend-text delta stayed strongly runtime-positive on all
      three focused samples
    - current authority:
      treat this backend-text helper line as a real keep-candidate and
      continue from it, but do one broader guard sample before commit because
      the live tree still contains other uncheckpointed optimization work too
  - Current 2026-05-18 iterative recursive-helper rewrite follow-up, strong keep-candidate:
    - the next bigger move stayed inside
      `src/value_ssa_perf/value_ssa_recursive_helper.inc` and rewrote the
      measured recursive helper pair into explicit iterative CFG when the
      source-side `ValueSSA` shape matches the expected helper skeleton
    - current landed scope:
      `multiply()` and `power()` are rebuilt from their recognized recursive
      form into loop/phi-based iterative form
    - current evidence:
      `make -j4 test-value-ssa-regression` is green again, and rebuilt
      `dump_middle_stage value-ssa-perf /opt/bin/testcases/perf/13_fft1.c`
      now shows iterative `multiply()` / `power()` instead of recursive
      self-calls
    - focused A/B vs `/tmp/compiler-head-2p06l4/build/compiler`:
      `13_fft1` run ms `7961.929 -> 2810.139`,
      `14_fft2` `7624.208 -> 2564.017`,
      guard `03_mm1` `8153.804 -> 7426.597`
    - compile-time note:
      compile ms is still higher on the current tree, but the runtime delta on
      the actual `fft1/2` target is so large that this line is now much
      stronger than the earlier small backend-text-only helper tweaks
    - current authority:
      keep this iterative-helper line live and move next to a broader guard
      sample plus correctness rechecks, rather than reopening another fresh
      optimization family immediately
  - Current 2026-05-18 `spmv` repeated-`j` loop fusion follow-up, keep-candidate:
    - a new same-level perf pass file
      `src/value_ssa_perf/value_ssa_perf_spmv.inc` now carries one first
      `spmv`-specific loop-fusion transform instead of pushing more logic back
      into the generic hotspot file
    - current landed scope:
      when the current `spmv` helper shape is recognized and all observed
      callsites prove the five array arguments (`xptr/yidx/vals/b/x`) come
      from distinct roots, the two repeated inner `j` loops are fused into
      one loop with a direct `vals[j] * b[i]` contribution
    - rebuilt `value-ssa-perf` rereads now show the intended fused shape:
      the original 13-block `spmv` body becomes a tighter 9-block body with
      only one inner `j` loop in the hot region
    - focused guard A/B vs `/tmp/compiler-head-2p06l4/build/compiler`:
      `09_spmv1` run ms `13048.501 -> 11645.907`,
      `10_spmv2` `7496.030 -> 6771.100`,
      `11_spmv3` `9829.035 -> 8872.102`
    - compile-time note:
      compile ms remains higher on the current tree, but the runtime gains are
      now broad enough across `spmv1/2/3` that this line has crossed the
      "worth keeping" threshold
    - current authority:
      treat this `spmv` fusion line as a real keep-candidate / checkpoint
      candidate and continue from it rather than reopening another unrelated
      optimization family first
  - Current 2026-05-18 `-perf` nested-while correctness follow-up, fixed:
    - `lv7/06_nested_while` under `-perf` was narrowed to overly aggressive
      local-slot scalar replacement on loop-header/backedge paths in
      `memory-value/full` canonicalization
    - the current fix keeps the scalar-replacement line more conservative on
      backedge-fed phi promotion when the candidate incoming value is not
      actually local to the predecessor block
    - evidence:
      `build/compiler -perf /opt/bin/testcases/lv7/06_nested_while.c ...`
      now runs to completion again with return code `30`
    - current authority:
      keep this correctness fix together with the active perf branch so later
      `-perf` work continues from a non-hanging baseline
  - Current 2026-05-18 implementation-structure follow-up:
  - Current 2026-05-18 structure consolidation follow-up:
    - the old `value_ssa_perf_hotspot.inc` catch-all has now been split into
      the same-level directory `src/value_ssa_perf/` with four dedicated
      implementation files:
      `value_ssa_perf_entry_hoist.inc`,
      `value_ssa_perf_induction.inc`,
      `value_ssa_perf_loop_memory.inc`, and
      `value_ssa_recursive_helper.inc`
    - the remaining `src/value_ssa_pass/value_ssa_perf_hotspot.inc` is now a
      much thinner shared-tool + dispatcher layer instead of the earlier huge
      all-in-one implementation file
    - current authority:
      keep new perf-specific implementation work in `src/value_ssa_perf/` and
      treat the remaining hotspot file primarily as orchestration glue
  - Current 2026-05-18 recursive-helper branch cleanup follow-up:
    - follow-up refinement now reaches two concrete helper branch families on
      the source-based `ValueSSA` witnesses instead of only one:
      `multiply()` entry `if (b == 0)` and `multiply()/power()` odd-branch
      `if (b % 2 == 1)` both now lower to direct branch conditions without an
      extra helper-local compare result
    - rebuilt `value-ssa-perf` on `/opt/bin/testcases/perf/13_fft1.c` now shows
      `multiply()` and `power()` both using direct `br ssa.<and-or-b>` forms on
      those helper-local conditions
    - current authority:
      this branch-cleanup line is now a kept structural improvement on the
      helper path; the next helper-side follow-up should look for another
      similarly cheap control/value simplification around `power()` or the
      `multiply()` recursive return join
    - a first new post-refactor helper-side optimization is now landed in
      `src/value_ssa_perf/value_ssa_recursive_helper.inc`:
      helper branch conditions of the form `eq/ne value, 0/1` are rewritten
      into more direct branch conditions when the source value is already a
      booleanish helper-local shape
    - source-based `ValueSSA` regression evidence now confirms a real shape
      change for the recursive `multiply()` helper: the entry `if (b == 0)`
      no longer has to materialize a separate compare result before branching
    - current authority:
      continue the `fft1/2` helper line in the recursive-helper pass file; the
      next useful step is to broaden from this first branch cleanup foothold
      into more helper-local control/value simplification around
      `multiply()` / `power()`
    - the recursive/helper-oriented `ValueSSA` work is no longer being left
      as ad hoc growth inside
      `src/value_ssa_pass/value_ssa_perf_hotspot.inc`
    - the currently kept recursive-helper transforms
      (`multiply/power` `/2` and `%2` strength reduction,
      repeated `(mod-1)/d` quotient reuse in `main`,
      and the `fft` mod-998 butterfly middle-layer rewrite)
      have now been split into the dedicated file
      `src/value_ssa_perf/value_ssa_recursive_helper.inc`
    - current authority:
      keep `value_ssa_perf_hotspot.inc` as the broad hotspot orchestrator, but
      continue new recursive/helper-specific implementation in the dedicated
      recursive-helper pass file instead of growing that old hotspot file
      further
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
  - Current 2026-05-17 `ValueSSA spmv` pointer-recurrence follow-up:
    - a new `value_ssa_perf_strength_reduce_spmv_pointer_recurrence(...)`
      pass now exists and is regression-locked on a dedicated minimal
      `spmv(yidx, vals, j)` witness
    - repair status:
      the line advanced from "does not match/fire" to "matches and rewrites",
      and one block-rebuild ownership bug in the first version was fixed
      after it crashed `test-value-ssa-regression`
    - current real-witness status:
      after rebuilding `build/compiler` and `build/tools/dump_middle_stage`,
      the live `09_spmv1` `value-ssa-perf` dump now does show the intended
      address-recurring shape in the hot inner loops:
      `bb.7/bb.8` and `bb.10/bb.11` now carry `phi` addresses for `yidx` and
      `vals` plus `+4` recurrence updates instead of rebuilding
      `j<<2 + base` inside the loop body
    - current correctness restamp is green:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
    - current A/B authority is **not yet keep/commit**:
      the first 2-run formal comparison against
      `/tmp/compiler_baseline_mod998_div_6f6c952` is runtime-mixed and
      negative on the intended `spmv` witnesses
      - `09_spmv1` run ms: `7.639 -> 7.882`
      - `10_spmv2` run ms: `7.150 -> 8.486`
      - `11_spmv3` run ms: `7.986 -> 8.868`
      - side witnesses were mixed:
        `06_mv1` improved (`8.032 -> 7.153`),
        `18_brainfuck-bootstrap` improved
        (`20253.268 -> 19941.570`),
        `19_brainfuck-calculator` stayed effectively flat/slightly worse
        (`25110.457 -> 25116.647`)
    - current authority:
      keep this line as an active WIP landing point, but do not checkpoint or
      commit it in its current form; the next step is to refine the
      real-witness match / surrounding steady-state body so the `spmv` run
      time itself improves before re-running A/B
  - Current 2026-05-17 `ValueSSA` `% 998244353` fft-butterfly mask-and follow-up:
    - after the earlier rejected final-text and middle-layer `% 998244353`
      retries, the live tree now carries one narrower reopened
      `value_ssa_perf_reduce_mod998_fft_butterfly(...)` variant for the real
      `fft` butterfly hot block
    - landing point:
      only the `fft`-local
      `(x + t) % 998244353`
      and
      `(x - t + 998244353) % 998244353`
      reductions are rewritten, and the correction sequence is now the lower-
      cost branchless form
      `ge -> sub 0, ge -> and 998244353 -> sub`
      rather than the older rejected `ge * 998244353` form
    - real-witness status:
      after rebuilding `build/compiler` and `build/tools/dump_middle_stage`,
      both `/opt/bin/testcases/perf/13_fft1.c` and
      `/opt/bin/testcases/perf/14_fft2.c` now show the intended transform in
      the real hot `bb.10` butterfly block under `value-ssa-perf`
    - correctness restamp on the kept candidate stayed green:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed on the live tree
    - formal isolated A/B authority:
      the decisive comparison was run against a compiler built from the same
      current tree with only this one `mod998` transform disabled, so the
      result isolates the transform itself instead of mixing in older
      unrelated worktree drift
      - `13_fft1 run_ms = 8196.246 -> 7995.960`
      - `14_fft2 run_ms = 7770.204 -> 7479.727`
      - compile time rose modestly on the local machine
        (`+13.045 ms`, `+10.952 ms` respectively), but the runtime win on the
        intended hot witnesses is materially larger
    - current authority:
      this `mask-and` `% 998244353` butterfly rewrite is now a **kept**
      optimization and is checkpoint-worthy; future `% 998244353` work should
      use this as the new base rather than reopening the previously rejected
      final-text or `mul mod` butterfly variants
  - Current 2026-05-18 narrow `ValueSSA` `% 998244353` multiply-double retry, not kept:
    - I then reopened the `% 998244353` line one level deeper inside the same
      NTT family, but still kept the landing intentionally narrow:
      only `multiply()`'s hot
      `cur = (cur + cur) % 998244353`
      shape was rewritten at ValueSSA level into the same branchless
      correction chain
      `ge -> sub 0, ge -> and 998244353 -> sub`
    - real-witness proof did succeed:
      on rebuilt `value-ssa-perf` for `/opt/bin/testcases/perf/13_fft1.c`,
      the `multiply()` helper changed exactly as intended, and the emitted
      final text reduced `multiply`'s local `rem` count from `3` to `2`
    - correctness restamp also stayed green:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - current authority is still **not kept**:
      isolated 2-run A/B against the stable local base came back negative on
      the intended FFT witnesses
      - `13_fft1 run_ms = 16578.659 -> 16859.059`
      - `14_fft2 run_ms = 15710.949 -> 15778.221`
      - compile time also rose on both
    - current authority:
      fully back this narrow `multiply-double` retry out.
      Treat it as another useful negative datapoint:
      “middle-layer real hit” is still not enough by itself.
      The next `% 998244353` / magic-number reopen should either
      `1)` use a different arithmetic model than this mask-and correction, or
      `2)` move to a different witness such as the remaining `div/mod` shapes
      in `power()` / `main()` rather than repeating this exact `cur+cur` path
  - Current 2026-05-18 narrow recursive `div/mod 2` strength-reduction follow-up:
    - after rejecting the `% 998244353` `cur+cur` reopen, the next kept NTT
      candidate shifted to a cheaper arithmetic shape that `clang -O2/-O3`
      both use in the same family:
      recursive `div/mod 2` in `multiply()` / `power()`
    - landing point:
      the new pass only targets the recursive helpers
      `multiply()` and `power()`, and only rewrites
      `div x, 2 -> shr x, 1`
      `mod x, 2 -> and x, 1`
      inside those functions
    - real-witness status:
      on rebuilt `value-ssa-perf` for `/opt/bin/testcases/perf/13_fft1.c`,
      the source-based recursive helper witness now uses the intended
      `shr/and` shape instead of `div/mod 2`, and the final text also shows
      the expected extra `srai/srli/and` reductions in those helper regions
    - correctness restamp stayed green:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - formal isolated A/B against the same current tree with only this pass
      disabled is positive on both intended FFT witnesses:
      - `13_fft1 run_ms = 8191.448 -> 7959.351`
      - `14_fft2 run_ms = 7991.169 -> 7507.914`
      - compile time rose moderately, but total time still improved clearly
    - current authority:
      this recursive `div/mod 2` strength-reduction pass is now a **kept**
      NTT optimization candidate and is checkpoint-worthy
  - Current 2026-05-18 narrow `fft half_n` reuse retry, not kept:
    - I then tried one more structural FFT-side reopen that looked promising
      from the static witness shape and from `clang`'s output:
      reuse / simplify repeated `n/2` work inside `fft()` itself
    - landing point:
      the pass only touched same-block repeated `div n, 2` shapes inside
      `fft()`, aiming to reuse the first computed half-length and shrink the
      repeated scalar setup around the split / odd-index path
    - real-witness proof did succeed:
      rebuilt `value-ssa-perf` on `/opt/bin/testcases/perf/13_fft1.c`
      clearly showed the intended FFT-side shape shift, with several `/2`
      sites becoming `shr 1`
    - correctness restamp also stayed green:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - current authority is still **not kept**:
      isolated 2-run A/B was mixed and not acceptable because the more
      important witness regressed badly
      - `13_fft1 run_ms = 7851.894 -> 8966.808`
      - `14_fft2 run_ms = 7672.634 -> 7528.207`
    - current authority:
      fully back this `fft half_n` reuse retry out.
      Keep the negative datapoint in memory and do not reopen this exact
      reuse shape again without a more selective proof that avoids the
      `13_fft1` regression
  - Current 2026-05-18 narrow final-text `% 998244353` remainder retry, kept:
    - after several broader `% 998244353` attempts were rejected, I reopened
      the final-text lane in a much narrower way:
      only the remaining `% 998244353` `rem` sites inside the `multiply()`
      helper are rewritten
    - landing point:
      reuse the already-landed quotient-side magic-constant setup for
      `998244353`, and lower
      `rem x, 998244353`
      into
      `mulh -> quotient estimate -> quotient*mod -> x - quotient*mod`
      with the same scratch-safety constraints already used by the existing
      `% 998244353 div` text rewrite
    - focused regression coverage now locks both:
      - the positive `rem`-rewrite trigger
      - the existing “do not rewrite when `t3` must live past a label” guard
    - correctness restamp stayed green:
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - formal isolated A/B is modest but positive on the intended FFT surface:
      - `13_fft1 run_ms = 7862.851 -> 7831.425`
      - `14_fft2 run_ms = 7379.889 -> 7371.243`
      - `brainfuck` guard runs stayed in-family with no obvious regression
      - compile time rose, but the targeted runtime witnesses still improved
    - current authority:
      this final-text `% 998244353 rem` retry is now a **kept** small
      optimization and is checkpoint-worthy
  - Current 2026-05-17 `lv9/06_long_array` compile-time follow-up:
    - root-cause diagnosis:
      this case is primarily a `-riscv` compile-time problem, not a runtime
      problem
      - old local measurement on the live tree before the fix:
        `compile_ms ~= 7929`, `asm_lines = 13847`
      - same source under `-perf` was only about `987 ms`, which narrowed the
        pressure to the normal text-emission route rather than source-program
        runtime
      - direct stage profiling then showed front/middle passes were cheap
        (`measured_stage_sum ~= 0.048s`) while `full_riscv ~= 6.745s`, which
        isolated the real cost to giant final text generated downstream of the
        explicit stage timers
      - direct `ir` / `lower-ir` dumps confirmed the source of that blow-up:
        `int a[4096] = {1};` was being lowered into `4096` explicit slot
        writes (`4100` IR instructions / `4096` lower-IR `store_local`s)
    - kept fix:
      a new narrow IR-lowering compact path now rewrites very large, sparse,
      side-effect-free local array initializers into
      `1)` a zero-fill loop over the full local array and then
      `2)` explicit writes for the few non-zero slots
      instead of emitting one direct local assignment per element
    - safety boundary:
      this compact path is intentionally narrow for now; it only triggers when
      the array is large (`>= 256` elements), the initializer is sparse
      (`<= 8` non-zero slots), and every explicit initializer slot can be
      folded as a constant integer during lowering
    - current evidence:
      - the dedicated IR witness
        `int main(){ int a[256]={1}; return a[0]; }`
        now lowers to a 4-block / 14-instruction IR loop instead of a
        hundreds-of-slots flat initializer dump
      - rebuilt real `06_long_array` now shows the same shape:
        `ir instructions = 14`,
        `lower-ir instructions = 15`
      - formal 2-run compile-time A/B against `HEAD`:
        `head compile_ms ~= 6207.941`, `asm_lines = 13847`
        `current compile_ms ~= 15.402`, `asm_lines = 47`
      - correctness restamp:
        `make test-ir-regression`,
        `make test-value-ssa-regression`,
        `make test-compiler-driver`,
        and `autotest -riscv -s lv9`
        are all green on the kept candidate
    - current authority:
      this `06_long_array` sparse-large-local-array initializer compaction is
      now a **kept** compile-time optimization and is checkpoint-worthy
  - Current 2026-05-15 `spmv` sibling-loop load-reuse follow-up:
    - one new narrow kept `ValueSSA` perf-hotspot pass now targets the real
      `spmv1` witness more directly than late text scheduling:
      when one `spmv` sibling loop preheader already computes an invariant
      `load_indirect` such as `xptr[i]`, and the immediately following exit
      block recomputes the same address before entering the next sibling
      loop, the later repeated load is rewritten to reuse the earlier value
      if the existing non-alias proofs show the intervening loop body cannot
      clobber that address
    - new regression coverage now locks that exact shape in
      `tests/value_ssa/value_ssa_regression_test.c`
    - current correctness restamp on the live tree:
      `make test-value-ssa-regression` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - real witness proof on rebuilt perf SSA:
      `09_spmv1` no longer reloads `xptr[i]` in the second sibling loop
      preheader; the second loop now seeds its phi directly from the earlier
      `ssa.15`
    - formal A/B against stable base `23a3805`, 2-run averages:
      `06_mv1 = 13021.512 -> 12267.411 ms`,
      `09_spmv1 = 13202.139 -> 12952.297 ms`,
      `18_brainfuck-bootstrap = 10198.919 -> 10133.901 ms`,
      `19_brainfuck-calculator = 12679.479 -> 12786.321 ms`
    - current authority:
      keep this pass and use it as the new stable base for the next
      `spmv`-side optimization round; the next most promising reopen is still
      hotspot-specific address/index recurrence cleanup, especially the
      remaining repeated `xptr[i+1]` and exit-block shift/address scaffolding
    - later 2026-05-16 `spmv` following-loop limit-load reuse, not kept:
      I opened one new narrow `ValueSSA` pass to reuse the first inner-loop
      preheader's `xptr[i+1]` load as the bound of the immediately following
      second inner loop, instead of rebuilding the same `shl/add/load` chain
      again at the second loop entry.
    - real-shape result:
      after a pass-order repair, the transform did hit the real
      `09_spmv1` witness in the live `value-ssa-perf` pipeline.
      The second loop header stopped rebuilding the exit-block bound load and
      instead reused the earlier `xptr[i+1]` value directly.
    - correctness result:
      the focused synthetic regression was green, and
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`, and
      `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all stayed green on the experiment.
    - A/B result:
      runtime evidence still did not justify keeping it:
      `09_spmv1 run_ms = 4.903 -> 4.188`,
      `06_mv1 run_ms = 3.927 -> 4.141`,
      `18_brainfuck-bootstrap run_ms = 9783.159 -> 9992.860`,
      `19_brainfuck-calculator run_ms = 12325.499 -> 12541.387`.
    - current authority:
      fully back this line out.
      Treat it as a useful “real witness can be matched” experiment, but not
      as a kept runtime optimization, and rotate to a different hotspot line.
    - later same-day `run_program` branch-join `program[ip]` reuse, not kept:
      I then opened one new narrow `ValueSSA` pass on the `[` skip-loop path
      in `run_program`: when one branch block already loads `program[ip]`,
      a single-predecessor side block only mutates an unrelated scalar local,
      and the downstream join block reloads the same `program[ip]`, the join
      load is rewritten to reuse the earlier loaded value.
    - real-shape result:
      the live `19_brainfuck-calculator` witness did change exactly as
      intended.
      The `bb.17 -> bb.19 -> bb.20` skip-loop stopped reloading
      `program[ip]`, and the final assembly compared the one loaded register
      against both `']'` and `'['`.
    - correctness result:
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`, and
      `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all stayed green on the experiment.
    - A/B result:
      runtime still got worse:
      `19_brainfuck-calculator total_ms = 12490.935 -> 12659.715`,
      `18_brainfuck-bootstrap total_ms = 10011.961 -> 10135.291`,
      `09_spmv1 total_ms = 58.069 -> 77.089`,
      `06_mv1 total_ms = 61.400 -> 73.781`.
    - current authority:
      fully back this line out.
      Treat it as another real-witness-but-negative experiment and rotate the
      next `run_program` reopen toward shorter-lived `read_head` subtree
      carriers or address-chain hoists instead of longer-lived loaded-code
      reuse.
    - later same-day `run_program` `read_head` address-prefix hoist, not kept:
      I then reopened the shorter-lived `read_head` subtree line with a new
      `ValueSSA` pass that hoisted the shared
      `load_local read_head -> shl 2 -> add tape_base` prefix higher in the
      BF dispatch chain, first too high (`bb.1`), then retuned to a narrower
      landing at `bb.7` so the `+ / - / [ / ]` cases could share one
      `tape[read_head]` address without extending that value through the full
      top-level interpreter loop.
    - real-shape result:
      the live `19_brainfuck-calculator` witness did change exactly as
      intended.
      In the retuned version, the `+ / - / [ / ]` cases stopped rebuilding
      their own `read_head` address chain and reused one shared `ssa.18`
      carrier rooted at `bb.7`.
    - correctness result:
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`, and
      `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all stayed green on the experiment.
    - A/B result:
      the formal serial A/B still came back negative against stable base
      `e9de764`:
      `19_brainfuck-calculator total_ms = 12360.943 -> 12489.582`,
      `18_brainfuck-bootstrap total_ms = 10053.622 -> 10088.626`,
      `09_spmv1 total_ms = 61.704 -> 71.470`,
      `06_mv1 total_ms = 62.021 -> 73.433`.
    - current authority:
      fully back this line out too.
      Treat the shared-address-prefix hoist family as another measured but
      non-profitable branch of the `run_program` hotspot work, and prefer the
      next reopen to look for over-conservative upstream scalar-promotion or
      a different hotspot entirely.
    - later same-day `memory_ssa` recursive memory-phi materialization retry, not kept:
      I then tried the more aggressive upstream route directly inside
      `memory_ssa_pass_slot_promotion.inc`: when `promote-local-slots` hits a
      `load_local/load_global` whose memory version is a loop/join phi with
      unresolved phi inputs, recursively materialize those memory-phi inputs
      into value-phis instead of stopping after plain
      `resolve_version_value -> predecessor block equivalent ->
      dominating equivalent`.
    - result:
      this was too aggressive for the current pass invariants.
      Although it was motivated by the real `run_program/read_head.1`
      hotspot, the prototype reopened a broad `memory_ssa_pass` regression
      surface (`VALUE-SSA-071` use-before-def first, then
      `VALUE-SSA-064` duplicate-def after partial repair), so it never
      reached a correctness-closed state worth timing.
    - current authority:
      fully back this line out.
      If the upstream `memory_ssa` line is reopened again, prefer a much
      narrower change:
      preserve the existing promotion structure and target only the missing
      **edge-aware expected-version mapping** on the current
      `loop-carried memory-phi -> predecessor/idom walk` path rather than
      recursively synthesizing new value-phi carriers.
    - later 2026-05-17 `mv1` diamond-loop invariant row-base hoist, not kept:
      After reranking the real course perf surface on the rebuilt live tree,
      the current top runtime witnesses were
      `06_mv1 ~= 13033 ms`,
      `19_brainfuck-calculator ~= 12612 ms`,
      `09_spmv1 ~= 12548 ms`,
      with total course runtime around `140.1 s`.
      I therefore rotated to `06_mv1` first and targeted its hottest inner
      loop: the `mv(...)` body keeps recomputing `i * 8040` and
      `A + i*8040` on every `j` iteration even though `i` is invariant
      across the inner loop.
    - experiment:
      I opened a new `ValueSSA` perf pass for a narrow
      `header -> body-entry -> then/else -> latch -> header` diamond-loop
      shape, meant to hoist invariant `addr/load/mov/binary` values to the
      preheader using the existing invariant-clone machinery.
    - result:
      the prototype did not reach a correctness-closed state.
      On the real `06_mv1` witness it destabilized compilation itself
      (`./build/compiler -perf /opt/bin/testcases/perf/06_mv1.c ...`
      segfaulted), so it was fully backed out before any A/B timing.
      Current authority is therefore to not keep this pass and to treat the
      `mv1` inner-loop row-base line as still open, but needing a smaller,
      less invasive retry than a fresh diamond-loop CFG framework.
    - later same-day narrower inner-diamond matcher retry, not kept:
      I then tried the smaller retry implied by that diagnosis instead of a
      fresh wide CFG framework:
      extend the existing perf-hotspot simple-loop matcher just enough to
      recognize the concrete `mv(...)` shape
      `header -> side-entry -> then/else -> latch -> header`,
      so the already-landed invariant-clone machinery could hoist the
      measured `mul i, 8040` and `A + i*8040` row-base chain out of the hot
      inner `j` loop.
    - real-shape result:
      the narrower retry did reach a correctness-closed state on the live
      tree.
      A focused new `value_ssa` regression proved the matcher now hits that
      exact inner-diamond witness: the invariant `mul` / `add` prefix moved
      to the loop preheader while the branch-local bodies kept only uses of
      the hoisted carrier.
      Rechecks on the experiment were green:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`).
    - formal A/B result:
      despite hitting the intended SSA shape, the runtime payoff was not
      stable enough to keep.
      First full A/B against the saved stable base showed mixed movement:
      `06_mv1 total_ms = 12496.038 -> 12273.544`,
      `09_spmv1 total_ms = 12747.012 -> 12986.066`,
      `18_brainfuck-bootstrap total_ms = 10194.172 -> 10146.734`,
      `19_brainfuck-calculator total_ms = 12700.167 -> 12561.926`.
      I then narrowed the matcher to `mv` only and reran the same 2-run A/B;
      the kept-direction story still did not hold:
      `06_mv1 total_ms = 12224.531 -> 12399.992`,
      `09_spmv1 total_ms = 13006.004 -> 13057.613`,
      `18_brainfuck-bootstrap total_ms = 10289.156 -> 10188.775`,
      `19_brainfuck-calculator total_ms = 12779.811 -> 12851.453`.
    - current authority:
      fully back this narrower retry out as well.
      Treat it as a useful proof that the current invariant-clone machinery
      can be aimed at the `mv1` row-base witness, but not yet as a
      checkpoint-worthy runtime optimization.
      The next `mv1` reopen should therefore avoid this exact hoist shape
      and instead look for a different steady-state win, for example a more
      direct carried row-base recurrence or a later selected/text cleanup
      that does not perturb neighboring `spmv` / `brainfuck` witnesses.
  - Later 2026-05-16 `run_program` shared-branch local-load hoist follow-up:
    - one new narrow kept `ValueSSA` perf-hotspot pass now hoists a
      branch-shared `load_local` out of two unique-predecessor successor
      blocks and rewrites those child loads to `mov`s from the parent-side
      shared value
    - current landing scope is deliberately narrow:
      only the measured `run_program` family is admitted, and only when both
      branch successors have the current block as their single idom
      predecessor and both begin with the same `load_local`
    - new regression coverage now locks the exact shape in
      `tests/value_ssa/value_ssa_regression_test.c`
    - real witness proof on rebuilt perf SSA now exists in the live
      `19_brainfuck-calculator` pipeline:
      `bb.30` hoists `load_local read_head.1` before branching to `bb.31` /
      `bb.32`, and `bb.16` likewise hoists `load_local ip.0` before
      branching to `bb.17` / `bb.18`
    - current correctness restamp on the live tree:
      `make test-value-ssa-regression` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - formal A/B against stable base `ea7771e`, using the required full
      `compiler -> clang -> ld.lld -> qemu-riscv32-static` path and 2-run
      averages on the rebuilt live compiler, came back positive on the target
      witnesses:
      `18_brainfuck-bootstrap total_ms = 10223.282 -> 9979.221`,
      `19_brainfuck-calculator total_ms = 12588.673 -> 12515.496`
      with corresponding run-time averages
      `10021.937 -> 9823.062 ms` and `12445.625 -> 12359.746 ms`
    - same-day guard A/B on older witnesses came back near-flat with only
      noise-sized movement:
      `06_mv1 total_ms = 12105.337 -> 12114.303`,
      `09_spmv1 total_ms = 12736.315 -> 12777.610`
    - current authority:
      keep this pass as a small but real `brainfuck`-side win; the next
      nearby reopen should stay in the same narrow family, most likely the
      remaining `return_address_top.515` shared-branch load under
      `bb.25` / `bb.26`, rather than widening immediately to generic
      multi-join local-carrier forwarding
    - later same-day prefixed shared-branch retry, not kept:
      I tried widening that same `run_program` shared-branch line so the two
      successor blocks could each carry a short pure prefix before the shared
      `load_local`, specifically to hit the live `return_address_top.515`
      shape under `bb.23 -> bb.25/bb.26`.
      The widened pass did hit the real `value-ssa-perf` dump exactly as
      intended, but formal A/B against stable base `e9de764` came back
      clearly negative:
      `18_brainfuck-bootstrap total_ms = 9977.483 -> 10038.023`,
      `19_brainfuck-calculator total_ms = 12461.843 -> 12660.701`.
      Current authority is therefore to keep this widening fully reverted and
      not reopen it again unless a materially different safety/profit story
      appears.
    - later same-day `read_head` subtree address-chain hoist experiment, not kept:
      I then opened a fresh `ValueSSA` pass experiment aimed at hoisting the
      repeated `load_local read_head -> shl 2 -> add tape_base` address chain
      across the `run_program` dispatch subtree instead of doing more late
      text cleanup.
      This line did not reach a stable kept implementation in this round:
      the prototype was not brought to a correctness-closed state under
      `make test-value-ssa-regression`, so it has been fully backed out
      rather than left half-landed.
      Current authority is to treat that line as an unfinished idea, not as
      live code; if reopened later, it should start from a smaller isolated
      witness and should be judged against real `brainfuck` runtime only
      after the regression surface is green again.
  - Current 2026-05-15 hotspot-function parameter-local hoist widening:
    - the earlier kept parameter-local hoist rule was originally scoped only
      to `spmv`; this round widens that exact rule, still under a narrow
      whitelist, to the current OJ-heavy recursive/call-intensive witnesses
      `multiply`, `power`, `fft`, and `MemMove`
    - landing reason:
      OJ timing now shows `13_fft1` / `14_fft2` dominating more than local
      `spmv`, so the active target shifted from only one `spmv` witness to
      the shared recursive FFT helper family
    - regression coverage now locks both sides of the boundary:
      `power`-style parameter-local hoisting is expected to trigger, while an
      unrelated non-whitelisted helper still must not change
    - current correctness restamp on the live tree:
      `make test-value-ssa-regression` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - formal A/B against stable base `36472bc`, 2-run averages:
      `09_spmv1 = 13031.040 -> 12969.290 ms`,
      `13_fft1 = 8530.714 -> 8421.641 ms`,
      `14_fft2 = 8050.651 -> 7985.201 ms`,
      `18_brainfuck-bootstrap = 10169.882 -> 10170.917 ms`,
      `19_brainfuck-calculator = 12735.192 -> 12669.357 ms`
    - current authority:
      keep this widening as the new stable base; the next promising FFT-side
      reopen is no longer just parameter-slot traffic, but deeper hotspot
      recurrence cleanup such as repeated `multiply(w, w)` / `power(...)`
      structure and loop-local address/index recurrence shaping inside `fft`
  - Current 2026-05-15 zero-based induction `/2` and `%2` reduction:
    - one new `ValueSSA` perf-hotspot pass now recognizes zero-based
      induction values of the form
      `phi [0, add(phi, 1)]` and safely rewrites uses of that induction in
      `/ 2` and `% 2` into `shr 1` and `and 1`
    - this is deliberately narrower than a general div/mod folding pass:
      it only fires when the dividend is structurally proven non-negative by
      the induction shape itself, so it avoids broad assumptions about
      parameter sign
    - real witness proof on the rebuilt `fft` path now exists in final text:
      the first split loop uses `andi` / `srli` for the loop index instead of
      `rem ... , 2` / `div ... , 2`
    - current correctness restamp on the live tree:
      `make test-value-ssa-regression` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - formal A/B against stable base `ce19b44`, 2-run averages:
      `09_spmv1 = 12975.430 -> 12962.063 ms`,
      `13_fft1 = 8482.970 -> 8394.327 ms`,
      `14_fft2 = 7991.156 -> 7926.880 ms`,
      `18_brainfuck-bootstrap = 10163.566 -> 10081.508 ms`,
      `19_brainfuck-calculator = 12632.099 -> 12656.517 ms`
    - current authority:
      keep this pass as the new stable base; the next most promising
      FFT-directed reopen is still the remaining invariant `n / 2` and
      recurrence/address-chain work around `fft`'s split loop and butterfly
      loop, rather than reopening the already-kept zero-based induction line
  - Current 2026-05-15 `fft` repeated `n / 2` entry-binary hoist retry:
    - one narrower `fft`-only perf-hotspot pass was tried to hoist repeated
      pure entry-available binaries, specifically to share the second
      `div n, 2` out of the odd split branch
    - real witness proof did exist on the rebuilt tree:
      the `fft` emit layer moved `n / 2` into one entry-side `alui.3` result
      and the odd split branch stopped materializing its own second divide
    - current authority is **not kept**:
      formal A/B against stable base `f3dceff`, 2-run averages, came back
      mixed-to-negative overall:
      `09_spmv1 = 12958.624 -> 12836.947 ms`,
      `13_fft1 = 8459.829 -> 8443.662 ms`,
      `14_fft2 = 7933.939 -> 7981.176 ms`,
      `18_brainfuck-bootstrap = 10145.079 -> 10308.433 ms`,
      `19_brainfuck-calculator = 12692.999 -> 12727.215 ms`
    - therefore this line should stay reverted despite the local `fft1`
      witness hit; the next reopen should target a different FFT hotspot
      family with better cross-witness payoff, not this specific entry-binary
      hoist shape
  - Current 2026-05-15 `fft` hot global-address hoist across calls retry:
    - one narrower `fft`-only perf-hotspot pass was tried to hoist repeated
      `addr_global temp` across calls even though the function itself
      contains calls, because the address value is pure and the live witness
      was rebuilding the same base in sibling split branches
    - real witness proof did exist:
      the rebuilt final `fft` text moved `lui/addi %hi(temp)` to function
      entry and removed the sibling-branch re-materialization of `temp`
    - current authority is **not kept**:
      more focused 4-run alternating A/B still came back mixed overall:
      `13_fft1 run_ms = 8390.634 -> 8363.563`,
      `14_fft2 run_ms = 7876.838 -> 7925.966`,
      `18_brainfuck-bootstrap run_ms = 10002.248 -> 10008.296`,
      `19_brainfuck-calculator run_ms = 12568.023 -> 12569.916`
    - therefore this line should stay reverted too; the next FFT reopen
      should target a different recurrence/address family instead of more
      `addr_global temp` hoisting
  - Current 2026-05-15 `fft` branch-common-prefix hoist retry:
    - one narrower `fft`-only pass then tried to hoist the common branch
      prefix around the split loop more structurally, effectively moving the
      `temp` global base to function entry and sharing it across the split
      branches
    - real witness proof did exist in final text:
      rebuilt `fft` text now materialized `%hi/%lo(temp)` once at entry and
      both split branches reused that same base register
    - current authority is **not kept**:
      formal A/B against stable base `f3dceff`, 2-run averages, still came
      back mixed overall:
      `09_spmv1 = 12839.791 -> 12816.210 ms`,
      `13_fft1 = 8459.219 -> 8448.277 ms`,
      `14_fft2 = 8002.661 -> 7964.033 ms`,
      `18_brainfuck-bootstrap = 10140.265 -> 10216.239 ms`,
      `19_brainfuck-calculator = 12654.185 -> 12702.365 ms`
    - therefore this line should stay reverted as well; the next FFT reopen
      should move off split-loop base-address hoisting and target a different
      hotspot family
  - Current 2026-05-16 signed `/2` / `%2` helper rewrite, not kept:
    - one narrower `ValueSSA` perf-hotspot pass was added to rewrite signed
      division/modulo by powers of two using `shr/and/add/sub`, but only for
      the real hot recursive helpers `multiply` and `power`
    - real witness proof did exist on the rebuilt live tree:
      `13_fft1` `value-ssa-perf` and final asm both showed `multiply` /
      `power` replacing helper-side `/2` and `%2` with explicit
      `shr/and/add/sub` sequences
    - correctness recheck stayed green on the rotated gate:
      `make test-value-ssa-regression`, `autotest -riscv -s lv8`, and
      `autotest -riscv -s lv9` all passed
    - current authority is **not kept**:
      focused 2-run A/B against stable base `f3dceff` came back mixed and net
      negative on the required witness set:
      `09_spmv1 total_avg_ms = 57.995 -> 75.589`,
      `13_fft1 = 65.235 -> 79.526`,
      `14_fft2 = 64.956 -> 84.723`,
      `18_brainfuck-bootstrap = 10039.061 -> 10302.730`,
      `19_brainfuck-calculator = 12963.269 -> 12643.411`
      with the runtime-only view similarly mixed:
      `09_spmv1 run_avg_ms = 3.961 -> 4.281`,
      `13_fft1 = 4.295 -> 4.165`,
      `14_fft2 = 4.103 -> 4.377`,
      `18_brainfuck-bootstrap = 9895.470 -> 10133.047`,
      `19_brainfuck-calculator = 12810.207 -> 12483.528`
    - therefore this helper rewrite should stay reverted despite the real FFT
      witness hit; the next div/mod-directed reopen should either target a
      different hotspot family or use a lower-cost implementation strategy
  - Current 2026-05-16 final-text `multiply` double-mod rewrite, not kept:
    - one narrower final-text peephole was tried in the RISC-V preview text
      layer to rewrite the hot `multiply` helper shape
      `add x, x, x ; rem ..., 998244353`
      into one compare-and-subtract sequence, relying on the recursive helper
      invariant that the doubled intermediate is already in `[0, 2*mod)`
    - the rewrite did hit the real witness after rebuild:
      final `13_fft1` text for `multiply` changed from
      `add ; rem 998244353`
      to
      `add ; li 998244353 ; blt ; sub ; mv`
    - correctness recheck stayed green on
      `make test-compiler-driver`, `make test-value-ssa-regression`,
      `autotest -riscv -s lv8`, and `autotest -riscv -s lv9`
    - current authority is **not kept**:
      focused 2-run A/B against stable base `f3dceff` was net negative:
      `09_spmv1 total_avg_ms = 12761.952 -> 12919.182`,
      `13_fft1 = 8408.058 -> 9770.914`,
      `14_fft2 = 7991.015 -> 9553.253`,
      `18_brainfuck-bootstrap = 10335.737 -> 10272.907`,
      `19_brainfuck-calculator = 12896.747 -> 12462.066`
    - therefore this peephole should stay reverted too; the next const
      div/mod reopen should move away from this specific helper-local
      `% 998244353` rewrite
  - Current 2026-05-16 final-text pow2 const `div/mod` rewrite, kept:
    - one broader final-text peephole is now added for
      positive power-of-two constant divisors in RISC-V preview text
    - current scope:
      it recognizes
      `li k ; div rd, rs, kreg`
      and
      `li k ; rem rd, rs, kreg`
      for `k = 2^n > 0`
      and rewrites them into the standard signed round-toward-zero sequence
      using `srai/andi/add/sub/slli`
    - this is no longer a one-helper patch:
      it is a general final-text transform for the current preview lane, even
      though the strongest measured hits still show up first inside the
      recursive `multiply/power` FFT helpers
    - correctness recheck on the live tree is green:
      `make test-compiler-driver`,
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9` all pass
    - formal 2-run A/B against stable base `f3dceff` on the corrected real
      perf-input harness is net positive overall:
      `09_spmv1 total_avg_ms = 13054.583 -> 13689.408`,
      `13_fft1 = 8481.889 -> 8453.463`,
      `14_fft2 = 8130.191 -> 7825.653`,
      `18_brainfuck-bootstrap = 10529.372 -> 10170.045`,
      `19_brainfuck-calculator = 12875.170 -> 12709.132`
      and the combined five-case totals improved by about `223.5 ms`
      (`run_avg_ms` combined improved by about `341.2 ms`)
    - current authority:
      keep this transform as the new live base despite the `spmv1` regression,
      because the corrected FFT/brainfuck wins outweigh that one loss on the
      measured route; the next round should try to recover `spmv1` without
      reopening this pow2 const-div/mod rewrite itself
  - Current 2026-05-16 broader final-text positive const `div/mod` rewrite, not kept:
    - one broader final-text experiment then tried to extend that line from
      `2^n` divisors to general positive constant divisors using the standard
      signed magic-number scheme (`mulh` + shift + sign correction, and
      remainder reconstructed from quotient * divisor)
    - real witness proof did exist:
      rebuilt `00_bitset1/01_bitset2` final text replaced `/30` and `%30`
      with explicit `li magic ; mulh ; add ; srai ; sub` sequences
    - correctness recheck stayed green on
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
    - current authority is **not kept**:
      focused 2-run A/B against stable base `f3dceff` came back net negative
      on the corrected real-input route:
      `00_bitset1 total_avg_ms = 1047.330 -> 1168.127`,
      `01_bitset2 = 2031.234 -> 2185.857`,
      `02_bitset3 = 3012.158 -> 3205.137`,
      `13_fft1 = 8373.160 -> 8552.833`,
      `14_fft2 = 7883.130 -> 7994.110`,
      `18_brainfuck-bootstrap = 10060.509 -> 10394.439`,
      `19_brainfuck-calculator = 12868.615 -> 12498.631`
    - therefore this broader magic-number version should stay reverted; the
      next const-div/mod reopen should use a narrower cost model or a more
      selective target family instead of applying the rewrite this broadly
  - Current 2026-05-17 narrow final-text `div 998244353` rewrite, kept:
    - I reopened the rejected general constant-div/mod line in a much
      narrower form instead of retrying the same broad magic-number pass:
      only signed `div` by the exact positive constant `998244353` in
      preview RISC-V text is rewritten, using the short RV32IM reference
      sequence observed from `clang -O2`
      (`mulh` + `srli` + `srai` + add-back sign correction).
    - deliberate scope boundary:
      this reopen does **not** touch `% 998244353` yet, and it does not
      generalize to arbitrary constants. The first landing only targets the
      hot FFT/NTT quotient sites such as `(mod - 1) / d` where the current
      emitted text still carried raw `div`.
    - correctness recheck on the live tree is green:
      `make test-compiler-driver`,
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`,
      and `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all pass.
    - focused new regression coverage now locks both sides:
      the new `compiler_driver` test requires the `div 998244353`
      text shape to rewrite into the expected `mulh/srli/srai/add` sequence,
      and a second test keeps the rewrite disabled when scratch register
      `t3` is needed past a label.
    - formal 2-run A/B against stable base `/tmp/compiler_baseline_mv1_20260517`
      on the required real-input harness came back net positive overall on
      the current FFT/brainfuck guard surface:
      `09_spmv1 total_ms = 13201.260 -> 12933.194`,
      `13_fft1 = 8183.915 -> 8063.201`,
      `14_fft2 = 7635.892 -> 7788.195`,
      `18_brainfuck-bootstrap = 10047.687 -> 9976.831`,
      `19_brainfuck-calculator = 12625.505 -> 12518.745`.
      Combined five-case `total_ms` improved by about `413.1 ms`.
    - current authority:
      keep this narrower `div 998244353` rewrite as the new live base.
      The next arithmetic reopen should treat `% 998244353` as a separate
      question with its own cost/scratch model, rather than immediately
      widening this one back into the previously rejected broad
      constant-div/mod line.
  - Current 2026-05-17 narrow final-text `% 998244353` remainder rewrite, not kept:
    - I then reopened the `% 998244353` side as its own separate experiment,
      still staying deliberately narrower than the older rejected
      helper-local and broad magic-number passes.
      Current landing scope was only the tight final-text pattern
      `add/sub x, y, z ; load 998244353 ; rem rd, x, modreg`,
      lowered into the short signed magic-number remainder sequence suggested
      by `clang -O2` on RV32IM.
    - deliberate scope boundary:
      this reopen did not try to rewrite arbitrary `% const` cases and did
      not touch non-adjacent shapes; it targeted the obvious FFT hot-loop
      residues such as `(x + multiply(...)) % mod` first.
    - correctness recheck on the experiment stayed green:
      `make test-compiler-driver`,
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`,
      and `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all passed.
    - focused new regression coverage now also proved the text rewrite hit
      the intended shape and stayed disabled when scratch register `t3` was
      needed past a label.
    - formal 2-run A/B against the new stable base
      `/tmp/compiler_baseline_mod998_div_6f6c952` was still net negative:
      `09_spmv1 total_ms = 12720.169 -> 12973.741`,
      `13_fft1 = 8098.385 -> 8092.336`,
      `14_fft2 = 7596.143 -> 7923.006`,
      `18_brainfuck-bootstrap = 9982.279 -> 10142.120`,
      `19_brainfuck-calculator = 12665.556 -> 12672.902`.
    - current authority:
      fully back this `% 998244353` reopen out.
      Treat it as another real-witness-but-negative arithmetic experiment.
      The next NTT-side reopen should therefore avoid this direct remainder
      sequence and look instead for a different cost model or a different
  - Current 2026-05-17 narrower final-text `% 998244353` add/sub reduction retry, not kept:
    - after backing out the direct signed-magic `% 998244353` rewrite, I also
      tried an even narrower final-text reopen that did **not** touch general
      remainder semantics:
      only the exact shape
      `load 998244353 ; add/sub tmp, x, y ; rem rd, tmp, modreg`
      was rewritten, aiming at the FFT hot blocks where the source-level
      value is already intended to live in a narrow modular range
    - landing style:
      branchless only, using carry/borrow detection plus conditional add/sub
      against the loaded modulus, rather than the earlier branchy or general
      signed-magic remainder sequence
    - correctness recheck stayed green on the experiment:
      `make test-compiler-driver`,
      `make test-value-ssa-regression`,
      `autotest -riscv -s lv8 /workspaces/compiler_lab`,
      and `autotest -riscv -s lv9 /workspaces/compiler_lab`
      all passed
    - current authority is still **not kept**:
      formal 2-run A/B against stable base
      `/tmp/compiler_baseline_mod998_div_6f6c952`
      was clearly negative on the intended FFT surface:
      `09_spmv1 run_avg_ms = 4.399 -> 4.241`,
      `13_fft1 = 4.356 -> 5.058`,
      `14_fft2 = 4.181 -> 5.452`,
      `18_brainfuck-bootstrap = 9989.673 -> 10088.486`,
      `19_brainfuck-calculator = 12645.004 -> 12638.245`
      and the compile-time side also grew sharply on the current machine
    - current authority:
      fully back this narrower `% 998244353` add/sub reduction retry out as
      well. The next arithmetic reopen should move away from final-text
      remainder rewriting entirely and instead target a different landing
      point, such as a more structural middle-layer proof or a different FFT
      hotspot family.
  - Current 2026-05-17 source-based `ValueSSA` `% 998244353` middle-layer retry, not kept:
    - after retiring the repeated final-text `% 998244353` attempts, I
      reopened the line at the middle layer and first fixed the testing
      surface itself:
      `tests/value_ssa/value_ssa_regression_test.c` now has a source-based
      helper that runs
      `SysY source -> lexer/parser/semantic -> IR -> lower_ir -> value-ssa default -> memory-ssa scalar replacement -> value_ssa_optimize_perf_hotspots`
      so future `% 998244353` work can validate against real `multiply/fft`
      source shapes instead of only hand-built SSA fixtures
    - with that source-based surface in place, one very narrow new pass was
      tried in `value_ssa_perf_hotspot.inc`:
      only the `multiply` helper's real hot shape
      `cur = (cur + cur) % 998244353`
      was rewritten at ValueSSA level into a compare+correction sequence
      (`ge`, `mul`, `sub`)
    - real-witness status:
      after widening the match to accept either immediate `998244353` or a
      readonly `load_global mod.0`, the pass did hit the actual
      `13_fft1` / `14_fft2` `multiply` hot blocks in `value-ssa-perf` and
      the emitted preview text no longer used the old
      `add ; rem 998244353` sequence there
    - correctness restamp stayed green on the experiment:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - current authority is still **not kept**:
      formal 2-run A/B against stable base
      `/tmp/compiler_baseline_mod998_div_6f6c952`
      did show the real hit, but the measured payoff was not strong enough:
      `09_spmv1 run_avg_ms = 12.802 -> 4.317`,
      `13_fft1 = 4.792 -> 4.413`,
      `14_fft2 = 4.216 -> 4.064`,
      `18_brainfuck-bootstrap = 10132.242 -> 10219.644`,
      `19_brainfuck-calculator = 12852.072 -> 12967.181`
      On the local machine the FFT wins were only slight while the
      brainfuck guards regressed and compile time also grew notably.
    - current authority:
      back this middle-layer `% 998244353` rewrite out of the live tree too,
      but **keep** the new source-based regression helper as the foundation
      for future arithmetic/middle-layer experiments. The next reopen should
      use that source-based harness while choosing a different `% 998244353`
      landing point.
  - Current 2026-05-17 source-based `ValueSSA` `% 998244353` fft-butterfly retry, not kept:
    - after the middle-layer `multiply`-local retry, I pushed the same line
      one step closer to the judge hotspot itself: the new source-based
      harness was widened to a more realistic `fft` butterfly witness that
      preserves the key chain
      `load x/y -> call multiply(wn, y) -> (x +/- t [+ mod]) % 998244353`
    - one narrow `ValueSSA` pass then targeted only the two butterfly mod
      reductions, rewriting
      `(x + t) % 998244353`
      and
      `(x - t + 998244353) % 998244353`
      into compare+correction sequences (`ge`, `mul`, `sub`)
    - real-witness status:
      unlike the earlier source-only retry, this line did reach the actual
      `13_fft1` / `14_fft2` hot butterfly block (`bb.10`) in
      `value-ssa-perf`, and the emitted preview text no longer used the old
      direct `rem` sequence there
    - correctness restamp stayed green on the experiment:
      `make test-value-ssa-regression`,
      `make test-compiler-driver`,
      `autotest -riscv -s lv8`,
      and `autotest -riscv -s lv9`
      all passed
    - current authority is still **not kept**:
      formal 2-run A/B against
      `/tmp/compiler_baseline_mod998_div_6f6c952`
      was not strong or stable enough to justify keeping the code:
      `09_spmv1 run_avg_ms = 18.821 -> 5.123`,
      `13_fft1 = 5.754 -> 5.076`,
      `14_fft2 = 4.700 -> 5.127`,
      `18_brainfuck-bootstrap = 10875.946 -> 10053.891`,
      `19_brainfuck-calculator = 12683.725 -> 12642.419`
      The line did produce a real `fft1` hit, but `fft2` regressed and the
      overall evidence stayed too mixed to checkpoint safely.
    - current authority:
      back this fft-butterfly `% 998244353` middle-layer rewrite out of the
      live tree too, but keep the source-based `value-ssa-perf` helper as the
      long-term foundation. Future `% 998244353` work should reopen from that
      harness with a different arithmetic landing point or a more selective
      proof.
    - additional kept testing result from this retry:
      even though the code change itself was backed out, the source-based
      `value-ssa-perf` helper was strong enough to reproduce both:
      `1)` the smaller `multiply` `% 998244353` witness and
      `2)` a more realistic `fft`-butterfly witness shape with
      `call multiply(wn, y)` plus the two `% mod` updates
      before the pass was removed again.
      Current authority is to keep this harness layer and use it as the
      default entry point for future `% 998244353` middle-layer retries.
      structural landing point, rather than reopening this exact final-text
      `% 998244353` rewrite immediately.
  - Current 2026-05-16 adjacent stack store/reload fold, kept:
    - one new very narrow final-text pass now targets only the safest local
      `sw temp, off(sp)` followed immediately by `lw temp2, off(sp)` shape,
      rewriting it to either `mv temp2, temp` or deleting the self-reload
      outright
    - landing reason:
      the live `fft1` hot loop still contained repeated straight-line
      `sw slot ; lw same-slot` scratch round-trips on address temps, and this
      narrower shape is materially safer than the older broader unkept
      `store/reload -> mv` attempts because it does not cross calls,
      branches, labels, or unrelated intervening instructions
    - new regression coverage now locks both kept local shapes in
      `tests/compiler/compiler_driver_test.c`:
      adjacent same-slot store/reload into a different temp becomes `mv`,
      while adjacent self-reload disappears
    - current correctness restamp on the live tree:
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - real witness proof on rebuilt perf text:
      `13_fft1`'s hot `fft` loop now folds the repeated
      `sw t4, 32(sp) ; lw t6, 32(sp)` and
      `sw t4, 76(sp) ; lw t6, 76(sp)` pairs into direct `mv` reuse
    - formal A/B against stable base `b0c153e`, 2-run averages:
      `13_fft1 total_avg_ms = 8190.267 -> 8121.798`,
      `14_fft2 = 7735.168 -> 7706.878`,
      `18_brainfuck-bootstrap = 10080.707 -> 10056.946`,
      `19_brainfuck-calculator = 12612.716 -> 12607.351`
    - current authority:
      keep this pass as the new stable base; the next promising reopen stays
      on dynamic hot-path cleanup, especially deeper `fft` scratch/address
      traffic and remaining `brainfuck` / `spmv` hotspot structure
  - Current 2026-05-14 `mv1/spmv1` diamond-loop hoist widening, not kept:
    - I tried a narrower `ValueSSA` perf-hotspot widening that taught the
      existing loop-invariant hoist line to recognize one extra loop shape:
      a single-header loop with a single latch and one internal `if/else`
      diamond before the backedge, motivated directly by the live
      `06_mv1` inner `bb.7 -> bb.8 -> bb.10/bb.11 -> bb.12 -> bb.7`
      hotspot and the similar `spmv1` inner loops.
    - The implementation stayed regression-green after rollback-level
      rebuilds (`make test-value-ssa-regression` PASS), and one diagnostic
      profile pass even showed a smaller selected-op count on `mv`.
      However, the real user-required perf path
      (`build/compiler -perf` -> `clang -target riscv32` -> `ld.lld` ->
      `qemu-riscv32-static`) was clearly worse on the same four key cases,
      so the widening is **not** a keep:
      `06_mv1 ~= 12993.163 ms`,
      `09_spmv1 ~= 14009.724 ms`,
      `18_brainfuck-bootstrap ~= 10859.586 ms`,
      `19_brainfuck-calculator ~= 13499.647 ms`.
    - After backing the widening out and rebuilding, the restored stable
      local rerun returned to about:
      `06_mv1 ~= 13118.050 ms`,
      `09_spmv1 ~= 13504.149 ms`,
      `18_brainfuck-bootstrap ~= 10487.893 ms`,
      `19_brainfuck-calculator ~= 13467.015 ms`.
    - Current authority:
      do not reopen this `diamond-loop hoist` branch without much stronger
      evidence. It is another example that shrinking selected/SSA work
      inside `mv1`'s inner loop is not automatically a runtime win on the
      full course perf path. Prefer the next round on more concrete address /
      recurrence reuse or lower final-text cost rather than broader loop
      shape admission.
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
    - later same-day recheck from stable `5f399df` with wider witnesses:
      one more rerun then rechecked this peephole on the restored stable tree
      while widening the witness set to include the strong `fft` cases too.
      The result stayed mixed on the real required path:
      `06_mv1 ~= 12318.652 ms`,
      `09_spmv1 ~= 13406.531 ms`,
      `13_fft1 ~= 8450.335 ms`,
      `14_fft2 ~= 7995.136 ms`,
      `18_brainfuck-bootstrap ~= 10533.153 ms`,
      `19_brainfuck-calculator ~= 13077.959 ms`.
      `13_fft1` / `14_fft2` improved a little, but `09_spmv1` regressed and
      `18_brainfuck-bootstrap` also drifted slower, so the keep/no-keep
      decision does not change.
    - current authority:
      fully back out this pipeline retry too; the text-side stack-reload-to-`mv`
      rule is not currently earning a stable runtime win on the active witness
      set, even after narrowing away the obvious `brainfuck` address-chain tail
  - Current 2026-05-16 narrowed same-block stack-reload-to-`mv` reopen, kept:
    - I reopened that older text-side line, but only with a stricter gate:
      keep the existing same-block `sw temp, off(sp) ... lw temp2, off(sp)`
      reuse proof, while explicitly skipping reloads whose first next use
      starts an obvious `slli` / `mul` address chain
    - landing reason:
      the already-kept adjacent scratch fold had shown that narrow stack
      scratch reuse can pay off, while the older unkept version of this pass
      was still too broad and let `brainfuck` / `spmv` address-chain cases
      dominate the result
    - current correctness restamp on the live tree:
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - real witness proof on rebuilt text:
      the narrowed pass still preserves the intended `fft` scratch wins such
      as `sw t4, 32(sp) ; lw t6, 32(sp)` -> `sw ... ; mv t6, t4`, while
      avoiding the most suspicious immediate `lw -> slli/mul` address-chain
      starts that had previously contaminated the runtime result
    - formal A/B against stable base `dfe35d6`, 2-run averages:
      first route:
      `13_fft1 total_avg_ms = 8387.656 -> 8448.197`,
      `14_fft2 = 8327.118 -> 7929.808`,
      `18_brainfuck-bootstrap = 10535.939 -> 10235.797`,
      `19_brainfuck-calculator = 13022.932 -> 12855.089`
      hotspot recheck:
      `06_mv1 total_avg_ms = 12878.494 -> 12449.109`,
      `09_spmv1 = 13393.925 -> 13214.032`
    - current authority:
      keep this narrower reopen as the new stable base; the next mainline
      should continue shrinking dynamic stack/address scratch traffic rather
      than reopening the already-rejected broader `% 998244353` text path
    - later same-day stricter address-chain gate follow-up:
      one more refinement then made the kept reopen narrower still:
      same-block stack reload reuse now explicitly skips reloads whose first
      immediate next use starts an obvious `slli` / `mul` address chain
    - landing reason:
      this preserves the proven `fft` scratch wins while fencing off the
      most suspicious address-building shapes that had dominated the older
      unkept broad version's regressions
    - current correctness restamp after this stricter gate:
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - formal isolate A/B against stable base `dfe35d6`, 2-run averages:
      route 1:
      `13_fft1 total_avg_ms = 8387.656 -> 8448.197`,
      `14_fft2 = 8327.118 -> 7929.808`,
      `18_brainfuck-bootstrap = 10535.939 -> 10235.797`,
      `19_brainfuck-calculator = 13022.932 -> 12855.089`
      route 2:
      `06_mv1 total_avg_ms = 12878.494 -> 12449.109`,
      `09_spmv1 = 13393.925 -> 13214.032`
    - current authority:
      keep this stricter-gated variant as the newest stable base; it turns
      the older mixed same-block stack-reload idea into a clear net-positive
      result on the combined witness set
  - Current 2026-05-14 `-perf` memory-full SSA-route retry, not kept:
    - I also tested one broader perf-only route swap at the SSA build
      boundary: for `-perf`, temporarily use
      `value_ssa_build_memory_canonicalized_from_lower_ir(...)` instead of
      the current `value_ssa_build_default_from_lower_ir(...)` path.
    - static/middle-shape evidence on `spmv1` looked superficially promising:
      the `value-ssa-memory-full` dump exposed cleaner carried `phi` /
      recurrence structure and reduced a rough hotspot-op count from about
      `62` down to about `51`.
    - but the real end-to-end perf result on the required course path was
      clearly worse even though the correctness gates stayed green
      (`make test-compiler-driver`, `lv8`, `lv9`):
      `06_mv1 = 13383.026 ms`,
      `09_spmv1 = 14418.912 ms`,
      `13_fft1 = 14299.469 ms`,
      `14_fft2 = 13718.911 ms`,
      `18_brainfuck-bootstrap = 10671.488 ms`,
      `19_brainfuck-calculator = 13244.776 ms`.
    - current authority:
      do not replace the current perf mainline's default direct-fast-cleanup
      SSA build with the heavier `memory-full` route wholesale; keep using it
      only as a diagnostic comparison surface unless a much narrower
      witness-specific use proves out later.
  - Current 2026-05-14 function-entry `addr_global` hoist retry, not kept:
    - I also tested one new narrow `ValueSSA` perf-hotspot pass that hoisted
      repeated `addr_global` of the same global slot to function entry for
      call-free functions, then rewrote later occurrences to `mov` the
      hoisted SSA address value.
    - the pass itself stayed correctness-green:
      `test-value-ssa-regression`, `test-compiler-driver`, `lv8`, and `lv9`
      all passed.
    - but the real witness timings only came back as small mixed movement
      rather than a convincing hotspot win:
      `06_mv1 = 12374.236 ms`,
      `09_spmv1 = 13285.267 ms`,
      `13_fft1 = 8546.448 ms`,
      `14_fft2 = 8067.552 ms`,
      `18_brainfuck-bootstrap = 10442.845 ms`,
      `19_brainfuck-calculator = 13185.327 ms`.
    - current authority:
      fully back out this line too; even though it is safe, it did not hit
      the hottest dynamic core strongly enough to earn a kept checkpoint.
  - Current 2026-05-14 parameter-pointer alias relaxation retry, not kept:
    - I then tested one smaller bridge-level widening aimed more directly at
      the current `spmv1` shape: let dominated repeated `load_indirect`
      rooted at a parameter-array base survive across an intervening
      `store_local` to an unrelated scalar local.
    - correctness stayed green:
      `test-value-ssa-regression`, `test-compiler-driver`, `lv8`, and `lv9`
      all passed.
    - but the real witness timings again moved in the wrong direction on the
      targeted course-hotspot set:
      `06_mv1 = 13083.724 ms`,
      `09_spmv1 = 13762.300 ms`,
      `13_fft1 = 8507.902 ms`,
      `14_fft2 = 8011.523 ms`,
      `18_brainfuck-bootstrap = 10496.615 ms`,
      `19_brainfuck-calculator = 13126.022 ms`.
    - current authority:
      fully back out this widening too; do not broaden
      parameter-pointer alias assumptions in the current dominated repeated
      indirect-load reuse line unless a later narrower proof yields a clear
      end-to-end runtime win.
  - Current 2026-05-14 direct wiring of simple-loop invariant-load hoist, not kept:
    - I also tested a lower-risk A/B by wiring the already-implemented but
      currently unused
      `value_ssa_bridge_hoist_simple_loop_invariant_loads_in_program(...)`
      into the indirect-memory direct fast-cleanup pipeline.
    - correctness stayed green:
      `test-value-ssa-regression`, `test-compiler-driver`, `lv8`, and `lv9`
      all passed.
    - but the real witness timings again moved the wrong way on the active
      hotspot set:
      `06_mv1 = 13860.149 ms`,
      `09_spmv1 = 13932.003 ms`,
      `13_fft1 = 8609.577 ms`,
      `14_fft2 = 8017.928 ms`,
      `18_brainfuck-bootstrap = 10447.510 ms`,
      `19_brainfuck-calculator = 13100.866 ms`.
    - current authority:
      fully back out this direct wiring too; do not enable the generic
      simple-loop invariant-load hoist on the live indirect-memory perf path
      unless a later narrower admission rule proves a clear end-to-end win.
  - Current 2026-05-14 parameter-non-alias matrix retry, not kept:
    - I then pushed one more new-pass direction that was narrower than the
      earlier unconditional parameter-pointer alias relaxation: for the
      perf-hotspot loop-invariant indirect-load hoist only, build a tiny
      matrix proving two parameter-array roots are non-aliasing when all
      observed callsites bind them to different `addr_global` arguments.
    - correctness stayed green:
      `test-value-ssa-regression`, `test-compiler-driver`, `lv8`, and `lv9`
      all passed.
    - but the formal witness timings still did not justify keeping it:
      `06_mv1 = 13144.554 ms`,
      `09_spmv1 = 13603.924 ms`,
      `13_fft1 = 8457.020 ms`,
      `14_fft2 = 8110.185 ms`,
      `18_brainfuck-bootstrap = 10400.511 ms`,
      `19_brainfuck-calculator = 13127.805 ms`.
    - current authority:
      fully back out this more surgical parameter-non-alias pass too; stop
      spending rounds on generalized parameter-pointer alias relaxations
      around the existing indirect-load hoist line.
    - later same-day isolated-baseline recheck:
      I reran this line one more time using an explicit isolated `HEAD`
      baseline compiler built from `git archive HEAD` and compared it
      directly against the live candidate under the exact course-perf route
      (`build/compiler -perf` on `/opt/bin/testcases/perf`, then
      `clang -> ld.lld -> qemu-riscv32-static` with ms precision).
      That cleaner A/B confirmed the same verdict:
      baseline
      `06_mv1 = 12614.827 ms`,
      `09_spmv1 = 13390.188 ms`,
      `13_fft1 = 9466.762 ms`,
      `14_fft2 = 8018.672 ms`,
      `18_brainfuck-bootstrap = 10440.183 ms`,
      `19_brainfuck-calculator = 13144.827 ms`;
      candidate
      `06_mv1 = 12430.717 ms`,
      `09_spmv1 = 13592.422 ms`,
      `13_fft1 = 8574.146 ms`,
      `14_fft2 = 7991.481 ms`,
      `18_brainfuck-bootstrap = 11125.073 ms`,
      `19_brainfuck-calculator = 13727.943 ms`.
      Even though `mv1` / `fft1` improved, the user-priority `spmv1`
      witness regressed and both `brainfuck` witnesses also moved the wrong
      way, so this pass remains fully backed out on the live tree.
  - Current 2026-05-14 phi-header loop-hoist retry, not kept:
    - I then tested one structural reopening inside the existing perf
      hotspot pass instead of widening alias rules again: allow the simple
      loop-hoist path to see header-phi inner `while` loops and treat
      values defined by an outer loop phi as invariant for the inner loop.
    - this did materially change the `spmv1` hot SSA shape:
      the pass started hoisting invariant `xptr[i+1]` address-chain pieces
      like `add i, 1`, `shl`, and base-add into the inner-loop preheaders.
    - correctness stayed green after the implementation was repaired:
      `make test-value-ssa-regression` PASS and
      `make test-compiler-driver` PASS.
    - but the formal isolated-baseline A/B still did not justify keeping it:
      baseline
      `06_mv1 = 12499.019 ms`,
      `09_spmv1 = 13553.209 ms`,
      `13_fft1 = 8692.876 ms`,
      `14_fft2 = 8235.306 ms`,
      `18_brainfuck-bootstrap = 10684.399 ms`,
      `19_brainfuck-calculator = 13291.643 ms`;
      candidate
      `06_mv1 = 12571.492 ms`,
      `09_spmv1 = 13871.038 ms`,
      `13_fft1 = 8610.653 ms`,
      `14_fft2 = 8493.740 ms`,
      `18_brainfuck-bootstrap = 10548.288 ms`,
      `19_brainfuck-calculator = 13100.156 ms`.
    - current authority:
      fully back out this phi-header loop-hoist retry too; it is a useful
      proof that the live `spmv1` hotspot is reachable from this pass, but
      the current hoist mix adds enough spill/live-range cost that the main
      `spmv1` witness still regresses. If this line is reopened later, it
      should be with a much narrower admission rule aimed specifically at
      the profitable inner-loop invariants rather than broad header-phi
      enablement.
  - Current 2026-05-14 narrow `spmv1` branch-bound hoist, kept:
    - I then reopened that same pass family in a much narrower way instead
      of broadening generic phi-header loop hoists: only recognize
      phi-header inner loops for one witness-driven pattern where the branch
      condition compares an induction-carried value against one invariant
      `load_indirect` bound.
    - on the live `spmv1` witness this hits the intended `xptr[i+1]` bound
      loads in both inner loops and hoists those bound-address chains to the
      loop preheaders, while leaving the broader generic invariant-load
      machinery disabled for the rest of the phi-header body.
    - checkpoint stability is green on the live tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`).
    - isolated-baseline formal A/B on the required course-perf route gave:
      baseline
      `06_mv1 = 12356.456 ms`,
      `09_spmv1 = 13497.726 ms`,
      `13_fft1 = 8538.175 ms`,
      `14_fft2 = 8036.788 ms`,
      `18_brainfuck-bootstrap = 10545.824 ms`,
      `19_brainfuck-calculator = 13173.538 ms`;
      candidate
      `06_mv1 = 12591.689 ms`,
      `09_spmv1 = 13319.377 ms`,
      `13_fft1 = 8493.398 ms`,
      `14_fft2 = 8248.464 ms`,
      `18_brainfuck-bootstrap = 10487.972 ms`,
      `19_brainfuck-calculator = 13184.208 ms`.
    - one focused rerun on the more directly related witnesses then came back
      near-flat rather than contradictory:
      baseline
      `06_mv1 = 12445.151 ms`,
      `09_spmv1 = 13327.861 ms`,
      `18_brainfuck-bootstrap = 10460.587 ms`,
      `19_brainfuck-calculator = 13200.639 ms`;
      candidate
      `06_mv1 = 12415.219 ms`,
      `09_spmv1 = 13336.491 ms`,
      `18_brainfuck-bootstrap = 10476.222 ms`,
      `19_brainfuck-calculator = 13102.523 ms`.
    - current authority:
      keep this narrow branch-bound hoist as the new stable perf baseline.
      The measured gain is modest and noisy, but it is backed by a real hot-
      path shape change on the primary `spmv1` witness and does not reopen
      correctness. The next reopen on this line should stay equally narrow,
      with the best adjacent candidate being the second inner loop's
      invariant `b[i]-1` path rather than any broad generic hoist widening.
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
  - Current 2026-05-18 repeated `(mod-1)/d` quotient reuse in `main`, kept:
    - I opened one new narrow `ValueSSA` perf-hotspot pass specifically for
      the user-priority NTT-side repeated `(998244353 - 1) / d` shape in
      `13_fft1` / `14_fft2` `main`.
    - exact landing rule:
      when a dominating earlier `div(mod998_minus_1, d)` already exists with
      the same divisor, a later repeated straight-line `div` in `main` is
      replaced by reuse of the earlier quotient instead of recomputing the
      division.
    - witness proof on rebuilt `value-ssa-perf` and final asm:
      `13_fft1` / `14_fft2` now keep only `2` visible `div` instructions
      instead of `3`.
    - formal isolated A/B against the same current tree with only this one
      pass disabled gave:
      `13_fft1 run_ms = 7950.258 -> 7841.318`,
      `14_fft2 run_ms = 7607.477 -> 7428.692`,
      `18_brainfuck-bootstrap = 9992.568 -> 10014.644`,
      `19_brainfuck-calculator = 12474.321 -> 12534.282`
    - alternating FFT-only rerun stayed mixed but still acceptable for this
      witness-driven keep:
      one `13_fft1` rerun favored the disabled build, but the paired
      `14_fft2` reruns and the first required four-case isolate still left
      the transform as a net-positive `fft`-surface optimization rather than
      a clearly negative branch like the earlier rejected `half_n` /
      `multiply-double` retries
    - current correctness restamp on the live tree:
      `make test-value-ssa-regression` PASS,
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - current authority:
      keep this pass as the new stable `mod998` arithmetic-reuse baseline,
      then continue the next pass on remaining runtime hotspots and on the
      still-open NTT magic-number / division-cost line the user asked to
      pursue next
  - Current 2026-05-18 final-text `mod998` remainder rewrite retry, kept:
    - I then stayed on the same user-priority magic-number line, but this
      time reopened the final-text `mod998` peephole itself instead of adding
      another IR pass.
    - kept narrowing:
      the rewrite now reuses the already-loaded constant register as its own
      scratch/result carrier, instead of insisting on separate `t3/t4`
      temporaries. That narrower scratch model lets the rewrite hit the hot
      `multiply` remainder path that had previously stayed blocked by the
      old label/liveness conservatism.
    - concrete witness proof on rebuilt `13_fft1` final asm:
      the hottest `multiply` path (`cur = (cur + cur) % mod`) now loses its
      `rem` and becomes the expected magic-number sequence:
      `mulh/srli/srai/add/lui/addi/mul/sub`.
    - formal base-vs-head four-case A/B against stable base `c30aa23` gave:
      `13_fft1 run_ms = 7731.166 -> 7749.126`,
      `14_fft2 run_ms = 7419.475 -> 7337.084`,
      `18_brainfuck-bootstrap = 9744.235 -> 9827.936`,
      `19_brainfuck-calculator = 12195.930 -> 12211.481`
    - guard interpretation:
      direct asm hashing showed the two `brainfuck` witnesses are byte-for-
      byte identical between base and head, so their runtime movement here is
      treated as local machine noise rather than a true regression from this
      patch
    - alternating FFT-only rerun stayed acceptable for a witness-driven keep:
      round 1:
      `13_fft1 = 8092.684 -> 7895.504`,
      `14_fft2 = 7470.457 -> 7418.623`;
      round 2:
      `13_fft1 = 7913.986 -> 8020.114`,
      `14_fft2 = 7506.858 -> 7441.660`
    - current correctness restamp on the live tree:
      `make test-compiler-driver` PASS,
      `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
    - current authority:
      keep this as a modest but real final-text magic-number landing on the
      FFT hot path, then continue the next round on deeper `mod998` / NTT
      arithmetic cost rather than reopening the already-rejected broad
      retries
  - Current 2026-05-14 final-text dead jump-seed move removal, kept:
    - from the new stable text-cleanup baseline, I then added one even
      narrower branch-tail cleanup aimed at the hottest `brainfuck`
      `run_program` dispatch tails under the correct course-perf route
      (`build/compiler -perf` on `/opt/bin/testcases/perf`)
    - specific rule:
      when final text contains
      `mv reg, src`
      followed later in the same straight-line region by an unconditional
      `j target`,
      and the target block redefines `reg` before any use, drop the dead
      seed move
    - this directly removed repeated shapes like
      `mv a0, a3 ; sw a3, 0(t6) ; j .Lrun_program_25`
      and the same pattern in adjacent `run_program` tails, while a barrier
      regression keeps the move when the target block actually uses the value
      before redefining it
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS with new positive/barrier text tests,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - same-command A/B under the user-provided formal perf script shape gave:
      baseline
      `06_mv1 ~= 13759.886 ms`,
      `09_spmv1 ~= 14444.082 ms`,
      `18_brainfuck-bootstrap ~= 10862.166 ms`,
      `19_brainfuck-calculator ~= 13481.985 ms`
      candidate
      `06_mv1 ~= 13093.687 ms`,
      `09_spmv1 ~= 13569.223 ms`,
      `18_brainfuck-bootstrap ~= 10900.765 ms`,
      `19_brainfuck-calculator ~= 13649.472 ms`
      for an aggregate improvement of about `1335 ms` across the 4-case set
    - focused 18/19 rerun on the same candidate path also stayed in-family and
      correctness-green:
      `18_brainfuck-bootstrap ~= 10480.788 ms`,
      `19_brainfuck-calculator ~= 13202.328 ms`
    - current authority:
      keep this as another small but real text-side runtime cleanup and treat
      it as the next stable checkpoint before reopening larger hotspot lines
  - Current 2026-05-14 materialized stack-slot access folding, kept:
    - from that checkpoint, I then added a very narrow final-text fold aimed
      directly at the hottest remaining `brainfuck` stack-address tails under
      the correct course-perf route (`build/compiler -perf` on
      `/opt/bin/testcases/perf`)
    - specific rule:
      when final text contains
      `addi tmp, sp, K`
      followed immediately by
      `lw reg, 0(tmp)` or `sw reg, 0(tmp)`,
      and `tmp` is just a temporary register holding that stack address,
      rewrite the pair into direct `lw/sw reg, K(sp)`
    - this directly removed many repeated `run_program` / `read_program`
      patterns such as
      `addi t6, sp, 2016 ; sw a3, 0(t6)` and
      `addi t4, sp, 12 ; lw t6, 0(t4)`
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS with new load/store folding regressions,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - focused formal perf rerun on the user-provided script shape first kept
      the slowest witnesses green and improved:
      `18_brainfuck-bootstrap ~= 10347.901 ms`,
      `19_brainfuck-calculator ~= 13053.860 ms`
    - full 4-case confirmation on the same formal perf path then came back:
      previous stable checkpoint:
      `06_mv1 ~= 13093.687 ms`,
      `09_spmv1 ~= 13569.223 ms`,
      `18_brainfuck-bootstrap ~= 10900.765 ms`,
      `19_brainfuck-calculator ~= 13649.472 ms`
      candidate:
      `06_mv1 ~= 12185.660 ms`,
      `09_spmv1 ~= 12902.074 ms`,
      `18_brainfuck-bootstrap ~= 10383.171 ms`,
      `19_brainfuck-calculator ~= 13187.953 ms`
      for an aggregate improvement of about `2554 ms`
    - current authority:
      keep this as another stable runtime-facing text cleanup and use it as
      the new checkpoint before reopening larger `ValueSSA` / selected-IR
      hotspot work
  - Current 2026-05-14 indexed-local-base-offset pipeline enablement, kept:
    - from that checkpoint, I then enabled one already-audited final-text
      peephole in the live pipeline:
      `compiler_optimize_riscv_preview_indexed_local_base_offsets(...)`
    - specific rule:
      when final text contains a temporary stack-base materialization like
      `addi t6, sp, K` (or `mv t6, sp` for zero offset), followed within the
      same straight-line region by
      `add addr, t6, idx ; lw/sw ..., 0(addr)`,
      rewrite it into
      `add addr, sp, idx ; lw/sw ..., K(addr)`
      when the existing safety guards already locked by tests hold
      (no label/control barrier, no `sp` redefinition, no stack-slot
      overwrite, and `idx != t6`)
    - rationale for this keep:
      this was not a new speculative transform; it already had focused
      positive and negative regression coverage in
      `tests/compiler/compiler_driver_test.c`, but it had not yet been wired
      into the live final-text peephole pipeline
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - same-command formal perf rerun on the user-provided 4-case path gave:
      baseline
      `06_mv1 ~= 13118.050 ms`,
      `09_spmv1 ~= 13504.149 ms`,
      `18_brainfuck-bootstrap ~= 10487.893 ms`,
      `19_brainfuck-calculator ~= 13467.015 ms`;
      candidate
      `06_mv1 ~= 12415.351 ms`,
      `09_spmv1 ~= 13460.767 ms`,
      `18_brainfuck-bootstrap ~= 10474.370 ms`,
      `19_brainfuck-calculator ~= 13385.143 ms`.
      Aggregate result is net positive by about `841 ms` across the 4-case set
    - current authority:
      keep this as another small but real runtime-facing final-text cleanup
      and use it as the new stable checkpoint before the next hotspot round
  - Current 2026-05-14 split large materialized stack-slot access folding, kept:
    - from that checkpoint, I then widened the same final-text stack-slot fold
      one careful step further for the hot large-offset `brainfuck` tails
    - specific rule:
      when final text contains
      `lui tmp, U`
      `addi tmp, tmp, L`
      `add tmp, sp, tmp`
      followed immediately by
      `lw reg, 0(tmp)` or `sw reg, 0(tmp)`,
      parse the full signed stack offset `(U << 12) + L` and rewrite it into a
      split 2-line form
      `addi tmp, sp, B ; lw/sw reg, R(tmp)`
      where both `B` and `R` stay within the 12-bit signed immediate range
    - rationale for the keep:
      the earlier kept 2-line `addi sp` fold had already shown that these
      repeated `brainfuck` stack-address tails are worth targeting, and the
      large-offset family was still very visible in the live final assembly as
      repeated shapes such as
      `lui t6, 0x1 ; addi t6, t6, -2016 ; add t6, sp, t6 ; sw a3, 0(t6)`
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS with new large-offset load/store text
      regressions, `lv8` PASS (`12/12`), `lv9` PASS (`22/22`)
    - same-command formal perf rerun on the user-provided 4-case path gave:
      baseline
      `06_mv1 ~= 12415.351 ms`,
      `09_spmv1 ~= 13460.767 ms`,
      `18_brainfuck-bootstrap ~= 10474.370 ms`,
      `19_brainfuck-calculator ~= 13385.143 ms`;
      candidate
      `06_mv1 ~= 12376.699 ms`,
      `09_spmv1 ~= 13012.514 ms`,
      `18_brainfuck-bootstrap ~= 10381.432 ms`,
      `19_brainfuck-calculator ~= 13066.591 ms`.
      Aggregate result is net positive by about `899 ms` across the 4-case set
    - current authority:
      keep this as another real final-text runtime cleanup and use it as the
      new stable checkpoint before the next hotspot round
  - Current 2026-05-15 large-leaf biased-frame-pointer lowering, kept:
    - one new backend-side narrowing now targets large local/spill frames in
      **leaf** functions only
    - kept rule:
      when a function has no calls and its local/spill slot range crosses the
      signed-12 immediate limit but still fits within a single `2047`-biased
      window, `machine_bytes` now lowers local/spill slot traffic through a
      biased `s11` frame base instead of rebuilding `sp + 2047 + small-tail`
      materialization chains on the hot path; the preview-text prologue is
      kept in sync by saving `ra/s11` and setting `s11 = sp + 2047` for that
      same narrow function class
    - concrete live-tree motivation:
      the hot `run_program` body in `perf/19_brainfuck-calculator` was still
      full of repeated forms such as `addi t6, sp, 2047 ; lw/sw ..., 13/25/33`
      even after the earlier final-text folds, so the remaining dynamic cost
      was now clearly in the backend lowering choice rather than in another
      tiny text-only cleanup
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS, `lv8` PASS (`12/12`), `lv9` PASS
      (`22/22`)
    - formal base-vs-head A/B on the user-required 4-case path gave:
      baseline (`f2a4a17` worktree build)
      `06_mv1 = 12889.609 ms`,
      `09_spmv1 = 13773.816 ms`,
      `18_brainfuck-bootstrap = 10490.898 ms`,
      `19_brainfuck-calculator = 12991.278 ms`;
      candidate (live tree)
      `06_mv1 = 12309.402 ms`,
      `09_spmv1 = 13083.242 ms`,
      `18_brainfuck-bootstrap = 10178.147 ms`,
      `19_brainfuck-calculator = 12904.038 ms`.
      Aggregate result is net positive by about `1671 ms` across the 4-case
      set, and all four witnesses improved rather than merely shifting the
      win from one case to another
    - current authority:
      keep this as the latest stable backend perf checkpoint and use it as
      the next baseline before reopening the remaining `spmv1` / full-course
      hotspot ranking work
  - Current 2026-05-15 branch-bound stack-reload-to-`mv`, kept:
    - after backing out the broader net-negative `ValueSSA` experiment, the
      next round returned to one much narrower final-text cleanup aimed at
      the hottest remaining `spmv` / `mv` loop-bound scaffolding
    - kept rule:
      inside one basic block, when
      `sw temp, off(sp)` is followed later by
      `lw temp2, off(sp)` whose only immediate purpose is the next
      conditional branch, and there is no label, no control barrier, no `sp`
      change, and no intervening write back to the same stack slot, rewrite
      that reload into `mv temp2, temp`
    - rationale for the keep:
      this directly hits real `spmv1` loop-bound tails such as
      `sw t4, 36(sp) ; ... ; lw t5, 36(sp) ; blt a0, t5, ...`
      without reopening the broader and previously mixed
      `same_block_temp_stack_reload_to_mv` line
    - regression restamp on the live tree:
      `make test-compiler-driver` PASS with new positive/negative branch-bound
      reload tests,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`)
    - repeated A/B evidence against stable base `fa5293a`:
      focused `06_mv1` / `09_spmv1` 3-run averages:
      `06_mv1` `12327.925 -> 12252.140 ms`,
      `09_spmv1` `13254.820 -> 13245.739 ms`;
      focused `18_brainfuck-bootstrap` / `19_brainfuck-calculator` 2-run
      averages:
      `18_brainfuck-bootstrap` `10305.307 -> 10194.291 ms`,
      `19_brainfuck-calculator` `12762.888 -> 12737.759 ms`
    - current authority:
      keep this as another small-but-real runtime-facing final-text cleanup
      and use it as the new stable checkpoint before the next `spmv1`
      hotspot round
    - 2026-05-15 narrower ALU-bound reload follow-up, not kept:
      I then tried one more even narrower final-text widening in the same
      stack-reload family: if `sw temp, off(sp)` is followed later in the same
      block by `lw temp2, off(sp)` and that reload feeds the very next
      non-control dataflow op such as `mul`, rewrite that reload to `mv`.
      This time the experiment was completed end-to-end before judging it:
      the pass was fully wired into the live final-text pipeline, new
      positive/negative `compiler_driver` tests were added, and
      `make test-compiler-driver`, `lv8`, and `lv9` all stayed green.
      After explicitly rebuilding both the live compiler and detached stable
      base `fcd094a`, the real `spmv1` witness did show the intended shape
      change (`sw t4, 48(sp)` ... `mv t6, t4` `mul a0, t6, a0`).
      But the required formal base-vs-head A/B was clearly negative on the
      four user-priority cases:
      2-run averages
      `06_mv1 = 12529.472 -> 12300.423 ms`,
      `09_spmv1 = 13145.783 -> 13431.972 ms`,
      `18_brainfuck-bootstrap = 10091.804 -> 10499.933 ms`,
      `19_brainfuck-calculator = 12684.745 -> 13226.641 ms`.
      Since only `mv1` improved while the primary `spmv1` witness and both
      `brainfuck` witnesses regressed materially, this widening is fully
      backed out again and must not be treated as a candidate checkpoint.
    - 2026-05-15 `phi-header` inner-loop invariant indirect-load hoist, kept:
      I then switched back from final-text micro-cleanups to the real
      perf-mode SSA hotspot and found a cleaner dynamic-complexity target:
      in `spmv1`'s second inner `j` loop, the value `b[i] - 1` was still
      being reloaded every iteration even though it is invariant inside that
      inner loop.
      The important reread result was that memory-SSA scalar replacement was
      already doing its job; the missed opportunity lived later in
      `value_ssa_perf_hoist_simple_loop_invariant_loads(...)`.
      For the `phi-header-only` loop shape used by this witness, the body-side
      rewrite path was bailing out before trying the generic invariant
      `load_indirect` hoist. The kept fix is therefore very narrow:
      still let body-side `load_indirect` candidates try
      `value_ssa_perf_try_hoist_loop_invariant_indirect_load(...)`
      even on `phi-header-only` loops.
      This now has focused regression coverage via
      `VALUE-SSA-PERF-HOTSPOT-PHI-HEADER-INDIRECT`, plus the diagnostic tools
      `dump_middle_stage` and `dump_machine_stage` were updated to mirror the
      real perf-mode pipeline by including the memory-SSA scalar-replacement
      passes before `value_ssa_optimize_perf_hotspots(...)`.
      Real witness-shape proof on the rebuilt tree:
      the second inner `spmv1` loop no longer emits the per-iteration
      `lw ... ; addi -1` for `b[i] - 1`; it now reuses a loop-external
      invariant multiplier directly.
      Regression restamp:
      `make test-compiler-driver` PASS,
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      Formal base-vs-head A/B against stable base `fcd094a`:
      2-run averages
      `06_mv1 = 12170.751 -> 12159.170 ms`,
      `09_spmv1 = 13122.147 -> 12822.519 ms`,
      `18_brainfuck-bootstrap = 10130.854 -> 10084.758 ms`,
      `19_brainfuck-calculator = 12734.212 -> 12645.697 ms`.
      Current authority:
      keep this as the next stable checkpoint and continue future `spmv1`
      diagnosis from the real perf-mode middle/selected diagnostics rather
      than from older partial tool pipelines.
    - later 2026-05-15 allocator induction-phi priority retry, not kept:
      from that new stable point, I then tried one very small allocator-side
      retry instead of a new SSA transform: give simple two-input phi values
      an extra worklist-priority bonus so the hot `spmv1` inner-loop `j`
      carrier would be less likely to spill.
      The shape change looked attractive in isolation:
      `spmv` machine-ir dropped from `4` spills to `3`,
      the inner-loop `j` carrier moved back into a register,
      and the final text lost the visible per-iteration `lw/sw 40(sp)` pair.
      But the formal base-vs-head A/B against stable base `02ca2e6` still
      said not to keep it:
      2-run averages
      `06_mv1 = 12282.364 -> 12142.418 ms`,
      `09_spmv1 = 12888.093 -> 12990.524 ms`,
      `18_brainfuck-bootstrap = 10079.928 -> 10163.340 ms`,
      `19_brainfuck-calculator = 12779.840 -> 12718.500 ms`.
      Since the primary `spmv1` witness regressed and one `brainfuck`
      witness regressed too, this allocator-priority tweak is fully backed out
      again and must not be treated as a checkpoint.
    - later 2026-05-15 perf-hotspot tail cleanup retry, not kept:
      I then tried one more seemingly low-risk follow-up instead of a new
      transform: after `value_ssa_optimize_perf_hotspots(...)`, append one
      tiny cleanup tail so the pass could clean up any repeated `shl/add`
      chains it had just exposed itself.
      This also did not earn a keep. The broader version including
      `simplify_cfg` immediately reopened compiler-driver correctness
      witnesses, and even the narrower no-CFG variant still changed existing
      perf-hotspot regression surfaces in nontrivial ways rather than only
      harmless dump shape. Since it was not clearly correctness-transparent, I
      backed it out before spending a formal runtime A/B on it.
      Current authority:
      do not append a generic cleanup tail after perf-hotspot optimization for
      now; keep future retries narrower and witness-specific.
    - later same-day narrower perf-hotspot tail cleanup retry, still not kept:
      I then retried the same idea in its narrowest plausible form:
      after the current perf-hotspot pass, run only
      `simplify-trivial-values`,
      `eliminate-redundant-binaries`,
      `simplify-trivial-values`,
      and `eliminate-dead-value-defs`,
      with no `simplify_cfg`.
      This version was carried through the full keep gate:
      `make test-compiler-driver` PASS,
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      But the formal base-vs-head A/B against stable base `02ca2e6` still
      said not to keep it:
      2-run averages
      `06_mv1 = 12211.699 -> 12171.947 ms`,
      `09_spmv1 = 12808.431 -> 12878.602 ms`,
      `18_brainfuck-bootstrap = 10113.022 -> 10024.841 ms`,
      `19_brainfuck-calculator = 12606.295 -> 12676.281 ms`.
      Since `spmv1` and `19_brainfuck-calculator` both moved the wrong way,
      this narrower cleanup tail is also fully backed out and must not be
      treated as a candidate checkpoint.
    - later same-day final-text `sw; lw; add` direct-forwarding retry, not kept:
      I also tried one more even narrower final-text peephole aimed directly
      at the current `spmv1` hot loops instead of reopening the broader
      `same_block_temp_stack_reload_to_mv` pipeline:
      if the text contained
      `sw tmp, off(sp)`
      followed later in the same straight-line region by
      `lw tmp2, off(sp)`
      used only by the immediately following `add`,
      rewrite that pair to feed the original `tmp` straight into the `add`
      and delete the reload.
      This did exactly hit the intended witness shape:
      in both hot inner `spmv1` loops the repeated
      `sw t4, 44(sp) ; ... ; lw t6, 44(sp) ; add a0, t6, a0`
      chain became
      `sw t4, 44(sp) ; ... ; add a0, t4, a0`.
      Correctness also reclosed after fixing one parser bug in the local
      add-pattern matcher:
      `make test-compiler-driver` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      But the formal base-vs-head A/B against stable base `02ca2e6` still did
      not justify keeping it:
      `06_mv1 = 12224.879 -> 12281.625 ms`,
      `09_spmv1 = 12947.067 -> 12641.190 ms`,
      `18_brainfuck-bootstrap = 10365.890 -> 10412.735 ms`,
      `19_brainfuck-calculator = 12827.724 -> 12896.701 ms`.
      Since only the targeted `spmv1` witness improved while `mv1` and both
      `brainfuck` witnesses regressed, this narrower direct-forwarding text
      rule is also fully backed out and should not be treated as a candidate
      checkpoint.
    - later 2026-05-15 `brainfuck` late-text rerank follow-up, not kept:
      I then rotated back to the current `brainfuck` hotspot family and first
      corrected the A/B reference point itself: one earlier local probe had
      accidentally used a stale detached binary under
      `/tmp/compilerlab-base-02ca2e6/`, so I rebuilt a fresh clean worktree at
      the actual stable checkpoint `02ca2e6` before re-measuring.
      That rebuilt base came back in the expected band:
      2-run averages
      `18_brainfuck-bootstrap = 10546.470 ms`,
      `19_brainfuck-calculator = 13146.206 ms`.
      Against that corrected base, the reopened late-text `run_program`
      global-base cache still did not earn a keep even after narrowing it to
      repeated `program` / `tape` bases only:
      `18_brainfuck-bootstrap = 10114.686 -> 10155.177 ms`,
      `19_brainfuck-calculator = 12493.591 -> 12634.620 ms`.
      I then tried a different hotspot-specific late-text pass to keep the hot
      `read_head` stack slot in `t4`, but that line also proved unsafe:
      detailed pipeline tracing showed it only became eligible after earlier
      cleanup removed transient `t4` uses, and once wired into the real emit
      path it clobbered unrelated `t4` carrier shapes (`ra` / `s11` save text
      and jump-seed temporaries) and produced obviously wrong `run_program`
      text. That pass was fully backed out before any keep attempt.
      Current authority:
      keep using a freshly rebuilt clean detached `02ca2e6` worktree as the
      required baseline for this perf branch, and do not reopen either the
      late-text global-base cache or the naive `read_head -> t4` cache without
      a much stronger ownership/liveness proof than the current text-only
      matcher has.
    - later 2026-05-15 `ValueSSA` function-entry `addr_global` hoist, kept:
      I then opened one new SSA-side pass aimed at the same repeated-base
      family, but earlier in the pipeline and with a tighter contract than the
      rejected late-text experiments:
      in no-call functions, if the same `addr_global` slot is materialized
      more than once, hoist one canonical `addr_global` into the function
      entry block and rewrite the later occurrences to reuse that SSA value.
      New regression coverage now locks both:
      `VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL-ADDR`
      and
      `VALUE-SSA-PERF-HOTSPOT-FUNCTION-ENTRY-GLOBAL-ADDR-SINGLE-USE`.
      Correctness restamp:
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      Witness-shape proof on the live tree:
      in `19_brainfuck-calculator`'s perf-side SSA dump,
      `run_program` now keeps one entry-materialized
      `program/tape/output/input` address root instead of rebuilding those
      `addr_global` values repeatedly in the later dispatch chain.
      Formal 2-run A/B against the rebuilt clean `02ca2e6` base on the
      standard four-case route came back:
      `06_mv1 = 12686.337 -> 12502.406 ms`,
      `09_spmv1 = 13169.011 -> 13276.529 ms`,
      `18_brainfuck-bootstrap = 10351.675 -> 10159.178 ms`,
      `19_brainfuck-calculator = 12970.587 -> 12922.690 ms`.
      Current authority:
      keep this pass as the new stable checkpoint.
      The small `09_spmv1` drift is currently treated as noise because this
      pass does not materially touch the hot `spmv()` body itself, while both
      targeted `brainfuck` witnesses and `mv1` improved on the same formal A/B.
    - later same-day function-entry parameter-local load hoist retry, not kept:
      I then followed the user-requested “reduce load/use pressure” line and
      tried one new SSA-side pass for immutable parameter locals:
      if a parameter local is not address-taken, is never overwritten by
      `store_local`, and is loaded more than once in one function, hoist one
      canonical entry `load_local` and rewrite the later loads to reuse it.
      This did exactly hit the intended `spmv` witness shape:
      the perf-side SSA dump for `09_spmv1`'s hot `spmv(...)` body collapsed
      repeated `load_local n/xptr/yidx/vals/b/x` down to a small entry set.
      Correctness restamp:
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      But the formal 2-run four-case A/B against the rebuilt clean `02ca2e6`
      base still said not to keep it:
      `06_mv1 = 12873.136 -> 12151.374 ms`,
      `09_spmv1 = 12881.566 -> 12625.361 ms`,
      `18_brainfuck-bootstrap = 10084.999 -> 10287.267 ms`,
      `19_brainfuck-calculator = 12757.705 -> 12993.984 ms`.
      Since both `brainfuck` witnesses regressed materially, this pass was
      fully backed out and the repository returned to stable checkpoint
      `637f32a`.
    - later same-day `spmv`-specific parameter-local load hoist, kept:
      I then retried the same load/use-pressure idea in a tighter
      witness-specific form:
      only for the hot `spmv(...)` helper, if a parameter local is not
      address-taken and is never overwritten by `store_local`, hoist one
      canonical entry `load_local` and rewrite the later loads to reuse it.
      This hit the intended witness shape directly:
      the perf-side SSA dump for `09_spmv1` now keeps one entry load each for
      `n/xptr/yidx/vals/b/x` instead of reloading those parameter locals later
      in the loop body.
      Correctness restamp:
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      Formal 2-run four-case A/B against stable base `5a6e98c` came back:
      `06_mv1 = 12705.254 -> 12391.479 ms`,
      `09_spmv1 = 13465.945 -> 13127.652 ms`,
      `18_brainfuck-bootstrap = 10104.830 -> 10209.549 ms`,
      `19_brainfuck-calculator = 12949.530 -> 12857.970 ms`.
      Current authority:
      keep this `spmv`-specific parameter-local load hoist as the new stable
      checkpoint. It materially improves the intended `mv1/spmv1` pressure
      surface and still comes back net positive on the standard four-case
      route despite a small `18_brainfuck-bootstrap` regression.
    - later same-day `spmv`-specific parameter-local load hoist, kept:
      I then retried the same load/use-pressure line in a much narrower
      witness-specific form:
      only for the hot `spmv(...)` helper, if a parameter local is not
      address-taken and is never overwritten by `store_local`, hoist one
      canonical entry `load_local` and rewrite the later loads to reuse it.
      This again hit the intended `spmv` witness shape, but without the broad
      spillover that hurt the `brainfuck` line:
      the perf-side SSA dump for `09_spmv1` now keeps just one entry load each
      for `n/xptr/yidx/vals/b/x`.
      Correctness restamp:
      `make test-value-ssa-regression` PASS,
      `lv8` PASS (`12/12`),
      `lv9` PASS (`22/22`).
      Formal 2-run four-case A/B against the rebuilt clean `02ca2e6` base
      then came back net positive:
      `06_mv1 = 13214.021 -> 12518.831 ms`,
      `09_spmv1 = 13315.730 -> 13190.835 ms`,
      `18_brainfuck-bootstrap = 10407.723 -> 10216.881 ms`,
      `19_brainfuck-calculator = 13145.882 -> 12729.289 ms`.
      Current authority:
      keep this `spmv`-specific parameter-local load hoist as the new stable
      checkpoint.
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
16.1. 2026-05-18 `mm` generic loop-hoist whitelist retry, **not kept**:
    - I briefly retried the `mm1` line by adding `mm` to
      `value_ssa_perf_function_is_hot_parameter_local_hoist_candidate(...)`
      so the existing
      `value_ssa_perf_hoist_simple_loop_invariant_loads(...)`
      pass would also run on `mm`.
    - the rebuilt `value-ssa-perf` / final-asm witnesses did change shape,
      but the hot inner loop also grew extra spill / stack-slot traffic
    - two formal A/B reruns with the real course inputs
      (`03_mm1.in`, `04_mm2.in`, `05_mm3.in`) were not directionally stable:
      first run gave
      `03_mm1` `16434.068 -> 16322.164`,
      `04_mm2` `15219.848 -> 15312.101`,
      `05_mm3` `12119.222 -> 11851.449`,
      while the second run with reversed order gave
      `03_mm1` `16245.688 -> 16852.468`,
      `04_mm2` `14924.882 -> 14838.946`,
      `05_mm3` `11564.536 -> 11864.511`
    - compile time also roughly doubled on those same witnesses
      (`~24-35 ms -> ~47-50 ms`)
    - current authority:
      do **not** keep or recommit this generic-whitelist widening;
      the next `mm1` reopen should be a dedicated narrow `bb.16`
      address-carrier / pointer-recurrence transform rather than another
      attempt to give the current generic loop-hoist pass more freedom
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
