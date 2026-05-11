# Compiler Lab Next Steps

## Current Authority

- [AGENTS.md](/workspaces/compiler_lab/AGENTS.md) defines startup order, role boundaries, and ownership.
- `docs/ir-conventions.md` is working memory for current engineering facts and safety boundaries.
- `docs/NEXT_STEPS.md` is the current roadmap and stage-status authority.
- `docs/ir/LOWER_IR_DESIGN.md` is the current design authority for downstream/lower-IR planning.
- `docs/ssa/VALUE_SSA_DESIGN.md` is the current design authority for the next likely post-lower-IR step.
- `docs/backend/MACHINE_RUNTIME_PLAN.md` is the current design/staging authority for the checkpointed post-load backend sibling.
- `docs/backend/MACHINE_LAUNCH_PLAN.md` is the current design/staging authority for the just-checkpointed post-runtime backend sibling.
- `docs/backend/MACHINE_STEP_PLAN.md` is the current design/staging authority for the just-checkpointed post-launch backend sibling.
- `docs/backend/MACHINE_DECODE_PLAN.md` is the current design/staging authority for the just-checkpointed post-step backend sibling.
- `docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md` is the current design/staging authority for the just-checkpointed post-decode backend sibling.
- `docs/backend/MACHINE_INTERP_PLAN.md` is the current design/staging authority for the just-checkpointed post-payload backend sibling.
- `docs/backend/MACHINE_TRANSITION_PLAN.md` is the current design/staging authority for the just-checkpointed post-interp backend sibling.
- `docs/backend/MACHINE_STATE_PLAN.md` is the current design/staging authority for the just-checkpointed post-transition backend sibling.
- `docs/backend/MACHINE_MUTATION_PLAN.md` is the current design/staging authority for the just-checkpointed post-state backend sibling.
- `docs/backend/MACHINE_WRITEBACK_PLAN.md` is the current design/staging authority for the just-checkpointed post-mutation backend sibling.
- `docs/backend/MACHINE_COMMIT_PLAN.md` is the current design/staging authority for the just-checkpointed post-writeback backend sibling.
- `docs/backend/MACHINE_APPLY_PLAN.md` is the current design/staging authority for the just-checkpointed post-commit backend sibling.
- `docs/backend/MACHINE_OBSERVE_PLAN.md` is the current design/staging authority for the just-checkpointed post-apply backend sibling.
- `docs/backend/MACHINE_DELTA_PLAN.md` is the current design/staging authority for the just-checkpointed post-observe backend sibling.
- `docs/backend/MACHINE_TRACE_PLAN.md` is the current design/staging authority for the just-checkpointed post-delta backend sibling.
- `docs/backend/MACHINE_EVENT_PLAN.md` is the current design/staging authority for the just-checkpointed post-trace backend sibling.
- `docs/backend/MACHINE_OUTCOME_PLAN.md` is the current design/staging authority for the just-checkpointed post-event backend sibling.
- `docs/backend/MACHINE_HISTORY_PLAN.md` is the current design/staging authority for the just-checkpointed post-outcome backend sibling.
- `docs/backend/MACHINE_TIMELINE_PLAN.md` is the current design/staging authority for the just-checkpointed post-history backend sibling.
- `docs/backend/MACHINE_LOG_PLAN.md` is the current design/staging authority for the just-checkpointed post-timeline backend sibling.
- `docs/backend/MACHINE_JOURNAL_PLAN.md` is the current design/staging authority for the new post-log backend mainline.
- `docs/backend/MACHINE_LOAD_PLAN.md` is the current design/staging authority for the just-checkpointed post-exec loader-prep backend sibling.
- `docs/backend/MACHINE_EXEC_PLAN.md` is the current design/staging authority for the just-checkpointed post-image exec-prep backend sibling.
- `docs/backend/MACHINE_IMAGE_PLAN.md` is the current design/staging authority for the just-checkpointed post-ELF image-prep backend sibling.
- `docs/ssa/MEMORY_SSA_DESIGN.md` is the current design/staging authority for the checkpointed memory-SSA line.

## Quick Start Snapshot

- Read this section first after a restart; only then drop into the long log below.
- Current checkpoint status:
  - stable recovery checkpoint: **kept**
  - `03_sort1` / `03_sort3`: reclosed
  - `lava-test/lava_test/kmp.sy`: reclosed
  - `lv8`, `lv9`, `autotest -riscv`, `autotest -perf`: green
- Current active remaining pressure is mainly perf / timeout oriented:
  - public perf:
    - `03_sort2.sy -> RUN_TIMEOUT`
    - `shuffle2.sy -> RUN_TIMEOUT`
  - external/private perf timeout cluster:
    - `brainfuck-mandelbrot-nerf.sy`
    - `dead-code-elimination-2.sy`
    - `dead-code-elimination-3.sy`
    - `hoist-2.sy`
    - `hoist-3.sy`
    - `instruction-combining-2.sy`
    - `instruction-combining-3.sy`
    - `integer-divide-optimization-2.sy`
    - `integer-divide-optimization-3.sy`
- Timeout interpretation policy:
  - do **not** assume every local `RUN_TIMEOUT` is immediate proof of a compiler bug
  - first classify each timeout as either:
    1. likely local-environment-limited (memory / paging / host I/O heavy), or
    2. likely real compiler/runtime pressure
  - prefer static instruction count, opcode histograms, compile-time timings,
    and testcase trip-count reasoning before overfitting to one local timeout
- Current default next loop:
  1. keep the recovered correctness-closed surfaces green
  2. classify remaining timeout cases
  3. optimize the genuine pressure cases for fewer instructions / less hot repeated work / better compile time
  4. rerun focused correctness witnesses after each kept perf change

## Current Guidance

1. Milestones A-E are closed for active implementation tracking.
2. Canonical IR v1 is the stable post-semantic handoff surface.
3. Canonical-IR pass work is now maintenance-first: reopen it for confirmed bugs, concrete optimization needs, or real structural pressure, not speculative expansion.
4. Any lower IR must be introduced as serial lowering:
   `AST -> semantic -> canonical IR -> lower IR -> later backend/codegen`
5. Do not build multiple parallel AST-to-IR pipelines.
6. Lower-IR work is now checkpointed into maintenance-first mode: keep the current phase-2 boundary stable, and reopen active expansion only for confirmed bugs, new lowering features, or a concrete downstream consumer need.
7. The strict `lower_ir -> value_ssa` conversion stage is now checkpointed. Treat conversion itself as maintenance-first unless a new correctness bug appears.
8. `machine_elf` is now checkpoint-ready / maintenance-first unless a concrete
   external-ELF consumer need reopens it.
9. `memory_ssa` / `memory_ssa_pass` is now checkpoint-ready /
   maintenance-first by default: reopen it for confirmed bugs, explicit new
    downstream-consumer pressure, or a deliberately chosen post-checkpoint
    Memory-SSA expansion, not for speculative broadening.
10. Backend target-direction memory: the intended final target language / ISA
    for this repository is **RISC-V**. Current generic or preview profiles are
    still allowed as staging tools, but downstream backend planning should
    treat them as temporary compatibility surfaces rather than as a change of
    final target direction.
11. A concrete post-checkpoint backend reopening is now active inside the
    otherwise maintenance-first bytes/object/ELF stack: direct
    `machine_ir`-report profile-aware builds should prefer making
    `riscv32-preview` change real `.text` bytes and downstream
    object/relocation artifacts, not only later-stage ELF header or generic
    relocation-policy metadata.
12. Backend repository-layout memory: machine code is no longer stored as a
    flat forest of top-level `src/machine_*` / `tests/machine_*` directories.
    Prefer the grouped layout:
    - `src/machine/lowering/` and `tests/machine/lowering/`
    - `src/machine/object/` and `tests/machine/object/`
    - `src/machine/runtime/` and `tests/machine/runtime/`
    - `src/machine/observe/` and `tests/machine/observe/`
    with public headers under the flat include namespace `include/machine/*.h`.
13. Current backend priority is **RISC-V correctness before backend
    optimization**. In the reopened backend line, prefer first making
    `machine_select -> machine_bytes -> machine_reloc -> machine_elf`
    materially more honest for RISC-V over adding new backend optimization
    families.
14. Treat instruction selection completion and legalization/expansion as part
    of that correctness-first RISC-V line, not as optional polish. In
    practice this means work such as immediate materialization, call/global
    address formation, compare/branch shaping, and other multi-instruction
    lowering sequences should be scheduled before broader performance-driven
    backend tuning.
15. Distinguish backend optimization layers by urgency:
    - do now: target-correct selection cleanup, legalization, expansion, and
      small branch/layout canonicalization directly needed for RISC-V output
    - do later: peephole cleanup, better immediate-form choice, and other
      backend quality tuning once the first real RISC-V artifact is stable
    - do much later: true late machine scheduling / instruction ordering over
      already-stable target instructions
16. Loop-oriented transformations such as loop unrolling are currently **not**
    part of the active backend mainline. They should be treated as SSA-side
    optimization work for a later phase, most likely under `value_ssa_pass`,
    after the current RISC-V lowering/object pipeline is materially more real.
17. Course `lv8`, `lv9`, and `-perf` are no longer the default active
    implementation line for this round. Treat them as regression baselines
    that must stay green while the current mainline shifts to hidden-course
    compatibility and return-flow audit work.
18. The current front-end / semantic mainline is now split into two linked
    audit tracks:
    - default submission-mode compatibility: find and close hidden
      `CTE/AE/WA` families without depending on the strict all-paths-return
      gate
    - strict audit quality: keep `--enforce-all-paths-return-check`
      available, but continue shrinking false positives until that mode is
      trustworthy rather than merely optional
19. Third-party SysY testcase suites are now part of the active evidence
    surface, not only optional future ideas. When available locally or after
    checkout, prefer periodic `autotest -t <suite-dir> /workspaces/compiler_lab`
    sweeps to search for hidden-like failures, then minimize any discovered
    failures back into repository regressions.
20. Known acquisition seeds for that external-suite line should now be treated
    as working inputs instead of fuzzy memory:
    - course doc reference:
      `https://pku-minic.github.io/online-doc/#/misc-app-ref/environment?id=%E4%BD%BF%E7%94%A8%E5%85%B6%E4%BB%96%E6%B5%8B%E8%AF%95%E7%94%A8%E4%BE%8B`
    - `https://github.com/pku-minic/minic-test-cases-2021s`
    - `https://github.com/pku-minic/minic-test-cases-2021f`
    - `https://github.com/segviol/indigo/tree/develop/test_codes/upload`
    - `https://github.com/TrivialCompiler/TrivialCompiler/tree/master/custom_test`
    - `https://github.com/ustb-owl/lava-test`
    - `https://github.com/jokerwyt/sysy-testsuit-collection`
    Current policy is to prefer the two `pku-minic` testcase trees first
    because they are closest to the course environment, then broaden with the
    public GitHub suites if those do not reproduce the remaining
    hidden-course failures.
    A broader persistence follow-up is now also in place locally: the current
    external suite roots under `/tmp/sysy-suites/` are mirrored under
    `third_party/sysy-suites/` (without `.git` metadata) so they survive
    image rebuilds instead of depending only on `/tmp/sysy-suites/`. The
    mirrored `minic-test-cases-2021f/s/performance` directories now also
    carry synthesized `.out` oracle files, so those mirrored paths can be
    swept directly instead of returning `SKIP` for missing outputs.

## Current Active Slice

- Current multi-round implementation focus is now the following ordered line,
  and it should be treated as the default mainline until these items are
  materially closed:
  1. keep the post-allocator correctness checkpoint green while the hidden / default compatibility audit continues to shrink the remaining `CTE` / `AE` / `RE` tails
  2. continue the explicit performance reopen on the narrowed public tails:
     - `hoist-2` compile-time pressure in the allocator-heavy path
     - keep the older locally-tracked `03_sort2` / `shuffle2` runtime witnesses as useful references, but treat the fresh full-sweep public authority as higher priority when it disagrees
     - broader course-internal `-perf` pressure, not only one public hot case
     - slow or timing-sensitive third-party perf cases when they reproduce
       clearly enough to optimize against
     - compiler compile-time itself as an explicit optimization target, not
       only generated-code/runtime performance
     - any newly reproduced external-suite pressure that survives the
       normalized-oracle sweeps
     - current external-suite note: raw `autotest -riscv -t lava-test/lava_test`
       can still false-flag the `many_parameters*` / `register_alloc*` /
       `test` cluster because those `.out` files are `CRLF`-terminated, but
       the repository's normalized-oracle sweep now revalidates the suite as
       genuinely green again (`tools/sweep_sysy_suite.py ... lava_test`
       => `21/21 PASS`), so this cluster should no longer be tracked as a
       live compiler red point
  3. after that performance round settles, reopen explicit target-direction
     tuning:
     - converge the machine-register count toward the final RISC-V direction
       (`32` architectural registers; current staging policy still decides
       how many allocatable logical registers are exposed at each backend
       lane, but future work should stop treating the current small flat bank
       as the end state)
     - add safe/common optimization work that materially improves generated
       code or compile-time/runtime performance
     - only stage loop-oriented transforms such as loop unrolling with care
       and only after correctness and more structural backend/SSA tuning stay
       green
  4. after any optimization round, rerun the same full correctness sweep
     again over course and external suites
  5. only then treat the current round as ready to end / checkpoint
- Current progress snapshot for that ordered line:
  - strict whole-repo runtime-RE audit:
    **in progress / ~98%**
    - current front-most kept closure inside the generated-program runtime-RE
      audit is back on
      `src/machine/lowering/machine_ir/machine_ir_slot_cleanup.inc`
      (`known-slot inlineable tail folding` around the
      `/tmp/noarray_hidden2/case_001.c` witness)
    - latest reread/audit chunk after that closure has now explicitly walked
      the next downstream runtime-risk spine again:
      `src/machine/lowering/machine_select/machine_select_lower_control.inc`,
      `src/machine/lowering/machine_select/machine_select_cleanup.inc`,
      `src/machine/object/machine_bytes/machine_bytes.c`, and
      `src/compiler/compiler_driver.c`
      - result of this chunk: no fresh concrete generated-program runtime-RE
        bug is proven yet in that reread span, so the active mainline still
        stays on the `machine_ir` closure plus continued rotated witness
        hunting rather than a newly localized downstream tail reopen
    - current priority inside that audit remains **generated-program runtime
      RE**, not compiler-process crashes: keep checking late preview-text
      peepholes, final text/control-flow export, stack/call/address-formation
    - fresh 2026-05-10 stability note inside that same audit: the reopened
      public/hidden-adjacent `register_alloc` WA line is now reclosed again.
      Root cause was allocator spill-slot assignment reusing a split child's
      parent slot **without** rechecking interference against values already
      assigned to that slot, which let later rewrite rounds alias live spilled
      values. The kept fix now only inherits the parent slot when it is still
      interference-safe; otherwise the allocator falls back to the normal slot
      search. Rechecks after the fix: `make test-value-ssa-alloc` PASS,
      `make test-machine-ir` PASS, focused public witnesses
      `92_register_alloc.sy` PASS and `114_register_alloc.sy` PASS (with real
      `.in` inputs), plus rotated spill-conflict scans over
      `compiler2023/.../functional` (`100` cases; only the already-unsupported
      `95_float`/`96-99_matrix_*` parser tails fail to build) and
      `compiler2021/.../h_functional` (`37` cases) found **no** remaining
      same-slot interference conflicts in allocate+rewrite results
    - immediate follow-up hunt after that fix first seemed to find a new
      synthetic witness under `/tmp/runtime_re_spill_followup/`, but that
      line is now **disqualified as generator UB rather than kept as a real
      compiler bug**. `stress_spill_101.sy` was failing only because the
      generator allowed `global_size < array_size`, then passed
      `len=array_size` into helpers whose first array argument was a shorter
      global array (`g9[18]` with `len=24`), so later `a[idx0]` accesses
      legitimately walked out of bounds. The generator now rejects that shape
      directly (`global-size must be >= array-size`), and the revalidated
      valid follow-up batches are green again:
      `/tmp/runtime_re_spill_followup_valid/` -> `3/3 PASS`,
      `/tmp/runtime_re_spill_followup_valid2/` -> `6/6 PASS`
      wrong-code risks, adjacent handoff surfaces that can corrupt the
      emitted program, and earlier control/data-flow fact lowering when a
      concrete runtime witness points back into it
    - current compiler-side/final-export substage is now **~97%**:
      caller-save / call-span rereads are complete as a no-new-bug slice, the
      `machine_bytes -> compiler_driver` summary-handoff helpers are reclosed,
      and the latest kept closures are the preview-text memory-dependent
      reuse barriers (`stack_addr_reuse` materialized-slot overwrite plus the
      call-like barrier refresh for `stack_addr_reuse` /
      `repeated_indexed_addr_triples` /
      `repeated_indexed_addr_sequences`, and the newer
      `stack_addr_reuse` post-barrier later-reload guard); the active line is
      now effectively a blend of “finish the tiny remaining final-export tail”
      and “broaden direct generated-program runtime repro hunting,” because
      that source-level compiler-driver detail is nearly exhausted and the
      latest concrete closure came from a runtime repro that pointed back into
      one final preview-text corner case
    - broader generated-program runtime repro hunting is now **~99%**:
      the latest concrete closure came from a minimized post-loop local-fact
      witness (`for (; x && getint(); x = x - 1) { f = 1; } if (f) ...`),
      which re-opened `src/ir/ir_lower_stmt.inc` and closed an unsound
      `while`/`for` exit-fact family; the immediately adjacent follow-up then
      closed another canonical-IR family there too (`if/while/for` condition
      side effects such as `if (i = 1)` were being dropped whenever
      truthiness pruning picked a branch early), and one more adjacent
      no-else join family there as well (`if (a) i = 2; if (i == 1) ...`
      had been treating the then-path facts as unconditional at the join),
      and now one expression-level sibling too (ternary/logical value joins
      were letting one branch's mutated-local facts dominate the join)
      without regressing the earlier runtime witnesses; the newest kept
      downstream closure then moved one layer later and closed the
      `compiler2023 hidden_functional/34_multi_loop.sy` witness on the
      Value-SSA allocate+rewrite line instead of canonical IR
      by stopping block-local spill splitting from re-splitting a family that
      was already represented through spill locals from an earlier rewrite
      round; the newest adjacent closure after that then reclosed the
      remaining many-args nested-call branch-condition family too, this time
      by keeping the final allocation result synchronized with the already
      rewritten spill shape instead of letting a later reallocation silently
      switch the spilled value family underneath the rewritten program.
      That newer allocator/result-sync closure reclosed
      `/tmp/ma3002_reduce/P_postpred_bool.sy`, `many_args_hidden_like_3002.sy`,
      and `many_args_hidden_like_3003.sy`; the next downstream closure after
      that then reclosed the reopened `compiler2023 hidden_functional/34_multi_loop.sy`
      timeout on the machine-ir line by protecting branch-carried live-through
      values from phi-elimination register clobbering, and the widened
      `/tmp/many_args_hidden_like_batch` is now `60/60 PASS`, while keeping
      the rotated hidden-like batch green
      (`21_union_find`, `22_matrix_multiply`, `23_json`, `24_array_only`,
      `28_side_effect2`, `30_many_dimensions`, `31_many_indirections`,
      `32_many_params3`, `34_multi_loop` all PASS);
      this leaves the compiler-side/final-export tail still maintenance-first
      while the runtime hunt has now pushed one concrete fix into the deeper
      Value-SSA rewrite loop
      without regressing the
      already-green
      focused `compiler2023` `09_BFS.sy` / `10_DFS.sy` runtime witnesses;
      the latest widened runtime-facing subsets (`minic-test-cases-2021f`
      `functional` full `100/100`, `minic-test-cases-2021s`
      `functional` full `112/112`, `minic-test-cases-2021s` `array*` `6/6`,
      `minic-test-cases-2021s` `global*` `1/1`,
      `minic-test-cases-2021s/performance` full `18/18`,
      `compiler2021/2021初赛所有用例/functional` full `103/103`,
      `lava-test/cases` full `162/162`,
      `compiler2021/2021初赛所有用例/h_functional` full `37/37`,
      `lava-test/cases` `long*` `5/5`, `lava-test/cases` `nested*` `3/3`,
      `lava-test/cases` `global*` `1/1`, `lava-test/cases` `short*` `3/3`,
      `TrivialCompiler/custom_test` full `29/29`,
      `TrivialCompiler/custom_test` `many*` `4/4`,
      `compiler2021/function_test2020` `register*` `1/1`,
      `compiler2021/function_test2020` full `111/111`,
      `compiler2021/function_test2020` `many*` `3/3`,
      `compiler2021/function_test2020` `matrix*` `4/4`,
      `indigo/test_codes/functional_test` full `111/111`,
      `lava-test/lava_test` is now also reclassified as runtime-green under
      the wider heavy-case budget already tracked in thread memory: the
      default sweep still shows exactly the known timeout-class pair
      (`many_parameters10000.sy`, `register_alloc10000.sy`), but focused
      reruns at `--case-timeout 60` bring both back to green (`1/1`, `1/1`),
      `minic-test-cases-2021f/performance` currently behaves the same way for
      the checked runtime-heavy tail: the full suite shows one heavy-case
      timeout at the default budget (`19_brainfuck-calculator.c:
      RUN_TIMEOUT`), but a focused rerun at `--case-timeout 60` brings that
      case back to green (`1/1 PASS`),
      `compiler2021/performance_test2021-public` also now looks more like a
      timeout-budget classification surface than a fresh runtime-crash source:
      the full suite still shows several default-budget red points, but the
      newly checked neighboring cases `03_sort3.sy`, `conv1.sy`, `conv2.sy`,
      `median2.sy`, and `shuffle1.sy` all come back green at
      `--case-timeout 60`, which leaves the older `03_sort2.sy` /
      `shuffle2.sy` pair as the clearest still-open public perf pressure
      signal rather than proving a new RE family; both still time out even at
      `--case-timeout 120`, so they remain true open perf witnesses rather
      than mere default-budget artifacts,
      `compiler2021/performance_test2021-private` and
      `lava-test/performance_test2021` now also align with that same picture:
      full sweeps mostly reduce to the already-tracked timeout cluster
      (`brainfuck-mandelbrot-nerf`, `dead-code-elimination-2/3`, `floyd-2`,
      `hoist-2/3`, `instruction-combining-2/3`,
      `integer-divide-optimization-2/3`), while the private-only
      `instruction-combining-1.sy` compile-time red point also clears under
      `--case-timeout 60`; `brainfuck-mandelbrot-nerf.sy` now also clears at
      `--case-timeout 120`, while `hoist-2.sy` remains a timeout even at the
      wider budget and therefore stays a true open perf-pressure witness
      rather than a fresh runtime-RE lead,
      `compiler2021/performance_test2021-private` currently sits at `18/29`
      under the default budget and `lava-test/performance_test2021` at
      `19/29`, but both are dominated by that already-known timeout/perf
      cluster rather than a new crash family,
      local stage profiling on the three clearest still-open witnesses keeps
      the same classification intact: `03_sort2.sy` and `shuffle2.sy` compile
      in only a few milliseconds locally (so their red status still points at
      generated-program/runtime performance, not compiler compile-time), while
      `hoist-2.sy` remains compile-side and is still dominated by
      `machine_ir_report` (`~3.1s` out of `~3.24s` total in the current local
      profile probe),
      one last narrow optimization follow-up is now also kept on the still-open
      `03_sort2` line: `machine_select` same-block pure-call reuse now
      recognizes repeated call arguments whose source is a same-block
      `load_indirect(addr)` when the underlying address operand itself stays
      identical and uninvalidated. That removes one surviving repeated
      `getNumPos(...)` shape from the `radixSort` hot block in the
      `machine_select` dump, but it does **not** yet close the public runtime
      tail by itself (`03_sort2.sy` still times out under the current public
      budget). A second neighboring `machine_select` cleanup is now also kept:
      same-block repeated `load_indirect(addr)` can now forward across
      intervening proven-non-alias local/global stores by reusing the earlier
      load via a spill-backed copy. This trims one more layer from the open
      `03_sort2` path (the current quick histogram first moved `call` down to
      `11` from the earlier `12` in the final `.s` image). A third neighboring
      `machine_select` cleanup is now also kept on top of that line:
      repeated cacheable-pure internal calls may now reuse a same-callee
      predecessor call across a **unique predecessor edge** when the raw arg
      operands stay identical and uninvalidated across the edge. This removes
      the still-visible `getNumPos(...)` recomputation on the
      `radixSort bb.16 -> bb.17` path in `03_sort2`, and the rebuilt final
      `.s` quick histogram now shows `call` down to `10`. It still does **not**
      close the public tail outright, and the latest keep/revalidate round
      reconfirmed that this remains a **perf-only** witness rather than a
      fresh runtime-crash family: `make test` is green again, `autotest -riscv
      -s lv9 /workspaces/compiler_lab` is green again (`22/22`), a rotated
      external correctness slice
      (`compiler2021/公开用例与运行时库/function_test2021 --limit 30`) is green
      again (`30/30`), another rotated slice
      (`indigo/test_codes/functional_test --limit 25`) is green again
      (`25/25`), and focused public reruns at `--case-timeout 120` still leave
      exactly the same open pair (`03_sort2.sy -> RUN_TIMEOUT`,
      `shuffle2.sy -> RUN_TIMEOUT`).
      `sysy-testsuit-collection/lvX` is now also reclassified as runtime-green
      under the wider heavy-case budget already recorded in thread memory:
      the default sweep still shows exactly the known timeout-class trio
      (`many_parameters10000.c`, `matrix-1.c`, `register_alloc10000.c`), but
      focused reruns at `--case-timeout 60` bring those back to green
      (`1/1`, `2/2`, `1/1` respectively),
      `lava-test/cases` `search*` `4/4`,
      `TrivialCompiler/custom_test` `loop*` `5/5`,
      the extra complex-algorithm spot checks
      (`brainfk`, `dijkstra`, `max_flow`, `n_queens`), and the newer
      algorithmic spot checks (`kmp`, `hanoi`, `backpack`, `substr`,
      `percolation`, `calculator`, `color`, `exgcd`, `gcd`) are all green
      after the `094_long_code.c` closure, so the next useful runtime-hunt
      chunk should keep rotating to different external/runtime-heavy slices
      instead of repeatedly rerunning the same long/array witnesses
  - post-allocator correctness checkpoint:
    **complete / 100%**
    - 2026-05-08 ordered source reread follow-up: the current hidden/default
      compatibility line is now also doing a slow file-by-file reread through
      the post-lower-IR stack instead of only chasing testcase names. That
      reread has now reached `value_ssa`, and one concrete structural verifier
      hole is closed in the live tree: `VALUE_SSA_INSTR_CALL` now rejects the
      malformed shape `arg_count == 0 && args != NULL` instead of accepting it
      silently. `tests/value_ssa/value_ssa_regression_test.c` now carries a
      focused regression that mutates a valid sample program into exactly that
      bad call shape, and the kept local rechecks for this audit point are
      `make test-value-ssa-regression` PASS and
      `make test-value-ssa-verifier` PASS.
    - 2026-05-08 ordered `value_ssa_pass` reread follow-up: one real
      optimizer-soundness hole is now closed in the local-store cleanup line.
      `value_ssa_eliminate_unread_scalar_local_stores(...)`,
      `value_ssa_eliminate_redundant_stores(...)`, and
      `value_ssa_eliminate_dead_stores(...)` had been treating address-taken
      scalar locals too aggressively, which let a shape like
      `addr_local x ; store_local x, 7 ; load_indirect addr(x)` lose the
      defining store even though the memory write was still observed through
      the indirect load path. The live tree now collects function-local
      `addr_local` facts and disables those local-slot store cleanups for
      address-taken locals instead of pretending only direct `load_local`
      reads matter. Focused kept rechecks after this closure are
      `make test-value-ssa-regression` PASS and
      `make test-compiler-driver` PASS, and the regression suite now also
      locks both the direct dead-store repro and the default-conversion
      address-taken scalar local witness.
    - 2026-05-08 ordered `value_ssa_pass` reread follow-up 2: one matching
      global-memory soundness cluster is now also closed in the live tree.
      The generic global load-forward / redundant-store / dead-store line had
      been treating indirect memory steps too weakly, so shapes like
      `store_global g, 9 ; addr_global g ; store_indirect ... ; load_global g`
      or `store_global g, 9 ; load_global g ; addr_global g ; store_indirect
      ... ; store_global g, loaded` could still be misoptimized as if the
      indirect memory step had not touched the same global. The current tree
      now uses a lightweight address-root proof inside the global optimization
      passes: if an indirect store is proven rooted at a different slot (for
      example `addr_global arr` vs `g`), unrelated forwarding/redundant-store
      cleanup may still proceed; if the indirect root matches the same global
      or cannot be proven disjoint, the affected global state is
      conservatively invalidated. Dead-store cleanup now also treats
      `load_indirect` as a global observer barrier instead of pretending only
      direct `load_global` reads matter. Focused kept rechecks after this
      closure are `make test-value-ssa-regression` PASS and
      `make test-compiler-driver` PASS, and the regression suite now locks
      both the new indirect-store/load global barriers and the earlier
      non-alias witness (`arr` vs `g`).
    - 2026-05-08 ordered `value_ssa` / `simplify_cfg` reread follow-up: one
      small but real structural void-return hole is now closed too. Fresh
      `ValueSsaBasicBlock` entries had been leaving their `terminator` payload
      uninitialized, `value_ssa_block_set_void_return(...)` did not clear the
      stale `return_value` fields, and `value_ssa_compact_value_ids(...)`
      inside CFG simplification had been remapping `return_value` for every
    - 2026-05-09 fresh runtime-RE restart follow-up: strict whole-code audit
      has now advanced through the machine-side active submission path from
      `src/machine/lowering/machine_ir/*` and
      `src/machine/lowering/machine_select/*` into
      `src/machine/object/machine_bytes/*`, and then through the final
      compiler text-export path in `src/compiler/compiler_driver.c`.
      Current conclusion for this reread chunk is:
      - no new confirmed generated-program runtime-RE bug has been proven yet
        in the reread `machine_ir` / `machine_select` / `machine_bytes` /
        `compiler_driver` surfaces beyond the already-closed semantic
        truthiness fixes and the already-closed tail-call text peephole fix
      - one suspicious but currently unconfirmed/dead-looking compiler-driver
        caller-save helper path was noted for later recheck, but it does not
        currently look like the main active hidden-RE explanation because the
        guard path is not on the live default text-export route
      - current sequential audit boundary is now **machine runtime entry**:
        `machine_exec` / `machine_load` / `machine_runtime` / adjacent runtime
        build+query layers are being reread next before the remaining
        `observe/*` tail
      - kept local recheck after this reread chunk:
        `make test-compiler-driver` PASS
    - 2026-05-09 runtime/image reread follow-up: one concrete downstream
      runtime bug is now closed in
      `src/machine/runtime/machine_image/machine_image_build.inc`.
      The image builder had still been importing only `.text` from ELF and
      silently dropping `.sbss` / `.sdata`, which meant global-object data
      preserved by the bytes/object/reloc/ELF pipeline could disappear before
      `machine_exec` / `machine_load` / `machine_runtime`.
      The live tree now preserves those data sections as image segments too,
      keeps `.sbss` zero-filled instead of pretending it has file bytes, and
      locks the image layer with a focused
      `tests/machine/runtime/machine_image/machine_image_test.c` regression
      that checks `.text/.sbss/.sdata` segment presence plus `g/h` symbol
      placement and data bytes.
      Current sequential audit boundary is now:
      - runtime `image/load/exec/runtime/launch/step/decode/payload/interp`
        plus observe `delta/trace/event/outcome/history/timeline/log/journal`
        have all been reread
      - no second confirmed generated-program runtime-RE bug has been proven
        yet in the remaining runtime/observe wrapper layers after the image
        fix
      - kept local rechecks after this closure:
        `make test-machine-image` PASS
      - broader kept rechecks after the live patch:
        - `make test-compiler-driver` PASS
        - `make test` PASS
        - `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
        - rotated third-party sweeps:
          - `minic-test-cases-2021f/functional --limit 40` PASS (`40/40`)
          - `sysy-testsuit-collection/lvX --limit 40` PASS (`40/40`)
          - `lava-test/shortcircuit_test --limit 25` PASS (`1/1`; suite root only contains one case)
    - 2026-05-09 machine-bytes follow-up: one more concrete generated-code
      runtime-risk bug is now closed in `src/machine/object/machine_bytes/*`.
      The generic bytecode bridge had been accepting control-flow targets that
      do not fit the legacy encoding width and silently truncating them when
      writing jump / branch payload bytes. That made the generic byte artifact
      path capable of jumping to the wrong block when a function had more than
      16 blocks, which is exactly the sort of combination-only wrong-code /
      runtime-RE shape the user warned about. The live tree now rejects
      control targets that do not fit the encoded byte-width before the byte
      bridge is committed, and the regression suite now locks the generic
      `branch`-width rejection witness in `tests/machine/object/machine_bytes/`.
      Current kept rechecks after this closure are:
      `make test-machine-bytes` PASS,
      `make test` PASS,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`),
      `minic-test-cases-2021f/functional --limit 40` PASS (`40/40`), and
      `sysy-testsuit-collection/lvX --limit 40` PASS (`40/40`).
    - 2026-05-09 machine-bytes follow-up 2: the same generic bytecode
      contract line is now also rejecting non-representable immediate /
      descriptor payloads instead of silently truncating them. In the live
      tree, generic `materialize-imm`, `alu-imm`, `cmp-imm`,
      `store-*_imm`, `return-imm`, and call-argument descriptors must now fit
      the legacy byte widths explicitly before the byte bridge is accepted.
      That closes another combination-only runtime-risk class where a generic
      payload could previously be rounded down and later decoded into the
      wrong state or wrong control outcome. The regression suite now locks the
      generic immediate-width rejection witness in
      `tests/machine/object/machine_bytes/machine_bytes_test.c`.
      Current kept rechecks after this closure are:
      `make test-machine-bytes` PASS,
      `make test` PASS,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`),
      `minic-test-cases-2021f/functional --limit 40` PASS (`40/40`), and
      `sysy-testsuit-collection/lvX --limit 40` PASS (`40/40`).
    - 2026-05-09 verification follow-up: the post-fix validation set for the
      generic bytecode width hardening is now fully settled again after a
      temporary external-sweep environment interruption. The live evidence
      surface now includes:
      - `make test` PASS
      - `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`)
      - `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`)
      - `minic-test-cases-2021s/functional --limit 40` PASS (`40/40`)
      - `minic-test-cases-2021f/functional --limit 40` PASS (`40/40`)
      - `indigo/test_codes/functional_test --limit 40` PASS (`40/40`)
      - `sysy-testsuit-collection/lvX --limit 40` PASS (`40/40`)
      The temporary `Permission denied` seen during one intermediate sweep was
      an environment-side launch artifact; the direct `build/compiler` binary
      remained executable and the rerun completed green.
    - 2026-05-09 decode-chain follow-up: one more runtime-facing generic
      opcode recognition gap is now closed. `machine_decode` had still been
      stopping its generic op-tag range at `STORE_GLOBAL_IMM`, which meant the
      later `LOAD_INDIRECT` / `STORE_INDIRECT` opcodes from `machine_select`
      were silently treated as unknown in the runtime decode chain. The live
      tree now recognizes those two op tags, and `machine_payload_decode`
      now also treats them as known opcodes with zero payload bytes instead of
      accidentally dropping them into the unknown/default path. Focused local
      regressions now lock both indirect-memory tags in
      `tests/machine/runtime/machine_decode/machine_decode_test.c` and
      `tests/machine/runtime/machine_payload_decode/machine_payload_decode_test.c`.
      Current kept rechecks after this closure are:
      `make test-machine-decode` PASS,
      `make test-machine-payload-decode` PASS,
      `make test` PASS,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`),
      `minic-test-cases-2021s/functional --limit 20` PASS (`20/20`),
      `TrivialCompiler/custom_test --limit 20` PASS (`20/20`),
      `lava-test/cases --limit 20` PASS (`20/20`), and
      `minic-test-cases-2021f/functional --limit 40` PASS (`40/40`).
    - 2026-05-09 observe-tail reread follow-up: the strict whole-code audit
      has now also reread the remaining observation/report siblings from
      `machine_outcome` through `machine_journal` with the same
      runtime-RE/combinational-risk lens. Current conclusion for this chunk is
      still conservative: no new concrete generated-program runtime bug is
      confirmed from the observe-side exact/preview/blocked classification
      ladders, and the current blocked-control / blocked-unsupported /
      control-halt / value/local/global/call families appear to propagate
      consistently across `event -> outcome -> history -> timeline -> log ->
      journal`. The active line is therefore now effectively in endgame /
      maintenance-first audit mode rather than sitting on one obvious unread
      downstream hotspot.
    - 2026-05-09 maintenance-tail follow-up: a second pass through the
      `machine_state -> machine_writeback -> machine_commit -> machine_apply`
      bridge plus the detailed `machine_event/outcome/history/timeline/log/
      journal` verify/build/classification paths still did not surface a new
      concrete runtime-RE bug. Current authority for this tail chunk is:
      - the recently fixed generic bytecode / decode-chain issues remain the
        last confirmed downstream runtime-risk closures
      - the later observe-side exact/preview/blocked ladders remain internally
        consistent on the currently audited `call`, `value`, `local/global
        update`, `control-halt`, `blocked-control`, and
        `blocked-unsupported` families
      - no new long-running sweep is left in flight from this chunk
    - 2026-05-09 report/query reread follow-up: the strict audit has now also
      revisited the report/query surfaces around
      `machine_image_query_dump`, `machine_exec_report`, `machine_load_report`,
      `machine_runtime_report`, and `machine_launch_report`, together with a
      fresh pass over the connected `machine_observe/delta/trace` summaries.
      Current conclusion for this chunk is still “no new concrete runtime bug
      confirmed”: the entry/stack/filter/overview/report-artifact paths appear
      consistent with the already-fixed lower-level contracts, and no new
      enum-tail omission or report-side state-drop was proven from this reread.
    - 2026-05-09 machine-select sibling-tail follow-up: the strict whole-code
      audit has now also finished another full reread pass over the remaining
      `machine_select` detail siblings, specifically
      `machine_select_lower_control.inc`, `machine_select_query.inc`, and
      `machine_select_report.inc`, with the same generated-program runtime-RE
      lens plus malformed-artifact hardening checks. Two concrete closures are
      now recorded from that pass:
      - a real selected-control wrong-code bug is fixed: constant/materialized
        branch folding in `machine_select` and the later selected cleanup fold
        had still been deciding branch truthiness on raw host `long long`
        immediates instead of normalized 32-bit SysY integer semantics, so a
        value such as `4294967296` could be folded as true instead of the real
        false/zero branch case after normalization
      - the selected lower-report query/dump surface now also rejects missing
        function/filter/block-summary tables and report count/offset drift
        cleanly instead of trusting malformed backing tables during by-name
        lookups or direct report dump assembly
      - kept rechecks after this closure:
        `make test-machine-select` PASS,
        `make test` PASS,
        `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
        and `tools/sweep_sysy_suite.py --filter while third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`7/7`)
      - command-state note: no long-running sweep remains in flight; one first
        attempt to run the rotated third-party subset via `autotest --filter`
        used the wrong CLI surface and was immediately corrected to
        `tools/sweep_sysy_suite.py`, so it should not be treated as a compiler
        regression signal
    - 2026-05-09 machine-layout sibling follow-up: the strict whole-code audit
      has now advanced one ordered stage further into the full
      `src/machine/lowering/machine_layout/*` sibling set (`core/query/verify/
      dump/report/lower`). This chunk closed one more downstream-contract bug
      family plus the matching report trust gap:
      - verifier authority now also locks layout-stage opcode-family shape
        invariants that had still been drifting open: `load_local` /
        `load_global` and store-local/store-global families now require the
        matching slot kind, spill-result versus register-result call families
        now require the matching result operand kind, and `_IMM` compare/binary
        / compare-branch layout families now require rhs-immediate form rather
        than silently accepting malformed non-immediate payloads
      - report/query/dump hardening now also reaches the layout layer:
        malformed layout reports with missing function/filter/block-summary
        tables or count/offset drift now fail cleanly instead of being trusted
        during by-name query or dump assembly
      - kept rechecks after this closure:
        `make test-machine-layout` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`),
        and `tools/sweep_sysy_suite.py --filter if_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`8/8`)
      - current sequential audit boundary is now the next layout consumer
        siblings, namely `src/machine/lowering/machine_emit/*`; the next
        queued stage after that is `src/machine/lowering/machine_encode/*`
      - long-running command state: none
    - 2026-05-09 machine-emit sibling follow-up: the strict whole-code audit
      has now also covered the full `src/machine/lowering/machine_emit/*`
      sibling set (`core/query/verify/dump/report/lower`) instead of only the
      stage spine. This chunk closed three linked contract holes:
      - verifier authority now also locks emit-stage opcode-family invariants
        that downstream `machine_encode` / `machine_bytes` implicitly depend
        on: `_IMM` binary/compare families require rhs-immediate form;
        spill-result versus register-result call families require the matching
        result operand kind; and load/store local/global families require the
        matching slot kind
      - emit lower itself now frees/reinitializes the destination program
        before repopulating it, so lowering into a previously used
        `MachineEmitProgram` no longer risks stale state / leaked old blocks
      - emit report/query/dump now also reject malformed backing-table shapes
        or block-summary-offset drift instead of trusting them during by-name
        lookup or report dump assembly
      - kept rechecks after this closure:
        `make test-machine-emit` PASS,
        `make test` PASS,
        `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
        and `tools/sweep_sysy_suite.py --filter short third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`2/2`)
      - current sequential audit boundary is now the next ordered consumer
        stage `src/machine/lowering/machine_encode/*`
      - long-running command state: none
    - 2026-05-09 machine-encode sibling follow-up: the strict whole-code audit
      has now also covered the full `src/machine/lowering/machine_encode/*`
      sibling set (`core/query/verify/dump/report/lower`). This chunk closed
      one more consumer-boundary contract cluster:
      - verifier authority now also locks the embedded emitted-op and
        terminator-family invariants that later bytes/object consumers rely on:
        encoded blocks now reject missing op tables, wrong local/global slot
        kinds, wrong spill-result/register-result call families, malformed
        immediate-call families, and compare-branch-imm shapes that violate
        rhs-immediate form
      - report/query/dump hardening now also reaches the encode layer:
        refresh re-verifies the owned encoded program first, by-name/block-label
        query helpers now clear outputs more defensively, and report dump now
        rejects missing function/filter/block-summary tables plus summary-offset
        drift instead of trusting malformed report backing state
      - kept rechecks after this closure:
        `make test-machine-encode` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`),
        and `tools/sweep_sysy_suite.py --filter sort_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`7/7`)
      - command-state note: one first rotated third-party attempt used
        `--filter recursion`, which produced `NO_CASES` for this suite root and
        was immediately replaced with the real `sort_test` slice; this is only
        operator filtering noise, not a compiler regression
      - current sequential audit boundary is now back at the already-reread
        post-encode downstream consumers (`machine_bytes/object/...`), so the
        remaining whole-repo work is mainly endgame maintenance-first reread /
        cross-stage recheck rather than one obvious unread machine stage
      - long-running command state: none
    - 2026-05-10 post-encode downstream cross-stage follow-up: the endgame
      maintenance-first reread is now back inside the `machine_bytes` consumer
      surface plus its immediate object/reloc dependents. No new concrete
      generated-program runtime-RE bug has been proven in `machine_bytes` code
      itself from this chunk yet, but one real cross-stage maintenance drift
      was exposed and reclosed: stricter `machine_encode` verifier contracts
      made several downstream bytes/object/reloc fixtures invalid because those
      tests had been building malformed emit/encode inputs (missing register
      banks for register-bearing ops, missing local/global counts for slot
      users, etc.) and expecting the later consumer to be the first layer to
      notice. The live tree now re-legalizes those fixtures so downstream
      tests measure the intended bytes/object/reloc behaviors rather than
      tripping on already-illegal upstream metadata.
      - kept rechecks after this closure:
        `make test-machine-bytes` PASS,
        `make test-machine-object` PASS,
        `make test-machine-reloc` PASS,
        `make test` PASS,
        `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
        and `tools/sweep_sysy_suite.py --filter matrix third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`4/4`)
      - command-state note: no long-running sweep remains in flight
      - next sequential reread boundary: continue the same post-encode
        downstream cross-stage pass through `machine_object` / `machine_reloc`
        / `machine_elf` implementation siblings, now with the refreshed
        bytes/object/reloc tests green again
    - 2026-05-10 object/reloc sibling follow-up: the same post-encode
      downstream pass has now advanced into the implementation siblings
      `src/machine/object/machine_object/*` and
      `src/machine/object/machine_reloc/*`. This chunk closed one concrete
      report/query robustness cluster plus one object-file byte-copy hole:
      - `machine_object_file_copy_section_bytes(...)` now rejects non-empty
        sections whose byte table is missing instead of blindly `memcpy(...)`
        from a null pointer
      - object/reloc by-name report queries now clear outputs more
        defensively, and object/reloc report dumps now reject malformed report
        table drift (missing section/symbol/fixup/relocation summary tables or
        summary-count mismatches) instead of trusting those backing tables
      - kept rechecks after this closure:
        `make test-machine-object` PASS,
        `make test-machine-reloc` PASS,
        `make test` PASS,
        `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`),
        and `tools/sweep_sysy_suite.py --filter add third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`
        PASS (`5/5`)
      - current sequential audit boundary is now the next downstream sibling
        stage `src/machine/object/machine_elf/*`
      - long-running command state: none
      `RETURN` terminator even when `has_return_value == 0`. In practice the
      common zero-init path often hid this, but the contract was still wrong:
      stale payload bits from a void return must not participate in SSA
      remapping at all. The live tree now zero-initializes new block
      terminators, explicitly clears the void-return payload, and only remaps
      return SSA refs when the return actually carries a value. Focused kept
      rechecks after this closure are `make test-value-ssa-regression` PASS
      and `make test-value-ssa-verifier` PASS, and the regression suite now
      also locks a dedicated builder-side stale-payload witness for void
      returns.
    - 2026-05-08 ordered downstream machine-view reread follow-up: one public
      API semantics bug is now closed in `value_ssa_machine`. The machine-view
      builder had been storing `used_machine_register_ids` and register-group
      ids as allocator color / bank index rather than as the public
      `register_id` field from `ValueSsaMachineRegisterDesc`. Current flat/split
      banks happened to hide this because their ids matched indices, but a
      non-identity custom bank would query the wrong "machine register ids"
      back out of the view. The live tree now keeps public machine register ids
      in those query surfaces and maps them back into bank indices internally
      only when it actually needs the bank row. Focused kept rechecks after
      this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS, and the machine test suite now
      also locks a dedicated non-identity-register-id witness (`10`/`20`)
      so this id/index mix-up stays closed.
    - 2026-05-08 ordered downstream machine-protection reread follow-up: the
      same public-id/index confusion was also present one layer deeper in
      `value_ssa_machine_protection`. Register protection pressure had been
      aggregating agenda items by treating public `machine_register_id` as if
      it were a dense used-register array index, and preservation hints had
      still been checking caller-clobber policy by indexing
      `bank->registers[source_machine_register_id]` directly. With a
      non-identity bank this could misbucket pressure rows or skip a real
      caller-clobbered source register entirely. The live tree now aggregates
      pressure by used-register slot while preserving public machine register
      ids in the outward-facing items/queries, and preservation-hint source
      checks now map the public id back into the bank before consulting
      machine policy. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS, and the machine test suite now also
      locks dedicated non-identity register-id witnesses for both
      register-pressure lookup and preservation-hint source/suggested ids.
    - 2026-05-08 ordered allocator rewrite-loop reread follow-up: one
      multi-round spill-family stability hole is now closed in
      `value_ssa_alloc`. The allocate+rewrite loop had been recomputing each
      function's `spill_local_floor` from the **current** `local_count` at the
      start of every rewrite iteration. That let spill locals introduced by an
      earlier rewrite round gradually stop looking like spill locals in later
      rounds, which in turn risked misclassifying reload/store spill-family
      values as ordinary locals during the next rewriteability scan. The live
      tree now freezes those per-function floors from iteration 0 onward, so
      all spill locals created during the first and later rewrite rounds keep
      the same "spill-local" side of the boundary instead of drifting back
      into the ordinary-local range. Focused kept rechecks after this closure
      are `make test-value-ssa-alloc` PASS, `make test-value-ssa-machine`
      PASS, and `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered backend-text reread follow-up: one small but real
      RISC-V text peephole safety hole is now closed in
      `compiler_optimize_riscv_preview_call_arg_load_swaps(...)`. The old
      rewrite only rejected the dangerous case `second_base == a0` before
      collapsing
      `lw a1, ... ; lw a0, ... ; mv t5, a0 ; mv a0, a1 ; mv a1, t5 ; call`.
      That was too weak, because `second_base == a1` is also unsafe: in the
      original sequence the second load may depend on the just-loaded `a1`
      value as its address base, so swapping the two loads can change the
      memory address and silently change the call argument. The live tree now
      refuses this peephole when the second load is based on either `a0` or
      `a1`, and `tests/compiler/compiler_driver_test.c` now locks a focused
      text-level regression for the `second_base == a1` shape. Focused kept
      rechecks after this closure are `make test-compiler-driver` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-09 ordered hidden/default compatibility follow-up: the
      remaining `sysy-testsuit-collection/lvX/bigintsub.c` red point from the
      latest full sweep has now split into, and closed as, two real bugs.
      First, the final RISC-V text peephole
      `compiler_optimize_riscv_preview_zero_adds(...)` had been deleting a
      `li reg, 0` seed even when that zero-valued register was still needed in
      a later labeled block. In the live `compare()` repro this turned
      `BigIntLength[0]` address formation into `base + stale a1`, which
      flipped the sign decision for the whole testcase. The live tree now
      treats labels as control-flow barriers for that peephole's safety check,
      and `tests/compiler/compiler_driver_test.c` now locks a focused
      cross-label regression for the unsafe shape. Second, canonical-IR
      lowering had been sharing mutable-local const-state across unknown
      `if/else` siblings while lowering them serially, so a join like
      `if (x>0) flag=0; else flag=1; if (flag==1) ...` could incorrectly fold
      the post-join condition to always true from the later branch's facts.
      The live tree now clones branch-local lowering scope snapshots for
      unknown `if` arms and merges only the intersection of const facts at the
      join, and `tests/ir/ir_regression_test.c` now locks the focused
      `flag/putch(45)` repro. Focused kept rechecks after this closure are
      `make test-compiler-driver` PASS, `make test-ir-regression` PASS, the
      direct `flag_merge_putch2` CLI repro now lowering to a real conditional
      branch again, and
      `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/sysy-testsuit-collection/lvX --filter bigintsub --verbose-failures --case-timeout 60`
      => `1/1 PASS`.
    - 2026-05-08 ordered allocator-record reread follow-up: the allocation
      result object itself now has a complete `function_name` lifecycle again.
      `ValueSsaAllocationResult` had been declaring `char *function_name`
      without clearing it in `value_ssa_allocation_result_init(...)` or
      releasing it in `value_ssa_allocation_result_free(...)`, so any caller
      that chose to use that field would have been sitting on an uninitialized
      pointer / leak hazard. The live tree now null-initializes the field,
      frees it, and also preserves it through the allocation-result clone
      helper; `tests/value_ssa/value_ssa_alloc_test.c` now carries a small
      focused lifecycle regression for that field. Focused kept rechecks after
      this closure are `make test-value-ssa-alloc` PASS and
      `make test-value-ssa-machine` PASS.
    - 2026-05-09 ordered allocator reread follow-up: split-child edge reuse
      now refuses to treat an ordinary branch arm as reusable just because it
      ends in `mov ...; jmp successor`. Reuse now requires the candidate block
      to be a genuine unique-predecessor split child of the same branch block,
      and the focused allocator regression now locks the no-accidental-reuse
      case.
    - 2026-05-08 ordered machine-dump reread follow-up: one small but real
      crash-proofing hole is now closed in `value_ssa_machine`'s dump layer.
      `value_ssa_dump_function_machine_allocation_view(...)` and
      `value_ssa_dump_program_machine_allocation_view(...)` had been assuming
      their artifact arrays were structurally present whenever the associated
      counts were nonzero, which meant a malformed in-memory view could reach
      null dereferences while formatting debug output instead of failing
      cleanly with a verifier-style contract error. The live tree now checks
      the key count/array invariants for function/program machine dump
      artifacts up front and rejects malformed views with explicit machine
      errors before any array walk begins. `tests/value_ssa/value_ssa_machine_test.c`
      now locks both malformed-function-view and malformed-program-view dump
      regressions, and focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine call-clobber dump reread follow-up: the same
      crash-proofing line has now also reached `value_ssa_machine_call_clobber`.
      `value_ssa_dump_machine_call_clobber_risk_report(...)` and
      `value_ssa_dump_program_machine_call_clobber_risk_report(...)` had still
      been trusting `item_count` / `function_count` / filter-count metadata
      without first checking that the backing arrays were present, so malformed
      in-memory report artifacts could still turn a debug dump into a null
      dereference instead of a clean contract failure. The live tree now
      validates those machine call-clobber dump artifacts up front and rejects
      malformed reports with explicit errors before any iteration begins.
      `tests/value_ssa/value_ssa_machine_test.c` now also locks malformed
      single-report and malformed program-report dump regressions, and focused
      kept rechecks after this closure are `make test-value-ssa-machine` PASS
      and `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine protection/planning dump reread follow-up:
      the same dump-side crash-proofing line has now also been extended across
      the downstream `value_ssa_machine_protection` report family. The machine
      protection agenda, program protection report, register-pressure report,
      preservation-hint report, and final planning report dumps had still been
      assuming their nested `function_views` / `items` / filter arrays were
      structurally present whenever the corresponding counts were nonzero, so a
      malformed in-memory artifact could still turn later debug/report dumping
      into a null dereference. The live tree now validates those top-level dump
      contracts before any iteration begins and rejects malformed artifacts
      with explicit machine errors instead of walking bad pointers. The machine
      test suite now also locks malformed dump regressions for protection
      agenda, protection report, pressure report, preservation-hint report,
      and planning report artifacts. Focused kept rechecks after this closure
      are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine query-surface reread follow-up: one small
      but real output-parameter hygiene hole is now closed in the public
      register-bank query helpers. `value_ssa_machine_register_bank_get_register(...)`
      and `value_ssa_machine_register_bank_find_register_by_name(...)` had
      been returning failure without first clearing their output pointer/index
      slots, which meant a caller reusing old storage could accidentally keep a
      stale successful lookup result after a failed query. The live tree now
      nulls/zeros those outputs on entry before checking the lookup contract,
      and `tests/value_ssa/value_ssa_machine_test.c` now locks focused failure-
      path regressions for both helpers. Focused kept rechecks after this
      closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine summary-query reread follow-up: the same
      output-parameter hygiene line also reached the top-level machine summary
      helpers. The public summary queries for call-clobber risk,
      protection agenda/report, register-pressure report, hotspot report,
      preservation-hint report, and planning report now clear their output
      slots up front before validating the report pointer, so a failed lookup
      or null report can no longer leave stale counts behind in caller-owned
      storage. The machine regression suite now also locks a focused null-
      report output-clearing witness for those summary surfaces. Focused kept
      rechecks after this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine allocation-query reread follow-up: the same
      failure-path hygiene line now also covers the earlier
      `value_ssa_machine_query` allocation surfaces. The register-bank summary,
      function/program machine-allocation summaries, program-level allocation
      function lookup, and allocate+rewrite machine-report summary/stat helpers
      had still been returning failure without first clearing all caller-owned
      outputs, so stale counts or stale function-view pointers could survive a
      failed query. The live tree now zeros/nulls those public outputs before
      validating the query contract, and the machine regression suite now also
      locks focused failure-path witnesses for those allocation-query helpers.
      Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine placement-query reread follow-up: one last
      same-family API hygiene hole is now also closed on the direct placement
      surface. `value_ssa_function_machine_allocation_view_get_placement(...)`
      had still been returning failure without first nulling `out_placement`,
      which meant a caller reusing old storage could accidentally retain a
      stale placement pointer after a failed lookup. The live tree now clears
      that output slot on entry, and the machine regression suite now locks a
      dedicated failed-placement-query witness. Focused kept rechecks after
      this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine local-summary reread follow-up: the same
      failure-path hygiene line now also reached one remaining lower-level
      local summary helper under `value_ssa_machine_protection`.
      `value_ssa_machine_register_protection_pressure_view_get_summary(...)`
      now clears its output counters before validating the input view pointer,
      so a failed summary query can no longer leak stale pressure totals back
      to the caller. The machine regression suite now also locks a focused
      null-view summary witness for that helper. Focused kept rechecks after
      this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine call-clobber query reread follow-up: one
      same-family API hygiene / malformed-artifact hole is now also closed on
      the `value_ssa_machine_call_clobber` query surface. The local/program
      call-clobber summary and filter helpers had still been trusting malformed
      in-memory reports, and `get_functions_with_risky_values(...)` could still
      hand a stale array pointer back to the caller even when the risky-function
      count was zero. The live tree now rejects malformed call-clobber report
      artifacts cleanly on the public query path, and the zero-count risky
      filter now preserves the same "count zero => output pointer NULL" contract
      as the sibling machine-allocation filters. The machine regression suite
      now also locks both the malformed-report failure path and the zero-count
      risky-filter pointer-clear witness. Focused kept rechecks after this
      closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine protection query reread follow-up: the same
      malformed-artifact contract now also reaches the downstream
      `value_ssa_machine_protection` query family. The agenda, program
      protection, register-pressure, program register-pressure, hotspot report,
      preservation-hint report, and planning report query helpers now reject
      malformed backing artifacts instead of walking missing arrays or
      returning success on structurally broken reports. The machine regression
      suite now also locks focused malformed-report / failure-path witnesses
      for the affected protection/query surfaces. Focused kept rechecks after
      this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine function-view contract reread follow-up: the
      same malformed-artifact hardening now also reaches two remaining
      function-level machine views under `value_ssa_machine_protection`.
      Hotspot views and preservation-hint views now reject stale payload
      shapes such as "no hotspot but leftover hotspot item counts" and
      "no hint but leftover candidate/protected-count payload" instead of
      quietly reporting success on builder-impossible in-memory artifacts.
      The machine regression suite now also locks focused malformed
      hotspot-view and hint-view summary/candidate failure-path witnesses.
      Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine planning contract reread follow-up: one more
      outer/inner contract gap is now closed on the `planning` surface.
      Function/program planning queries and dumps had still only been checking
      the outer planning shell, so malformed nested pressure/hotspot/hint
      subviews could still slip through an apparently valid planning view or
      report. The live tree now validates those nested machine subviews before
      planning summaries/dumps succeed, and the machine regression suite now
      also locks a focused malformed planning-view summary failure-path
      witness. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine nested-report reread follow-up: the same
      outer/inner contract hardening has now been extended back through the
      remaining program-level machine report families. Program call-clobber,
      protection, register-pressure, hotspot, and preservation-hint reports
      now validate their nested function views too, so a report no longer
      counts as healthy if one of its embedded per-function artifacts is
      already structurally broken. The machine regression suite now also locks
      focused malformed nested-report witnesses for those surfaces. Focused
      kept rechecks after this closure are `make test-value-ssa-machine` PASS
      and `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine core-builder reread follow-up: one lower-
      level malformed-bank crash path is now also closed in
      `value_ssa_machine_core`. Function/program machine-allocation view
      builders and the internal register-id lookup helper had still been
      assuming that `bank->register_count > 0` implied `bank->registers != NULL`,
      so a structurally broken machine bank could reach direct
      `bank->registers[...]` access during machine-view construction instead of
      failing cleanly at the builder boundary. The live tree now rejects that
      malformed-bank shape up front, and the machine regression suite now also
      locks focused malformed-bank builder witnesses for both function and
      program machine-view construction. Focused kept rechecks after this
      closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine bank-identity reread follow-up: the same
      lower-level machine-view construction line now also rejects duplicate
      public `register_id` values inside one bank. Those ids are used as
      stable lookup keys throughout the machine-view / pressure / preservation
      / planning surfaces, so duplicate ids could silently collapse register
      identity and misbucket later group/query results. The live tree now
      treats duplicate register ids as malformed bank input for function/program
      machine-view construction, and the machine regression suite now also
      locks focused duplicate-id malformed-bank witnesses for both builders.
      Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine allocation-query contract reread follow-up:
      one more same-family malformed-artifact hole is now closed on the raw
      machine-allocation query surface itself. Helpers such as
      `get_machine_colored_values(...)`, `get_spilled_values(...)`,
      `get_caller_clobbered_values(...)`, and `get_used_machine_registers(...)`
      had still been returning success even when their exposed counts were
      nonzero but the corresponding backing arrays were missing. The live tree
      now rejects those malformed allocation-view shapes cleanly on the public
      query path, and the machine regression suite now also locks focused
      malformed allocation-query failure-path witnesses. Focused kept rechecks
      after this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine bank-dump contract reread follow-up: the
      same malformed-bank hardening now also reaches the textual dump side.
      `value_ssa_dump_machine_register_bank(...)` had still been trusting
      broken bank artifacts, so shapes like `register_count > 0 && registers ==
      NULL` or duplicate public `register_id` rows could still reach the dump
      formatter instead of being rejected at the machine boundary. The live
      tree now validates machine-bank dump artifacts up front, and the machine
      regression suite now also locks focused malformed-bank and duplicate-id
      dump failure witnesses. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine report-query reread follow-up: the same
      nested-artifact hardening now also reaches the
      `allocate_rewrite_machine_report` query surface. The report-level
      `get_program_view(...)`, `get_function_view(...)`, and summary helpers
      had still been trusting the embedded machine `program view` even when
      that nested artifact was already structurally broken, so report queries
      could still return success on malformed in-memory reports. The live tree
      now validates the nested machine-allocation view before those report
      queries succeed, and the machine regression suite now also locks a
      focused malformed machine-report query witness. Focused kept rechecks
      after this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-09 ordered machine allocation nested-query reread follow-up:
      the same nested-artifact rule is now also applied one level lower on the
      raw machine-allocation query surface. Program allocation summaries and
      by-name lookups now reject a malformed outer `program view` when any
      embedded per-function machine-allocation view is already structurally
      broken, instead of reporting success on a half-valid shell. The machine
      regression suite now also locks a focused malformed nested program-
      allocation query witness. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-09 ordered machine report-dump error-propagation follow-up: one
      small but real diagnostics bug is now also closed on the allocate+rewrite
      machine-report dump wrapper. `value_ssa_dump_allocate_rewrite_machine_
      report_artifact(...)` had still been overwriting downstream malformed-
      artifact failures with its own generic `VALUE-SSA-MACHINE-026` "out of
      memory" code, which hid the real nested machine-view error source. The
      live tree now preserves the underlying nested dump/query error instead of
      flattening everything into the wrapper's generic OOM code, and the
      machine regression suite now also locks a focused malformed machine-
      report dump witness for that propagation path. Focused kept rechecks
      after this closure are `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-09 hidden/default runtime-global-init follow-up: one real
      hidden-only compatibility seam is now materially narrowed across the
      canonical-IR, lower-IR, and `lower_ir -> value_ssa` boundaries instead
      of staying as a hidden probe only. `__global.init` is no longer
      verifier-forced to stay single-block in canonical IR or lower IR, while
      `__program.init` keeps the older strict one-block startup-helper
      contract; runtime global-init finalization now also seals any remaining
      unterminated helper block with `ret 0` after branchy/logical initializer
      lowering; and lower-IR live-in analysis now counts `load_indirect` /
      `store_indirect` temp uses so join temps that are only consumed through
      indirect memory still receive the needed Value-SSA phi. Focused kept
      rechecks after this closure are:
      `make test-ir-regression` PASS,
      `make test-ir-verifier` PASS,
      `make test-lower-ir-regression` PASS,
      `make test-lower-ir-verifier` PASS,
      `make test-value-ssa-regression` PASS,
      plus repository-local runtime-global-init probes under
      `/tmp/runtime_init_probes` now pass `4/4` through the full
      `build/compiler -> clang -> qemu-riscv32-static` path. Current
      authority is therefore that branchy top-level initializer shapes are no
      longer blocked by the older single-block helper assumption or by the
      indirect-memory live-in hole, and the remaining hidden `RE` line has
      narrowed back toward other still-unreproduced families.
    - 2026-05-09 hidden/default runtime-global-init follow-up 2: that same
      helper-growth line now also closes the next real source-level reopen
      that only appeared once the audit started chaining *multiple* runtime
      globals through one helper. If an earlier runtime initializer used
      short-circuit / branchy control flow, `ir_lower` had still been
      hardcoding later initializer emission back into `__global.init` block
      `bb.0`, and `ir_ensure_runtime_global_init_function(...)` was still
      treating any already-multi-block helper as if it must have been a
      user-defined illegal reserved function. The live tree now rejects
      reserved helper names directly at the AST external-function boundary,
      while subsequent runtime-global initializer emission appends to the
      last still-unterminated helper block instead of assuming `bb.0` stays
      open forever. Focused kept rechecks after this closure are
      `make test-ir-regression` PASS,
      `make test-lower-ir-regression` PASS,
      plus the hidden-like end-to-end repro
      `/tmp/hid_repro_case03/03_runtime_global_short_circuit_dep.sy`
      now passes through the full `autotest -riscv` path instead of failing
      with `IR-LOWER-024`. Current authority is therefore that
      "branchy runtime global init followed by later runtime globals" is no
      longer an open hidden/default compile blocker.
    - 2026-05-09 hidden/default array-initializer follow-up: one real
      nested-brace lowering hole is now also closed in the local/global array
      initializer expansion helper itself. In the `rank == 1` path,
      `ir_expand_array_initializer_from_items(...)` had still been letting a
      nested scalar list such as `{{1,2},3}` advance the outer element cursor
      by only one slot even though the nested list had already filled multiple
      scalar positions; that silently overwrote earlier elements (`{1,2},3`
      lowering like `{1,3,0,0}`) and, near the tail of the array, also left a
      real out-of-bounds write risk because the nested recursion was still
      being called with the full original 1D extent instead of the remaining
      element capacity. The live tree now bounds that nested recursion by the
      remaining 1D capacity and advances the outer cursor by the number of
      scalar slots actually consumed (with empty `{}` still conservatively
      consuming one zero-filled slot). Focused kept rechecks after this
      closure are `make test-ir-regression` PASS,
      `make test-lower-ir-regression` PASS,
      `make test-compiler-driver` PASS, and the direct end-to-end repro
      `/tmp/brace_mix_repro/brace_mix.c` now passes `autotest -riscv` with the
      corrected result instead of the earlier wrong-array layout. Current
      authority is therefore that 1D array initializer mixtures of nested
      brace groups plus later scalar items are no longer an open hidden
      compatibility risk.
    - 2026-05-09 hidden/default evidence-surface expansion follow-up: after
      closing the brace-mix bug, the next round did not immediately expose a
      second real failure, but it materially widened the confidence surface on
      the still-open hidden `RE` lines. The live tree now has fresh green
      evidence on:
      - runtime/local brace-mix follow-ups (`/tmp/brace_runtime_followups`
        => `4/4 PASS`)
      - deeper 3D / nested-partial array initialization probes
        (`/tmp/deeper_array_probes` => `6/6 PASS`)
      - a public near-hidden pack containing `short_circuit1`,
        `many_global_var`, `hanoi`, `long_array`, `const_array_defn`,
        `nested_calls`, and `nested_calls2`
        (`/tmp/near_hidden_pack` => `7/7 PASS`)
      - parameter / many-argument call near-neighbors including
        `func_param_arr` and `many_params1/2/3`
      - a generated defined-semantics differential batch over
        `global/array + short-circuit + helper side effects + loops`
        (`/tmp/random_defined_diff` => `40/40 PASS`)
      - a fully ASan-rebuilt compiler tree under `asanbuild/`, with focused
        compile smoke on the most suspicious hidden-like sources also passing
        without sanitizer complaints
      Current authority is therefore that the remaining hidden `RE` tail is
      increasingly unlikely to be a broad array-init / short-circuit / nested-
      call mainline bug, and more likely to be a much narrower still-
      unreproduced edge combination.
    - 2026-05-09 hidden/default machine-select follow-up: one further
      downstream liveness hole is now also closed on the branch-shaping side.
      `machine_select_lower_control` had been deciding whether a materialized
      boolean / compare result was live out of the source block without
      counting later `load_indirect` / `store_indirect` uses in successor
      blocks, so branch lowering could incorrectly fold away a compare result
      that was still consumed through indirect memory. The live tree now
      counts those indirect-memory uses in the machine-IR live-out probe, and
      the machine-select regression suite now locks a focused
      compare-branch-live-out-via-`store_indirect` witness. Focused kept
      rechecks after this closure are `make test-machine-select` PASS plus the
      earlier hidden-ish array/short-circuit probe bundles remaining green.
    - 2026-05-09 hidden/default compiler-driver peephole follow-up: one real
      text-export soundness hole is now also closed in the repeated indexed
      address reuse line. The final RISC-V preview peepholes for
      `repeated_indexed_addr_triples` / `repeated_indexed_addr_sequences` had
      been treating repeated `slli; lw; add` address formation as reusable
      across a middle region that only avoided some register redefinitions,
      but they were not invalidating on an intervening overwrite of the stack
      slot / loaded base memory itself, and the generic sequence form also was
      not invalidating on a redefinition of the load-base register. The live
      tree now keeps those repeated address recomputations when the relevant
      memory slot or base register changes, and the compiler-driver regression
      suite now locks three focused text-level witnesses for:
      repeated stack-slot base overwrite,
      repeated generic base-memory overwrite,
      and repeated generic base-register redefinition.
      Focused kept rechecks after this closure are `make test-compiler-driver`
      PASS.
    - 2026-05-09 hidden/default compiler-driver peephole follow-up 2: that
      same repeated-indexed-address line now also guards the stack-materialized
      overwrite variant instead of only direct `sw ..., offset(sp)` writes.
      When the repeated base load is really coming from a stack slot, an
      intervening two-line store like `addi t5, sp, K ; sw ..., 0(t5)` now
      also invalidates reuse for both the stack-slot triple form and the more
      generic repeated indexed sequence form, instead of letting the peephole
      silently treat the loaded stack base as unchanged. The compiler-driver
      regression suite now also locks focused text-level witnesses for those
      materialized-stack-slot overwrite shapes. Focused kept rechecks after
      this closure are still `make test-compiler-driver` PASS.
    - 2026-05-09 hidden/default compiler-driver peephole follow-up 3: one
      related stack-held-base soundness hole is now also closed in the
      `indexed_local_base_offsets` text fold. That peephole had been replacing
      `lw tN, K(sp)` loaded base pointers with direct `sp + K` addressing
      inside a later indexed local-base sequence, but it was not stopping when
      the same stack slot `K(sp)` got overwritten in between, either through a
      direct `sw ..., K(sp)` or through the materialized-address form
      `addi/mv tmp, sp, K ; sw ..., 0(tmp)`. The live tree now treats both
      shapes as invalidation barriers for that fold, and the compiler-driver
      regression suite now locks focused text-level witnesses for both direct
      and materialized stack-slot overwrite cases. Focused kept rechecks after
      this closure remain `make test-compiler-driver` PASS.
    - 2026-05-09 hidden/default compiler-driver peephole follow-up 4: two
      smaller constant-fold text peepholes are now also aligned with the same
      cross-label safety rule that the zero-add line already learned earlier.
      `mul_by_four` and `sub_by_one` had still been deleting the `li 4` / `li
      1` materialization whenever the constant register was not reused before
      redefinition *inside the current straight-line region*, but unlike the
      newer zero-add line they were still letting a later label hide a
      downstream use of that same constant register. The live tree now treats
      “needed past label before redefinition” as an invalidation barrier for
      both folds, and the compiler-driver regression suite now locks focused
      text-level witnesses for the cross-label `li 4 ; mul ...` and `li 1 ;
      sub ...` shapes. Focused kept rechecks after this closure remain
      `make test-compiler-driver` PASS.
    - 2026-05-09 hidden/default compiler-driver peephole follow-up 5: two
      stack/local-base folds are now also aligned with that same cross-label
      conservatism instead of remaining straight-line-only. `stack_addr_reuse`
      no longer deletes the original stack-address materialization when that
      temporary register may still be needed past a later label, and
      `indexed_local_base_offsets` likewise no longer drops the loaded
      base-register materialization when that base register may still be
      needed after a later label. The compiler-driver regression suite now
      also locks focused text-level witnesses for both cross-label shapes.
      Focused kept rechecks after this closure remain
      `make test-compiler-driver` PASS.
    - 2026-05-08 ordered machine malformed-bank reread follow-up: one small
      but real crash-proofing hole is now also closed on the machine register-
      bank query surface itself. `value_ssa_machine_register_bank_get_summary(...)`
      and `value_ssa_machine_register_bank_find_register_by_name(...)` had
      still been assuming that `register_count > 0` implied `registers != NULL`,
      so a malformed in-memory bank artifact could reach a null dereference
      during summary or name-based lookup instead of failing cleanly. The live
      tree now rejects that malformed-bank shape up front, and the machine
      regression suite now also locks dedicated malformed-bank summary and name-
      lookup failure witnesses. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 ordered machine hotspot/planning dump reread follow-up: the
      same dump-side crash-proofing line has now also been extended to the last
      two downstream report surfaces that had still been trusting their top-
      level artifact wiring too much. `value_ssa_dump_program_machine_register_
      protection_hotspot_report(...)` had still been iterating
      `function_views` without first rejecting the malformed shape
      `function_count > 0 && function_views == NULL`, and
      `value_ssa_dump_function_machine_planning_view(...)` had still been
      assuming its nested pressure/hint sub-artifacts were structurally present
      whenever their counts implied real contents. The live tree now rejects
      those malformed hotspot/planning artifact shapes up front instead of
      letting debug/report dumping walk bad pointers, and
      `tests/value_ssa/value_ssa_machine_test.c` now also locks focused
      malformed hotspot-report and malformed function-planning dump
      regressions. Focused kept rechecks after this closure are
      `make test-value-ssa-machine` PASS and
      `make test-value-ssa-regression` PASS.
    - 2026-05-08 hidden-OJ follow-up: the latest course submission evidence
      has reopened a concrete hidden/default compatibility cluster that should
      now be treated as active plan memory rather than as vague future risk.
      Current hidden-only red points reported from OJ are:
      - likely shared segfault / array-object / global-object family (`RE`):
        `05_global_arr_init`, `06_long_array`, `08_arr_access`,
        `09_const_arr_read`, `14_short_circuit1`, `17_func_expr2`,
        `18_global_var2`, `27_bfs2`, `32_global_1darr3`,
        `33_global_2darr3`, `34_zero_init3`
      - likely dead-loop / control-flow family (`RTLE`, treat as probable
        semantic loop bugs before blaming missing optimization):
        `20_loops3`, `22_func_param3`, `24_gcd3`
      The nearest visible `lv9` array/public analogues remain green on the
      rebuilt local compiler (`05_global_arr_init`, `06_long_array`,
      `08_arr_access`, `09_const_arr_read` all PASS), so current authority is
      that the reopened pressure is again hidden-only rather than a simple
      regression on the nearest public course witnesses. The next debug order
      should therefore prioritize the largest shared hidden family first:
      array/global-object segfaults, then the probable loop-condition RTLE
      family.
    - 2026-05-08 RTLE follow-up: one concrete hidden-like dead-loop family is
      now locally reproduced and closed instead of remaining only a naming
      guess from OJ. The minimized repro shape is a `for` loop whose condition
      depends on a mutable scalar local (`b < N`) while the step updates that
      local through an unknown function call (`b = step(b)`). Canonical IR
      lowering had been incorrectly proving that such a loop condition “stays
      true forever” because the flow-state evaluator fell back to stale
      scope-level `has_const_value` metadata for a tracked local after that
      local’s current value had already become unknown. In practice that let
      the loop header drop its exit branch entirely, which reproduced as
      `RUN_TIMEOUT` on a focused synthetic cluster. The current tree now
      blocks that fallback for tracked-but-unknown locals, keeps the exit
      path, and revalidates the family on `make test-ir-regression`,
      `make test-compiler-driver`, `lv9` (`22/22`), and a `91/91` local
      loop/parameter/gcd differential sweep whose previous failures were all
      on the `for(...; b < N; b = step(b))` subfamily.
    - course regression baselines remain green in the current tree:
      `lv8` (`12/12`) and `lv9` (`22/22`)
    - one reusable third-party sweep tool now exists in-repo:
      `tools/sweep_sysy_suite.py`, which follows the rebuilt
      `build/compiler -> clang -> ld.lld -> qemu-riscv32-static` path and
      applies the current normalized-oracle policy for third-party suites
      (including common `stdout`, `stdout+exit`, `stdout+"\\n"+exit`,
      `stdout+exit+"\\n"`, `stdout+"\\n"+exit+"\\n"`, trailing blank-line,
      and trailing-space noise)
    - directly sweepable external suites are now green again under that rule:
      `compiler2021/公开用例与运行时库/function_test2021` (`103/103`),
      `segviol/indigo/test_codes/functional_test` (`111/111`),
      `TrivialCompiler/custom_test` (`29/29`),
      and `ustb-owl/lava-test/cases` (`162/162`)
    - `jokerwyt/sysy-testsuit-collection/lvX` is now green again in the
      rebuilt current tree (`467/467`). Older local memories about
      `many_parameters10000.c`, `register_alloc10000.c`, and `matrix-1.c`
      should now be treated as historical checkpoints rather than as live
      red points.
    - the earlier local gap on `minic-test-cases-2021s` / `2021f` is now also
      materially closed in the current tree: the sweep harness can now fall
      back to the environment's native `libsysy.a` to synthesize host-side
      oracles for those `.c`-based suites when the local clone does not carry
      ready-made `.out` files or checked-out runtime sources. Focused reruns
      now keep `minic-test-cases-2021f/functional` green again (`100/100`)
      and `minic-test-cases-2021s/functional` green again (`112/112`) under
      the same rebuilt-compiler path and normalized-oracle rule.
    - one temporarily reopened hidden-like correctness tail is now also
      reclosed in the live tree and should be treated as part of the green
      baseline again rather than as an active unknown:
      `compiler2021/公开用例与运行时库/function_test2021/079_calculator.sy`
      had reproduced as a rebuilt-path `SIGSEGV` (`stdout=""`, `exit=-11`)
      because the RISC-V text exporter could bind the wrong preview data
      fixup at a given patch offset. In practice that let zero-valued global
      stores print malformed sequences like `lui t5, 0x0 ; sw zero, 0(t5)`
      instead of preserving the intended `%hi/%lo(symbol)` relocation pair.
      The current tree now resolves preview fixups by both
      `patch_byte_offset` and fixup family during text export, and
      `test-compiler-driver` now carries a dedicated zero-global-store symbol
      fixup regression so this failure shape stays locked down. Focused
      reruns are green again after that closure:
      `test-compiler-driver`, `lv9` (`22/22`), and
      `compiler2021/公开用例与运行时库/function_test2021` (`103/103`).
    - one second course-adjacent correctness tail is now also closed and
      regression-locked in the live tree: `minic-test-cases-2021s/functional`
      had reopened on `080_unlucky_data.c` as a false `RUN_TIMEOUT`, but the
      root cause was not raw performance pressure. The real bug was an
      over-aggressive loop-proof in canonical IR lowering: when a loop-carried
      local was only updated under an unknown `if` branch inside the loop, the
      flow-state proof could incorrectly treat the loop condition as staying
      permanently true and erase the explicit loop-header condition block.
      The current tree now treats unknown inner control flow as a blocker for
      that "loop condition stays true" proof and also preserves only merged /
      agreed-known state across unknown `if` joins. Focused checks are green
      again after that closure: `test-ir-regression`, the minimized
      `while(mon<=12){ if(getint()) mon=mon+1; }` IR shape, the original
      `080_unlucky_data.c` rebuilt-path runtime, and the full
      `minic-test-cases-2021s/functional` sweep (`112/112`). Together with
      the already-green `minic-test-cases-2021f/functional` rerun (`100/100`),
      this widens the post-fix course-adjacent evidence surface materially
      beyond the earlier `compiler2021` / `lv8` / `lv9` checkpoint.
  - post-checkpoint performance reopen:
    **in progress / roughly 87%**
    - 2026-05-08 recovery note: the current rollback/repair checkpoint has
      now reclosed the visible `03_sort1/03_sort3` correctness regressions by
      backing out the final `compiler_driver` text-peephole invocation
      `compiler_optimize_riscv_preview_indexed_local_base_offsets(...)` from
      the live preview pipeline. Revalidated local surfaces on this recovered
      tree are: `make test-compiler-driver` PASS, `make test-value-ssa-regression`
      PASS, `autotest -riscv -s lv8` (`12/12`), `autotest -riscv -s lv9`
      (`22/22`), focused public reruns
      `performance_test2021-public/03_sort1.sy` PASS,
      `performance_test2021-public/03_sort2.sy -> RUN_TIMEOUT`,
      `performance_test2021-public/03_sort3.sy` PASS, and
      `compiler2021/公开用例与运行时库/2021初赛所有用例/performance`
      back to `14/15` with only `03_sort2.sy -> RUN_TIMEOUT`. A full rerun of
      `performance_test2021-public` is currently in flight to re-stamp the
      expected `29/30` checkpoint explicitly in one log instead of only by
      focused witness inference.
    - current mainline policy shift: the older local `03_sort2` / `shuffle2`
      runtime tails are now explicitly treated as **frozen / maintenance-
      first** rather than as the primary optimization target for each next
      turn. They remain useful witnesses and must not regress, but the
      current tree has already pushed them into a diminishing-returns zone
      where further work is increasingly likely to cost disproportionate
      correctness risk for only small local wins. Unless a new very narrow
      and obviously safe hotspot line emerges, the active performance reopen
      should now prioritize higher-yield external perf / compile-time pressure
      (for example `lava-test/performance_test2021` residuals and allocator-
      side compile-time tails) over repeatedly reopening these same two
      public runtime cases.
    - future revisit note: after the current active red-point / external-perf
      loop is cleared, it is still reasonable to reopen these two local static
      witnesses **carefully** under a correctness-first rule. The current
      post-recovery rebuilt baselines are `03_sort2.sy ~= total_instructions=374`
      and `shuffle2.sy ~= total_instructions=258`. Treat those as the new
      “must not regress” floor for any later narrow optimization retry, and do
      not spend the current turn-range on them unless a very obviously safe
      follow-up appears.
    - timeout-evaluation policy for the current local environment is now
      explicit: do **not** treat every `RUN_TIMEOUT` as immediate proof of a
      compiler semantic or algorithmic regression. Some large perf inputs may
      also miss the local budget because the workstation is memory-tight and
      pays disproportionate paging / host-side I/O cost during full-input
      execution. Therefore each timeout line should first be classified into:
      1. likely local-environment-limited timeout (needs caution before
         over-optimizing against it), or
      2. likely real compiler/runtime pressure (worth active optimization).
      Preferred evidence for that classification is: static instruction count
      (`tools/analyze_riscv_asm.py`), direct opcode histograms, focused
      compile-time timings, structural trip-count reasoning from the testcase
      itself, and whether nearby smaller / equivalent witnesses already stay
      green under the same rebuilt compiler.
    - current next-round execution loop should therefore be:
      1. keep the correctness-closed surfaces closed (`lv8`, `lv9`,
         `03_sort1`, `03_sort3`, `lava-test/lava_test/kmp.sy`)
      2. classify each remaining timeout by “local-memory-limited vs real
         compiler/runtime pressure”
      3. for the genuine pressure cases, prefer optimizations that either
         reduce static instruction count or clearly remove hot repeated work
         in the emitted program
      4. treat compile-time speed itself as a first-class perf target, not
         only generated-code runtime
      5. after each kept perf chunk, rerun rotated focused correctness
         witnesses before broadening again
    - a later same-day full sweep now supersedes several older “nearly all
      green” perf-side memories and should be treated as the live authority
      for the current workspace. The rebuilt tree still keeps the broad
      repository/course baselines green (`make test`, `autotest -riscv`, and
      `autotest -perf` all pass), but the broader public/external perf
      pressure is no longer the older single-point picture:
      `compiler2021/公开用例与运行时库/2021初赛所有用例/performance`
      is now `12/15` with
      `03_sort1.sy -> MISMATCH`,
      `03_sort2.sy -> RUN_TIMEOUT`,
      `03_sort3.sy -> MISMATCH`;
      `compiler2021/公开用例与运行时库/performance_test2021-public`
      is now `27/30` with that same `03_sort{1,2,3}` residual set; and
      `segviol/indigo/test_codes/performance_test` is now `9/10` with
      `03_sort1.sy -> MISMATCH`.
      Current authority is therefore that the visible public perf surface has
      materially reopened beyond the older “only `03_sort2` remains” memory,
      and the next debug pass should treat the `03_sort1/2/3` cluster as one
      coherent active problem rather than as a stale single-case timeout.
    - a broader 2026-05-07 evidence refresh now also exists in explicit
      scoreboard form instead of only as scattered one-off notes, and it is
      important because it makes the current public/external pressure surface
      unusually consistent:
      `autotest -riscv /workspaces/compiler_lab` is green (`130/130`),
      `autotest -perf /workspaces/compiler_lab` is green (`130/130`),
      `compiler2021/公开用例与运行时库/2021初赛所有用例/functional`
      is green (`103/103`),
      `compiler2021/公开用例与运行时库/2021初赛所有用例/h_functional`
      is green (`37/37`),
      `compiler2021/公开用例与运行时库/2021初赛所有用例/performance`
      is `14/15` with only `03_sort2.sy -> RUN_TIMEOUT`,
      and `compiler2021/公开用例与运行时库/performance_test2021-public`
      is `29/30` with only `03_sort2.sy -> RUN_TIMEOUT`.
      Current authority is therefore stronger than an earlier “probably only
      one public perf tail” guess: on the current tree the public course-style
      performance surface now reproduces one stable remaining red point,
      `03_sort2`, and does so consistently across both visible perf suites.
    - a later same-day live-tree refresh has now superseded that earlier
      `29/30` public-perf snapshot, and it should be treated as the current
      authority for the code now in the workspace. The kept indirect-memory
      direct-cleanup line now forwards safe local loads plus
      non-address-taken global loads and then re-runs a narrow redundant-
      binary cleanup on the direct-build Value-SSA path. That change is
      revalidated on the current tree by `make test-value-ssa-regression`,
      `make test-compiler-driver`, `autotest -riscv -s lv9`
      (`22/22`), and rotated external functional sweeps
      (`minic-test-cases-2021s/functional` `112/112`,
      `indigo/test_codes/functional_test` `111/111`). On the performance
      side it produces real static wins on both currently reproduced public
      timeout witnesses. Current stable rebuilt witnesses on the live tree are
      no longer the earlier `428/331` pair: `03_sort2.sy` now sits around
      `total_instructions=399`, and `shuffle2.sy` now sits around
      `total_instructions=260`. The runtime side is still open
      under the same `60s` single-case budget, however: focused reruns keep
      both `03_sort2.sy -> RUN_TIMEOUT` and `shuffle2.sy -> RUN_TIMEOUT`.
      Current authority is therefore that this indirect-memory direct-cleanup
      step is a kept optimization checkpoint rather than a backed-out trial,
      but the public runtime tail has widened on the current live tree to a
      concrete pair (`03_sort2`, `shuffle2`) instead of collapsing to all
      green.
- 2026-05-07: One later same-day `machine_select` follow-up is now also kept, and it is materially different from the earlier selected-side retries that were backed out. The current pure-call reuse path now understands block live-in register/spill arguments instead of only arguments whose defining load/address op appears earlier in the **same** block. That matters directly for the stable `03_sort2` residue, because one of the remaining hot `radixSort` blocks was repeatedly calling `getNumPos(spill.8, spill.9)` after those spill args were defined in the predecessor block, so the previous block-local arg-describer could not even recognize the calls as equal. The new live-in operand descriptor plus regression coverage (`test_machine_select_reuses_repeated_internal_pure_call_with_live_in_spill_args`) is green together with `make test-machine-select`, `make test-value-ssa-regression`, `make test-compiler-driver`, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep (`TrivialCompiler/custom_test` `29/29`). On the current stable tree this follow-up tightens the public static witness again on the more call-heavy tail: `03_sort2.sy` moves from about `419` down to about `413`, while `shuffle2.sy` stays roughly flat at `283`. Runtime still does not close under the same `60s` single-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a real local win on the `03_sort2` path, but not to overclaim it as the final runtime-closing step.
- 2026-05-07: One same-day Value-SSA-side follow-up is now also kept on the other half of the public tail pair. The current indirect-memory direct-cleanup line now also inlines an extremely narrow class of internal helpers: single-block returning functions whose bodies stay inside `load_local(parameter)`, `load_global`, `mov`, and one or more pure `binary` ops, with no stores, indirect memory, address-taking, or nested calls. This is deliberately not a general inliner; it exists because the live `shuffle2` residue had converged onto the tiny helper `hash(k){ return k % hashmod; }`, so removing that repeated internal call overhead was a more direct next step than more block-local cleanup. The new regression `VALUE-SSA-CONVERT-DEFAULT-TINY-HELPER-INLINE` is green together with `make test-value-ssa-regression`, `make test-compiler-driver`, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep (`lava-test/cases` `162/162`). On the current stable tree this trims the public `shuffle2` static witness further from about `283` down to about `268`, with the direct `.s` opcode histogram also dropping call count from roughly `12` to `10`, while the already-improved `03_sort2` witness stays about flat at `413`. Runtime still does not close under the same `60s` single-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a real local win on the `shuffle2/hash` line, while treating the remaining runtime gap as still-open pressure rather than as solved.
- 2026-05-07: A later final-text peephole retry was tested and explicitly **not** kept. I tried a very conservative entry-block reload elision in `compiler_driver` that replaced an initial `lw reg, off(sp)` with `mv reg, aN` when the same function had just stored that incoming argument register to the matching stack slot at function entry. The motivation was reasonable because hot leaf functions like `insert`, `reduce`, and `getNumPos` still spill arguments to stack and then immediately reload them. On tiny hand probes the text looked better, but the real course surface rejected the change: `make test-compiler-driver` and `make test-value-ssa-regression` stayed green after expectation refreshes, yet `autotest -riscv -s lv9` regressed badly (`13/22` with multiple array/sort wrong answers). The peephole has therefore been fully backed out again. Current authority is to treat this as a failed/unkept text-side trial rather than as a checkpoint, and to prefer later work on IR/selected/runtime cost directly over more function-entry text rewrites of this kind.
- 2026-05-07: A later rebuild-first recheck overturned that earlier “default `-riscv` widening failed” conclusion and replaced it with a real kept checkpoint. The previous negative read had been contaminated by stale-binary evidence: after explicitly rebuilding `build/compiler` first and then rerunning the same surfaces, widening `value_ssa_optimize_perf_hotspots(...)` from the `COMPILER_MODE_PERF` path to the default `COMPILER_MODE_RISCV` path held up cleanly. The only immediate red point was one repository-local text expectation in `test_compiler_driver` (`int g = 7; return g;` now quite reasonably lowers to `li a0, 7` instead of forcing a `lui/lw` pair), and that expectation has now been updated to lock the improved shape. The revalidated kept surface after the widening is: `make test-value-ssa-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv9` (`22/22`), rotated external functional sweeps `TrivialCompiler/custom_test` (`29/29`) and `minic-test-cases-2021s/functional` first `20/20`, plus focused public perf witnesses still reproducing only the same runtime tails (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`). On the kept tree the default `-riscv` public static witness improves one more step on the `03_sort2` line, from about `total_instructions=413` down to about `408`, while `shuffle2.sy` stays about flat at `268`; the rebuilt `-perf` witness for `03_sort2` now matches the same `408` image too, so the two modes are back on one real shared path instead of only looking similar in principle. Current authority is therefore that this pass *is* now general enough for the default RISC-V path under its present narrow transform set; the earlier rollback memory should be treated as superseded by this rebuild-first recheck rather than as the live conclusion.
- 2026-05-07: One more same-day Value-SSA-side retry was also tested and explicitly **not** kept. I tried an even narrower-looking cleanup that deleted `store_local` writes to locals that appeared to be “never read” in the current Value-SSA function, hoping to remove obvious junk such as `getNumPos`’s dead `tmp = 1` stack slot. On a superficial static witness this looked spectacular: the rebuilt `03_sort2.sy` text image collapsed all the way to about `total_instructions=359`. That win was fake. The pass was reasoning at the wrong storage granularity for local aggregate/array layouts: in practice it deleted most of the zero-initialization stores for `radixSort`’s local `head` / `tail` / `cnt` arrays, leaving only the first element writes alive because later dynamic accesses reached those locals through aggregate-style addressing rather than plain matching `load_local` rows. I rolled the pass back immediately after inspecting the emitted assembly and seeing that bogus collapse. The rolled-back tree is revalidated again (`make test-value-ssa-regression`, `make test-compiler-driver`, `autotest -riscv -s lv9` all green), and the stable `03_sort2` witness returns to `total_instructions=408`. Current authority is therefore explicit: do **not** attempt slot-by-slot “unread local store” cleanup on the direct Value-SSA path unless/until the analysis understands local aggregate/array aliasing well enough to distinguish scalar locals from aggregate-backed slot families.
- 2026-05-07: A later same-day structural follow-up then reopened that failed idea in a kept, narrower form. The key observation was that canonical IR still knew which local slots belonged to array/aggregate families (`IrLocal.array_rank`), but that metadata was being dropped before the direct Value-SSA cleanup line. The current tree now propagates local `array_rank` metadata through `IR -> LowerIR -> ValueSSA`, and canonical IR now stamps the declared `array_rank` across the whole contiguous local-slot family created for one local array declaration, not only the base slot. On top of that, the direct-build Value-SSA cleanup line now deletes only unread **scalar** local stores, while conservatively treating any array/aggregate local slot as ineligible for that cleanup. The validation surface for this kept step is: `make test-lower-ir-regression` PASS, `make test-value-ssa-regression` PASS (including new default-conversion regressions that lock both “unread scalar local store is removed” and “array local stores are preserved”), `make test-compiler-driver` PASS, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep `TrivialCompiler/custom_test` (`29/29`). On the public static witnesses this trims a little more real stack traffic without reopening the earlier array-family bug: `03_sort2.sy` improves from about `408` down to about `404`, and `shuffle2.sy` improves from about `268` down to about `266`. The runtime witnesses still stay open under the same budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a small but real structural cleanup win on the direct path, not as the final runtime-closing step.
- 2026-05-07: A later selected-side retry was also tested and explicitly **not** kept. I prototyped a very narrow cross-block cleanup that tried to reuse an edge-available `load_indirect(addr_global + index)` result in a unique-successor block instead of recomputing the same address/load chain again. The motivating witness was real (`shuffle2/insert` has exactly that shape from the head-bucket probe into the `p = head[h]` successor block), and the prototype even fired on a synthetic unit shape. But it did not yet have a sound enough interaction story with the broader selected cleanup pipeline: after landing the first version, the existing `machine_select` regression matrix reopened on an unrelated live-in/internal-call case (`test_machine_select_reuses_repeated_internal_pure_call_with_live_in_spill_args`), with verification failing during lowering. Because that meant the new cleanup still had hidden coupling with current selected alias/live-in reasoning, I rolled it back completely instead of trying to patch it forward in the same round. Current authority is therefore to keep this idea in the “promising but not yet proven” bucket: cross-block redundant indirect-load reuse may still be worthwhile later, but only after a tighter proof about predecessor-available values and selected live-ins is in place.
- 2026-05-07: A later same-day Value-SSA-side follow-up then landed on exactly that same `shuffle2` bucket-probe shape, but at the safer direct-build layer instead of at `machine_select`. The current indirect-memory direct fast-cleanup line now also forwards one narrow class of repeated `load_indirect` across a unique predecessor edge: if the predecessor already computed a `load_indirect(addr)` and reaches the successor through its unique idom edge with no intervening memory write/call after that load, and the successor re-materializes the same pure address expression at block entry only to load it again, the second load is rewritten to a `mov` of the predecessor result. Existing cleanup then drops the now-dead local address chain. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-UNIQUE-PRED-INDIRECT-LOAD-FORWARD`) now locks that shape down, and the kept surface is revalidated by `make test-value-ssa-regression`, `make test-lower-ir-regression`, `make test-compiler-driver`, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep `TrivialCompiler/custom_test` (`29/29`). On the public static witnesses this does exactly what the earlier selected-side attempt was aiming for, but without reopening the selected regression line: `03_sort2.sy` stays about flat at `404`, while `shuffle2.sy` improves again from about `266` down to about `262` by removing the duplicate `head[h]` bucket probe on the false branch of `insert`. Runtime still does not close under the same `60s` budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a real local win on the `shuffle2` line rather than as a full tail closure.
- 2026-05-07: One more same-day direct-path Value-SSA cleanup step has now landed on top of that edge-load-forward checkpoint, and this one helps both public witnesses at once. The kept indirect-memory direct fast-cleanup line now also reuses repeated `addr_local` / `addr_global` roots **within one block**: a later same-slot address formation is rewritten to a `mov` of the earlier root, and the existing trivial-value + redundant-binary cleanup then collapses the now-identical downstream address arithmetic. This is deliberately narrower than general expression CSE; the kept rule only touches repeated slot-root formation and relies on the already-landed cleanup passes to do the rest. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-REPEATED-ADDR-ROOT-REUSE`) now locks that shape down. The revalidated kept surface is again: `make test-value-ssa-regression` PASS, `make test-lower-ir-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep `TrivialCompiler/custom_test` (`29/29`). On the public static witnesses this trims a little more real address setup from both lines: `03_sort2.sy` improves from about `404` down to about `399`, while `shuffle2.sy` improves from about `262` down to about `260`. The focused public runtime witnesses still remain `RUN_TIMEOUT` under the same `60s` budget, so current authority is to keep this as another small but real structural win on the direct path rather than as a runtime-closure claim.
- 2026-05-07: One later same-day Value-SSA follow-up is now also kept on the still-open `03_sort2` residue, and it is deliberately narrower than a general call-CSE pass. The direct indirect-memory cleanup line now also reuses repeated **same-block pure internal calls** when the callee has a real body, touches only local slots plus readonly scalar globals, performs no indirect memory, no global writes, no address-taking, and no nested calls, and the later call repeats the same callee name plus identical SSA/immediate args. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-REPEATED-PURE-INTERNAL-CALL-REUSE`) now locks that shape on a multi-block helper that mutates locals and reads one readonly global. A later same-day safety tightening also stayed kept: the same-block reuse no longer crosses intervening `call`, `store_global`, or `store_indirect` rows, so the optimization boundary is narrower but less speculative than the first prototype. The revalidated kept surface for the live tree is now: `make test-value-ssa-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv8` (`12/12`), `autotest -riscv -s lv9` (`22/22`), and rotated external functional sweeps `indigo/test_codes/functional_test` (`111/111`) plus `minic-test-cases-2021f/functional` (`100/100`). After explicit `make compiler` rebuilds to avoid stale-CLI evidence, the focused public perf witnesses still remain open under the same `60s` one-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`). The rebuilt static direction is still mildly positive on the `03_sort2` line, but the kept witness should now be read from the post-tightening tree rather than from the earlier looser prototype: under the current quick-count witness rule, `03_sort2` sits around `395` instead of the earlier `402`, while `shuffle2` stays effectively flat around `274`. Current authority is therefore to keep this as a modest but real 03-sort2-side structural win with greener safety boundaries, while treating the remaining work as runtime-tail closure rather than as another correctness or stale-binary issue.
- 2026-05-07: One neighboring extension of that same idea was also tested and explicitly **not** kept. I prototyped a unique-predecessor cross-edge version of the pure-internal-call reuse so a successor block could reuse an equal pure call computed in its sole idom predecessor. Focused regression coverage proved the transform itself was implementable, but after rebuild-first measurement it did not earn its complexity on the live public witness: `03_sort2` did not reduce `call getNumPos` count, the quick-count static witness drifted the wrong way (`~391 -> ~393` in the temporary prototype lane), and the focused public runtime witness still stayed `RUN_TIMEOUT`. That cross-edge version has therefore been fully backed out again. Current authority is to keep only the narrower same-block reuse with explicit blockers, and to treat cross-edge pure-call reuse as an unkept experiment rather than as the next default optimization step.
- 2026-05-07: One more same-day direct-path Value-SSA cleanup step is now also kept, and this one finally explains one of the “why does the dump still show obviously repeated adds?” tails. The problem was not that `value_ssa_eliminate_redundant_binaries(...)` failed to run; it was that the first redundant-binary pass could create a `mov`, then the following trivial-value simplify would collapse that `mov` and only *then* expose a second-layer duplicate binary that the pipeline no longer revisited. The current direct indirect-memory cleanup tail now therefore runs one extra small fixed-point round of `normalize binary operands -> eliminate redundant binaries -> simplify trivial values -> eliminate dead value defs` after the first such round. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-REDUNDANT-BINARY-FIXED-POINT`) now locks exactly that shape: two equal `add x,1` rows feed two later `add <same>,2` rows, and the second cleanup round must collapse the exposed duplicate into the shorter final chain. This change also widened one repository-local text expectation rather than reopening a bug: the `compiler-driver` “call-arg load swap” witness now accepts the stronger optimized shape where a tiny internal helper call is folded all the way to `lw a0, 0(a1)` plus `addi a0, a0, 5` with no remaining `call`. The revalidated kept surface for the live tree is: `make test-value-ssa-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv8` (`12/12`), and a rotated external functional sweep `TrivialCompiler/custom_test` (`29/29`). On the rebuilt public static witnesses this extra fixed-point round produces a real new drop on both open perf tails: `03_sort2` moves from about `395` down to about `383`, and `shuffle2` moves from about `274` down to about `272`. The focused public runtime witnesses are still open under the same `60s` one-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as another real structural cleanup checkpoint while continuing to treat runtime-tail closure as the active remaining problem.
- 2026-05-07: One later same-day indirect-memory follow-up is now also kept on top of that fixed-point checkpoint, and this one is deliberately alias-aware rather than just “forward more loads”. The current direct cleanup line now also forwards repeated **same-block `load_indirect`** rows when the address is unchanged and every intervening write can be proven not to alias that loaded address. The first landed proof surface is intentionally narrow: it understands `addr_local`, `addr_global`, and `load_local(parameter-array)` roots, proves at least `local-vs-global`, `local-vs-parameter-pointer`, and `different-global-vs-global` non-aliasing, and still treats unknown indirect roots or calls as barriers. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-SAME-BLOCK-INDIRECT-LOAD-FORWARD-PARAM-NONALIAS`) now locks the representative shape directly: a local-array `load_indirect(addr_local ...)` may survive across a scalar-local store and an intervening `store_indirect(load_local parameter-array, ...)` and be reused as the final return value. The revalidated kept surface for the live tree is: `make test-value-ssa-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep `minic-test-cases-2021s/functional` (`112/112`). On the rebuilt public static witnesses this alias-aware load reuse gives one more real step on the `03_sort2` line without reopening `shuffle2`: `03_sort2` moves from about `383` down to about `382`, while `shuffle2` stays about flat at `272`. The focused public runtime witnesses still remain `RUN_TIMEOUT` under the same `60s` one-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a small but real `03_sort2`-side alias-proof win while continuing to treat the remaining work as runtime-tail closure rather than as correctness repair.
- 2026-05-07: One more same-day Value-SSA refinement is now also kept on the `03_sort2` line, and it reopens an earlier caution point with a narrower proof instead of with a broad heuristic. The current **same-block pure internal call reuse** no longer treats `store_indirect` as an automatic barrier. That earlier barrier turned out to be overly conservative for the actually-landed pure-callee class, because those callees are already restricted to value arguments plus readonly globals and explicitly do **not** read indirect memory. So the kept rule now blocks only across intervening `call` rows, not across arbitrary local/global/indirect stores. A focused default-conversion regression (`VALUE-SSA-CONVERT-DEFAULT-REPEATED-PURE-INTERNAL-CALL-ACROSS-STORE-INDIRECT`) now locks the intended shape directly: two equal pure helper calls may be merged even when the caller performs an intervening `store_indirect` to an unrelated global array slot between them. The revalidated kept surface for the live tree is: `make test-value-ssa-regression` PASS, `make test-compiler-driver` PASS, `autotest -riscv -s lv8` (`12/12`), and a rotated external functional sweep `indigo/test_codes/functional_test` (`111/111`). On the rebuilt public static witnesses this narrower barrier meaningfully helps the still-open hot line: `03_sort2` drops again from about `382` down to about `377`, while `shuffle2` stays about flat at `272`. Focused public runtime witnesses still remain open under the same `60s` one-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a real `03_sort2`-side structural win with a tighter purity argument, while treating the remaining open work as runtime-tail closure rather than as another correctness or alias-safety bug.
- 2026-05-07: One later same-day final-text/runtime-adjacent cleanup is now also kept, and this one finally starts shaving a bit of the obvious call-staging overhead that remained after the earlier Value-SSA work. The preview text exporter now recognizes one very narrow seven-line stack-staged two-argument call pattern: `lw tmp0, src0(sp); sw tmp0, dst0(sp); lw tmp1, src1(sp); sw tmp1, dst1(sp); lw a0, dst0(sp); lw a1, dst1(sp); call ...`. When that exact shape appears, it now folds to the shorter direct form `lw a0, src0(sp); sw a0, dst0(sp); lw a1, src1(sp); sw a1, dst1(sp); call ...` instead of pointlessly reloading the just-staged arguments from the temporary stack slots. This is intentionally much narrower than the earlier unsafe entry-reload experiment: it is local, adjacent, and preserves the same post-call stack snapshots. The revalidated kept surface for the live tree is: `make test-compiler-driver` PASS, `autotest -riscv -s lv9` (`22/22`), and a rotated external functional sweep `TrivialCompiler/custom_test` (`29/29`). On the rebuilt public static witnesses this text-level fold gives one more small real step on the `03_sort2` line without reopening `shuffle2`: `03_sort2` moves from about `377` down to about `375`, while `shuffle2` stays about flat at `272`. Focused public runtime witnesses still remain `RUN_TIMEOUT` under the same `60s` one-case budget (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to keep this as a small but real call-staging cleanup win while continuing to treat the remaining work as runtime-tail closure rather than as a correctness problem.
- 2026-05-07: One neighboring final-text `repeated lui %hi(symbol)` peephole was also tested and explicitly **not** kept. The idea was to drop a repeated `lui rd, %hi(sym)` when the same block later rebuilt the same high half into the same register without an intervening redefinition. On the immediate static witnesses it looked promising (`03_sort2: ~375 -> ~374`, `shuffle2: ~272 -> ~269`), but the course surface rejected it: `autotest -riscv -s lv9 /workspaces/compiler_lab` reopened on `13_complex_arr_params` as a runtime wrong answer (`-11`). The optimization has therefore been fully backed out again, and post-backout checks reclose the affected surfaces (`make test-compiler-driver` PASS, `autotest -riscv -s lv9` back to `22/22`). Current authority is therefore explicit: do **not** treat repeated-`lui` deletion as a kept backend/text optimization in the current tree; it is a recorded failed experiment rather than an active checkpoint.
    - that same refreshed sweep also widened the explicit third-party
      scorecard instead of leaving the external surface at the level of older
      broad statements. The current rebuilt tree keeps
      `minic-test-cases-2021f/functional` green (`100/100`),
      `minic-test-cases-2021s/functional` green (`112/112`),
      `indigo/test_codes/functional_test` green (`111/111`), and
      `indigo/test_codes/performance_test` green (`10/10`).
      The two `pku-minic` performance directories are not active red points
      in the present environment; they currently come back as `SKIP`
      (`2021f/performance: 20`, `2021s/performance: 18`) because the local
      clones do not carry the `.out` assets required by the current harness.
      The remaining meaningful third-party perf pressure at the latest full
      sweep is still concentrated, but the concrete residual set has changed
      materially from the older notes. `lava-test/performance_test2021` is
      now `25/29`, and
      `compiler2021/公开用例与运行时库/performance_test2021-private`
      is also `25/29`, both with the same residual set:
      `dead-code-elimination-3.sy -> RUN_TIMEOUT`,
      `hoist-2.sy -> RUN_TIMEOUT`,
      `hoist-3.sy -> RUN_TIMEOUT`, and
      `integer-divide-optimization-3.sy -> RUN_TIMEOUT`.
      This supersedes the older memory that the line was dominated by
      `COMPILE_TIMEOUT` / `COMPILE_FAIL` on the whole
      `dead-code-elimination-*` / `integer-divide-optimization-*` families.
      Current authority is therefore that the external perf surface has
      improved on some earlier compile-side failures, but still retains a
      smaller runtime-timeout cluster that should be treated as active.
    - the latest same-day full sweep also widened the non-course external
      scorecard beyond the older “all green except lava perf” story:
      `lava-test/lava_test` is now `18/21` with
      `kmp.sy -> MISMATCH`,
      `many_parameters10000.sy -> COMPILE_TIMEOUT`, and
      `register_alloc10000.sy -> COMPILE_TIMEOUT`;
      `lava-test/custom_test` remains green (`29/29`);
      `lava-test/shortcircuit_test` remains green (`1/1`);
      `minic-test-cases-2021s/performance` remains `18 SKIP`;
      `minic-test-cases-2021f/performance` remains `20 SKIP`.
      Current authority is therefore that giant-parameter compile-time
      pressure and one functional/perf-adjacent `kmp` mismatch are now part
      of the live external scorecard, not just optional future curiosity.
    - 2026-05-08 follow-up: the `kmp.sy` correctness red point is now reclosed.
      The root cause was in `machine_ir_copy_cleanup`: its backward
      needed/live scan did not mark `load_indirect` address operands or
      `store_indirect` address/value operands as uses, so local-array address
      chains like `addr_local + index` could be deleted as dead defs during
      canonicalization. After restoring those indirect-memory use marks, the
      focused witness `lava-test/lava_test/kmp.sy` is back to PASS, and a
      fresh full rerun of `lava-test/lava_test` is now green (`21/21`).
      Current authority is therefore that this external sub-suite is no
      longer carrying an active correctness red point; the remaining external
      pressure stays on the timeout / compile-time lines instead.
    - the first current-round baseline points are now refreshed rather than
      inherited only from older notes: the rebuilt CLI currently compiles
      `lava-test/performance_test2021/hoist-2.sy` to RISC-V assembly in about
      `11.8s`, while the still-open public runtime tail
      `compiler2021/performance_test2021-public/03_sort2.sy` remains a
      `RUN_TIMEOUT` under the current `60s` per-case sweep budget
    - current working interpretation is now materially ahead of the older
      `26%` snapshot that this section previously carried. The compile-side
      pressure has already yielded one real allocator-side win on `hoist-2`,
      and the runtime-side `03_sort2` line has already accumulated several
      safe, kept late backend / final-text wins:
      `608 -> 523 -> 515 -> 482 -> 488 -> 485 -> 434 -> 430 -> 428` total emitted instructions
      across the successive kept checkpoints, with major hot-family movement
      such as `li: 139 -> 12`, `mul: 34 -> 0` in the final text, and
      `slli`, `mv`, `addi`, `srai`, and `andi` replacing several earlier
      helper-register patterns.
    - current authority is also slightly different from what the older
      wording implied: the local-machine `03_sort2` `RUN_TIMEOUT` is still a
      valid recorded data point, but it is no longer the only gate for
      whether this performance reopen has materially progressed. Given the
      huge `n=30000000` public input and the environment-sensitive IO /
      memory-bandwidth cost of that case, this round should now be judged
      primarily by `1)` correctness staying green, `2)` compile-side pressure
      on `hoist-2` not regressing, and `3)` real static/runtime-cost
      reductions on the public hot loops, rather than by forcing this one
      local machine to beat the same `60s` wall.
    - a later 2026-05-07 focused perf recheck then tightened the active
      witness set again with fresh one-case reproductions instead of relying
      only on the older broad-sweep memory. The currently reproduced tails are
      still:
      `compiler2021/.../performance_test2021-public/03_sort2.sy ->
      RUN_TIMEOUT`,
      `lava-test/performance_test2021/hoist-2.sy -> RUN_TIMEOUT`,
      `lava-test/performance_test2021/dead-code-elimination-1.sy ->
      COMPILE_TIMEOUT`, and
      `lava-test/performance_test2021/integer-divide-optimization-1.sy ->
      COMPILE_TIMEOUT`.
    - that same recheck also added fresh stage-level profiling so the compile
      side is less opaque than before. `hoist-2.sy` now again shows a clear
      `machine_ir_report` bottleneck
      (`machine_ir_report ~= 17.358s`, `full_riscv ~= 17.635s`, later layers
      negligible). `integer-divide-optimization-1.sy` shows the same shape
      but more severely (`value_ssa ~= 1.783s`,
      `machine_ir_report ~= 51.836s`, later layers negligible). By contrast,
      `dead-code-elimination-1.sy` still reaches `lower_ir` quickly
      (`lexer ~= 0.042s`, `parser ~= 0.168s`, `semantic ~= 0.073s`,
      `ir_lower ~= 0.048s`, `lower_ir ~= 0.025s`) and then goes silent for
      tens of seconds before even emitting the next stage marker, so current
      authority is that this particular compile-time tail is earlier than the
      `machine_ir_report` hotspot and likely sits in the heavy `value_ssa`
      pipeline or just beyond it.
    - current reopen interpretation is therefore sharper than before:
      `03_sort2` remains a runtime/program-performance tail,
      `hoist-2` and `integer-divide-optimization-1` are now clearly
      `machine_ir_report`-dominated compile-time tails,
      and `dead-code-elimination-1` is a deeper earlier-stage compile-time
      tail that should be debugged in `value_ssa`/allocator-side work before
      spending more time on later backend/export layers.
    - a later same-day follow-up then materially improved two of those compile
      tails instead of only diagnosing them. First, the `value_ssa` default
      entry now treats "contains at least one extreme straight-line function"
      as sufficient to use the direct-build fast path; it no longer requires
      every body function in the program to be a single-block return just to
      help one giant helper function. After explicitly rebuilding the CLI with
      that bridge change, `dead-code-elimination-1.sy` rechecks green instead
      of timing out at compile time. Second, the no-move allocator-plan fast
      path was tightened so trivially uncoalesced cases skip the expensive
      effective-degree maintenance machinery instead of paying for coalescing
      facts that are provably absent. That change keeps allocator/value-SSA
      regressions green and drops the representative
      `integer-divide-optimization-1.sy` compile profile from
      `machine_ir_report ~= 57.323s` down to roughly
      `machine_ir_report ~= 6.218s`, which also rechecks as a full PASS under
      the focused perf sweep.
    - those same fixes also sharpen the remaining external perf shape rather
      than simply erasing it. After the latest reruns:
      `dead-code-elimination-1.sy` is now PASS,
      `integer-divide-optimization-1.sy` is now PASS,
      while `dead-code-elimination-2.sy`,
      `dead-code-elimination-3.sy`, and `hoist-2.sy` now reproduce as
      `RUN_TIMEOUT` rather than compile-time failure. Current authority is
      therefore that this line has advanced from "several opaque compile
      timeouts" to a narrower split:
      `03_sort2` plus `hoist-2` and the remaining `dead-code-elimination-*`
      cases are now runtime/perf tails, while the compile-time-only
      `integer-divide-optimization-*` family is at least partially reclosed.
    - one first follow-up idea in this stage has now been tried and explicitly
      *not* promoted: I tested a conservative default-path value-SSA reopen
      around redundant pure-call elimination / memory-value canonicalization
      in hopes of helping the `03_sort2` runtime tail, but the observed result
      was the wrong tradeoff for the current tree. That experiment either left
      `03_sort2` unchanged (`RUN_TIMEOUT`) or regressed nearby baselines
      (`hoist-2` compile time drifted up into the mid-teens, and one variant
      even reopened `lv9` with a wrong-answer `-11` symptom). Current
      authority is therefore to keep those new default-path pass hooks *out*
      of the mainline and treat that direction as a failed experiment rather
      than as a checkpoint. The stable current baseline after backing it back
      out is again: `lv8` (`12/12`), `lv9` (`22/22`), `hoist-2` compile in
      roughly `15s`, and `03_sort2` still `RUN_TIMEOUT`.
    - one first compile-time optimization move is now landed and measured on
      the live mainline instead of only discussed abstractly: the
      `machine_ir` entry path no longer clones the whole `ValueSsaProgram`
      eagerly for every compile when the active lowering path will stay on the
      all-spill or conservative non-rewrite branch anyway. That cut one full
      needless SSA-program copy out of the common compile path for large
      programs while keeping the real allocate+rewrite branch unchanged.
      After fixing one intermediate regression in that refactor and rerunning
      the key baselines, the current rebuilt tree is green again on
      `test-compiler-driver`, `test-ir-regression`, and `git diff --check`,
      and the direct `hoist-2` compile baseline moved from roughly
      `13.3s` down to roughly `11.8s`. Current authority is therefore that
      the performance reopen has now produced one real compile-time win on the
      allocator-heavy path, while the distinct runtime tail `03_sort2` still
      reproduces independently as a `RUN_TIMEOUT`.
    - one later perf-only experiment was also tried and then explicitly
      backed back out, and it should stay recorded as a failed direction
      rather than being rediscovered repeatedly. I tested a very narrow
      `-perf`-only Value-SSA pass that tried to reuse repeated same-block pure
      internal calls (motivated by the repeated `getNumPos(...)` pattern in
      `03_sort2`). The result was the wrong tradeoff for the current tree:
      `03_sort2` still timed out under the same `60s` budget, while the
      compile-time baseline drifted back upward (`hoist-2` moved from the
      improved `~11.8s` band back into roughly `13s`). That experiment has now
      been removed again, and the current live baseline after rollback is:
      `test-compiler-driver` green, `test-ir-regression` green,
      `hoist-2` back around `12.9s`, and `03_sort2` still `RUN_TIMEOUT`.
    - a second, even narrower retry on that same family has now also been
      tried and rejected so we do not circle back to it later by accident.
      I reintroduced the repeated-helper-call idea in a smaller form: a tiny
      `-perf`-only block-local pure internal-call reuse pass, wired directly
      into the `compiler_driver` perf path instead of reopening a larger
      canonicalization pipeline. That version also failed the same two-way
      test: `03_sort2` still timed out under the same `60s` budget, while the
      compile-side `hoist-2` baseline regressed badly into the mid-`15s`
      range. It has now been removed again, and the current rolled-back
      baseline is back to `test-value-ssa-regression` green,
      `test-compiler-driver` green, and `hoist-2` around `12.6s`. Current
      authority is therefore explicit: **do not reopen pure-call reuse /
      repeated-helper-call CSE as the next performance move**; that line has
      now failed twice with the wrong compile-time/runtime tradeoff.
    - a third performance experiment has now also been tried, measured, and
      explicitly backed out: letting `value_ssa_forward_local_loads(...)`
      handle non-address-taken scalar locals even in programs that also use
      indirect memory, and then wiring that pass into the `-perf` path only.
      This was a soundness-first idea aimed at array-heavy programs like
      `03_sort2`, and the safety boundary is now regression-locked (address-
      taken locals do not get forwarded). But as a performance move it still
      failed the same practical test: `03_sort2` remained `RUN_TIMEOUT`, while
      the compile-side `hoist-2` baseline regressed into the mid-`15s` range.
      That experiment has now also been removed again. The current rolled-back
      baseline after all three rejected perf branches is back to
      `test-value-ssa-regression` green, `test-compiler-driver` green, and
      `hoist-2` around `11.4s`. Current authority is therefore stronger than
      before: the next performance move should avoid both repeated-helper-call
      reuse and “extra `-perf` entrypoint canonicalization/forwarding passes”
      as primary directions.
    - one reopened repository-internal regression-sync tail is now also mostly
      reclosed in the live tree and should not be confused with the public
      performance mainline itself. The previously red `make test` cluster in
      `machine_bytes` / `machine_object` / `machine_reloc` turned out to be
      dominated by stale internal expectations after the newer generic-byte
      tags, paired preview-call patch spans, large-slot lowering shapes, and
      explicit Memory-SSA entry-memory dumps landed. The current tree now has
      those internal byte/object/reloc, runtime/observe snapshot, CLI smoke,
      and Memory-SSA bridge expectations refreshed together, so the remaining
      repository-internal maintenance tail is narrower: `test-value-ssa-alloc`
      still needs a clean end-to-end rerun under a runner/latency-aware check
      before the full internal `make test` sweep can be called fully reclosed.
      The latest 2026-05-07 follow-up also split that allocator tail into two
      more concrete pieces instead of leaving it as one fuzzy “alloc test is
      weird” note. First, the earlier parallel `make test` `Text file busy`
      failure was materially a runner problem, not an allocator correctness
      symptom; current test runners now use unique `mktemp`-backed copy paths
      so that collision class is no longer the leading issue. Second, there
      was a real allocator-policy regression on the focused retry-family
      slices `VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE` and
      `VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE`: hand-built test plans
      without `value_item_indices` caused
      `value_ssa_allocator_plan_find_value(...)` to fail, which silently hid
      family-pressure facts and let retry recovery comparisons fall through to
      raw `value_id` order. The current tree now restores a linear-scan
      fallback for manual plans without index tables, and focused repro wrappers
      again confirm the intended reasons on those slices
      (`family-pressure` for the dual-evict case and
      `retry-generic-eviction` for the lighter-blocker-family case). Current
      authority is therefore that the allocator correctness tail is narrower
      again, while the still-open part of `test-value-ssa-alloc` is mainly its
      aggregate runtime/latency rather than an unresolved retry-policy
      correctness mystery.
      A later 2026-05-07 follow-up tightened that diagnosis further and also
      corrected the earlier "mostly latency only" optimism. The focused
      move-engine/family-freeze witness line is materially healthier again in
      the current tree: the previously non-progressing
      `VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE` path now exits the repeated
      `freeze ssa.3` loop, the paired
      `VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY` witness is back on its intended
      released-vs-freeze ordering, and the already-reopened retry-family
      focused witnesses still recheck green together
      (`RETRY-DUAL-EVICT-FAMILY-PRESSURE`,
      `RETRY-BLOCKER-FAMILY-PRESSURE`,
      `RETRY-PREFERRED`). The key kept implementation change is that
      move-engine queue refresh now forces a broader reseed around frozen-path
      transitions instead of relying only on narrow affected-root refresh, so
      "freeze consumed but neighboring family state stayed stale" no longer
      traps the planner in the same tiny family slice.
      A later same-day follow-up then reclosed that broader allocator tail too.
      Two more manual-artifact fallbacks were restored on the same principle as
      the earlier `plan_find_value(...)` repair:
      `value_ssa_allocator_coalesce_analysis_find_pair(...)` now linearly scans
      hand-built coalesce rows when no pair index table exists, and
      `value_ssa_allocator_move_worklist_find_root(...)` now linearly scans
      hand-built move-work rows when no root-index table exists. In parallel,
      the no-move allocator-plan fast path was tightened so it still respects
      coalesced effective degree / coalesce class facts instead of flattening
      every manual no-move case back to raw degree/class-1 planning.
      Focused rechecks then materially reclosed the whole earlier cluster:
      `coalesce-opportunity-agenda`, `move-transition` family/coalesce
      witnesses, `spill-family-tie`, targeted/weighted recolor witnesses, and
      the `RETRY-PREFERRED` manual path are green again after updating that
      test's expected preferred-color source to the more precise
      `coalesce-direct(ssa.4)` surface now that direct pair lookup works again.
      Most importantly, the aggregate internal allocator suite is green again
      in the current tree: direct rerun of
      `./build/value_ssa/value_ssa_alloc_test` now passes, and adjacent
      internal rechecks stay green too (`test-memory-ssa-regression`,
      `test-compiler-driver`). Current authority is therefore that this
      reopened allocator-maintenance tail is reclosed again, and the next
      internal sweep can move back up to the broader repository `make test`
      level instead of staying pinned on allocator-only cleanup.
      A follow-up rotated correctness sweep on the same tree now also widens
      the external/public confidence surface immediately after that reclosure
      instead of leaving it implied by older checkpoints: the full course
      `autotest -riscv /workspaces/compiler_lab` baseline is green again
      (`130/130`), `indigo/test_codes/functional_test` is green again
      (`111/111`), `minic-test-cases-2021s/functional` is green again
      (`112/112`), and the wider
      `sysy-testsuit-collection/lvX` sweep is green again end-to-end
      (`467/467`). Current authority is therefore that the allocator-tail
      repair did not only restore repository-internal self-consistency; it
      also survived one fresh rotated course/external revalidation round.
      It is also worth preserving the intermediate state explicitly because
      that is the snapshot the user used to triage the line: at the earlier
      2026-05-07 broad sweep point, `make test` itself was still visibly red
      with failures concentrated in exactly
      `machine-bytes`, `machine-object`, and `machine-reloc`, even while the
      course/public/external user-facing surfaces were already mostly green.
      That asymmetry is now part of project memory on purpose: it explains why
      the next implementation step after the wide public sweep was “repair the
      repository’s own artifact/backend regression expectations” rather than
      “keep chasing new public functional breakage.”
    - a fourth experiment also failed and should now be treated as another
      closed direction rather than fresh unexplored ground: a late
      `machine_select`-cleanup strength reduction that rewrote
      `mul reg, 2^k` into `shl reg, k` before bytes lowering. This looked
      attractive because `03_sort2` contains many `*4`-style address/index
      expressions and the change lives late in the backend, but in the current
      tree it still produced the wrong tradeoff: `03_sort2` remained
      `RUN_TIMEOUT`, while the compile-side `hoist-2` baseline again regressed
      into the mid-`16s` range. That experiment has now also been removed
      again, and the current rolled-back baseline is back to
      `test-machine-select` green, `test-compiler-driver` green, and
      `hoist-2` around `12.5s`. Current authority is therefore even sharper:
      the next performance move should not be another blind local rewrite in
      either the `-perf` SSA entry path or the late selected-op cleanup path;
      we now need a more profiling-driven or statistics-guided next step.
    - profiling has now been refreshed to support that next step instead of
      relying only on intuition. Fresh standalone measurements with
      `tools/profile_compile_pipeline.py` show the current `hoist-2.sy`
      baseline at roughly `13.56s` for the normal `-riscv` path and
      `13.25s` for the `-perf` path, so the present perf reopen is no longer
      dominated by a giant hidden compile-time regression from the latest
      experiments. A fresh opcode histogram for `03_sort2.sy` also shows the
      expected hot classes directly in the generated text: `lw/sw/li/add`
      dominate, `mul` still appears `34` times, and `call` still appears
      `15` times. Current authority is therefore that the runtime tail is
      still likely indexed-address / repeated-load heavy rather than being a
      pure control-flow issue, and the next performance move should focus on
      a late, locality-aware index/address optimization rather than more
      entrypoint-level SSA passes.
    - one later backend-side micro-optimization round has now also produced a
      real, measurable code-quality win on that same `03_sort2` path, even
      though it has not yet closed the final runtime timeout. The current
      `riscv32-preview` bytes path now uses the architectural zero register
      for `store_local_imm 0` / `store_global_imm 0` instead of first
      materializing a temporary zero, small `addr_local` forms now lower
      directly to `addi rd, sp, imm` when the slot offset already fits the
      signed-12-bit immediate range, and zero-valued prepared operands may
      now flow through the later operand-prep path as `x0` instead of as an
      extra synthesized scratch register. Direct rebuilt-CLI reruns now show
      the public `03_sort2.sy` text histogram moving from the earlier
      `total_instructions=608`, `li=139`, `add=74`, `addi=24` baseline down
      to roughly `total_instructions=523`, `li=54`, `add=44`, `addi=54`
      while preserving the same broad hot families (`lw=133`, `sw=106`,
      `mul=34`, `call=15`). The public runtime result is still not closed:
      the same single-case sweep under `tools/sweep_sysy_suite.py` continues
      to reproduce `03_sort2.sy: RUN_TIMEOUT` at the current `60s`
      per-case budget. Current authority is therefore that late backend
      cleanup can still buy real instruction-count wins on the public hot
      case, but the remaining tail is now specifically the unresolved
      `addr_local + zero-index add` / repeated indexed-address family rather
      than the already-closed zero-store materialization family.
    - the compile-side baseline has now also been rechecked after the latest
      late text/export peephole rounds so this line is not relying only on
      older allocator-era numbers. Current `tools/profile_compile_pipeline.py`
      reruns keep `hoist-2.sy` in the same band rather than showing a new
      regression (`full-compiler ~= 13.50s`, `full-perf ~= 13.57s` in the
      present tree), which means the recent final-text cleanups are not
      secretly paying for their small `03_sort2` wins by reopening the old
      compile-time pressure.
    - one extra post-checkpoint text-side micro-optimization candidate has
      now also been tried, measured, and explicitly rejected so we do not
      keep rediscovering it later under a new name. I tested a very narrow
      "dead immediate" final-text cleanup that removed `li rd, imm` when the
      very next instruction redefined the same destination register without
      reading it. In practice this was the wrong tradeoff for the current
      tree: it did not improve the public `03_sort2` hot-case static shape
      (the rebuilt text remained at `TOTAL=491` in that trial), while the
      compile-side `hoist-2` baseline drifted the wrong direction
      (`full-compiler ~= 14.06s`, `full-perf ~= 13.82s`). That experiment has
      now been backed out again. The current live post-rollback snapshot is
      slightly better on the static side (`03_sort2` text back to
      `TOTAL=489`, `li=15`) while keeping `test-compiler-driver` green, and
      the refreshed compile-side baseline is now roughly
      `full-compiler ~= 14.09s`, `full-perf ~= 13.37s`. Current authority is
      therefore explicit: **do not spend the next performance step on generic
      adjacent-line dead-immediate cleanup**; it was measurable, safe enough
      to test, and still not worth keeping.
    - one follow-up widening of the existing `zero_adds` text cleanup has now
      also been tried and rejected for the same reason. I tested a broader
      variant that also tried to fold the self-overwrite form
      `li rd, 0 ; add rd, base, rd` into `mv rd, base` when the zero-valued
      register was not used again before redefinition. That variant stayed
      correctness-green on `test-compiler-driver`, but it still did not move
      the public `03_sort2` static shape off the same `TOTAL=489` plateau in
      a meaningful way and pushed the compile-side perf path the wrong way
      (`hoist-2` refresh during the trial was about
      `full-compiler ~= 13.67s`, `full-perf ~= 14.19s`). It has now also been
      backed out. Current authority is therefore even narrower: **do not keep
      iterating on text-export-only variants of the current zero-add folding
      family**; that neighborhood is now measured enough to demote in favor of
      more structural machine-layer opportunities.
    - one first truly structural follow-up on that same hotspot family has now
      also been probed and then paused rather than landed: I tested teaching
      `machine_bytes` to lower `ALU_IMM` subtraction by immediate as a direct
      `addi rd, rs, -imm` when the negated immediate still fits the signed
      12-bit RV32I form. That *is* the right long-term structural direction
      for patterns such as `sub x, x, 1` that still appear in `03_sort2`, but
      in the current tree that change immediately collides with a broader
      byte-layout / test-contract surface: `test-machine-bytes` reopened on
      multiple fixed-byte-shape expectations as soon as that immediate form was
      admitted. Current authority is therefore not "never do sub-imm
      immediateization", but rather: **do not land this optimization as an
      isolated local tweak during the current reopen**. If we revisit it, it
      should be done as a deliberate machine-bytes contract refresh with the
      associated test / byte-shape / patch-offset expectations updated
      together, not as a quick one-line encoding shortcut.
    - one adjacent machine-select-side probe has now also been tried and then
      parked without landing. The promising idea was to fold the obvious
      zero-offset local-address pattern
      `addr_local slot ; imm 0 ; add base, zero ; load/store_indirect`
      back into direct `load_local` / `store_local`, because `03_sort2`
      still shows several of those sequences at the selected-program level.
      The observation itself is real and still useful as roadmap memory: that
      pattern exists in the current selected dump and is a better structural
      target than more text-export peepholes. But the first cleanup attempt
      did not actually hit the live `03_sort2` residuals under the current
      selected shapes, so it produced no kept win and has been fully backed
      out. Current authority is therefore: **keep zero-offset local-address
      folding on the candidate list, but do not keep half-working selected
      cleanup scaffolding in the tree**. If we revisit it, start from the
      real `03_sort2` selected-form witnesses rather than from a guessed
      adjacent-op shape.
    - that same conclusion has now been strengthened by a second pass over the
      exact live witnesses rather than only a first guess. I re-ran the
      `03_sort2` selected dump after instrumenting the obvious
      `addr_local + imm 0 + add + load/store_indirect` family and confirmed
      that the residuals still do exist, but the concrete shapes in the hot
      blocks are more specific and less uniform than the naive adjacent-op
      cleanup assumed. A tightened selected-cleanup attempt that only targeted
      zero-offset local indirects still failed to produce a kept win on the
      real hot blocks and has also been fully backed out. The refreshed
      compile-side baseline after unwinding that probe remains in the same
      band (`hoist-2` roughly `full-compiler ~= 13.89s`,
      `full-perf ~= 13.82s`) with `test-compiler-driver` green again.
      Current authority is therefore sharper than before: **the next
      performance step should begin from one recorded live witness block and
      derive the exact structural fold from that witness, not from a generic
      "zero offset local indirect" slogan alone**.
    - one more cleanup-side root-cause check has now also been completed and
      should narrow future search space. I patched the selected-cleanup
      use/rewrite accounting so `load_indirect` / `store_indirect` address
      operands are finally counted as real uses for copy/materialize
      forwarding, which removes one obvious blind spot from the current
      cleanup infrastructure. That change is correctness-safe enough to keep
      (`test-compiler-driver` stays green), but it still did **not** move the
      live `03_sort2` selected witnesses or the downstream static profile by
      itself. Current authority is therefore even more specific: the remaining
      `03_sort2` residuals are not just "cleanup forgot indirect addresses";
      there is at least one additional structural barrier in the actual
      selected shapes. The next reopen step should therefore inspect one exact
      hot witness end-to-end instead of continuing to widen generic selected
      cleanup heuristics.
    - one final text-side sanity check has now also been closed out so we can
      stop second-guessing that line. I tightened the `zero_adds` witness
      logic specifically for the live `li a0, 0 ; add a0, a1, a0 ; lw ...`
      shape, on the theory that the zero temp should count as dead once the
      `add` itself overwrites `a0`. That hypothesis turned out not to be the
      real bottleneck for the current tree: the rebuilt `03_sort2` static
      profile still stayed on the same `TOTAL=491`, `li=17` plateau, while
      `test-compiler-driver` stayed green. Current authority is therefore now
      explicit on this branch too: **the next performance step should not
      spend more time on `compiler_driver` text-side `zero_adds` heuristics**.
      The remaining residual is more likely to require an earlier emit/bytes
      structural specialization than another tweak to the final pretty-printer
      peephole.
    - one last "maybe the main cleanup was simply gated off" hypothesis has
      now also been tested and rejected. I temporarily reopened the top-level
      `machine_select_cleanup_program(...)` gate for indirect-memory selected
      programs after fixing indirect-address use accounting, but the live
      `03_sort2` selected witnesses still did not change in the expected hot
      blocks. That means the current barrier is not just "cleanup never ran on
      indirect-memory programs"; there is still at least one later-layer
      structural reason those zero-offset address patterns survive. This probe
      has been fully backed out again. Current authority is therefore explicit:
      **do not spend the next round reopening broad selected-cleanup gates**.
      The next credible move remains a downstream specialization derived from
      one exact surviving witness.
    - that downstream witness has now been pinned down one layer more
      precisely. I traced the representative `03_sort2` residual
      `addi a1, sp, 144 ; li a0, 0 ; add a0, a1, a0 ; lw ...` back through the
      emit/layout dumps and confirmed it is **not** a bytes-side invention.
      The current selected/layout/emit chain really is carrying the generic
      pattern `addr_local slot ; imm 0 ; add ; load_indirect/store_indirect`
      all the way down to bytes, which then faithfully lowers it into the
      final `li 0 ; add ; lw/sw` text shape. Current authority is therefore
      sharper again: **the next optimization should not start in
      `machine_bytes` as a print-time special case**. It should begin at the
      selected/emit-side representation boundary by canonicalizing zero-offset
      local-address indirects into direct local slot operations before bytes
      lowering.
    - one small but useful control experiment has now also closed a false lead
      in that same area. I added a tiny compiler-driver regression that checks
      a simple local-array zero-offset witness does **not** keep the full
      `li 0 ; add base, 0 ; lw` text sequence in the final RISC-V output. That
      test passes in the current tree, which means the simplest zero-offset
      local-address case is already being handled well enough. Current
      authority is therefore narrower again: the stubborn `03_sort2` residuals
      are not the plain "local slot at offset 0" case, but a more complex
      witness involving additional address/value composition (for example
      spill-carried bases, bucket-array address chains, or repeated composed
      local-array roots). The next performance step should therefore mine that
      *more complex* witness directly instead of spending more time on the
      already-green trivial zero-offset case.
    - that control result is now reinforced by one direct end-to-end witness
      check. The new compiler-driver regression for the simple local-array
      case stays green together with `lv9`, so the plain
      `addr_local + imm 0 + add + load/store_indirect` shape is no longer a
      productive debugging target by itself. Current authority is therefore
      specific enough to guide the next round: **skip the trivial local-root
      witness entirely and move straight to the complex `03_sort2` survivors**
      such as spill-carried bucket roots and repeated array-root compositions
      around `getNumPos(...)`.
    - the first debugging tool for that complex-witness line is now also in
      place: `tools/analyze_machine_emit_addr_local.py` scans one
      machine-emit dump and summarizes repeated `addr_local` roots, explicit
      zero-offset root witnesses, and rough root-use counts. On the current
      `03_sort2` emit dump it narrows the practical search space to a tiny
      set immediately: zero-offset witnesses exist only for `local.4`,
      `local.20`, and `local.36`, and the hottest root by simple use count is
      `local.4` (`14` uses, vs `8` for `local.20` and `4` for `local.36`).
      Current authority is therefore now concrete enough for the next
      implementation round: **start from the `local.4` witness family first,
      then widen to `local.20` / `local.36` only if needed**.
    - one first selected-side structural specialization on that exact
      `local.4` witness family is now landed instead of only discussed. The
      current tree adds a narrow block-local root-reuse pass in
      `machine_select` that can run even when indirect-memory ops are still
      present: repeated `addr_local` / `addr_global` materializations for the
      same slot may now reuse one earlier still-live result instead of
      rematerializing the root each time. This is intentionally *not* a broad
      reopening of the old selected-cleanup gate; it is a small direct pass
      aimed at the real `03_sort2` hotspot shape. A new repository-local
      debug helper, `tools/dump_machine_stage.c`, now also makes the
      `machine-ir` / `machine_select` / `machine_emit` / preview-bytes dumps
      reproducible from one SysY input instead of relying on transient `/tmp`
      binaries.
    - that same narrow root-reuse pass now has direct evidence on the live
      `03_sort2` path rather than only a synthetic unit test. In the current
      `machine_select` dump, `radixSort` drops from `288` selected ops to
      `281`, `bb.17` drops from `41` ops to `38`, and the earlier repeated
      `spill.3/4/5/7 = addr_local local.4` chain now reuses one stable
      `spill.3` root for the later three local-array-root compositions
      instead of rematerializing the same base four times around
      `getNumPos(...)`. The current rebuilt RISC-V text snapshot on the same
      case is now down again to about `479` total instructions while keeping
      `li=12`. Guardrails are currently green after landing that slice:
      `make test-machine-select`, `make test-compiler-driver`, and
      `git diff --check`.
    - one immediate follow-up on that same line has now also been completed so
      future rounds do not have to rediscover the same safety/perf boundary.
      I split a second, more conservative selected-side cleanup candidate out
      from the older broad gate: block-liveness for selected cleanup now also
      counts `load_indirect` / `store_indirect` operands, and a pure
      value-only cleanup subpass may now run before the broader
      non-indirect-memory cleanup. Current measured result is deliberately
      modest: it does **not** improve the live `03_sort2` static checkpoint
      beyond the same `479`-instruction plateau, so it should be treated as
      correctness-preserving cleanup infrastructure rather than as a new
      performance win by itself. The important part is that current guardrails
      still stay green after widening the validation surface around it:
      `make test-machine-select`, `autotest -riscv -s lv9
      /workspaces/compiler_lab` (`22/22`), full
      `minic-test-cases-2021f/functional` (`100/100`), and
      `git diff --check` are all green in the current tree.
    - a follow-up tightening of the selected pure-value forwarding rule is
      also now complete and likewise remains correctness-green on the wider
      validation surface. The current rule is intentionally stricter about the
      last visible use of a forwarded operand before allowing the copy to
      disappear, which keeps the recent `03_sort2` block-local value cleanup
      from turning into an over-eager alias skip. The current tree still
      preserves the same `479`-instruction public snapshot, but the
      correctness evidence now includes a rechecked `machine-select` test
      suite, `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`), a
      full `minic-test-cases-2021f/functional` sweep (`100/100`), and
      `git diff --check` in the same tree.
    - one more very narrow cleanup is now also landed and validated in the
      same safety-first style: selected cleanup now removes pure `copy`
      self-noops directly and can also drop one redundant same-result
      `addr_local` redefinition when the exact same slot is materialized again
      into the same resource before use. This did not move the public
      `03_sort2` text checkpoint off the same `479`-instruction plateau, but
      it did reduce selected noise in the live dump and remains green on the
      expanded validation surface: `make test-machine-select`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`),
      `minic-test-cases-2021f/functional` (`100/100`), and `git diff --check`.
    - one final follow-up in that same cleanup/debugging family has now also
      been tried and bounded. I taught selected cleanup to fold the immediate
      pair `addr_local/addr_global -> copy` when the original address result
      has no later visible use beyond that copy, and I regression-locked both
      the plain `copy self` no-op removal and the `addr_local -> copy ->
      final use` fold shape in `test-machine-select`. This is correctness-
      green on the same local evidence surface (`machine-select`, `lv9`,
      third-party functional sample), but it still does **not** push the live
      `03_sort2` text image below the same `479`-instruction checkpoint.
      Current authority is therefore that the selected-side “obvious copy
      noise” line is now much better debugged and partly cleaned, but is no
      longer the most likely remaining source of another big public perf win
      by itself.
    - one more debugging pass now narrows the likely next structural win more
      cleanly than the earlier selected-noise notes did. The live selected
      dump no longer keeps the earlier obvious `copy self` clutter after the
      latest cleanup refinements, but the final RISC-V text still stays at the
      same `479`-instruction checkpoint. A new helper,
      `tools/analyze_riscv_call_moves.py`, shows that many of the remaining
      visible `mv` clusters around `getNumPos(...)` / `radixSort(...)` are now
      ordinary call-argument permutation cycles (for example a `2`-arg swap or
      a `4`-arg cycle) rather than accidental selected-noise leftovers, so
      that line is less likely to hide another large cheap win by itself.
      In contrast, another new helper,
      `tools/analyze_machine_select_remat_spills.py`, isolates just two tight
      same-block rematerializable spill-root candidates in the live
      `03_sort2` selected dump:
      `bb.17 spill.3 = addr_local local.4` (`4` same-block uses) and
      `bb.5 spill.0 = addr_local local.36` (`2` same-block uses). Current
      authority is therefore that the next more promising backend-side probe
      is no longer “more selected copy cleanup”, but a narrow bytes-side or
      late-lowering experiment around rematerializing these spill-backed local
      array roots instead of always materializing them into stack slots and
      loading them back.
    - that narrower rematerialization probe is now also landed as a first
      text-side pass and has produced a real, measured improvement on the live
      `03_sort2` checkpoint instead of only a theoretical one. The current
      final-text post-pass now recognizes one narrow stack-address reuse shape
      and rewrites the easy local-address rematerialization pairs directly in
      the emitted `.s`, which moves the live public `03_sort2` image down from
      the older `479`-instruction plateau to about `456` total instructions
      while keeping the same broad hot families and preserving the current
      correctness surfaces. Fresh reruns after that change are still green on
      `make test-compiler-driver`, `autotest -riscv -s lv9
      /workspaces/compiler_lab`, `minic-test-cases-2021f/functional`,
      `minic-test-cases-2021s/functional`, `compiler2021/.../function_test2021`,
      `indigo/test_codes/functional_test`, `lava-test/cases`, and
      `TrivialCompiler/custom_test`; `tools/profile_compile_pipeline.py` now
      keeps `hoist-2.sy` in the same acceptable band (`full-compiler
      ~= 15.259s`, `full-perf ~= 14.220s`) rather than exposing a regression.
    - one follow-up final-text/local-root fold has now also landed on top of
      that same safe late-export line, but this round importantly included a
      real external-suite correctness correction before it was kept. The new
      peephole folds the narrow scratch-root shape
      `addi/mv t*, sp, K ; ... ; add addr, t*, idx ; lw value, 0(addr)` into
      `add addr, sp, idx ; lw value, K(addr)` so the final `.s` stops paying
      an extra stack-root materialization on some of the surviving
      `03_sort2` indexed local-array witnesses. A first broader attempt also
      touched non-scratch base registers and immediately reopened
      `TrivialCompiler/custom_test/array.sy` (`stdout=""`, `exit=169`), so
      current authority is explicit: keep this optimization **only** on the
      scratch-root (`t*`) subfamily for now, and keep the new
      `compiler_driver_test` regression that blocks the array-parameter
      misfold. With that safety narrowing in place, the direct rebuilt-CLI
      `03_sort2` histogram moves one more step from `456` down to about
      `452` total instructions (`addi: 54 -> 50` while the main hot families
      otherwise stay broadly stable), and the rechecked correctness surface is
      green again on `make test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab`, and the full
      `TrivialCompiler/custom_test` external suite (`29/29`). A later rotated
      rerun also keeps `indigo/test_codes/functional_test` green end-to-end
      (`111/111`), so this kept version is no longer relying on only one
      external family after the array-parameter correction. The compile-side
      guardrail also stays acceptable after this kept variant and even nudges
      slightly better again on the current local machine:
      `tools/profile_compile_pipeline.py` now reports `hoist-2.sy` around
      `full-compiler ~= 13.030s`, `full-perf ~= 13.248s`.
    - one more final-text call-adjacent fold has now also landed on the same
      `03_sort2` line, and this one removes a real repeated ABI-shuffle
      pattern instead of only another address-root rematerialization. The
      current preview text exporter now recognizes the narrow sequence
      `lw a1, ... ; lw a0, ... ; mv t5, a0 ; mv a0, a1 ; mv a1, t5 ; call ...`
      and rewrites it into the equivalent direct call-argument load order
      `lw a0, ... ; lw a1, ... ; call ...`, eliminating the three-register
      swap helper when the values were about to be passed to the call anyway.
      A focused repository-local regression now locks that shape directly in
      `compiler_driver_test`, and the live public hot repro benefits from it
      in several places around the repeated `getNumPos(...)` cluster. On the
      rebuilt `03_sort2` text image, the current stable checkpoint now moves
      from about `452` total instructions down to about `434`, with `mv`
      dropping materially from `29` to `11` while the rest of the hot opcode
      families remain broadly stable. Rechecked correctness is still green on
      `make test-compiler-driver`, `autotest -riscv -s lv9
      /workspaces/compiler_lab`, and the full `TrivialCompiler/custom_test`
      external suite (`29/29`). Current compile-time authority is more
      nuanced than the earlier `hoist-2` guardrail alone: a later control
      measurement on `lava-test/cases/107_long_code2.sy` still shows a slow
      compile path (`~32s..35s`), but a temporary local disable/re-enable
      experiment on this new call-swap fold confirms that slowdown is **not**
      uniquely caused by this fold itself. In practice that means the `434`
      `03_sort2` checkpoint is worth keeping for now while compile-time
      follow-up continues on the broader large-text path rather than by
      immediately reverting this specific runtime-side win.
    - the compile-side reopen around `lava-test/cases/107_long_code2.sy` now
      also has a first real stage breakdown instead of only wall-clock
      suspicion. A new repository-local probe,
      `tools/profile_compiler_stages.c`, times the major pipeline slices from
      source text through final compiler text export. On the current rebuilt
      tree, the big cost on `107_long_code2.sy` is **not** the final preview
      text pretty-printer/peephole tail: the measured stage sum is already
      about `30.316s`, the full `-riscv` compile is about `30.374s`, and the
      estimated post-`machine_bytes` text-export tail is only about `0.058s`.
      The real hot layers are upstream:
      `value_ssa ~= 6.275s`, `machine_ir_report ~= 9.602s`,
      and `machine_select ~= 14.385s`, while `machine_layout`,
      `machine_emit`, and `machine_bytes` are effectively negligible on this
      case. Current authority is therefore explicit: the next compile-time
      optimization step for this large-text reopen should focus on the
      `value_ssa -> machine_ir_report -> machine_select` line, not on further
      tuning the final RISC-V text-export peepholes.
    - that same compile-time diagnosis is now also one layer more specific on
      the selected side instead of stopping at the coarse `machine_select`
      bucket. The selected lowering path now supports an opt-in timing trace
      (`MACHINE_SELECT_TRACE_TIMING=1`) that reports its major internal
      phases. On repeated `107_long_code2.sy` measurements, the selected-side
      cost is **not** clone/lower or verifier overhead: those stay around
      `0.005s`. The dominant selected hotspot is the pure cleanup loop itself
      (`cleanup_pure ~= 12.2s..12.5s`), with the narrower address-root reuse
      pass a distant second (`cleanup_reuse_addr_roots ~= 1.8s` in the common
      runs). Current authority is therefore sharper than before: the next
      compile-time optimization step should begin inside
      `machine_select_cleanup_pure_program(...)`, especially its iterative
      per-function fixed-point loop and the helpers it repeatedly calls, not
      inside selected lowering construction, verifier entry, or the
      downstream layout/emit/bytes layers.
    - that same `cleanup_pure` hotspot is now also broken down into its real
      inner costs instead of being only one big 12-second box. On the live
      `107_long_code2.sy` repro, `live_out` recomputation itself is almost
      free (`~0.001s`) and the canonicalize/copy-self/terminator subpasses are
      negligible too. The first fixed-point iteration is dominated by
      `forward_trivial_defs_in_block(...)` (`~5.5s..7.8s`) plus
      `remove_dead_defs_in_block(...)` (`~1.7s..2.5s`), with
      `remove_redundant_addr_defs_in_block(...)` a smaller but still visible
      secondary cost (`~0.3s..0.8s`). The second fixed-point iteration then
      keeps paying another `~2s+` mostly on
      `remove_dead_defs_in_block(...)` after `forward_trivial_defs_in_block(...)`
      has already dropped to zero. Current authority is therefore sharper
      again: if we want the first low-risk selected-side compile-time win, the
      best default target is the repeated whole-block scans in
      `forward_trivial_defs_in_block(...)` and
      `remove_dead_defs_in_block(...)`, not liveness math itself.
    - the first “what if we simply do not run `cleanup_pure` on this
      pathological shape?” diagnostic probe is now also concrete enough to use
      as roadmap memory. I added opt-in environment toggles
      `MACHINE_SELECT_SKIP_CLEANUP_PURE=1` and
      `MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS=1` so the current tree can compare
      costs without changing default behavior. On `107_long_code2.sy`, the
      `skip_cleanup_pure` probe collapses selected time from the usual
      `~14s` band down to about `3.4s`, and the full `-riscv` compile drops
      from the low-`30s` band to about `21.9s`, while the selected artifact
      summary stays effectively unchanged (`ops=16005`, `blocks=1`,
      `spills=1`, same byte count). Current authority is therefore that
      `cleanup_pure` really is the dominant compile-time regression surface on
      this extreme single-block/no-call giant spill program, and a future
      default heuristic that suppresses or narrows that cleanup on similarly
      pathological shapes is a credible next move rather than only a
      speculative idea.
    - that heuristic direction now also has a stronger upper-bound data point
      instead of only the earlier “skip pure” half-step. On the same
      `107_long_code2.sy` extreme single-block giant-spill case, the combined
      diagnostic probe
      `MACHINE_SELECT_SKIP_CLEANUP_PURE=1 MACHINE_SELECT_SKIP_REUSE_ADDR_ROOTS=1`
      reduces selected time to effectively zero (`machine_select ~= 0.006s`)
      and pulls the full rebuilt `-riscv` compile down to about `19.9s`,
      which is finally back under the `20s` sweep budget. The selected
      artifact summary still stays unchanged on this probe
      (`ops=16005`, `blocks=1`, `spills=1`, same byte count), so current
      authority is that the heavy selected cleanup passes are not buying any
      meaningful selected-IR size change on this pathological shape. The next
      practical step is therefore no longer “prove whether a cleanup-skip
      heuristic might matter”; it is “turn that extremely narrow giant-single-
      block heuristic into a default kept rule, then remeasure the real suite
      and watch for correctness drift”.
    - that giant-single-block selected-cleanup heuristic is now also landed on
      the default path in a deliberately narrow first kept form instead of
      only as a diagnostic environment override. Current `machine_select` now
      skips both `cleanup_pure` and `reuse_addr_roots` automatically on the
      same pathological shape we had been probing: one-function, one-block,
      no-call, huge-spill selected programs
      (`spill_slot_count >= 20000`, `op_count >= 12000`) that end in a
      return-like terminator. Fresh reruns keep the nearby correctness surface
      green on `make test-compiler-driver`, `autotest -riscv -s lv9
      /workspaces/compiler_lab` (`22/22`), and the full
      `TrivialCompiler/custom_test` suite (`29/29`). More importantly, the
      original large-text compile repro is now materially reclosed on the
      default compiler path rather than only in diagnostic mode:
      `tools/profile_compile_pipeline.py` now reports
      `107_long_code2.sy` around `full-compiler ~= 18.551s`,
      `full-perf ~= 18.453s`, and the direct
      `tools/sweep_sysy_suite.py --filter 107_long_code2.sy` rerun is green
      again under the default `20s` per-case budget. The public runtime-side
      checkpoint stays intact on the same tree too: `03_sort2` remains at the
      kept `434`-instruction text image. Current authority is therefore that
      this first giant-case selected-cleanup heuristic is worth keeping, and
      the next compile-time follow-up should now shift from “can we recover
      `107_long_code2` at all?” toward the still-open upstream hotspots in
      `value_ssa` and `machine_ir_report`, plus any broader-but-still-safe
      generalization of the same cleanup heuristic if later evidence supports
      it.
    - the first `machine_ir_report`-side follow-up has now also landed and
      produced a much larger compile-time win than the earlier selected-side
      heuristic alone. The hot giant case still had no `VALUE_SSA_INSTR_CALL`
      at all, yet `machine_ir_lower_function_from_value_ssa(...)` always ran
      `machine_ir_build_protected_call_crossing_spill_slots(...)`, which in
      turn forced a full `value_ssa_compute_allocation_prep(...)` pass only to
      discover there were no call-crossing values to protect. The current tree
      now short-circuits that whole protected-spill path when the function has
      no call instructions. On `107_long_code2.sy`, that drives the rebuilt
      default compiler from the earlier high-teens/low-twenties band down to
      about `full-compiler ~= 8.777s`, `full-perf ~= 8.703s`, while keeping
      the nearby correctness surface green (`test-compiler-driver`, `lv9`,
      `TrivialCompiler/custom_test`) and preserving the kept
      `03_sort2 = 434` text checkpoint. Updated `MACHINE_IR_TRACE_TIMING=1`
      evidence now shows the remaining `machine_ir_report` cost is largely
      concentrated in `lower-machine-ir` itself (`~8.7s..10.4s`) plus a much
      smaller `build-layout` slice (`~0.8s..0.9s`), while
      `all-spill-result`, `attach-layout-context`, and `build-machine-view`
      are effectively negligible. Current authority is therefore that the
      old `machine_ir` hotspot has materially narrowed, and the next
      compile-time step should shift further upstream rather than reopening
      the already-closed protected-call-crossing spill-prep path.
    - the next upstream compile-time follow-up has now also landed in a very
      narrow kept form, and it materially helps the same giant public stress
      line without broadening the default Value-SSA path for ordinary
      programs. `value_ssa_build_default_from_lower_ir(...)` now bypasses the
      heavy default memory-full canonicalization bridge only for an extreme
      straight-line shape:
      one function, one block, no calls, direct `return`, no indirect-memory
      ops, and a very large temp/instruction count (`next_temp_id >= 12000`,
      `instruction_count >= 12000`). On the live
      `lava-test/cases/107_long_code2.sy` repro, fresh reruns now keep the
      rebuilt CLI in the high-7s/low-8s band instead of the earlier
      high-8s/low-9s band:
      `tools/profile_compile_pipeline.py` now reports about
      `full-compiler ~= 7.811s`, `full-perf ~= 7.889s`, while a direct stage
      probe shows the remaining hot slices at roughly
      `value_ssa ~= 4.852s` and `machine_ir_report ~= 0.753s`.
      Nearby correctness evidence stays green on the same tree:
      `make test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`),
      `indigo/test_codes/functional_test` (`111/111`),
      `tools/sweep_sysy_suite.py --filter 107_long_code2.sy`,
      and the kept `03_sort2 = 434` text checkpoint. Current authority is
      therefore that the giant compile-time reopen is now mostly a
      `value_ssa`-side default-bridge / giant-shape policy question rather
      than a `machine_ir` lowering emergency, while the separate runtime tail
      `03_sort2` still remains open.
    - the current `hoist-2` compile-time line now has a much sharper
      root-cause picture and one materially stronger kept checkpoint in the
      live tree. Fresh `VALUE_SSA_TRACE_TIMING=1 + MACHINE_IR_TRACE_TIMING=1`
      probes show that `hoist-2` is not bottlenecked by the earlier
      `value_ssa` default bridge (`~0.13s..0.15s`) and not by downstream
      selected/layout/bytes work; the real cost sits inside
      `value_ssa_allocate_and_rewrite_program_single_block_spills(...)`,
      and more specifically inside allocator-plan construction for the hot
      function `func`. Current retained instrumentation now prints per-
      function allocator timing such as
      `build-step ...`, `full-step build/entry`,
      `pipeline-step plan/execute`, `execute-step select-phase`,
      `execute-step spill-slot-assign`, plus the rewrite-loop
      `reallocated-functions=M/N` summary under `VALUE_SSA_TRACE_TIMING=1`.
      That evidence shows a stable pattern on the hot function:
      `build-step total ~= 0.05s`, `pipeline-step plan ~= 2.0s..2.7s`,
      `execute-step select-phase ~= 0.19s..0.24s`, and
      `spill-slot-assign ~= ~0.005s`, while each rewrite round still
      reallocates only `1/7` functions. One first compile-time win from that
      diagnosis is now also materially closed and kept in the mainline:
      rewrite-loop reallocation may reuse the prior allocation result for
      functions untouched by the current rewrite round instead of blindly
      reallocating the whole program every time. After fully rebuilding the
      current CLI and rerunning the same public repro, the current `hoist-2`
      checkpoint is now in the low-`3s` band instead of the earlier low-teens
      band:
      `tools/profile_compile_pipeline.py` now reports about
      `full-compiler ~= 3.260s`, `full-perf ~= 3.208s`.
      Two later follow-up experiments have also now been explicitly ruled out
      by measurement and should stay recorded as dead directions rather than
      being retried by accident:
      `1)` a static no-affinity allocator-plan builder originally written with
      an O(n^2) selection sort, and
      `2)` the same no-affinity fast path after fixing that local sort to
      `qsort`; both variants stayed correctness-green but regressed `hoist-2`
      back into roughly the `~12.5s..14s` band. Current authority is
      therefore that the next compile-time step on `hoist-2` should continue
      targeting real state reuse inside the kept allocator path, not replace
      the whole plan builder with a separate static fast path just because
      the current affinity graph is sparse.
      One newer kept helper in that same line is now also worth treating as
      part of the live baseline rather than as ad hoc local debug code: the
      move-engine incremental refresh path now builds one explicit
      interference-neighbor adjacency surface and reuses it for
      active-degree / effective-degree updates, so those hot refresh sites no
      longer rescan whole interference-matrix rows just to visit real
      neighbors. Focused reruns on the current tree keep
      `test-value-ssa-regression`, `test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`),
      `minic-test-cases-2021s/functional` (`112/112`), and
      `indigo/test_codes/functional_test` (`111/111`) green, while the
      public runtime-side static witness still holds at `03_sort2 = 434`
      (`mv=11`).
      One nearby allocator bookkeeping retry has also now been explicitly
      ruled out and should stay recorded as a failed direction: I tried a
      broader root-list dedup / compaction rewrite that reused
      `scratch_root_flags` across additional move-engine refresh lists in
      hopes of trimming `plan-driver` bookkeeping. That variant stayed
      correctness-green but regressed representative `hoist-2`
      allocator-stage timings back upward (for example
      `allocate+rewrite ~= 11.6s..12.7s` on reruns), so it has been removed
      again. Current authority is therefore to keep the interference-neighbor
      helper but **not** continue the broader root-list/scratch-flag rewrite
      direction as the next compile-time move.
      One later targeted no-move fast path is now also landed and should be
      treated as the current main kept checkpoint for this line. Fresh rebuilt
      diagnostics now show that the hot `hoist-2` body function is a nearly
      pure `simplify/spill` case (`move-related-values=0`, `coalesce=0`,
      `freeze=0`), so the earlier full move-engine scheduling/refresh path
      was paying for machinery that this shape never used. The current tree
      now detects that narrow case and lets
      `value_ssa_compute_allocator_plan(...)` build a direct no-move plan
      instead of entering the full move-engine pipeline. Focused reruns keep
      `test-value-ssa-regression` green, `test-compiler-driver` green,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` green (`22/22`), keep
      the full `minic-test-cases-2021f/functional` sweep green (`100/100`),
      and still keep the public runtime-side static witness at
      `03_sort2 = 434` (`mv=11`). More importantly, fresh rebuilt timing now
      returns `hoist-2` to the same low-`3s` kept band:
      `tools/profile_compile_pipeline.py` reports about
      `full-compiler ~= 3.37s`, `full-perf ~= 3.62s`, and fresh staged traces
      now show repeated `no-move-plan total ~= 0.06s..0.08s` with
      `allocate+rewrite` dropping into roughly the `~1.7s..3.5s` band on
      representative reruns. Current authority is therefore that any further
      allocator-side compile-time tuning should start *after* this no-move
      fast path checkpoint, not before it.
    - the latest `03_sort2` runtime-side reopen has now also narrowed one more
      false lead instead of only broadening the search space. Fresh selected /
      emit / final-assembly inspection now shows the hottest remaining
      residual is no longer the plain local-array-root shape by itself, but
      the repeated same-argument `getNumPos(...)` call family inside the hot
      `radixSort` loop (for example the repeated
      `getNumPos(216(sp), 0(sp))` cluster around `LradixSort_18`). I tried a
      very narrow selected-side block-local pure-call reuse probe in the
      `machine_select_cleanup_pure_program(...)` line, but it did not produce
      a kept win and has been removed again. The direct diagnostic result was
      still useful: the current purity/arg-descriptor model only reached an
      earlier weaker witness (`block=5` arg-descriptor failures on
      load-indirect-derived call args), while the truly hot repeated-call
      cluster remained untouched. Current authority is therefore now sharper:
      **treat repeated `getNumPos(...)` calls as the real runtime residual,
      but do not continue the pure-cleanup call-cache rewrite direction as-is;
      the next credible attempt should move to the main selected/cleanup path
      or another later layer that can cache call results more explicitly
      instead of trying to reuse the overwritten call-result register.**
      That same runtime-residual line now also has a first repository-local
      witness tool so future rounds do not have to rediscover the hot shape by
      hand. `tools/analyze_machine_select_repeated_calls.py` scans one
      selected dump, traces simple call arguments back to local/global/immediate
      sources, and reports repeated same-signature call clusters per block.
      On the current `03_sort2` selected dump it already isolates one clear
      main target: `radixSort bb.17` repeats
      `getNumPos(local:54, local:0)` three extra times inside the hot loop.
      That exact witness has now also yielded one first kept mainline win in
      the live tree: a new narrow selected-side cleanup pass now runs on the
      real indirect-memory path and reuses repeated internal pure-call results
      through an explicit spill cache when the callee has a body, stays free
      of global writes, the call arguments still trace back to the same
      local/global/immediate/address descriptors, and no intervening call or
      conflicting store invalidates the cached result. This is intentionally
      narrower than the earlier rejected pure-cleanup rewrite direction: it
      does not try to reopen whole-program call CSE, and it only attacks the
      concrete repeated-helper hotspot on the path that `03_sort2` actually
      uses. The current repository-local regression now locks one focused
      indirect-memory case where the second internal call is replaced by a
      `copy spill.*` reuse instead of a second `call`.
    - that new repeated-internal-call reuse pass now also has direct live
      witness evidence on the real `03_sort2` path rather than only on the
      synthetic unit test. After rebuilding the current tree, the refreshed
      selected dump no longer reports any repeated same-signature call cluster
      from `tools/analyze_machine_select_repeated_calls.py`; the first kept
      form dropped the hot `radixSort bb.17` block from `38` ops / `4`
      `getNumPos(...)` calls down to `32` ops / `2` `getNumPos(...)` calls,
      and the whole `radixSort` function from `268` ops / `8` calls to
      `262` ops / `6` calls while preserving the same `spills=1` shape. That
      line has now advanced one more kept step too: after relaxing the
      over-conservative "any intervening `store_indirect` is a barrier"
      condition for this already-proven internal pure-call family, the live
      selected witness drops again to `radixSort bb.17 = 29` ops / `1` call
      and `radixSort = 259` ops / `5` calls. The rebuilt final RISC-V text
      now also moves below the previous protected `434` checkpoint to
      `total_instructions=430` while still holding `mv=11`; the text-side
      `call` count drops again from `13` to `12`. Current authority is
      therefore that the repeated-`getNumPos(...)` runtime residual is no
      longer only diagnosed or partially reduced; the hot `bb.17` cluster is
      now essentially collapsed to one remaining `getNumPos(...)` call on the
      live mainline.
    - that same hot-block line has now yielded one more kept follow-up on top
      of the `430` checkpoint, and this time the win is no longer about call
      reuse itself. A new narrow selected-side cleanup pass now reuses a later
      register-valued pure expression when the exact same expression has
      already been materialized into a spill slot earlier in the block; the
      comparison is block-local and recursive over the current small pure
      expression family (`copy`, `materialize_imm`, `alu`, `alui`, `cmp`,
      `cmpi`) so it can recognize cases such as "this `reg.0` is again the
      same `spill.8 << 4`-derived address component" instead of comparing only
      raw operand ids. On the live `03_sort2` witness this removes the final
      repeated `spill.8`-scaled address recomputation at the tail of
      `radixSort bb.17`, changing the last chain from a fresh
      `alui.2 spill.8, 4 ; alu.0 spill.3, ... ; load_indirect ...` sequence to
      direct `load_indirect spill.6`. That line did produce a temporary
      `427`-instruction local witness during the immediate optimization pass,
      but it did not survive the later stability screen unchanged. The current
      stable rebuilt witness after backing out the still-unsettled follow-up
      branch is therefore `total_instructions=428` with the same headline hot
      families (`call=12`, `mv=11`) instead of the briefly lower unstable
      intermediate. Current authority is therefore that the `bb.17`
      `getNumPos(...)`-adjacent residual has still been pushed down another
      layer versus the older `430` checkpoint, but the stable kept floor for
      now is `428`, not the transient `427`.
    - that newest spill-expression reuse slice has now also gone through one
      full correctness tighten-and-keep cycle instead of staying at the
      earlier "green on nearby rotated suites" level only. While widening the
      recheck surface, `minic-test-cases-2021f/functional/097_register_alloc.c`
      briefly reopened as a `MISMATCH` (`expected 194`, `actual 37`), which
      pointed straight back at the new recursive small-pure-expression reuse
      line. The practical fix was to keep that pass on the indirect-memory
      mainline it was designed for instead of letting it run on all selected
      programs: the non-indirect high-register-pressure repro returns to green,
      while the indirect-memory line stays on the same reduced hot-family
      shape. Current authority is therefore sharper than before:
      the recursive spill-expression reuse pass is now a kept
      **indirect-memory-only** optimization, not a generally enabled
      selected-side fold.
    - one deeper correctness hardening follow-up is now also landed on top of
      that gate. The recursive small-pure-expression matcher itself had an
      over-permissive equality rule: if two operands reused the same register
      or spill id but one side no longer had a matching reaching small-pure
      def at the use site, the matcher could still fall back to plain operand-
      id equality and silently treat the values as identical. A new focused
      `machine_select` regression now locks the concrete bad shape: one spill
      slot is first used to build an address-derived pure expression, then the
      same spill slot is redefined to a different address before a superficially
      similar later expression is formed. The current matcher now rejects that
      false equality unless both sides really resolve to the same reaching
      definition tree (or the exact same defining op), and current reruns keep
      both the stable `428` `03_sort2` witness and the full nearby course line
      intact.
    - one additional selected-side follow-up idea was then probed and
      deliberately *not* kept: reusing repeated same-block `load_local`
      operations for non-address-taken locals. The motivating live residuals
      were real (`bb.18` / `bb.22` / `bb.23` still contain repeated
      `load_local local.52/local.55` chains), but the first implementation
      proved too fragile before it earned a stable checkpoint. It interfered
      with already-landed repeated-internal-call cleanup coverage, and once it
      was fully backed back out the stable `03_sort2` witness settled at
      `428` rather than the briefly lower transient `427`. Current authority
      is therefore explicit: **do not treat non-address-taken load-local reuse
      as a kept optimization yet**. If that line reopens later, it should be
      rebuilt from a narrower proof/debug base instead of resumed from the
      reverted patch.
    - the next diagnostic step on that remaining runtime-side line is now also
      in-repo instead of staying conversational only. A new helper,
      `tools/analyze_machine_select_addr_loads.py`, scans one selected dump,
      recursively describes `load_indirect` / `store_indirect` address trees,
      and reports repeated address shapes per block. On the analyzed `03_sort2`
      selected witness it already narrows the next plausible residue family to
      repeated `load_indirect` address trees such as
      `alu.0(local:1, alui.2(local:52, 4))` in `radixSort bb.5` and
      `alu.0(addr_local:4, alui.2(local:52, 4))` in `radixSort bb.18`, without
      reopening the broader failed `load_local`-reuse branch. Current
      authority is therefore to use that helper as the next hotspot classifier
      before attempting another selected-side fold.
    - that hotspot-classification line now also has a second companion helper
      above raw address-only scans. `tools/analyze_machine_select_repeated_exprs.py`
      recursively describes selected-side small pure expressions
      (`copy/materialize/alu/alui/cmp/cmpi`) and reports repeated signatures
      per block. On the same current `03_sort2` witness it sharpens the next
      likely profitable residue family further: repeated
      `alui.2(local:52,4)` chains in `radixSort bb.8` / `bb.18` and repeated
      `alui.2(local:55,4)` chains in `bb.22`, plus the paired repeated
      address-tree form `alu.0(addr_local:4, alui.2(local:52,4))` in
      `bb.18`. A first attempt to keep a more general register-valued
      expression-reuse fold on top of that evidence did not improve the stable
      `03_sort2` witness beyond `428`, so current authority is to keep the new
      expression-analysis helper but not keep that no-win fold itself.
    - that same diagnostics-first line now also has a third companion helper
      for blocker inspection instead of only raw repetition counts.
      `tools/analyze_machine_select_load_reuse_blockers.py` scans consecutive
      repeated `load_indirect` address trees inside one block and prints the
      intervening operations between those repeated loads. On the current
      stable `03_sort2` witness it shows that the remaining hot address family
      is not just "one more missing trivial CSE": for
      `alu.0(local:1, alui.2(local:52, 4))` in `radixSort bb.5`, the repeat is
      separated by `call getNumPos(...)` plus several value-prep ops; for
      `alu.0(addr_local:4, alui.2(local:52, 4))` in `bb.18`, the repeated load
      is separated by a real `store_indirect(...)` into the same bucket array
      region plus more address reconstruction. Current authority is therefore
      that the next credible win should likely come from a narrower
      address-tree/result reuse pattern above those concrete blocker families,
      not from reopening broad generic load-local or load-indirect reuse.
    - that blocker-inspection line now also has a fourth companion helper for
      repeated pure-expression families themselves instead of only the final
      `load_indirect` addresses. `tools/analyze_machine_select_expr_reuse_blockers.py`
      scans repeated small pure expressions (`copy/materialize/alu/alui/cmp/cmpi`)
      inside one block and classifies the intervening blockers between each
      repeated pair. On the current stable `03_sort2` witness it sharpens the
      residue map further:
      `alui.2(local:52,4)` and `alui.2(local:55,4)` are by far the most common
      repeated expression families, but their repetitions are usually
      separated by fresh `load_local local.52/local.55`, more address-root
      rebuilding, and sometimes real `store_indirect(...)` barriers. Together
      with the earlier address/load blocker helper, current authority is
      therefore that the remaining `03_sort2` tail is now well enough
      diagnosed that the next optimization attempt should be chosen against
      one concrete blocker family rather than against "repeated expressions in
      general".
    - one additional selected-side follow-up idea was then tried and also
      explicitly *not* kept: reusing the already-computed address result of an
      earlier `load_indirect` / `store_indirect` address tree when the same
      address tree is rebuilt later in the same block. The motivating local
      shape was real (`radixSort bb.18` repeatedly rebuilds
      `alu.0(addr_local:4, alui.2(local:52, 4))`), but the first
      implementation did not even hit its focused regression in the desired
      way, which confirmed that the remaining blocker is deeper than just
      "address roots are equal". That branch has now been fully backed back
      out, and the stable current `03_sort2` witness remains `428`. Current
      authority is therefore to keep the richer hotspot diagnostics, but not
      keep that first indirect-address-result reuse pass itself.
    - the latest diagnostic pass over those helper outputs now also gives a
      more explicit ranking of which blocker families dominate the remaining
      `03_sort2` tail. `tools/classify_machine_select_hotspots.py` can now
      bucket the blocker reports into coarse families such as
      `call_barrier`, `indirect_store_barrier`, `reload_same_local`,
      `rebuild_address_root`, `nested_load_indirect`, and
      `rebuild_small_expr`. On the current stable witness, the remaining hot
      address/load and small-expression families are dominated much more by
      `rebuild_small_expr` plus repeated `load_local local.52/local.55`
      reloads than by raw calls or direct alias-unsafe stores:
      - repeated `load_indirect` address trees are currently led by
        `alu.0(addr_local:4, alui.2(local:52,4))`, where
        `rebuild_small_expr` and `reload_same_local` dominate while real
        `indirect_store_barrier` appears but is not the majority case
      - repeated pure expressions are currently dominated by
        `alui.2(local:52,4)` and `alui.2(local:55,4)`, again with
        `rebuild_small_expr` and `reload_same_local` far outweighing
        `call_barrier`
      Current authority is therefore that the next credible runtime-side
      experiment should probably target one very narrow
      `load_local local.52/local.55 -> alui.2(...,4)` reuse family first,
      before reopening broader address-tree or load-indirect reuse.
    - that exact narrow selected-side follow-up has now also been tried once
      and explicitly rejected as a kept checkpoint. I wired in a
      `machine_select` cleanup experiment that tried to reuse repeated
      same-block `load_local local.N -> alui.2(...,4)` shapes for
      non-address-taken locals, landed focused unit coverage for that rewrite,
      and rechecked the nearby floor (`make test-machine-select`,
      `make test-compiler-driver`, `autotest -riscv -s lv8`). Fresh
      `03_sort2` evidence then showed the wrong outcome for a kept step:
      the stable selected witness did not change, the rebuilt final text
      remained at `total_instructions=428`, and the residual blocker picture
      was still dominated by same-register rebuild/reload structure rather
      than by one missing safe fold. That experiment has now been fully
      backed back out, so current authority is to keep the richer diagnostics
      and the stable `428` checkpoint, but not keep that
      `scaled-local-index` reuse pass itself.
    - a rotated external-suite correctness sweep on the rolled-back stable
      tree has now also reopened one concrete non-`03_sort2` pressure line,
      and it usefully narrows where to look next. A fresh
      `sysy-testsuit-collection/lvX` rerun on the current compiler came back
      `462/467`, with the surviving red points concentrated in giant compile-
      time / giant-runtime cases rather than in semantic mismatches:
      `many_parameters10000.c` (`COMPILE_TIMEOUT`),
      `many_parameters5000.c` (`COMPILE_TIMEOUT`),
      `register_alloc10000.c` (`COMPILE_TIMEOUT`),
      `register_alloc1000.c` (`COMPILE_FAIL`), and
      `matrix-1.c` (`RUN_TIMEOUT`). Focused follow-up immediately narrowed
      that tail further: `register_alloc1000.c` was failing on
      `MEMORY-SSA-PASS-086` (`value-cleanup closure did not converge within
      256 iterations`), so the current tree now treats that cleanup closure
      the same conservative way it already treats join-binary GVN
      non-convergence: repeated/over-budget cleanup iterations stop at the
      current verifier-valid Value-SSA state instead of aborting the whole
      compile. After rebuilding the real CLI, focused reruns now reclose
      `register_alloc1000.c` and `many_parameters5000.c` to successful
      `build/compiler -riscv ... -o ...` assembly generation, and
      `matrix-1.c` also clears the direct compile-to-assembly path again.
      Current authority is therefore that the reopened external pressure is
      now narrower than the raw `462/467` sweep first implied: the next
      compile-time follow-up should focus on the still-heavy
      `10000`-scale giant tails (`many_parameters10000` and
      `register_alloc10000`) rather than on the already reclosed
      `register_alloc1000` / `many_parameters5000` pair.
    - that same `10000`-scale compile-time tail has now moved forward again
      after one more bridge-level fast-path refinement instead of only being
      reclassified as "still big". Focused timing showed that both
      `many_parameters10000.c` and `register_alloc10000.c` were spending
      their time before allocator/backend work, inside
      `value_ssa_build_default_from_lower_ir(...)`'s default
      memory-full canonicalized build. The earlier
      `extreme-straight-line` fast path existed, but it was too narrow for
      these real cases because it effectively wanted the whole program to look
      like one lone body function. The current tree now treats a broader but
      still conservative family as eligible for the same direct-build escape
      hatch: non-indirect-memory programs whose body functions are all single-
      block straight-line returns, and where at least one body function is
      giant enough (for example `parameter_count > 512`,
      `next_temp_id >= 12000`, or `instruction_count >= 12000`). Focused
      regression coverage now locks that bridge decision on a synthetic
      declaration-plus-giant-parameter lower-IR case, and the in-repo floors
      stay green (`test-value-ssa-regression`, `test-compiler-driver`).
      Real giant-case evidence also improved materially: with
      `VALUE_SSA_TRACE_TIMING=1`, both surviving `10000`-scale cases now hit
      the fast path (`extreme-straight-line=1`) and reduce their Value-SSA
      build stage from "no stage line within tens of seconds" down to about
      `5.6s` (`many_parameters10000`) and `7.1s`
      (`register_alloc10000`). Direct rebuilt-CLI reruns now also reclose the
      compile-to-assembly path on both cases:
      `build/compiler -riscv .../many_parameters10000.c -o ...` and
      `.../register_alloc10000.c -o ...` both produce large `.s` outputs
      successfully again under a `60s` compile-only budget. Current authority
      is therefore that the active `10000`-scale line is no longer blocked on
      the old Value-SSA default canonicalization bottleneck; the remaining
      reopen there is now the heavier full case pipeline budget
      (compile + assemble/link/run in suite sweeps), not the earlier
      pre-backend build failure mode.
    - the `MEMORY-SSA-PASS-086` line itself is now also better explained than
      the earlier "probably some oscillation" diagnosis, and one direct
      improvement has already landed from that deeper trace. I added an
      opt-in `MEMORY_SSA_PASS_TRACE=1` closure trace on
      `memory_ssa_pass_run_value_cleanup_closure(...)` and reran the original
      focused repro `register_alloc1000.c`. The trace showed that this case
      is not primarily bouncing across many unrelated canonicalization steps:
      the only repeatedly mutating steps in the closure are
      `fold-constants`, `simplify-trivial-values`, and
      `eliminate-dead-value-defs`, while
      `normalize-binary-operands`, `simplify-algebraic-identities`,
      `eliminate-redundant-binaries`, and `simplify-cfg` stay inert on this
      repro. More importantly, the trace shape is now clear enough to name:
      it looks like a very slow constant-propagation / dead-def cleanup chain,
      not a small two-state oscillation. Before the latest fold-side change,
      that repro was still taking roughly `232` value-cleanup iterations to
      stabilize. I then taught `value_ssa_fold_constants(...)` to resolve
      immediate values through `mov` definition chains instead of requiring
      the binary operands to already be literal immediates at the direct use
      site. Focused regression coverage now locks that behavior on a
      `mov -> binary -> binary` constant chain, and the in-repo floors remain
      green (`test-value-ssa-regression`, `test-compiler-driver`). The same
      `MEMORY_SSA_PASS_TRACE` rerun on `register_alloc1000.c` now shows the
      closure collapsing to one real mutating iteration and then immediate
      stabilization, instead of the earlier 200+ iteration crawl. Focused
      external reruns are green again on the rebuilt compiler:
      `register_alloc1000.c`, `many_parameters5000.c`, and
      `many_parameters10000.c` all pass again under
      `tools/sweep_sysy_suite.py --case-timeout 180`. Current authority is
      therefore more precise than before: the reopened `086` family was
      partly a current-tree regression in Value-SSA shaping, but it also
      relied on an older cleanup-budget weakness in `memory_ssa_pass`; the
      current tree now addresses both sides well enough that the earlier giant
      repros no longer sit on that same slow closure path.
    - one further selected-side retry on the stable `03_sort2` residue family
      has now also been measured and backed back out, so this branch should be
      treated as exhausted for the moment rather than as a still-promising
      next guess. This version was more structured than the earlier rejected
      `scaled-local-index` copy fold: it rewrote later uses onto an earlier
      surviving `alui.2(local.N,4)` result so the later
      `load_local + alui.2` pair could become dead together. Focused
      `machine_select` regressions proved that the local rewrite worked on a
      toy block and still stayed blocked for address-taken locals, but fresh
      real evidence on the stable tree still did not move the kept public hot
      witness: `03_sort2` remains `total_instructions=428`, and the repeated
      `alui.2(local:52,4)` / `alui.2(local:55,4)` hotspot counts on the
      stable selected dump remain materially unchanged. That experiment has now
      been fully removed again. Current authority is therefore stronger than
      before: the remaining `03_sort2` tail is very unlikely to close from
      another small same-block scaled-index reuse tweak at selected cleanup
      level.
    - the current giant-case profiling line now also has a first meaningful
      post-Value-SSA split instead of one blended "still slow somewhere"
      bucket. With `VALUE_SSA_TRACE_TIMING`, `MACHINE_IR_TRACE_TIMING`, and
      `MACHINE_SELECT_TRACE_TIMING` enabled on the rebuilt CLI:
      - `register_alloc10000.c` now clears Value-SSA in about `7.6s`, then
        spends another large chunk inside `machine_ir` itself
        (`lower-machine-ir` about `7.7s`, `machine_ir` total about `8.2s`)
      - `many_parameters10000.c` now clears Value-SSA in about `5.9s` and
        `machine_ir` in about `0.6s`, but then spends its biggest visible
        chunk in `machine_select` pure cleanup, specifically dead-def removal
        on one giant function (`pure fn=1 iter=1 dead ≈ 9.4s`)
      Current authority is therefore that the giant compile-time line is no
      longer dominated by one shared pre-backend bottleneck. The next compile-
      time move should likely choose between:
      - `machine_ir` lowering cost on `register_alloc10000`
      - `machine_select` dead-def / pure-cleanup cost on
        `many_parameters10000`
      rather than assuming the same optimization will help both equally.
  - final full correctness sweep after performance reopen:
    **in progress / roughly 96%**
    - the first post-optimization checkpoint reruns are now real rather than
      hypothetical: `autotest -riscv -s lv8 /workspaces/compiler_lab` is
      green again (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab`
      is green again (`22/22`) after rolling back the unsafe `div/rem by
      power-of-two` text rewrite, and a representative external correctness
      slice stays green too (`compiler2021/.../function_test2021` first
      `40/40` cases under `tools/sweep_sysy_suite.py`). Current authority is
      therefore that the current `~485` static-codegen checkpoint is not just
      a local pretty-print curiosity; it is already compatible with the
      nearby course and external correctness surface that we have rechecked so
      far
    - the next post-optimization recheck wave has now advanced that evidence
      again after the kept repeated-internal-call reuse landing. Focused reruns
      remain green on `make test-machine-select`, `make test-compiler-driver`,
      and `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`), and a
      rotated external-suite sweep is also green again on the rebuilt current
      compiler: `TrivialCompiler/custom_test` (`29/29`). One further rotated
      sweep now also reconfirms the broader third-party functional surface on
      the newer `430` checkpoint itself: `lava-test/cases` is green again
      (`162/162`) on the rebuilt current compiler. After the later
      `430 -> 427` follow-up, fresh reruns are still green on
      `make test-machine-select`, `make test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`), and one
      more rotated external suite:
      `indigo/test_codes/functional_test` (`111/111`). Current authority is
      therefore that the newer reduced post-`430` checkpoint is also staying
      compatible with the nearby course line and with multiple previously
      green third-party-functional slices, not only with the earlier `430`
      one.
      The same recheck wave has now also reclosed the broader
      `minic-test-cases-2021f/functional` surface after the temporary
      `097_register_alloc.c` reopen: focused reruns first confirmed that one
      case was the only new red point, then the indirect-memory-only gate on
      the new spill-expression reuse pass restored the full suite to
      `100/100` while keeping the stable reduced `03_sort2` checkpoint intact.
      The later matcher hardening reruns then also reconfirmed the same floor:
      `make test-machine-select`, `make test-compiler-driver`,
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`), and the
      focused `minic-test-cases-2021f/functional/097_register_alloc.c` repro
      are all green again while the rebuilt `03_sort2` text remains at
      `total_instructions=428`.
      far.
    - the latest rotated recheck wave now also records one deliberately
      different external surface instead of reusing the same smaller comfort
      suites again. On the stable rolled-back `428` tree, fresh reruns keep
      `make test-machine-select` green, keep `make test-compiler-driver`
      green, and keep `autotest -riscv -s lv9 /workspaces/compiler_lab`
      green (`22/22`). The same wave then widened to
      `sysy-testsuit-collection/lvX`, which came back `462/467` and exposed a
      giant-case-only residual tail instead of a new semantic correctness
      family. Focused reruns after the new `MEMORY-SSA-PASS-086`
      non-convergence guard now also reclose two of those five points on the
      rebuilt CLI (`register_alloc1000.c` and `many_parameters5000.c`
      compile successfully again), while the heavier `10000`-scale compile
      cases remain the active follow-up. Current authority is therefore that
      the latest correctness wave still supports the post-reopen stability
      story on course and nearby external functional surfaces, but the
      remaining not-yet-closed pressure has shifted back onto giant
      compile-time budgeting rather than onto new wrong-answer/runtime
      correctness regressions.
    - that same rotated `lvX` giant-case cluster has now advanced again after
      the latest Value-SSA giant fast path plus deeper `mov`-chain constant
      folding. The most representative focused reruns are now green on the
      rebuilt compiler under widened single-case budgets:
      `register_alloc1000.c` passes again,
      `many_parameters5000.c` passes again,
      `many_parameters10000.c` passes again,
      `register_alloc10000.c` passes again, and
      `matrix-1.c` passes again under
      `tools/sweep_sysy_suite.py --case-timeout 180`. Current authority is
      therefore that the earlier reopened `lvX` giant-case cluster is no
      longer sitting on the same compile-fail / compile-time tail shape that
      produced the intermediate `462/467` reading; the next remaining
      correctness work should rotate outward again instead of staying
      fixated on this now-reclosed subset.
    - the broader course-facing floor has also now been widened again after
      those same fixes instead of relying only on earlier `lv8/lv9` spot
      checks. Fresh reruns on the rebuilt current tree keep the full course
      `-riscv` surface green again (`autotest -riscv /workspaces/compiler_lab`)
      and keep another rotated external functional surface green again
      (`minic-test-cases-2021f/functional` `100/100`). Current authority is
      therefore that the recent Value-SSA / Memory-SSA fixes plus the later
      backed-out `machine_select` retry still sit on a broad course-facing and
      third-party functional floor, not only on the smaller sub-suite checks
      that were used earlier in the round.
    - that same post-reopen correctness sweep has now widened again after the
      latest giant-case compile-time work instead of relying only on the older
      `compiler2021` slice. Fresh reruns on the current tree keep
      `make test-compiler-driver` green, keep
      `autotest -riscv -s lv9 /workspaces/compiler_lab` green again (`22/22`),
      keep the full `indigo/test_codes/functional_test` suite green
      (`111/111`), and keep the direct
      `tools/sweep_sysy_suite.py /tmp/sysy-suites/lava-test/cases --filter 107_long_code2.sy`
      repro green after the new `value_ssa` giant-shape fast path landed.
      The public runtime-side static witness also remains intact on the same
      tree: `03_sort2.sy` still analyzes to `total_instructions=434`
      with the same hot-family shape (`lw=127`, `sw=100`, `mv=11`). Current
      authority is therefore that the latest compile-time changes are no
      longer only locally timed wins; they already sit on a broader rotated
      external + course correctness surface too.
    - that same correctness floor has now also been rechecked once more after
      the latest `hoist-2` allocator-loop follow-up instead of only after the
      earlier `107_long_code2` work. The current tree still keeps
      `make test-compiler-driver` green, keeps
      `autotest -riscv -s lv9 /workspaces/compiler_lab` green again (`22/22`),
      keeps the full `indigo/test_codes/functional_test` suite green
      (`111/111`), keeps the direct
      `tools/sweep_sysy_suite.py /tmp/sysy-suites/lava-test/cases --filter 107_long_code2.sy`
      probe green, and still keeps the public runtime-side static witness at
      the same `03_sort2 = 434` checkpoint. Current authority is therefore
      that the new allocator-loop follow-up is not just locally timed; it is
      already sitting on the same course + rotated external correctness floor
      too.
    - that same correctness floor has now widened again after the later
      `hoist-2` allocator-plan round instead of relying only on the earlier
      `lv9 + indigo + 107_long_code2` slice. The current tree still keeps
      `make test-compiler-driver` green, keeps
      `autotest -riscv -s lv8 /workspaces/compiler_lab` green again (`12/12`),
      keeps `autotest -riscv -s lv9 /workspaces/compiler_lab` green again
      (`22/22`), keeps the full `indigo/test_codes/functional_test` suite
      green (`111/111`), keeps the full
      `minic-test-cases-2021f/functional` sweep green (`100/100`), keeps the
      direct `lava-test/cases --filter 107_long_code2.sy` probe green, and
      still keeps the public runtime-side static witness at the same
      `03_sort2 = 434` checkpoint. Current authority is therefore that the
      current kept allocator/instrumentation line is sitting on a materially
      broader course + rotated external correctness floor than before.
    - that rotating external evidence surface is now a little broader again in
      the current tree: a focused `indigo/test_codes/functional_test` sample
      (`30/30`) is green too, so the recent selected-cleanup/debugging edits
      are not only holding on course `lv9` and the `minic-test-cases-2021f`
      functional tree, but also on one more third-party suite family with a
      different testcase mix.
    - that same rotated third-party evidence is now wider than the earlier
      spot-check wording implies. After the latest kept `03_sort2`
      text/export fold was narrowed back to the scratch-root-safe family, the
      current rebuilt compiler keeps the full
      `indigo/test_codes/functional_test` suite green again too (`111/111`),
      not only the earlier `30/30` slice. Current authority is therefore that
      this round's kept performance tweak has already survived both the
      `TrivialCompiler/custom_test` array-heavy surface and one larger
      independent external functional family.
    - the next kept `03_sort2` fold has now also survived another focused
      post-change recheck instead of only inheriting the earlier green rows.
      After landing the call-argument load-order fold, both
      `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22`) and the full
      `TrivialCompiler/custom_test` suite (`29/29`) are green again on the
      rebuilt compiler. Current authority is therefore that the live
      `03_sort2 = 434` checkpoint is still sitting on a revalidated course +
      external correctness floor, even though compile-time follow-up remains
      open elsewhere on the large-text side.
    - the same sweep line has now also done one more useful job in the current
      tree: it caught and then reclosed a real regression during the latest
      `03_sort2` tuning step instead of only rubber-stamping already-green
      cases. `TrivialCompiler/custom_test/array.sy` briefly reopened as a
      `MISMATCH` (`stdout=""`, `exit=169`) when the new indexed-local-root
      fold was allowed to rewrite non-scratch base registers, but the current
      kept tree narrows that fold back to scratch-root witnesses only. After
      that repair, both the focused `array.sy` rerun and the full
      `TrivialCompiler/custom_test` suite are green again (`29/29`), so this
      round should be remembered as evidence that the rotating external suites
      are actively catching real backend/text-export soundness edges rather
      than only adding redundant green rows.
  - hidden/default compatibility audit line:
    **in progress / roughly 99%**
    - 2026-05-09 restart-mode note: by explicit user request, the active
      hidden/default line has now reopened a **sequential source audit** from
      the front of the compiler pipeline rather than only testcase-driven
      chasing. Current ordered reread plan is:
      `lexer -> parser -> semantic -> ir -> ir_pass -> lower_ir -> value_ssa/value_ssa_pass -> memory_ssa/memory_ssa_pass -> machine`.
      Progress for this restart audit should be logged here chunk by chunk so a
      later restart can resume from the last reread boundary instead of
      guessing from chat history.
    - 2026-05-09 sequential source audit log:
      - restart note for the runtime-RE reframe: by explicit user request,
        the current audit target is now again **generated-program runtime
        RE**, not compiler self-crash. This pass is restarting from the
        front of the repository implementation tree and must be treated as a
        fresh whole-repo reread, not as a continuation of the earlier
        compiler-robustness interpretation.
      - fresh-restart progress note: the **current live restart boundary**
        for this runtime-RE pass is back at the front of the pipeline.
        Treat the earlier later-stage machine/backend coverage below as
        historical evidence from the previous pass, not as proof that the
        fresh restart has already re-covered those files. On the fresh
        restart, current reread coverage is:
        `src/lexer/lexer.c`, the full `src/parser/*` stage,
        the full `src/semantic/*` stage, and the front of `src/ir/*`
        (`ir.c`, `ir_core.inc`, `ir_dump.inc`), with the next boundary still
        inside the remaining IR sibling files before moving on to
        `src/ir_pass/*`.
      - fresh-restart progress update 2: the fresh runtime-RE reread has now
        pushed deeper into the IR stage and closed one concrete control-flow
        correctness bug in `src/ir/ir_lower_stmt.inc`. The local
        flow-state helper used by condition-truthiness pruning and
        loop-stays-true proofs had still been evaluating some unary/binary/
        compound-assignment expressions with host-width behavior instead of
        the compiler's 32-bit SysY integer semantics. That could let
        front-end lowering prove the wrong branch/loop truthiness and thus
        mutate generated control flow before later backend stages even saw
        the program. The live tree now normalizes those flow-state
        evaluations through 32-bit SysY semantics, including the shift-width
        boundary and wraparound-sensitive unary minus path. A focused
        compiler-driver regression now locks the representative wraparound
        loop-condition witness, and validation after the fix is:
        `make test-compiler-driver` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        plus rotated third-party sweeps
        `minic-test-cases-2021s/functional --limit 25` PASS and
        `indigo/test_codes/functional_test --limit 20` PASS.
      - fresh-restart progress update 3: the fresh runtime-RE reread has now
        also covered the remaining `src/ir/*` sibling files plus the full
        `src/ir_pass/*` stage, and it has moved into the front of the
        downstream `src/lower_ir/*` and `src/value_ssa/*` stages. No second
        concrete runtime-RE root cause is confirmed from this chunk yet: the
        currently reread `ir_pass`, `lower_ir`, and `value_ssa` front paths
        remain structurally consistent on branch/call/return lowering and on
        verifier-side contracts. The next fresh-restart boundary is to keep
        moving through the rest of `value_ssa/*`, then `value_ssa_pass/*`,
        before descending into `memory_ssa/*` and later machine/backend
        siblings.
      - fresh-restart progress update 4: one more concrete runtime-RE risk
        was confirmed and fixed in the shared front-end semantic flow helper
        inside `src/semantic/semantic_core_flow.inc` together with the
        matching `ir` helper path. The constant-fold/truthiness logic had been
        using host-width `long long` arithmetic and `LLONG_MIN` guards for
        shifts, division, modulus, and unary-minus paths, which could
        disagree with 32-bit SysY wraparound semantics and therefore make the
        frontend prove the wrong branch or loop condition before lowering.
        The live tree now normalizes those flow helper evaluations through
        32-bit SysY semantics. A focused driver regression now locks a
        wraparound loop-condition witness, and validation after the fix is:
        `make test-compiler-driver` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        plus rotated third-party sweeps
        `TrivialCompiler/custom_test --limit 20` PASS and
        `lava-test/cases --limit 20` PASS.
      - fresh-restart progress update 5: the fresh whole-repo runtime-RE
        reread has now also covered the currently relevant mid-stack sibling
        directories beyond the earlier IR/lower-IR front:
        `src/value_ssa/*`, the main `src/value_ssa_pass/*` optimization line,
        `src/memory_ssa/*`, `src/memory_ssa_pass/*`,
        `src/value_ssa_interp/*`, `src/value_ssa_alloc/*`, and
        `src/value_ssa_machine/*`. No additional concrete runtime-RE root
        cause was confirmed from this chunk: the current reread did not find a
        second wrong-code bug in those allocator/memory-SSA/protection/report
        siblings beyond the already-closed 32-bit flow-semantics drift.
        The next fresh-restart boundary is now the actual machine backend tree
        (`src/machine/lowering/*`, then `src/machine/object/*`, then
        `src/machine/runtime/*`, then `src/machine/observe/*`) under the same
        generated-program runtime-RE lens.
      - strict whole-src sub-audit coverage inside this hidden/default line is
        now roughly **68%** overall: front-half
        (`lexer -> parser -> semantic -> ir -> ir_pass -> lower_ir ->
        value_ssa/value_ssa_pass/value_ssa_interp`) is fully reread, the
        `memory_ssa` / `memory_ssa_pass` / `value_ssa_machine` side is mostly
        reread, and the downstream `src/machine/*` audit has now entered the
        object/runtime/observe sibling groups but has **not** finished their
        full `*.inc` detail files yet
      - chunk 4 restart note: by explicit user request, the active audit is
        now reset again into **strict whole-code audit mode**. Coverage target
        is no longer “mainline plus nearby suspicious files”, but **all
        implementation files under `src/`**, read in pipeline order while also
        widening to sibling/support modules at each stage. Future progress
        notes in this mode should say whether a chunk covered only the stage
        spine or the full set of sibling implementation files.
      - chunk 4 rule-tightening note: by explicit user reminder, this
        whole-src audit must not skip any pass layer just because it is not
        the most suspicious mainline. Required coverage explicitly includes
        every pass directory under `src/`, including `src/ir_pass/*`,
        `src/value_ssa_pass/*`, and `src/memory_ssa_pass/*`, alongside the
        non-pass representation/lowering/runtime files.
      - chunk 4 started: reread boundary reset to `src/lexer/lexer.c` and the
        full `src/parser/*` stage as the first whole-code-audit segment
      - chunk 4 progress update: strict whole-code audit has now restarted for
        real at the front edge and is rereading the **full parser stage**
        rather than only the parser spine; current coverage includes
        `src/lexer/lexer.c`, `src/parser/parser.c`,
        `src/parser/parser_ast_compat.inc`, and the fresh pass through the
        parser expression/statement units is in progress
      - chunk 4 progress update 2: the strict whole-code reread now also
        covers the **full semantic stage**, including
        `src/semantic/semantic.c`, `semantic_entry.inc`,
        `semantic_callable_rules.inc`, `semantic_scope_rules.inc`, and the
        full `semantic_core_flow.inc` rather than only its loop/mainline
        excerpts
      - chunk 4 findings: no new deterministic bug yet across the fully
        reread parser+semantic stages; current whole-code audit still points
        downstream of the front-end semantic gate for the hidden/default
        residual risk
      - chunk 4 progress update 3: strict whole-code audit is now inside the
        **full IR stage** and has already reread more than the IR spine:
        current coverage includes `src/ir/ir.c`, `ir_core.inc`,
        `ir_dump.inc`, `ir_lower_expr.inc`, `ir_lower_scope.inc`,
        `ir_global_init.inc`, and `ir_global_dep.inc`
      - chunk 4 findings 2: no new deterministic bug yet in the currently
        reread IR files; the fresh whole-code pass still has not surfaced a
        front-half/root-cause defect before the later lower/backend layers,
        but IR stage coverage is not complete yet because `ir_lower_stmt.inc`
        and `ir_verify.inc` still need the same full-file treatment before the
        stage can be called fully read
      - chunk 4 correction note: the strict whole-code audit order previously
        under-described the pass layer; by explicit user reminder,
        `src/ir_pass/*` is now treated as a required sibling stage between
        `src/ir/*` and `src/lower_ir/*`, not as optional background context
      - chunk 4 progress update 3b: the full `src/ir_pass/*` sibling set has
        now been reread too (`ir_pass.c`, `ir_pass_core.inc`,
        `ir_pass_temp_analysis.inc`, `ir_pass_fold.inc`,
        `ir_pass_const.inc`, `ir_pass_copy.inc`,
        `ir_pass_cfg_analysis.inc`, `ir_pass_cfg.inc`,
        `ir_pass_dce.inc`, `ir_pass_pipeline.inc`). No concrete new hidden-RE
        bug has been closed from this pass-layer reread; current risk still
        points further downstream toward allocator / machine / backend
        combinations rather than canonical IR pass-local folding/copy/CFG
        cleanup alone
      - chunk 4 progress update 4: the strict whole-code audit has now also
        completed the full `src/value_ssa/*` and `src/value_ssa_pass/*`
        sibling sets, not just their spines. Current reread coverage now
        includes `value_ssa.c`, `value_ssa_analysis.inc`,
        `value_ssa_dump.inc`, `value_ssa_from_lower_ir.inc`,
        `value_ssa_rename.inc`, `value_ssa_verify.inc`, plus the full
        `value_ssa_pass.c` aggregator and all pass siblings from
        `value_ssa_simplify.inc` through `value_ssa_pass_bridge.inc`. No new
        concrete bug has been found in this chunk yet; the current audit is
        still pushing downstream toward allocator / machine integration
        hazards, with long-running sweep state still none
      - chunk 4 progress update 5: the strict whole-code audit has now moved
        past the pure Value-SSA passes and into the downstream allocator /
        memory-SSA side. This chunk is **partial-stage coverage, not yet full
        stage closure**: current reread coverage includes the high-risk
        allocator rewrite/select/spill files
        (`src/value_ssa_alloc/value_ssa_alloc.c`,
        `value_ssa_alloc_color.inc`, `value_ssa_alloc_rewrite.inc`,
        `value_ssa_alloc_select.inc`, `value_ssa_alloc_spill.inc`,
        `value_ssa_alloc_move_engine.inc`) plus the front of
        `src/value_ssa_machine/*` and `src/memory_ssa/*`
        (`memory_ssa.c`, `memory_ssa_analysis.inc`,
        `memory_ssa_from_value_ssa.inc`, `memory_ssa_verify.inc`,
        `memory_ssa_dump.inc`). No concrete hidden-RE root cause is closed
        yet from this partial downstream reread; current open suspicion stays
        on allocator/machine/backend combination edges rather than the already
        reread front-half pipeline
      - chunk 4 dynamic evidence: rotated external rechecks during the strict
        audit are still green on the live tree, including
        `tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test --limit 3`
        (`3/3 PASS`) and
        `tools/sweep_sysy_suite.py third_party/sysy-suites/indigo/test_codes/functional_test --limit 10`
        (`10/10 PASS`)
      - chunk 4 follow-up fix: one real downstream semantic drift bug is now
        closed in `src/value_ssa_interp/*`. The interpreter had been executing
        binary arithmetic/bitwise/shift/global-init paths in host `long long`
        space instead of the project’s 32-bit SysY integer semantics, so
        oracle-style interpretation could disagree with the real compiler on
        wraparound and signed-right-shift cases. The live tree now normalizes
        immediates, args, globals, stores, call returns, and binary results
        through 32-bit SysY semantics inside `value_ssa_interp`, and the
        interpreter tests now also lock explicit overflow/shift witnesses.
      - chunk 4 validation after fix:
        `make test-value-ssa-interp` PASS,
        `make test-value-ssa-regression` PASS,
        `make test-value-ssa-oracle` PASS,
        `autotest -riscv -s lv8 /workspaces/compiler_lab` (`12/12 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test --limit 8`
        (`8/8 PASS`)
      - chunk 4 progress update 6: strict whole-code audit has now also read
        the full `src/value_ssa_interp/*` sibling set and most of
        `src/memory_ssa_pass/*` plus `src/value_ssa_machine/*`. Current
        remaining unread pressure inside the strict whole-src audit is mainly
        the rest of `memory_ssa_pass` detail paths and the large downstream
        `src/machine/*` tree.
      - chunk 4 progress update 7: downstream machine-stage whole-code audit
        is now underway for real rather than only spot-checking one suspicious
        file. Current coverage already includes the front/core/query/verify
        surfaces for `src/machine/lowering/machine_ir/*` and
        `src/machine/lowering/machine_select/*`, plus the already-reread final
        text exporter area in `src/compiler/compiler_driver.c`. No new
        deterministic hidden-RE root cause is closed from this first machine
        chunk yet; current remaining risk stays in later machine sibling
        combinations and still-unfinished downstream stage coverage.
      - chunk 4 progress update 8: the strict audit has now also covered the
        full `src/value_ssa_interp/*` sibling set, the bulk of
        `src/memory_ssa_pass/*` including `memory_ssa_pass_pipeline.inc`,
        `memory_ssa_pass_load_forward.inc`,
        `memory_ssa_pass_store_cleanup.inc`, and
        `memory_ssa_pass_memory_cse.inc`, plus the large front/mid surfaces of
        `src/value_ssa_machine/*` and the main lowering-stage siblings
        `src/machine/lowering/machine_ir/*` and
        `src/machine/lowering/machine_select/*`. No second concrete bug beyond
        the interpreter semantic-drift fix is closed from this chunk yet; the
        remaining uncertainty is still concentrated in later machine/backend
        sibling combinations and the not-yet-finished downstream stage sweep.
      - chunk 4 progress update 9: the strict reread has now also revisited
        the remaining `memory_ssa_pass` closure path plus more downstream
        machine object/runtime/observe fronts. Concrete new closure: a real
        hidden-RE risk existed in
        `src/memory_ssa_pass/memory_ssa_pass_memory_cse.inc`, where both
        repeated-state trackers (`seen_states[seen_count++]`) could write past
        their fixed `256`-entry buffers on the last iteration instead of
        failing cleanly. The live tree now guards those bounds before writing,
        so the memory-binary closure fails with an explicit diagnostic instead
        of corrupting memory when a pathological convergence case reaches the
        cap.
      - chunk 4 follow-up fix 2: the same reread also closed one adjacent
        constant-semantics drift family around that downstream area. The
        join-binary immediate fold inside `memory_ssa_pass` and the selected
        immediate fold inside `machine_select` had still been using host
        `long long` arithmetic instead of normalized 32-bit SysY integer
        semantics, which could mis-fold overflow/shift/compare combinations
        only after several lowering/cleanup stages had already composed.
        Those folds now normalize operands/results through 32-bit SysY
        semantics; `machine_ir` kept its conservative `div/mod/shift`
        no-fold policy while still normalizing the safe add/sub/mul/bitwise
        immediate folds that remain enabled there.
      - chunk 4 validation after fix 2:
        `make test-machine-ir` PASS,
        `make test-machine-select` PASS,
        `make test-memory-ssa-pass` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/minic-test-cases-2021s/functional --limit 8`
        (`8/8 PASS`)
      - chunk 4 progress update 10: the downstream whole-src reread has now
        also crossed the top-level object/runtime/observe implementation
        fronts (`machine_bytes.c`, `machine_object.c`, `machine_reloc.c`,
        `machine_container.c`, `machine_elf.c`,
        `machine_transition.c`, `machine_state.c`, `machine_mutation.c`,
        `machine_writeback.c`, `machine_commit.c`, `machine_apply.c`,
        `machine_payload_decode.c`, `machine_interp.c`,
        `machine_observe.c`, `machine_delta.c`, `machine_trace.c`,
        `machine_event.c`, `machine_outcome.c`, `machine_history.c`,
        `machine_timeline.c`, `machine_log.c`, `machine_journal.c`). No new
        second-stage deterministic crash bug is confirmed from these files
        yet; most are state/report glue and the current unread pressure is now
        narrower: the remaining downstream audit debt is mainly the sibling
        `*.inc` detail files under the machine object/runtime families plus
        any still-unfinished query/report helpers that were only partially
        sampled earlier.
      - chunk 4 follow-up fix 3: one smaller but real machine-stage query
        contract bug is now closed in
        `src/machine/runtime/machine_exec/machine_exec_core.inc`.
        `machine_exec_report_get_segment_filter_count(..., ENTRY, ...)` had
        been returning `1` even on an empty freshly initialized report, which
        made the filter-count surface inconsistent with the later filtered
        lookup that correctly returned failure. The live tree now reports `0`
        entry rows when the report has no segment summaries yet, and
        `tests/machine/runtime/machine_exec/machine_exec_test.c` now locks
        that empty-report boundary.
      - chunk 4 validation after fix 3:
        `make test-machine-exec` PASS
      - chunk 4 progress update 11: the strict whole-src audit has now also
        sampled the deeper runtime/object sibling detail files rather than
        stopping at their aggregators, including the current passes through
        `machine_exec_core/build/report/dump`, `machine_image_core/build/verify`,
        `machine_load_core/build/query/report/dump`, and the first wider
        `machine_runtime_query/build/report` surfaces. No further hidden-RE
        root cause is confirmed from those files yet beyond the new
        `machine_exec` query-count bug; the remaining unread machine debt is
        now mostly the still-unfinished object/ELF detail helpers plus any
        runtime/observe query/report siblings that have not yet been read end
        to end.
      - chunk 4 follow-up fix 4: one real zero-byte boundary cluster is now
        closed across the machine image/load/runtime copy helpers. Fresh
        rereads found that `machine_image_build_from_machine_elf_file(...)`,
        `machine_load_file_copy_flat_memory_image(...)`, and the neighboring
        runtime flat-copy path were still willing to route zero-length copies
        through potentially `NULL` byte pointers. The live tree now skips those
        `memcpy(...)` calls when the relevant byte count is `0`, instead of
        relying on implementation-defined tolerance for `memcpy(NULL, ..., 0)`.
        The new focused `machine_load` regression now hand-builds a valid load
        artifact with one normal `.text` segment plus one zero-length
        auxiliary segment, then exercises both the load-side flat-memory copy
        and the downstream `machine_runtime_build_from_machine_load_file(...)`
        plus runtime flat-memory copy path.
      - chunk 4 validation after fix 4:
        `make test-machine-load` PASS,
        `make test-machine-image` PASS,
        `make test-machine-runtime` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/minic-test-cases-2021s/functional --limit 4`
        (`4/4 PASS`)
      - chunk 4 progress update 12: strict whole-src downstream coverage is
        now roughly **74%** overall. The audit has moved further through the
        machine object/runtime detail helpers (`machine_bytes_*`,
        `machine_exec_*`, `machine_image_*`, `machine_load_*`,
        `machine_runtime_*`) and through the observe-side top-level modules,
        but the remaining whole-stage debt still includes unfinished
        `machine_elf_*` detail helpers plus some object/runtime/observe
        sibling query/report files that have not yet been read end to end.
      - chunk 4 follow-up fix 5: one more zero-byte copy boundary was
        tightened in the object/runtime side after the later machine-object
        rereads. `machine_image_build_from_machine_elf_file(...)`,
        `machine_load_file_copy_flat_memory_image(...)`, and the analogous
        runtime flat-copy helper now explicitly skip `memcpy(...)` when the
        relevant byte span is zero, instead of depending on zero-length
        copies against potentially `NULL` storage. The corresponding focused
        `machine_load` regression exercises a valid load/runtime artifact
        with one normal `.text` segment and one zero-length auxiliary
        segment to ensure the behavior stays intentional.
      - chunk 4 validation after fix 5:
        `make test-machine-image` PASS,
        `make test-machine-load` PASS,
        `make test-machine-runtime` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/minic-test-cases-2021s/functional --limit 4`
        (`4/4 PASS`)
      - chunk 4 progress update 13: strict whole-src audit coverage is now
        roughly **80%** overall. The machine object/runtime zero-length copy
        boundary is now fixed and regression-locked, while the unread debt is
        concentrated in the longer `machine_elf_*` detail helpers and the
        remaining machine object/runtime/observe query/report helpers that
        still need full end-to-end rereads before the whole-src sweep can be
        called complete.
      - chunk 4 progress update 14: the remaining machine object / ELF tail
        was reread further and one more concrete empty-container contract bug
        was found and fixed. `machine_object_file_get_section_symbols(...)`,
        `machine_object_file_get_section_fixups(...)`,
        `machine_reloc_file_get_section_relocations(...)`,
        `machine_bytes_report_get_section_symbol_summaries(...)`,
        `machine_bytes_report_get_section_fixup_summaries(...)`, and the
        corresponding `machine_container_file_copy_bytes(...)` /
        `machine_elf_file_copy_bytes(...)` helpers now treat zero-count
        subqueries and zero-byte owned artifacts as valid empty contracts
        instead of requiring a non-NULL backing array even when the count is
        zero. Focused regressions were added for the empty-section / empty-file
        boundaries across `machine_bytes`, `machine_object`, `machine_reloc`,
        `machine_container`, and `machine_elf`.
      - chunk 4 validation after fix 6:
        `make test-machine-bytes` PASS,
        `make test-machine-object` PASS,
        `make test-machine-reloc` PASS,
        `make test-machine-container` PASS,
        `make test-machine-elf` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test --limit 6`
        (`6/6 PASS`)
      - chunk 4 progress update 15: the strict whole-src reread has now also
        moved deeper through the runtime/observe side while keeping the
        machine-object tail stable. One more real runtime contract mismatch
        was closed in `src/machine/runtime/machine_runtime/machine_runtime_query.inc`
        and `src/machine/runtime/machine_runtime/machine_runtime_report.inc`:
        the region-count / gap-filter surfaces now key off the actual gap
        summary instead of blindly assuming every runtime file has a concrete
        non-overlapping gap row, so zero-gap runtime shapes continue to report
        a consistent region surface instead of drifting between count and
        lookup behavior. A focused regression in
        `tests/machine/runtime/machine_runtime/machine_runtime_test.c` now
        locks the zero-gap surface on the live tree.
      - chunk 4 validation after fix 7:
        `make test-machine-runtime` PASS,
        `make test-machine-launch` PASS,
        `make test` PASS,
        `autotest -riscv -s lv9 /workspaces/compiler_lab` (`22/22 PASS`),
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test --limit 4`
        (`4/4 PASS`)
      - chunk 4 progress update 16: one more hidden-RE crash path was closed
        on the runtime/launch boundary. `machine_launch_file_free(...)` had
        been assuming `register_count > 0` implied a real `registers` array
        and could segfault when a half-built or malformed launch file was
        torn down, so the free path now checks the backing pointer before
        walking register names. The launch query/report helpers also now
        reject malformed register-storage states explicitly instead of
        dereferencing missing arrays. A focused regression in
        `tests/machine/runtime/machine_launch/machine_launch_test.c` now locks
        that malformed-register contract, and both `make test-machine-launch`
        and the direct launch test binary are green again.
      - chunk 4 progress update 17: the observe tail was reread further and a
        matching null-refresh crash was closed as well. `machine_history`,
        `machine_timeline`, `machine_log`, and `machine_journal` already
        behaved conservatively on their main build/query paths, but the
        `report_refresh(...)` helpers for `machine_timeline`, `machine_log`,
        and `machine_journal` still dereferenced `report->file` before
        checking whether `report` itself was null. Those refresh entrypoints
        now fail fast on a null report instead of segfaulting, and focused
        regressions in the corresponding observe tests lock the null-refresh
        contract.
      - chunk 4 validation after fix 8:
        `make test-machine-timeline` PASS,
        `make test-machine-log` PASS,
        `make test-machine-journal` PASS,
        `./build/machine_timeline/machine_timeline_test` PASS,
        `./build/machine_log/machine_log_test` PASS,
        `./build/machine_journal/machine_journal_test` PASS,
        and `tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test --limit 4`
        (`4/4 PASS`)
      - chunk 4 progress update 18: the next observe-side audit slice also
        closed a null-report refresh crash family. `machine_timeline`,
        `machine_log`, and `machine_journal` all expose `report_refresh(...)`
        helpers; the `timeline` / `log` sides are now explicitly regression-
        locked on null-report rejection, and the `journal` side now also
        fails fast with a real diagnostic instead of dereferencing
        `report->file` on a null input. Focused observe regressions now lock
        the null-refresh contract across timeline/log/journal, and the direct
        observe test binaries plus the course `lv9` path remain green.
      - chunk 4 progress update 19: the generated-code audit has now also
        closed one concrete wrong-code bug in the final RISC-V text exporter
        inside `src/compiler/compiler_driver.c`. The preview tail-call
        peephole had been skipping over intervening `lw ..., off(s11)`
        restore rows between `jal ra, ...` and the strict adjacent
        `lw s11/ra; addi sp; ret` epilogue, which meant the rewritten tail
        call could silently drop callee-saved restoration state. The live
        tree now only folds the strict adjacent epilogue shape and leaves
        separated restore rows untouched. Focused validation after the fix is
        `make test-compiler-driver` PASS, `make test` PASS, `git diff --check`
        PASS, plus rotated third-party sweeps
        `third_party/sysy-suites/TrivialCompiler/custom_test --limit 18`
        PASS and `third_party/sysy-suites/lava-test/cases --limit 15` PASS,
        followed by `autotest -riscv -s lv9` PASS (`22/22`).
      - recheck rotation note: keep rotating the concrete validation sample
        set from round to round instead of reusing one tiny fixed batch every
        time. The current default should be to keep the evidence surface
        moving so nearby hidden-style families keep getting exercised rather
        than becoming one memorized witness list.
      - recheck breadth note: when using sampled validation, prefer a broader
        sample than only a couple of cases. The default should be to take a
        visibly wider slice so the recheck covers multiple nearby families
        before treating the current round as stable.
      - chunk 4 long-running command state: none; the course `lv9` run has
        completed, and the remaining third-party sweep is no longer in flight
      - next reread boundary: finish the remaining machine-stage sibling
        detail files (`src/machine/object/*/*.inc`,
        `src/machine/runtime/*/*.inc`, and any unread machine query/report
        helpers), then close the strict whole-src audit boundary for the
        machine tree
      - chunk 4 long-running command state: none; the earlier local
        `machine-ir`, `machine-select`, `memory-ssa-pass`, `lv9`, and
        third-party functional sweeps have all completed and been harvested
      - next reread boundary: finish the remaining machine-stage sibling
        detail files (`src/machine/object/*/*.inc`,
        `src/machine/runtime/*/*.inc`, and any unread machine query/report
        helpers), then close the strict whole-src audit boundary for the
        machine tree
      - chunk 3 restart note: by explicit follow-up user request, the active
        audit is now being restarted **again** from `tokenize` and should not
        inherit the earlier downstream suspicion as its working conclusion;
        dead-loop mislowering is now tracked as a co-equal hidden-RE risk
        alongside plain crash/corruption
      - chunk 3 started: reread boundary reset to `src/lexer/lexer.c` and a
        fresh full-order pass through `src/parser/*`
      - chunk 1 started: reread restart boundary reset to `src/lexer/lexer.c`
        and `src/parser/*`
      - long-running command state: none
      - chunk 1 progress update: reread completed on
        `src/lexer/lexer.c`, `src/parser/parser.c`, and the current
        expression-core branch in `src/parser/parser_core_expr.inc`
      - chunk 1 findings: no new deterministic bug yet; current restart audit
        suspicion still stays downstream of tokenization/basic expression
        parsing rather than at the lexer / parser-core front edge
      - next reread boundary: `src/parser/parser_stmt_decl_tu.inc`
      - chunk 2 progress update: reread completed on
        `src/parser/parser_stmt_decl_tu.inc`, `src/semantic/*`, plus the
        current canonical-IR front edge in `src/ir/ir.c`,
        `src/ir/ir_lower_stmt.inc`, `src/ir/ir_global_init.inc`,
        `src/ir/ir_global_dep.inc`, and `src/ir/ir_verify.inc`
      - chunk 2 dynamic evidence:
        `hidden_probe21`, `hidden_probe22`, and `hidden_probe24` were rerun
        and their red cases are still the intentional over-initializer
        negatives (`IR-LOWER-035` on legal-size mismatch probes), not a new
        live compiler bug; `hidden_probe25b` (`80/80 PASS`) and
        `hidden_probe5` (`45/45 PASS`) remain green on the rebuilt tree
      - chunk 2 findings: no new deterministic hidden-RE root cause found in
        parser / semantic / canonical-IR front-half reread; current suspicion
        narrows further toward the default indirect-memory pipeline after
        lower-IR, especially the direct `value_ssa` fast-cleanup lane and/or
        later backend preview-text peepholes rather than front-end AST
        assembly or early canonical-IR declaration/control-flow shaping
      - next reread boundary: `src/lower_ir/*` then the default
        `value_ssa` / `value_ssa_pass` indirect-memory fast path
      - long-running command state: none
      - chunk 3 progress update: fresh reread completed again on
        `src/lexer/lexer.c`, `src/parser/parser.c`,
        `src/parser/parser_core_expr.inc`, and the full
        `src/parser/parser_stmt_decl_tu.inc`
      - chunk 3 findings: no new deterministic bug yet at the tokenizer /
        parser front edge; parser-side constant-true loop bookkeeping is still
        clearly local-only and does not itself own semantic/control-flow
        authority, so the current fresh pass still points downstream for the
        dead-loop / hidden-RE risk
      - chunk 3 progress update 2: fresh reread also covered the semantic
        front gate and core flow helpers in `src/semantic/semantic.c`,
        `semantic_entry.inc`, `semantic_callable_rules.inc`,
        `semantic_scope_rules.inc`, and the loop/guard-state parts of
        `semantic_core_flow.inc`
      - chunk 3 findings 2: no new deterministic bug yet in semantic return /
        loop analysis; the current tree still keeps the important boundary
        explicit that `may_non_terminating` is only a loop-summary heuristic
        and not itself final proof for rejecting all-path returns, so the
        fresh dead-loop/RE suspicion remains downstream of semantic
      - chunk 3 progress update 3: fresh reread also covered the current
        canonical-IR / lower-IR loop-shaping front line in `src/ir/ir.c`,
        `ir_lower_stmt.inc`, `ir_global_init.inc`, `ir_global_dep.inc`,
        `ir_verify.inc`, plus the lower-IR front edge in
        `src/lower_ir/lower_ir.c`, `lower_from_ir.inc`, and
        `lower_ir_verify.inc`
      - chunk 3 findings 3: no new deterministic bug yet in the current
        `IR -> lower_ir` front line; the `while/for` exit-block choice,
        loop-target bookkeeping, and lower-IR value/slot split still look
        internally consistent on reread, so the fresh dead-loop/RE suspicion
        keeps narrowing toward the later default `value_ssa` indirect-memory
        fast-cleanup path and/or still-later backend preview-text peepholes
      - chunk 3 progress update 4: fresh reread also covered the current
        `src/value_ssa/*` conversion/verifier front edge, the direct-fast-path
        entry in `src/value_ssa_pass/value_ssa_pass_bridge.inc`, the current
        `src/memory_ssa*` / `src/memory_ssa_pass*` load-forward front edge,
        plus downstream `src/machine/lowering/machine_ir/*`,
        `src/machine/lowering/machine_select/*`, and the final preview-text
        peephole section of `src/compiler/compiler_driver.c`
      - chunk 3 dynamic evidence 2: focused downstream array/control-flow
        witnesses remain green on the rebuilt tree:
        `hidden_probe27` (`60/60 PASS`) and `hidden_probe28` (`12/12 PASS`);
        the focused compiler-driver regression binary is also still green
        (`make test-compiler-driver` PASS)
      - chunk 3 findings 4: no new deterministic bug yet in the reread of
        `value_ssa` / `memory_ssa` / `machine` mainline; current suspicion is
        now narrower and more downstream-looking than before, with the fresh
        pass not reproducing a new front-half error and the widened
        array/control-flow probes still pushing uncertainty back toward hidden-
        only residuals in the default indirect-memory fast path or later final
        backend text/output details
      - chunk 3 progress update 5: audit strategy widened again by explicit
        user request; the current reread is no longer only the obvious
        mainline modules. Fresh broad-sweep reads now also cover adjacent
        support layers such as `src/ast/ast.c`, `src/ir_pass/*`,
        `src/value_ssa_alloc/*` front entry, `src/value_ssa_machine/*` front
        entry, and downstream object/runtime/observe front entries including
        `machine_bytes`, `machine_object`, `machine_reloc`, `machine_elf`,
        `machine_runtime`, and `machine_observe`
      - chunk 3 findings 5: no new deterministic bug surfaced yet from that
        broader adjacency sweep either; current widened-array/control-flow
        downstream probes remain green (`hidden_probe27` `60/60 PASS`,
        `hidden_probe28` `12/12 PASS`), so the remaining hidden/default risk
        still looks more like a combination-only downstream contract hole than
        a single obvious standalone front-half bug
      - next reread boundary: continue fresh pass deeper through remaining
        `src/value_ssa_pass/*`, `src/machine/*`, and keep searching for a
        concrete hidden/default repro that survives the current green probe
        surface
    - the earlier local nested-call corruption repro was closed at the real
      downstream boundary in `machine_ir`, but hidden-course status must stay
      **reopened** because the platform symptom for `22_nested_calls` is still
      `AE` (assembler error), not the earlier local wrong-answer/runtime
      corruption shape
    - default compiler mode now intentionally bypasses all-paths-return
      rejection, which closes one large false-CTE source for submission mode
    - nearby public analogues around `nested_calls`, `short_circuit`, and
      `long_array` now compile / assemble / run locally again after the recent
      backend and canonicalization fixes, so the remaining risk surface is
      increasingly hidden-only rather than broadly reproducible in public
      suites
    - the previously reopened public `091_long_func.c` branch-range blocker is
      no longer a standing local red point in the current tree: after the
      latest preview-branch fallback revisit plus the void-builtin cleanup,
      focused `compile -> assemble -> link -> run` revalidation came back
      green again on `091_long_func.c`, `098_nested_calls.c`, and the
      surrounding `090..099` public `lvX` tail
    - current hidden/default target list is now explicit rather than vague:
      `17_while_if2` (`CTE`), `22_nested_calls` (`AE`), plus the known
      segmentation-fault `RE` cluster:
      `05_global_arr_init`, `06_long_array`, `08_arr_access`,
      `09_const_arr_read`, `14_short_circuit1`, `17_func_expr2`,
      `18_global_var2`, `19_hanoi2`, `32_global_1darr3`,
      `33_global_2darr3`, `34_zero_init3`
    - the nearest public analogues for those hidden families have now been
      rechecked again after the latest lowering repair instead of being left
      as stale assumptions. Current focused sweeps are green on:
      `function_test2021/{033,039,040,041}_while_if*`,
      `h_functional/{104,105}_long_array*`,
      `h_functional/{115,116}_nested_calls*`,
      `minic-test-cases-2021f/{080_max_flow,092_long_array,093_long_array2,098_nested_calls}`,
      which materially narrows the remaining compatibility uncertainty back
      toward hidden-only cases rather than a reproduced public family.
    - that public-neighbor closure has now been widened once more around the
      earlier hidden `RE` cluster instead of stopping at `long_array` /
      `nested_calls` / `max_flow`. Current focused sweeps are also green on:
      `function_test2021/{058,059}_short_circuit*`,
      `minic-test-cases-2021f/{043,044,090}_short_circuit*`,
      `sysy-testsuit-collection/lvX/{043,044,048,058,059,090,102}_short_circuit*`,
      course `lv9/{05_global_arr_init,08_arr_access,09_const_arr_read,10_arr_in_loop}`,
      and `sysy-testsuit-collection/lvX/{040_global_var,097_many_global_var}`.
      Current authority is therefore that the remaining hidden/default risk is
      no longer well represented by the closest publicly available families:
      those public analogues are green again in the rebuilt tree, so the
      residual uncertainty is increasingly hidden-only rather than “one more
      nearby public testcase we have not rechecked yet”.
    - 2026-05-08 follow-up: one more hidden-like backend/runtime corruption
      bug has now been closed and regression-locked even though the exact OJ
      cases are still unavailable locally. `machine_ir_copy_cleanup` had been
      dropping indirect-memory producer chains because its backward
      needed/live scan failed to mark `load_indirect` address operands and
      `store_indirect` address/value operands as uses. That let
      `addr_local/addr_global + index` chains disappear during
      canonicalization, which directly explained the previously reproduced
      `lava-test/lava_test/kmp.sy` segfault/mismatch. The fix is now kept and
      locked by new `machine_ir` regressions for both indirect-load and
      indirect-store use preservation, and focused rechecks stay green on
      `make test-machine-ir`, `make test-compiler-driver`, `lv9` (`22/22`),
      and `lava-test/lava_test/kmp.sy`. Current authority is therefore that
      the hidden RE cluster has at least one real shared indirect-memory
      corruption vector removed, even though the remaining OJ-only cases still
      need a direct repro or a stronger synthetic analogue.
    - 2026-05-08 second follow-up: the same `machine_ir_copy_cleanup` line had
      one more narrower-but-real cross-block hole that is now also closed and
      regression-locked. The block-liveness/use collection used by dead-def
      cleanup was still ignoring successor-side `load_indirect` address uses
      plus `store_indirect` address/value uses, which meant a predecessor-only
      producer could still look dead when its only remaining consumer sat in a
      later block. That reopened exactly the same class of hidden-like
      corruption risk at CFG boundaries even after the earlier same-block
      backward-scan repair. The current tree now marks those indirect operands
      in block use-sets too, with dedicated `machine_ir` regressions for both
      cross-block indirect-load and cross-block indirect-store preservation.
      Focused rechecks are green again on `make test-machine-ir`,
      `make test-compiler-driver`, `autotest -riscv -s lv9` (`22/22`), plus
      the local synthetic hidden-neighbor CFG/array suite used to stress
      short-circuit / func-expr / BFS-like indirect-memory edges. Current
      authority is therefore that another plausible shared hidden RE vector has
      been removed even though the exact OJ cases are still not reproduced
      verbatim locally.
    - 2026-05-08 RTLE follow-up: the hidden/default line now has one concrete
      dead-loop closure too, not only more RE-side cleanup. A focused
      differential sweep over `91` synthetic loop/parameter/gcd cases found a
      real timeout cluster on `for (; b < N; b = step(b))` with a mutable
      local condition variable and a call-based step expression. The root
      cause was canonical IR lowering, not later optimization: the
      flow-state proof path reused stale scope-level constant metadata for a
      tracked local after that local had already become “unknown”, so the
      `for` header could incorrectly conclude the condition stayed true and
      drop the exit branch entirely. The fix is now landed and regression-
      locked in `tests/ir/ir_regression_test.c`, and the same `91`-case local
      sweep is green end-to-end after rebuilding the CLI compiler. Current
      authority is therefore that the RTLE cluster is no longer a pure blind
      hunt: at least one real loop-proof bug is now closed, while the
      remaining hidden RTLE risk should focus on other loop/control-flow
      shapes rather than this exact mutable-local + call-step family.
    - 2026-05-08 next RTLE follow-up: the same canonical-IR loop-proof line
      had another real dead-loop hole on ordinary local updates, not only the
      earlier call-based step family. The flow-state evaluator was still not
      modeling `++`, `--`, or compound assignments such as `+=` / `-=` for
      tracked scalar locals, so loops like `while (b < 3) { b++; }` and
      `for (; b < 3; b += 1) {}` could still be misproved as “condition stays
      true forever” and lose their exit edge. That family is now fixed in
      `ir_lower_stmt` by teaching flow-state evaluation to update tracked
      identifier values across prefix/postfix increment/decrement and compound
      assignment forms, with new IR regressions plus a focused `5/5`
      compile/link/run probe suite all green after rebuilding `build/compiler`.
      Current authority is therefore that the hidden RTLE hunt has now closed
      two distinct loop-proof subfamilies inside canonical IR instead of only
      the original call-step witness.
    - 2026-05-09 loop/control-flow reread follow-up: the same
      `ir_lower_stmt` proof layer had one more real control-flow soundness
      hole on assignment-valued conditions. `ir_lower_flow_state_eval_expr(...)`
      was clearing the tracked local to "unknown" for shapes like
      `b = foo()` when the RHS was non-constant, but it still returned
      success without producing a known truthiness value; on top of that, the
      main condition fast path (`ir_try_eval_condition_truthiness_with_scope`)
      was still skipping scope-aware flow evaluation entirely and consulting
      only the older expression-only truthiness helper. In practice this let
      `while (1) { if (b = foo()) break; } return 3;` collapse into an
      unconditional dead loop, because the `if` condition was misclassified
      as known-false during lowering/proof instead of being treated as
      unknown runtime control. The live tree now treats assignment conditions
      with non-constant RHS as unknown in the flow-state evaluator and
      evaluates condition truthiness through the current scope-aware
      flow-state path before falling back to the older pure-expression helper.
      A user-facing compiler-driver regression now locks the minimal repro,
      and focused rechecks are green on `make test-compiler-driver`,
      `make test-ir-regression`, and the current repository-wide `make test`
      sweep. Current authority is therefore that another concrete hidden-like
      loop/control-flow RTLE source is now closed in canonical IR lowering,
      specifically for assignment-in-condition shapes that guard `break` /
      `continue` exits.
    - 2026-05-09 public-course control-flow follow-up: the next broad
      course-facing rerun finally reproduced a public cluster instead of only
      hidden-course suspicions. A live `autotest -riscv` pass exposed
      `lv7/04_while_if` (WA), `lv7/06_nested_while` (TLE),
      `lv7/08_if_break` (WA), `lv7/09_continue` (TLE), and
      `lv7/10_if_continue` (WA). Reading those witnesses together showed two
      more over-aggressive shortcuts in `ir_lower_stmt`: loop-body condition
      pruning was still willing to reuse scope-local flow facts while already
      inside a loop body, and the `body_forces_non_exit` fast path still
      treated nested-loop / continue-heavy bodies as if they could not reach a
      real exit edge. The live tree now keeps both boundaries stricter: the
      scope-aware condition truthiness fast path is disabled while lowering
      inside an active loop body, and `body_forces_non_exit` now refuses to
      fire for bodies that may reach `continue` or that already contain nested
      loops. Focused public rechecks are now green on the exact reproduced
      cluster (`autotest -riscv -s lv7` => `12/12`), in addition to the
      earlier `make test-compiler-driver`, `make test-ir-regression`, and
      repository-wide `make test` evidence. Current authority is therefore
      that the public `lv7` while/break/continue family is materially
      reclosed, and future control-flow hunting should move past that public
      cluster instead of treating it as an open red point.
    - 2026-05-09 perf follow-up: the next wider course rerun immediately
      surfaced a second concrete public cluster after the `lv7` closure, and
      this one was not another loop-body CFG issue but a runtime global-value
      truthiness mistake. The live `autotest -riscv` pass stayed green through
      `lv7`, `lv8`, and `lv9`, but reopened `perf/00_bitset1`,
      `perf/01_bitset2`, and `perf/02_bitset3` as uniform `WRONG ANSWER`
      cases with observed `-11` output. A smaller direct repro then showed the
      same root cause even more clearly:
      `int g=0; g=x; if (g < 0) ...` was being folded as if `g` still kept its
      compile-time initializer value. The bug lived in
      `ir_lower_flow_state_eval_expr(...)`: when an identifier was not a
      tracked local binding, it still fell back to the generic constant-
      expression evaluator, which in turn treated scalar globals with constant
      initializers as if they were immutable runtime constants. The live tree
      now restricts that flow-state evaluator to current local-scope facts
      only, instead of consulting global initializer metadata for runtime
      truthiness folding. Focused rechecks are green on the exact public perf
      reopening (`autotest -riscv -s perf` now re-closes
      `00_bitset1..02_bitset3`), plus `make test-compiler-driver`,
      `make test-ir-regression`, and the already-reclosed `lv7/lv8/lv9`
      course subsets. Current authority is therefore that the new bitset
      regression cluster is materially closed, and the remaining perf line can
      return to performance pressure rather than correctness drift.
    - 2026-05-08 front-end audit follow-up: the ordered reread of
      lexer/parser/semantic code also found one real semantic soundness hole
      even though it does not currently look like the main hidden RE/RTLE
      driver. `semantic_scope_rules` had been rejecting writes to plain
      `const` scalars but still allowed writes through const-array elements
      and const-array `++/--` forms (`a[0] = 3`, `a[0]++`). That is now fixed
      and regression-locked in the semantic suite. Current authority is that
      this front-end repair is worth keeping as general correctness hardening,
      while the main hidden compatibility pressure still remains deeper in the
      IR/backend runtime path because OJ-visible RE/RTLE symptoms did not
      materially shift on this semantic issue alone.
    - 2026-05-08 global-init audit follow-up: the ordered IR reread then found
      one real array/global dependency hole inside `ir_global_dep`. Runtime
      global-initializer dependency collection and dependency-path tracing were
      both skipping `AST_EXPR_SUBSCRIPT`, so a top-level initializer like
      `int a = b[0];` could silently miss its dependency on global `b` and
      slip past the existing `IR-LOWER-022` guard even though the scalar
      `int a = b;` form was already rejected. The current tree now traverses
      both subscript base and index expressions in the dependency collector and
      trace walker, with a new IR regression locking the direct
      global-array-subscript case and a rebuilt CLI repro now failing as
      expected with `dependency path: a -> b`. Current authority is therefore
      that the hidden RE/global-family audit has one more concrete closure in
      the canonical global-initializer layer rather than only more broad
      synthetic green sweeps.
    - 2026-05-08 compiler-driver audit follow-up: the ordered reread then
      found one logic bug in the final RISC-V text-export fixup lookup path.
      `compiler_find_fixup_at_patch_offset_and_kind_cached(...)` used to fall
      back to returning “the first fixup at that patch offset” even when the
      requested fixup kind did not match, which meant the pretty-printer could
      theoretically misclassify an instruction as a data-address / data-load /
      data-store symbolic form just because some *other* fixup shared the same
      patch offset. The current tree now requires an exact kind match and
      returns `NULL` otherwise. Focused rechecks stay green on
      `make test-compiler-driver` and `autotest -riscv -s lv9` (`22/22`).
      Current authority is that this is a correctness hardening fix in the
      final text-export boundary; it has not yet produced a standalone hidden
      repro by itself, but it removes one dangerous wrong-fixup fallback from
      the executable assembly printer.
    - 2026-05-09 compiler-driver audit follow-up: the continued ordered reread
      found another real text-export correctness hole in
      `compiler_emit_global_sections(...)`. The `.sdata` path had been
      emitting exactly one `.word` for every initialized global object even
      when that object's recorded `byte_size` was larger than `4`, which left
      the pretty-printed assembly structurally inconsistent with the object's
      advertised size and with the already-more-honest downstream
      bytes/object/ELF artifact layers. The live tree now keeps the first
      initialized word and emits a trailing `.zero (byte_size - 4)` padding
      block for larger initialized objects instead of silently truncating the
      textual data definition. Focused rechecks are green on
      `make test-compiler-driver` and the current repository-wide `make test`
      sweep. Current authority is that this is another kept compiler-driver
      hardening fix; source-level SysY array initializers still usually travel
      through `.sbss + __global.init`, so this closure is recorded primarily
      as an executable-assembly/export honesty repair rather than as the main
      hidden-course RE root cause.
    - 2026-05-08 lower-ir audit follow-up: the ordered reread of the
      post-IR boundary also found one verifier hole in `lower_ir` itself. The
      temp-availability check (`LOWER-IR-VERIFY-059`) had been validating
      `mov`, `binary`, `call`, and direct `store_*` uses, but it still skipped
      indirect-memory operands entirely, so `load_indirect addr`,
      `store_indirect addr`, and `store_indirect value` could evade the
      all-incoming-path definition check. The current tree now validates those
      indirect temp uses too, with new verifier regressions covering join-path
      partial-definition shapes for both indirect load and indirect store.
      Current authority is that this is another real downstream-hardening fix
      in the `lower_ir -> value_ssa` boundary family, aligned with the earlier
      machine-ir indirect-use closures.
    - 2026-05-09 array-initializer semantics follow-up: the hidden/default
      reread has now also reclosed one real brace-initializer soundness hole
      in `ir_lower_stmt` instead of only adding more synthetic green rows. The
      earlier rank-1 (`1D`) nested-brace repair turned out to be too
      aggressive: it treated `{{1,2},3}` as if the nested `{1,2}` should fill
      several later array slots, but the current C/SysY-like scalar behavior
      is that a nested init-list at `rank == 1` still initializes exactly one
      scalar element from its first scalar payload and then consumes exactly
      one outer initializer item. The live tree now peels only that first
      scalar payload for `rank == 1`, which closes both a direct wrong-answer
      repro (`int a[8]={{1,2},3,{4,5},6};` had been returning `91` instead of
      host-oracle `43`) and a mixed-brace compile-fail repro
      (`int a[2][3]={{1},2,3,{4,5}};` had been failing at `IR-LOWER-033`).
      The IR and lower-IR regression suites now lock both the corrected `1D`
      local-array lowering shape and acceptance of the mixed `2D` brace form,
      and focused rechecks are green on `make test-ir-regression`,
      `make test-lower-ir-regression`, `/tmp/hidden_probe3` (`8/8 PASS`),
      `minic-test-cases-2021f/functional` (`100/100 PASS`),
      `minic-test-cases-2021s/functional` (`112/112 PASS`), plus targeted
      public neighbors `076_hanoi.c`, `092_long_array.c`, `093_long_array2.c`,
      `097_many_global_var.c`, and `098_nested_calls.c`.
    - 2026-05-09 func-expr/global follow-up: the next synthetic differential
      round finally reproduced a hidden-like non-array bug cluster in the live
      default pipeline instead of only more green neighbors. A `45`-case
      source-level sweep over defined-semantics `short_circuit /
      func_expr-ish / global-shadow / recursion` shapes came back
      `18/45 PASS` before the fix, with a `27`-case uniform red cluster on the
      `func_expr-ish` slice. The minimized witness was
      `int s=2; ... int v = pick(u, h()); return s + u*10 + v*100;`, where the
      generated program always returned `expected-3`: `call h()` still ran, but
      the later `return s + ...` reused the pre-call global `s`. The bug was
      not in final text emission or plain lower-ir/value-ssa conversion; it
      came from `memory_ssa_pass_forward_global_loads(...)`, whose join-load
      PRE logic was willing to push a `load_global` into predecessors even when
      the load's expected memory version had already been defined by an earlier
      instruction in the same join block (here: a global-writing `call h()`).
      The live tree now blocks that PRE/phi-forward path for same-block
      instruction-defined memory versions, and focused rechecks are green on
      `make test-memory-ssa-pass`, `make test-compiler-driver`, and the
      rebuilt `/tmp/hidden_probe5` differential suite (`45/45 PASS`). Current
      authority is therefore that the hidden `func_expr2 / global_var2` risk
      line has one more real shared root cause removed, specifically the
      “same-block global-writing call followed by later global load in a join
      block” family.
    - 2026-05-09 backend-text call-barrier follow-up: one neighboring
      executable-assembly safety hole is now also closed in the final RISC-V
      text peepholes. The repeated indexed-address reuse folds in
      `compiler_driver` had been treating plain `call foo` rows as if they
      were invisible, because the text-side “defines/uses reg” helpers only
      understood ordinary explicit operands and therefore missed the caller-
      clobber effect on `a*`, `t*`, and `ra`. In direct text-level witnesses,
      both the repeated indexed triple and repeated indexed sequence rewrites
      were willing to delete the second address formation across an
      intervening `call foo`, which is not sound. The live tree now treats
      `call`/`jal ra`/`jalr ra`/`ecall` as caller-clobber barriers for those
      registers, and `tests/compiler/compiler_driver_test.c` now locks both
      text-level regressions. Focused recheck: `make test-compiler-driver`
      PASS.
    - 2026-05-10 array/parameter widening follow-up: the next widening round
      on the array-heavy public surface is still green on correctness. New
      synthetic suites covering global-array helper/control-flow mixes,
      global-array slice decay, array-parameter aliasing, dimension-`1`
      extreme shapes, same-declaration multi-global mixes, and larger random
      array/alias/control-flow combinations all stayed green
      (`hidden_probe16`: `60/60`, `hidden_probe17`: `12/12`,
      `hidden_probe18`: `14/14`, `hidden_probe19b`: `12/12`,
      `hidden_probe20`: `120/120`, `hidden_probe21b`: `9/9`,
      `hidden_probe22b`: `9/9`). Public external witnesses also widened again
      without reopening correctness: `024_array_traverse3.c`,
      `028_func_param_arr.c`, `074_full_conn.c`, `079_kmp.c`, manual
      `test_array.sy`, and manual `array.sy` checks stayed green, and a wider
      `lvX` parameter sweep only reopened `many_parameters10000.c` as
      `COMPILE_TIMEOUT`. Current authority is therefore that this round
      pushed the hidden array/global compatibility line further toward
      hidden-only residuals again, while the one newly surfaced public issue
      belongs to the already-open compile-time/perf line rather than to a new
      correctness family.
  - strict `returns-all-paths` audit line:
    **in progress / roughly 90%**
    - several real false-positive loop families are now fixed and locked
      with semantic regressions
    - current understanding is now explicit: this audit is not a one-layer
      semantic-only gate. The same user-visible strict/default behavior is
      shaped by at least three linked layers:
      `semantic_compute_function_returns_all_paths(...)` for the front gate,
      the reachability/fallthrough-sensitive runtime-global-dependency walk
      in canonical IR (`ir_global_dep`) for initializer-side control-flow
      pruning, and the local-state-aware loop/if fallthrough shaping in
      canonical IR lowering (`ir_lower_stmt`) that decides whether legal
      strict programs keep a real exit path or collapse into malformed/dead
      CFG on the way to later stages
    - the residual work is now to widen legal-family search and keep those
      three layers aligned, not to reopen the already-closed simple
      guard-refinement cases
    - a broader public strict-mode sweep over `compiler2021` and
      `sysy-testsuit-collection` near-neighbor files (`while_if`, `if_test`,
      `short_circuit`, `break`, `continue`) is now also clean: `41/41`
      representative cases compile successfully under
      `--enforce-all-paths-return-check`
    - the previously reopened alias-fed nested-loop residual is now also
      closed at the real canonical-IR lowering boundary rather than only in
      semantic notes: `ir_lower_stmt` now keeps loop-exit blocks only when
      there is a real path from a constant-true loop body back out to the
      surrounding function, so the strict alias family
      (`while(1){ if(a){return 1;} int b=a; b=1; for(;b<2;b=b+1){ if(b){return 2;} } }`)
      no longer dies with `IR-LOWER-009`. That family is now locked in both
      IR and compiler-driver regressions, and the rebuilt strict CLI again
      lowers it all the way to RISC-V text.
  - external-suite sweep line:
    **in progress / roughly 95-96%**
    - `minic-test-cases-2021s` and `minic-test-cases-2021f` are now locally
      available and have already produced real differential results
    - the first real external-suite closure (`088_int_literal`) is now fixed
      and regression-locked in-repo
    - the earlier public `081_n_queens` / `084_side_effect` suspicions are no
      longer treated as live compiler bugs after rerunning them with the
      correct `.in` / `.out` oracle handling
    - current oracle policy is now explicit rather than partly implicit:
      third-party `.out` files may encode either plain stdout, `stdout+exit`,
      or `stdout + "\\n" + exit`, so broad sweeps should normalize against all
      three shapes before treating a mismatch as a compiler bug
    - with that normalized oracle handling, the current `sysy-testsuit-
      collection/lvX` `090..099` tail is green end-to-end locally, including
      the earlier pressure points `091_long_func.c`, `092_long_array.c`, and
      `098_nested_calls.c`
    - the full local `minic-test-cases-2021f/functional` sweep is now also
      green again under the same normalized oracle handling (`100` functional
      cases, `0` failures), which materially lowers the odds that the current
      compatibility tail is still hiding a broad public functional regression
    - the same is now true for `minic-test-cases-2021s/functional`
      (`100` functional cases, `0` failures), so both close-to-course public
      functional suite trees are currently green under the rebuilt CLI
    - `compiler2021/公开用例与运行时库/function_test2021` is now also green under the
      normalized oracle rule (`103` public functional cases, `0` real failures)
    - `indigo/test_codes/functional_test` is now also green under the same
      rule (`111` functional cases, `0` real failures)
    - `TrivialCompiler/custom_test` and `lava-test/cases` have now both been
      swept far enough to matter for hidden-neighbor hunting; the remaining
      mismatches there are currently formatting-only oracle noise (for example
      suites that encode trailing `0\n` as output text) rather than reproduced
      semantic/codegen failures
    - `compiler2021/performance_test2021-public` is now also narrowed to a
      very small real tail instead of an open-ended suspicion: current local
      reruns now leave exactly one reproduced red point, `03_sort2.sy`
      (`RUN_TIMEOUT`), while `median2.sy` has been reclosed and is now green
      again; that makes `03_sort2.sy` the best remaining public high-pressure
      follow-up target once the broad sweep itself is considered sufficiently
      closed
    - `03_sort2.sy` now also has a tighter runtime diagnosis: the full input
      times out, but a much smaller prefix input still completes quickly, so
      the remaining tail is more likely a genuine algorithmic/performance
      hotspot than a compiler-regression bug
    - a later suite-batch sweep also found a new public third-party perf
      cluster in `lava-performance`: `dead-code-elimination-1/2/3.sy`,
      `floyd-2.sy`, and `hoist-2.sy`. The first trio has now been narrowed
      twice: first from generic compile-time stalls to a semantic-scope
      hotspot, and then from that hotspot to a concrete downstream far-call
      limitation. After the current bytes fix, the trio now reaches runtime
      rather than compile failure. `floyd-2.sy` in particular is now observed
      as output-format noise rather than a semantic mismatch, so the remaining
      real external pressure there is now mostly `03_sort2.sy` / `hoist-2.sy`
      style runtime/performance budgeting rather than compiler correctness
      bugs. Direct reruns now also keep `hoist-2.sy` in that same bucket: it
      remains a pure runtime-timeout case and its source is dominated by
      repeated loop-invariant arithmetic, so current authority is to treat it
      as an optimization/perf follow-up rather than as a correctness defect.
      More specifically, fresh stage timing now shows `hoist-2.sy` spends
      almost all compiler-side time inside `machine_ir_report`, while
      `03_sort2.sy` compiles quickly end-to-end and therefore looks like a
      program-runtime/performance issue rather than a compiler-stage hotspot
    - the full local `sysy-testsuit-collection` sweep has now also materially
      narrowed from `15` real red points to no confirmed semantic/compiler
      red points in the currently investigated pressure cluster: the earlier
      `many_parameters*`, `register_alloc10000`, and `107_long_code2`
      failures are now reclosed after the latest large-spill scratch fix, and
      the only residual sweep noise is the `brainfk` pair whose bodies match
      exactly and differ only by suite-formatting (`\r\n` vs `\n0`)
    - current public-tail pressure is therefore now more about the remaining
      hidden-only `CTE/AE/RE` families than about reproduced public stress
      tails; the previously red large-parameter / large-register public
      cluster is currently reclosed locally after the latest backend fix
  - `lv8` course line: **complete / 12 of 12 passing**
    under
    `CDE_LIBRARY_PATH=/opt/lib CDE_INCLUDE_PATH=/opt/include /opt/bin/autotest -riscv -t /opt/bin/testcases -s lv8 /workspaces/compiler_lab`
  - `lv9` course line: **complete / 22 of 22 passing**
    under
    `CDE_LIBRARY_PATH=/opt/lib CDE_INCLUDE_PATH=/opt/include /opt/bin/autotest -riscv -t /opt/bin/testcases -s lv9 /workspaces/compiler_lab`
  - `lv9` engineering checkpoint:
    array admission, indirect array lowering, preview bytes/object lowering,
    and the remaining sort-family backend/runtime closure are now all green;
    the active mainline can move back to the reopened downstream
    RISC-V artifact-honesty line (`machine_reloc -> machine_elf`) when the
    next round starts
  - `perf` course line: **course-correctness complete / effectively 100%**
    - `perf/00_bitset1` was reopened by one real backend text-export bug:
      global-address materialization for `addr_global` had been printed as
      `lui + mv` instead of `lui + addi %lo(symbol)`, which truncated global
      array addresses to page bases in the final textual RISC-V surface
    - that text-export bug is now fixed in `compiler_driver`, and focused
      compiler-driver regression coverage now locks global-array address
      materialization in call arguments (`addi rd, rs1, %lo(symbol)`)
    - staged reruns now reclose `perf/00_bitset1`, repeated full-course
      `autotest -perf /workspaces/compiler_lab` verification has re-passed the
      front/middle perf suite through `perf/12_fft0`, and the remaining tail
      `13_fft1 .. 19_brainfuck-calculator` has also been rechecked green
      one-by-one with the current `build/compiler`
    - current remaining perf work is therefore no longer correctness, but
      only optional performance-score tuning if/when that line is explicitly
      reopened
  - `void` compatibility line: **course-complete / effectively 100% toward ideal**
    with the old front-end-only compatibility bridge now materially shrunk
  - SysY builtin callable-visibility line: **course-complete / roughly 80% toward ideal**
  - regression / ideal-state tail: **checkpoint-near / roughly 98%**
  - full public course `-riscv` baseline:
    **complete / 130 of 130 passing**
    under
    `CDE_LIBRARY_PATH=/opt/lib CDE_INCLUDE_PATH=/opt/include /opt/bin/autotest -riscv -t /opt/bin/testcases /workspaces/compiler_lab`
  - older closure lines remain maintenance-first:
    `lv7` is complete, `SEMA-CF-001` is checkpoint-near, and
    `machine_select` cleanup is effectively checkpointed unless a concrete bug
    reopens them
  - Immediate implementation order for the current round:
    1. update roadmap authority first whenever the active line changes
    2. finish cloning and keeping locally available the full external-suite
       set (`compiler2021`, `minic-test-cases-2021s`,
       `minic-test-cases-2021f`, `segviol/indigo`,
       `TrivialCompiler/TrivialCompiler`, `ustb-owl/lava-test`,
       `jokerwyt/sysy-testsuit-collection`)
    3. run broad default-mode sweeps to hunt hidden-like `CTE/AE/RE/WA`
       families
    4. prioritize real reproduced failures in this order:
       hidden `17_while_if2`, hidden `22_nested_calls`, the known
       `RE=segfault` cluster, then the remaining public compile-time tails
       (`107_long_code2.c`, `many_parameters10000.c`)
    5. rerun promising return-flow-shaped failures under
       `--enforce-all-paths-return-check`
    6. minimize each confirmed bug to a standalone SysY repro and lock it in
       repository regressions
- Current staged `lv9` plan should now be read as:
  - `LV9-A` syntax admission: **mostly complete / roughly 75%**
    lexer/parser/AST now accept bracket tokens, subscript expressions, array
    rank metadata, and brace-initializer token consumption well enough to
    move failures downstream
  - `LV9-B` semantic stabilization: **mostly complete / roughly 85%**
    subscript expression traversal is now materially in place, and
    brace-initializer expressions are no longer parser-only AST leaves:
    semantic scope / flow walks now recurse through `AST_EXPR_INIT_LIST`
    instead of silently skipping it
  - `LV9-C` canonical IR storage-model reopen: **in progress / roughly 82%**
    current canonical IR now preserves array-object metadata for
    locals/globals/parameters and has the first explicit address/indirect
    opcode family (`addr_local`, `addr_global`, `load_indirect`,
    `store_indirect`); local/global array initializer lowering now also
    expands real brace-initializer writes instead of keeping arrays as
    declaration-only metadata, but the remaining closure is to finish the
    deeper parameter-array / decay cases and to normalize the new more-honest
    IR expectations across focused regressions
  - `LV9-D` lower-IR and downstream projection: **in progress / roughly 78%**
    `lower_ir`, `value_ssa`, and `machine_ir` already know the new indirect
    opcode family structurally, but the active closure is still to make the
    selected/backend path consume those ops for real instead of stopping at
    the old slot-only selected memory model
    - current temporary honesty boundary:
      programs that use indirect array memory now stay on the direct
      `value_ssa_build_from_lower_ir(...)` route instead of the default
      memory-SSA/value canonicalization pipeline, because the current
      `memory_ssa_pass` / `value_ssa_canonicalize_program(...)` path is still
      scalar-slot-centric and does not yet canonicalize indirect memory
      soundly
    - current landed closure inside that stage:
      `machine_select -> machine_layout -> machine_emit -> machine_bytes`
      now materially accept and lower the first `addr_local`, `addr_global`,
      `load_indirect`, and `store_indirect` slice well enough for real
      array-element and parameter-array code to reach preview RISC-V output
      on representative cases, and global object byte-size metadata now
      flows far enough downstream that array globals are no longer forced
      into 4-byte scalar object shells in the preview emitter/object path
  - `LV9-E` course closure: **complete / 100%**
    local/global arrays, array parameters/decay, initializer semantics, and
    final `autotest -riscv -s lv9` closure are all done
  - Immediate implementation order for the current round:
    1. snapshot and commit the now-green `lv9` branch
    2. return to the reopened downstream RISC-V artifact-honesty line
       (`machine_reloc -> machine_elf`) on the next round
- The downstream observe/runtime provenance follow-through line
  (`machine_elf -> ... -> machine_journal`) is now effectively
  **checkpointed end-to-end** for the current workstream.
- The default code-changing backend mainline therefore returns to the earlier
  **RISC-V correctness / artifact-honesty reopen** spanning:
  `machine_select -> machine_bytes -> machine_reloc -> machine_elf`.
- For "现在做到哪了" on the active backend mainline, prefer the following
  snapshot unless the work visibly shifts again:
  - `machine_select` immediate/legalization support: roughly `99%`
  - `machine_bytes` RISC-V preview honesty slice: roughly `99%`
  - `machine_reloc` preview relocation-honesty slice: roughly `96%`
  - `machine_elf` external-artifact honesty follow-through: roughly `98%`
  - reopened upstream RISC-V backend line overall: roughly `97%`
- Current concrete wording should prefer:
  - `machine_select` is effectively checkpointed for this reopen
  - that same selected-side surface now also has a first explicit
    compatibility/policy summary on program/report entrypoints, so upstream
    consumers can query the current preview register cap and the selected-side
    legalization/preservation facts the downstream RISC-V preview lane relies
    on without reopening `machine_bytes`
  - that same selected-side compatibility surface now also has a first
    explicit `riscv32-preview` verifier entrypoint, so selected programs that
    exceed the current preview register-cap boundary can fail at
    `machine_select` instead of only much later in `machine_bytes`
  - that same compatibility/policy surface now also survives through
    `machine_emit` and `machine_encode` reports, so the current preview-lane
    register-cap / spill-preservation / global-slot-preservation facts remain
    visible across the whole `machine_select -> machine_emit -> machine_encode
    -> machine_bytes` mainline instead of dropping out in the middle
  - that same middle-layer compatibility surface now also has explicit
    `riscv32-preview` verifier entrypoints on both `machine_emit` and
    `machine_encode`, so oversized logical register banks can now fail
    cleanly at any stage from selected lowering through abstract encoding,
    instead of only after the bytes bridge
  - that same early-compatibility chain now also reaches `machine_layout`, so
    the current preview register-cap boundary can fail cleanly from
    `machine_select` through layout, emit, encode, and bytes rather than only
    at the later encoding side
  - `machine_layout` now also has a first direct program/function/block query
    surface, so the mainline no longer has to jump straight from selected-side
    structured navigation to emit-side reports whenever it wants structured
    layout inspection
  - `machine_layout` now also has a first structured report artifact with
    cached function/block summaries plus policy facts, so the mainline can now
    stay artifact-oriented across `machine_select -> machine_layout ->
    machine_emit` instead of skipping directly from selected reports to emit
    reports
  - that same chain now also uses a consistent explicit compatibility-verifier
    surface all the way through `machine_bytes`, so every active lowering
    stage from selected form through byte-bearing output can reject the
    current preview register-cap incompatibility in its own native API layer
  - `machine_encode` now also performs one first deeper preview-bridge
    compatibility check rather than only local shape screening, so current
    preview bytes-range failures such as out-of-range branch/jump/call shapes
    can now be rejected at encode time instead of only after entering
    `machine_bytes`
  - the same deeper preview-bytes compatibility check now also reaches
    `machine_emit` and `machine_layout`, so the current bytes-range failure
    class can already surface from layout/emit/encode layers rather than only
    after the final byte-bearing bridge
  - that same deeper preview-bytes compatibility check now also reaches
    `machine_select`, so the current bytes-range failure class can be
    rejected from the earliest selected boundary instead of only after the
    later layout/emit/encode stages
  - `machine_bytes` is the nearest active upstream sibling when we are
    deepening preview lowering semantics or locking them with regression
    coverage
  - a first real `compiler` CLI entrypoint is now wired into the repository:
    `compiler -riscv input.sy -o output` and `compiler -perf input.sy -o output`
    both drive the current SysY -> canonical IR -> lower IR -> value SSA ->
    default memory-canonicalized Value-SSA -> machine IR -> preview byte bridge
    and currently emit honest
    RISC-V-flavored textual assembly for the current `rv32im`-shaped preview
    subset, including an explicit `.attribute arch, "rv32im"` header plus
    function metadata/local labels, and a first explicit
    `.section .sbss,"aw",@nobits` / `.section .sdata,"aw",@progbits`
    surface for globals with current preview `lui %hi(symbol)` +
    `lw/sw %lo(symbol)(reg)` address-formation/load-store text for both loads
    and stores, rather than
    falling back to the older internal `machine_elf` dump or pretending a
    fully pretty / ABI-complete final assembly printer already exists
  - that same preview asm surface is now also smoke-checked against a real
    assembler boundary in this environment: the repository test matrix feeds a
    representative `compiler -riscv` output through `llvm-mc -triple=riscv32`
    and requires object-file assembly to succeed, so the current text output
    is no longer only "human-readable preview asm" but "assembler-accepted
    preview asm" for the covered subset
  - the default lower-ir -> memory-full canonicalization line no longer drops
    a helper function's `store_global` just because the helper itself does not
    read that global again locally; a focused regression now locks the
    caller-observed global-store case, and `compiler -riscv` is back on the
    default canonicalization route rather than a temporary classic-line
    workaround
  - `-koopa` remains intentionally unsupported for now, because this
    workstream is targeting the direct RISC-V-side backend line rather than
    reopening a separate Koopa output contract
  - the latest `machine_bytes` reopen landed large local/global slot-offset
    honesty for `riscv32-preview`, so slot-based memory ops no longer depend
    on 12-bit offset luck any more than spill/frame accesses do
  - that same reopened `machine_bytes` line now also has one explicit preview
    register-bank honesty boundary: the current `riscv32-preview` lane only
    claims `reg.0..reg.7 -> a0..a7`, and larger logical register banks now
    fail visibly at `machine_bytes` instead of silently aliasing through
    modulo-8 register mapping
  - that same reopened `machine_bytes` line now also exposes a consumer-facing
    target-policy summary on profile/program/report surfaces, so later stages
    can query the current preview register cap, internal PC-relative honesty,
    direct-fallthrough honesty, paired global addressing, and RV32M-shaped
    arithmetic support without inferring those facts from implementation
    details
  - that same consumer-facing policy surface now also survives through
    `machine_object`, `machine_reloc`, and `machine_elf` summaries, so
    downstream artifact consumers no longer lose those bytes-side capability
    facts when they cross into later object / relocation / ELF layers
  - that same reopened `machine_bytes` line now also has an explicit
    honest-failure boundary for internal preview PC-relative control/call
    targets, so out-of-range branch/jump/call displacements fail visibly
    instead of silently truncating into invalid immediates
  - `machine_reloc` remains the next most likely structural reopen if the
    pressure is about direct-preview versus imported/reprofiled relocation
    honesty
  - the latest `machine_reloc` reopen landed relocation-family summary
    queries plus verifier-backed preview addend/fallthrough honesty, so
    relocation consumers no longer need to hand-scan rows for basic family
    counts and malformed preview addend drift now fails at the relocation
    boundary itself; that same consumer-facing surface now also has a first
    structured report artifact above the raw relocation file
  - that same reopened bytes/object/reloc line now also carries one first
    real global-object artifact surface instead of leaving globals only in
    the compiler text exporter: `machine_bytes` reports now surface
    `.sbss` / `.sdata` section summaries plus `global-object` symbols, and
    direct `machine_object -> machine_reloc` builds now preserve those
    sections structurally even when they carry zero relocations
  - that same object/reloc consumer-facing dump/report surface now also keeps
    explicit target-profile names instead of flattening non-RISC-V preview
    lanes back into generic or numeric profile placeholders, so downstream
    artifact snapshots can distinguish at least `generic`,
    `riscv32-preview`, and `i386-preview` directly without mirroring
    implementation-side enums
  - that same profile-name honesty is now also regression-locked on the
    non-RISC-V preview side rather than being only a generic-path cleanup:
    focused `machine_object` / `machine_reloc` tests now explicitly require
    `i386-preview` to survive through dump/report text too, so the surface
    can no longer silently regress back to a generic/numeric placeholder
  - that same artifact-honesty thread now also reaches `machine_container`
    instead of dropping the selected relocation profile/policy facts between
    `machine_reloc` and `machine_elf`: container dumps now surface the
    inherited profile name and the same addend/fallthrough policy summary the
    reloc layer already exposes, and the `i386-preview` lane is explicitly
    regression-locked there too
  - that same downstream honesty line now also reaches the actual
    `machine_elf` report dump surface instead of stopping at raw-file dump
    plus separate query helpers: report dumps now emit the cached artifact,
    header, policy, and relocation-family summaries explicitly before the
    embedded file dump, so callers no longer need to choose between
    “structured summary API” and “human-readable dump text” at the ELF
    boundary
  - that same reopened RISC-V line now also has one first explicit
    data-reference slice for global accesses: small preview `load_global` /
    `store_global` families may now survive as paired `data-addr` +
    `data-load` / `data-store` references/fixups/relocations, and direct
    preview ELF builds may now map those references to
    `R_RISCV_HI20 + R_RISCV_LO12_I/S` instead of leaving every global access
    as a text-only convention with no artifact-side relocation meaning
  - that same data-reference slice is now also visible in summary/query
    surfaces rather than only buried in raw relocation rows: relocation-family
    and target-policy helpers may now expose paired data-addr / data-load /
    data-store relocation kinds and RISC-V preview data-relocation opcode
    mappings directly
  - the latest `machine_object` reopen landed target-policy/fixup-family
    summaries plus verifier-backed preview target-byte-offset honesty, so the
    object boundary now preserves the same direct-preview contract more
    explicitly instead of forcing every later consumer straight to reloc/elf;
    that same family-summary surface now also counts data-side object fixups,
    not only call/control fixups, and now also has a first structured report
    artifact above the raw object file
  - `machine_elf` remains the downstream object-format boundary if the
    pressure is about making that information boundary explicit to external
    consumers
  - the latest `machine_elf` reopen landed verifier-backed direct-versus-
    imported preview semantics, so canonical direct preview ELF artifacts can
    no longer silently keep secondary control relocations while generic-origin
    reprofiling helpers still remain explicitly legal via `origin_profile`
  - that same direct-build `machine_elf` line now also preserves one first
    non-`.text` global-object slice from the upstream object chain: when the
    object artifact carries `.sbss` / `.sdata` sections and `global-object`
    symbols, direct ELF builds may now serialize those sections plus
    `STT_OBJECT` globals instead of flattening everything back to a strict
    `.text`-only object view, and the repository's own
    parse/normalize/refresh path can now round-trip that same canonical
    optional data-section family too
  - the latest landed `machine_elf` consumer-facing slice is one first
    relocation-family summary on raw/report artifacts, so external consumers
    no longer need to rescan relocation rows just to distinguish direct
    preview single-branch relocation shapes from imported/reprofiled
    two-control-relocation shapes
- The observe-side line is no longer the default answer for the active
  code-changing backend position. Use it as historical/checkpoint context,
  not as the default current frontier, unless a concrete downstream consumer
  bug reopens it.
- The just-finished Memory-SSA / Memory-SSA-pass line in
  [docs/ssa/MEMORY_SSA_DESIGN.md](/workspaces/compiler_lab/docs/ssa/MEMORY_SSA_DESIGN.md)
  is now checkpoint-ready / maintenance-first unless a concrete bug or
  deliberately chosen post-checkpoint expansion reopens it.
- The historical `machine_journal` snapshot remains useful as a record of the
  furthest previously landed downstream consumer stage, but it is no longer
  the default answer for the current code-changing backend position unless the
  work explicitly reopens journal-specific behavior.
- The Machine-Journal roadmap in
  [docs/backend/MACHINE_JOURNAL_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_JOURNAL_PLAN.md)
  should now be treated as the active downstream backend progress snapshot.
- The Machine-Log roadmap in
  [docs/backend/MACHINE_LOG_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LOG_PLAN.md)
  should now be treated as the just-checkpointed historical post-timeline
  log snapshot and as checkpoint-ready / maintenance-first unless a concrete
  log / bridge bug reopens it.
- The Machine-Timeline roadmap in
  [docs/backend/MACHINE_TIMELINE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TIMELINE_PLAN.md)
  should now be treated as the just-checkpointed historical post-history
  timeline snapshot and as checkpoint-ready / maintenance-first unless a
  concrete timeline / bridge bug reopens it.
- The Machine-History roadmap in
  [docs/backend/MACHINE_HISTORY_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_HISTORY_PLAN.md)
  should now be treated as the just-checkpointed historical post-outcome
  history snapshot and as checkpoint-ready / maintenance-first unless a
  concrete history / bridge bug reopens it.
- The Machine-Outcome roadmap in
  [docs/backend/MACHINE_OUTCOME_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_OUTCOME_PLAN.md)
  should now be treated as the just-checkpointed historical post-event
  outcome snapshot and as checkpoint-ready / maintenance-first unless a
  concrete outcome / bridge bug reopens it.
- The Machine-Event roadmap in
  [docs/backend/MACHINE_EVENT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_EVENT_PLAN.md)
  should now be treated as the just-checkpointed historical post-trace event
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  event / bridge bug reopens it.
- The Machine-Trace roadmap in
  [docs/backend/MACHINE_TRACE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TRACE_PLAN.md)
  should now be treated as the just-checkpointed historical post-delta trace
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  trace / bridge bug reopens it.
- The Machine-Delta roadmap in
  [docs/backend/MACHINE_DELTA_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_DELTA_PLAN.md)
  should now be treated as the just-checkpointed historical post-observe
  delta snapshot and as checkpoint-ready / maintenance-first unless a
  concrete delta / bridge bug reopens it.
- The Machine-Observe roadmap in
  [docs/backend/MACHINE_OBSERVE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_OBSERVE_PLAN.md)
  should now be treated as the just-checkpointed historical post-apply
  observe snapshot and as checkpoint-ready / maintenance-first unless a
  concrete observe / bridge bug reopens it.
- The Machine-Apply roadmap in
  [docs/backend/MACHINE_APPLY_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_APPLY_PLAN.md)
  should now be treated as the just-checkpointed historical post-commit apply
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  apply / bridge bug reopens it.
- The Machine-Commit roadmap in
  [docs/backend/MACHINE_COMMIT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_COMMIT_PLAN.md)
  should now be treated as the just-checkpointed historical post-writeback
  commit snapshot and as checkpoint-ready / maintenance-first unless a
  concrete commit / bridge bug reopens it.
- The Machine-Writeback roadmap in
  [docs/backend/MACHINE_WRITEBACK_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_WRITEBACK_PLAN.md)
  should now be treated as the just-checkpointed historical post-mutation
  writeback snapshot and as checkpoint-ready / maintenance-first unless a
  concrete writeback / bridge bug reopens it.
- The Machine-Mutation roadmap in
  [docs/backend/MACHINE_MUTATION_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_MUTATION_PLAN.md)
  should now be treated as the just-checkpointed historical post-state
  mutation snapshot and as checkpoint-ready / maintenance-first unless a
  concrete mutation / bridge bug reopens it.
- The Machine-State roadmap in
  [docs/backend/MACHINE_STATE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_STATE_PLAN.md)
  should now be treated as the just-checkpointed historical post-transition
  state snapshot and as checkpoint-ready / maintenance-first unless a concrete
  state / bridge bug reopens it.
- The Machine-Transition roadmap in
  [docs/backend/MACHINE_TRANSITION_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TRANSITION_PLAN.md)
  should now be treated as the just-checkpointed historical post-interp
  transition snapshot and as checkpoint-ready / maintenance-first unless a
  concrete transition / bridge bug reopens it.
- The Machine-Interp roadmap in
  [docs/backend/MACHINE_INTERP_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_INTERP_PLAN.md)
  should now be treated as the just-checkpointed historical post-payload
  execution-result snapshot and as checkpoint-ready / maintenance-first unless
  a concrete interp / bridge bug reopens it.
- The Machine-Payload-Decode roadmap in
  [docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md)
  should now be treated as the just-checkpointed historical post-decode
  payload-byte snapshot and as checkpoint-ready / maintenance-first unless a
  concrete payload-decode / bridge bug reopens it.
- The Machine-Decode roadmap in
  [docs/backend/MACHINE_DECODE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_DECODE_PLAN.md)
  should now be treated as the just-checkpointed historical post-step
  tag-decode snapshot and as checkpoint-ready / maintenance-first unless a
  concrete tag-decode / bridge bug reopens it.
- The Machine-Step roadmap in
  [docs/backend/MACHINE_STEP_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_STEP_PLAN.md)
  should now be treated as the just-checkpointed historical post-launch
  fetch-state snapshot and as checkpoint-ready / maintenance-first unless a
  concrete fetch-state / mapped-byte bug reopens it.
- The Machine-Launch roadmap in
  [docs/backend/MACHINE_LAUNCH_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LAUNCH_PLAN.md)
  should now be treated as the just-checkpointed historical post-runtime
  launch snapshot and as checkpoint-ready / maintenance-first unless a
  concrete launch-state / richer-register-seeding bug reopens it.
- The Machine-Runtime roadmap in
  [docs/backend/MACHINE_RUNTIME_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_RUNTIME_PLAN.md)
  should now be treated as the just-checkpointed historical post-load runtime
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  runtime-memory / launch-surface bug reopens it.
- The Machine-Load roadmap in
  [docs/backend/MACHINE_LOAD_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LOAD_PLAN.md)
  should now be treated as the just-checkpointed historical post-exec load
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  load-byte, entry-map, or upstream-bridge bug reopens it.
- The Machine-Exec roadmap in
  [docs/backend/MACHINE_EXEC_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_EXEC_PLAN.md)
  should now be treated as the just-checkpointed historical post-image exec
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  exec-entry or upstream-bridge bug reopens it.
- The Machine-Image roadmap in
  [docs/backend/MACHINE_IMAGE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_IMAGE_PLAN.md)
  should now be treated as the just-checkpointed historical post-ELF image
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  image-address or relocation-view bug reopens it.
- The Machine-ELF roadmap in
  [docs/backend/MACHINE_ELF_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_ELF_PLAN.md)
  should now be treated as the just-checkpointed historical post-container
  snapshot and as checkpoint-ready / maintenance-first unless a concrete
  external-ELF compatibility need reopens it.
- The Machine-Select / target-lowering roadmap in
  [docs/backend/MACHINE_SELECT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_SELECT_PLAN.md)
  should now be treated as a historical checkpoint-ready sibling rather than
  the active implementation authority.
- The Machine-Layout / branch-lowering roadmap in
  [docs/backend/MACHINE_LAYOUT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LAYOUT_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete layout bug or downstream consumer pressure reopens it.
- The Machine-Emit / label-surfacing roadmap in
  [docs/backend/MACHINE_EMIT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_EMIT_PLAN.md)
  should now be treated as checkpoint-near / maintenance-first unless a
  concrete emitted-artifact bug or downstream consumer pressure reopens it.
- The Machine-Encode / offset-assignment roadmap in
  [docs/backend/MACHINE_ENCODE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_ENCODE_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete encode-prep bug or downstream byte-bearing consumer pressure
  reopens it.
- The Machine-IR roadmap in
  [docs/backend/MACHINE_IR_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_IR_PLAN.md)
  should now be treated as a near-closed cleanup/checkpoint authority unless a
  concrete machine-ir consumer bug reopens it.
- The allocator-mainline authority in
  [docs/ssa/VALUE_SSA_ALLOCATOR_PLAN.md](/workspaces/compiler_lab/docs/ssa/VALUE_SSA_ALLOCATOR_PLAN.md)
  should now be treated as near-closed / checkpointed unless a concrete
  allocator bug reopens it.
- The machine-register-model authority in
  [docs/ssa/VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md](/workspaces/compiler_lab/docs/ssa/VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md)
  should now be treated as stage-close / maintenance-first unless a concrete
  `machine_ir` gap reopens it.
- The Machine-Bytes / byte-bearing roadmap in
  [docs/backend/MACHINE_BYTES_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_BYTES_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete byte/fixup bug or downstream object-facing consumer pressure
  reopens it.
- The Machine-Object / object-facing roadmap in
  [docs/backend/MACHINE_OBJECT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_OBJECT_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete object-surface bug or downstream relocation-facing consumer
  pressure reopens it.
- The Machine-Reloc / relocation-facing roadmap in
  [docs/backend/MACHINE_RELOC_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_RELOC_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete relocation-surface bug or downstream final-serialization consumer
  pressure reopens it.
- The Machine-Container / final-serialization roadmap in
  [docs/backend/MACHINE_CONTAINER_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_CONTAINER_PLAN.md)
  should now be treated as checkpoint-ready / maintenance-first unless a
  concrete container-surface bug or downstream target-specific-format consumer
  pressure reopens it.
- The next recommended post-`machine_container` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_ELF_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_ELF_PLAN.md).
- The next recommended post-`machine_elf` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_IMAGE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_IMAGE_PLAN.md).
- The next recommended post-`machine_image` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_EXEC_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_EXEC_PLAN.md).
- The next recommended post-`machine_exec` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_LOAD_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LOAD_PLAN.md).
- The next recommended post-`machine_load` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_RUNTIME_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_RUNTIME_PLAN.md).
- The next recommended post-`machine_runtime` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_LAUNCH_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LAUNCH_PLAN.md).
- The next recommended post-`machine_launch` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_STEP_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_STEP_PLAN.md).
- The next recommended post-`machine_step` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_DECODE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_DECODE_PLAN.md).
- The next recommended post-`machine_decode` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md).
- The next recommended post-`machine_payload_decode` consumer-planning
  authority has now advanced to
  [docs/backend/MACHINE_INTERP_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_INTERP_PLAN.md).
- The next recommended post-`machine_interp` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_TRANSITION_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TRANSITION_PLAN.md).
- The next recommended post-`machine_transition` consumer-planning authority
  has now advanced to
  [docs/backend/MACHINE_STATE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_STATE_PLAN.md).
- The next recommended post-`machine_state` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_MUTATION_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_MUTATION_PLAN.md).
- The next recommended post-`machine_mutation` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_WRITEBACK_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_WRITEBACK_PLAN.md).
- The next recommended post-`machine_writeback` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_COMMIT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_COMMIT_PLAN.md).
- The next recommended post-`machine_commit` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_APPLY_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_APPLY_PLAN.md).
- The next recommended post-`machine_apply` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_OBSERVE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_OBSERVE_PLAN.md).
- The next recommended post-`machine_observe` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_DELTA_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_DELTA_PLAN.md).
- The next recommended post-`machine_delta` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_TRACE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TRACE_PLAN.md).
- The next recommended post-`machine_trace` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_EVENT_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_EVENT_PLAN.md).
- The next recommended post-`machine_event` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_OUTCOME_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_OUTCOME_PLAN.md).
- The next recommended post-`machine_outcome` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_HISTORY_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_HISTORY_PLAN.md).
- The next recommended post-`machine_history` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_TIMELINE_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_TIMELINE_PLAN.md).
- The next recommended post-`machine_timeline` consumer-planning authority has
  now advanced to
  [docs/backend/MACHINE_LOG_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_LOG_PLAN.md).
- The next recommended post-`machine_log` consumer-planning authority has now
  advanced to
  [docs/backend/MACHINE_JOURNAL_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_JOURNAL_PLAN.md).
- When asking "现在做到哪了", prefer the staged percentages recorded in the
  currently active plan document.
- The previous post-`machine_container` consumer-planning authority is now in
  [docs/backend/MACHINE_ELF_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_ELF_PLAN.md).
- The previous post-`machine_reloc` consumer-planning authority is now in
  [docs/backend/MACHINE_CONTAINER_PLAN.md](/workspaces/compiler_lab/docs/backend/MACHINE_CONTAINER_PLAN.md).
- For the just-checkpointed machine-elf line, the relevant historical
  snapshot is now in `docs/backend/MACHINE_ELF_PLAN.md`, especially:
  - `Stage MELF4 target profile / relocation policy plumbing`
  - `Stage MELF5 ELF parse / round-trip surface`
- For the just-checkpointed machine-layout line, the relevant historical
  snapshot is now in `docs/backend/MACHINE_LAYOUT_PLAN.md`, especially:
  - `Stage ML2 first lowering from machine_select`
  - `Stage ML3 first upstream bridge`
- For the just-checkpointed machine-emit line, the relevant historical
  snapshot is now in `docs/backend/MACHINE_EMIT_PLAN.md`, especially:
  - `Stage ME1 representation skeleton`
  - `Stage ME2 first lowering from machine_layout`
  - `Stage ME3 first upstream bridge`
- For the just-checkpointed machine-encode line, the relevant historical
  snapshot is now in `docs/backend/MACHINE_ENCODE_PLAN.md`, especially:
  - `Stage MC1 representation skeleton`
  - `Stage MC2 first lowering from machine_emit`
  - `Stage MC3 first upstream bridge`
- For the just-closed machine-ir line, the relevant historical snapshot is
  still in `docs/backend/MACHINE_IR_PLAN.md`, especially:
  - `Stage MIR1 representation skeleton`
  - `Stage MIR2 first Value-SSA bridge`
  - `Stage MIR3 first consumer migration`
  - `Stage MIR4 machine cleanup pressure`

## Current State

- Milestone A: closed. Semantic-authority retirement is complete.
- Milestone B: closed. Parser/AST internal decoupling and metadata-retention cleanup are complete.
- Milestone C: closed. `test-fanalyzer`, `test-asan`, and `test-strict-warnings` have green baselines.
- Milestone D: maintenance-mode only. Reopen for concrete semantic bugs.
- Milestone E: closed. Canonical IR bootstrap, verifier hardening, declaration/global/runtime-init support, and dependency precision guardrails are in place.
- Milestone F0: complete. Pass pipeline scaffold is landed.
- Milestone F1: effectively phase-complete. Shared analysis surfaces cover def/use, CFG, temp equivalence, and side-effect classification needed by current transforms.
- Milestone F2: soft-closed. Low-risk canonical-IR transforms are broad enough for this phase and should now be reopened only for real missed-cleanup or correctness issues.
- Milestone F3: soft-closed. Fixed-point, semantic guardrail, canonical-form, and stepwise verifier protections are landed.
- Lower IR: phase 0 is frozen, phase 1 skeleton is landed, and the current phase 2 checkpoint now covers representative serial lowering, verifier hardening, and builder/API fail-fast enough to enter maintenance-first mode.
- Value SSA: phase S1 skeleton is landed as a separate module with manual-construction builder helpers, dump, verifier, and isolated regression/verifier tests; phase S2 now has an initial lower-IR conversion slice for straight-line, single-join diamond, simple slot-carried loops, and a first representative loop-header phi shape for loop-carried temps. Separate narrow cleanup layers are now landed for local mov/phi use-site cleanup, trivial CFG cleanup (`same-target br`, immediate-condition `br`, dead-block/value-id compaction), dead pure value-definition cleanup (`phi`, `mov`, `binary`, `load_*`) while preserving effectful `call/store_*`, and conservative slot-side cleanup/forwarding around straight-chain known local/global values. A first shared CFG analysis/helper layer is also landed for predecessor/reachability/dominator-tree/frontier facts, iterated phi-placement closure, dominator-tree preorder, block-level liveness, a first conservative liveness-driven interference graph, a first non-interfering copy-affinity graph over `mov` edges, a first bundled allocation-prep summary surface over those analyses, a first stable allocation-worklist surface over that summary, a split dominator-tree enter/leave walk scaffold, a first scoped rename-state stack, a first block-local use-site rewrite helper, a first predecessor-specific phi-input rewrite helper, a first verifier-safe function-level alpha-renaming orchestration, and a first small program-level canonicalization entrypoint on verifier-legal SSA programs.
- Value SSA: there is now also a first execution-oriented sibling module for direct SSA evaluation. `include/value_ssa_interp.h` and `src/value_ssa_interp/` provide a narrow interpreter that can execute verifier-legal SSA functions for regression/oracle use without redefining the IR boundary or drifting into memory SSA.
- Value SSA: the recent result-layer hardening slice is considered reviewer-cleared for continued implementation. Current bridge/canonicalization authority is strong enough that progress no longer needs to pause for more result-layer-only review unless a new concrete bug appears.
- Lower IR: a first CFG/def-site analysis slice is now landed for the real conversion mainline. The input layer can now compute predecessor counts, reachability, dominators, immediate dominators, dominator-tree children, dominance frontiers, temp definition blocks, and iterated phi placement directly on verifier-legal lower-IR functions.
- Lower IR: the current conversion mainline can now also source formal dominator-tree traversal from the input layer and drive block conversion directly from it, so future rename/construction work no longer needs to keep an ad hoc CFG-ordering fallback alive once lower-IR analysis is present.
- Lower IR: input-layer analysis authority now also covers a representative multi-backedge loop-header join-temp family, not only the earlier single-backedge loop-header example.
- Value SSA: representative conversion coverage now also includes a multi-backedge loop-carried temp shape, not only the earlier single-backedge loop-header phi example.
- Machine Select: the selected-lowering line is now effectively at a
  checkpointed backend boundary. A real sibling module with verifier, dump,
  query/summary surfaces, canonicalized `machine_ir` bridge, selected cleanup,
  and explicit call/result/return family shaping is landed strongly enough
  that further expansion should now be need-driven rather than the default
  active mainline.
- Machine Image: a first sibling post-ELF load-image-prep layer is now landed
  with separate `include/machine/image.h`, `src/machine/runtime/machine_image/`, and
  `tests/machine/runtime/machine_image/` surfaces. The current slice has a real verifier,
  dump, summary/query helpers, direct lowering from `machine_elf`, and an
  upstream bridge from canonicalized `machine_ir` through the current object /
  ELF pipeline. Current lowering projects `.text` into one first load segment
  with a profile-owned base virtual address, surfaces virtual addresses for
  image-defined text symbols, selects one entry symbol/address when a suitable
  global text function exists, and exposes relocation sites as resolved
  in-image targets versus unresolved external obligations. The module now also
  has a first structured report artifact above that raw image file, including
  cached header / entry-symbol / segment / symbol / relocation summaries plus
  resolved-versus-unresolved relocation index sets. Raw and report-side
  consumer navigation are now also more address-oriented than the first cut:
  callers can locate segment coverage by virtual address, symbol summaries by
  virtual address, relocation summaries by site virtual address, and
  resolved-versus-unresolved relocation subsets directly instead of rescanning
  every table manually. That same query surface now also has first explicit
  segment-local symbol/relocation subset access on both raw image files and
  structured image reports, so later image consumers can inspect one load
  segment's resident symbols and relocation sites without replaying a whole
  image-table filter loop. The structured report layer now also caches those
  segment-local symbol/relocation partitions, including resolved-versus-
  unresolved relocation subsets per segment, so downstream consumers no
  longer need to reconstruct those filtered index views from the whole-image
  tables on every query. That same report layer now also exposes first
  segment-artifact retrieval by segment index, name, or covered virtual
  address, so a downstream consumer can step from one bridged image report to
  one cached segment-scoped artifact directly instead of orchestrating a
  bundle of separate segment-local queries. That same object-style consumer
  boundary now also reaches cached symbol artifacts and relocation artifacts,
  including source-segment and resolved-target context where available, so
  later callers can traverse one bridged image report as a linked artifact
  graph rather than as disconnected summary tables. That same linked artifact
  graph is now also navigable from symbol targets back to their incoming
  relocation subsets, including resolved-versus-unresolved partitions, so
  downstream consumers can inspect one target symbol's image obligations
  without reconstructing reverse edges by hand. That same report boundary now
  also offers a higher-level overview artifact with cached symbol-class
  partitions, so a downstream caller can begin from top-level image summary
  slices and only descend into detailed segment/symbol/relocation artifacts
  where needed. That same overview entry now also covers top-level
  relocation-class slices such as resolved/unresolved and call/control
  families, so common image-obligation views no longer need to be rebuilt
  outside the module either. That same report structure is now also surfaced
  in `machine_image_dump_report(...)` and the report-dump bridge entrypoints,
  so text-mode consumers of machine-ir/ELF/bytes can see the bridged
  overview/filter/cache picture directly instead of only the raw image table
  dump. That bridge story now also reaches existing
  `MachineElfReport` artifacts directly rather than only raw ELF files, and
  now also reaches ELF bytes directly for image file / image report / dump
  construction without forcing a caller to stage an explicit intermediate
  `MachineElfFile`. The module now also has a first artifact lifecycle of its
  own through image-file clone, report refresh, and direct raw/report dump
  helpers from image/ELF artifacts, so downstream experiments can keep image
  artifacts alive and re-summarize them locally instead of rebuilding all the
  glue by hand. That same image-prep surface now also has explicit
  target-policy summary artifacts plus profile-aware build/report/dump/
  report-dump variants across image-file, ELF-file, ELF-report, ELF-bytes,
  and direct `MachineIrAllocateRewriteReport` bridge inputs, so later
  consumers can choose the active target profile at the image boundary
  without reopening `machine_elf` policy plumbing by hand. This is
  intentionally still image-prep rather than a full linker or final
  executable mainline: default next work should stay focused on broadening
  the load-image artifact itself unless a later multi-object or executable
  step is explicitly chosen.
- Machine Exec: a first sibling post-image executable-prep layer is now landed
  with separate `include/machine/exec.h`, `src/machine/runtime/machine_exec/`, and
  `tests/machine/runtime/machine_exec/` surfaces. The current slice has a real verifier,
  dump, summary/query helpers, direct lowering from `machine_image`, and an
  upstream bridge from canonicalized `machine_ir` through the current image
  pipeline. Current lowering intentionally stays conservative: it requires a
  known entry address, rejects images with unresolved relocations, now also
  requires that the chosen entry lands inside an executable segment,
  preserves current image segment order/virtual addresses, and surfaces one
  first executable permission policy over those segments instead of
  pretending to be a full loader or final runtime container. The module now
  also has a first structured report artifact above the raw exec file,
  including cached header / segment summaries plus executable-segment index
  subsets, and that same report-owned surface is now reflected in the report
  dump helpers too. Raw and report-side consumer navigation are now also more
  entry/address-aware than the first cut: callers can recover the entry
  segment directly and find the covering segment for one virtual address
  without rescanning the whole segment table manually. That same report layer
  is now also more artifact-oriented than the first cut: callers can start
  from an overview artifact, recover segment artifacts by index/name/covered
  address, and walk executable / non-executable / entry segment filter views
  instead of hand-rebuilding those common slices outside the module. The
  bridge side already reaches `MachineImageFile`, `MachineImageReport`, and
  `MachineIrAllocateRewriteReport` for raw exec files, exec reports, and
  direct dump/report-dump helpers, and the module now also has a first small
  artifact lifecycle of its own through exec-file clone, report refresh, and
  direct dump-from-file helpers. That same upstream bridge family now also
  reaches `MachineElfFile`, `MachineElfReport`, ELF bytes, and profile-aware
  `machine_ir` bridge variants directly for raw exec files, exec reports,
  direct dumps, and report dumps. So the new downstream line can already be
  exercised end-to-end without reopening earlier backend siblings. This is
  intentionally still executable-prep rather than full linker / loader /
  runtime semantics: default next work should stay focused on broadening this
  executable-candidate boundary unless a later runtime-facing mainline is
  explicitly chosen.
- Machine Load: a first sibling post-exec loader-prep layer is now landed
  with separate `include/machine/load.h`, `src/machine/runtime/machine_load/`, and
  `tests/machine/runtime/machine_load/` surfaces. The current slice has a real verifier,
  dump, summary/query helpers, direct lowering from `machine_exec`, and an
  upstream bridge from canonicalized `machine_ir` through the current image /
  exec pipeline. Current lowering intentionally stays conservative: it
  preserves segment order, permissions, and virtual addresses while
  materializing explicit owned byte-bearing load segments, now rounds
  `memory_byte_count` up through a profile-owned load-alignment policy,
  explicitly zero-fills the tail beyond current file bytes inside the owned
  payload, and surfaces the entry-containing load segment without yet
  claiming page-table policy beyond that first alignment rule, stack setup,
  or process runtime semantics. The module now also has a first structured
  report artifact above the raw load file, including cached header / segment
  summaries plus executable and non-executable segment index subsets, and
  that same report-owned surface is reflected in the report dump helpers too.
  The active load-alignment / zero-fill rule is now also surfaced as an
  explicit target-policy artifact on both raw and report sides instead of
  being hidden entirely inside lowering. Raw and report-side consumer
  navigation are now also more load-map-aware than the first cut: callers can
  recover the entry segment directly, find the covering segment for one
  virtual address, read one mapped byte by virtual address, copy one whole
  flat memory image, and walk executable / non-executable / entry segment
  filter views without rescanning the whole load-segment table manually. The
  bridge side already reaches `MachineExecFile`, `MachineExecReport`,
  `MachineImageFile`, `MachineImageReport`, `MachineElfFile`,
  `MachineElfReport`, ELF bytes, and profile-aware `machine_ir` bridge
  variants for raw load files, load reports, direct dumps, and report dumps.
  This line should now be treated as checkpoint-ready / maintenance-first by
  default: the next deliberately chosen runtime-facing mainline has already
  advanced downstream to `machine_runtime`.
- Machine Runtime: a first sibling post-load process-prep layer is now landed
  with separate `include/machine/runtime.h`, `src/machine/runtime/machine_runtime/`, and
  `tests/machine/runtime/machine_runtime/` surfaces. The current slice has a real verifier,
  dump, summary/query helpers, direct lowering from `machine_load`, and an
  upstream bridge from canonicalized `machine_ir` through the current image /
  exec / load pipeline. Current lowering intentionally stays conservative: it
  preserves load-backed runtime segments and entry PC, adds one synthetic
  writable non-executable stack segment through a profile-owned
  `stack_alignment` / `stack_byte_count` / `stack_gap_byte_count` policy,
  surfaces one initial stack pointer at the top of that stack segment, and
  exposes one first unified runtime memory view over load + stack segments
  without yet claiming argv/env/auxv materialization, scheduler semantics, or
  full OS/runtime behavior. The module now also has a first structured report
  artifact above the raw runtime file, including cached header / target-policy
  / memory / segment summaries plus executable and non-executable segment
  index subsets and direct stack-segment ownership. Raw and report-side
  consumer navigation are now also more runtime-memory-aware than the first
  cut: callers can recover the entry segment directly, recover the stack
  segment directly, find the covering segment for one virtual address, read
  one mapped byte by virtual address, copy one whole flat runtime image, and
  walk executable / non-executable / entry / stack filter views without
  rescanning the runtime-segment table manually. That same runtime-memory
  surface now also reaches flat memory offsets directly: callers can ask
  which runtime segment owns one flat runtime-memory offset, read one mapped
  byte by offset, copy one segment's owned bytes directly, and recover one
  report-side segment summary/artifact by offset instead of manually
  translating back to virtual addresses. The bridge side already
  reaches `MachineLoadFile`, `MachineLoadReport`, `MachineExecFile`,
  `MachineExecReport`, `MachineImageFile`, `MachineImageReport`,
  `MachineElfFile`, `MachineElfReport`, ELF bytes, and profile-aware
  `MachineIrAllocateRewriteReport` variants for raw runtime files, runtime
  reports, direct dumps, and report dumps. This is intentionally still
  process-prep rather than full runtime semantics: default next work should
  stay focused on broadening this runtime/process artifact unless a later
  launch or execution consumer layer is explicitly chosen.
- Machine Emit: a first sibling label-surfacing post-layout layer is now
  landed with separate `include/machine/emit.h`, `src/machine/lowering/machine_emit/`, and
  `tests/machine/lowering/machine_emit/` surfaces. The current slice has a real verifier,
  dump, summary surface, direct lowering from `machine_layout`, and an
  upstream bridge from canonicalized `machine_ir` through `machine_select`
  and `machine_layout`. Current lowering preserves selected ops and lowered
  layout terminator families while assigning stable globally unique emitted
  labels in block order, so dumps can now surface artifact-ready names such
  as `F0.L0`, `F0.L1`, ... rather than only raw `layout.N` indices. The
  public surface now also has first
  whole-program/per-function query helpers plus direct `lower + dump`
  convenience entrypoints, so later backend consumers no longer need to
  rebuild that emission-prep plumbing by hand. That query surface now also
  reaches emitted blocks directly: current authority includes function lookup
  by name, emitted-block lookup by emit index or globally unique label, and block-level
  summary over emitted/original ids plus terminator kind. A first structured
  report artifact is now landed above that query layer too: direct
  `machine_layout` input and canonicalized `machine_ir`/`MachineIrAllocateRewriteReport`
  bridge input can now produce emitted-program shape reports with per-function
  summaries, per-block shape summaries, and prefiltered function index sets
  for emitted call/fallthrough/branch families. That report surface now also
  has first exact dump entrypoints on both the direct-layout side and the
  `MachineIrAllocateRewriteReport` bridge side, so artifact-style emitted
  inspection no longer needs a hand-written wrapper around report build plus
  dump. Program/report lookup now also treats emitted labels as globally
  unique artifact names rather than only function-local shorthands. On top of
  that, an existing emitted program can now be deep-cloned, turned back into
  a fresh emitted report, and have that report shape refreshed explicitly
  after later local mutations. This is
  intentionally still emission-prep rather than final encoding:
  default next work should stay bug-driven or
  downstream-consumer-driven unless a later encoding mainline is explicitly
  chosen.
- Machine Encode: a first sibling offset-aware post-emit layer is now landed
  with separate `include/machine/encode.h`, `src/machine/lowering/machine_encode/`, and
  `tests/machine/lowering/machine_encode/` surfaces. The current slice has a real verifier,
  dump, summary surface, direct lowering from `machine_emit`, and an upstream
  bridge from canonicalized `machine_ir` through `machine_select`,
  `machine_layout`, and `machine_emit`. Current lowering intentionally uses a
  tiny abstract code-unit model rather than real bytes: each selected op gets
  one unit and each terminator gets one unit, which is already enough to give
  later consumers stable block `start/end` offsets and target offsets in
  dumps without prematurely locking an ISA byte format. The first surface now
  also has small function/block summary helpers plus a deep-clone path for
  encoded programs themselves, so later post-encode experiments do not need
  to bounce back to `machine_emit` after every local mutation. That helper
  surface now also reaches program-level label lookup plus function-local
  offset-to-block lookup, so downstream consumers can resolve one encoded
  block by either emitted name or abstract code-unit position without
  restaging manual scans. It now also has a first structured terminator-target
  summary API, so consumers can ask one encoded block for resolved target
  labels and target offsets directly instead of reparsing the dump text.
  Verifier-side authority is now also a bit more real there: encoded
  terminators are checked for in-range target indices instead of only
  validating offset contiguity/span bookkeeping. A first
  structured `machine_encode` report artifact is now also landed above the raw
  encoded program, with per-function summaries, per-block offset summaries,
  and direct report dumps from both `machine_emit` and `machine_ir` input.
  That bridge side now also accepts an existing `MachineEmitLowerReport`
  artifact directly for encode program/report/dump construction, so downstream
  consumers that already live on emitted reports do not need to peel the
  raw emitted program back out by hand just to continue into encode.
  That same bridge story now also reaches `MachineIrAllocateRewriteReport`
  directly for encode program/report/dump construction, so a report-oriented
  downstream consumer can continue from machine-ir all the way through encode
  without manually rebuilding intermediate emitted artifacts.
  The encoded report side now also has first high-level function-index sets
  for call-bearing, fallthrough-bearing, and branch-bearing encoded
  functions, so later consumers can stay on the report artifact instead of
  rescanning every summary manually.
  That report surface now also has first lookup helpers by emitted label and
  by per-function abstract offset, plus a direct report-side terminator-target
  query for one encoded block. Program/report lookup can now also resolve one
  encoded block directly from `function_name + abstract_offset`, so later
  consumers do not have to hand-stage that lookup themselves. Plain `branch`
  and `compare-branch` families now also dump resolved target labels/offsets
  explicitly on the encode side instead of falling back to generic `term=N`,
  and the current
  `make test-machine-encode`, `make test`, and `git diff --check` checkpoint
  stays green with those helpers in place. The current first machine-encode
  slice should now be read as checkpoint-ready rather than merely
  checkpoint-near. This is
  intentionally still encoding-prep rather than real byte encoding: default next work should
  stay bug-driven or downstream-consumer-driven unless a later true encoding
  mainline is explicitly chosen.
- Machine ELF: the first post-container ELF sibling is now no longer limited
  to one invisible hardcoded header policy. Current authority still keeps the
  repository conservative by defaulting public builds to a generic ELF32
  profile, but the `machine_elf` boundary now also has an explicit target-
  profile concept that owns `e_machine`, `e_flags`, and relocation-type
  mapping policy. That same surface is now exercised by a focused
  `riscv32-preview` profile and now also by an `i386-preview` profile, with
  verifier/header/dump/query coverage over both profile-selected header fields
  and current call/control relocation opcode mapping. On top of that,
  `machine_elf` now also has a first parse /
  round-trip surface: one generated ELF byte image can be parsed back into a
  fresh `MachineElfFile`, have its section/symbol/relocation/profile metadata
  reconstructed, and then be re-verified through the same public verifier.
  That parse side is now also a little more consumer-facing and a little less
  permissive than the first cut: direct `bytes -> dump` convenience exists,
  the verifier now treats the repository's current canonical section
  family/link semantics as part of the parse contract rather than only
  checking generic byte-table consistency, and one parsed/edited
  `MachineElfFile` can now also be refreshed back into canonical ELF bytes
  without having to rebuild the whole artifact through `machine_container`
  again first. The importer is now also one notch less coupled to one exact
  emitted section-header order: it can accept the repository's current ELF
  subset with reordered section headers when the section-link/name semantics
  remain self-consistent, then normalize that input back into the repository's
  canonical internal section order and rebuilt byte image. It is also now one
  notch less coupled to “exactly the canonical 6 sections”: extra non-core
  sections can be tolerated on import as long as the required core
  `.text/.strtab/.symtab/.rel.text/.shstrtab` family remains self-consistent.
  The importer is now also one notch less coupled to “every canonical section
  must already exist in the input”: `.rel.text` may be absent on import and
  will be synthesized back as an empty canonical section during normalization.
  Symbol-table import is now also one notch less coupled to the repository's
  exact local/global ordering: a self-consistent non-canonical `symtab`
  ordering can now be accepted and normalized back into the repository's
  canonical local-then-global layout with relocation symbol indices remapped
  accordingly.
  That import path is now also explicitly usable as a canonicalizer: accepted
  byte images can be normalized directly back into the repository's canonical
  ELF byte image without routing through `machine_container`, and that same
  canonicalization path can now also re-emit directly under a requested
  preview target profile rather than only preserving the imported profile.
  On top of that, imported/normalized artifacts now also have a first direct
  clone surface, and raw bytes can now also be lifted directly into a
  canonicalized `MachineElfFile`, so local edit experiments do not need to
  consume the only parsed copy in place or hand-stage parse/refresh glue. On
  top of that, the line now also has a first structured report artifact above
  the raw ELF file, so later consumers can stay on cached
  header/section/symbol/relocation summaries instead of rebuilding that scan
  every time.
  The active mainline is therefore no longer “does an ELF skeleton exist at
  all?” but “how much more target-policy realism and parse/import hardening
  should be added on top of the now-explicit ELF boundary before switching to
  a new sibling?”
- Machine Bytes: a first sibling byte-bearing post-encode layer is now landed
  with separate `include/machine/bytes.h`, `src/machine/object/machine_bytes/`, and
  `tests/machine/object/machine_bytes/` surfaces. The current slice has a real verifier,
  dump, summary surface, direct lowering from `machine_encode`, and upstream
  bridges from both canonicalized `machine_ir` and report-oriented
  `MachineEncodeReport` / `MachineIrAllocateRewriteReport` artifacts. Current
  lowering still stays target-agnostic, but it is now no longer only
  `tag + filler`: immediate families, call arg counts/arg kinds, branch target
  pairs, compare ops, and `branch_on_true` state now surface into the first
  payload bytes directly. So this stage has moved further from “offset counts
  only” toward “real byte-bearing structure exists as data” without yet
  claiming final ISA encoding. The first report surface now also carries
  per-function byte summaries, per-block byte summaries, and high-level
  function index sets for call/fallthrough/branch families, and those
  function summaries now count real op-byte / terminator-byte totals rather
  than only raw item counts. So downstream consumers can stay on
  one byte-bearing artifact instead of rescanning summaries by hand. That raw
  byte-bearing surface now also reaches direct byte-image consumers more
  cleanly than before: one block's bytes can be viewed directly, one
  function/program/report can be copied into a flat byte image, and whole-
  program byte-offset lookup now exists on the program side too rather than
  only on the report artifact. It now also has direct program/report
  convenience helpers for total byte count, absolute per-function byte spans
  on the raw program side, and per-function/per-block byte copying without
  forcing later consumers to restage those slices manually. The report side
  now also has a first structured reference-summary surface for call sites and
  control-flow target sites, including owner offsets, patch offsets, and
  resolved target labels/offsets, and it now also lifts that into a first
  explicit fixup-summary artifact with target kind plus owner/patch span
  metadata. On top of that, the same report layer now also exposes a first
  minimal symbol summary surface for defined function/block symbols plus
  unresolved external call symbols, and fixups can resolve directly to those
  symbol indices. It now also exposes one minimal `.text` section summary over
  the byte image, including section span plus function/block/symbol/fixup
  counts. Section-oriented consumers can now also read the symbol slice and
  fixup slice belonging to `.text` directly, and the report dump prints that
  section/symbol/fixup snapshot in one place. So later relocation/fixup consumers no longer need to
  reconstruct those sites from dump text or reinterpret raw byte/reference
  scans by hand. That
  bridge line now also reaches raw `machine_emit` and `MachineEmitLowerReport`
  artifacts directly, so byte-bearing consumers can continue from emitted
  artifacts without restaging a manual encode step first. This is
  intentionally still pre-relocation and pre-object-file rather than full
  target-final encoding: default next work should now focus on deepening this
  byte-bearing line or choosing the next relocation/object-file mainline.
- Machine Object: a first sibling object-facing post-bytes layer is now
  landed with separate `include/machine/object.h`, `src/machine/object/machine_object/`, and
  `tests/machine/object/machine_object/` surfaces. The current slice has a real verifier,
  dump, direct lowering from both `MachineBytesReport` and `MachineBytesProgram`,
  and a first upstream bridge from `MachineIrAllocateRewriteReport` through the
  current byte-bearing pipeline. Current lowering intentionally stays
  conservative: it preserves one `.text` section, section-local bytes, defined
  function/block symbols, unresolved external call symbols, and call/control
  fixups as first-class object-facing containers rather than claiming final
  relocation encoding or full object-file serialization. The object-facing
  query layer now also exposes section lookup by name, section-byte copying,
  section-local symbol/fixup slices, symbol lookup by name, direct fixup lookup,
  and summary helpers over sections/symbols/fixups. This should now be treated
  as a checkpoint-ready sibling rather than the continuing default mainline:
  the first object-facing artifact is real enough to advance under test, while
  later target-specific relocation encoding or final file serialization should
  remain explicitly future work. The current `make test-machine-object`,
  `make test`, and `git diff --check` checkpoint is now green for this first
  object-facing slice.
- Machine Reloc: a first sibling relocation-facing post-object layer is now
  landed with separate `include/machine/reloc.h`, `src/machine/object/machine_reloc/`, and
  `tests/machine/object/machine_reloc/` surfaces. The current slice has a real verifier,
  dump, direct lowering from `MachineObjectFile`, and a first upstream bridge
  from `MachineIrAllocateRewriteReport` through the current object pipeline.
  Current lowering intentionally stays conservative: it preserves the embedded
  object artifact, surfaces one relocation table per object section, and lifts
  current call/control fixups into explicit relocation records with patch-span
  and target-symbol metadata rather than claiming target-final relocation
  opcode encoding or final file serialization. The relocation-facing query
  layer now also exposes whole-file summaries, relocation-section lookup by
  name, section-local relocation slices, direct relocation lookup, and summary
  helpers over relocation sections/records. This should now be treated as a
  checkpoint-ready sibling rather than the continuing default mainline: the
  first relocation artifact is real enough to advance under test, while final
  object-file serialization should remain the next explicit downstream choice.
  The current `make test-machine-reloc`, `make test`, and `git diff --check`
  checkpoint is now green for this first relocation-facing slice.
- Machine Container: a first sibling final-serialization post-reloc layer is
  now landed with separate `include/machine/container.h`,
  `src/machine/object/machine_container/`, and `tests/machine/object/machine_container/` surfaces. The
  current slice has a real verifier, dump, direct serialization from
  `MachineRelocFile`, and a first upstream bridge from
  `MachineIrAllocateRewriteReport` through the current relocation pipeline.
  Current serialization intentionally stays format-agnostic: it preserves the
  embedded relocation artifact, emits a stable custom byte container with
  header/section-table/symbol-table/relocation-table/string-table/payload
  regions, and exposes layout/query helpers over that container rather than
  claiming ELF/COFF/Mach-O compatibility. This should now be treated as a
  checkpoint-ready sibling rather than the continuing default mainline: the
  first final serialized artifact is real enough to advance under test, while
  target-specific object format compatibility remains the next explicit
  downstream choice. The current `make test-machine-container`, `make test`,
  and `git diff --check` checkpoint is now green for this first
  container-facing slice.
- Machine ELF: a first sibling target-specific post-container layer is now
  landed with separate `include/machine/elf.h`, `src/machine/object/machine_elf/`, and
  `tests/machine/object/machine_elf/` surfaces. The current slice has a real verifier, dump,
  direct ELF serialization from `MachineContainerFile`, and a first upstream
  bridge from `MachineIrAllocateRewriteReport` through the current container
  pipeline. Current serialization intentionally stays conservative but real:
  it emits one ELF32 little-endian relocatable skeleton with section headers,
  `.text`, `.strtab`, `.symtab`, `.rel.text`, and `.shstrtab`, while still
  using the repository's current placeholder relocation-type mapping instead
  of claiming full platform-ABI relocation semantics. This should now be
  treated as the active backend mainline: the first target-specific object
  format artifact is real enough to advance under test, while stronger ABI
  compatibility or sibling object formats remain explicit downstream choices.
  The current `make test-machine-elf`, `make test`, and `git diff --check`
  checkpoint is now green for this first ELF-facing slice.
- Machine Layout: a first sibling linear-layout layer is now landed with
  separate `include/machine/layout.h`, `src/machine/lowering/machine_layout/`, and
  `tests/machine/lowering/machine_layout/` surfaces. The current slice has a real verifier,
  dump, summary surface, direct lowering from `machine_select`, and an
  upstream bridge from canonicalized `machine_ir` through `machine_select`.
  Current lowering already preserves selected ops inside layout blocks and
  lowers `jump` / `br` / `cmpbr` families into `fallthrough`-aware terminator
  forms. It now also has a first real block-ordering policy rather than
  preserving source order blindly: preferred successors may be laid out first
  so direct branch/jump fallthroughs are created by layout itself. That
  policy is now also slightly more selective than raw DFS: on two-way control
  it may prefer a single-predecessor, chain-extending successor as the
  fallthrough path instead of following a fixed successor order, and it now
  also has a first trace-span tie-break when both sides are locally plausible.
  That local branch scoring now also discounts successors that only jump
  straight into a shared tail that the current local-growth policy would defer
  anyway, instead of counting that deferred tail as if it were an immediate
  local trace extension.
  It now also has a first explicit shared-merge deferral rule, so a branch arm
  may stop before sinking into a multi-predecessor merge tail and let that
  shared tail be laid out after the branch arms instead. Current authority is
  still intentionally conservative there: preserving local branch-arm trace
  shape currently wins over a more aggressive "merge became ready, stitch it
  immediately" policy, and the current local-trace rule now enforces that more
  directly by keeping such shared tails deferred until the later seed phase.
  Above that local rule, seed selection now also has one small function-level
  stitching preference: a deferred shared tail that is already fully ready may
  be picked ahead of an unrelated longer-but-not-ready deferred trace. That
  seed policy is now also explicit in shape rather than only being implied by
  numeric comparator order: ready shared tails, ready continuations,
  attached-unready work, and detached traces are now distinct classes before
  later tie-breaks apply, and that predecessor-frontier freshness tie-break
  now applies across the non-detached seed side broadly rather than only to
  ready shared merges. In other words, current function-level stitching may
  continue to read as mostly branch/compare-symmetric at this point:
  direct-side compare-family authority now explicitly covers not only the
  earlier defer/seed/continuation families, but also the most local
  branch-reorder preference and the "multiple ready shared merges compete"
  freshness family. It now also covers several older local branch-shaping
  heuristics that used to be branch-only: the single-predecessor fallthrough
  preference, the local trace-span tie-break, and the
  "do not count an immediately deferred shared tail as ordinary local trace"
  scoring rule. It now also covers one seed-class ordering boundary that used
  to be locked only through later bridge work: `ready continuation >
  attached-unready`.
  In other words, current function-level stitching may
  continue the freshest already-attached region whether that next seed is a
  ready shared tail or an ordinary ready continuation, before falling back to
  broader visited-count, trace, or id tie-breaks. The current branch and seed
  heuristics now also consume one shared function-level trace-span view rather
  than each recomputing their own local estimate ad hoc. Bridge-side
  authority is also a bit stronger now on canonicalized shared-tail
  micro-shapes with real effectful wrapper ops: current tests explicitly lock
  the layout that survives `machine_ir` cleanup, instead of assuming those
  bridge cases must preserve the same block decomposition as direct
  `machine_select` input. That bridge-side explicitness now also covers the
  compare-branch sibling of the same family, including canonicalized cases
  where upstream cleanup legally sinks a shared return tail into each
  effectful arm before `machine_layout` sees the CFG. In other words, bridge
  authority is now starting to lock both:
  the multi-block effectful shared-tail cases that still survive as layout
  problems, and the more aggressively canonicalized compare/shared-tail cases
  that arrive at layout only after the old shared return has already been
  duplicated into each arm. Bridge coverage is now also beginning to reach the
  seed-selection layer itself, not only local branch-lowering shape: one
  canonicalized effectful CFG family now locks the "multiple ready shared
  merges compete" behavior on the bridge too. That bridge-side seed/stitching
  authority now also has a compare-family sibling, so the current ready-shared
  merge freshness policy is no longer bridge-locked only for plain branch
  families. Put differently: the bridge side now has both branch-driven and
  compare-driven canonicalized CFG families covering the current shared-tail
  freshness/seed-ordering story, not only the earlier local fallthrough
  families. That compare-side bridge coverage now also reaches the
  ready-continuation sibling of the same stitching family, so compare-driven
  bridge authority is no longer limited to shared-merge cases only. Bridge
  authority now also reaches the branch-local scoring layer on both major
  sibling families, so canonicalized branch/shared-tail and
  compare/shared-tail bridge coverage now lock not just seed ordering but
  also the "do not overcount an immediately deferred shared tail as local
  trace" preference. Bridge coverage now also reaches the older seed-priority rule
  itself, not only freshness among ready candidates: one canonicalized
  effectful CFG family now locks that a ready shared merge may still beat a
  longer-but-not-ready deferred trace on the bridge too, and that same rule
  now also has a canonicalized compare-family bridge sibling. Bridge
  authority now also explicitly reaches the older local trace-span tie-break:
  canonicalized branch-driven and compare-driven effectful CFG families both
  lock that tied local successor scores may still defer to the longer
  surviving post-cleanup trace. Bridge authority now also explicitly reaches
  the seed-class boundary between ready continuations and attached-but-unready
  work: canonicalized branch-driven and compare-driven effectful CFG families
  both lock that a ready continuation may still outrank an older attached
  deferred trace. Bridge authority now also explicitly reaches the older
  single-predecessor / chain-extending successor preference itself:
  canonicalized branch-driven and compare-driven effectful CFG families both
  lock that layout may still choose the successor whose post-cleanup
  surviving block continues into a single-predecessor chain. Bridge authority
  now also explicitly reaches the most local branch-reorder preference
  itself: canonicalized branch-driven and compare-driven CFG families both
  lock that layout may still reorder the immediate branch targets to make the
  preferred successor the direct fallthrough block. This is now the active
  backend consumer line rather than only a plan document, but it should also
  be read as checkpoint-near: default next work should be bug-driven or
  downstream-consumer-driven rather than "add more layout heuristics by
  default" unless a clearly missing family is found.

## Canonical IR Maintenance Stance

- Keep canonical IR stable as the front-end output surface.
- Keep existing canonical-IR passes verifier-safe and regression-locked.
- Prefer bug fixes, correctness hardening, and contract cleanup over new speculative transforms.
- Do not force SSA directly onto canonical IR.
- Do not begin register allocation or machine-oriented backend modeling at this stage.

## Lower IR Direction

Current recommended direction:

- introduce a downstream lower-memory IR rather than mutating canonical IR in place
- keep canonical block/basic-block/terminator CFG shape in the first lower layer
- keep phase 1 explicit and small
- prefer slot-specific memory operations first:
  - `load_local`
  - `store_local`
  - `load_global`
  - `store_global`
- make lower-IR value instructions temp/immediate-only and keep locals/globals as slot-only entities
- defer generic address-taking, SSA, and register allocation until the lower-memory layer proves its value

For detailed rationale and examples, read `docs/ir/LOWER_IR_DESIGN.md`.

## Near-Term Plan

1. Maintain canonical IR and its pass layer in bug-driven mode.
2. Keep the phase 1 lower-IR skeleton stable behind a separate module boundary:
   - `include/lower_ir.h`
   - `src/lower_ir/`
   - `tests/lower_ir/`
3. Phase 1 status:
   complete enough for manual construction, dump, verifier, and isolated tests.
4. Phase 2 lower-IR checkpoint:
   keep serial lowering from canonical IR stable while preserving CFG shape and the value/slot split explicit.
5. Current phase 2 prototype coverage:
   - slot-valued `ret` lowered via explicit `load_*`
   - slot-valued `br` conditions lowered via explicit `load_*`
   - slot-valued binary operands lowered via explicit `load_*`
   - canonical `mov` to local/global lowered as `store_*`
   - slot-valued call arguments lowered via explicit `load_*`
   - canonical global initializer metadata copied into lower IR
   - lower-IR verifier now enforces the reserved startup/helper contract for runtime global initialization (`__global.init` / `__program.init`)
   - lower-IR verifier now enforces canonical-style public call/name contracts too: known callees only, matching arg counts, duplicate-name rejection, and function/global collision rejection
   - lower-IR verifier now also rejects malformed metadata tables explicitly instead of trusting non-NULL global/function/local/block backing arrays when the corresponding counts are nonzero
   - lower-IR verifier now also rejects count/capacity metadata mismatches explicitly, so malformed programs cannot walk past global/function/local/block/instruction table bounds before reporting an error
   - lower-IR verifier now also rejects unreachable body blocks, bringing CFG reachability into the lower-IR public contract instead of only checking local target validity
   - lower-IR builder/API now fail fast on several common malformed construction paths: declaration-only functions cannot append blocks or allocate temps, parameter locals must remain a prefix, and blocks reject both post-terminator instruction appends and terminator overwrite attempts
   - `lower_ir_lower_from_ir(...)` now self-verifies the produced lower IR before returning success, so lowering regressions trip at the production boundary instead of relying on all callers to run a separate verifier pass
   - lower-IR verifier now also checks temp definition/availability contracts: temps must be defined somewhere, same-block duplicate temp definitions are rejected, and temp uses must be available on all incoming paths while still allowing the current mutually-exclusive join-temp lowering pattern
   - lower-IR temp-definition checks now only honor live `has_result` temp outputs, so result-less store instructions cannot forge fake temp definitions through hidden payload fields
   - lower-IR verifier now also enforces a minimal declaration/signature shape contract: `parameter_count` must match the parameter-prefix locals, and ordinary declaration-only functions cannot carry extra non-parameter locals or temp state
   - exact dump regressions now lock runtime-global startup flow and no-`main` `__program.init` fallback in lowered output
   - exact dump regressions now lock representative control-flow-heavy lowering too: `if` joins, short-circuit branch conditions, and `while` backedges
   - exact dump regressions now also cover `while`-break exits, `for` init/step CFG, nested loop break/continue, and branch-local global writeback with join-block reload
   - exact dump regressions now cover call-heavy CFG shapes too: declared-call `for` condition/step lowering and runtime-global startup interaction with branchy `main`
   - exact dump regressions now cover representative value-materialization shapes too: local `&&` values, ternary value expressions, declared-call logical values, and nested logical-mix value joins
6. Lower-IR current maintenance stance:
   - keep the current builder/API and verifier contracts stable
   - treat the present regression matrix as sufficient authority for the current phase
   - reopen this slice only for concrete bugs, new lowering features, or a concrete downstream consumer need
7. The next active design/execution target is now value-SSA phase S2 on top of lower IR; follow `docs/ssa/VALUE_SSA_DESIGN.md`, not ad hoc SSA experiments directly on lower IR.
8. Near-term value-SSA implementation goal:
   - keep the strict conversion boundary stable:
      - `include/value_ssa.h`
      - `src/value_ssa/`
      - `include/value_ssa_pass.h`
      - `src/value_ssa_pass/`
      - `tests/value_ssa/`
   - keep new SSA-native optimization/cleanup passes grouped under sibling module `src/value_ssa_pass/` so core representation/analysis/conversion files do not turn into another mixed grab-bag
   - keep pass aggregation in `src/value_ssa_pass/value_ssa_pass.c` rather than folding those transforms back into `value_ssa.c`
   - treat `lower_ir` as the stable construction input and `value_ssa` as the first downstream optimization authority
   - the next code-facing work should now move above conversion:
     - shared SSA def/use and use-list facts
     - first narrow SSA-side simplification/optimization passes
     - keep these value-only and do not drift into memory-SSA work yet
   - do not reopen broad lower-IR hardening or unrelated infrastructure cleanup unless it blocks SSA-side analysis/pass work directly
9. The current stage question is no longer "can lower_ir convert into value_ssa correctly?" but "what is the smallest stable shared SSA analysis surface the first real SSA passes should consume?"

## Current Stop Condition

- Treat the current lower-IR Phase 2 loop as checkpoint-complete once `make test-lower-ir-regression`, `make test-lower-ir-verifier`, and `make test` stay green with the present representative matrix.
- Do not continue adding lower-IR regression families by default once the matrix covers:
  - straight-line slot/value lowering
  - control-flow slot materialization
  - loop/backedge and heavier CFG shapes
  - call-heavy CFG shapes
  - runtime-global startup/helper interaction
  - value-materialization joins
- Reopen active expansion only for bug-driven work, a newly introduced lowering feature, or a clearly missing CFG/value family that the current matrix does not represent.
- Reopen builder/API or verifier hardening only for confirmed malformed-program holes, not by default.
- Defer earlier high-complexity infrastructure cleanups (for example linear-scan scope internals) unless they become a demonstrated blocker for value-SSA work or a confirmed performance/maintenance hotspot.

## Guardrails

- Every functional change should come with regression coverage.
- Keep `make test` green at each step.
- Pass-stage work must preserve canonical-IR contracts: verifier must remain green before and after passes.
- Lower-IR work must not turn canonical IR into a mixed transitional representation.
- Keep stable internal numeric IDs for blocks, locals, temps, and later lower-IR entities; human-readable names belong in dumps, not identity.
- Do not introduce lower IR as a second front-end authority surface.
- Do not treat `docs/ir/LOWER_IR_DESIGN.md` as implementation permission by itself; roadmap and user intent still matter.

## Current Known Limitations

- Front-end control-flow analysis remains a structured approximation, not a full symbolic path solver.
- Canonical IR is intentionally non-SSA and still mixes storage-like entities (`local/global`) with value-like use sites.
- Current canonical-IR passes are designed for canonical IR only; they are not automatically reusable on a future lower IR.
- Lower IR is implemented as an early prototype stage with skeleton, verifier, dump, and minimal serial lowering coverage, but it is not yet a backend-facing or SSA-ready layer.
- Value SSA now has a separate phase S1 skeleton plus a first phase-S2 conversion slice from lower IR, but that slice is intentionally narrow: straight-line CFGs, diamonds, simple slot-carried loops, and a representative loop-header phi case are supported; broader cyclic CFG families are still deferred. Current simplification support is also intentionally narrow: it rewrites use sites through trivial `mov` / trivial `phi` resolutions, but it does not yet do full dead-SSA cleanup or renumbering.
- Some earlier front-end / semantic internals still use straightforward rather than highly optimized data structures; treat those as deferred cleanup candidates, not the current active workstream.
- SSA, register allocation, and machine-oriented code generation are all out of scope for the current active implementation phase.

## Recent Checkpoints

- 2026-04-05: Canonical IR pass pipeline was stabilized through F0-F3, including soundness fixes, verifier coupling, fixed-point guards, and diagnostic cleanup.
- 2026-04-05: Canonical-IR pass work was soft-closed into maintenance-first mode unless bug-driven or need-driven.
- 2026-04-05: Lower-IR exploration was reframed as design-first work with serial-lowering constraints.
- 2026-04-05: The first lower-IR draft now recommends a small explicit-memory stage before SSA or backend-oriented work.
- 2026-04-05: Lower-IR phase 0 was frozen around slot-specific memory ops, temp/immediate-only value instructions, canonical-CFG reuse, and separation between value refs and slot refs.
- 2026-04-05: Lower-IR phase 1 started with a separate `include/lower_ir.h`, `src/lower_ir/`, and `tests/lower_ir/` skeleton for manual construction, dump, verifier, and isolated tests.
- 2026-04-05: Lower-IR phase 2 started with a minimal `lower_ir_lower_from_ir(...)` path that preserves canonical CFG shape while rewriting slot-valued operands into explicit `load_* / store_*` operations for representative return, assignment, branch-condition, call-argument, and global-read/write shapes.
- 2026-04-05: Lower-IR verifier now mirrors canonical runtime-global startup/helper rules: reserved helper names require runtime globals, `__global.init` / `__program.init` must keep the fixed helper shape, and startup calls to `__global.init` are only legal from `main` entry or `__program.init` entry; dedicated lower-IR verifier regressions now lock the malformed-helper and missing-startup-call cases.
- 2026-04-05: Lower-IR verifier now also rejects malformed public call/name surfaces that canonical IR already rejects: unknown callees, call arg-count mismatches, duplicate function names, duplicate global names, and function/global symbol collisions all have dedicated lower-IR verifier coverage.
- 2026-04-05: Lower-IR verifier is now also crash-proof against malformed metadata tables: nonzero global/function/local/block counts with NULL backing arrays are rejected explicitly, and dedicated verifier regressions lock those entry points as contract failures rather than crash surfaces.
- 2026-04-05: Lower-IR verifier now also rejects count/capacity metadata mismatches (`global_count`, `function_count`, `local_count`, `block_count`, and `instruction_count` exceeding their capacities) before any table walk, closing another malformed-program crash/OOB surface and adding dedicated verifier regressions for each layer.
- 2026-04-05: Lower-IR verifier now also rejects unreachable basic blocks, aligning lower-IR CFG contract expectations with canonical IR and adding a dedicated unreachable-block verifier regression.
- 2026-04-05: Lower-IR builder/API was tightened to fail fast on common malformed construction shapes (declaration-only block/temp creation, parameter-prefix breakage, post-terminator instruction append, and terminator overwrite), with dedicated lower-IR regression coverage for each case.
- 2026-04-05: `lower_ir_lower_from_ir(...)` now self-verifies its output before returning success, turning lower-IR verifier into an internal production guard as well as an external API contract.
- 2026-04-05: Lower-IR verifier now also rejects malformed temp flow, not just temp-range misuse: missing temp definitions, same-block duplicate temp definitions, and use-before-availability now fail verification, while a dedicated positive verifier case keeps the current valid join-temp multi-definition lowering shape accepted.
- 2026-04-05: Lower-IR verifier no longer lets result-less `store_*` instructions forge fake temp definitions through hidden `result` payloads, and dedicated verifier regressions now lock that hole closed.
- 2026-04-05: Lower-IR verifier now also enforces a minimal declaration/signature shape contract: parameter-prefix locals must agree with `parameter_count`, and ordinary declaration-only functions cannot hide extra non-parameter locals or temp state.
- 2026-04-05: Lower-IR runtime-init coverage now also includes exact dump regressions for the normal startup path and no-`main` `__program.init` fallback, locking that helper bodies use explicit `store_global`/`load_global` forms rather than only relying on verifier-side contract checks.
- 2026-04-05: Lower-IR exact-dump regressions now also cover more control-flow-heavy slot/value cases, including single-arm and two-arm `if` joins, short-circuit branch conditions, and `while` backedges, locking that explicit `load_* / store_*` materialization composes cleanly with canonical block topology and preserved canonical temps.
- 2026-04-05: Lower-IR exact-dump coverage now extends through heavier CFG shapes as well, including `while`-break exits, `for` init/step loops, nested loop break/continue control, and branch-local global writeback followed by join-block reload; these cases now serve as authority that serial lowering composes explicit slot materialization with preserved canonical block IDs and canonical temps across larger CFGs.
- 2026-04-05: Lower-IR exact-dump coverage now also reaches call-heavy CFG shapes, including declared-call `for` conditions/steps and a branchy `main` that still begins with runtime-global startup helper invocation; these locks also establish the current temp-numbering authority when canonical user-expression temps and injected startup calls coexist.
- 2026-04-05: Lower-IR exact-dump coverage now also includes representative value-materialization joins (`&&`, ternary value, declared-call logical values, nested logical mixes). With straight-line, CFG-heavy, call-heavy, runtime-init, and value-materialization families all represented, the current Phase 2 loop is considered checkpoint-complete and should now return to need-driven expansion.
- 2026-04-07: With lower IR checkpointed, the next planned design target became value-SSA on top of lower IR; a first design draft now captures the boundary, non-goals, proposed representation, and first-slice plan.
- 2026-04-07: Value-SSA phase S1 is now landed as a separate `include/value_ssa.h`, `src/value_ssa/`, and `tests/value_ssa/` skeleton with manual builder helpers, verifier, dump, and isolated regression/verifier tests. The next likely step is a very small phase-S2 conversion slice from lower IR.
- 2026-04-07: Value-SSA phase S2 has now started with a first `lower_ir -> value_ssa` conversion entrypoint. The current supported slice is intentionally narrow: straight-line lower-IR shapes, single-join diamonds, simple slot-carried loops, and a representative loop-carried temp join now convert into explicit SSA values plus phi nodes where needed. Broader cyclic CFG families remain a separate follow-up slice.
- 2026-04-07: Value-SSA now also has a separate trivial-values simplify pass. The current contract is deliberately modest: it resolves use sites through trivial `mov` chains and same-value `phi` nodes, but it does not yet try to perform full dead-definition cleanup or SSA renumbering.
- 2026-04-07: Value-SSA now also has a separate narrow CFG simplify pass. The current contract is still intentionally modest: it only folds same-target branches and immediate-condition branches, then compacts any newly unreachable blocks plus surviving SSA value ids so verifier legality remains preserved after the CFG rewrite.
- 2026-04-07: Value-SSA now also has a separate narrow dead-def cleanup pass. The current contract is intentionally modest: it only removes unused pure value definitions (`phi`, `mov`, `binary`, `load_*`) and then compacts surviving SSA value ids; it must continue to preserve effectful instructions such as dead-result `call` and all `store_*`.
- 2026-04-07: Value-SSA now also has a first shared CFG analysis groundwork slice. `value_ssa_compute_cfg_analysis(...)` computes predecessor counts, reachability, the full dominator relation, immediate dominators, explicit dominator-tree children, and dominance frontiers for verifier-legal SSA functions, `value_ssa_compute_phi_placement(...)` exposes iterated phi-placement closure from a definition-block set, and `value_ssa_compute_dominator_tree_preorder(...)` exposes a direct dominator-tree traversal order. Representative regression authority is locked on both a diamond and a loop-header-phi shape so later rename/dominator consumers do not have to rebuild these facts ad hoc.
- 2026-04-07: Value-SSA rename groundwork now also has a split dominator-tree walk scaffold. `value_ssa_walk_dominator_tree(...)` exposes explicit enter/leave callbacks over the current dominator tree so later stack-based rename state can sit on a reusable traversal helper instead of re-embedding tree walk logic into each rename experiment.
- 2026-04-07: Value-SSA rename groundwork now also has a first scoped rename-state stack. The new state helper supports block-nested scope begin/end, binding shadowing, and rollback, and current regressions already lock both direct scope semantics and dominator-tree-walk integration on a diamond so later rename work can focus on actual renaming rules instead of rebuilding scope bookkeeping.
- 2026-04-07: Value-SSA rename groundwork now also has a first block-local use-site rewrite helper. Given the current rename-state bindings, `value_ssa_rename_rewrite_block_uses(...)` rewrites instruction/terminator uses without touching defs or phi-input placement, and current regressions lock both value-operand and branch-condition rewriting.
- 2026-04-07: Value-SSA rename groundwork now also has a first predecessor-specific phi-input rewrite helper. Given the current rename-state bindings for one predecessor path, `value_ssa_rename_rewrite_phi_inputs_for_predecessor(...)` rewrites only that predecessor's phi operands in a successor block; current regressions lock this separately from block-local use rewriting.
- 2026-04-07: Value-SSA now also has a first real function-level alpha-renaming orchestration. `value_ssa_rename_function_values(...)` composes dominator-tree walk, scoped bindings, block-local use rewriting, local def renaming, and successor-edge phi-input rewriting into one verifier-safe pass; current authority is locked on a deliberately scrambled diamond that canonicalizes back to dominator-order SSA ids.
- 2026-04-07: Value-SSA now also has a first small program-level canonicalization entrypoint. `value_ssa_canonicalize_program(...)` composes trivial value cleanup, trivial CFG cleanup, dead pure-def cleanup, and alpha-renaming into one result-layer stabilization pass; current authority is locked on a scrambled diamond with an extra trivial copy that now canonicalizes to a stable compact dump in one call.
- 2026-04-07: Value-SSA conversion and result-layer stabilization are now also bridged by a one-shot entrypoint. `value_ssa_build_canonicalized_from_lower_ir(...)` runs `lower_ir -> value_ssa` conversion and then the current canonicalization pipeline, so downstream callers can request a directly stabilized SSA dump without manually composing conversion and cleanup.
- 2026-04-07: Value-SSA canonicalization now also has an explicit fixed-point regression: the scrambled-diamond authority case must produce the same dump on the first and second `value_ssa_canonicalize_program(...)` run, with verifier success after both runs.
- 2026-04-07: `value_ssa_canonicalize_program(...)` now self-verifies both its input and its output. That turns the result-layer canonicalization entrypoint into a stronger public boundary: malformed SSA is rejected before cleanup starts, and successful canonicalization guarantees verifier-legal output without requiring every caller to remember an extra explicit verify step.
- 2026-04-07: Value-SSA now also classifies effectful vs removable dead definitions through a shared internal helper instead of a DCE-local switch. Current authority also explicitly locks that full-program canonicalization must preserve dead-result `call` instructions rather than optimizing them away while cleaning up surrounding pure SSA values.
- 2026-04-07: Value-SSA bridge authority now also explicitly locks dead-result `call` preservation through the full `lower_ir -> value_ssa -> canonicalize` path, not only through direct SSA-level canonicalization. A dedicated lower-IR dead-call case now ensures the one-shot bridge still returns `ssa.0 = call id(); ret 0` instead of incorrectly cleaning the call away.
- 2026-04-07: Value-SSA dead-def classification is now also sound for dangerous binary ops. Dead-result `div`/`mod`/shift instructions are no longer treated as removable pure defs, and direct DCE, full SSA canonicalization, and the one-shot lower-IR bridge now all have dedicated regressions locking that `div 1, 0; ret 0`-style shapes are preserved rather than optimized away.
- 2026-04-07: Current Value-SSA dangerous-binary handling is intentionally conservative: dead-result `div`/`mod`/shift ops are all preserved, including obviously safe constant cases such as `div 1, 1`. If future cleanup wants to recover those, it should do so by introducing a narrower value-sensitive dangerous-binary predicate rather than reverting to "all binary ops are removable".
- 2026-04-07: The preferred one-shot Value-SSA bridge is now also treated as a stabilized-output boundary, not just a convenience wrapper. `value_ssa_build_canonicalized_from_lower_ir(...)` now performs an extra canonicalization convergence step internally, and a dedicated bridge fixed-point regression locks that the representative diamond authority case is already unchanged by one more caller-side canonicalization run.
- 2026-04-08: The current Value-SSA result-layer hardening loop is now treated as reviewer-cleared for forward progress. Recent reviewer-reported contract and safety gaps have been fixed, so work no longer needs to pause for more result-layer-only review before continuing along the planned value-SSA path.
- 2026-04-08: Lower-IR now also has a first dedicated analysis slice for the true conversion input layer. `lower_ir_compute_cfg_analysis(...)`, `lower_ir_collect_temp_definition_blocks(...)`, and `lower_ir_compute_phi_placement(...)` are landed with direct regression authority on both a diamond join-temp shape and a loop-header join-temp shape, so future `lower_ir -> analysis -> conversion -> value_ssa` work can stand on lower-IR facts instead of SSA-side prototypes alone.
- 2026-04-08: Value-SSA conversion has now started consuming that lower-IR-side analysis directly. The current `value_ssa_build_from_lower_ir(...)` path still preserves the existing representative conversion dumps, but predecessor facts and phi-candidate placement are now sourced from `lower_ir_compute_cfg_analysis(...)` plus `lower_ir_compute_phi_placement(...)` rather than only from ad hoc SSA-side reconstruction.
- 2026-04-08: Value-SSA conversion now also consumes lower-IR dominator-tree traversal directly while processing blocks. Representative bridge outputs still stay unchanged, but the conversion path has stopped relying on an internal topo-order/CFG-index fallback once lower-IR analysis is available.
- 2026-04-08: Value-SSA conversion coverage now also includes a representative multi-backedge loop-carried temp shape. The current bridge can merge an entry seed plus multiple distinct backedge defs into one loop-header phi without leaving the existing raw bridge authority family.
- 2026-04-08: Lower-IR CFG/phi-placement authority now also covers the same multi-backedge loop-header join-temp family. Predecessor counts, immediate dominators, dominance frontiers, phi placement, dominator preorder, and dominator walk are all regression-locked on that input shape.
- 2026-04-08: Lower-IR analysis now also exposes a direct batch temp-to-phi-candidate API. `lower_ir_compute_temp_phi_candidates(...)` is regression-locked on representative diamond/loop families, and `value_ssa_build_from_lower_ir(...)` now consumes that input-layer surface instead of precomputing the same phi-candidate matrix through an SSA-local helper.
- 2026-04-08: Lower-IR analysis now also exposes a compact predecessor-list surface, and `value_ssa` conversion/phi-finalization now consume that list directly instead of rescanning the full predecessor matrix at each join. This does not remove predecessor-state dependence yet, but it moves another CFG-detail responsibility off the SSA-local conversion code and into the input layer.
- 2026-04-08: Lower-IR analysis now also exposes a compact successor-list surface. Current lower-IR regression authority locks successor ordering/content on representative diamond and loop families, and this gives the eventual conversion-mainline cleanup a formal input-layer successor view instead of relying only on ad hoc terminator decoding.
- 2026-04-08: Value-SSA entry-map construction now also consumes lower-IR immediate dominators as the default inheritance path. Non-phi temps now enter a block by inheriting the idom-out value first, and the old predecessor-state merge is narrowed to phi-candidate temps rather than recomputing every temp from predecessor readiness.
- 2026-04-08: Lower-IR analysis now also exposes a block-local phi-temp list helper on top of the existing temp-to-block candidate matrix. Direct lower-IR regression authority now locks those compact per-block phi-temp lists on representative diamond and loop families, and value-SSA conversion now consumes that block-local view instead of rescanning all temp ids at each phi-capable block.
- 2026-04-08: Value-SSA conversion now also stores pending-phi creation state with one fewer parallel array. `pending_phi_ids == SIZE_MAX` is now the only "phi not created yet" sentinel, so the old `pending_phi_present` bitmatrix is gone while the raw bridge authority remains unchanged.
- 2026-04-08: Value-SSA conversion no longer carries the old `out_valid/current_valid` bitsets. Temp availability is now represented by current rename bindings plus `SIZE_MAX` sentinels inside the remaining predecessor-facing snapshot surfaces.
- 2026-04-08: Value-SSA conversion now also drops the flat `phi_candidate_blocks` matrix from its long-lived conversion state. Lower IR still computes that matrix as an intermediate, but conversion now keeps only the compact block-local phi-temp lists during the dominator walk and phi finalization.
- 2026-04-08: Lower-IR input analysis now also exposes a direct `lower_ir_compute_temp_phi_candidate_lists(...)` helper, so consumers that only need per-block phi temps no longer have to materialize and thread a raw temp-by-block phi-candidate matrix themselves.
- 2026-04-08: Value-SSA conversion now consumes that direct lower-ir phi-list helper and also stores one `const LowerIrCfgAnalysis *` in bridge state instead of copying immediate-dominator/predecessor pointers into parallel fields, further shrinking duplicated input-analysis facts out of the bridge-local state.
- 2026-04-08: Value-SSA conversion now also drops the separate `processed_blocks` bitset. Block-completion checks derive directly from whether the corresponding output SSA block already has its terminator, so one more predecessor-readiness state layer is gone while representative bridge authority remains unchanged.
- 2026-04-08: Value-SSA conversion now also stores pending-phi value ids in compact block-local candidate order instead of a sparse `block * temp` matrix. Pending-phi bookkeeping is now keyed by `phi_candidate_offsets + candidate_index`, which keeps it aligned with the lower-ir block-local phi-temp lists and removes another temp-space-shaped bridge-local state surface.
- 2026-04-08: Value-SSA conversion now also compacts the phi-candidate temp ids themselves into that same block-local candidate order, instead of retaining a long-lived `block_count * temp_count` candidate-temp buffer through conversion. Candidate-temp lookup and pending-phi state now share one compact indexing scheme.
- 2026-04-08: Value-SSA conversion now also runs lower-ir temp bindings through a real dominator-tree scope state during conversion. In-block temp uses now resolve through the current rename binding scope, while the remaining predecessor-facing snapshot narrows toward a compact successor-phi-demand role instead of remaining the main in-block lookup source.
- 2026-04-09: Lower-IR input analysis now also exposes pruned phi-candidate lists. `lower_ir_collect_temp_live_in_blocks(...)` computes per-temp live-in blocks, and `lower_ir_compute_pruned_temp_phi_candidate_lists(...)` intersects liveness with iterated phi placement so strict downstream SSA construction can consume a tighter block-local phi list than the older conservative candidate matrix.
- 2026-04-09: Precision note on that strict-construction setup: the long-lived bridge state now consumes only the compact pruned phi list, but setup still materializes a temporary `raw_phi_candidate_temps` scratch buffer before compacting. So the persistent-state shrink is complete, while the setup scratch path is not yet fully compact.
- 2026-04-09: Value-SSA conversion has now been switched to the strict dominator-tree construction path. The bridge precreates all pruned candidate phis before traversal, binds those phi results on block entry, rewrites block-local lower-ir temp uses/defs only through the dominator-scope rename state, and fills successor phi incoming edges directly on predecessor leave.
- 2026-04-09: Value-SSA conversion no longer depends on deferred successor-phi snapshots or post-hoc backfill to build the normal SSA graph. The remaining finalize step is only a completeness check over the already-materialized phi inputs, and bridge output is alpha-renamed after construction so raw dumps stay dense and stable in dominator order.
- 2026-04-10: The first post-conversion SSA shared-analysis surface is now underway. `value_ssa_compute_def_use_analysis(...)` materializes per-value def-site metadata plus explicit use-site lists, providing the intended substrate for first real SSA-side optimization passes instead of one-off scans embedded inside each transform.
- 2026-04-12: That shared SSA analysis surface now also includes block-level liveness. `value_ssa_compute_liveness_analysis(...)` can compute verifier-safe `live_in/live_out` facts directly on Value-SSA CFGs, including phi-edge uses on predecessor exits and representative loop-backedge shapes; keep this as the reusable liveness substrate instead of recomputing ad hoc block liveness inside later passes.
- 2026-04-12: That liveness layer now also has a first concrete consumer. `value_ssa_compute_interference_graph(...)` builds a conservative Value-SSA interference graph from those live facts, including phi-edge predecessor liveness; treat this as backend-preparatory shared dataflow infrastructure, not as an excuse to skip ahead into full register allocation.
- 2026-04-12: That interference layer now also has a first direct consumer. `value_ssa_compute_copy_affinity_graph(...)` records non-interfering `mov` pairs as symmetric copy-affinity weights, so later allocator-prep/coalescing work can consume one shared candidate surface instead of rescanning SSA instructions ad hoc.
- 2026-04-12: Those allocator-oriented facts are now also available as one bundled summary. `value_ssa_compute_allocation_prep(...)` composes def-use, liveness, interference, and copy-affinity into per-value def/use counts, covered live-block lists, live-block counts, single-block-range flags, interference degrees, affinity sums, move-related flags, and a sorted candidate list, so downstream allocation-prep experiments can consume one stable surface instead of hand-stitching those analyses together every time.
- 2026-04-12: There is now also a first stable worklist surface over that summary. `value_ssa_compute_allocation_worklist(...)` classifies values into small allocator-prep buckets and sorts them by a deterministic priority heuristic, so later live-range/coalescing/allocation experiments can start from one shared ordering surface instead of each building a custom queue.
- 2026-04-12: That allocator-prep stack is now also directly inspectable. `value_ssa_dump_allocation_prep(...)`, `value_ssa_dump_allocation_worklist(...)`, and `value_ssa_allocation_work_class_name(...)` provide a stable debug surface for the current summary/worklist layers, so heuristic experiments can compare concrete text output instead of wiring temporary logging into each caller.
- 2026-04-12: That same allocator-prep stack now also has a few narrow query helpers on top. Live-block slices, affinity weights, and worklist lookup can all be queried directly from the shared summary/worklist surfaces instead of forcing later callers to open-code the same raw-array traversal.
- 2026-04-12: That helper layer now also covers the remaining common scans: later callers can directly look up one affinity candidate, ask for the top affinity neighbors of one value, and query the contiguous range for one worklist class instead of open-coding those scans over shared candidate/worklist arrays.
- 2026-04-10: The first SSA-native pass now also consumes that surface. `value_ssa_simplify_trivial_values(...)` has been moved off its private full-function def/use scan and onto the shared def/use/use-list analysis, establishing the intended layering for future SSA-side passes.
- 2026-04-10: `value_ssa_eliminate_dead_value_defs(...)` now also consumes the same shared def/use surface instead of rebuilding private use-count tables, so the first two SSA-side cleanup passes already share one analysis substrate.
- 2026-04-10: A first genuinely new SSA-native optimization pass is now landed above that shared surface: `value_ssa_fold_constants(...)` performs safe immediate/immediate binary folding while intentionally leaving dangerous/division/mod/shift cases untouched. The default Value-SSA canonicalization pipeline now includes this fold stage between trivial-value cleanup and CFG/DCE cleanup.
- 2026-04-10: A second narrow SSA-native optimization pass is now landed too: `value_ssa_simplify_algebraic_identities(...)` rewrites only safe binary identities/same-operand simplifications (for example `x+0`, `x*1`, `x&-1`, `x/1`, `x%1`, `x==x`) into `mov` form without touching dangerous div/mod/shift behavior outside those explicitly safe cases.
- 2026-04-10: A narrow SSA-side binary canonicalization pass is now landed too: `value_ssa_normalize_binary_operands(...)` pushes immediates to the RHS when safe and flips ordered comparisons as needed, so identity/fold stages and future SSA analyses see a more stable binary form.
- 2026-04-10: A narrow SSA-side dominator-aware redundant-binary cleanup pass is now landed too: `value_ssa_eliminate_redundant_binaries(...)` rewrites later dominated safe binaries to `mov` of an earlier equivalent result while continuing to preserve dangerous/trapping `div`/`mod`/shift ops.
- 2026-04-10: A narrow SSA-side local-slot forwarding pass is now landed too: `value_ssa_forward_local_loads(...)` rewrites same-block `load_local` instructions to `mov` when an earlier `store_local` or earlier same-block `load_local` already determines the slot's value. This stays explicitly local-slot-only rather than drifting into memory SSA.
- 2026-04-10: That same local-slot forwarding pass now also carries known local values across straight dominator-chain blocks when a block has a single predecessor equal to its immediate dominator. It still intentionally drops knowledge at joins rather than pretending to solve full memory flow.
- 2026-04-10: A matching narrow SSA-side global-slot forwarding pass is now landed too: `value_ssa_forward_global_loads(...)` rewrites `load_global` instructions to `mov` across same-block or straight dominator-chain regions when an earlier `store_global` or earlier forwarded `load_global` already determines the slot's value, while conservatively treating `call` as a kill point for known globals and still dropping knowledge at joins.
- 2026-04-10: A narrow SSA-side dead-store cleanup pass is now landed too: `value_ssa_eliminate_dead_stores(...)` removes same-block dead `store_local` / `store_global` instructions that are overwritten before any later read/observation of the same slot, and now also carries that overwritten-store cleanup across straight `jmp` chains when the successor has the current block as its unique idom predecessor. The pass stays conservative at joins and branch exits, and `call` still blocks global-store removal.
- 2026-04-11: A narrow SSA-side redundant-store cleanup pass is now landed too: `value_ssa_eliminate_redundant_stores(...)` removes a `store_local` or `store_global` when the current straight-chain known slot value already equals the stored value. This stays deliberately conservative: knowledge crosses only same-block or single-idom straight chains, joins still clear the slot state, and `call` still kills known globals.
- 2026-04-10: Value-SSA CFG cleanup now also threads through empty jump-only blocks, folds jumps to empty return blocks, and folds branch diamonds whose two empty successors return the same value. The canonicalization pipeline now runs DCE before CFG cleanup as well so value cleanup can expose those empty-return shapes before they are folded.
- 2026-04-12: Value-SSA CFG cleanup now also merges a `jmp` directly into a non-phi successor block when that successor has exactly one predecessor. Current authority is intentionally local and conservative: no phi-bearing targets are merged, but straight-line blocks such as `load; jmp; add; ret` now canonicalize to one surviving block.
- 2026-04-12: Value-SSA now also has a first interpreter slice for test/oracle use. `value_ssa_interp_execute_function(...)` / `value_ssa_interp_execute_main(...)` can execute verifier-legal SSA with `mov`, `binary`, `load_*`, `store_*`, `jmp`, `br`, `ret`, internal calls, and optional external-call callbacks; current regression authority locks straight-line execution, phi/branch execution, internal-call execution, external-call callback execution, and rejection of uninitialized local loads.
- 2026-04-10: The Value-SSA canonicalization pipeline now intentionally reruns trivial-value simplification after the identity/fold/CSE stages, so new `mov` results created by those value-only passes can collapse before CFG cleanup and dead-def elimination.
- 2026-04-08: Value-SSA conversion coverage now also includes a representative multi-carried-temp loop shape. The current bridge can materialize parallel loop-header phis for multiple independent carried temps in the same loop without changing the existing raw bridge authority style.
- 2026-04-08: Lower-IR input-layer analysis now also locks the same multi-carried-temp loop family at the phi-candidate level. `lower_ir_compute_temp_phi_candidates(...)` is now regression-checked on a loop header that simultaneously carries two independent temps, so conversion coverage and input-layer authority stay aligned on that shape.
- 2026-04-08: Value-SSA conversion now also covers a representative nested-loop carried-temp family. The bridge no longer fails early on inner headers that inherit an outer carried temp without needing a phi there: if lower-IR analysis marks no phi candidate and the already-processed predecessors agree on the inherited value, conversion now keeps that value live through the nested loop while still materializing the true inner-loop phi separately.
- 2026-04-08: Value-SSA conversion is now also locked against incidental sibling-order dependence inside supported CFG families. A scrambled-diamond regression with `bb.1` as the join block now ensures conversion can pre-create a pending phi when no predecessor has been processed yet, but will not do so once any already-processed predecessor has already shown the temp unavailable on that edge.
- 2026-04-08: Lower-IR input-layer authority now also includes a representative nested-loop carried-temp family. Current batch temp-phi-candidate analysis distinguishes "outer carried temp inherited through an inner header without a phi there" from the separately inner-carried temp whose iterated dominance frontier may still conservatively mark the outer header as a phi candidate.
- 2026-04-08: Lower-IR input-layer authority now also includes a representative nested double-loop/same-temp family. The current batch temp-phi-candidate analysis exposes both the outer header and the inner header when one carried temp is live through both loop levels, matching the bridge's current nested-phi behavior on that shape.
- 2026-04-08: Lower-IR input-layer authority now also covers the stronger nested same-temp + multi-backedge-inner-loop family. Even when the inner loop itself has multiple backedges, the current temp-phi-candidate surface still exposes the expected outer-header and inner-header candidates for the shared carried temp, matching the bridge's current behavior on that shape.

## Execution Log

- 2026-05-04: `lv9` closed at 22/22 passing after fixing the preview backend's real spill/local frame layout and the caller-save/return-value interaction around `a0`; the remaining sort-family wrong answers were caused by the driver restoring `a0` across calls that still needed the returned value.
- 2026-05-04: `machine_bytes` spill slots are now laid out after per-function locals instead of using the old temporary gap hack, so preview spill accesses no longer overlap local slots or frame-owned values.
- 2026-05-04: `compiler_driver` caller-save handling now keeps `a0` live across calls whenever the value produced by the call is still consumed later in the same block or in the block terminator, which fixed the last `sort1`-`sort6` wrong-answer tail.
- 2026-05-04: The reopened downstream object/reloc/ELF honesty line advanced again: focused `machine_object`, `machine_reloc`, and `machine_elf` regressions now lock non-4-byte global objects (`byte_size`-driven `.sdata` sizing and symbol sizing) instead of only 4-byte scalar globals. In parallel, `machine_object` now materializes initializer bytes into larger writable global objects by writing the low 4 bytes of the scalar initializer and leaving the remaining object bytes zero-filled, so the downstream `.sdata` / relocation / ELF chain stays honest for larger preview objects instead of silently collapsing them back to a 4-byte-only assumption.
- 2026-03-19: Milestone A continued: expanding AST detail by recording function parameter counts in top-level AST externals.
- 2026-03-19: Milestone A continued: recording declaration initializer metadata (`has_initializer`) for top-level AST declaration externals.
- 2026-03-19: Milestone A continued: parser now accepts function declarations (`int f(int);`) and AST records function declaration/definition metadata (`is_function_definition`).
- 2026-03-19: Milestone A continued: AST now records function-body `return_statement_count`, and semantic analysis rejects function definitions without any `return`.
- 2026-03-19: Milestone A continued: strengthened return semantic from "at least one return" to "all control-flow paths return", with parser metadata (`returns_on_all_paths`) and regressions.
- 2026-03-19: Milestone A continued: refined all-path return approximation with fallthrough tracking and conservative handling of syntactic infinite loops (`while(1)`, `for(;;)`) so unreachable trailing `return` does not satisfy the rule.
- 2026-03-19: Milestone A continued: added `break`/`continue` keyword and parser support, including loop-context validation and flow propagation so `while(1){break;} return ...` is treated as reachable while break/continue outside loops are rejected.
- 2026-03-19: Milestone A continued: AST now records per-function loop statement count (`loop_statement_count`) with parser regression coverage and dump support.
- 2026-03-19: Milestone A continued: AST now records per-function if statement count (`if_statement_count`) with parser regression coverage and dump support.
- 2026-03-21: Milestone A continued: AST now records per-function break/continue statement counts (`break_statement_count`, `continue_statement_count`) with parser regression coverage and dump support.
- 2026-03-21: Milestone A continued: AST now records per-function declaration statement count (`declaration_statement_count`) with parser regression coverage and dump support.
- 2026-03-21: Milestone A (Expression Tree A1) started: added minimal standalone expression AST parsing for primary + parenthesized expressions with dedicated parser regressions and no semantic rule changes.
- 2026-03-21: Milestone A (Expression Tree A2) continued: standalone expression AST now covers multiplicative/additive binary expressions with precedence + left-associativity regressions.
- 2026-03-21: Milestone A (Expression Tree A3-partial) continued: standalone expression AST now covers relational expressions (`<`, `<=`, `>`, `>=`) with precedence/associativity regressions; equality/assignment remained deferred at this checkpoint.
- 2026-03-21: Milestone A (Expression Tree A3) continued: standalone expression AST now covers equality expressions (`==`, `!=`) and assignment expressions (`=`) with precedence/associativity regressions.
- 2026-03-21: Milestone A (Expression Tree A4) continued: standalone expression AST now covers unary prefix expressions (`+`, `-`) with unary-chain and precedence regressions.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary edge-case regressions for assignment RHS (`a=-b`), parenthesized additive operand (`-(a+b)`), and multiplicative right unary (`a*-b`).
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary negative regressions for incomplete operands (`a=+`, `a*-`, `-(a+)`) with primary-expression diagnostics.
- 2026-03-21: Milestone A hygiene: expression regression helper now targets assignment main entrypoint directly; added explicit equality-alias forwarding compatibility lock.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added assignment left-hand-side boundary regressions (`(a)=b`, `1=b`, `a+b=c`) to lock current identifier-only assignment acceptance.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added assignment/equality mix boundary regressions (`a=b==c`, `a==(b=c)`, reject `a==b=c`) to lock precedence and boundary behavior.
- 2026-03-21: Milestone A (Expression Tree A4) continued: expanded assignment LHS boundary locks with nested/parenthesized cases (`(a=b)=c`, `((a))=b`, `a=(b==c)=d`) to prevent accidental lvalue broadening.
- 2026-03-21: Milestone A hygiene: standardized assignment-boundary diagnostics in regressions to assert `EOF` + `ASSIGN` together for clearer failure localization.
- 2026-03-21: Milestone A hygiene: lightly grouped expression regression sections (positive/compatibility/negative) for easier maintenance without behavior changes.
- 2026-03-21: Milestone A hygiene: extracted shared helper for assignment-failure expression regressions to reduce duplicated tokenize/parse/diagnostic boilerplate.
- 2026-03-21: Milestone A hygiene: migrated remaining expression failure templates (trailing tokens and deep-parentheses recursion-limit case) to the shared helper.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary logical-not (`!`) support in lexer/parser expression paths with parser/lexer regression coverage (`!a`, `a=!b`, and reject `a=!`).
- 2026-03-21: Milestone A hygiene: kept smoke fixtures in sync with new operator support by updating both `tests/lexer/test.c` and `tests/parser/test.c` to include unary `!` usage.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary bitwise-not (`~`) support in lexer/parser expression paths with parser/lexer regression coverage (`~a`, `a=~b`, and reject `a=~`), and synced smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A5) continued: added logical-and/logical-or (`&&`, `||`) expression support with precedence/associativity/boundary regressions; lexer still rejects single `&` and `|` by design until bitwise layers are introduced.
- 2026-03-21: Milestone A (Expression Tree A6) continued: added bitwise-and/bitwise-xor/bitwise-or (`&`, `^`, `|`) expression support with precedence/boundary regressions, updated lexer tokenization for single-character bitwise operators, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A7) continued: added shift expression support (`<<`, `>>`) with additive/relational precedence and associativity regressions, lexer shift tokenization, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A8) continued: added conditional expression support (`?:`) with right-associativity and precedence regressions, added lexer tokens for `?`/`:`, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A9) continued: added comma expression support (`,`) as lowest-precedence expression layer with precedence/associativity/boundary regressions and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A10) continued: added unary prefix increment/decrement support (`++`, `--`) in lexer/parser expression paths with regression coverage and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A11) continued: added postfix increment/decrement support (`a++`, `a--`) in parser expression paths with AST/precedence/negative regressions and synchronized smoke fixtures.
- 2026-03-23: Milestone A hardening: tightened `++/--` operand acceptance to identifier-lvalue in both AST and translation-unit parser paths, added non-lvalue rejection regressions, and added lexer boundary-sequence locks for `<<=`, `>>=`, `&=`, `|=`, `^=` prior to compound-assignment support.
- 2026-03-23: Milestone A (Expression Tree A12) continued: added postfix function-call expression support in AST and translation-unit parser paths (including argument-list parsing), added positive/negative parser regressions, and synchronized lexer/parser smoke fixtures.
- 2026-03-23: Milestone A (Expression Tree A13) continued: added compound assignment operator support (`+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `^=`, `|=`) in lexer and assignment parser layers, added translation-unit/expression-AST regressions, and synchronized smoke fixtures.
- 2026-03-24: Milestone A (Semantic A1) continued: added baseline callable semantic check by recording called function names from function bodies and rejecting calls to undeclared functions in semantic analysis, with dedicated semantic regressions.
- 2026-03-24: Milestone A (Semantic A2) continued: refined callable diagnostics to distinguish `undeclared function` from `call to non-function symbol`, with dedicated semantic regression coverage.
- 2026-03-24: Milestone A (Semantic A3) continued: callable semantic diagnostics now report call-site line/column by carrying parser call metadata (`name + line + column`) into AST externals, with regression lock for location accuracy.
- 2026-03-24: Milestone A (Semantic A4) continued: added minimal callable argument-count check (call arity must match declared function parameter count), with dedicated semantic regression coverage.
- 2026-03-24: Milestone A (Semantic A5) hardening: fixed parser call-metadata realloc failure safety (no stale-pointer cleanup path), and locked current chained-call semantic behavior as known limitation.
- 2026-03-24: Milestone A (Semantic A6) continued: tightened minimal callable semantics to reject indirect/chained call-result forms (e.g., `f()(1)`) as `SEMA-CALL-005: call result is not callable`, with regression lock.
- 2026-03-24: Milestone A (Semantic A7) hardening: fixed parenthesized call-result bypass so `(f())()`, `(f(1))(2)`, and `((f(1)))(2)` are consistently rejected under the `SEMA-CALL-005` callable policy, with dedicated semantic regression locks.
- 2026-03-24: Milestone A (Semantic A8) continued: callable checks now enforce declaration-before-use visibility order (prior declarations/prototypes required), with forward-call regression locks.
- 2026-03-24: Milestone A (Semantic A9) continued: refined forward-call diagnostics to distinguish `before declaration` from truly `undeclared function`.
- 2026-03-24: Milestone A (Semantic A10) continued: forward-call diagnostics now include first declaration location (`line:column`) for faster triage.
- 2026-03-24: Milestone A (Semantic A11) continued: split indirect-call diagnostics into two categories: `Call result is not callable` and `Non-identifier callee not supported`, with regression coverage.
- 2026-03-24: Milestone A (Semantic A12) continued: unified callable diagnostics under stable error-code prefixes (`SEMA-CALL-001..006`) for IDE/script matching.
- 2026-03-24: Milestone A (Semantic A13) continued: aligned TU callable-shape recognition with expression parsing for parenthesized call-result chaining forms (`((f)(1))(2)`, `((f)())()`), with parser+semantic regression locks.
- 2026-03-24: Milestone A (Semantic A14) continued: enabled parser-to-semantic routing for parenthesized non-identifier callees (for example `(f+1)()`), so these forms now fail with `SEMA-CALL-006` in normal analysis flow rather than requiring synthetic metadata-only coverage.
- 2026-03-25: Milestone A (Semantic A15) continued: added boundary regressions that lock parser/semantic split for non-identifier callees: parenthesized forms reach semantic and report `SEMA-CALL-006`, while non-parenthesized forms remain parser-rejected.
- 2026-03-25: Milestone A (Semantic A16) continued: added a table-driven callable diagnostic matrix lock (`SEMA-CALL-001..006`) covering representative direct-call, call-result, and non-identifier callee forms.
- 2026-03-25: Milestone A (Semantic A17) continued: expanded table-driven `SEMA-CALL-006` coverage with additional parenthesized non-identifier callee shapes (nested parentheses and logical-expression forms) to further harden parser/semantic boundary guarantees.
- 2026-03-25: Milestone A (Semantic A18) continued: added a table-driven callable accept matrix for semantic-pass baselines (direct calls, prior prototypes, and parenthesized direct-identifier callees) to complement the existing `SEMA-CALL-001..006` failure matrix.
- 2026-03-25: Milestone A (Parser A19) continued: added table-driven parser callable entry matrices for both translation-unit and expression-AST entrypoints, locking accepted vs rejected callee-shape boundaries in a single consolidated suite.
- 2026-03-25: Milestone A (Semantic A20) continued: upgraded callable failure matrix assertions from code-only checks to `error-code + key diagnostic snippet + call-site line/column` checks for stronger IDE/script stability guarantees.
- 2026-03-25: Milestone A (Semantic A21) continued: enriched `SEMA-CALL-004` payload to include explicit expected vs actual argument counts (`expected N, got M`) and locked this wording in matrix regressions.
- 2026-03-25: Milestone A (Semantic A22) continued: enriched `SEMA-CALL-002` with stable declaration-location key fields (`decl_line`, `decl_col`) while preserving existing human-readable `first declaration at L:C` wording.
- 2026-03-25: Milestone A (Semantic A23) continued: unified `SEMA-CALL-001..006` diagnostics with stable key-value payload fragments (`callee`, `decl_line/decl_col`, `expected/got`, `callee_kind`) and updated matrix assertions accordingly.
- 2026-03-25: Milestone A (Semantic A24) hardening: increased semantic diagnostic buffer capacity, removed redundant `SEMA-CALL-004` payload duplication, and added long-identifier regression lock to prevent message-tail truncation (`expected/got` fields must survive).
- 2026-03-25: Milestone A (Statement AST S1-1) continued: landed minimal function-body statement AST structure (`compound/declaration/expression/if/while/for/return/break/continue`) and attached `function_body` to function-definition externals while preserving existing parser metadata behavior.
- 2026-03-25: Milestone A (Statement AST S1-2) continued: statement AST nodes now capture key expression subtrees (`expression`, `return`, `if` condition, `while` condition, `for` init/condition/step) via token-slice expression-AST reconstruction, with no semantic behavior changes.
- 2026-03-25: Milestone A (Statement AST S1-3) continued: added explicit statement expression-slot metadata (`primary`, `condition`, `for_init`, `for_step`) and parser regression locks for slot/index stability; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-boot) started: semantic now performs statement-AST callable shadow traversal and cross-checks parser callable metadata (`name/line/column/arg_count/callee_kind`) for internal consistency (`SEMA-INT-001..003`), while keeping existing callable diagnostics/behavior unchanged; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-2) continued: callable semantic checks now consume an AST-primary call view (with parser-metadata fallback when statement AST body is unavailable), preserving existing `SEMA-CALL-001..006` behavior and diagnostics; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-3) continued: tightened AST-primary migration gate so function definitions must provide statement AST body (`SEMA-INT-005` on contract breach), and updated semantic regressions accordingly; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-4) continued: made callable-view source explicit in semantic internal data flow (`AST-primary` vs `metadata-fallback`) and added a definition-path guard (`SEMA-INT-006`) to ensure function-definition callable checks never run on fallback view; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-5) continued: relaxed hard dependency on parser callable metadata for AST-primary function definitions by making metadata parity check a shadow toggle, and added semantic regression lock proving AST-primary callable results remain correct even when parser metadata is tampered; full `make test` remained green.
- 2026-03-27: Milestone A (Semantic S2-6) continued: replaced single macro parity toggle with explicit migration-strategy constant (`STRICT_PARITY` / `AST_PRIMARY_ONLY`) so S2 policy switching is centralized and does not require scattered preprocessor edits; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-7) continued: introduced reusable semantic AST post-order walk helpers and moved AST-primary callable rule evaluation to visitor-driven checks (keeping metadata fallback path for non-body contexts), reducing intermediate call-list coupling while preserving existing `SEMA-CALL-001..006` behavior; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-7) hardening: added regression lock proving AST-primary visitor callable checks are authoritative for chained-call rejection (`SEMA-CALL-005`) even when parser callable metadata is intentionally tampered.
- 2026-03-29: Milestone A (Semantic S2-8) continued: switched function all-path-return semantic gating to AST-primary statement-flow evaluation (metadata used only as optional strict-parity guard), and added tamper regression lock proving return rejection remains correct when `returns_on_all_paths` parser metadata is intentionally forced; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-9) cleanup: removed currently unreachable callable metadata-fallback/guard branches (`SEMA-INT-004`, `SEMA-INT-006`) from active callable-check path and made function-definition callable checks explicitly AST-primary, reducing maintenance ambiguity without behavior change; added explicit regression lock that direct chained form `a()()` remains rejected as `SEMA-CALL-005`; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-10) consistency: converged callable migration notes with current implementation by documenting function-definition callable checks as AST-primary-only and marking `SEMA-INT-004`/`SEMA-INT-006` as retired from active callable path (historical-only), reducing roadmap/code interpretation drift; full `make test` remained green.
- 2026-03-29: Milestone planning refresh: marked S2 as closed for implementation tracking, shifted active plan to S3 scope semantics, and rewrote near-term plan blocks to separate completed work from remaining tasks.
- 2026-03-29: Milestone A (Semantic S3-1) started: parser now records function parameter names plus local declaration names on statement AST nodes (including `for(int i=...)` init declarations), and semantic now enforces baseline scope rules (`SEMA-SCOPE-001` duplicate-local-in-scope reject, `SEMA-SCOPE-002` undeclared-identifier-use reject) while preserving block shadowing acceptance; added parser + semantic regression locks and kept full `make test` green.
- 2026-03-29: Milestone A (Semantic S3-2) continued: aligned function-scope integration so parameters and function-body top-level declarations share the same scope frame (rejecting parameter redeclaration in the function body), and expanded scope regressions for `for` init lifetime and shadowing interactions; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: expanded scope regression locks for duplicate function-parameter names, block-local lifetime boundary (post-block use reject), for-body local lifetime boundary (post-loop use reject), and inner-block parameter shadowing acceptance; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) bugfix: declaration initializer expressions are now preserved in statement AST for both block declarations and `for(int ... )` init declarations, so scope traversal correctly rejects undeclared-identifier uses inside initializer expressions (`SEMA-SCOPE-002`); added parser + semantic regression locks for this path and kept full `make test` green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added callable/scope boundary enforcement so direct-identifier callees shadowed by local declarations (including parameters, local block declarations, and `for` init declarations) are rejected as `SEMA-CALL-003` (non-function symbol), and expanded callable diagnostic/pass matrices in one batch to lock these shadowing paths; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added a callable shadow precheck pass so local non-function callee shadowing is diagnosed as `SEMA-CALL-003` before top-level callable lookup (closing `CALL-001/002` precedence drift in local-shadow cases), and batch-expanded callable matrix coverage for no-top-level/later-declaration shadow variants plus non-identifier stability (`SEMA-CALL-006` unchanged); full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added diagnostic-ordering regression locks for mixed-error call sites (ensuring `SEMA-CALL-003/005/006` remain prioritized when applicable before scope undeclared-identifier checks, while declared-callee + undeclared-arg still reports `SEMA-SCOPE-002`), keeping callable/scope layering deterministic; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: batch-expanded callable/scope interaction matrix for declaration-initializer and loop-slot expressions (`for` condition/step), including ordering locks that keep `SEMA-CALL-003/004/005/006` precedence stable in those slots and fallback locks that keep declared-callee + undeclared-arg paths on `SEMA-SCOPE-002`; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) bugfix/hardening: scope traversal now evaluates each declarator initializer before introducing that declarator name (including `for(int ... )` init declarations), closing same-declaration forward-reference leaks such as `int x=y,y=1` and `for(int i=j,j=0;...)`; parser initializer slots are now aligned 1:1 with declarator order (nullable for no-initializer declarators), and parser+semantic regressions were batch-expanded for reject/accept ordering variants; full `make test` remained green.
- 2026-03-29: Milestone A (Parser ergonomics) low-priority hardening: clarified expression recursion-limit diagnostic wording to explicitly state it is call-depth-frame based (`%zu call-depth frames`) so deep-parentheses failures are interpreted correctly; behavior unchanged and parser regressions remained green.
- 2026-03-29: Milestone A stage-gate: revalidated full suite (`make test`) with scope/callable/CF matrices green after latest S3-2 hardening slices, and marked S3-2 complete for milestone tracking; active work now transitions to S3-3 cleanup.
- 2026-03-30: Milestone A (Semantic S3-3) first cleanup slice landed: retired callable metadata parity rails from active semantic callable path by removing parser `called_function_*` consistency cross-check (`SEMA-INT-001/002/003`) for function-definition callable analysis; callable rules now remain AST-primary only in active flow, and full `make test` stayed green.
- 2026-03-30: Milestone A (Semantic S3-3) callable-retirement closure: verified no remaining `called_function_*` references in semantic implementation (`src/semantic/semantic.c`) and revalidated full suite (`make test`) with callable/scope/CF behavior unchanged; mark `called_function_*` semantic-authority retirement complete and transition active cleanup to S3-4 (`returns_on_all_paths`).
- 2026-03-30: Milestone A (Semantic S3-4) first cleanup slice landed: removed `returns_on_all_paths` parser-metadata parity gate from semantic return-path analysis (`SEMA-INT-008` rail retired), keeping return enforcement AST-primary from statement-flow results only; revalidated full suite (`make test`) with unchanged callable/scope/CF outcomes.
- 2026-03-30: Milestone A (S3 post-cleanup verification) completed: confirmed semantic implementation has zero references to callable metadata (`called_function_*`), return metadata (`returns_on_all_paths`), and statement-counter metadata (`loop/if/break/continue/declaration counts`); S3 semantic-authority retirement goals are complete and handoff focus moves to Milestone B decoupling.
- 2026-03-30: Milestone A tracking correction (historical, superseded by next clarification): S3 remains in progress; semantic-authority retirement is complete, but parser-side metadata retention cleanup (callable/return/counter legacy collection and compatibility touchpoints) is still pending before S3 closure.
- 2026-03-30: Milestone A tracking clarification: S3 closure criterion follows semantic-authority retirement scope; parser-side metadata retention cleanup is classified as Milestone B decoupling work and does not block S3 completion.
- 2026-03-30: Milestone transition checkpoint: Milestone A is closed for implementation tracking (semantic-authority retirement complete and validated), active implementation is switched to Milestone B, and a detailed B execution plan is recorded below as the next working contract.
- 2026-03-30: Roadmap consistency sweep: normalized top-level guidance and near-term milestone states so document-wide wording reflects the same boundary (`A closed`, `B active`).
- 2026-03-30: Milestone B (B0.5) completed: split oversized regression suites into aggregator + themed include fragments (`tests/parser/parser_regression_test.c` + `tests/parser/parser_regression_cases_*.inc`, `tests/semantic/semantic_regression_test.c` + `tests/semantic/semantic_regression_*.inc`) while preserving original case execution order; full `make test` remained green.
- 2026-03-30: Milestone B (B0.5) refinement: further split parser expression-heavy fragment into smaller themed files (`parser_regression_cases_expr_ast_a.inc`, `parser_regression_cases_expr_ast_b.inc`, `parser_regression_cases_ast_meta.inc`) and updated aggregator includes; full `make test` remained green.
- 2026-03-30: Milestone B (B0.5) maintainability refinement: unified parser split-fragment IntelliSense prelude into one shared include (`parser_regression_intellisense_prelude.inc`), removed duplicated local macro/type guards across parser fragments, and added explicit split-fragment inventory comments in parser/semantic regression entry files for safer review and future edits; full `make test` remained green.
- 2026-03-30: Milestone B (B1) completed: retired parser-side callable metadata retention interfaces by removing `called_function_*` fields from `AstExternal`, removing parser AST-output plumbing for callable metadata in translation-unit AST assembly, and updating semantic regressions to stop tampering retired callable metadata fields while preserving AST-primary callable behavior assertions; full `make test` remained green.
- 2026-03-30: Milestone B (B2) completed: retired parser-side return all-path metadata retention by removing `returns_on_all_paths` from `AstExternal`, removing parser return-flow write plumbing from function-external/TU-AST assembly, and aligning parser/semantic regressions plus parser AST dump expectations to the AST-primary return-flow contract; full `make test` remained green.
- 2026-03-30: Milestone B (B3) completed: retired parser-side statement-counter retention by removing `loop/if/break/continue/declaration` counter fields from `AstExternal`, removing parser accounting/write paths and parser dump output for these counters, and pruning parser regressions that specifically asserted retired metadata while keeping AST-primary parse/semantic behavior checks; full `make test` remained green.
- 2026-03-30: Milestone B (B4) completed: removed parser dependency on `ast_internal.h` by introducing parser-local AST lifecycle/add-external helpers (`parser_ast_*`) and routing parser AST ownership operations through that local compatibility layer, preserving legacy parser-only link behavior (`parser_legacy_link_test`) while reducing parser/AST internal coupling; full `make test` remained green.
- 2026-03-30: Milestone B (B5) completed and milestone closed: executed retirement verification bundle (`make test` green + grep checks confirming retired metadata families absent from `AstExternal`, parser external write paths, active semantic implementation, and test assertions), and finalized handoff focus to Milestone C static-analysis cleanup.
- 2026-03-30: Large-file split follow-up (Phase S parser slice) completed: split `src/parser/parser.c` into function-oriented fragments (`parser_ast_compat.inc`, `parser_core_expr.inc`, `parser_stmt_decl_tu.inc`) with `parser.c` as the aggregator entry, preserving parser behavior and legacy-link compatibility; full `make test` remained green.
- 2026-03-30: Milestone C baseline started: re-ran full suite with GCC `-fanalyzer` and ASan instrumentation; all suites stayed green. Scoped suppression was added for one GCC `-Wanalyzer-fd-leak` false positive in `tests/lexer/lexer_regression_test.c` (`run_with_stderr_suppressed` stderr redirection helper), while preserving normal warning coverage for the rest of the codebase.
- 2026-03-30: Milestone C strict-warning sweep: re-ran full suite under stricter warning flags (`-Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2`) and confirmed zero additional warnings with all regression suites green.
- 2026-03-30: Milestone C ergonomics: added reusable Makefile targets `test-fanalyzer`, `test-asan`, and `test-strict-warnings` and validated all three targets end-to-end to keep static/runtime analysis checks one-command reproducible.
- 2026-03-30: Large-file split follow-up (Phase S semantic slice) completed: split `src/semantic/semantic.c` into function-oriented fragments (`semantic_core_flow.inc`, `semantic_callable_rules.inc`, `semantic_scope_rules.inc`, `semantic_entry.inc`) with `semantic.c` as aggregator entry; behavior preserved and full `make test` remained green (commit `116af1b`).
- 2026-03-30: Milestone C closure checkpoint: re-ran `test-fanalyzer`, `test-asan`, and `test-strict-warnings`; all three targets passed with zero warnings and semantic/parser/lexer regression suites green.
- 2026-03-30: Milestone transition checkpoint: switched active implementation focus from Milestone C to Milestone D strict-path semantics.
- 2026-03-30: Milestone D (D1) tests-first slice landed: flipped semantic CF matrix expectations for `CF-02`/`CF-03`/`CF-06` from accept to reject in strict-path target group.
- 2026-03-30: Milestone D (D2) semantic slice landed: tightened loop return-path analysis for `while(1)`/`for(;;)` when loop bodies can both break and reiterate (`fallthrough` or `continue`), preventing trailing returns from masking non-termination paths; full `make test` and all analysis targets remained green.
- 2026-03-30: Milestone D (D3) coverage slice landed: expanded strict-path CF matrix with nested-loop and mixed break/continue edge locks (`CF-11..CF-18`) and kept full `make clean && make test` plus analysis targets green.
- 2026-03-30: Milestone D (findings hardening) landed: closed equivalent-true condition bypasses by extending constant-true flow recognition to parenthesized/unary/comma constant expressions (for example `(1)`, `+1`, `(0,1)`), added dedicated regressions (`CF-19..CF-25`), and stabilized reject assertions with error code `SEMA-CF-001`; full `make clean && make test` and analysis targets remained green.
- 2026-03-30: Milestone D (D4 build reliability) landed: added explicit parser/semantic `.inc` dependency tracking in `Makefile` so include-fragment edits trigger rebuilds without manual clean.
- 2026-03-30: Milestone D (parser boundary hardening) landed: soft-retired parser callable metadata collection in active path (`PARSER_ENABLE_CALL_METADATA_TRACKING=0` default), clarified parser-flow ownership comments (parser flow is syntax-local; semantic flow is authority), and added parser regression lock proving parser does not enforce semantic all-path return policy.
- 2026-03-30: Milestone D (low-risk guardrail) landed: added explicit maintenance checklist note in parser AST compatibility layer to keep lifecycle helpers synchronized with `include/ast_internal.h` when AST ownership fields evolve.
- 2026-03-31: Milestone D (D6 parser flow convergence) landed: removed parser-side `guaranteed_return` flow output/equations from `parse_*_with_flow` internal interfaces, keeping only syntax-local `may_fallthrough`/`may_break` propagation plus loop-context validation; full `make clean && make test` and analysis targets remained green.
- 2026-03-31: Milestone D (D7 callable metadata hard-retire) landed: removed remaining parser callable metadata state/plumbing from active parser paths and converged call parsing interfaces away from parser-side callable retention; `make test` remained green.
- 2026-03-31: Milestone D (D8 AST lifecycle de-dup) landed: introduced shared lifecycle template instantiation for AST internal and parser compatibility layers, added parser regression lock for complex AST clear-on-failure behavior, and kept `make test` green.
- 2026-03-31: Milestone D (post-D8 convergence cleanup) landed: parser function-external return counting now activates only when explicitly requested by caller output (`out_return_statement_count`), with parser tracking state restored after function-body parse to keep non-AST parse path state-minimal; `make test` remained green.
- 2026-03-31: Milestone D (D9 strict-path constant-condition expansion) landed: semantic flow constant evaluation now recognizes ternary and key binary expression families (logical/equality/relational/bitwise plus short-circuit handling for `&&`/`||`) for loop-condition strict-path reasoning, with CF matrix expansion (`CF-26..CF-31`) and `make test` green.
- 2026-03-31: Milestone D (D9 arithmetic/shift guard expansion) landed: strict-path constant-condition evaluation now also covers arithmetic and shift binary forms (`+ - * / % << >>`) with conservative safety guards for division-by-zero, signed-overflow-sensitive cases, and invalid shift widths, plus CF matrix expansion (`CF-32..CF-40`); `make test` remained green.
- 2026-03-31: Milestone D (D9 strict-path boundary matrix expansion) landed: added nested-loop plus mixed `break`/`continue` with ternary/constant-operator condition combinations (`CF-41..CF-48`) to lock conservative all-path semantics at deeper loop nesting boundaries; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail) landed: retired parser-global function return tracking state (`track_function_return_count` / `function_return_count`) and removed return-statement side-effect counting from parse flow; function `return_statement_count` is now derived from function-body AST traversal only when requested, keeping local-control as the sole parser statement-control path; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail follow-up) landed: removed retired callable-name token compatibility plumbing from expression parsing (`token_slice_callable_name_token` and `out_callable_name_token` path in `parse_primary_with_flags`/`parse_postfix`), leaving callable checks on active local parse flags only; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail batch cleanup) landed: retired legacy expression AST alias APIs (`parser_parse_expression_ast_primary/additive/relational/equality`) and switched regression coverage to the canonical assignment-level entrypoint; also deduplicated parser public-entry setup (`parser_prepare`) across TU/TU-AST/expression parse entrypoints to remove compatibility-era duplicated state/bootstrap branches; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail function-external split) landed: split function external parsing into dedicated signature/body helpers (`parse_function_external_signature`, `parse_function_external_definition_body`) and centralized parameter-name ownership commit, reducing compatibility-era branching in `parse_function_external` while preserving AST contract outputs; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail TU-loop dedup) landed: introduced shared translation-unit external-or-declaration helper (`parse_translation_unit_external_or_declaration`) to unify TU and TU-AST loops that previously duplicated "try function external, then rewind and fallback to declaration" control flow; behavior preserved and `make test` remained green.
- 2026-03-31: Milestone D (D11 top-level initializer semantic closure) landed: parser now persists file-scope declaration initializer AST on `AstExternal`, semantic entry now applies callable/scope checks to top-level initializer expressions (closing prior bypass where `has_initializer` existed without semantic expression traversal), and regressions now lock reject/pass matrix coverage for undeclared identifier, call-to-non-function, call-before-declaration, and same-declaration forward-reference top-level initializer cases; `make test` remained green.
- 2026-03-31: Milestone D (D12 top-level definition + allocator hardening) landed: semantic entry now rejects duplicate top-level variable definitions when repeated declarations both carry initializer definitions (`SEMA-TOP-001`) while preserving declaration + single-definition compatibility; regressions now lock both reject and pass forms. In parallel, dynamic growth paths in lexer/parser/semantic scope stack/AST external storage now use overflow-safe capacity expansion guards to prevent `size_t` wraparound on very large inputs; `make test` remained green.
- 2026-03-31: Milestone D (D13 initializer self-visibility + unary-overflow guard + strict-target reliability) landed: top-level initializer semantic checks now exclude the current external from visibility while still allowing prior declarations, fixing same-declaration self-reference/call false accepts and non-function misdiagnostics (`int x=x;`, `int x=x();`); callable and scope regressions were expanded (`TOPINIT-SCOPE-003`, `TOPINIT-CALL-003`). Strict-path constant folding now guards unary minus on `LLONG_MIN` to avoid signed-overflow UB during semantic flow evaluation, with CF coverage expansion (`CF-49`). Makefile test execution now runs copied test binaries (`*.run.$$`) to mitigate environment-specific `Text file busy` failures in strict-warning runs; `make test` and `make test-strict-warnings` are green.
- 2026-04-01: Milestone D (D14 modulo-overflow guard + runner suffix fix) landed: strict-path constant folding now also guards modulo on `LLONG_MIN % -1` to avoid semantic-flow `SIGFPE`/UB from legal source in loop conditions, with CF coverage expansion (`CF-50`). Makefile test runner temp-copy suffix escaping was corrected from `.$$` to `.$$$$` so shell PID expansion is actually applied per run, avoiding fixed `.run.$` collisions across concurrent sessions; `make test` and `make test-strict-warnings` are green.
- 2026-04-01: Milestone D (D15 semantic expression-depth guard) landed: semantic recursive expression traversals (callable postorder walk, scope/call-shadow visibility checks, strict-path constant folding) now enforce a shared depth ceiling and report `SEMA-INT-012` instead of crashing on extremely deep left-associative expression trees. Regression coverage now includes generated deep-chain input to lock non-crashing failure behavior; `make test` is green.
- 2026-04-01: Milestone D (D16 constant-if flow narrowing) landed: `AST_STMT_IF` flow merge now narrows on constant condition truthiness using the same constant evaluator family used by loop strict-path checks, so constant-true/constant-false `if` branches no longer conservatively merge unreachable paths into false `SEMA-CF-001` rejects. CF matrix coverage now includes `if(1)`, `if((1))`, `if(+1)`, `if((0,1))`, `if(0) ... else ...`, and loop-contained constant-true break paths (`CF-51..CF-56`); `make test` is green.
- 2026-04-01: Milestone D (D18 loop CF guard-stability heuristic) landed: non-constant `while`/`for` flow now assumes the loop may execute at least once, but no longer treats first-iteration exits implied only by reusing the entry condition as proof of fallthrough. Loops are rejected only when control flow can reliably prove a stable-guard dead-loop shape: the exit depends on identifiers that never change in the body/step and are not plausibly affected by calls. Mutation-driven exits and call-driven exits remain accepted, while shapes such as `while(a){}`, `while(a){if(a){break;}}`, `while(1){if(a){break;}}`, and `for(;a;){continue;}` are rejected; `make test` is green.
- 2026-04-01: Milestone D (D19 loop guard shadow-sensitivity) landed: loop CF guard tracking now resolves identifiers against local declaration scopes, so same-name block or `for`-init shadowing inside loop bodies no longer counts as mutating or exiting the outer loop guard by name alone. Per-iteration body declarations now also contribute their initializer dependencies when a shadowed break guard is rebuilt from outer state, so mutation-driven exits like `int a=b; if(a) break; b--;` remain accepted while stable shadowed dead loops stay rejected. Regression coverage now locks both representative dead-loop forms (`CF-64..CF-66`) and rebuilt-guard accept cases (`CF-67..CF-68`); `make test` is green.
- 2026-04-01: Roadmap consistency cleanup: top-level Milestone D wording, D10/D17/D18 status text, and known-limitations language were reconciled with the current conservative CF policy so historical strict-path slices are clearly archival only.
- 2026-04-03: Milestone D bugfix landed: non-constant `while`/`for` conditions no longer inherit all-path return from a return-only loop body without proving first-iteration entry. Regression coverage now locks zero-iteration holes such as `while(a){return 1;}`, `for(;a;){return 1;}`, call-guard variants, and `break; return ...` unreachable-tail forms; `make test` is green.
- 2026-04-03: Milestone D documentation alignment landed: CF matrix commentary was synchronized with the actual stable-guard policy so value-dependent loops are no longer described as unconditionally accepted.
- 2026-04-03: Milestone transition checkpoint: front-end control-flow/diagnostic work is considered sufficiently stabilized for handoff, and active implementation focus shifts to Milestone E IR generation bootstrap.
- 2026-04-03: Milestone E planning decision: the first IR will be a single canonical block-based three-address IR with explicit basic blocks/terminators, non-SSA locals, unique internal IDs for blocks/locals/temps, and pretty-printed names only at dump time.
- 2026-04-03: Milestone E implementation slice landed: IR core was split into focused include fragments (`ir_core`, scope lowering, expression lowering, statement lowering, verify, dump) with IntelliSense-safe aggregation, and the build now tracks split IR dependencies.
- 2026-04-03: Milestone E implementation slice landed: canonical CFG lowering now covers explicit `ret/jmp/br` terminators, `if/while/for`, and `break/continue`, using stable block IDs in lowering state to avoid realloc-driven stale block pointers.
- 2026-04-03: Milestone E implementation slice landed: expression lowering now covers compare/equality, short-circuit logical forms, bitwise/shift/comma, direct calls, compound assignment, prefix/postfix `++/--`, and ternary in both value and branch contexts.
- 2026-04-03: Milestone E verifier slice landed: public `ir_verify_program` now enforces structural and flow invariants (mandatory terminators, target validity, reachability, known ops, temp definition coverage, all-path temp availability at use sites, and direct-call arity checks).
- 2026-04-03: Milestone E declaration-awareness slice landed: IR now preserves function declarations as signature-only entries, merges declaration+definition by function name, validates declaration-only shape constraints, and allows declaration-driven call arity verification.
- 2026-04-03: Milestone E verifier hardening landed: verifier now rejects duplicate temp definitions inside the same basic block (`IR-VERIFY-031`) while preserving cross-block join-temp materialization patterns used by branch-merging lowering.
- 2026-04-03: Milestone E callable-boundary slice landed: declaration/definition merges now tolerate parameter-name drift (arity remains authoritative), definition lowering rejects duplicate body-lowering attempts, and verifier rejects unknown direct-call targets (`IR-VERIFY-032`) in addition to call-arity checks.
- 2026-04-03: Milestone E verifier hardening landed: call instruction payload consistency is now validated (`IR-VERIFY-033` rejects `arg_count>0` with `args=NULL`), preventing malformed-IR null-dereference risk during verification.
- 2026-04-03: Milestone E large-slice coverage expansion landed: declaration-aware IR regressions now lock repeated declaration merge behavior and declared-call CFG lowering paths (including call-driven `if` conditions), while verifier regressions now lock duplicate-function-name rejection (`IR-VERIFY-027`) and declaration-only invariant failures for temps/non-parameter-locals (`IR-VERIFY-029`/`IR-VERIFY-030`).
- 2026-04-03: Milestone E verifier resilience hardening landed: verifier now guards malformed function/block payload tables (`IR-VERIFY-034/035/036`) and rejects non-canonical zero-arg call payloads (`IR-VERIFY-037`), with dedicated malformed-IR regression coverage.
- 2026-04-03: Milestone E declaration-aware matrix expansion landed: regressions now lock unnamed-declaration to named-definition merge behavior and declared-call lowering under while-condition CFG paths, extending mixed callable+CFG coverage beyond if-only cases.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regressions now also lock declared-call behavior under logical short-circuit condition lowering, logical value materialization, and ternary value lowering, reinforcing call+CFG interactions across branch-producing expression families.
- 2026-04-03: Milestone E verifier metadata-consistency expansion landed: verifier now rejects malformed count/capacity relationships for locals/blocks/instructions (`IR-VERIFY-038/039/040`) before deep traversal, and regression coverage now locks those invariant failures explicitly.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regression matrix now also locks declared-call behavior in ternary branch-context conditions, extending call+CFG interaction coverage for branch-producing ternary forms.
- 2026-04-03: Milestone E verifier metadata-consistency expansion landed: verifier now also enforces program-level function table consistency (`IR-VERIFY-041/042` for function_count/capacity and NULL function-table payload violations), with dedicated malformed-program regression locks.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regression matrix now locks declared-call behavior in comma-expression sequencing, extending declaration-call coverage beyond branch-only expression contexts.
- 2026-04-03: Milestone E global-symbol slice landed: IR data model now includes first-class globals (`IR_VALUE_GLOBAL`, global symbol table in `IrProgram`), lowering now records top-level declarations as globals (with constant initializer evaluation and dependency support on prior initialized globals), and expression lowering now resolves identifier lvalues as local-first then global (enabling global `=`, compound assignment, and prefix/postfix `++/--` in function bodies).
- 2026-04-03: Milestone E global-verifier/dump coverage landed: verifier now validates global value references and global-table metadata/name/id invariants (`IR-VERIFY-043..047`), dump output now emits `global ...` declarations, and IR regression/verifier suites were expanded to lock global read/write/update/shadowing behavior plus malformed-global metadata cases.
- 2026-04-03: Milestone E runtime-global-init slice landed: non-constant top-level initializers are now lowered into a reserved internal IR function (`__global.init`) that evaluates initializer expressions at runtime and writes results into target globals in source order.
- 2026-04-03: Milestone E runtime-init integration landed: when runtime global initialization is present, lowering now injects a startup call from lowered `main` entry to `__global.init`, and verifier now enforces constant/runtime initializer-flag exclusivity on globals (`IR-VERIFY-048`) with dedicated regression locks.
- 2026-04-03: Milestone E runtime-init entrypoint fallback landed: when runtime global initialization exists but no lowered `main` definition is available, lowering now auto-exports `__program.init` that calls `__global.init` once and returns, replacing the previous hard error path.
- 2026-04-03: Milestone E global-init dependency hardening landed: runtime top-level initializer lowering now collects referenced global dependencies and rejects initializers that depend on globals before those globals are initialized (`IR-LOWER-022`), preventing self/loop-style uninitialized dependency chains.
- 2026-04-03: Milestone E global-operator matrix expansion landed: IR regressions now explicitly lock global shift/bitwise compound assignment paths and runtime-global-init dependency rejection behavior in addition to existing global read/write/update coverage.
- 2026-04-03: Milestone E runtime-init verifier contract hardening landed: verifier now enforces that runtime global initialization implies a valid `__global.init` helper and a startup entry call path from either `main` entry or `__program.init` (`IR-VERIFY-049/050`), with dedicated malformed-IR regression locks.
- 2026-04-03: Milestone E runtime-init contract tightening landed: verifier now enforces fixed internal-init function shapes (`__global.init` / `__program.init` must be zero-parameter, zero-local, single-block, `ret 0`) and `__program.init` entry must begin with `call __global.init()` (`IR-VERIFY-051..054`), with dedicated no-main fallback tamper negative coverage.
- 2026-04-03: Milestone E runtime-init contract tightening follow-up landed: verifier now additionally requires `__program.init` entry to contain exactly one instruction before `ret 0` (`call __global.init()` only), and rejects reserved internal helper names when runtime global initialization is absent (`IR-VERIFY-055/056`), with dedicated malformed-IR negative locks.
- 2026-04-03: Milestone E runtime-global dependency closure landed: top-level runtime initializer dependency analysis now traverses function-body call chains (with scope-aware global-read collection) instead of only direct initializer AST identifiers, so indirect reads of not-yet-initialized globals are rejected under existing `IR-LOWER-022` policy; IR regressions now include indirect-call dependency negative coverage.
- 2026-04-03: Milestone E runtime-global dependency precision follow-up landed: function-body dependency collection is now reachability-aware for obvious dead-code shapes (statement fallthrough after `return`/`break`/`continue`, constant-condition pruning for `if(1)` / `if(0)` / `while(0)` / `for(...;0;...)`), reducing `IR-LOWER-022` false positives from syntactic-only traversal while keeping conservative behavior for non-constant loop reachability.
- 2026-04-03: Milestone E runtime-global dependency diagnostics follow-up landed: lowering now predeclares global symbol slots before initializer lowering and, on `IR-LOWER-022` failures, attempts dependency-graph cycle reconstruction to emit concrete chain diagnostics (for example `a -> b -> a`) when the failure is cycle-driven; regression coverage now locks this chain output.
- 2026-04-04: Milestone E runtime-global-init error-contract alignment landed: overflow/unsupported-constant top-level initializer forms now mark `invalid_constant` and attempt runtime-init fallback instead of surfacing a dedicated top-level overflow code; `IR-LOWER-023` is retired from active diagnostics, while hard-stop behavior remains on `IR-LOWER-018` (for example divide/mod by zero or unsupported constant operator), `IR-LOWER-019` (shift-count out of range), and `IR-LOWER-022` (not-yet-initialized global dependency).
- 2026-04-04: Milestone E runtime-global cycle diagnostics upgrade landed: `IR-LOWER-022` cycle output is now callee-aware by stitching per-edge dependency paths (for example `a -> read_b() -> b -> read_a() -> a`) instead of only global-level node chains; IR regression coverage now locks this callee-aware cycle-chain form.
- 2026-04-04: Milestone E verifier value-legality hardening slice landed: verifier now enforces that `IR_INSTR_BINARY` and `IR_INSTR_CALL` results must be temporaries (`IR-VERIFY-057/058`), preventing malformed IR from writing compute/call results directly into non-temp destinations; verifier regressions now include dedicated negative locks for both constraints.
- 2026-04-04: Milestone E callable-boundary hardening slice landed: call lowering now performs pre-emit callable validation against the IR function symbol table and rejects malformed call sites early with source-local diagnostics (`IR-LOWER-025` unknown callee, `IR-LOWER-026` arity mismatch) instead of relying only on post-lowering verifier checks; IR regressions now include semantic-skipped negative locks to keep these lowering guardrails stable.
- 2026-04-04: Milestone E callable declaration/definition boundary follow-up landed: IR regressions now also lock lowering-side declaration/definition interaction guardrails under semantic-skipped mode (`IR-LOWER-020` declaration arity mismatch and `IR-LOWER-021` duplicate function body lowering), ensuring these safety checks remain enforced even when semantic gating is bypassed.
- 2026-04-04: Milestone E/E7 regression matrix expansion slice landed: declaration-aware call coverage now explicitly includes `for` condition + step call paths (for example `for(;pred(x);x=step(x))`) so mixed control-flow/call lowering remains stable beyond existing `if`/`while`/ternary/comma call-shape locks.
- 2026-04-04: Milestone E/E7 regression matrix follow-up landed: declaration-aware `for` coverage now also locks init-position call lowering (for example `for(x=init(x);pred(x);x=step(x))`), extending mixed control-flow/call stability from condition/step-only to full init+condition+step call shapes.
- 2026-04-04: Milestone E/E7 regression matrix expansion follow-up landed: declaration-aware call coverage now also locks logical-OR short-circuit shapes in both condition and value contexts (for example `if(pred(x)||pred(0))` and `return pred(x)||pred(1)`), plus standalone `for` init-call lowering (`for(x=init(x);x;x=x-1)`) to close remaining mixed short-circuit/loop call-form gaps.
- 2026-04-04: Milestone E/E7 regression matrix large-batch follow-up landed: declaration-aware `for` call coverage now additionally locks logical-AND and logical-OR loop-condition short-circuit forms plus comma-expression call shapes in both step and init positions (for example `for(;pred(x)&&gate(x);x=step(x))`, `for(;pred(x)||gate(x);x=step(x))`, `x=(step1(x),step2(x))`, `x=(init1(x),init2(x))`).
- 2026-04-04: Milestone E/E7 regression matrix completion slice landed: declaration-aware mixed control-flow/call coverage now also locks nested short-circuit compositions in value/condition/loop contexts (for example `(pred(x)&&gate(x))||tail(x)` and `if((pred(x)||gate(x))&&tail(x))`), closing the remaining nested-mix edge cases identified for E7.
- 2026-04-04: Milestone E callable-boundary policy follow-up landed: verifier now enforces reserved internal initializer helper call-site policy (`IR-VERIFY-059/060`): `__global.init` is only legal at startup entry call sites (`main` entry or `__program.init` entry), and `__program.init` is rejected as a normal in-body call target; verifier tests now include both acceptance and negative locks for these policy boundaries.
- 2026-04-04: Milestone E/E8 verifier metadata-consistency slice landed: verifier now rejects duplicate global symbol entries (`IR-VERIFY-062`) so malformed IR with colliding global names cannot pass structural checks; verifier coverage now includes dedicated negative lock for duplicate global-name collisions.
- 2026-04-04: Milestone E/E8 verifier symbol-namespace consistency slice landed: verifier now rejects function/global symbol-name collisions (`IR-VERIFY-063`) so malformed IR cannot bind one identifier to both a function entry and a global entry in the same program namespace; verifier coverage now includes dedicated negative lock for this collision shape.
- 2026-04-04: Milestone E runtime-global dependency precision follow-up landed: loop dependency collection now uses loop-control reachability effects to prune dead post-loop paths and unreachable `for` step reads (for example `while(1){return ...}` and `for(;1;step){return ...}` no longer over-collect), while preserving reachable-step dependencies via `continue`; IR regressions now include both dead-loop acceptance and live-step-via-continue negative locks.
- 2026-04-04: Milestone E runtime-global dependency precision guard follow-up landed: IR regressions now additionally lock `for`-step reachability boundaries for unconditional `break` (step dead, should not count) versus infinite-loop fallthrough (step live, should count), reducing future risk of both over-pruning and under-pruning around `IR-LOWER-022` loop-step dependency collection.
- 2026-04-04: Milestone E runtime-global dependency nested-loop guard follow-up landed: IR regressions now lock constant-true `while` nested-branch loop-exit precision in both directions (`if(0) break` keeps post-loop reads dead; `if(1) break` keeps post-loop reads live), further reducing `IR-LOWER-022` false positives without hiding real reachable dependencies.
- 2026-04-04: Milestone E runtime-global dependency inner-loop control guard follow-up landed: regressions now additionally lock that inner-loop `break` does not incorrectly make outer-loop post-read/step paths live unless outer control actually exits via outer `break`/`continue` (outer-break keeps step dead, outer-continue keeps step live), tightening `IR-LOWER-022` precision around nested loop-control composition.
- 2026-04-04: Milestone E runtime-global dependency consistency guard follow-up landed: regressions now explicitly lock that unreachable `continue` edges in constant-true loops do not leak into post-loop dependency reachability (`while(1){if(0) continue; return 1;} return b;` keeps post-loop `b` dead), reinforcing alignment between dependency collection and diagnostic-path tracing.
- 2026-04-04: Roadmap transition checkpoint: Milestone E is marked closed for current scope (canonical IR bootstrap + hardening complete), and active roadmap focus is switched to Milestone F pass-stage work on canonical IR.
- 2026-04-05: Milestone F first slice landed: added `src/ir_pass/` scaffold with pass-pipeline entrypoints, dedicated `test-ir-pass` coverage, and a first safe immediate-binary constant-folding pass that folds arithmetic/bitwise/compare/shift cases only when UB-sensitive boundaries (overflow, divide/mod by zero, invalid shifts) are avoided.
- 2026-04-05: Milestone F follow-up slice landed: default pass pipeline now also removes dead temp definitions for pure `mov`/`binary` instructions and compacts surviving temp IDs afterward so pass output continues to satisfy canonical IR verifier contracts.
- 2026-04-05: Milestone F follow-up slice landed: default pass pipeline now also performs temp-copy propagation for `mov`-defined temps, enabling nested fold chains to collapse further before dead-temp cleanup while preserving verifier validity.
- 2026-04-05: Milestone F structural follow-up landed: split oversized `src/ir_pass/ir_pass.c` into focused include fragments (`core`, `temp_analysis`, `fold`, `copy`, `dce`, `pipeline`) and taught the build to track them explicitly, keeping pass-layer growth aligned with the repository's existing split-file pattern.
- 2026-04-05: Milestone F1 first utility slice landed: `ir_pass` now builds a shared internal temp-analysis view (`use_counts` plus `mov`-copy facts) that is reused by both temp-copy propagation and dead-temp elimination instead of maintaining separate ad hoc scans.
- 2026-04-05: Milestone F1/F2 follow-up slice landed: shared `ir_pass` temp analysis now also derives recursive temp-constant facts from `mov`/safe-binary def chains, and the default pipeline now runs a dedicated temp-constant propagation pass so longer constant-temp chains collapse without relying on repeated fold/copy alternation alone.
- 2026-04-05: Milestone F2 CFG cleanup slice landed: the default pass pipeline now simplifies branch terminators whose condition is already immediate (or whose successors are identical) into `jmp`, then removes unreachable blocks with block-id/target remapping so canonical IR verifier reachability and dense temp-ID contracts remain preserved after CFG cleanup.
- 2026-04-05: Milestone F2 CFG cleanup follow-up landed: CFG simplification now also threads targets through instruction-free trampoline blocks that only `jmp` onward, allowing branch/jump successors to bypass empty forwarding blocks before unreachable-block compaction removes them.
- 2026-04-05: Milestone F2 CFG cleanup follow-up landed: CFG simplification now also folds `jmp` terminators directly into `ret` when the jump target is an instruction-free return block, so constant-branch cleanup can collapse all the way to a single returning block before dead-block compaction.
- 2026-04-05: Milestone F2 CFG cleanup follow-up landed: CFG simplification now also merges a `jmp` directly into its target block when that target has exactly one predecessor, allowing nonempty fallthrough blocks to collapse into their only jumping predecessor before the usual unreachable-block cleanup.
- 2026-04-05: Milestone F1 utility follow-up landed: CFG cleanup now consumes a shared internal CFG-analysis view (`reachable` plus predecessor counts) instead of ad hoc rescans, separating reusable graph facts from the transform logic and preparing later block-level passes to share the same analysis surface.
- 2026-04-05: Milestone F2 fold follow-up landed: the existing binary-fold pass now also applies conservative single-immediate identity reductions (`+0`, `-0`, `*0/*1`, `/1`, `%1`, `&0/&-1`, `|0`, `^0`, `<<0`, `>>0`), letting non-constant expression chains collapse into copy/constant opportunities without crossing UB-sensitive arithmetic edges.
- 2026-04-05: Milestone F2 fold follow-up landed: the same fold pass now also simplifies safe same-operand binaries (`x-x`, `x^x`, `x&x`, `x|x`, and reflexive comparisons such as `x==x`, `x<x`, `x>=x`), so redundant value-preserving chains can collapse even when neither side is immediate.
- 2026-04-05: Milestone F1 utility follow-up landed: `ir_pass` now has a shared internal instruction side-effect classifier, and pass regressions explicitly lock that dead-result `call` instructions survive both direct DCE and the default optimization pipeline even when surrounding pure work is removed.
- 2026-04-05: Milestone F1 soundness follow-up landed: shared temp analysis now treats cross-block multi-definition temps as ambiguous instead of silently overwriting one definition with another, so temp-copy and temp-constant propagation remain conservative on verifier-legal join-temp materialization patterns.
- 2026-04-05: Milestone F1 utility follow-up landed: shared temp analysis now also materializes explicit temp definition-count and unique def-site facts, so later def/use-driven transforms can build on stable analysis surfaces instead of ad hoc per-pass scans or hidden state in temp-definition payloads.
- 2026-04-05: Milestone F1/F2 utility follow-up landed: `ir_pass` now exposes a shared `IrValueRef` equality helper, and CFG simplification uses it to fold branch diamonds whose two instruction-free successors `ret` the same non-temp value directly into a local `ret`.
- 2026-04-05: Milestone F2 CFG follow-up landed: same-return diamond folding now also covers equal temp return values when both successor blocks are instruction-free, relying on existing verifier availability guarantees that make those temps valid at the branch block exit too.
- 2026-04-05: Milestone F1/F2 follow-up landed: CFG same-return folding now compares successor return values through shared temp-analysis equivalence (safe constant and copy resolution), so direct `simplify_cfg` can collapse diamonds whose empty return successors are equivalent even when they do not return literally identical temps or immediates.
- 2026-04-05: Milestone F1 utility follow-up landed: shared temp analysis now also records explicit unique use-site facts (operand kind plus site location), and temp-copy propagation consumes those facts to rewrite single-use temp copies directly before falling back to full-function scanning for multi-use temps.
- 2026-04-05: Milestone F2 pipeline follow-up landed: the default pipeline now revisits `simplify-cfg` after the first dead-temp cleanup pass, then reruns dead-temp elimination, so branch-diamond folds that only become available after temp propagation plus DCE empty return successors are no longer left behind.
- 2026-04-05: Milestone F2 CFG follow-up landed: direct `simplify-cfg` now also folds same-return diamonds whose two successor blocks each contain a single pure `mov` feeding the `ret`, as long as the moved values resolve to the same branch-exit-available value under existing temp-equivalence rules, so these cases no longer need propagation plus DCE before collapsing.
- 2026-04-05: Milestone F2/F3 soundness follow-up landed: dead-temp elimination no longer treats all binary ops as silently erasable, preserving dead-result `div`/`mod`/shift instructions that may still enforce runtime validation, while binary identity folding no longer erases shift-by-zero operations whose validity depends on operand constraints; pipeline regressions now lock the reviewer-reported `a/0; return 0;` and `(a/0)*0` shapes.
- 2026-04-05: Milestone F3 stability follow-up landed: `ir_pass` regressions now explicitly lock representative default-pipeline fixed-point behavior by dumping the IR after one default run and after a second run on the same program, requiring identical output plus verifier success across fold/DCE cleanup, dead-result call preservation, and post-DCE CFG cleanup shapes.
- 2026-04-05: Milestone F3 regression follow-up landed: default-pipeline regressions now also lock two pipeline-level semantic guardrails that were previously only covered at individual-pass scope, namely that CFG cleanup plus DCE still preserves dead-result `call` instructions on taken branches, and that multi-definition join-temp cases preserve path distinction under the full pipeline instead of collapsing to one path's constant/copy fact.
- 2026-04-05: Milestone F3 canonical-form follow-up landed: selected default-pipeline guardrails now also lock the exact post-pass IR dump for stable representative shapes, including branch-local dead-result call cleanup, multi-def constant-path preservation, and post-DCE same-return collapse to `ret 9`, so future cleanup changes must be explicit about canonical-form drift rather than only preserving coarse counts or verifier success.
- 2026-04-05: Milestone F3 stepwise follow-up landed: pass regressions now also run the current default pass order one step at a time on representative high-risk shapes, requiring verifier success after every individual pass and preserving key invariants such as side-effecting call count and multi-def branch distinction throughout the sequence rather than only at the final pipeline output.
- 2026-04-05: Milestone F3 canonical-form coverage follow-up landed: default-pipeline exact-dump locks now additionally cover same-temp branch folding collapsing to a single `ret 7` block and single-predecessor nonempty successor merge collapsing to a single `add` plus `ret` block, broadening the current notion of acceptable canonical endpoints beyond only side-effect/cfg-cleanup shapes.
- 2026-04-05: Milestone F3 API-contract follow-up landed: `ir_pass_run_pipeline(...)` now verifier-checks its input IR before running passes and verifier-checks pass output after every pass, so malformed custom-pass pipelines fail at the API boundary instead of only being caught by a separate verifier call later.
- 2026-04-05: Milestone F3 diagnostic follow-up landed: pipeline-level verifier hook diagnostics were renumbered to dedicated `IR-PASS-034/035` codes so they no longer collide with temp-copy propagation's existing `IR-PASS-015/016` error contract.
- 2026-04-05: Milestone F3 diagnostic follow-up landed: remaining cross-pass `IR-PASS` code collisions were cleaned up too, with dead-temp elimination's temp-use-table overflow moving to `IR-PASS-017` and CFG simplify's malformed-block-table check moving to `IR-PASS-040`, leaving only one intentional same-meaning reuse (`IR-PASS-011` inside temp-analysis OOM paths).
- 2026-04-13: Memory-SSA phase M0 started as a separate sibling skeleton under `include/memory_ssa.h`, `src/memory_ssa/`, and `tests/memory_ssa/`. The landed slice currently covers manual-construction builders for tracked slots/blocks/phis/instruction-local memory accesses, a first verifier, a first dump surface, and isolated regression/verifier targets for straight local and join-phi authority shapes.
- 2026-04-13: Memory-SSA phase M1 has now also started in a deliberately narrow bridge form. `memory_ssa_build_from_value_ssa(...)` can now build from verifier-legal acyclic `value_ssa` functions, collecting tracked slots, threading per-slot memory versions through straight-line and diamond CFGs, and materializing join-block memory phis where predecessor slot versions differ. Loop/cyclic CFG families remain intentionally deferred beyond this first acyclic bridge slice.
- 2026-04-13: That first Memory-SSA bridge now also covers a representative cyclic family. `memory_ssa_build_from_value_ssa(...)` can now precreate per-slot memory phis, bind them on dominator-tree block entry, and fill predecessor-specific phi inputs on successor edges, which is enough to bridge straight-line, diamond, and a first loop-header local-slot shape from `value_ssa` into `memory_ssa`.
- 2026-04-13: The first Memory-SSA bridge now also has a real conservative effect rule for calls: calls may carry explicit global-slot memory access annotations that consume the current known global version and define a fresh killed global version, and the bridge is now regression-locked on a representative `store_global; call; load_global` shape so post-call global loads read the call-produced memory version rather than the pre-call store directly.
- 2026-04-13: The bridge now also seeds entry memory versions for tracked globals and tracked parameter locals, so conservative call/global modeling and parameter-slot loads no longer depend on a prior explicit store in the same function before any memory version exists. Representative regressions now lock both the seeded parameter-local load shape and the seeded global-call barrier shape.
- 2026-04-14: Memory-SSA now also has a first shared def/use analysis surface. `memory_ssa_compute_def_use_analysis(...)` materializes per-version defining slot ids, defining site kind/location, use counts, and explicit use-site slices across entry seeds, phi inputs, load uses, and call uses, with isolated analysis coverage under `tests/memory_ssa/memory_ssa_analysis_test.c`.
- 2026-04-14: That shared Memory-SSA surface now also has first narrow consumer queries above it: `memory_ssa_resolve_load_store_value(...)` can answer the direct store-to-load case when a load's reaching memory version is instruction-defined by a store, and `memory_ssa_is_trivially_dead_store(...)` can already expose the conservative zero-use local-store case without pretending to solve global observability or transitive phi/call deadness yet.
- 2026-04-14: Memory-SSA now also has a first sibling pass module under `include/memory_ssa_pass.h` and `src/memory_ssa_pass/`, instead of pushing Memory-SSA-aware rewrites straight into the existing `value_ssa_pass` compilation unit. The first landed slice is deliberately narrow: `memory_ssa_pass_forward_local_loads(...)` and `memory_ssa_pass_eliminate_dead_local_stores(...)`.
- 2026-04-14: Those first Memory-SSA-backed passes are now regression-locked on representative CFG shapes that the older straight-chain heuristics did not cover well: local-load forwarding across a join with one preserved reaching store-version and branch-exit dead local-store cleanup when no later load reads the slot. They remain standalone consumers for now rather than being folded into the default Value-SSA canonicalization pipeline yet.
- 2026-04-14: Memory-SSA load-value resolution now also recurses through same-value memory-phi joins instead of stopping at “direct store only”. Representative pass and analysis coverage now lock both the positive same-value-phi case and the negative differing-value/call-barrier cases, so Memory-SSA-backed load forwarding can cross a small class of real join blocks without pretending to solve arbitrary cyclic memory reasoning.
- 2026-04-14: Memory-SSA store annotations now also record the overwritten incoming memory version when one exists, so `store_*` no longer acts like a pure def-only leaf in the Memory-SSA graph. The debug dump now prints that edge explicitly as `mem.new = store_* slot @ mem.old, value`.
- 2026-04-14: That extra store-side Memory-SSA edge now has a first direct consumer too: `memory_ssa_pass_eliminate_redundant_stores(...)` can remove same-value `store_local` / `store_global` instructions even across join shapes when the incoming memory version recursively resolves to the same value. Representative pass coverage now locks positive same-value-join cases and negative call-barrier cases.
- 2026-04-14: Memory-SSA dead-store querying is no longer local-only. `memory_ssa_is_trivially_dead_store(...)` now treats any store whose defining memory version has zero uses as dead, including globals, with existing call barriers and later loads naturally keeping the relevant memory version live.
- 2026-04-14: That generalized dead-store fact now has a sibling pass consumer too: `memory_ssa_pass_eliminate_dead_stores(...)` can erase zero-use `store_local` and zero-use `store_global` instructions, while representative pass coverage now also locks both the positive dead-global case and the negative “store before call/load stays live” global case.
- 2026-04-14: Memory-SSA dead-store reasoning is now also stronger than pure zero-use checking. A store whose defining memory version is used only as the overwritten prior-version edge of later stores is now treated as dead too, so overwritten-store cleanup no longer disappears just because store-to-store edges are modeled explicitly in Memory-SSA.
- 2026-04-14: `memory_ssa_pass` now also has a first explicit mini-pipeline entrypoint, `memory_ssa_pass_run_pipeline(...)`, which currently sequences forwarding, redundant-store cleanup, dead-store cleanup, and a final forwarding revisit. Representative coverage now locks a mixed combo shape where the pipeline legitimately deletes every redundant store and leaves only the surviving branch plus `mov 7; ret`.
- 2026-04-14: Memory-SSA-backed pass coverage now also includes representative loop families, not only joins. Current authority explicitly locks same-value loop-carried local/global cases where forwarding succeeds through the loop-header memory phi, the negative differing-value loop case where forwarding stays conservative, and a same-value loop pipeline case where repeated cleanup legitimately strips all redundant stores and leaves only loop control plus `mov 7; ret`.
- 2026-04-14: `memory_ssa_pass_run_pipeline(...)` is now also verifier-gated and fixed-point-oriented rather than one-shot best effort. It rejects verifier-invalid input up front, reruns the current pass order until the dumped Value-SSA program stops changing, and reports nonconvergence if that does not happen within the current bounded iteration budget. Dedicated pass coverage now locks both fixed-point stability across repeated runs and invalid-input rejection.
- 2026-04-14: `memory_ssa_pass` has now also been physically split into focused implementation fragments (`core`, `load_forward`, `store_cleanup`, `pipeline`) instead of continuing to grow as one large compilation unit. The Makefile now tracks those include fragments explicitly so future pass growth stays modular and test rebuilds remain precise.
- 2026-04-14: Memory-SSA dead-store reasoning now also propagates through non-observing phi chains instead of stopping at direct store-to-store edges. Representative analysis and pass coverage now lock both branch-phi-overwrite and loop-overwrite local/global families, where the intermediate memory phi is itself only feeding later overwrites and therefore does not keep the earlier stores alive.
- 2026-04-14: `memory_ssa_pass_run_pipeline(...)` now also hands exposed constants off to a short existing Value-SSA arithmetic cleanup tail instead of stopping at memory-only rewrites. Representative pipeline coverage now locks a concrete case where Memory-SSA forwarding turns a `load_local` into `7`, and the post-tail value cleanup finishes the job all the way to `ret 12`.
- 2026-04-14: `memory_ssa_pass_run_pipeline(...)` now also performs a small Value-SSA cleanup tail after the Memory-SSA-specific fixed point, specifically trivial-value simplification plus dead value-def cleanup. Representative pipeline outputs are now locked against that stronger post-pass canonical form, for example collapsing the previous `ssa.N = mov 7; ret ssa.N` endpoints to direct `ret 7` where appropriate.
- 2026-04-15: `memory_ssa_pass_run_pipeline(...)` now takes its fixed-point boundary across the full memory-plus-value cleanup sequence instead of only the Memory-SSA-specific prefix. The current tail also includes `simplify_cfg`, so a branch condition exposed by load forwarding can now collapse a dead path, let the next Memory-SSA iteration rebuild on the simplified CFG, and unlock later forwarding that was previously stuck behind a differing-value join or dead-path global call barrier. Representative pass coverage now locks both local and global “CFG revisit unlocks the next memory cleanup” shapes, while existing exact-dump pipeline expectations were tightened to the stronger canonical endpoints (`ret 7`, `ret 12`, and the loop/call-barrier infinite-loop forms where the exit path was already unreachable).
- 2026-04-15: Memory-SSA-backed load forwarding is now also able to reuse an earlier dominating load of the same memory version, not only concrete store-resolved values. That means entry-seeded parameter locals and post-call global versions can still shed repeated loads when the first load already materialized the value into SSA. Representative pass coverage now locks both a parameter-local diamond case (`load_local a.0` in entry reused at the join) and a post-call global case where a second `load_global g.0` across a straight jump is rewritten to `mov` of the first load; the pipeline coverage also locks the resulting canonical endpoint (`call touch(); load_global g.0; ret ...`) after trivial cleanup and block merge.
- 2026-04-15: Memory-SSA-backed load forwarding is now also able to synthesize a join phi for unknown repeated loads, not only reuse one dominating load. When a join-block `load_local` / `load_global` reads a memory version whose value is still unknown, but every predecessor already has some SSA value equivalent to reading that same slot from that same version, the pass now inserts a small value-phi in the join block and rewrites the load to `mov` of that phi. Representative pass coverage now locks both the direct-load diamond shapes and a stronger parameter-local case where one predecessor wraps the load in `mov` before the join, proving the join forwarder now consumes predecessor-side SSA glue rather than only raw load instructions.
- 2026-04-16: Memory-SSA-backed load forwarding now also has a first narrow PRE-style slice on acyclic joins. If a join-block load reads one memory version and some predecessors already materialize an equivalent value while others do not, the pass may now conservatively insert missing predecessor-edge loads and still rewrite the join load to a phi of predecessor values instead of giving up. Current authority intentionally excludes loop-header/backedge families; representative pass coverage now locks both a parameter-local case and a post-call global case where only one predecessor had already loaded the value and the other predecessor now gets a synthesized edge load.
- 2026-04-16: Memory-SSA-backed load forwarding is now also able to reuse an already-materialized dominating equivalent value, not only a raw dominating load or a freshly synthesized phi at the current join. If a dominating scope already holds some SSA value equivalent to reading the same slot from the same memory version, later loads may now rewrite directly to that existing value even when it is glue such as `mov(phi(load, load))`. Representative pass coverage now locks both parameter-local and global join families where a join block already contains the equivalent phi/mov glue and a later load in that same block now reuses it instead of rebuilding another phi or reloading.
- 2026-04-16: Memory-SSA now also has a first explicit local-slot promotion pass instead of leaving all such behavior as an implicit side effect of generic load forwarding. `memory_ssa_pass_promote_local_slots(...)` rewrites `load_local` to use a dominating value-phi when the corresponding memory version is defined by a dominating memory-phi whose inputs can already be expressed as SSA values. Representative pass coverage now locks a loop-local family where the exit load is promoted to `mov` of a header phi (`phi [7, 9]`) before the later general cleanup stages run.
- 2026-04-16: That first local-slot promotion pass now also handles join-block loads directly, not only later dominated loads. If a join block's own `load_local` reads a memory version defined by the same block's memory-phi, and the incoming memory versions are already SSA-expressible, `memory_ssa_pass_promote_local_slots(...)` may now materialize a value-phi in that join block and rewrite the load to `mov` of the new phi. Representative pass coverage now also locks a direct local-join family with predecessor stores `1/2` and a join load promoted to `phi [1,2]`.
- 2026-04-15: Redundant-store cleanup is now also able to recognize “write back the exact value you just loaded from this same memory version” even when that value is unknown. Concretely, `memory_ssa_pass_eliminate_redundant_stores(...)` now tracks store operands back through trivial `mov` chains to a dominating `load_local` / `load_global`, and if that load read the same slot from the same incoming memory version the store is erased as a no-op. Representative pass coverage now locks both a parameter-local entry case (`load_local a.0; store_local a.0, ssa.0`) and a post-call global case (`call touch(); load_global g.0; store_global g.0, ssa.0`), while pipeline coverage now also locks the chained canonical endpoint where that dead store disappears and the remaining block simplifies to `call touch(); load_global g.0; ret ssa.0`.
- 2026-04-15: Memory-SSA call modeling is no longer forced to treat every internal call as an all-global kill. The bridge now computes a conservative transitive global read/write summary for each internal `value_ssa` function and uses it to annotate calls per tracked slot: untouched globals get no access, read-only globals keep a use-only observation edge, and writeable globals still receive a fresh def-version kill. Declaration-only/unknown callees stay conservative all-global barriers. Representative bridge and pass coverage now lock both a pure internal helper that no longer kills `g.0` and an internal helper that reads only `h.1`, where a caller's later `load_global g.0` can still forward through the call because `g.0` is no longer spuriously modeled as clobbered.
- 2026-04-15: Redundant-store cleanup now also treats phi-fed values by semantic equivalence to the same slot/version, not only by exact load-site identity. If every phi input is itself just “the value currently read from this slot at this incoming memory version”, the store is erased as a no-op write-back even when the phi merges sibling loads from different predecessor blocks. Representative pass coverage now locks both the earlier same-source-phi case and a stronger diamond case where each predecessor does its own `load_local a.0`, the join phi merges those two loads, and `store_local a.0, phi` still disappears; pipeline coverage keeps the fully collapsed `ret 0` endpoint for that family too.
- 2026-04-15: The current Memory-SSA-backed cleanup family is now also exposed as its own first-class fixed-point pass, `memory_ssa_pass_canonicalize_memory_values(...)`, instead of only being an inlined prefix inside the larger experiment pipeline. That pass now owns the “memory-value normalization” slice: local/global forwarding, redundant/dead store cleanup, and the minimal SSA cleanup needed to stabilize the resulting shape (`simplify_trivial_values`, dead-value-def cleanup, `simplify_cfg`). Representative pass coverage now locks both a local join/write-back case that collapses all the way to `ret 0` and a post-call global chain that stabilizes at `call touch(); load_global g.0; ret ...`, and the full pipeline now consumes this pass as one named stage before the later arithmetic/value-only cleanup.
- 2026-04-16: Memory-SSA phi value resolution is now cycle-aware. `memory_ssa_resolve_version_value_recursive(...)` can now resolve loop-carried memory phis when all non-self-referential inputs agree on the same value, instead of unconditionally failing on any phi input that traces back through a cycle. This enables redundant store elimination and dead store elimination to work on loop-invariant slot values that the earlier single-pass resolver could not see through. A two-pass iterative wrapper (`memory_ssa_resolve_version_value_iterative(...)`) also handles indirect phi cycles (phiA → phiB → phiA) by resetting unresolved PHI versions and re-resolving with cached first-pass results. Representative analysis coverage now locks same-value loop resolution (positive), differing-value loop resolution (negative), loop-with-inner-diamond resolution, nested-loop resolution, and indirect phi-cycle resolution. Representative pass coverage now locks nested-diamond same-value pipeline collapse (to `ret 42`) and loop-with-inner-diamond same-value pipeline collapse (to infinite loop skeleton).
- 2026-04-16: Memory-SSA analysis now also exposes a public version-level value resolution API. `memory_ssa_resolve_version_value(...)` takes a memory version ID directly and returns the resolved value if one exists, without requiring callers to identify a specific load or store instruction site first. This decouples the resolver from instruction-site queries and makes it available to future consumers (loop-invariant analysis, cross-block value queries, custom pass heuristics). Representative analysis coverage now locks direct store-version resolution, same-value phi resolution, loop-invariant phi resolution, entry-version non-resolution, and differing-value phi non-resolution through the new public API.
- 2026-04-16: Memory-SSA analysis now also exposes the cross-layer SSA-value/memory-version equivalence queries that the pass layer had previously been keeping private. `memory_ssa_value_matches_slot_version(...)` answers whether an SSA value denotes the value of one slot at one specific memory version by tracing through mov chains, load instructions, and aligned value-phi / memory-phi structure; `memory_ssa_find_block_equivalent_value(...)` scans one block for an already-materialized SSA value with that meaning. `memory_ssa_pass` now delegates its memory-value matching logic to those public analysis APIs instead of carrying a separate private copy. Representative analysis coverage now locks both direct slot-version matching and block-local equivalent-value lookup through the public interface.
- 2026-04-16: That Memory-SSA analysis/public-API lift is now also complete at the implementation boundary, not only in interface shape. The last pass-private forwarding wrappers in `memory_ssa_pass_core.inc` have been removed, and the actual consumers in the load-forward/store-cleanup fragments now call the public analysis APIs directly. This closes the layering gap where documentation had already promoted the equivalence queries to analysis-layer authority but `memory_ssa_pass` still carried a thin private forwarding shim.
- 2026-04-16: Memory-SSA now also has a first true public one-shot consumer above its pass fragments. `memory_ssa_pass_build_canonicalized_from_lower_ir(...)` lowers a `LowerIrProgram` to Value-SSA and then runs the Memory-SSA-backed fixed-point pipeline before returning. This gives callers a memory-aware sibling to `value_ssa_build_canonicalized_from_lower_ir(...)` instead of forcing them to manually compose “convert, then run memory pipeline” in ad hoc test or consumer code. Representative pass coverage now locks a lower-IR local-join case where the new entrypoint collapses `store_local a.0, 7; ...; load_local a.0` all the way to `ret 7`.
- 2026-04-16: That lower-IR Memory-SSA bridge now also comes in explicit memory-only vs full-canonicalization tiers, matching the existing in-memory program APIs. `memory_ssa_pass_build_memory_canonicalized_from_lower_ir(...)` lowers to Value-SSA and stops after Memory-SSA-backed memory-value normalization, while `memory_ssa_pass_build_canonicalized_from_lower_ir(...)` continues through the later arithmetic/value-only cleanup tail. Representative pass coverage now locks the distinction on one lower-IR program: the memory-only entrypoint stabilizes at `ssa.0 = add 7, 5; ret ssa.0`, while the full entrypoint continues to `ret 12`.
- 2026-04-16: Value-SSA now also has a first public high-level consumer of that Memory-SSA lower-IR bridge instead of leaving the bridge only under `memory_ssa_pass`. `value_ssa_build_memory_value_canonicalized_from_lower_ir(...)` and `value_ssa_build_memory_canonicalized_from_lower_ir(...)` delegate to the corresponding Memory-SSA-backed lower-IR one-shot builders, so a Value-SSA-facing caller can now pick among the old classic canonicalized conversion, a memory-value-normalized conversion, or the stronger full memory-aware canonicalized conversion. Representative Value-SSA regression coverage now locks both a join-local case that collapses to `ret 7` only through the memory-aware entry and a fold case that distinguishes memory-value-only output (`add 7, 5`) from the full memory-aware endpoint (`ret 12`).
- 2026-04-16: That Value-SSA lower-IR surface now also has a unified policy-selecting entrypoint above the three named wrappers. `value_ssa_build_from_lower_ir_with_canonicalization(...)` takes a `ValueSsaLowerIrCanonicalizeMode` and dispatches to classic canonicalization, memory-value canonicalization, or full memory-aware canonicalization. The three named wrappers now delegate to this mode-based entrypoint, and representative regression coverage locks all three modes plus rejection of an unknown mode. The Value-SSA oracle also now consumes this mode-based lower-IR entrypoint directly for memory-aware semantic checks, so the higher-level caller surface is no longer only regression-checked by dump shape.
- 2026-04-16: That high-level Value-SSA lower-IR surface now also has a concrete default helper for ordinary callers. `value_ssa_build_default_from_lower_ir(...)` currently selects `VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL`, so production-style callers can ask for "the repository's preferred canonicalized Value-SSA output" without hardcoding policy at every call site. Representative regression coverage now locks that default helper on the fold family (`ret 12`), and the Value-SSA oracle also now consumes the default helper directly for a lower-IR semantic check, so the intended non-policy caller surface is exercised too.
- 2026-04-16: The explicit slot-promotion line now also has a global-slot sibling instead of leaving global promotion only as an internal fallback inside `forward_global_loads`. `memory_ssa_pass_promote_global_slots(...)` applies the same dominating-memory-phi rewrite to `load_global`, `memory_ssa_pass_canonicalize_memory_values(...)` now runs it before global load forwarding, and representative pass coverage now locks both positive same-value join/loop families and a negative call-barrier family where promotion intentionally does nothing.
- 2026-04-16: Slot promotion now also reuses dominating ancestor values while assembling memory-phi inputs, not only values materialized directly in the immediate predecessor block. If one phi input still carries an older slot version and the equivalent SSA value already exists in a dominating ancestor, `memory_ssa_pass_promote_local_slots(...)` / `memory_ssa_pass_promote_global_slots(...)` may now use that ancestor value instead of giving up. Representative pass coverage now locks both a parameter-local case and a global case where one predecessor writes back the entry-loaded value while the sibling predecessor carries the entry version unchanged, and the join load is still promoted to a value-phi.
- 2026-04-16: Slot promotion now also canonicalizes the trivial "all promoted inputs are identical" case on the spot instead of manufacturing a redundant value-phi and relying on later cleanup. When every assembled input value agrees, the promoted `load_local` / `load_global` now becomes a direct `mov` of that common value. Representative promotion coverage now locks same-value join/loop global families and ancestor-input local/global families at that stronger direct-`mov` endpoint.
- 2026-04-16: Memory-SSA now also has a first explicit local-slot scalar-replacement consumer above the earlier promotion/forward/store-cleanup pieces. `memory_ssa_pass_eliminate_redundant_local_stores(...)` exposes the local-only redundant-store subpass, and `memory_ssa_pass_scalar_replace_local_slots(...)` composes local promotion, local load forwarding, local redundant/dead store cleanup, and a small SSA cleanup tail to erase simple local-slot carrier skeletons directly. `memory_ssa_pass_canonicalize_memory_values(...)` now uses that higher-level local consumer before continuing with the remaining global/mixed memory cleanup. Representative pass coverage now locks both the local-only redundant-store entrypoint and a direct local-join scalar-replacement family that ends at a value-phi plus `ret`, with no surviving local stores or loads.
- 2026-04-16: That higher-level scalar-replacement line now also has a global-slot sibling instead of leaving the global half as loose individual steps inside the memory canonicalizer. `memory_ssa_pass_eliminate_redundant_global_stores(...)` and `memory_ssa_pass_eliminate_dead_global_stores(...)` now expose the global-only cleanup building blocks, `memory_ssa_pass_scalar_replace_global_slots(...)` composes the current global promotion/forward/cleanup family plus SSA cleanup, and `memory_ssa_pass_canonicalize_memory_values(...)` now consumes that global sibling directly after the local one. Representative pass coverage now locks the global-only redundant/dead store entrypoints and a nonconstant-condition global-join scalar-replacement family that stabilizes at `load_local c; br c; phi [7,9]; ret`, with no surviving global loads or stores.
- 2026-04-16: The scalar-replacement line now also has one combined public consumer above the local/global siblings. `memory_ssa_pass_scalar_replace_slots(...)` reruns the local-slot and global-slot scalar-replacement consumers to a shared fixed point, and `memory_ssa_pass_canonicalize_memory_values(...)` now consumes that combined entrypoint instead of spelling out the two halves directly. Representative pass coverage now also locks a mixed local/global join family where one branch writes both `a.0` and `g.0`, the other writes alternate values, and the final program stabilizes at `load_local c; br c; phi(local); phi(global); add; ret` with no surviving slot loads/stores for either carrier.
- 2026-04-16: That combined scalar-replacement consumer is now also regression-locked on a stronger multi-predecessor mixed family instead of only the earlier two-way join. When one incoming path writes one `(local, global)` pair and a sibling path splits again into two alternative `(local, global)` pairs, `memory_ssa_pass_scalar_replace_slots(...)` now stabilizes at one three-input local phi, one three-input global phi, and the final arithmetic use, with no surviving carrier-slot loads/stores in the joined region.
- 2026-04-16: The same combined scalar-replacement consumer now also has a first mixed loop-exit authority family, not only acyclic joins. In a call-free loop where both one local slot and one global slot carry `(entry value, backedge value)` pairs and the exit path consumes both, `memory_ssa_pass_scalar_replace_slots(...)` now stabilizes with the carrier phis placed directly at the loop header and the exit block reduced to the arithmetic use plus `ret`, with no surviving local/global carrier loads or stores.
- 2026-04-16: That combined scalar-replacement consumer now also has a first mixed call-barrier authority family. When a local slot is still fully replaceable across a join but one global path crosses an unknown call, `memory_ssa_pass_scalar_replace_slots(...)` no longer needs to give up on the global half entirely: it may materialize one explicit post-call `load_global` on the blocked path, build a value-phi that merges that unknown edge with the known non-call edge, and still eliminate the original joined global load/store carrier skeleton around the arithmetic use.
- 2026-04-16: That same mixed call-barrier behavior is now also regression-locked on a stronger multi-predecessor family, not only a two-way join. When one joined path carries known local/global values, another sibling path carries a different known pair, and a third sibling path crosses an unknown call before joining, `memory_ssa_pass_scalar_replace_slots(...)` now stabilizes at a three-input local phi plus a three-input global phi whose barrier edge is an explicit post-call `load_global`, followed by the final arithmetic use.
- 2026-04-16: The combined scalar-replacement line is now also regression-locked on a partially blocked three-way mixed family where only the global half of one incoming edge is unknown. In that case `memory_ssa_pass_scalar_replace_slots(...)` continues to scalar-replace the local carrier fully across all three paths, while the global carrier still collapses to a three-input value-phi whose blocked edge is the explicit post-call `load_global`. This confirms the current layering is no longer all-or-nothing across slot kinds or across joined predecessor sets.
- 2026-04-16: The same partial-barrier behavior is now also regression-locked on a cyclic mixed family. In a loop where the local carrier remains fully known across the backedge but the global carrier passes through an unknown call on the backedge, `memory_ssa_pass_scalar_replace_slots(...)` now keeps the local carrier as a loop-header phi while still leaving the global half conservative at the loop exit (`load_global` after the barrier). This confirms the current mixed scalar-replacement layering remains stable in cyclic CFGs too, not only at acyclic joins.
- 2026-04-16: The next mainline has now started in a deliberately narrow form: the full Memory-SSA-backed pipeline now also runs `value_ssa_eliminate_redundant_binaries(...)` after scalar replacement and arithmetic normalization/identity cleanup, so repeated arithmetic over memory-derived SSA values can collapse without inventing a separate memory-aware GVN framework yet. Representative pass coverage now locks a join family where two repeated `add(load_local a, load_global g)` computations become one `add(phi(a), phi(g))` plus `ret`, proving the existing redundant-binary CSE can now act as a first memory-aware CSE stage once Memory-SSA has exposed stable SSA operands.
- 2026-04-16: That first memory-aware CSE stage now also has an explicit public consumer of its own. `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` packages slot scalar replacement together with the existing Value-SSA binary normalize/fold/identity/CSE tail, and representative pass coverage now locks the same repeated-memory-derived-`add` family both through this dedicated API and through the full Memory-SSA pipeline. So the current mainline is no longer only “pipeline happened to get smarter”; there is now a named narrow entrypoint for conservative memory-aware binary CSE.
- 2026-04-16: That narrow memory-aware binary-CSE entrypoint is now also regression-locked on a partially unknown global family rather than only a fully known join. In a three-way mixed join where one global edge crosses an unknown call, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` still collapses repeated `add(local, global)` computations after scalar replacement, reusing the local phi plus the partially unknown global phi (`known`, `known`, `post-call load`) instead of requiring every incoming edge to be fully concrete.
- 2026-04-16: That same narrow memory-aware CSE stage is now also able to canonicalize some joined arithmetic one step further than "keep `add(phi, phi)`". After a new value-exposure substage that leaves predecessor computations alive long enough to be reused, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` may now rewrite a joined binary into a phi of edge results when those edge results come either from predecessor-side binary computations or from per-edge immediate folding (`phi [11], [22]` instead of `add(phi [1,2], phi [10,20])`). This is still a deliberately narrow join-local rule, not a general GVN framework, but it is the first landed step in that direction.
- 2026-04-17: That join-local memory-aware CSE rule now also handles mixed per-edge sources instead of choosing one uniform strategy for the whole join. After edge-local operand normalization, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` may now reuse a predecessor-side binary on one partially unknown edge while still constant-folding sibling known edges, producing joined results such as `phi [11], [22], [add(load_global g.0, 3)]` instead of falling back to `add(phi, phi)`. The full `memory_ssa_pass` pipeline now also runs this stronger memory-aware CSE stage before its final dead-value/CFG cleanup, so predecessor-side arithmetic that enables the rewrite is not deleted too early. Representative direct-pass and full-pipeline coverage now both lock that partial-unknown predecessor-binary family.
- 2026-04-17: That same join-local memory-aware CSE line now also recurses one layer through same-block join-local SSA glue instead of stopping at block-entry phis only. When a join block computes one joined binary and then immediately feeds it into a later safe `mov`/`binary` in the same block, predecessor projection can now look through that local definition and recover final per-edge results (`phi [16], [27]` from `sum = add(phi, phi); add sum, 5`) rather than leaving the second expression as `add(phi, 5)`. Representative direct-pass and full-pipeline coverage now both lock that nested joined-binary family.
- 2026-04-17: That join-local memory-aware CSE line now also has a first narrow predecessor-materialization slice instead of requiring every reusable edge result to already be present. If one predecessor jumps directly to the join and the projected operands for a safe binary are already available on that edge, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` may materialize the missing predecessor-side binary and reuse it on the next fixed-point iteration. This strengthens the earlier partial-unknown global-barrier family from `add(phi, phi)` all the way to `phi [11], [22], [add(load_global g.0, 3)]`, and representative direct-pass/full-pipeline coverage now also locks a stronger nested family where the same mechanism materializes both `add(load_global g.0, 3)` and then `add(<that>, 5)` on the unknown edge before the join collapses to `phi [16], [27], [...]`.
- 2026-04-17: That same predecessor-materialization slice now also applies to direct branch predecessors, not only plain `jmp -> join` blocks. If a branching predecessor has one edge to the join and the projected operands for a safe binary are already available in that predecessor, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` may now materialize the binary before the branch terminator and still reuse it on the join edge. Representative direct-pass/full-pipeline coverage now locks a partial-unknown global-barrier family where the blocked predecessor does `call; load_global; load_local e; br e, join, exit`, and the pass still strengthens the join to `phi [11], [22], [add(load_global g.0, 3)]` while preserving the non-join exit edge.
- 2026-04-17: That branch-predecessor materialization line is now also regression-locked on a stronger combined family rather than only a one-layer join result. In a blocked branch predecessor where the join path needs both `add(load_global g.0, 3)` and then `add(<that>, 5)`, the current fixed-point loop now materializes both safe binaries before the branch terminator and still collapses the join to one final result phi (`phi [16], [27], [add(add(load_global g.0, 3), 5)]`) while preserving the sibling non-join exit edge. This is mainly a coverage/logging step: it confirms the earlier branch-materialization and same-block recursive projection slices compose on one real CFG shape.
- 2026-04-17: That join-local projection/materialization logic no longer depends on one fixed-point iteration per missing predecessor-side binary. The blocked-edge projector can now recurse through same-block `mov` / safe `binary` glue, synthesize the needed predecessor-side chain immediately, and still finish the current join rewrite in the same pass. Representative direct-pass/full-pipeline coverage now locks a deeper branch-blocked family where the unknown edge materializes `add(load_global g.0, 3)`, then `+5`, `+7`, `+9`, `+11`, and `+13` before the branch terminator, while the join itself collapses straight to `phi [56], [67], [that_deep_chain]`.
- 2026-05-01: Slot-precise internal-call summaries now also have explicit mixed local/global authority above the earlier scalar-replacement layer. In a three-way mixed join where the helper edge writes `a.0=3`, `g.0=30`, then calls an internal helper that only reads `h.1`, both `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and the full `memory_ssa_pass_run_pipeline(...)` now collapse two repeated joined `add(load_local a.0, load_global g.0)` computations all the way to `phi [11], [22], [33]` instead of preserving a fake `g.0` barrier on the helper edge. This extends the current `M2` authority from mixed scalar-replacement-only coverage into mixed helper-call memory-aware CSE and pipeline coverage as well.
- 2026-05-01: That same slot-precise helper-call authority now also covers the current recursive join-local CSE/materialization slice rather than stopping at the shallow mixed join. In representative `materialized` and `branch-materialized-nested` families, the helper edge writes `a.0=3`, `g.0=30`, then calls an internal helper that only reads `h.1`; after that, both `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and the full pipeline now collapse the joined arithmetic all the way to direct per-edge constants such as `phi [16], [27], [38]`, without preserving a synthetic helper-edge `load_global g.0`, `add g.0, 3`, or `add(add(g.0, 3), 5)` chain. This pushes `M2` authority one step deeper: precise helper summaries now reach the first recursive predecessor-materialization / branch-materialization families too, not only the earlier shallow mixed helper-call case.
- 2026-05-01: That same blocked-edge helper-call coverage now also spans both ends of the current branch-materialized family rather than only the earlier recursive middle slice. In representative shallow branch-materialized and deep branch-materialized families, the helper edge again writes `a.0=3`, `g.0=30`, then calls the internal `helper()` that only reads `h.1`; after that, both the direct Memory-SSA-aware CSE entrypoint and the full pipeline now collapse the join to direct per-edge constants such as `phi [11], [22], [33]` and `phi [56], [67], [78]` while preserving the sibling non-join exit edge. So the current `M2` authority now covers shallow, nested, and deep branch-materialized helper-call families under slot-precise internal-call summaries, not only one recursive example plus the earlier non-branch materialized case.
- 2026-05-01: That same helper-summary line now also closes the current non-branch blocked-edge predecessor-binary family instead of leaving it as a `touch()`-only case. In the representative three-way join where one predecessor already materializes `add(load_global g.0, 3)` before the join, but the internal helper only reads `h.1`, both `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and the full pipeline now collapse the final join all the way to `phi [11], [22], [33]` without preserving a fake post-helper `load_global g.0` or the helper-edge predecessor binary. So by this point the current `M2` authority covers the simple blocked-edge predecessor-binary family, the non-branch materialized family, and the shallow/nested/deep branch-materialized family under slot-precise internal-call summaries.
- 2026-05-01: That same precise helper-call line now also reaches one loop-level high-level consumer instead of stopping at acyclic join and blocked-edge families. In the representative loop where `main` first stores `g.0 = 9`, then loops forever through an internal `helper()` that only reads `h.1`, the full Memory-SSA pipeline now deletes the `g.0` store entirely and collapses the program to a pure `call helper(); jmp` loop. This matters because the earlier `touch()` version had to keep the `g.0` store live forever and treat the loop body as a barrier; the new helper-summary coverage proves that slot-precise call modeling now influences loop-shaped high-level cleanup too, not only join-local CSE/materialization families.
- 2026-05-01: That same loop-level helper-call authority now also reaches mixed local/global scalar replacement instead of only the final pipeline cleanup. In the representative loop where `main` initializes `(a.0, g.0)` to `(1, 10)`, the backedge rewrites only `a.0` to `2`, and the loop body calls the internal `helper()` that only reads `h.1`, `memory_ssa_pass_scalar_replace_slots(...)` now erases the `g.0` carrier completely, keeps only the loop-header local phi for `a.0`, and reduces the exit to `add(phi(local), 10)`. This closes another meaningful `M2` gap because precise helper-call summaries are now shaping loop-level mixed scalar replacement itself, not only loop pipeline cleanup or the earlier acyclic mixed helper-call families.
- 2026-05-01: That same loop-level mixed helper-call authority now also survives through the full high-level Memory-SSA pipeline, not only the standalone scalar-replacement entrypoint. On that same representative `(a.0, g.0) = (1, 10)` loop where only `a.0` changes on the backedge and the loop body calls the internal `helper()` that only reads `h.1`, `memory_ssa_pass_run_pipeline(...)` now stabilizes at the same `add(phi(local), 10)` exit form instead of reintroducing `g.0` carrier loads/stores or keeping extra slot traffic alive. So the current `M2` authority now includes loop-shaped mixed helper-call scalar replacement both as a direct consumer and as part of the full Memory-SSA cleanup pipeline.
- 2026-05-01: We now also have a more explicit loop-header global-readonly boundary across consumer layers instead of only a single “readonly helper loops are fine” statement. In the representative loop where `main` first loads unknown `g.0`, the loop header reloads `g.0`, and the body calls the internal `helper()` that only reads `h.1`, `memory_ssa_pass_promote_global_slots(...)` still stays conservative and keeps the loop-header `load_global g.0`, while `memory_ssa_pass_scalar_replace_global_slots(...)` and the full pipeline go further and collapse the dead exit away, leaving a `call helper(); jmp` self-loop carrying the entry-loaded global value by phi. This is a useful near-final `M2` fact because it makes the current boundary between loop-level promotion conservatism and stronger loop-level scalar-replacement/pipeline cleanup explicit under precise internal-call summaries.
- 2026-05-01: That same loop-header global-readonly boundary now also covers the intermediate memory-value canonicalizer instead of skipping straight from promotion to the full pipeline. On the same representative loop, `memory_ssa_pass_canonicalize_memory_values(...)` is already on the stronger side of the boundary: like `scalar_replace_global_slots(...)` and `memory_ssa_pass_run_pipeline(...)`, it deletes the entry `load_global g.0` plus the dead exit and collapses the program to a pure `call helper(); jmp` self-loop, while `memory_ssa_pass_promote_global_slots(...)` still keeps the loop-header reload. This is another meaningful near-final `M2` checkpoint because the current layer split is now explicit across promotion, memory-value canonicalization, scalar replacement, and the full pipeline under precise internal-call summaries.
- 2026-05-01: We now also have a second explicit loop-level helper-call stratification on the “known entry store” side rather than only the earlier “unknown entry load” side. In the representative loop where `main` first stores `g.0 = 9`, the body calls the internal `helper()` that only reads `h.1`, and the exit reads `g.0`, `memory_ssa_pass_forward_global_loads(...)` is already strong enough to reduce the exit to `mov 9`, `memory_ssa_pass_promote_global_slots(...)` still remains conservative and keeps the exit `load_global g.0`, and `memory_ssa_pass_canonicalize_memory_values(...)` plus the full pipeline go further and delete the store/load/exit path entirely, collapsing the program to a pure `call helper(); jmp` loop. This is a useful near-final `M2` fact because it shows the current layer boundary is consistent across both loop-level global-helper seeding modes: unknown entry load and known entry store.
- 2026-05-01: That same known-entry-store helper-loop ladder is now also explicit on the global-only scalar-replacement layer, not only on forwarding/promotion/canonicalize/pipeline. On the same representative loop, `memory_ssa_pass_scalar_replace_global_slots(...)` is already on the stronger side of the boundary with `memory_ssa_pass_canonicalize_memory_values(...)` and `memory_ssa_pass_run_pipeline(...)`: it deletes the entry `store_global g.0, 9` plus the dead exit and collapses the program to pure `call helper(); jmp`, while `memory_ssa_pass_promote_global_slots(...)` still keeps the exit `load_global g.0`. This is another near-final `M2` checkpoint because the pure-global helper-loop consumer ladder is now fully populated across forwarding, promotion, scalar replacement, memory-value canonicalization, and the full pipeline.
- 2026-05-01: We now also have a first pure read-only helper-call repeated-load ladder instead of only loop- and join-shaped families. In the representative straight-line case `call helper(); load_global g.0; ... later load_global g.0`, where the internal `helper()` reads only `h.1`, `memory_ssa_pass_forward_global_loads(...)` already reuses the first `load_global g.0`, and both `memory_ssa_pass_canonicalize_memory_values(...)` and `memory_ssa_pass_run_pipeline(...)` stabilize at the same single-load form `call helper(); load_global g.0; ret ...`. This is another useful near-final `M2` checkpoint because it shows the slot-precise call summary now also feeds a direct repeated-read consumer ladder, not only store-seeded loops or join/materialization families.
- 2026-05-01: We now also have a first explicit readonly-helper partial-phi-forward global family at the raw forwarding layer rather than only repeated-load or loop examples. In representative two-way, scrambled, ancestor-reuse, and multi-pred join shapes where an internal `helper()` reads only `h.1`, `memory_ssa_pass_forward_global_loads(...)` now propagates the one surviving `g.0` version across the join by inserting missing predecessor-side loads where needed, building join phis over sibling loads, and preferring an already available ancestor-carried value when one path already has it. This is another near-final `M2` checkpoint because it shows the precise call summary now influences the foundational global-join forwarding family itself, not only the later scalar-replace/canonicalize/pipeline consumers above it.
- 2026-05-01: On top of that same readonly-helper partial-phi-forward family, the global-only scalar-replacement consumer is now also explicitly stronger than the raw forwarding surface instead of merely matching it. In representative two-way, scrambled, ancestor-reuse, and multi-pred helper-join shapes, `memory_ssa_pass_forward_global_loads(...)` still exposes the surviving `g.0` meaning through join-local `phi/mov` glue, but `memory_ssa_pass_scalar_replace_global_slots(...)` already goes further and collapses the whole family to the simpler `call helper(); load_global g.0; ret ...` form. This is another near-final `M2` checkpoint because it sharpens the layer split: readonly-helper join forwarding is one thing, and the higher scalar-replacement layer already knows how to flatten that global carrier skeleton away when nothing else still depends on the join structure.
- 2026-05-01: That same global-only scalar-replacement layer now also has explicit readonly-helper store-shape authority instead of being locked only on repeated-load, join, and loop families. In representative dead-store-before-helper and redundant-store-after-helper shapes where the internal `helper()` reads only `h.1`, `memory_ssa_pass_scalar_replace_global_slots(...)` now deletes the `g.0` carrier store entirely and simplifies the program to `call helper(); ret 0`. This is another useful near-final `M2` checkpoint because it confirms the slot-precise helper summary feeding the pure-global join ladder also reaches the direct global-store cleanup shapes at the scalar-replacement tier, not only the later memory-value / pipeline consumers.
- 2026-05-01: That same readonly-helper global-join family now also reaches the intermediate and full high-level canonicalizers instead of stopping at scalar replacement. In representative two-way, scrambled, ancestor-reuse, and multi-pred helper-join shapes, both `memory_ssa_pass_canonicalize_memory_values(...)` and `memory_ssa_pass_run_pipeline(...)` now stabilize at the same flattened `call helper(); load_global g.0; ret ...` form as `memory_ssa_pass_scalar_replace_global_slots(...)`, while the raw forwarding layer still intentionally surfaces the more explicit `phi/mov` glue. This is another near-final `M2` checkpoint because the layer split is now explicit across forwarding, scalar replacement, memory-value canonicalization, and the full pipeline for the readonly-helper global-join family.
- 2026-05-01: That same pure-global readonly-helper family now also explicitly reaches the direct memory-aware redundant-binary consumer instead of leaving that middle layer documented only for mixed/materialized helper-call shapes. In the representative repeated-load case and the representative two-way, scrambled, ancestor-reuse, and multi-pred partial-phi helper-join shapes, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` now stabilizes at the same flattened `call helper(); load_global g.0; ret ...` form as `memory_ssa_pass_scalar_replace_global_slots(...)`, `memory_ssa_pass_canonicalize_memory_values(...)`, and `memory_ssa_pass_run_pipeline(...)`, while the raw forwarding layer still intentionally keeps the more explicit `phi/mov` glue where applicable. This is another useful near-final `M2` checkpoint because the pure-global readonly-helper ladder is now explicit across forwarding, scalar replacement, direct memory-aware CSE, memory-value canonicalization, and the full pipeline.
- 2026-05-01: The pure-global unknown-call barrier line now also has an explicit stronger-consumer ladder instead of being documented mainly through forwarding, dead-store cleanup, and one loop-pipeline endpoint. In representative straight-line `store_global g.0, 9; call touch(); load_global g.0` and loop-shaped `store_global g.0, 9; ... call touch() ... exit load_global g.0` families, `memory_ssa_pass_scalar_replace_global_slots(...)` stays conservative at the barrier, `memory_ssa_pass_canonicalize_memory_values(...)` keeps the same `store/call/load` or `store/jmp/call-loop` form, and the stronger `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` / `memory_ssa_pass_run_pipeline(...)` layers still remain on that same side of the boundary rather than drifting toward the readonly-helper endpoints. This is another useful near-final `M2` checkpoint because the pure-global unknown-call story is now explicit across scalar replacement, memory-value canonicalization, direct memory-aware CSE, and the full pipeline, not only across forwarding/promotion/store-cleanup layers.
- 2026-05-01: That same direct memory-aware CSE layer now also has explicit helper-call store-shape and loop-shape authority instead of stopping at repeated-load and join families. In representative dead-store-before-helper and redundant-store-after-helper shapes it already simplifies to `call helper(); ret 0`; on the representative known-entry-store helper loop it also collapses to a pure `call helper(); jmp` self-loop; and on the representative mixed local/global helper loop it stabilizes at the same `add(phi(local), 10)` exit form as `memory_ssa_pass_scalar_replace_slots(...)` / `memory_ssa_pass_canonicalize_memory_values(...)`. But the representative loop-header global-readonly helper family is still one layer more conservative there: unlike `memory_ssa_pass_canonicalize_memory_values(...)` and `memory_ssa_pass_run_pipeline(...)`, `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` currently keeps the entry `load_global g.0` plus the loop-carried phi while still deleting the dead exit. This is another useful near-final `M2` checkpoint because direct memory-aware CSE now has an explicit loop/store ladder of its own, not only the earlier join/materialization ladder.
- 2026-05-01: The intermediate memory-value canonicalizer now also has explicit mixed local/global helper-call authority instead of leaving the acyclic mixed families documented only at the lower scalar-replace tier and the higher CSE/pipeline tier. In representative two-way and three-way mixed joins where an internal `helper()` reads only `h.1`, `memory_ssa_pass_canonicalize_memory_values(...)` now stabilizes at the same conservative `phi(local) + phi/global-carrier + add` boundary as `memory_ssa_pass_scalar_replace_slots(...)`: the local carrier is fully scalar-replaced, the helper edge may still contribute one surviving `load_global g.0`, and the final arithmetic intentionally remains `add(...)` because this tier stops before memory-aware redundant-binary elimination. This is another useful near-final `M2` checkpoint because it makes the middle layer boundary explicit for mixed helper-call families too, not only for pure-global helper joins or for the stronger CSE/pipeline endpoints.
- 2026-05-01: That same mixed helper-call family now also has a fuller join-shape ladder instead of being locked only on one scrambled case plus the earlier multi-pred and loop shapes. In representative source-order two-way, scrambled two-way, and scrambled multi-pred joins where the helper edge still contributes one surviving `load_global g.0`, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now stabilize at the expected `phi(local) + phi(global) + add` boundary, even when the join block is numbered before one or more predecessor blocks. On top of that, the stronger `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and `memory_ssa_pass_run_pipeline(...)` layers already go one step further on both the two-way and scrambled-multi-pred joins and may fold the join to phi-of-edge-results forms such as `phi [11], [add(load_global g.0, 2)]` and `phi [11], [22], [add(load_global g.0, 3)]` instead of preserving the explicit `add(phi(local), phi(global))`; and the representative three-way mixed helper join remains strong enough to collapse all the way to per-edge constants (`11/22/33`). This is another useful near-final `M2` checkpoint because it makes the mixed-family layer split explicit across ordinary two-way, scrambled two-way, scrambled multi-pred, and multi-pred helper-call joins.
- 2026-05-01: That same mixed helper-call join ladder now also has an explicit ordinary multi-pred intermediate endpoint instead of jumping directly from the lower/middle `phi(local) + phi(global) + add` boundary to the fully-known-edge `11/22/33` case. In the representative source-order three-way join where the helper edge still contributes only `local=3` plus one surviving `load_global g.0`, both `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and `memory_ssa_pass_run_pipeline(...)` now stabilize at `phi [11], [22], [add(load_global g.0, 3)]`. This is another useful near-final `M2` checkpoint because it makes the direct-CSE/pipeline boundary explicit for the ordinary multi-pred mixed helper family too, not only for the scrambled variant or the later fully-known-edge family.
- 2026-05-01: That same mixed helper-call authority now also reaches the current `materialized` and `branch-materialized-nested` families at the lower and middle consumers instead of being documented there only on the stronger memory-aware CSE / pipeline side. In representative shapes where the helper edge stores `(a.0, g.0) = (3, 30)` before calling the internal readonly `helper()`, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now reduce the remaining slot traffic to explicit local/global carrier phis followed by the surviving arithmetic chain (`add(phi(local), phi(global))`, then `+5` where present), without reintroducing a fake unknown-call barrier on the helper edge. This is another useful near-final `M2` checkpoint because it makes the lower/middle-layer boundary explicit for the first recursive mixed helper-call materialization families too, not only for the later direct-CSE/pipeline endpoint that collapses them all the way to edge-result phis.
- 2026-05-01: The mixed local/global family now also has an explicit unknown-global-barrier ladder instead of being documented mainly through the more precise readonly-helper side. In representative two-way, three-way, and loop-shaped `touch()` barrier examples, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now intentionally stop at the conservative `phi(local) + phi(global/load_global) + add` boundary, while the stronger `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and `memory_ssa_pass_run_pipeline(...)` layers may still fold the two-way and three-way join shapes one step further to phi-of-edge-results forms such as `phi [11], [add(load_global g.0, 2)]` and `phi [11], [22], [add(load_global g.0, 3)]`. But the loop-shaped unknown-barrier family still stays one layer more conservative there and preserves the exit-side `load_global g.0` plus the final `add(phi(local), load_global g.0)` form. This is another useful near-final `M2` checkpoint because it makes the layered boundary under a true unknown global barrier explicit across two-way, multi-pred, and loop-shaped mixed families, not only under the slot-precise internal-helper call summaries.
- 2026-05-01: That same unknown-global-barrier loop family is now also explicit at the stronger direct-CSE/pipeline tier rather than being inferred only from the lower scalar-replace/memory-value layers plus the pure-global loop-barrier examples. In the representative loop where `main` seeds `(a.0, g.0) = (1, 10)`, the backedge rewrites only `a.0` to `2`, and the loop body calls unknown `touch()`, both `memory_ssa_pass_eliminate_redundant_memory_binaries(...)` and `memory_ssa_pass_run_pipeline(...)` remain on the conservative side of the boundary: they preserve the entry `store_global g.0, 10`, the exit `load_global g.0`, and the final `add(phi(local), load_global g.0)` instead of collapsing toward the stronger readonly-helper `add(phi(local), 10)` or `call; jmp` endpoints. This is another useful near-final `M2` checkpoint because it makes the loop-side split under a true unknown global barrier explicit all the way up through the strongest current consumers too.
- 2026-05-01: That same lower/middle-layer materialization authority now also spans the branch-materialized family's shallow and deep ends rather than only the earlier nested middle slice. In representative branch-materialized and branch-materialized-deep shapes where the helper edge stores `(a.0, g.0) = (3, 30)`, then calls the readonly `helper()` before branching either to the join or to the side exit, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now preserve the same explicit local/global carrier phis at the join and leave the remaining arithmetic chain in SSA form (`add(phi(local), phi(global))`, then the current `+5/+7/+9/+11/+13` tail where present), while the sibling non-join exit edge remains intact. This is another useful near-final `M2` checkpoint because the lower/middle-layer boundary is now explicit across shallow, nested, and deep branch-materialized mixed helper-call families too, not only on the later direct-CSE/pipeline side.
- 2026-05-01: That same lower/middle-layer mixed helper-call authority now also reaches the current partial-unknown predecessor family instead of leaving that family documented only on the stronger direct-CSE/pipeline side. In the representative three-way join where the helper edge stores `(a.0, g.0) = (3, 30)`, then calls the readonly `helper()` and still materializes one local `add(3, g.0)` on that edge, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now reduce the surviving memory traffic to the same explicit local/global carrier phis as the ordinary mixed multi-pred family, namely `phi(local=[1,2,3]) + phi(global=[10,20,30]) + add`, without reintroducing an unknown-call-style barrier on the helper edge. This is another useful near-final `M2` checkpoint because it makes the lower/middle-layer boundary explicit for the partial-unknown mixed helper-call family too, not only for the later direct-CSE/pipeline endpoint that keeps the helper-edge predecessor binary visible.
- 2026-05-01: That same lower/middle-layer mixed-shape authority now also reaches the current partial-unknown materialized and branch-materialized families instead of leaving those shapes documented only on the stronger direct-CSE/pipeline side. In representative `materialized-partial-unknown` and `branch-materialized-partial-unknown` shapes, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now reduce the surviving memory traffic to explicit local/global carrier phis plus the remaining join-local arithmetic (`add(phi(local), phi(global))`, then `+5` where present), while still preserving the side-exit branch shape on the branch-materialized family. This is another useful near-final `M2` checkpoint because it makes the lower/middle-layer boundary explicit for the partial-unknown materialization families too, not only for the later direct-CSE/pipeline endpoints that keep the barrier-edge materialized arithmetic visible.
- 2026-05-01: That same lower/middle-layer partial-unknown materialization authority now also spans the branch-materialized family's nested and deep ends rather than stopping at the shallow branch case. In representative branch-materialized-nested-partial-unknown and branch-materialized-deep-partial-unknown shapes, both `memory_ssa_pass_scalar_replace_slots(...)` and `memory_ssa_pass_canonicalize_memory_values(...)` now preserve the same explicit local/global carrier phis at the join and leave the remaining arithmetic chain in SSA form (`add(phi(local), phi(global))`, then `+5` and the later `+7/+9/+11/+13` tail where present), while the sibling non-join exit edge remains intact. This is another useful near-final `M2` checkpoint because the lower/middle-layer boundary is now explicit across shallow, nested, and deep branch-materialized partial-unknown families too, not only for the later direct-CSE/pipeline endpoints that keep the barrier-edge materialized arithmetic visible more explicitly.
- 2026-04-17: Implementation-structure note: that join-local predecessor projector now threads one explicit internal projection/materialization context instead of passing the same `(function, def_use, join block, predecessor edge, allow_materialize)` parameter set through every recursive helper call. This is not a semantic change, but it makes the current narrow join-local expression-projector easier to extend without further parameter-plumbing churn.
- 2026-04-17: The first real allocator consumer is now underway as its own sibling module instead of remaining only allocator-prep facts. `include/value_ssa_alloc.h` and `src/value_ssa_alloc/` now provide a first conservative allocation-result surface, a first `value_ssa_allocate_function(...)` entrypoint, a debug dump surface, and isolated allocator regression coverage. Current scope is intentionally small: deterministic coloring under a budget, affinity-biased color preference when safe, and spill-candidate marking when no color is available. Spill rewrite remains deferred.
- 2026-04-17: That first allocator surface now also has a small program-level and query layer above the initial function allocator. `value_ssa_allocate_program(...)`, per-value color/spill query helpers, and regression coverage for both now exist, so later spill-policy or allocator-driver work no longer needs to treat the result as an opaque dump-only artifact.
- 2026-04-17: The first allocator is now also slightly stronger than "if the current value has no free color, spill the current value". Under the same conservative greedy policy, it may now evict one already-colored lower-priority interfering neighbor and reuse that freed color for the current higher-priority value, then mark the evicted neighbor as the spill-candidate instead. This is still far short of a full simplify/select allocator, but it is the first real spill-choice refinement above the initial baseline.
- 2026-04-17: That allocator surface now also has a more formal spill-score/result-observability layer instead of leaving spill policy as an implicit internal heuristic only. Spill score components are now computed through one dedicated helper, per-value spill priority is queryable, program-level allocation dumping exists beside the earlier function-level dump, and allocator regression coverage now locks those surfaces too. This makes later spill-policy tuning more reviewable because the scoring/result layer is no longer opaque.
- 2026-04-17: The allocator's move-preference policy is now also stronger than "pick the first available affinity partner color". When multiple colors remain legal, the current conservative allocator now scores them by the total copy-affinity weight already sitting on each available color and prefers the highest-weight choice. This is still a greedy colorer rather than real coalescing, but it gives the current allocator a more meaningful affinity-driven choice rule.
- 2026-04-17: The allocator's local eviction rule is now also stricter than "find any lower-priority interfering neighbor on some blocked color". A neighbor is only evictable if it is the unique blocker for that color among the current value's already-colored interfering neighbors; colors still blocked by multiple interfering values are no longer treated as falsely freed just because one lower-priority occupant was removable. This closes a correctness hole in the earlier local spill-choice refinement while keeping the rule conservative and deterministic.
- 2026-04-17: The allocator result surface now also carries deterministic spill-slot assignment instead of stopping at a bare spill flag. Spilled values are numbered into stable per-function spill slots, spill-slot queries now exist beside the earlier color/spill/priority queries, and both function/program allocation dumps now expose those slot ids. This still does not rewrite IR, but it moves the allocator from "who spills?" toward "who spills where?" and makes later spill-rewrite work easier to stage.
- 2026-04-17: That spill-prep/result layer now also exposes small aggregate queries (`count_colored`, `count_spilled`) above the earlier per-value queries. This is still intentionally short of spill rewrite: the allocator can now answer "how many values colored/spilled?" and "which spill slot?" directly, but IR rewrite/insertion work remains deferred until there is a safe instruction-insertion story for it.
- 2026-04-17: A first extremely narrow spill rewrite slice is now landed above that spill-prep surface. `value_ssa_rewrite_program_single_block_spills(...)` currently supports only instruction-defined spilled values whose uses stay within the defining block; it now handles both same-block instruction uses and same-block terminator uses (`ret` / `br` condition) by assigning a spill local slot, inserting a `store_local` after the defining instruction, and inserting `load_local` reloads immediately before each rewritten use. This is still far from a general spill pipeline, but it is the first end-to-end proof that allocator results can feed real IR rewrite when the CFG/use shape is simple enough.
- 2026-04-17: That spill rewrite slice now handles same-function non-phi uses, not only uses in the defining block. For spilled instruction-defined values, ordinary instruction uses and terminator uses in successor blocks can now receive reloads in their own blocks while the defining block gets the store after the def; the rewriter recomputes def/use facts between spilled values so insertion from one rewrite does not stale the next rewrite's instruction indices. Phi-input spill placement remains deliberately unsupported because it needs edge-specific placement.
- 2026-04-17: Phi-input spill placement is now supported for the first simple edge-aware spill rewrite shape. When a spilled value is used by a phi input, the rewriter inserts the reload in that input's predecessor block before the terminator and rewrites only the corresponding phi incoming value to the reload result. This keeps the existing instruction-defined-value restriction, but it means the current spill rewrite can now handle ordinary instruction uses, terminator uses, and phi incoming-edge uses inside one function.
- 2026-04-17: Allocator spill rewrite now also has a cleanup-facing helper. `value_ssa_rewrite_program_single_block_spills_and_canonicalize(...)` runs the current spill rewrite slice and then feeds the result through the existing Value-SSA canonicalization pipeline, giving callers a verifier-checked/stabilized endpoint instead of forcing them to remember the cleanup step manually. The lower-level rewrite-only entry remains available for focused rewrite tests.
- 2026-04-17: The allocator's first high-level driver now also returns a post-rewrite allocation result rather than leaving callers with a stale pre-rewrite snapshot. `value_ssa_allocate_and_rewrite_program_single_block_spills(...)` now allocates, rewrites plus canonicalizes, and then reallocates on the final program before returning, so its result surface stays aligned with the rewritten SSA graph's final `next_value_id`/spill state instead of describing the pre-rewrite program only. Dedicated allocator regression coverage now locks that result/program alignment contract.
- 2026-04-17: That same allocator driver is now also explicitly bounded as a small fixed-point loop rather than an unguarded "keep rewriting until maybe stable" helper. It may perform multiple allocate/rewrite/reallocate rounds when newly exposed spills remain rewriteable under the current narrow policy, but it now stops with an explicit error if that process fails to converge within a small iteration cap. Current regression coverage also locks that the driver does not keep respilling its own reload family on a representative cross-block shape.
- 2026-04-17: That same allocator fixed-point driver is now also slightly more observable and slightly less wasteful. A stats-bearing entrypoint can now report how many allocation rounds and rewrite rounds were actually performed, and the helper no longer performs one last redundant reallocation when a fixed-point iteration discovers there is nothing left to rewrite. Current allocator coverage locks both a rewriteing case (`allocation_rounds >= 1`, `rewrite_rounds >= 1`) and a no-rewrite smoke case (`allocation_rounds == 1`, `rewrite_rounds == 0`).
- 2026-04-17: That allocator-driver observability layer now also has a stable text dump surface of its own. `value_ssa_dump_allocate_rewrite_stats(...)` prints the current fixed-point driver's allocation/rewrite round counts in one small line, and allocator regression coverage now locks the no-rewrite smoke form exactly. This gives later allocator tuning and review a lightweight way to inspect driver behavior without relying only on debugger/probe output.
- 2026-04-17: Allocator spill rewrite now also supports the first phi-defined spilled values rather than only instruction-defined ones. When the spilled SSA value is defined by a block-entry phi, the rewriter inserts the spill store at the beginning of that block's normal instruction stream, then rewrites later instruction/terminator/phi-input uses through ordinary spill reloads. The implementation now rewrites uses one at a time after recomputing def/use facts, which keeps insertion indices current as reloads are added and gives both instruction-defined and phi-defined spills the same safer rewrite path. Dedicated allocator coverage locks a phi-result spill that stores in the join block and reloads before return.
- 2026-04-17: The new phi-defined spill rewrite path now has broader mixed-use coverage before further feature expansion. Allocator regressions now cover one phi-defined spilled value used by same-block arithmetic, same-block branch condition, cross-block arithmetic, and successor phi input in a single CFG shape, confirming that the one-use-at-a-time rewrite path composes across ordinary, terminator, and edge uses. While adding that coverage, the shared Value-SSA def/use analysis was also hardened to initialize all def-site arrays to `SIZE_MAX` sentinels up front, so missing definitions and phi/instruction def-site checks no longer depend on uninitialized allocation contents.
- 2026-04-17: The high-level allocator fixed-point driver now also admits phi-defined spilled values through its rewriteability gate, not only through direct/manual rewrite tests. The exact phi-result store/reload placement remains locked by the focused direct-rewrite regressions, while the high-level driver regression now verifies the public behavior that matters after canonicalization: real allocator-selected spills can rewrite, the final program/result stay aligned, and the driver reports rewrite rounds even when later cleanup folds the original phi-heavy shape down.
- 2026-04-17: Allocator spill-reload placement now has a first narrow edge-splitting slice for branch predecessors feeding successor phi inputs. When only one outgoing edge from a branch block feeds the phi that uses a spilled value, the rewriter now creates a fresh split block on that edge, redirects only the relevant branch target to the split block, inserts the reload there, jumps onward to the original phi block, and rewrites all phis in the successor to use the split block as predecessor. Representative allocator coverage locks both a focused branch-predecessor phi-input case and a stronger phi-defined multi-use case where the branch's sibling edge stays untouched while the spilled phi input moves through the split block.
- 2026-04-17: That allocator edge-splitting slice now also reuses existing split blocks on the same branch edge instead of creating chains of split blocks for multiple spilled phi inputs. The reuse logic recognizes the narrow split-block shape it creates (`load_local`/`mov` glue followed by `jmp successor`) and avoids mistaking ordinary sibling predecessor blocks for reusable splits. If multiple phi inputs on the same split edge need the same spill slot value, they now share the existing reload result rather than inserting duplicate loads. Representative allocator coverage locks a two-phi-input edge where both phis use one shared reload from one split block.
- 2026-04-17: That same split-block reuse line now also covers multiple distinct spill slots on the same branch edge, not only repeated uses of one spill slot. When two successor phi inputs on the same split edge require different spilled predecessor values, the rewriter now reuses the same split block and appends the additional spill-slot reload there instead of creating another split block. Current direct allocator coverage locks the present precise endpoint: one reused split block, one `load_local spill.0`, one `load_local spill.1`, and (for the second slot's phi) a small `mov` glue value that keeps the phi input available from the split block without introducing a second split edge.
- 2026-04-17: That distinct-spill-slot split-block reuse endpoint is now also slightly cleaner than before. When a phi input on an already-split edge had previously been represented by a small split-block `mov` glue value and the underlying source value is later itself spilled, the rewriter now upgrades that `mov` in place to a direct `load_local spill.N` instead of leaving an extra `mov(load)` layer behind. Representative allocator coverage now locks the stronger direct endpoint with one reused split block containing exactly one `load_local spill.0` and one `load_local spill.1`.
- 2026-04-17: Allocator spill storage is now also shared across non-interfering spilled values instead of assigning one dedicated spill slot per spilled SSA value. `value_ssa_allocate_function(...)` now greedily reuses an existing spill slot whenever the new spilled value does not interfere with the values already mapped there, and spill rewrite now reuses/creates one local per slot (`spill.<slot>`) rather than appending a fresh anonymous spill local for every rewritten value. Direct allocator regressions now lock the rewritten dump shape under that naming (`spill.0.0`, `spill.1.1`, ...), while the high-level fixed-point regression checks the stable public effect that matters after canonicalization: shared spill-slot allocation is reported, at least one rewrite round occurs, and the final program still converges to the expected result.
- 2026-04-17: Allocator spill rewrite now also has a first narrow local reload-reuse optimization instead of inserting one fresh `load_local spill.N` for every later use in the same block. When a same-block instruction, terminator, or predecessor-side phi input can already see a dominating reload of that same spill slot with no intervening `store_local spill.N`, the rewriter now reuses the existing SSA reload value directly. Current coverage locks both a focused same-block multi-use family (`one reload, two later uses`) and a stronger phi-defined mixed-use family where the join block's existing reload is reused not only by same-block arithmetic and branch condition, but also as the direct predecessor input to a successor phi without creating the earlier split-edge reload block.
- 2026-04-17: That allocator reload-reuse line now also extends one conservative step across straight-line CFG edges instead of stopping strictly at block boundaries. When a block has exactly one predecessor and that predecessor already ends with an available reload of the same spill slot, later uses in the successor may now reuse the predecessor's SSA reload value directly rather than inserting another `load_local spill.N`. Current coverage locks both a focused two-block straight-line family and the stronger phi-defined mixed-use family where a reload created in the join block is now reused by a unique-predecessor successor block as well (`bb.4: add ssa.7, 2`) instead of reloading in that successor.
- 2026-04-17: That same conservative reload-reuse rule now also walks back through a whole unique-predecessor chain rather than stopping after one edge. If block `B2` has one predecessor `B1`, and `B1` also has one predecessor `B0`, the rewriter may now reuse a reload created in `B0` directly in both later blocks as long as no intervening `store_local spill.N` hides it. Current coverage locks a three-block straight-line family where one `load_local spill.0.0` created in `bb.0` now feeds both `bb.1` and `bb.2` without any additional reloads.
- 2026-04-17: Allocator policy now also has a first explicit phase-planning surface instead of relying only on the older allocation worklist classes plus implicit loop order. `value_ssa_alloc` now exposes `ValueSsaAllocatorPlan`, `ValueSsaAllocatorPhase`, `value_ssa_compute_allocator_plan(...)`, `value_ssa_dump_allocator_plan(...)`, and a focused find helper. The current phase heuristic is deliberately narrow and K-aware: move-related low-degree values start in `freeze`, degree-at-least-K values start in `spill`, and the rest start in `simplify`. `value_ssa_allocate_function(...)` now consumes that phase-ordered plan as its traversal surface, and representative allocator regressions now lock both a move-hint smoke family and a loop-constrained family through the public plan API/dump.
- 2026-04-17: That allocator plan surface is now also iterative rather than a one-shot static phase label. `value_ssa_compute_allocator_plan(...)` now tracks an active degree per unscheduled value, repeatedly picks the best current `freeze` / `spill` / `simplify` candidate, and decrements the active degree of still-unscheduled neighbors after each pick. This gives the current allocator its first real worklist-like behavior: a value that initially looks spill-heavy may later fall into `simplify` once enough neighbors have already been removed. Representative allocator coverage now locks exactly that dynamic downgrade in a loop family, while `value_ssa_allocate_function(...)` continues to consume the resulting plan as its current traversal order.
- 2026-04-17: That iterative allocator plan now also has a first explicit "remove first, select later" discipline rather than being consumed in the same direction it is built. Plan construction now prefers current `simplify` candidates ahead of `freeze`, and `freeze` ahead of `spill`, so the dump reads like a removal order. `value_ssa_allocate_function(...)` now colors in the reverse plan order, giving the current allocator its first real stack/select flavor above the earlier greedy pass. This is still deliberately lighter than a full Briggs/Chaitin allocator, but it means the repository now has the essential remove-order / reverse-select shape in place before future coalescing/freeze/spill refinements.
- 2026-04-17: That remove/select allocator skeleton is now also a little more informative and a little less arbitrary inside each phase. `ValueSsaAllocatorPlanItem` now records both the original interference degree and the current active degree seen at the moment of removal, and the public plan dump now prints that evolution as `degree=initial->active`. The iterative selector also now uses a small phase-local tie-break policy instead of only falling through to generic priority: within `spill`, higher active-degree values are preferred earlier; within `freeze`, lower active-degree move-related values are preferred earlier. This stays intentionally conservative, but it makes the worklist evolution easier to inspect and gives the current allocator a slightly more backend-like choice policy without yet introducing full Briggs-style freeze/select state machines.
- 2026-04-17: That iterative/remove-select allocator line now also tracks move-related state dynamically instead of treating it as a permanent bit copied from allocation prep. `ValueSsaAllocatorPlanItem` now records both `initial_move_related` and `active_move_related`, `value_ssa_compute_allocator_plan(...)` now carries a live move-related mask that can change as values are removed, and the public plan dump now prints that evolution as `move_related=yes->yes` / `no->no` (with room for future `yes->no` downgrade families as the allocator grows). Representative allocator coverage now locks both sides of the current intended behavior: active degree can drop across the iterative removal walk, and move-related state is preserved while a value still has live affinity edges. Together with the earlier degree tracking and reverse-select coloring, this means the allocator plan is no longer just a prettier static queue; it is now a small dynamic state machine over active degree plus active affinity.
- 2026-04-18: The allocator plan now also has a first real `freeze` operation rather than merely assigning some values the `freeze` phase label. During plan construction, when no `simplify` candidate is available but a move-related candidate exists, the allocator now marks one current `freeze` value as frozen, recomputes active move-related state, and then continues scheduling from the new state. In practice that means some move-hint families now thaw all the way into later `simplify` removals (`move_related=yes->no`, `phase=simplify`) once their remaining live affinity constraints have been frozen away. The current plan dump should therefore be read as a final removal order after internal freeze steps, not as a log of transient pre-freeze labels. Representative allocator coverage now locks that stronger behavior on both a small affinity+interference family and a larger move-chain family, while the rest of the allocator pipeline continues to color in reverse plan order.
- 2026-04-18: The allocator plan now also records an explicit removal kind, not only the final phase label. `ValueSsaAllocatorPlanItem` carries `removal_kind`, `value_ssa_allocator_removal_kind_name(...)` is public, and the plan dump now prints `remove=simplify-remove` or `remove=spill-remove`. This matters because the current freeze operation can thaw former move-hint values into final `phase=simplify`; the removal kind is the stable signal for how a value actually left the worklist. Allocator regressions now lock both sides: move-hint families that freeze/thaw leave through `simplify-remove`, while constrained loop/move-chain values that still cannot simplify leave through `spill-remove`. This gives the later select/spill stage an explicit hook instead of requiring it to infer intent from the final phase alone.
- 2026-04-18: The select/result side now consumes that removal-kind hook instead of leaving it as a plan-only annotation. `ValueSsaAllocationResult` now records `spill_intended_flags` and `spill_confirmed_flags`, with public query helpers and allocation dumps that distinguish `spill-intended` colored results from `spill-confirmed` spilled results. During reverse-select coloring, values removed via `remove=spill-remove` are marked spill-intended first; if coloring still fails, the allocator confirms the spill and assigns the spill slot as before. Representative allocator coverage now locks the confirmed path on a constrained loop family (`remove=spill-remove`, intended=yes, confirmed=yes), giving later optimistic-coloring work an explicit result channel without changing spill rewrite semantics yet.
- 2026-04-18: The allocator select stage is now split out from the high-level color driver. `value_ssa_alloc_select.inc` owns preferred-color selection, reverse-plan traversal, spill-intent marking, and spill confirmation; `value_ssa_alloc_color.inc` now mostly composes analysis/prep/worklist/plan/select/spill-slot assignment. This makes the current backend-prep layering clearer and gives select semantics a focused file before future refinement. While doing the split, allocator coverage also locked the naturally occurring optimistic-coloring success path: in the existing spill clique family, one `remove=spill-remove` value now finishes colored with `spill-intended` but not `spill-confirmed`, while a different value still receives the real spill slot. The confirmed-spill loop family remains covered too, so both sides of the intended/confirmed split are now regression-locked.
- 2026-04-18: Allocator select behavior now also has a first public stats surface. `ValueSsaAllocationSelectStats`, `value_ssa_compute_allocation_select_stats(...)`, `value_ssa_dump_allocation_select_stats(...)`, and the init helper report colored values, real spills, spill-intended values, spill-confirmed values, and optimistically colored spill-intended values. This makes allocator policy tuning observable without parsing full allocation dumps by hand. Representative allocator coverage now locks the current spill clique endpoint as `colored=4 spilled=1 spill_intended=1 spill_confirmed=0 optimistic_colored=1`, which directly captures the newly split select semantics.
- 2026-04-18: Allocator select behavior now also has a first per-value trace dump. `value_ssa_dump_allocation_select_trace(...)` takes the allocator plan plus allocation result and prints the reverse-select order with each value's removal kind and final select outcome (`colored`, `optimistic-colored`, `spill-confirmed`, or `spill`). This complements the aggregate select stats: stats answer "how many", while the trace answers "which value and in what select order". Allocator coverage now locks a spill-clique trace containing both `outcome=optimistic-colored` and a real spill outcome, making the intended/confirmed split inspectable without stepping through `value_ssa_alloc_select.inc`.
- 2026-04-18: The allocator plan now also carries an explicit first spill-cost field instead of relying only on the generic worklist priority for every decision. `ValueSsaAllocatorPlanItem.spill_cost` is currently a conservative local score from use count, live-block count, and affinity sum; plan dumps now print it, and spill-phase tie-breaking prefers lower spill cost before falling back to active degree/priority/id. This does not replace the existing priority surface yet, but it gives future spill-policy tuning a dedicated field and keeps spill-choice reasoning separate from the broader worklist ordering score.
- 2026-04-18: That spill-cost surface is now no longer a black-box total only. `ValueSsaAllocatorPlanItem` carries the use-count, live-range, and affinity components beside the total, `value_ssa_allocator_plan_get_spill_cost(...)` exposes them as a focused query helper, and the allocator-plan dump prints `spill_cost=T(uses=U,blocks=B,affinity=A)`. Focused allocator coverage now locks both the component accounting and the spill-phase tie-break that prefers a lower-cost candidate before a higher-use candidate under a tight color budget.
- 2026-04-18: Allocator select traces now carry the same spill-cost breakdown as the removal plan. `value_ssa_dump_allocation_select_trace(...)` still reports reverse-select outcome (`colored`, `optimistic-colored`, `spill-confirmed`, or `spill`), but each line now also includes the total/use/live-range/affinity cost seen at removal time, so review can connect "why this value was removed as a spill candidate" with "what happened during select" without comparing two separate dumps by hand.
- 2026-04-18: Spill rewrite now also has a first conservative dominating-reload reuse rule above the earlier same-block and unique-predecessor-chain reuse. If one block already materializes `load_local spill.N` and that reload dominates a later use through a region with no intervening `store_local spill.N`, rewrite may now reuse that existing SSA reload even after reconverging CFG shape instead of inserting another load in a later dominated block. Focused allocator coverage now locks both the positive reconverged case and the negative clobber case.
- 2026-04-18: While landing that broader reload reuse, allocator spill rewrite also closed a real correctness hole in the older unique-predecessor-chain rule. Reuse no longer walks past an intervening `store_local spill.N` while searching earlier blocks for a spill reload, so a later use cannot accidentally keep reading a stale pre-clobber reload from farther up the chain. The new negative dominating-reload regression is also guarding this older path.
- 2026-04-18: Allocator roadmap authority is now updated in `docs/ssa/VALUE_SSA_ALLOCATOR_PLAN.md` for the post-first-allocator state. The near-term sequence is no longer "land a first allocator" but "finish one more short spill-rewrite CFG-coverage stretch, then switch to explicit coalescing, then strengthen optimistic coloring retry and spill-cost policy before later Briggs/Chaitin-style consolidation, live-range splitting, and target/machine constraints."
- 2026-04-18: Spill rewrite loop coverage is now also stronger than straight chains or reconverged acyclic CFG only. Phi-defined spilled values carried by a loop header may now materialize one header reload and reuse it through both the loop body and the exit path when no loop-body clobber intervenes. Focused allocator coverage now locks that positive loop family explicitly.
- 2026-04-18: While adding that loop family, allocator spill rewrite also closed another correctness hole in the older unique-predecessor-chain reuse rule. Reuse no longer looks only into predecessor blocks; it first checks whether the current target block itself has already `store_local spill.N` before the use. This prevents stale predecessor reload reuse across a same-block spill-slot clobber inside loop bodies or other single-predecessor descendants. The new negative loop-clobber regression is guarding that boundary.
- 2026-04-18: Spill rewrite reconvergence coverage is now also stronger than one-join diamonds only. A single dominating spill reload may now be reused through multiple successive reconverged CFG layers (`join -> split -> join -> split -> join`) when no intermediate path stores to that spill slot. Focused allocator coverage now locks a nested two-reconvergence family where one `load_local spill.0.0` materialized at the first join feeds later uses through two more joins.
- 2026-04-18: That stronger reconvergence line is now also guarded against nested-path clobber. If one branch in an earlier reconverged region performs `store_local spill.N`, later joins reached from that region must materialize a fresh reload instead of continuing to reuse the older dominating value. The new nested-clobber regression now locks this negative family together with the earlier same-block, chain, and loop clobber guards.
- 2026-04-18: Spill rewrite edge-placement coverage now also explicitly includes loop backedges, not only acyclic branch-predecessor edges. When a loop body branches and only one outgoing edge returns to a loop-header phi that needs a spilled incoming value, rewrite may now split just that backedge, place the spill reload in the split block, and retarget only the header phi inputs that actually come from the backedge family. Focused allocator coverage now locks this single-phi loop-backedge shape directly.
- 2026-04-18: That loop-backedge edge-placement line is now also regression-locked on a stronger shared split-block family. When two spilled body definitions both feed loop-header phis on the same backedge, rewrite now uses one split block on that backedge and materializes the needed incoming values there (`load_local`, `mov` glue for unrelated carried phis, and a second `load_local`) instead of creating separate backedge blocks. This extends the earlier acyclic branch-edge split-block reuse story into cyclic CFGs.
- 2026-04-18: The allocator mainline has now started its explicit coalescing phase rather than continuing only with affinity-biased coloring. `ValueSsaAllocatorCoalesceAnalysis` now records conservative coalescing decisions for copy-affinity candidates, including pair weight, whether the pair was already interference-filtered away, merged-neighbor count, and whether it is safe under the current color budget. Public compute/query/dump APIs are now available under `include/value_ssa_alloc.h`, with implementation split into `src/value_ssa_alloc/value_ssa_alloc_coalesce.inc`.
- 2026-04-18: That first coalescing layer is now also consumed by select, not just dumped. Preferred-color selection first tries a `can_coalesce` partner's already assigned color when it is available and not blocked, then falls back to the older affinity-weighted color preference. Allocator coverage now locks safe coalescing pairs, interference-filtered move pairs, color-budget-blocked merge decisions, and the behavior that a coalescible pair is assigned the same color when legal.
- 2026-04-18: Implementation-structure note for this first coalescing slice: the current public analysis only reasons about pairs that survive the existing copy-affinity prefilter, so directly interfering `mov` pairs are represented today by absence from the candidate set rather than by an explicit `interferes=yes` coalescing record. Budget-blocked cases are now locked separately through manual prep/interference/affinity inputs so the second-stage `merged_neighbor_count` policy is regression-covered without relying on fragile program-shape-specific liveness accidents.
- 2026-04-18: The explicit coalescing line now also has its first representative/class surface rather than only isolated pair decisions. `ValueSsaAllocatorCoalesceAnalysis` now records `value_roots` plus per-root `class_sizes`, `value_ssa_allocator_coalesce_analysis_get_class(...)` exposes that family query directly, and the coalesce dump now prints one `class ssa.X -> root=ssa.R size=N` line per value. Coverage now locks both a simple safe pair family and a deeper move chain family, proving the current analysis can collapse more than one edge into one coalesced class before later mainline worklist integration.
- 2026-04-24: Allocator select now consumes that coalesced-class surface directly, not only direct coalesce pairs. Preferred-color selection can reuse an already assigned color from any same-class value when the color is still legal for the current value, with direct-pair weights still used as the local tie-break when present. A chain-coloring regression now locks that transitive coalesced families color consistently through the public allocation entrypoint.
- 2026-04-24: Allocator coalescing has now crossed one more layer upward into the mainline plan/worklist surface. `ValueSsaAllocatorPlanItem` now records coalesced representative/size plus both raw and coalesced "effective" pressure (`initial/active_effective_degree`, `initial/active_effective_move_related`, `was_frozen`), and `value_ssa_compute_allocator_plan(...)` now classifies `freeze` / `spill` / `simplify` using that effective pressure rather than blindly treating family-internal accepted move edges as still-live move pressure. In practice this means a safe coalesced move family can now go straight through simplify without first being frozen only because of its own internal `mov` chain, while still preserving the older raw pressure fields for inspection. Focused allocator coverage now locks that coalesced-family no-freeze behavior and the richer plan dump surface.
- 2026-04-24: That coalescing-aware plan/worklist line now also treats external move pressure and freeze at family granularity rather than purely per-value granularity. If one member of a current coalesced family still has an active affinity edge to another family, every unscheduled member of that family now reports `initial_effective_move_related=yes`; and when `value_ssa_compute_allocator_plan(...)` chooses a `freeze` step for one member, it freezes the whole current family together instead of leaving sibling members looking independently move-related. Focused allocator coverage now locks both sides of that behavior through a manual fact-surface regression, which is a better fit for the current "coalesced families are beginning to act like one allocator object" direction even before full George/Briggs move-worklists land.
- 2026-04-24: Select is now also more intentional about preserving coalesced/affinity-preferred colors under simple pressure instead of only reusing them when they are already free. When no free color remains, and the preferred color for the current value is blocked by exactly one lower-priority interfering value, the allocator may now evict that specific blocker to keep the preferred color rather than falling back immediately to some other evictable color. A new artifact-driven public experiment hook, `value_ssa_allocate_function_from_plan(...)`, now exposes the current select + spill-slot-assignment layer over caller-provided prep/interference/coalesce/plan facts, and focused allocator coverage uses that hook to lock the targeted-coalescing-eviction behavior directly.
- 2026-04-24: That more intentional select layer is now also an explicit observable surface rather than only a hidden internal policy. `ValueSsaAllocationResult` now records whether a value used a preferred color, the source of that preference (`coalesce-direct`, `coalesce-class`, or `affinity`), the partner value that supplied it, and whether a targeted preferred-color eviction happened (including the evicted blocker). `value_ssa_dump_allocation_select_trace(...)` now prints those facts directly, and allocator regressions now lock both the ordinary direct-coalescing preferred-color case and the targeted-coalescing-eviction case through that surface.
- 2026-04-24: The allocator now also has a first explicit family-level move-state analysis above coalescing plus plan. `ValueSsaAllocatorMoveFamilyAnalysis` summarizes one item per current coalesced family with root, size, external move-neighbor count, external affinity weight, simplify-vs-spill removal counts, whether the family started move-related, whether it was frozen, and a small state label (`internal`, `released`, `frozen`). Public compute/query/dump APIs are now available under `include/value_ssa_alloc.h` / `src/value_ssa_alloc/value_ssa_alloc_move.inc`, and focused allocator coverage now locks both an all-internal coalesced family and a manually-constructed external-move frozen family.
- 2026-04-24: There is now also a first explicit allocator move/coalesce worklist surface above that family analysis. `ValueSsaAllocatorMoveWorklist` classifies current coalesced families into stable buckets (`coalesce-ready`, `freeze-pending`, `released`, `internal`), assigns a deterministic priority from family size plus external move/affinity pressure, and exposes public compute/query/dump APIs in `include/value_ssa_alloc.h` / `src/value_ssa_alloc/value_ssa_alloc_move_worklist.inc`. Focused allocator coverage now locks both an all-internal family and a mixed frozen-vs-coalesce-ready family, including the current ordering choice that `coalesce-ready` families sort before `freeze-pending` ones.
- 2026-04-24: That move/coalesce worklist layer is now also partially feeding back into allocator behavior rather than only being observed after plan construction. `ValueSsaAllocatorPlanItem` now records `move_work_class`, `move_work_priority`, and current family move pressure (`family_external_neighbor_count`, `family_external_affinity_weight_sum`), the public allocator-plan dump now prints those fields, and the current `freeze` tie-break now prefers lower move-work priority families first. In the current representative frozen-family scenario this means the lighter-pressure single-value family gets frozen first while the heavier sibling family is released, and focused allocator coverage now locks that resulting planner/worklist agreement.
- 2026-04-24: Planner items now also record one explicit family-work transition snapshot instead of only one current family-work classification. `ValueSsaAllocatorPlanItem` now carries `move_work_class -> move_work_next_class` plus family move pressure before/after the removal step, and the public allocator-plan dump prints those transitions directly. Current regression authority now locks both a trivial `internal -> internal` smoke case and a representative family-pressure case where one family is already `released -> released` by the time its member is removed while the lighter sibling family records `freeze-pending -> internal` on its final removal step. This is still not a full active-moves event log, but it is the first explicit per-step state-machine artifact above the allocator plan.
- 2026-04-26: The spill-cost surface is now slightly richer without changing allocator shape: allocation prep also records an approximate per-value call-density sum over the reachable blocks covered by each live range, and the current spill-cost consumers (`value_ssa_compute_allocator_plan(...)`, reverse select, and late retry) now include that signal alongside use count, covered-block count, loop-depth sum, and affinity sum. The approximation is intentionally conservative rather than instruction-exact, but it gives planner/select one more backend-flavored pressure cue before machine call-clobber modeling exists. Focused allocator coverage now locks both the producer side (`value_ssa_compute_allocation_prep(...)` reports the expected call-density contrast on a small call/live-through shape) and the consumer side (under otherwise tied manual facts, the lower-call-density value is chosen as the earlier spill candidate).
- 2026-04-26: That same spill-cost line now also has a first explicit local-vs-global asymmetry. When `ValueSsaAllocationPrep.single_block_live_ranges` is available, current spill-cost consumers add a small cross-block penalty to wider live ranges while leaving truly single-block ranges at the older baseline. This is deliberately modest, but it means planner spill ordering and select-time unique-blocker eviction now both prefer sacrificing a single-block value before an otherwise identical wider live range. Focused allocator coverage now locks both sides: a manual spill-plan tie where the single-block value is scheduled earlier, and an artifact-driven select tie where the single-block blocker is the one evicted.
- 2026-04-26: The older allocation-worklist priority is now also pulled a bit closer to that richer spill-cost surface instead of remaining on its original `affinity/degree/live-block/use` island. `value_ssa_compute_allocation_worklist(...)` still prioritizes affinity and degree first, but its priority now also incorporates loop-depth, call-density, and cross-block live-range penalties, so early allocator ordering is at least directionally aligned with the later spill heuristics. The public allocation-prep/worklist dumps now print `loops=` / `calls=` / `single_block=` directly beside those items too, and focused coverage locks one same-class worklist family where the value carrying the newer pressure cues now sorts ahead by priority.
- 2026-04-26: The allocator's call-pressure story is now also one notch more precise than block-level call density alone. Allocation prep now records `call_crossing_counts`, meaning the number of reachable call instructions across which a value remains live after the call. Current spill-cost consumers and worklist priority both include that signal alongside loop-depth, call-density, and cross-block penalties, and the public prep/worklist dumps now print it as `call_cross=`. Focused coverage now locks both the producer side (a small SSA fixture where one value truly lives through a call and another does not) and the consumer side (a manual spill-plan tie where the higher `call_cross` value is preserved longer).
- 2026-04-26: The richer spill-cost surface now also has its first proper explanation API instead of leaving review to decode one large `spill_live_range_cost` number by hand. `ValueSsaAllocatorPlanItem` now records separate live-block, loop-depth, call-density, call-crossing, and cross-block penalty components; `value_ssa_allocator_plan_get_spill_cost_detail(...)` exposes them directly; allocator-plan dumps append the same `detail=(...)` breakdown; and select traces now carry that same detail suffix at line end. Focused coverage now locks both the query helper and the representative smoke-plan dump shape, so later spill-policy tuning has a stable place to explain itself.
- 2026-04-26: Spill cost is now also a little less biased toward "where the live range exists" and a little more aware of "where the value is actually consumed". Allocation prep now records `use_loop_depth_sums` from def-use sites plus loop-depth analysis, worklist priority now consumes that signal, and spill-cost detail now exposes it as `use_loops=...` through both the plan item/query helper and allocator-plan/select-trace dumps. Focused coverage now locks both a producer-side loop fixture (values used in the loop pick up `use_loop_depth_sums`) and a consumer-side manual spill tie where the value with hotter loop uses is preserved longer.
- 2026-04-26: That "use-side pressure" story now also includes call-heavy blocks, which is the last spill-cost slice we explicitly wanted before cutting back to allocator mainline work. Allocation prep now records `use_call_density_sums` from def-use sites plus per-block call counts; worklist priority and spill-cost both consume it; and spill-cost detail now exposes it as `use_calls=...` through the plan item/query helper plus allocator-plan/select-trace dumps. Focused coverage now locks both the data surface and a manual spill-plan tie where the value with hotter call-adjacent uses is preserved longer. Treat this as the current endpoint of the spill-cost expansion line unless a concrete bug appears.
- 2026-04-26: Coalescing has now taken one real step back toward the allocator mainline instead of remaining a pure preference surface. `value_ssa_compute_allocator_coalesce_analysis(...)` no longer accepts only non-interfering pairs whose merged neighbor-class count is strictly below `K`; it now also accepts a conservative George-style safe case when one candidate class's neighbors are all either already adjacent to the anchor class or individually low-degree under the current color budget. Focused coverage updates the old budget-blocked toy pair into a new George-safe positive case (`merged_neighbors=2`, `can_coalesce=yes` at `K=2`) and adds a stronger negative family where the same merged-neighbor count is still rejected because both exposed neighbor classes remain high-degree and nonadjacent to the opposite side.
- 2026-04-25: That family-work transition story is now also available as a dedicated public trace surface rather than only being embedded inside `ValueSsaAllocatorPlanItem`, and it now includes explicit `freeze` events rather than only removal snapshots. `ValueSsaAllocatorMoveTransitionTrace` records one event per family-state change with root, event kind (`freeze` / `remove`), removal kind, move-work class before/after, active family member count before/after, and family move pressure before/after; `value_ssa_compute_allocator_move_transition_trace(...)` / `value_ssa_dump_allocator_move_transition_trace(...)` expose that sequence directly under `include/value_ssa_alloc.h` / `src/value_ssa_alloc/value_ssa_alloc_move_transition.inc`. The current implementation now re-simulates the planner's family-state evolution from allocation facts plus the current worklist policy while building the trace instead of merely copying fields from finished plan items, and focused allocator coverage now locks a representative progression including `event=freeze ssa.3`, `coalesce-ready -> freeze-pending`, `released -> released`, `freeze-pending -> internal`, and `family_members=1->0`.
- 2026-04-25: Allocator plan construction and move-transition tracing now share one internal scheduling engine instead of maintaining two parallel remove/freeze simulations. `value_ssa_compute_allocator_plan(...)` and `value_ssa_compute_allocator_move_transition_trace(...)` both delegate to that shared move-engine core, focused allocator coverage now locks that each plan item's remove-side transition fields agree with the corresponding trace `remove` event, and the refactor also re-locked the planner's original `simplify > freeze > spill` priority after the first draft briefly regressed to `simplify > spill > freeze`.
- 2026-04-25: Move-worklist state now also feeds allocator mainline behavior one step further than the earlier freeze-only tie-break. In the `simplify` phase, the planner now prefers `released` families before `freeze-pending`, and `freeze-pending` before plain `internal`, so once a move-related family has shed external pressure the remove walk keeps draining that family before falling back to unrelated simplify work. Focused allocator coverage now locks a mirrored-family case where a higher-id released family must be removed before a lower-id freeze-pending sibling, proving this is no longer an incidental value-id artifact.
- 2026-04-25: That same family move-work surface now also influences `spill` choice, not just `freeze` and `simplify`. When spill cost and active degree tie, the planner now prefers spilling `internal` values before `released`, `released` before `freeze-pending`, and `freeze-pending` before `coalesce-ready`, so still-promising move families survive tied spill decisions a little longer. Focused allocator coverage now locks a manual three-value tie where the first spill-remove must target a higher-id internal value ahead of a lower-id coalesce-ready competitor.
- 2026-04-25: `simplify` ordering is now also more explicitly family-state-driven than before. The planner no longer lets the older raw allocation work class (`move-hint`, `constrained`, `global`, ...) outrank `released` / `freeze-pending` / `internal` family state during simplify selection; instead it picks by family move-work rank first and only then falls back to raw work class. Focused allocator coverage now locks a manual released-family case where a `released` family member with a deliberately worse raw work class must still be removed before a `freeze-pending` competitor with a better raw class.
- 2026-04-25: That simplify-side family scheduling is now also stronger within one move-work class, and the move engine carries it more explicitly. The allocator now recomputes one current family move-work-class array after each freeze/remove step and uses that shared state when building plan items and trace transitions, rather than only classifying family state ad hoc at each observation site. On top of that, `simplify` now also uses `move_work_priority` within one family-state class, so among two `released` families the higher-priority one is drained first; focused allocator coverage now locks a case where a larger released family with a deliberately worse raw work class must still be removed before a smaller released competitor.
- 2026-04-25: The allocator move engine is now also a little more family-first in how it searches each phase. During one planner step it first picks the best candidate member inside each coalesced family for `simplify` / `freeze` / `spill`, and only then compares those per-family representatives globally. That means one family no longer competes through several sibling values in the same phase scan, which is a small but real move from "family facts influence per-value scheduling" toward "families are becoming the actual work objects". Current public artifacts are unchanged, but the internal scheduling shape is now closer to an eventual explicit family worklist mainline.
- 2026-04-25: That family-first phase search now also has a clearer internal execution surface. The move engine seeds one explicit family phase-entry per root with current move-work class/pressure and the best representative member seen for each phase, and the main loop now selects `simplify` / `freeze` / `spill` from those family entries instead of manually juggling several root-indexed scratch arrays inline. Focused allocator coverage now also locks that once a family is `released`, the planner picks the best member inside that family first (not merely some arbitrary sibling that happened to appear earlier in the worklist scan).
- 2026-04-25: The move engine now also carries one explicit active-family root list while building those family phase entries, instead of scanning every potential root id during phase selection. Current family entries are seeded only for roots that still have active members, each entry now keeps a current family queue priority based on best-member priority plus current family pressure/member count, and phase winner selection walks that active-family list rather than a full `0..value_count-1` root sweep. This is still rebuilt every planner step, not yet an incrementally updated family queue, but it is much closer to the internal shape needed for a real family-worklist allocator mainline.
- 2026-04-25: That active-family queue layer now also has a clearer notion of "current family queue priority" as a shared state fact. The move engine recomputes one family queue priority per active root from the best unscheduled member priority plus current family pressure/member count, seeds that into each current family phase-entry, and plan items now report that family-level queue priority rather than every sibling recomputing an unrelated per-value move-work score. Focused allocator coverage now locks both the existing "pick the best released-family member" story and the updated smoke dump fact that the same family's reported queue priority drops as the family shrinks across remove steps.
- 2026-04-25: The current family queue state is now also more explicitly phase-local inside the move engine. Each active family entry now carries one current family queue priority plus per-phase queue priorities for the best current `simplify` / `freeze` / `spill` representative, and phase-entry selection walks the active-family root list while using those shared current family queue facts. This still does not incrementally update the active family queue after each transition, but it means the engine is no longer recomputing family queue state ad hoc inside individual sibling previews; the current family queue is now a real intermediate layer with its own state shape.
- 2026-04-25: That phase-local family queue layer now also has explicit per-phase ready-root lists. After building the current family phase-entry pool, the move engine now materializes current `simplify` / `freeze` / `spill` family root lists and chooses each phase winner from the corresponding current family queue instead of scanning one generic active-family list with `has_*` checks inline. This is still a rebuild-each-step design, but the internal control flow is now much closer to the classic "multiple current worklists" shape even before true incremental queue maintenance lands.
- 2026-04-25: The move engine control flow is now also organized around one explicit "refresh current family queues -> pick current phase winner -> apply transition -> refresh current family queues" rhythm. The implementation still rebuilds current family queues globally after each `simplify` / `freeze` / `spill` transition, but the queue refresh itself is now a dedicated helper surface instead of several open-coded rebuild fragments around each phase arm. This is a structural step toward later incremental family queue maintenance: the main loop shape now already matches the future local-update allocator flow even though the update granularity is still coarse.
- 2026-04-25: That current-family-queue layer is now also carried through the move engine as an explicit internal state object instead of a long loose bundle of array/count parameters. Runtime facts (degrees, move-related masks, family pressure, queue priorities) are now grouped separately from current queue state (family phase entries plus active/per-phase root lists), which makes later incremental queue work a direct follow-on step instead of another prerequisite refactor.
- 2026-04-25: Active family roots are now also maintained incrementally after the initial seed. Because the current move-engine loop only removes values or freezes families, later queue refreshes now compact the already-active root list instead of rescanning every possible root id to rediscover the same surviving families each time. Runtime facts are still globally recomputed after each transition, but current root membership itself has now become persistent queue state, and focused allocator coverage now also locks that one released family's reported queue priority continues to drop as the family is drained member by member.
- 2026-04-25: The current queue now also owns one explicit family-member slice map over the allocation worklist. The move engine buckets worklist items by coalesced root once up front, preserves each family's internal worklist order, and then rebuilds each active family phase-entry by scanning only that root's member slice instead of rescanning the whole worklist during every refresh. This still stops short of affected-root-only queue refresh, but it is an important internal shift: candidate selection is now genuinely root-local, and focused allocator coverage now also locks that a released family's best member is still chosen correctly when its members are interleaved with other families in the original worklist.
- 2026-04-25: Queue refresh has now also crossed its first real local-update boundary. After each transition, the move engine now explicitly collects an affected-root set instead of blindly rebuilding every active family entry: remove transitions mark the changed family plus the families reached through that value's interference/affinity edges, while freeze transitions mark the frozen family plus the families reached through any still-active member's external affinity edges. Runtime facts are still recomputed globally, but current family phase-entry rebuild is now affected-root-only, and focused allocator coverage still locks the released-family best-member behavior even when original worklist order is interleaved.
- 2026-04-25: The current `simplify` / `freeze` / `spill` ready-root lists are now also incrementally maintained instead of being rebuilt from the whole active-family set every step. The move engine now keeps explicit ready flags for each phase-local root list, updates only the affected roots after an entry rebuild, clears drained families cleanly when a root goes inactive, and then compacts those ready-root arrays locally. This means queue bookkeeping itself, not only family-entry rebuild, has now crossed into affected-root maintenance while preserving the existing allocator-plan/transition behavior.
- 2026-04-25: Current family queue priorities now also follow that local-update story. The move engine no longer zeroes the entire family-priority table on each refresh; it clears only the active roots on a full reseed or only the affected roots on an incremental refresh, then recomputes queue priority only for the roots whose family entries are being rebuilt. That means the queue-facing state (`entries`, `ready lists`, and queue priorities) is now consistently updated on an affected-root basis, leaving the globally recomputed runtime analyses as the next obvious frontier.
- 2026-04-25: That frontier has now started to move too. `active_family_member_counts` are no longer rebuilt from a full scheduled-value scan on every incremental refresh: `remove` now decrements the removed value's root count in place, `freeze` leaves family size alone, and only a full reseed does the old global rebuild. `active_family_move_work_classes` now piggyback on that same boundary by being recomputed only for affected roots during incremental refresh. So the queue-facing state is fully affected-root-local, and the first runtime-family facts underneath it have crossed that boundary too; the main globally recomputed remainder is now value-level degree/move-related/effective state plus family external move-pressure summaries.
- 2026-04-25: Family external move-pressure summaries have now crossed that same boundary too. `active_family_external_neighbor_counts` and `active_family_external_affinity_sums` still rebuild globally on a full reseed, but incremental refresh now clears and recomputes only the affected roots' family-pressure rows by scanning those roots' active family members against current live affinity edges. That means the family-level runtime state (`member_counts`, `external pressure`, `move_work_classes`, `queue priorities`, entries, and ready lists) is now consistently maintained on an affected-root basis; the remaining global recomputation is concentrated in the value-level degree/move-related/effective analyses.
- 2026-04-25: That remaining value-level runtime layer has now started to split too. `active_degrees` are no longer recomputed from a full interference-graph scan on every incremental refresh: full reseed still rebuilds them globally, but a `remove` transition now simply zeroes the removed value's degree and decrements its still-active interfering neighbors in place, while `freeze` leaves degrees unchanged. The still-global remainder is now narrower and more explicit: `active_move_related`, `active_effective_degrees`, and `active_effective_move_related` still recompute globally on top of the updated degree state.
- 2026-04-25: `active_move_related` now follows that same split. Full reseed still rebuilds it globally, but incremental refresh now recomputes it only for values that live in affected roots by scanning those values' current live affinity neighbors. That leaves the still-global value-level remainder narrower again: `active_effective_degrees` and `active_effective_move_related` still rebuild globally on top of the updated `active_degrees` and `active_move_related` state.
- 2026-04-25: `active_effective_degrees` and `active_effective_move_related` have now also crossed into affected-root maintenance. Full reseed still rebuilds them globally, but incremental refresh now recomputes both only for values in affected roots: effective degree is rebuilt from each value's current live neighboring root set, and effective move-relatedness is rebuilt from the affected roots' current external affinity edges. That means the allocator move engine's incremental refresh path is now almost entirely local-state-driven; the broad global recomputation path is increasingly just the full reseed boundary.
- 2026-04-25: Spill-cost policy has now also taken a first real step beyond the original flat summary. Allocation prep computes a natural-loop depth per block from CFG backedges/dominators, accumulates that into `loop_depth_sums` for each value's covered live blocks, and the current spill-cost total now treats live range cost as `live_block_count + loop_depth_sum` rather than just raw covered-block count. Focused analysis coverage now locks representative loop-depth sums on a simple loop family, while allocator spill-cost regressions stay green on straight-line families because their loop contribution remains zero.
- 2026-04-25: Spill policy now also has a first explicit rematerialization-friendly hint. Allocation prep marks values defined by a direct `mov immediate` as rematerializable, focused analysis coverage now locks that producer fact, and the spill-phase comparator now prefers such values ahead of equally costly non-rematerializable ones before the older effective-degree tie-break. This is still deliberately narrow and does not yet rewrite spills into true rematerialization sites, but it gives the allocator a real "cheap to recompute" signal in spill ordering rather than leaving rematerialization friendliness as roadmap text only.
- 2026-04-25: Select-time eviction is now treated as a real confirmed-spill outcome instead of a half-accounted side effect. When reverse select evicts an already-colored lower-priority blocker to free a color for the current value, that blocker is now marked `spill-confirmed`, stale preferred-color/targeted-eviction metadata is cleared, and allocation dumps plus select stats report the result through the same intended-vs-confirmed channel as ordinary failed spill candidates. Focused allocator coverage now locks this with an artifact-driven three-value clique case and updates the existing select-stats baseline to include the newly visible confirmed spill.
- 2026-04-25: The local optimistic-eviction heuristic is now also more policy-aware on exact priority ties. If multiple unique blockers are evictable for the current value, select now prefers blockers that were already `spill-intended`, then blockers marked `rematerializable`, then lower spill-cost blockers, before falling back to value-id order. This is still intentionally conservative and local, but it is a real step from "evict any uniquely blocking low-priority neighbor" toward "use the allocator's own spill cheapness signals consistently during late select". Focused allocator regressions now lock both the spill-intended tie and the rematerializable tie directly through `value_ssa_allocate_function_from_plan(...)`.
- 2026-04-25: Reverse select now also has a first explicit optimistic-retry recovery pass instead of treating the first confirmed spill set as final forever. After the initial reverse-select coloring walk, the allocator now rescans currently spilled values and greedily recolors any one that has become colorable under the final neighborhood without performing new evictions. This already catches a real family the earlier allocator missed: a value can be confirmed-spilled when both colors are blocked, then later become colorable because a higher-priority value evicts one blocker and the replacement does not interfere with the older spilled value. Focused artifact-driven allocator coverage now locks that exact "late eviction frees an older spill candidate" shape, including the expected end state where the earlier `spill-remove` value becomes `optimistic-colored` and only the evicted blocker remains `spill-confirmed`.
- 2026-04-25: That recovery path is now also a formal observable result state rather than something only inferable from before/after dumps. `ValueSsaAllocationResult` now carries `spill_recovered_flags` with a public query helper, allocation dumps can annotate colored values as `spill-recovered`, select traces now distinguish `recovered-colored` and `optimistic-recovered`, and aggregate select stats now include `spill_recovered_count`. Focused allocator coverage locks both the ordinary retry family and the earlier targeted-eviction family through that surface, so later optimistic-coloring work can tell "direct optimistic color" apart from "confirmed spill that got recovered later" without replaying the whole select process mentally.
- 2026-04-25: Optimistic retry is now also one step closer to a real late select discipline instead of being limited to "pick up a completely free color if one appears". After the main reverse-select walk, retry may now perform one more conservative unique-blocker eviction for a currently spilled value when a target color is blocked by exactly one lower-priority colored neighbor. Preferred retry colors still take precedence, and otherwise the late retry eviction uses the same spill-cheapness tie-breaks as ordinary select eviction. Focused artifact-driven coverage now locks a representative five-value family where a `spill-remove` value is not recoverable by free-color reuse alone, but does become `optimistic-recovered` after the late retry stage evicts one remaining uniquely-blocking low-priority neighbor.
- 2026-04-25: Select stats now also distinguish direct optimistic success from late retry recovery explicitly. `ValueSsaAllocationSelectStats` now reports `optimistic_direct_count` and `optimistic_recovered_count` alongside the older aggregate `optimistic_colored_count`, so review no longer has to infer whether an optimistic color came from the initial reverse-select pass or from the later recovery pass. Focused allocator coverage now locks both sides: the baseline spill clique still contributes one direct optimistic color, while the late-retry families contribute recovered optimistic colors instead.
- 2026-04-25: Late retry recovery is now also split one notch further into "free-color recovery" versus "recovery that still required a retry-stage eviction". `ValueSsaAllocationResult` now carries `used_retry_eviction_flags` and `retry_eviction_blocker_value_ids`, there is a public query helper for them, select traces can print `retry-evict=ssa.N`, and aggregate select stats now include `retry_eviction_recovered_count`. Focused allocator coverage now locks both recovery flavors: the simpler late-retry family recovers without retry eviction, while the stronger five-value family only becomes `optimistic-recovered` after retry itself evicts one uniquely-blocking low-priority neighbor.
- 2026-04-25: Main-pass targeted eviction and retry-pass eviction are now also separated as distinct observable result channels rather than sharing one overloaded "evicted something" bit. The allocator now reserves `used_targeted_eviction_flags` for the original reverse-select step and uses the new `used_retry_eviction_flags` / `retry_eviction_blocker_value_ids` pair specifically for late recovery. This keeps later optimistic-coloring work honest about where an eviction came from, and focused allocator coverage now checks both sides explicitly: the older targeted-coalescing case still reports only main-pass targeted eviction, while the retry-eviction family reports only retry-stage eviction.
- 2026-04-25: The allocator now also has a first bounded blocker-recolor step ahead of actual spill on unique-blocker conflicts. When the main select walk or the late retry walk wants a color that is blocked by exactly one lower-priority colored neighbor, it now first tries to move that blocker to another legal color. If that succeeds, the waiting value gets the desired color and the blocker never becomes a confirmed spill. Focused coverage now locks both sides: the stronger retry-eviction family still needs a real retry spill of the blocker when recolor is impossible, while the sibling recolor family now keeps the blocker colored and still recovers the earlier `spill-remove` value.
- 2026-04-25: That same recolor-first logic has now also upgraded one older targeted-coalescing family from "spill-ish" behavior to true no-new-spill behavior. In the artifact-driven preferred-color case, the allocator can now preserve the preferred color by moving the former blocker aside directly instead of forcing it through a spill/recovery detour. Regression expectations are updated accordingly: the target value still records the preferred coalesced color, but the old blocker stays colored and is no longer marked as recovered from spill because it never had to leave the colored set.
- 2026-04-25: Main select no longer waits until after it has picked a blocker to notice that one candidate was recolorable and another was not. When multiple unique low-priority blockers are tied closely enough to be candidate evictees, the allocator now prefers a recolorable blocker first, so the decision itself is already biased toward "no new spill" rather than only the post-choice handler being recolor-aware. Focused allocator coverage now locks a representative 3-color family where the target value has two same-priority blocker options; the allocator now chooses the blocker that can move to a third shared color, leaving every value colored and producing one direct optimistic success instead of an unnecessary spill.
- 2026-04-25: Retry candidate ordering has now also started to internalize that same "avoid creating a new spill if possible" bias. When two spilled values are otherwise close enough that retry is comparing their own priority/cost lanes, recovery options that can free their blocker by immediate recolor are now preferred over recovery options that still require a real blocker spill. Existing late-retry families still pass unchanged, but the policy is now one notch more coherent: both the main select walk and the retry walk are beginning to rank candidates by net spill avoidance, not only by the waiting value's own local score.
- 2026-04-25: Retry ordering is now also one notch more family-aware. When multiple provisional spill candidates are otherwise tied tightly enough that retry is comparing their recovery options, options with stronger preferred-color sources (`coalesce-direct`, then `coalesce-class`, then `affinity`) now win before the older value-local spill-cost tie-breaks. Focused artifact-driven coverage now locks a representative family where a lower-id generic candidate and a higher-id coalesced candidate are both recoverable through the same newly freed color, but only one can come back; retry now chooses the coalesced/preferred recovery instead of defaulting to the old id-driven fallback.
- 2026-04-25: Retry ordering is now also starting to value larger coalesced families, not only stronger preferred-source kinds. When two provisional spill candidates are both recoverable through `coalesce-class`-style recovery and are otherwise tied closely enough, retry now prefers the candidate whose recovery preserves the larger coalesced class. Focused artifact-driven coverage now locks a representative 8-value family where two same-priority spill candidates are both recoverable through the same newly freed color, both through `coalesce-class`, but one belongs to a class of size 2 and the other to a class of size 3; retry now recovers the size-3 family member instead of falling back to the lower-id size-2 candidate.
- 2026-04-25: That same size-aware retry rule now effectively establishes the first explicit "family value beats scalar fallback" checkpoint in the allocator. Once retry has determined that two provisional spill candidates are in the same preferred-recovery tier, it no longer drops straight to local spill-cost/id order; it first asks which recovery preserves the larger coalesced family. This is still far short of true merged-node allocation, but it is now a concrete, regression-locked place where family-scale preservation outranks scalar fallback policy.
- 2026-04-25: Family-size awareness has now also moved forward from retry into the main select walk itself. When generic unique-blocker eviction must choose between otherwise similar low-priority blockers, main select now prefers evicting the blocker from the smaller coalesced family before it falls back to the older scalar tie-break stack. Focused artifact-driven coverage now locks a representative 4-value family where two same-priority blockers would both free the target color equally well, but one belongs to a class of size 2 and the other to a class of size 1; main select now spills the size-1 blocker and preserves the larger family instead of defaulting to the lower-id larger-family blocker.
- 2026-04-25: That means family-scale preservation is no longer confined to late retry/recovery policy. The allocator's first generic unique-blocker spill choice now already has a small but real merged-aware preference: after the earlier bounded local checks, it will sacrifice the smaller coalesced family first. This is still a long way from merged-node allocation, but it is the first regression-locked place where the initial spill-creation path itself is making a family-aware choice rather than a purely scalar one.
- 2026-04-25: Family-aware policy has now also reached the spill-risk planning layer itself. In spill-phase tie cases where the current cost/rematerialization gates are otherwise equal, the allocator plan now orders smaller coalesced classes ahead of larger ones for `spill-remove`, instead of falling straight through to the older scalar tie-break stack. Focused artifact-driven coverage now locks a representative 3-value plan family where two equal-cost spill candidates differ mainly by coalesced class size; the singleton class is now scheduled into spill earlier than the size-2 class. This is still narrow, but it is the first place where family value influences "who becomes a spill candidate" rather than only "how later select/retry treats that candidate".
- 2026-04-25: That spill-risk planning layer is now also one notch more family-pressure-aware than only "smaller class first". When spill cost, rematerialization, and move-work class tie, the planner now prefers spilling the family with lower external affinity weight and then fewer external move-neighbor families before it falls back to coalesce-class size and later scalar tie-breaks. Focused allocator coverage now locks both sides with manual fact surfaces: one family pair that differs only in external affinity pressure and one pair that differs only in neighbor-family count, and in both cases the lighter-pressure family is now scheduled earlier for `spill-remove`.
- 2026-04-25: That family-pressure signal is now also consumed one phase later by main select itself instead of stopping at planning. When generic unique-blocker eviction must choose between otherwise tied low-priority blockers, main select now prefers the lighter-pressure blocker family first: weaker move-work class, then lower external affinity weight, then fewer external move-neighbor families, before it falls back to coalesced-class size and the older scalar spill-cost stack. Focused allocator coverage now locks two artifact-driven families where blocker class size is held equal and only family pressure differs; in both the allocator now spills the higher-id lighter-pressure blocker instead of defaulting to lower-id order or only class-size preference.
- 2026-04-25: That same family-pressure signal now also reaches late retry candidate selection instead of stopping at main select. Once two spilled values reach the same retry tier and the earlier retry gates have tied, retry now prefers recovering the heavier-pressure family first: stronger move-work class, then higher external affinity weight, then more external move-neighbor families, before it falls back to scalar spill-cost/id order. Focused allocator coverage now locks both sides: one true double-candidate late-retry family where both candidates become free-color-recoverable only after a later select step frees one color, and one stronger double-candidate family where both candidates still require generic retry eviction of the same newly-exposed blocker. In both cases retry now recovers the higher-id heavier-pressure family member instead of defaulting to lower-id order.
- 2026-04-25: Retry policy is now also one notch more explicit on the blocker side, not only on "which spilled value should come back". For same-tier retry-eviction candidates, blocker-family comparison is now consulted before the older candidate-local spill-cost/id fallback, so the allocator can prefer the recovery that sacrifices the lighter blocker family even when the competing spilled value itself has the larger scalar spill cost. Focused allocator coverage now locks a 7-value family where the lower-id candidate has strictly higher spill cost but would require evicting the heavier blocker family, while the higher-id candidate can be recovered by evicting a lighter blocker family; retry now brings back the higher-id candidate first and only then recovers the other one.
- 2026-04-25: Family-pressure comparison in main select and late retry now also counts current active family member count instead of leaving that signal implicit inside queue-priority math only. Focused allocator coverage now locks one main-select family where blocker class size and external pressure tie but one blocker family has fewer active members, and one late-retry family where candidate external pressure ties but one candidate family still has more active members; main select now sacrifices the smaller surviving family first, while retry now preserves the larger surviving family first.
- 2026-04-25: The family-pressure policy is now also slightly more structurally unified across allocator stages rather than only behaviorally similar by convention. Planner spill ordering now uses the same shared family-pressure comparison surface already consumed by main select and late retry (`move_work_class`, external affinity weight, external neighbor count, active family member count), and the move engine's spill-family queue tie-break now also uses spill-phase move-work rank instead of reusing simplify-phase rank there. This does not yet make the allocator a full formal move-worklist mainline, but it does mean adjacent stages are starting to share one explicit definition of "lighter family" / "heavier family" rather than carrying subtly divergent local copies.
- 2026-04-26: The public move-family / move-worklist surfaces now also distinguish raw external move pressure from actually safe external coalesce opportunities. `ValueSsaAllocatorMoveFamilyItem` / `ValueSsaAllocatorMoveWorkItem` now expose `coalesce_ready_neighbor_family_count`, the move-family and move-worklist dumps print `ready_neighbors=...`, and `value_ssa_compute_allocator_move_worklist(...)` only classifies a released family as `coalesce-ready` when that safe-neighbor count is nonzero. Focused allocator coverage now locks both a manual positive `coalesce-ready` family and the real negative case that a family with unsafe external affinity must remain merely `released`.
- 2026-04-26: That `safe-ready` distinction now also threads through the allocator's dynamic move-engine rather than living only in the public static worklist helpers. `value_ssa_compute_allocator_plan(...)` / `value_ssa_compute_allocator_move_transition_trace(...)` now derive family move-work class from active safe-ready neighbor counts while still using raw external neighbor counts as pressure/priority signals, so dynamic plan items and `freeze` traces no longer promote an unsafe-but-affine family to `coalesce-ready` just because some external move edge is still live. Focused allocator coverage now locks the updated negative transition shape (`released -> freeze-pending`, not `coalesce-ready -> freeze-pending`) together with the spill-family plan expectations that should stay merely `released`.
- 2026-04-26: That same dynamic safe-ready line is now also first-class observable state instead of only an internal classification input. `ValueSsaAllocatorPlanItem` and `ValueSsaAllocatorMoveTransitionStep` now record safe-ready neighbor counts beside raw external neighbor counts, and the allocator plan / move-transition dumps now print `family_pressure=(neighbors=...,ready=...,affinity=...)`. Focused allocator coverage updates the plan dump authority accordingly, so later move-worklist/coalescing work can inspect not only the derived family class but also the concrete active-safe-neighbor reason behind it.
- 2026-04-26: That safe-ready signal now also feeds back into allocator family-pressure policy itself rather than remaining a read-only explanation field. Shared family-pressure comparison now treats higher safe-ready neighbor count as a heavier family before it falls through to raw affinity / external-neighbor / member-count ties, and move-family queue priority likewise scores safe-ready neighbor count ahead of raw external-neighbor pressure. Focused allocator coverage now locks both a static move-worklist tie where the family with more ready neighbors must sort first and a retry tie where the allocator recovers the family with more safe-ready continuation first.
- 2026-04-26: That same family-pressure policy now also distinguishes ready-edge weight from mere ready-edge cardinality. Static move-family/worklist analysis, dynamic move-engine state, allocator plan items, and move-transition steps now track `coalesce_ready_affinity_weight_sum`; shared family-pressure comparison and move-family queue priority now consult that ready-affinity weight before falling through to the older raw external-affinity lanes. Focused allocator coverage now locks the stronger interpretation indirectly through the updated move-worklist/retry family-pressure line, so later coalescing-mainline work can preserve not only "more legal continuations" but also "heavier legal continuations".
- 2026-04-26: That safe-ready pressure line now also exposes one explicit best-partner slice instead of only totals. Static move-family/worklist analysis, dynamic move-engine state, allocator plan items, and move-transition steps now track the current strongest safe-ready partner root plus its aggregated ready-affinity weight, and shared family-pressure comparison / family queue priority now consult that best-partner weight after the broader ready-neighbor / ready-affinity totals. Focused allocator coverage now also locks both a move-worklist tie and a retry tie where total ready pressure is close enough that the stronger best-partner signal must decide the outcome.
- 2026-04-26: The allocator now also has its first explicit coalesce-opportunity agenda surface instead of leaving "which pair should we try next?" implicit inside family-pressure fields. `ValueSsaAllocatorCoalesceOpportunityAgenda` now summarizes one item per currently surfaced best-partner pair with directional preference flags, mutual-best status, aggregated directed preference weights, endpoint move-work classes/priorities, and one deterministic opportunity priority; public compute/query/dump APIs are available under `include/value_ssa_alloc.h` / `src/value_ssa_alloc/value_ssa_alloc_coalesce_agenda.inc`. Focused allocator coverage now locks a manual agenda where a mutual pair must sort ahead of a stronger one-sided pair, which is the first explicit step from family-pressure heuristics toward a future partner-driven coalescing mainline.
- 2026-04-26: That coalesce-opportunity agenda now also participates directly in allocator behavior instead of remaining a read-only companion surface. On simplify/freeze-side family ties inside `value_ssa_alloc_move_engine`, coalesce-ready families now consult the agenda-derived best-partner signal (`mutual_best`, then best-partner weight) before falling back to older queue-priority ties, and focused allocator coverage now locks a plan family where a mutual-best ready family is chosen ahead of a comparable one-sided ready family. This is still not yet a true merge action, but it is the first point where an explicit pair-agenda layer affects transition choice rather than only explaining it afterward.
- 2026-04-26: That pair-agenda participation is now one notch stronger than "only break exact local ties". During simplify/freeze-side family candidate selection, the move engine now gives a coalesce-ready family with a live mutual-best pair agenda priority a direct chance to outrank a merely higher generic family queue priority, and focused allocator coverage now locks a representative plan where the mutual-best ready family must be selected ahead of a competing one-sided family even after that competing family's raw member priorities are raised. This is still not yet structural node merging, but it means explicit pair opportunities are beginning to influence the mainline transition order itself rather than only the last comparison layer.
- 2026-04-26: The explicit coalescing line now also has its first real action event, not only pair ordering. `value_ssa_alloc_move_engine` can now consume one live mutual-best pair as a `coalesce` transition before later remove steps, the move-transition trace records both family endpoints plus before/after family-pressure snapshots for that pair, and focused allocator coverage now locks a representative freeze-side mutual pair that must emit `coalesce` before the later simplify removals. This is still conservative family-state consumption rather than true merged-node allocation or alias maintenance, but it is the first point where a surfaced coalesce opportunity is actually acted on by the mainline.
- 2026-04-26: That coalesce-action line now also feeds a real downstream merged-family alias surface instead of remaining only runtime trace state. `ValueSsaAllocatorPlan` now records final runtime coalesce roots/class sizes, later plan-item previews use that runtime alias map, and artifact-driven select can now consume those final merged families for `coalesce-class` preference even when the static coalesce roots stay separate. Focused allocator coverage now locks both the final merged roots from a manual coalesce event and a direct `allocate_function_from_plan(...)` case where runtime merged-family aliasing is the thing that enables the preferred-color decision.
- 2026-04-26: That runtime merged-family alias is now also being consumed by the move-engine body itself instead of remaining only a downstream artifact. After an explicit `coalesce` event, the move engine now rebuilds current family member slices from the runtime merged roots, merges active family member/spill counters into the surviving representative, refreshes affected roots through the runtime root map, and later remove steps now run under the merged representative root rather than the old pre-merge sibling root. Focused allocator coverage now locks that stronger behavior on the representative mutual-pair family by checking both final runtime roots and the later `ssa.2` remove step running under merged root `ssa.0`.
- 2026-04-26: That same queue body now also keeps its live root lists closer to runtime representatives instead of leaving more old-root shadow around until later comparisons. `affected_family_roots`, `active_family_roots`, and phase-ready root compaction now normalize through the runtime merged-family root map and deduplicate representatives, so the family-phase entry pool and pair-selection walk are more often iterating the surviving merged root directly instead of first carrying stale sibling roots and only indirectly observing the merge through later state tables.
- 2026-04-26: The queue-side decision helpers now also consume those merged representatives more directly instead of only inheriting cleaner root lists. `compute_family_phase_pair_priority`, `pick_best_family_phase_candidate`, and `pick_best_family_phase_coalesce_action` now canonicalize their working root ids through the runtime merged-family map before checking partner relationships, comparing pair priority, or breaking family winner ties. This is still not a true merged-family object model, but the phase chooser is now less dependent on “root-indexed arrays plus later normalization” and more directly reasoning over the surviving representative root.
- 2026-04-26: The move-engine queue/entry state now also clears some loser-root identity eagerly instead of only waiting for later refresh passes to wash it out. When a runtime `coalesce` event merges two roots, the current queue state immediately clears the losing root's phase-entry/ready-flag identity, and active-root refresh now also scrubs obsolete loser-root flags when canonicalization/dedup proves a surviving representative has already taken over. This still lives inside root-indexed storage, but the queue surface is less willing to treat the losing root as a live first-class identity after merge.
- 2026-04-26: That cleanup is now also enforced one layer more uniformly at queue-entry time instead of only through a few scattered refresh helpers. Before family-phase entry rebuilds run, the current move engine now explicitly normalizes both `active_family_roots` and `affected_family_roots` through the runtime merged-family map and scrubs obsolete loser-root identity while deduplicating representatives. So the family-phase entry pipeline is now entering its main rebuild pass with a more coherent representative-root set rather than relying as heavily on downstream spot-normalization.
- 2026-04-26: The runtime family facts are now also starting to present a more representative-owned access surface instead of forcing each queue helper to index the raw root tables directly. `value_ssa_alloc_move_engine` now has a small representative-family state view helper that reads current member count, external pressure, best-partner facts, queue priority, and move-work class for the surviving root in one place, and the phase-entry rebuild / family-queue-priority path now consume that helper instead of open-coding the same root-array reads repeatedly. The storage is still array-backed, but the queue side is beginning to treat those arrays as implementation detail rather than the primary semantic surface.
- 2026-04-26: That representative-owned state view is now also reaching deeper into the move-engine body instead of stopping at entry reset. Best-partner roots read through that helper are now canonicalized through the runtime merged-family map, phase-entry rebuild re-centers its member-slice scan on the surviving representative root, pair-choice helpers consume representative entries rather than more raw root slots, and remove/freeze/coalesce-leader after-state snapshots now read `best_partner`, external pressure, and move-work class from the representative state view. Focused allocator coverage now also locks the post-coalesce leader step having no stale best-partner residue (`best_partner=-0@0`, no external pressure) once the merged family has no remaining safe-ready partner.
- 2026-04-26: The remaining write-side runtime-family paths are now also a little less willing to treat stale loser roots as first-class update targets. Incremental move-work-class refresh, family move-pressure clear/recompute, and queue-priority reset now all collapse their incoming root sets through the runtime merged-family map before writing back table state, while the one place that still wants the loser root's exact post-merge slot state for observability (`coalesce` trace partner-after fields) now reads it through an explicit slot-local helper instead of reusing representative-owned family-state reads. This is still not a full merged-family object store, but the "read representative state here, accidentally write stale old-root state there" mismatch is smaller than before.
- 2026-04-26: The runtime merged-family alias is now also being consumed one step deeper by the allocator's color-choice policy rather than stopping at queue/trace state. `value_ssa_alloc_select` now summarizes the already-colored members of one runtime merged family into a family color state and prefers the dominant surviving family color for later siblings before falling back to scalar affinity-only choice. Focused allocator coverage now locks a representative from-plan family where the merged family has already split across colors `0/1/1`; the later sibling now follows the family-dominant color `1` instead of merely inheriting the lowest-color earlier sibling. That is still not true merged-node coloring, but it is a real step from "runtime alias affects bookkeeping" toward "runtime alias affects actual allocation policy".
- 2026-04-26: That runtime-family policy surface now also reaches the blocker/retry side of select rather than only the "which color do I prefer?" side. Main-select blocker-family choice and late-retry candidate-family choice now compare a small dynamic runtime family activity summary for the surviving representative (currently colored members versus already spilled members) before they fall back to the older static family-pressure snapshot. Focused allocator coverage now locks both directions: one main-select family where two blocker families are statically tied but only one still has two colored siblings in play, and one late-retry family where two spill candidates are statically tied but only one still has a colored sibling family already on the board. This is still far short of true merged-node allocation, but the allocator is now beginning to protect or sacrifice families as runtime representative objects, not only as static plan labels.
- 2026-04-26: The generic blocker path now also becomes structurally more representative-owned, not only behaviorally more family-aware. In both main generic eviction and retry-side generic eviction-option discovery, the allocator now first picks one best blocker inside each runtime merged family and only then compares those family representatives globally. That means a family with several sibling blockers no longer gets multiple independent chances to win just because its members occupy several colors; generic sacrifice is now closer to "choose one blocker family, then one blocker member inside it" than to "scan every blocker value and hope family-aware tie-breaks compensate later".
- 2026-04-27: The late-retry candidate entrance now follows that same family-object direction too. Before the allocator compares spilled values globally for recovery, it now first picks one best recoverable spilled representative inside each runtime merged family and only then compares those family representatives. Focused allocator coverage now locks a small family where two spilled siblings share one merged family but only the stronger sibling is allowed to stand in as the family's retry representative while the weaker sibling remains spilled. This still is not a full merged-node retry worklist, but it means the top-level retry race itself is no longer "one entry per spilled value" once those values already belong to the same surviving merged family.
- 2026-04-27: That retry-family representative step is now also the actual top-level recovery entry discipline, not only a late tie-break idea layered over a scalar scan. The allocator now materializes one recoverable candidate per runtime merged family before global retry comparison begins, so a family with multiple spilled siblings cannot crowd the retry race with several near-duplicate entries and hope the later family-aware comparator cleans it up. This is still smaller than a full explicit retry-family worklist, but the entry object for late retry is now materially more family-shaped than before.
- 2026-04-27: Assignment-side family policy now also has one first explicit "color support" signal above raw family activity. When generic blocker selection or retry candidate comparison reaches the family-aware lanes, the allocator can now compare how many already-colored members of the runtime merged family currently sit on the relevant color, rather than only how many family members are alive in aggregate. Existing allocator regressions remain green across that change, so the current family-aware policy no longer treats "two live families" as assignment-equivalent when one family has already built a stronger color foothold than the other.
- 2026-04-27: That family color-support signal now also reaches recolor assignment itself instead of stopping at candidate comparison. `value_ssa_alloc_find_recolor_color_for_blocker(...)` now ranks legal recolor targets by current runtime family support for the candidate color before falling back to deterministic color order, so a recolorable blocker will preferentially move onto a color its merged family already occupies more strongly rather than just taking the first legal spare slot. Existing allocator regressions stay green across that change, so recolor is now modestly but explicitly helping maintain merged-family color cohesion.
- 2026-04-27: That recolor-cohesion rule now also threads through the preferred-color targeted-eviction branch instead of only the generic blocker path. The allocator now passes plan/runtime family context into `try_evict_lower_priority_neighbor_for_color(...)`, so even when a preferred coalesce/family color forces the allocator down the targeted "move this one blocker off my requested color" path, the blocker's recolor still prefers the strongest current family-supported destination color rather than bypassing the newer cohesion rule.
- 2026-04-27: That targeted-family recolor line is now also regression-locked directly. Focused allocator coverage now includes a small `coalesce-direct` family where the target sees all colors blocked, must evict the unique blocker on the preferred family color, and that blocker can legally move to either of two alternate colors; the allocator now chooses the alternate color already occupied by the blocker's own merged-family sibling rather than the lower-numbered spare color. So "family cohesion survives targeted preferred-color eviction" is no longer only an inferred property of the code path.
- 2026-04-27: Runtime family color preference no longer depends exclusively on a separate `coalesce_analysis` object being threaded through select. When final runtime merged-family arrays are absent but the allocator plan itself still carries coalesced family roots/class sizes, select/retry can now fall back to those plan-side family facts to recover merged-family color preference. Focused allocator coverage now locks a `from_plan(..., NULL)` family where one value should still follow the already-established family color even though no external coalesce-analysis artifact is provided.
- 2026-04-27: The authority order on that fallback is now also tightened so new plan-side family behavior does not accidentally override older explicit analysis-backed paths. Select/retry now resolve family color facts in the order `final runtime merged-family arrays -> coalesce_analysis -> plan-side family facts`, and focused allocator coverage keeps both sides alive: the newer `from_plan(..., NULL)` family-color fallback stays green, while the older analysis-backed targeted/coalesce/retry fixtures continue to see their original family interpretation rather than being silently reinterpreted through partial plan metadata.
- 2026-04-27: Free-color assignment now also has a first dedicated family-aware policy hook instead of being only an inline "take the first unblocked color" fallback. The current helper is intentionally conservative and still degrades cleanly to the old lower-color order when no runtime family color support exists, but this gives later allocator work one stable place to grow genuine family-owned free-color policy without reopening the middle of `value_ssa_alloc_select_colors(...)` each time. Existing regressions remain green across the refactor.
- 2026-04-27: Blocker recolor now also consumes the same preferred-color/runtime-family surface more deeply instead of stopping at raw family support count. When a targeted or retry-side blocker can legally move to multiple alternate colors inside its merged family, recolor now first asks the usual preferred-color query over the legal recolor mask, so it can keep following the stronger direct family/coalesce partner rather than immediately collapsing to lower color order on support-count ties. Focused allocator coverage now locks a `coalesce-direct` targeted-eviction family where the blocker may legally move to two equally supported sibling colors, and the allocator is expected to choose the heavier direct-partner color.
- 2026-04-27: That recolor-target signal now also feeds back into generic victim choice rather than only the later recolor assignment step. When two unique low-priority blockers are both recolorable, main generic eviction now prefers the blocker whose legal recolor destination already has stronger support inside that blocker's merged family before it falls back to the older blocked-color order. Focused allocator coverage now locks a 7-value main-select family where the old policy would have chosen the lower blocked color, but the allocator is now expected to sacrifice the blocker whose recolor lands on the stronger family-supported target color instead.
- 2026-04-27: The retry side now also starts to consume that recolor-target family signal instead of only asking whether a blocker is recolorable at all. When two eviction-backed recovery candidates are otherwise still close, retry may now prefer the candidate whose blocker would recolor onto a more strongly supported family color before it falls back to the older blocker/value-local tie-breaks. Existing allocator regressions stay green across that change, so late recovery is now one notch less blind to the post-eviction family state it would create.
- 2026-04-27: The allocator result/query surface now also records blocker aftermath for targeted/retry eviction instead of only "which blocker was involved". `ValueSsaAllocationResult` can now report whether the blocker was recolored and which new color it took, select traces print that as `targeted-evict=ssa.N->colorM` / `retry-evict=ssa.N->colorM`, and focused allocator coverage now locks both a targeted-family recolor case and a retry recolor case on that richer surface. That does not finish the dedicated retry-order fixture yet, but it turns blocker-recolor behavior into a first-class observable contract instead of an inference from the final color map.
- 2026-04-27: That blocker-aftermath surface now also covers main generic eviction rather than stopping at targeted/retry-specific paths. `ValueSsaAllocationResult` can now report the blocker used by ordinary main optimistic eviction plus whether that blocker was recolored and to which color, select traces print it as `main-evict=ssa.N` or `main-evict=ssa.N->colorM`, and focused allocator coverage now locks both a main-recolor positive case and a main-family non-recolor case. So bounded eviction aftermath is now observable across all three main allocator lanes: main generic eviction, targeted preferred-color eviction, and retry eviction.
- 2026-04-27: With that broader blocker-aftermath surface in place, the regression strategy for the still-missing retry-order fixture is now simpler and narrower. Existing allocator coverage no longer has to infer blocker fate indirectly from whole-trace color evolution; it can directly assert which blocker was chosen and whether that blocker was recolored or sacrificed. That does not by itself finish the clean retry-order fixture, but it removes one of the main reasons earlier attempts kept turning into noisy trace archaeology.
- 2026-04-27: Retry-side generic eviction now also uses the same richer blocker-aftermath comparison surface when choosing one blocker representative per runtime merged family, instead of first collapsing each family through a weaker blocker-only local rule and only then comparing those representatives globally. In practice that means retry-family representatives now preserve the same recolorability / recolor-target support / runtime family color-support policy already used by main generic eviction before the late recovery race even begins, so the "family entry object" on generic retry is slightly closer to a real merged-family allocator decision than before.
- 2026-04-27: The late-retry entrance is now also a little more structurally family-owned internally, not only behaviorally family-aware. Instead of carrying separate `family_best_value_ids[]` and `family_best_options[]` arrays through recovery selection, retry now materializes one explicit internal family-candidate object per runtime merged family (`family root + chosen spilled representative + chosen recovery option`) and uses the same shared recovery comparator both to update each family candidate and to pick the global winner. That does not yet make retry a full public worklist surface, but it means the entry object itself is now materially closer to "one merged-family recovery candidate" than to "two synchronized scalar arrays with family semantics implied from outside."
- 2026-04-27: That same object-shaped cleanup has now also reached generic blocker selection itself instead of stopping at recovery winners. Main generic eviction and retry generic-eviction option discovery no longer each rely on ad hoc per-family `best blocker id / color` parallel arrays; both now first materialize one explicit internal blocker-family candidate per runtime merged family (`family root + chosen blocker representative + blocked color + recolor aftermath`) and then select from those family candidates. This is still internal structure rather than a public allocator surface, but the generic sacrifice side is now less "scalar scan plus family comments" and more "one blocker-family candidate object per merged family," which is a better fit for the later merged-family allocator mainline.
- 2026-04-27: That generic blocker-family line now also shares its collection entrypoint instead of having two near-parallel scans that merely happened to produce the same shape. Main generic eviction and retry generic-eviction option discovery now both feed their precomputed unique-blocker color facts through one shared family-candidate collector before later family-level selection happens. So the two generic lanes are no longer only converging on the same *data shape*; they now also converge on the same "how do we materialize blocker-family candidates from current color facts?" step, which trims another duplicated slice out of the merged-family allocator mainline.
- 2026-04-27: That shared generic-blocker collector is now also regression-locked on the main-select side rather than only being trusted through retry coverage plus green totals. The existing main recolor-target-support family now explicitly asserts both the chosen generic blocker (`ssa.1`) and its recolor aftermath (`->color2`), so "main generic eviction picked the right representative from that blocker family" is now a direct contract instead of an inference from the final color map.
- 2026-04-27: The select/retry result-writing side is now also a little less fragmented than before. Preferred-color claim metadata, colored-result assignment, and eviction-aftermath recording are no longer hand-written separately at each main-select and late-retry callsite; they now flow through small shared helpers before the lane-specific control flow continues. This is still internal cleanup rather than a new public surface, but it means the allocator is starting to share not only candidate collection and comparison semantics, but also the "how do we record the chosen representative decision?" step across preferred / targeted / generic / retry paths.
- 2026-04-27: That sharing line now also reaches the *claim* side of the preferred branch rather than only the later metadata writeback. Main select and late retry both now enter through one shared preferred-claim helper that can surface either a direct preferred-color claim or a `preferred-evict` claim with the blocker/recolor aftermath already attached, after which each lane still decides where generic free colors or generic eviction fit in its own ordering. This is still not a full one-function select engine, but the allocator now shares more of the actual representative-decision semantics on the preferred branch, not only the final bookkeeping.
- 2026-04-27: Main select itself now also has a more explicit internal "claim -> apply" skeleton rather than one long open-coded branch that interleaves choice and result mutation. The current select walk now resolves one internal claim object (`direct preferred`, `free generic`, `targeted preferred-evict`, `generic evict`, or no color) and then applies that claim through one shared path, instead of writing preferred/generic/targeted aftermath directly at several branch-local sites. This is still internal structure, but it is the clearest current step toward making the four select lanes (`preferred / targeted / generic / retry`) look like one representative-owned decision protocol rather than several partially overlapping ad hoc flows.
- 2026-04-27: That same protocol shape now also reaches late retry's final winner application instead of stopping at main select. Retry recovery now resolves one internal claim object for the chosen spilled representative (including the `preferred-evict` vs generic-evict aftermath and the special "also record retry eviction" case) and then applies that claim through one shared recovered-result path, rather than hand-mutating spill/color/preferred/eviction fields in a separate retry-only tail. So the allocator now shares not only candidate collection and claim formation, but also more of the final representative-commit semantics across main select and retry.
- 2026-04-27: The main/retry similarity now also extends one layer deeper into the eviction-backed branch itself. Instead of each side separately performing "try targeted/generic eviction, inspect blocker recolor outcome, then fill claim fields", they now share one internal eviction-backed claim materializer that turns an eviction-flavored option into a representative claim object plus aftermath metadata. The lanes still differ in ordering and whether they also record retry-eviction sideband state, but the core "one option in, one eviction-aware claim out" step is no longer duplicated across main select and retry.
- 2026-04-27: That three-stage shape is now also more explicit on the main-select side itself rather than only being implied by helper reuse. Main select now walks much closer to a real `collect blocked-color facts -> form option -> materialize claim -> apply claim` skeleton, and retry now reuses the same option/materialization spine for its own winner path. This is still not a single public "select protocol" API, but it is the first point where both sides are materially organized around the same internal stage order instead of only sharing isolated helpers inside otherwise different flows.
- 2026-04-27: That select/retry protocol line now also has one explicit shared query surface instead of each lane assembling blocked colors, blocker counts, and preferred-option state in its own ad hoc shape. `value_ssa_alloc_select` now threads a single reusable `select query` buffer through the reverse-select walk, late retry reuses the same query protocol while scanning spilled-family candidates, and both lanes now form their option from that same query object before later claim materialization/application happens. Existing allocator regressions remain green across the refactor, so this is a structural mainline step toward one merged-family select protocol rather than only helper-level code sharing.
- 2026-04-27: The move-engine side now also consumes a more explicit representative-owned family runtime surface instead of making later queue/phase-entry consumers re-read the same merged-family state through a wide set of root-indexed parallel arrays. A dedicated internal `family_runtime_facts` array now mirrors the current merged-family member/pressure/partner/queue/work-class state, phase-entry rebuilds plus plan-preview generation now read that unified family surface, and obsolete loser-root cleanup now clears the representative-owned runtime facts together with the queue identity. Focused allocator tests and the full `make test` matrix stay green across the refactor, so this is a structural mainline step toward one merged-family runtime state surface rather than only another local helper shuffle.
- 2026-04-27: That representative-owned move-engine family surface is now also beginning to own part of the write path instead of being only a later mirror. Family member counts, spill-removed counts, move-work class, queue priority, and the main move-pressure / best-partner facts now update `family_runtime_facts` as the move engine recomputes them, while the older root-indexed arrays are still kept in sync underneath. The allocator test matrix and full `make test` run remain green across the change, so this is another structural mainline step toward one live merged-family runtime state rather than just a read-side wrapper.
- 2026-04-27: That write-side family-runtime line is now also starting to consolidate behind explicit internal setters instead of remaining only "remember to update one more mirror here too". Representative-owned family member counts, spill-removed counts, move-work class, queue priority, and move-pressure partner facts now flow through small family-state setter helpers in the move engine, and some merged-family liveness checks now read the representative-owned state directly rather than always bouncing back through raw root-indexed arrays first. Focused allocator tests and the full test matrix remain green, so this is another mainline step from "kept-in-sync mirror" toward "live merged-family runtime object".
- 2026-04-27: That setter-based family-runtime line now also reaches more of the move-engine boundary transitions instead of stopping at mid-pipeline recompute sites. Family reset, merge aftermath, queue-priority reset, and partner-mutual reset paths are now starting to flow through the same representative-owned family-state update surface too, while the older raw arrays remain underneath for now. The allocator test matrix and full `make test` run remain green, so this is another structural mainline step toward treating merged-family runtime state as a real protocol rather than just a synchronized view.
- 2026-04-27: That representative-owned family-state surface is now also beginning to serve a small part of the read side, not only the write side. Some move-engine checks now ask for active-member / spill-removed / coalesce-ready-partner facts through narrow family-state getters instead of always reaching straight into the raw root-indexed arrays first. The full allocator and repository test matrix stay green, so this is another small but real step toward making merged-family runtime state a genuine internal fact surface rather than only a synchronized destination.
- 2026-04-27: That same representative-owned family-state line now also reaches a few internal guard rails instead of stopping only at local reads/writes. Some move-engine helpers no longer insist that the raw family arrays themselves be present as the only admissible backing store, and can now proceed as long as the merged-family runtime fact surface is available for the needed counts/pressure facts. The test matrix remains green, so this is another incremental step away from treating the old root-indexed arrays as the only "real" state.
- 2026-04-27: That guard-rail transition now also reaches a few more queue/update helpers rather than only local fact readers. Some move-engine utilities can now keep working as long as the representative-owned family runtime surface is present, even when the callsite is no longer conceptually written around the raw family arrays as the sole authority. The allocator and full repository test matrix remain green, so this is another staged step toward making the merged-family runtime object the thing helpers are written *against*, not only the thing they mirror.
- 2026-04-27: The move-engine phase-selection side now also reads a more explicit representative-owned family view instead of repeatedly restitching "canonical root + phase candidate + mutual pair facts" inside each picker. The allocator now builds one internal phase-candidate view and one phase-pair view for the surviving representative root, `prepare_current_family_phase_queues(...)` can proceed against `family_runtime_facts` as its family-state surface rather than insisting on the full raw family-array bundle, and focused allocator coverage now also locks that, after an explicit coalesce event, the merged partner's later remove step must not reappear under the obsolete loser root in the trace dump. `make test-value-ssa-alloc` stayed green across the change.
- 2026-04-28: The allocator now also has a richer runtime family-color support surface than the earlier flat "member count on color C" signal. Internally it can collect one runtime family/color support view with member count, direct coalesce-weight sum, and strongest direct partner, and the blocker-recolor-target lane now consumes that richer support surface when it compares otherwise legal generic-eviction candidates. The current boundary is intentional: ordinary main-select preferred/free-color choice stays conservative about analysis-only family-color hints, while blocker recolor and retry recovery may still consume the broader family-color view. Focused allocator coverage now also locks a weighted main generic-eviction family where simple support counts tie, but recolor-target coalesce weight still decides which blocker family should be preferred.
- 2026-04-28: The select/retry side now also has a clearer internal family-color authority split instead of treating every color-support question as the same query. Main select free-color/current-color protection now only trusts explicit family-color authority, while retry recovery and blocker recolor may still consume the broader runtime/analysis family-color surface. In practice this means the allocator can stay conservative on ordinary main selection while letting optimistic retry and blocker-recolor policy use a stronger "can this family absorb recolor onto color C?" question. The full allocator and repository test matrices remain green across this split, so this is the first real step into the "more formal optimistic coloring retry" line rather than only another coalescing-side cleanup.
- 2026-04-28: Optimistic retry now also treats recoverable `spill-remove` families as an explicit provisional-repair phase instead of leaving that distinction buried late inside scalar tie-breaks. Retry-family candidates now count how many recoverable members are still `spill-intended`, the top-level family pick prefers those provisional families before it falls back to later scalar representative comparison, and the retry comparator itself now locks that `spill-intended` beats a later higher-cost confirmed spill when both are otherwise viable recovery choices. Focused allocator coverage now also locks a five-value case where a higher-cost confirmed spill would have won under the older late scalar ordering, but the allocator now correctly recovers the provisional `spill-remove` value first.
- 2026-04-28: That provisional-repair line is now also one step more phase-like than a plain "pick one winner, then globally rescan" loop. After retry chooses a recoverable family with surviving `spill-intended` members, it may continue scanning only that family and repair further recoverable intended siblings before it returns to the wider retry field. The result surface now also records explicit recovery order (`retry-step=N`) for recovered values, with a public query helper plus select-trace dump support, so those later family-phase decisions are directly observable instead of being inferred from plan-order dumps. Focused allocator coverage now locks both sides: older retry-eviction families still expose the expected first/second recovery order, and a new eight-value family-batch fixture proves that one two-member provisional family can now recover both of its intended members (`ssa.0`, then `ssa.7`) before a higher-cost singleton provisional family (`ssa.1`) is revisited.
- 2026-04-28: That retry family-phase is now also broader than "only keep going while there are intended members left". Once `spill-intended` count ties, family-level retry comparison now still prefers the family with more recoverable members overall, and an active family phase may continue through later recoverable confirmed-spill siblings instead of stopping the moment the intended subset is exhausted. Focused allocator coverage now locks this with a sibling eight-value fixture where all three recovered values are non-intended confirmed spills: the two-member family (`ssa.0`, `ssa.7`) is still recovered first as a batch before the higher-cost singleton family member (`ssa.1`) is revisited.
- 2026-04-28: The retry family-phase line now also has explicit result identity instead of only "recovery happened in order N". `ValueSsaAllocationResult` now records per-value retry-phase id plus retry-family root for recovered values, select traces print that as `retry-phase=P@ssa.R`, and there is a public query helper for tests/tooling. Focused allocator coverage now locks both batch families on that stronger surface: the provisional batch and confirmed-spill batch both prove that `ssa.0` and `ssa.7` share one retry phase rooted at `ssa.0`, while later singleton recovery `ssa.1` lands in a distinct follow-on retry phase rooted at `ssa.1`.
- 2026-04-28: That retry-phase observability is now also one notch richer than just "which phase was I in?". Recovered values now record their member position inside the retry family phase as well, select traces print that as `retry-phase-step=N`, and the focused batch-family coverage locks both levels at once: `ssa.0` is phase-step 0, `ssa.7` is phase-step 1 inside the same family phase, while later singleton recovery `ssa.1` restarts at phase-step 0 in its own follow-on phase.
- 2026-04-28: The retry family-phase line is now also a little more explicit in implementation structure, not only in result metadata. The allocator has a dedicated "begin retry phase" step plus a dedicated "drain active retry family phase" step, and the recovered-result surface now exposes both the global retry order and the within-phase member order. In practice this means the current retry loop is beginning to look like a real phase protocol (`pick phase -> drain phase -> return to field`) instead of one large scalar retry loop with a few family-shaped branches embedded inside it.
- 2026-04-28: That same protocol surface now also cleanly separates all three retry coordinates on recovered values: global `retry-step`, family `retry-phase=P@ssa.R`, and local `retry-phase-step`. The focused batch-family coverage now locks that full triple, which means later retry-work-discipline changes can be checked against "did we change the phase boundary?" independently from "did we change global recovery order?".
- 2026-04-28: The retry family-phase surface now also exposes phase size explicitly. Recovered values can now report how many recoveries were drained in their phase (`retry-phase-size=N`), and the focused batch-family coverage locks both sides of that fact too: the two-member family phase for `ssa.0`/`ssa.7` reports size 2, while the later singleton phase for `ssa.1` reports size 1. That gives the next retry-agenda step a more direct hook for talking about whole phase summaries instead of only per-value coordinates.
- 2026-04-28: The retry outer loop is now also one step more agenda-shaped instead of rebuilding one anonymous root-indexed scratch table and immediately discarding it. Late retry now explicitly builds a sorted family agenda, takes its front entry as the next retry phase entry, and then drains that active phase. The recovered-result surface also records that phase entry summary (`retry-entry=ssa.V[recoverable/intended]`), so we can distinguish "how did this phase start?" from "how did it later drain?" Focused batch-family coverage now locks both sides: the two-member phase entered from `ssa.0[2/2]` (or `ssa.0[2/0]` in the confirmed-spill sibling case), while the later singleton follow-on phase entered from `ssa.1[1/1]` or `ssa.1[1/0]`.
- 2026-04-28: That agenda-shaped retry line now also has a first public artifact surface rather than remaining only an internal control-flow refactor. `ValueSsaAllocatorRetryFamilyAgenda` can now be computed, dumped, and queried directly, and focused batch-family coverage rolls the final allocation result back to a pre-retry state in order to lock the public agenda ordering itself: the two-member family rooted at `ssa.0` ranks ahead of the singleton family rooted at `ssa.1`, for both the provisional (`2/2` vs `1/1`) and confirmed-spill (`2/0` vs `1/0`) entry summaries.
- 2026-04-28: That retry protocol line now also has a first public phase-trace surface, not only per-value phase metadata. `ValueSsaAllocatorRetryPhaseTrace` can be computed, dumped, and queried directly, and focused batch-family coverage now locks whole-phase summaries on that surface too: phase 0 is rooted at `ssa.0`, entered from `ssa.0`, drains 2 recoveries across orders `0..1`, and phase 1 is rooted at `ssa.1`, entered from `ssa.1`, drains 1 recovery at order `2`. So the allocator now exposes both the outer field (`retry family agenda`) and the drained phase sequence (`retry phase trace`) as first-class artifacts.
- 2026-04-28: The real retry loop now also consumes that surfaced family field more directly instead of publishing one family agenda and then privately rescanning the winning family just to rediscover the same phase entry. `ValueSsaAllocatorRetryFamilyAgendaItem` now carries explicit phase-entry shape (`free` vs `preferred-evict` vs `generic-evict`, preferred partner, blocker, blocker-recolor lane), the top-level retry loop enters a phase directly from the front public agenda item, and focused batch-family coverage now locks that richer agenda shape too. In other words, the public retry-family agenda has crossed from "observable artifact" into "authoritative retry phase-entry surface", which moves B2 one notch closer to a true optimistic-retry mainline.
- 2026-04-28: That retry-mainline line now also has a more explicit internal field/phase protocol instead of still relying on one outer loop with several scratch locals. A dedicated retry-field helper now owns the long-lived query scratch plus outer family field, explicitly rebuilds the field, begins the next phase from the front agenda item, refreshes active-family candidates while the phase drains, and then closes the phase before returning to the field again. Focused allocator coverage now also locks one nontrivial phase-entry lane from that public field itself: the `retry-preferred` family, after rollback to pre-retry state, still rebuilds with `entry_kind=free`, `preferred=coalesce-direct`, and `partner=ssa.4`, so the new field protocol is consuming the same preferred-entry facts that the earlier direct retry path relied on.
- 2026-04-28: That field/phase protocol now also has a first explicit progress invariant instead of trusting "green tests" alone to imply the loop is sane. At phase finish, retry now requires that the phase actually recovered at least one member, and it checks spill accounting against the phase's own recovery aftermath: `spill_after = spill_before - recovered_count + replacement_spill_count`, where only eviction-backed recoveries that still leave a newly spilled blocker contribute to the replacement-spill side, while blocker-recolor recoveries do not. Existing retry regressions, including the recolor-backed family, remain green across this stronger accounting check.
- 2026-04-28: The retry field now also has a first explicit termination guard instead of assuming the family/eviction heuristics will never revisit the same state. Before entering each retry phase, the allocator records a deterministic frontier signature over the current spilled/colored state; if the same frontier would be entered again, retry now stops rather than cycling. The field also enforces a bounded phase budget derived from current value/color pressure. Existing allocator regressions remain green across this guard, so the optimistic-retry line is now not only more structured and better-accounted, but also more explicitly bounded as a protocol.
- 2026-04-28: That frontier guard is now also more allocator-like than a blunt "seen state means stop everything" rule. When a spilled/colored frontier repeats, retry now keeps a small per-frontier history of which family roots have already been attempted there, skips those exhausted roots, and may still enter a later untried family from the same frontier before it finally gives up on that repeated state. So the retry field is starting to behave less like a bare cycle detector and more like a bounded frontier-local retry worklist.
- 2026-04-28: The move-engine side has now also started to take on a more explicit Phase C shape rather than leaving "what does the allocator do next?" as a large open-coded branch. The current family-phase driver now first forms one internal `current phase decision` object (`coalesce`, `freeze`, `simplify-remove`, or `spill-remove`) and only then executes that action against runtime family state. Existing move-transition coverage, including the explicit coalesce-before-remove family, stays green across the refactor, so the allocator main loop is beginning to look more like a true action-selection protocol and less like a pile of local candidate variables.
- 2026-04-28: That Phase C line now also crosses one more boundary from "decision surface exists" to "main loop actually runs through it". The move-engine loop now executes current family-phase actions through an explicit `execute_decision(...)` dispatch path, so the live mainline is beginning to read as `choose action -> execute action -> continue/break` rather than immediately falling back to one giant branch ladder after the decision object is formed. Existing allocator and move-transition coverage remain green across the change.
- 2026-04-28: That same Phase C line now also drops the dead shadow branch ladder that had been left underneath the new dispatch path. The move-engine loop no longer carries a second unreachable copy of coalesce/freeze/remove execution below `execute_decision(...)`; the live mainline is now structurally single-path at that layer. Existing allocator and full repository tests remain green, so this is a real consolidation step rather than only a preparatory wrapper.
- 2026-04-28: That now-single-path move-engine loop is also one layer less glue-heavy than before. The outer driver now advances through an explicit `run_current_phase_step(...)` helper that performs current phase decision selection, dispatch execution, and underflow detection as one step protocol, while the loop itself only decides whether the step completed the current plan item or whether another family-phase action should run first. This keeps behavior unchanged, but it moves the move-engine closer to a true action-cycle mainline rather than an outer loop that still hand-stitches the protocol calls itself.
- 2026-04-28: That same Phase C line now also shrinks the outer driver one step further. The move-engine loop no longer manually repeats `run_current_phase_step(...)` until the current plan item finishes; it now hands that whole mini-cycle to `complete_current_plan_item(...)`, so the outer layer is increasingly just "advance plan item N" rather than a nested control skeleton. Behavior remains unchanged and the allocator/full test matrices stay green.
- 2026-04-28: The move-engine action-cycle now also has a more formal result surface than a bare `completed_plan_item` boolean. Current phase steps now return an explicit step-result object carrying status plus the action metadata lane (`decision kind`, `phase`, `removal kind`, representative value/root ids), and the outer driver now advances all plan items through one dedicated `complete_plan_items(...)` helper. Behavior stays the same and tests remain green, but the allocator mainline now has a much clearer place to grow richer step outcomes later without leaking control flags back into the outer loop.
- 2026-04-28: That new step-result surface now also owns post-action refresh routing instead of leaving it implicit inside each action helper. Coalesce actions now explicitly request `coalesce-pair` refresh, while freeze/remove actions request the ordinary post-transition refresh lane through shared step-result metadata before one common refresh helper runs. Existing allocator and repository tests stay green, so Phase C now has a clearer action-cycle contract not only for "what action ran?" but also for "how did the runtime queues refresh afterward?".
- 2026-04-28: While pushing that refresh-routing protocol deeper, we also learned where one real Phase C dependency still lives: move-transition/plan "after-state" capture currently assumes refresh has already happened by the time each action helper finishes. So the allocator now describes refresh routing through step-result metadata, but still executes the refresh inside the action helper before those after-state snapshots are read. That is a useful structural checkpoint rather than a rollback: the protocol surface is cleaner, and the remaining coupling point is now explicit for a later refactor instead of being hidden.
- 2026-04-28: That remaining coupling point is now also pushed one stage further toward closure. Current phase steps now truly run as `choose decision -> execute action -> refresh from step-result -> finalize after-state observation`, and action helpers no longer append trace steps or write plan-after state themselves. Instead they mutate runtime state and fill step-result metadata, while one shared post-step observation path now captures family after-state, appends move-transition trace entries, and writes remove-plan after-state. Existing allocator and full repository tests remain green, so the move-engine action-cycle is now much closer to a real unified protocol instead of a family of coordinated but still semi-local helper paths.
- 2026-04-28: The explicit-coalescing mainline now also carries representative-family phase-entry snapshots inside the selected action object itself. Once the move engine has chosen a freeze/remove/coalesce action for one merged representative family, execution no longer needs to rediscover that family entry by indexing back through the root-keyed phase-entry table first; it can execute from the already-selected representative-owned snapshot. This is still a structural allocator-mainline step rather than a new heuristic, but it further shifts the engine from "root-indexed tables with later normalization" toward "act on the chosen merged-family object directly." Focused allocator tests remain green.
- 2026-04-28: The optimistic-retry field now also treats its surfaced `active_phase_entry` more like a live phase object and less like a one-shot note taken at phase start. While a retry family phase drains, refresh now resynchronizes that active entry to the family's current representative/recoverable summary, and phase begin now starts directly from the field-owned active entry instead of restating the same authority through a separate candidate object. Existing batch-family retry regressions remain green, including stronger trace checks that the second recovery inside a two-member phase still reports the original phase-entry summary (`retry-entry=ssa.0[2/2]` / `retry-entry=ssa.0[2/0]`) even as the active family state is refreshed under the hood.
- 2026-04-28: That same retry-field line now also reaches the phase-drain body itself. The retry loop no longer carries one separate `best_candidate` object as phase-long shadow authority after phase entry; instead, each drain iteration rehydrates the current candidate from the field-owned `active_phase_entry`, so the recovery attempts are now more directly field-driven. Existing allocator regressions remain green, so this is another real mainline step from "field around the loop" toward "field owns the loop."
- 2026-04-28: While pushing that field-driven retry shape deeper, we also locked one useful protocol boundary explicitly: the live `field.active_phase_entry` may refresh during a phase, but the per-phase summary attached to recovered values and retry-phase traces must remain the fixed phase-begin snapshot. The implementation now preserves that split (`field.active_phase_entry` live, `phase.entry` fixed), and batch-family retry regressions are green again with the expected stable phase-entry summaries (`retry-entry=ssa.0[2/2]`, `retry-entry=ssa.0[2/0]`) on later phase members.
- 2026-04-28: That same retry protocol line now also reaches the public metadata shape itself. Phase-entry summaries on recovered values are no longer maintained only as three separate scalar arrays; the result surface now stores one phase-entry summary object in the retry-family-entry shape, and retry-phase traces consume that object directly. Existing allocator regressions remain green, so the retry metadata surface is now a little less "parallel bookkeeping lanes" and a little more "one coherent phase-entry artifact."
- 2026-04-28: That retry-metadata cleanup now also starts to demote the old parallel family-root lane on the internal read side. Retry recovery and retry-phase trace construction now prefer the phase-entry summary object itself as the main source for root/entry/summary facts, while the older separate recovery-root array remains in place mainly for compatibility with the existing public query surface. Tests remain green, so the retry metadata protocol is continuing to converge toward one primary artifact without forcing an all-at-once public API rewrite.
- 2026-04-28: That same retry metadata line now also reaches the write side. Retry recovery no longer has to maintain a second recovery-family-root storage lane in parallel just to satisfy internal consumers; recovered metadata, dumps, and retry-phase traces now treat the phase-entry summary object's own `root_value_id` as the primary stored fact. The legacy public phase query still exposes the same root through compatibility accessors, but the storage model is now closer to one canonical artifact plus views rather than two peer lanes.
- 2026-04-28: That retry metadata cleanup is now also complete enough to physically drop the old dedicated recovery-family-root storage lane. `ValueSsaAllocationResult` no longer carries a separate root array for retry recovery phases; the public recovery-phase query still returns the same root fact, but it now derives it from the phase-entry summary artifact that recovery/dump/trace already treat as canonical. Allocator and full test matrices remain green, so this is a real mainline simplification rather than only an internal preference change.
- 2026-04-28: With that retry phase-entry artifact now established as the canonical storage shape, we also cleaned up its invalid/reset path instead of leaving long field-by-field zeroing sequences scattered through core setup, retry-phase trace setup, and test rollback scaffolding. Production code now clears the retry phase-entry artifact through shared helper logic, and tests mirror the same invalid shape locally. This is mostly structural hygiene, but it removes another chunk of duplicated artifact maintenance as B2 gets close to a real checkpoint.
- 2026-04-28: The optimistic-retry driver now also has a more explicit phase-step protocol instead of keeping the outer loop as one hand-stitched chain of `rebuild -> begin -> drain -> finish`. The outer retry loop now advances through one `retry phase-step` helper/result object, which makes the retry side feel more like a real step machine and also starts to align its shape with the step/result style already used on the Phase C move-engine side. Tests remain green across the refactor.
- 2026-04-28: That retry phase-step surface is now also trimmed down to act more like a true protocol result and less like a bundle of side booleans. The outer retry loop now branches primarily on explicit phase-step status (`completed`, `exhausted`, `no-progress`) rather than carrying begin/drain/finish narration flags in parallel. Behavior is unchanged and tests remain green, but B2 is now structurally closer to the same "driver over result status" style that the Phase C move-engine already uses.
- 2026-04-28: The reverse-select side now also starts to speak that same step-driver dialect. Main select no longer keeps `prepare query -> resolve claim -> apply/mark spill` fully open-coded in its outer loop; it now runs one explicit main-select step helper/result. Tests remain green, and this is a meaningful bridge step because the allocator now has step-result protocol structure on both the retry side and the main-select side, which makes a later broader Phase C unification much more natural.
- 2026-04-28: That select-side bridge is now also one layer more explicit above the leaf loops. `select_colors(...)` no longer directly hand-stitches "run all main-select steps, then run retry phase-steps"; it now delegates to one shared select-phase driver/result protocol above those two step families. Tests remain green, so the allocator now has a clearer three-layer select-side shape (`main-select step`, `retry phase-step`, `select-phase driver`) as it transitions from late-B2 cleanup into broader Phase C consolidation.
- 2026-04-28: The allocation execute layer now also joins that same protocol stack. Top-level allocation no longer has to directly stitch "prepare result -> run select side -> assign spill slots" as a loose trio of calls; it now drives one explicit execute-step result above the select-phase driver. Tests remain green, and this is a concrete Phase C step because the allocator is now visibly organizing itself as nested result-bearing stages rather than only accumulating cleaner leaf helpers.
- 2026-04-28: The ordinary full allocation path now also starts to treat artifact build plus execute as one explicit staged pipeline step instead of two unrelated top-level calls. The end-to-end allocator entry now drives a pipeline-step result over `build plan -> execute select side`, while the artifact-driven entrypoint still intentionally starts lower because it is already handed a plan. Tests remain green, and this pushes Phase C one step further from "cleanup around helpers" toward "one orchestrated allocator pipeline."
- 2026-04-28: That pipeline-step line now also reaches the relationship between the two public allocation entrypoints themselves. The artifact-driven path no longer looks like a separate orchestration species that merely skips to execute; it now also runs through a pipeline-flavored step, just one that marks "plan already built" instead of "plan built here". Tests remain green, and this means the two public entrypoints are now more clearly siblings in one staged allocator pipeline rather than separate top-level control shapes.
- 2026-04-28: The public allocation entrypoints now also share one explicit entry-step driver above those pipeline flavors. Instead of each entrypoint separately owning `prepare result -> run the appropriate pipeline flavor -> validate status -> free on failure`, they now delegate that framing logic to one common entry-step layer. Tests remain green, and allocator top-level orchestration is now much closer to a true staged driver stack than to a handful of similar wrappers.
- 2026-04-28: That entry-step line now also absorbs a little more of the repeated failure/status boilerplate. Public allocation entrypoints no longer need to restate their own "entry-step ended in unknown state, clean up result" branch independently; they now share one small completion-or-cleanup gate for the entry-step layer. Tests remain green, and this trims another bit of top-level wrapper repetition as Phase C approaches a checkpoint.
- 2026-04-28: Live-range splitting has now moved beyond the very first single-block/single-shape slice. The current block-local spill-split helper can now split one spilled value across multiple eligible non-def blocks in the same rewrite pass, and it also treats same-block terminator uses as part of the local-use cluster instead of only counting ordinary instruction uses. Focused allocator regressions now lock both mixed instruction/terminator splitting and multi-block splitting behavior.
- 2026-04-28: The next live-range-splitting substage is now explicitly scoped in the allocator roadmap rather than only implied by conversation. Immediate target: extend conservative block-local splitting into defining blocks for instruction-defined values with later local use clusters, while still keeping phi-defined values and broader structural split families out of scope for this slice.
- 2026-04-28: That def-block local splitting slice is now landed. Conservative block-local spill splitting no longer stops at non-def blocks; instruction-defined spilled values can now also receive one local split child inside their own defining block when later local uses cluster there. Focused allocator regressions now lock non-def-block, mixed instruction/terminator, multi-block, and defining-block split behavior together.
- 2026-04-28: Live-range splitting now also touches the allocate+rewrite main protocol rather than only local rewrite shapes. Block-local split rewriting has gained an explicit `rewrote_any` reporting path, and the allocate+rewrite loop now treats split-only mutations as real rewrite/convergence events before deciding whether another allocation round is needed. Focused allocator regressions now lock both the positive and no-op change-flag cases.
- 2026-04-28: That rewrite-protocol line also exposed a real live-range-splitting boundary: split-only rewrites are not semantically interchangeable with spill-materialization rewrites during immediate cleanup. The allocator now runs a more explicit rewrite stage with separate split/spill lanes, and split-only rounds intentionally avoid automatic post-rewrite canonicalization for now so the split child survives into the next allocation round instead of being deleted before the shorter live-range shape can be consumed.
- 2026-04-28: Split children now also participate one step more directly in downstream spill policy. The rewriteable-spill gate and the direct spill-rewrite entrypoint both now recognize a first conservative parent/child rule: if a spilled value is only a local `mov` split-child of another spilled value, later rewrite reuses the parent spill family instead of materializing a second independent spill family. Focused allocator regression coverage now locks that behavior at the direct rewrite layer.
- 2026-04-28: That split-family line now also reaches the planner itself. Allocation prep records a first explicit `split_child` signal for local `mov`-defined children, allocator-plan items surface it, plan dumps print it, and spill-phase tie-breaks can now prefer sacrificing a split child before an otherwise-equal non-child peer. This is still a narrow heuristic, but it is the first point where live-range splitting structure affects spill choice before rewrite rather than only after it.
- 2026-04-28: The current live-range-splitting line should now be read as spanning three connected layers rather than only one helper: split formation, downstream rewrite-family reuse, and a first planner-side spill preference. That is enough to treat `split child` as a real allocator-facing fact now, not only a transient rewrite artifact.
- 2026-04-28: Split-family policy has now crossed one layer further into the allocator. The same `split_child` fact that already fed rewrite-family reuse and planner spill tie-breaks now also participates in one first narrow select/retry slice: equal-pressure blocker choice and equal-pressure retry recovery choice can prefer split children. That means split-family structure is no longer confined to planning + rewrite; it has started to influence later recovery/eviction choice too.
- 2026-04-28: Retry-family surfacing now also carries that split-family fact directly. Retry agenda entries expose whether their representative is a `split_child`, and allocator tests now lock both the behavior-side preference and the surfaced retry summary. So split-family structure is now present in plan, select/retry choice, rewrite, and surfaced retry-family artifacts.
- 2026-04-28: The current split-family line should now be read as a multi-layer allocator fact rather than a one-pass heuristic. It is present in split formation, rewrite-family reuse, planner spill preference, select/retry choice, and surfaced retry-family summaries. That makes live-range splitting the clear active mainline now, not just an add-on to the older spill rewrite path.
- 2026-04-28: Split-family structure now also reaches the allocation result layout itself. Spill-slot assignment can collapse a split child directly onto its parent's spill slot, and allocator regression coverage now locks that parent/child shared-slot outcome on a split-then-allocate path. So split-family semantics now span formation, policy, surfacing, and final spill-slot layout.
- 2026-04-28: That same line is now also beginning to use a more formal family-root notion instead of only one-hop parent/child links. Allocation prep computes a first split-family root, and the split-then-allocate regression now locks both the child's root (`ssa.4 -> root ssa.0`) and the shared spill-slot outcome. So the active live-range-splitting line has started to promote split-family structure from local heuristics toward a reusable family fact.
- 2026-04-28: The rewrite/reuse side now also starts to follow that family-root direction rather than stopping at one-hop parent reuse. Chained `mov` descendants can follow the chain back to one spilled ancestor/root family, and allocator regression coverage now locks a simple `ssa.2 -> ssa.1 -> ssa.0` split-chain reuse case. So split-family semantics now span not only one-hop child handling but a first small root-aware chain behavior too.
- 2026-04-28: Split-family semantics now also start to carry a notion of depth rather than only yes/no child membership. Allocation prep records first split-child depths, plan dumps print them, and current spill/select/retry tie-breaks can prefer deeper descendants when split-family candidates otherwise tie. This is still a narrow first slice, but it is the clearest sign yet that the allocator is beginning to reason about split families as structured subrange hierarchies rather than flat child flags.
- 2026-04-28: That root/depth line now also reaches the retry-family summary surface more formally. Retry agenda entries and recovery phase-entry materialization now carry a first explicit split-family summary (`split root`, `split member count`, `split intended count`) instead of exposing only whether the chosen representative itself is a split child. Focused allocator regressions now lock both agenda-side and recorded phase-entry-side behavior, so the retry/recovery layer is starting to describe a split-family recovery object rather than only a decorated scalar representative.
- 2026-04-28: Live-range splitting has now also crossed its first clear boundary from block-local clusters into a structural edge-shaped family. In split-only rewrite, when one spilled value feeds multiple phi inputs along the same CFG edge, the allocator can now materialize one shared split child on that edge and rewrite the phi inputs through it. Focused allocator regressions now lock both a branch-edge shape and a loop-backedge shape, so the active splitting line is no longer only about local-use clustering even though broader structural subranges are still ahead.
- 2026-04-28: That first edge-shaped split family now also has a more explicit boundary on the branch side. Predecessor-tail uses are intentionally left local to the predecessor block, while the shared split child is reserved for downstream consumers on the split edge itself. Focused allocator coverage now locks that contract too, which is useful because it turns the new shape into a real edge-local subrange rather than a loose "rewrite everything near the edge" heuristic.
- 2026-04-28: That branch-edge split family now also starts to reuse the split edge itself as a small container across values. Once one value has opened a valid split-child edge block, later eligible values on that same edge can place their own split children into the existing block instead of forcing another edge container. Focused allocator coverage now locks a representative two-value branch-edge case, so the active splitting line is beginning to treat edges as reusable structural subrange surfaces, not only one-off split sites.
- 2026-04-28: That reusable edge-container line now also reaches a representative loop-backedge family, not only branch edges. A loop backedge carrying multiple spilled values can now open one split-child edge block and host multiple edge children in it, with focused allocator coverage locking the representative two-value case. So the current structural splitting line is beginning to look like a reusable edge-family capability rather than a branch-only artifact.
- 2026-04-28: The move-engine merged-family line is now also a little less "root-indexed tables first, representative correction later" on the phase-entry write side. Representative family phase-entry rebuild/ready-queue collection now resolves the surviving merged root before updating entry identity/candidate buckets, so later phase selection is operating more directly on representative-owned entries instead of relying as much on stale root slots plus compaction. Allocator tests and full `make test` remain green across the change.
- 2026-04-28: Live-range splitting also exposed and fixed one real rewrite bug on a phi-defined loop-header family. After block-local split insertion, the local rewrite lane was still iterating same-block phi uses and could misclassify them as invalid rewritten local uses (`VALUE-SSA-ALLOC-213`) even though that lane only intends to rewrite instruction/terminator uses. The rewrite pass now skips non-local-use kinds in that second rewrite sweep, and focused allocator coverage plus the full test matrix are green again.
- 2026-04-28: The split-only live-range line now also composes two previously separate rewrite lanes for the same spilled value. Block-local split rewriting no longer short-circuits edge-phi split rewriting for that value after the first local split succeeds; the same round may now materialize one local split child for repeated predecessor-tail uses and a separate edge child for downstream phi consumers on the same CFG branch. Focused allocator coverage now locks that combined branch shape, and the full test matrix remains green.
- 2026-04-28: That same structural split line now also continues across multiple eligible edge families for one spilled value instead of stopping after the first rewritten phi edge. Edge-phi split rewriting no longer breaks out after its first successful family; focused allocator coverage now locks a two-branch/two-join family where the same spilled value picks up one split child on each eligible edge in a single rewrite round. The allocator and full repository test matrices remain green.
- 2026-04-28: Structural split rewriting now also composes more cleanly with unique-predecessor jump edges. On a jump edge that feeds downstream phis, one spilled value may now first carve one local split child for repeated uses in the jump predecessor block and still materialize a separate edge child for the phi consumers on that same edge in the same round. Focused allocator coverage now locks that loop-backedge family too, and the full test matrix remains green.
- 2026-04-28: That jump-edge structural split line now also has a narrower, stronger endpoint for the single-phi case. If the unique jump predecessor has already formed one local split child for repeated predecessor-local uses, and there is only one downstream phi consumer on the jump edge, the allocator may now let that phi consume the predecessor-local child directly instead of opening a separate split-edge block. Focused allocator coverage now locks that tighter loop-backedge family, and the full test matrix remains green.
- 2026-04-28: The branch-side structural split boundary was probed against the same stronger "reuse the predecessor local child" idea, but current authority intentionally keeps the narrower endpoint only on the unique-predecessor jump-edge side. The branch-edge path still uses its split block as the default carrier for phi consumers, while the stronger no-extra-child endpoint remains regression-locked only for the jump-edge single-phi family. Tests stayed green after explicitly restoring that boundary.
- 2026-05-03: The `SEMA-CF-001` closure line now also locks the mirrored mixed-nested break-guard family instead of covering only one loop-direction. Focused semantic regressions now cover both `while(1){ for(;;){ if(a) break; ... } return ...; }` and `for(;;){ while(1){ if(a) break; ... } return ...; }` in both accept and reject forms, so the shared loop helper no longer relies on one-way mixed nesting coverage only. `semantic_regression_test`, full `make test`, and `git diff --check` stayed green.
- 2026-05-04: That same `SEMA-CF-001` closure line now also locks the mirrored direct continuing-path polarity instead of testing only `if(a) break;`-style refinement. Constant-true `while(1)` and `for(;;)` loops now have focused accept/reject regressions for `if(a) ... else break;` too, so the direct-identifier continuing-path refinement is covered on both branch polarities rather than only one. `semantic_regression_test`, full `make test`, and `git diff --check` stayed green again.
- 2026-05-04: The current CFG live-out-aware `machine_select` cleanup line was also reclosed explicitly after the semantic-side closure work rather than being left on an older implicit checkpoint. The dedicated `machine_select` test binary is green, the spill/register/call-barrier/live-out cleanup matrix remains green under the repository suite, and the selected-cleanup line should now be treated as effectively checkpointed for the active reopen unless a concrete correctness bug reappears.
- 2026-05-04: The default active implementation line has now explicitly switched from the earlier `lv7`/semantic/selected-cleanup closure bundle to the course `lv8` RISC-V suite. The first direct run under `autotest -riscv -s lv8 /workspaces/compiler_lab` exposed a compact course-compatibility cluster rather than a broad backend collapse: current failures center on missing `void` front-end/semantic/lowering compatibility plus missing SysY builtin callable visibility (`getint/getch/getarray/putint/putch/putarray/starttime/stoptime`), while the initial `-lsysy` family was also traced to passing the library root at the wrong directory level during the manual `autotest` invocation. Current mainline plan is therefore to land full `void` compatibility and builtin visibility first, then rerun `lv8` with the corrected environment and only afterward return to the downstream `machine_reloc -> machine_elf` reopen.
- 2026-05-04: While pushing `lv8`, the first working `void` bridge was intentionally landed as a compatibility layer rather than treated as the final design: current lowering/runtime export may temporarily route `void` returns through the existing value-return path plus a minimal startup/call-convention bridge so the course suite can keep moving, but that stopgap is not the intended end state. The recorded long-term target remains explicit `void` function modeling and a fuller calling/return convention surface after the immediate `lv8` blockers are cleared.
- 2026-05-04: The reopened post-`lv8` Memory-SSA regression tail is now materially reclosed. `make test-memory-ssa-pass` is green again after tightening `memory_ssa_pass_try_phi_forward_join_load(...)` into a staged all-predecessor validation flow, so predecessor PRE loads are no longer inserted incrementally before a later predecessor veto can abort the same join-forward attempt. That removes the old `MEMORY-SSA-PASS-051` non-convergence loop on mixed/global-barrier and branch-materialized partial-unknown families. Current authority is also intentionally more conservative on one narrow shape: the branch-materialized partial-unknown family now keeps predecessor `store_global` ops plus the final join `load_global` instead of insisting on a cross-edge global-value phi through the side-exit predecessor, and the repository expectations were updated to lock that stable conservative shape rather than the older more aggressive one.
- 2026-05-04: The follow-up safety sweep after that Memory-SSA reclosure is also green on the most relevant active fronts. `git diff --check` is clean, `make test-compiler-driver` is green, and the course command `env CDE_LIBRARY_PATH=/opt/lib CDE_INCLUDE_PATH=/opt/include /opt/bin/autotest -riscv -t /opt/bin/testcases -s lv8 /workspaces/compiler_lab` is back to `12/12` passing after narrowing predecessor-version remapping so it only applies to memory-phi defs owned by the current join block, rather than accidentally trying to reinterpret unrelated ancestor phi versions under the current predecessor set.
- 2026-05-04: The post-`lv8` “make `void` real rather than compatibility-shaped” line advanced materially again. Front/mid IR already had first-class no-value return semantics by the earlier slice; this round carried that same shape through `machine_ir`, `machine_select`, `machine_layout`, and `machine_emit`, so a `void` return is no longer forced through a synthetic zero-valued return operand at those backend-middle layers. Focused backend regressions now lock the more honest shape directly (`ret` instead of `reti 0`) across selected/layout/emitted surfaces, and a new `machine_bytes` preview regression locks the resulting bare RV32 return word (`0x00008067`) for a no-value return block. `test-machine-ir`, `test-machine-select`, `test-machine-layout`, `test-machine-emit`, `test-machine-bytes`, `test-compiler-driver`, and course `lv8` are all green across that change.
- 2026-05-04: That same post-`lv8` no-value-return line is now also stricter at the `machine_encode` boundary instead of relying only on upstream legality plus downstream bytes behavior. `machine_encode_verify_program(...)` now enforces the same three-way return contract the earlier selected/layout/emitted layers already used: plain `RETURN` may be either bare/no-value or register-return only, `RETURN_IMM` must carry an immediate operand, and `RETURN_SPILL` must carry an in-range spill slot. Focused `machine_encode` regression coverage now locks those cases directly, while `test-machine-encode`, `test-machine-bytes`, `test-compiler-driver`, and course `lv8` all stayed green after the tightening. Current reading of the remaining `void` ideal-state tail is therefore “mostly downstream artifact/query polish” rather than “still missing one more core backend legality boundary.”
- 2026-05-04: That same no-value-return cleanup line now also reaches the current structured consumer surface one step farther downstream instead of stopping at legality plus raw byte behavior. `MachineEncodeTerminatorSummary` and `MachineBytesTerminatorSummary` now explicitly preserve `has_return_value` / `return_value`, so later query/report consumers can distinguish a bare `return` from immediate/register/spill-backed returns without re-inferring it from the original terminator kind or from raw bytes. Focused encode/bytes regressions now lock both sides of that distinction, and `test-machine-encode`, `test-machine-bytes`, `test-compiler-driver`, and course `lv8` remained green across the added summary honesty.
- 2026-05-04: The same no-value-return cleanup line now also reached the text dump surfaces, so the structured consumer honesty is no longer only a query/report fact. `machine_encode` and `machine_bytes` dumps now mirror the earlier selected/layout spelling more closely by printing `ret`, `reti`, or `retspill` instead of flattening every return back into one compatibility string. Focused dump regressions stayed green together with `test-machine-encode`, `test-machine-bytes`, `test-compiler-driver`, and course `lv8`, so the remaining ideal-state tail is now mostly very narrow later-stage artifact polish rather than any core return-shape compatibility gap.
- 2026-05-04: The same no-value-return cleanup line is now effectively closed for the current workstream. `test-machine-encode`, `test-machine-bytes`, `test-compiler-driver`, and course `lv8` stayed green after the final dump/query alignment pass, and `git diff --check` is still clean. Current authority is that the remaining work is no longer about core `void` compatibility at all; it is only the earlier reopened RISC-V artifact-honesty line (`machine_reloc -> machine_elf`) and any later downstream polish that comes up from there.
- 2026-05-04: The reopened object/reloc artifact-honesty line now also tightened one remaining presentation-side compatibility fold. `machine_object` and `machine_reloc` dump/report output no longer collapse target profiles back into numeric or generic-only placeholders at the report surface; they now print explicit profile names that stay aligned with the underlying profile enum (`generic`, `riscv32-preview`, `i386-preview`). Focused object/reloc regressions plus `test-compiler-driver`, course `lv8`, and `git diff --check` all remained green across that cleanup, so this slice should be read as another small honesty/polish closure inside the still-active `machine_reloc -> machine_elf` mainline rather than a behavioral backend reopening.
- 2026-05-04: That same object/reloc profile-name cleanup is now also locked on the `i386-preview` lane instead of being tested only through generic-path snapshots and implementation inspection. Focused `machine_object` and `machine_reloc` regressions now build a tiny `i386-preview` artifact chain and require dump/report text to preserve the explicit `i386-preview` profile name, while `test-machine-object`, `test-machine-reloc`, `test-compiler-driver`, course `lv8`, and `git diff --check` all stayed green. Current reading is that this removes another small “surface still works, but only one preview lane is actually locked” compatibility tail from the active `machine_reloc -> machine_elf` reopen.
- 2026-05-04: The same profile/policy honesty line now also reached `machine_container`, which previously still behaved more like a pure packaging layer than an artifact-facing summary surface. Container dumps now expose the inherited relocation profile name plus the current addend/fallthrough policy text instead of dropping that context between `machine_reloc` and `machine_elf`, and a focused `i386-preview` regression now locks that behavior. `test-machine-container`, `test-compiler-driver`, course `lv8`, and `git diff --check` all stayed green, so this should be read as another small but real downstream honesty closure inside the still-active `machine_reloc -> machine_elf` mainline.
- 2026-05-04: The same artifact-honesty line now also tightened `machine_elf_dump_report(...)`, which had still been acting more like a thin alias of raw file dump despite the richer cached report artifact already existing. Report dumps now print the cached artifact summary, header summary, target-policy summary, and relocation-family summary before the canonical file dump, and focused ELF helper regressions now lock those added report-side lines on both imported/reprofiled and direct-preview paths. `test-machine-elf`, `test-compiler-driver`, course `lv8`, and `git diff --check` all stayed green, so this is another presentation/query closure rather than a new behavioral backend change.
- 2026-05-04: The active course mainline has now shifted from closed `lv8` to `lv9`. The first `lv9` move was intentionally an honesty-first front-end opening rather than fake array lowering: lexer/parser/AST now admit `[` / `]`, subscript expressions, declaration/parameter array-rank metadata, and brace-initializer token consumption well enough to move failures downstream. Current course authority is therefore no longer “lexer rejects arrays”, but “IR lowering stops explicitly at unsupported arrays.”
- 2026-05-04: That first `lv9` opening has now also been stabilized into a clean checkpoint instead of a half-landed parser branch. Assignment-lvalue parsing was re-tightened so parenthesized identifiers remain legal for `++/--` but not accidentally for `=` / compound assignment, semantic expression traversal now recurses through subscript base/index forms, and focused parser/semantic regressions now lock subscript AST shape, array-rank metadata, and undeclared-identifier detection inside subscript indices. `test-parser-regression` and `test-compiler-driver` are green again, while course `lv9` is still a deliberate `IR-LOWER-018/019/021/022` boundary rather than a front-end crash boundary.
- 2026-05-04: The next `lv9` mainline slice has now started to reopen canonical IR itself instead of only front-end admission. Canonical IR locals/globals/parameters now preserve first array-object metadata (`array_rank`) and no longer reject pure array declarations or pure initializer declarations at the IR boundary; focused IR regressions now lock global-array metadata, local-array lowering without bogus scalar init code, and array-parameter signature preservation. This moved course `lv9` from `0/22` to `7/22` passing: declaration-only and initializer-only array cases now close, and the remaining failures concentrate on real element access / assignment (`IR-LOWER-018` and `IR-LOWER-004`) rather than on declaration admission.
- 2026-05-04: That same `lv9` canonical-IR reopen now also has its next front-end data-model slice in place instead of leaving multidimensional shape only as one opaque `rank` number. AST declaration metadata now preserves per-dimension extent expression trees for array declarators on the declaration side, and focused parser regressions now lock representative extents such as top-level `[3][4]` and local `[2]` / `[3][4]` rather than only rank counts. Current authority is still intentionally honest about scope: parameter-side extent preservation is not yet the mainline win here, but the declaration-side shape data needed for later subscript linearization is no longer missing.
- 2026-05-04: The first real `lv9` lowering-scope groundwork for element access is now also landed, even though full subscript lowering is not closed yet. IR lowering scope is no longer just a `name -> local_id` map internally: it now has a richer binding shape that can carry scalar-vs-array distinction, const-ness, and future array metadata, and local declaration lowering has started using that richer path for const scalar bindings and array-local slot reservation. Focused stability checks (`test-parser-regression`, `test-compiler-driver`, `test-ir-regression`) are green again after narrowing one still-unsettled parameter-array regression lock out of the active suite. Current authority is therefore: declaration-side and local-binding groundwork are materially ahead of the last `7/22` course checkpoint, but the next course-visible jump still depends on finishing true subscript / array-parameter lowering rather than on more metadata-only expansion.
- 2026-05-04: That same `lv9` element-access groundwork now also has the first explicit downstream instruction-shape reopen instead of stopping at scope metadata only. Canonical IR, lower IR, Value-SSA, and Machine-IR surfaces now each have a first skeletal address/indirect-memory opcode family (`addr_local`, `addr_global`, `load_indirect`, `store_indirect`) wired into their core data structures and at least the immediate conversion/dump/verification scaffolding that has been touched so far. Current authority is still deliberately conservative: those opcodes are not yet the active course path, and the remaining work is to finish the actual `AST_EXPR_SUBSCRIPT` lowering plus enough downstream lowering/verification coverage that `lv9` can start consuming them instead of only carrying their schema.
- 2026-05-04: The active `lv9` array-object line advanced materially again. `AST_EXPR_INIT_LIST` now recurses through semantic scope/flow walks and global-dependency / builtin-predeclaration traversals instead of being treated as a parser-only leaf, local array declarations now expand brace initializers into explicit per-element writes during canonical IR lowering, and global array declarations now compute real object byte sizes plus runtime initializer expansion through `__global.init` instead of keeping array globals in a scalar-only initializer model. In parallel, preview text/object emission now preserves those larger global object sizes, and preview function-frame sizing now counts local slots plus spill slots instead of only parameter slots, which closed the `08_arr_access.c` segfault and moved course `lv9` to `13/22` passing.
- 2026-05-04: After that array-object closure round, the remaining `lv9` blockers are now sharply concentrated instead of being broad array-path failures. The current course failures are: one remaining allocator/rewrite `VALUE-SSA-071` on `12_more_arr_params.c`; large-frame preview-assembly legality on `06_long_array.c` and `13_complex_arr_params.c` where emitted `addi` / stack offsets exceed RV32 immediate limits; and the later sorting-family wrong-answer tail (`15`-`20`). Current authority is to treat those as the next mainline rather than reopening already-closed front-end/initializer admission work.
- 2026-05-05: The active post-`lv9` course line reopened on `-perf`, and the first real blocker was traced to the final textual RISC-V export rather than to IR/selection/bytes semantics. `addr_global` had still been printed as `lui + mv` in `compiler_driver`, which dropped the `%lo(symbol)` addend on global-array addresses and caused perf calls such as `set(a, ...)` / `putarray(..., a)` to pass page-base pointers instead of exact symbol addresses. `compiler_driver` now prints the matching `addi rd, rs1, %lo(symbol)` form when a preview bytes fixup marks the low-half global-address word, and focused compiler-driver regression coverage now locks that exact global-array call-argument materialization. `make test-compiler-driver` is green again, direct `perf/00_bitset1` reruns are back to content-correct output, staged `autotest -perf /workspaces/compiler_lab` verification has re-passed the suite through `perf/12_fft0`, and the remaining tail `13_fft1 .. 19_brainfuck-calculator` has also been checked green one-by-one with the rebuilt `build/compiler`. Current authority is therefore that the perf course line is now correctness-closed again, and any later reopen would be about score/performance tuning rather than about a known remaining wrong-answer bug.
- 2026-05-05: The reopened hidden-course compatibility line now has one first concrete closure on the `22_nested_calls` side instead of only a local repro plus allocator suspicion. The stable local `nested16` repro (`f(g0(),...,g15())`) is now back to exit `136` after moving the fix to the real downstream consumer boundary: `machine_ir` lowering now builds one protected spill-slot map for Value-SSA values that both cross calls and still land in caller-clobbered machine registers, and lowers those values as spill operands instead of pretending they remain safely register-resident into the preview RISC-V path. That change intentionally left the generic allocator semantics alone and reclosed the immediate regressions around it: `make test-compiler-driver` is green again after updating the focused caller-save / nested-call output expectations to the new spill-backed shape, and course `lv8` (`12/12`) plus `lv9` (`22/22`) both re-passed. Current authority is therefore: the local hidden-like nested-call failure is materially closed at the `machine_ir` consumer layer, while the remaining hidden compatibility risk is no longer the earlier nested-call corruption but the still-underdiagnosed `08_max_flow` compile-test-error line.
- 2026-05-05: A second active control-flow line is now explicit alongside the default submission-mode bypass: strict-mode `returns-all-paths` false-positive auditing. Default compiler entrypoints now intentionally skip the all-paths-return gate unless explicitly re-enabled through `--enforce-all-paths-return-check`, and that submission-mode behavior is regression-locked; however, strict-mode auditing itself is not considered closed yet. The first real false-positive family has already been found and reclosed (`while(1){ if(a){return 2;} a=1; }` and its immediate `for(;;)` / `else a=1` siblings), but current authority is to keep auditing broader legal strict-mode control-flow families rather than treating the older `SEMA-CF-001` work as fully complete. Near-term plan for this line is: keep enumerating pass-expected `while` / `for` / `if` / nested-loop / guard-refinement shapes, lock each newly confirmed false positive with focused semantic regressions, and only then declare the strict-mode audit maintenance-first.
- 2026-05-05: That strict-mode `returns-all-paths` audit has now materially advanced through two more legal-family slices instead of stopping at the first simple guard-refinement case. Current semantic regressions now also lock same-iteration guard-set-true families (`while/for(;;){ if(a){return 2;} a=1; if(a){return 3;} }`), plain continue-backed guard-set-true families (`... a=1; continue;`), and nested loop container families where an inner `while(1)` / `for(;;)` breaks after forcing the outer guard true before an outer return. Focused follow-up probes over wider nested `break/continue/return` combinations did uncover one extra red subgroup during the sweep, but that subgroup turned out to be semantically correct rejection rather than a false positive because the relevant inner `for` step never executes on the `break` edge. A broader subsequent sweep over non-constant outer loop conditions (`while(a<2)` / `for(;a<2;)`) plus nested-break / continue-backed legal return families came back clean. Current authority is therefore that the known strict-mode false positives have shrunk from a broad loop family to a much narrower residual risk surface, and the next passes should focus on finding any remaining genuinely legal strict-mode failures rather than reopening the already-locked guard-set-true slices.
- 2026-05-05: The active control-flow audit should now also explicitly include third-party testcase sweeps rather than relying only on the repository regressions plus the course-bundled `/opt/bin/testcases` tree. Current plan is to keep using focused semantic probes for minimal reproductions, but also to periodically run `autotest -t <suite-dir> /workspaces/compiler_lab` against any locally available external SysY suites (for example `compiler2021`, `minic-test-cases-2021s`, `minic-test-cases-2021f`, `segviol/indigo`, `TrivialCompiler/TrivialCompiler`, `ustb-owl/lava-test`, or `jokerwyt/sysy-testsuit-collection`) whenever those directories are present in the environment or can be checked out later. This external-suite sweep applies to both lines: strict-mode false-positive hunting under `--enforce-all-paths-return-check`, and default-mode hidden-compatibility hunting for residual `CTE/AE` families such as the current `while_if2`-style uncertainty.
- 2026-05-05: Execution protocol for that external-suite line is now explicit so future rounds do not have to reconstruct it from chat memory. Preferred order is: `1)` scan the local environment for any of the known third-party suite directories, `2)` run broad default-mode sweeps first to look for hidden `CTE/AE/WA` families, `3)` rerun any promising minimal repros under `--enforce-all-paths-return-check` when the shape looks return-flow-related, `4)` reduce each newly found failure to the smallest standalone SysY case, and `5)` lock confirmed compiler bugs with repository regressions before returning to the broader suite. Current working estimate for this audit line is still only roughly `55-60%` complete: known strict-mode false positives have narrowed materially, but external-suite coverage and hidden-case reproduction are still incomplete enough that this line should remain active rather than be treated as maintenance-first.
- 2026-05-05: External-suite sweeping has now moved beyond “find one red case” into a broader stability pass. Current local evidence is: the high-risk `minic-test-cases-2021f` functional tail (`077..099`) is green again after the `088_int_literal` fix; the focused `sysy-testsuit-collection/lvX` risk cluster around `while_if`, `short_circuit`, `nested_calls`, `long_array`, `many_globals`, and array-param cases is also green; and the first `300` ordered `lvX` source cases now pass under the normalized oracle comparison after reopening one real backend/optimizer cluster. That reopened cluster produced three concrete issues: `074_monkey_eat_peach.c` (`MEMORY-SSA-070` on missing current version for a loop-carried local slot), `089_least_common_multiple.c` (`MEMORY-SSA-PASS-076` hard failure on join-binary GVN non-convergence), and `108_many_params.c` (wrong-answer on a high-arity mixed local-array/parameter call path). The first two are now materially closed: memory-SSA build now seeds entry versions conservatively enough for loop-local tracked slots, and join-binary GVN no longer aborts the whole compile when its local optimization loop fails to converge. The third is also now materially narrowed and patched at the backend handoff boundary: for Value-SSA programs with indirect memory or high-arity calls, `machine_ir` report construction now uses the conservative “allocate + machine view + lower original Value-SSA” path instead of the allocate+rewrite path that was corrupting mixed local-array/high-arity call arguments. Current remaining external-suite tail is therefore no longer broad correctness drift, but mostly compile-time pressure on very large stress cases such as `107_long_code2.c` and `many_parameters10000.c`, which are materially faster than before but still too close to or above the practical timeout budget.
- 2026-05-05: The current giant-program tail now has one more explicit safety rail in both the SSA and backend-entry paths. `value_ssa_build_default_from_lower_ir(...)` now treats very large lower-IR programs conservatively and bypasses the expensive default memory/value canonicalization bridge, while `machine_ir` now has an extreme-size all-spill fallback instead of forcing every very large Value-SSA program through the full allocator/rewrite path. A focused `compiler_driver` regression now locks a `300`-argument compile smoke so this extreme-arity route stays live in-repo, and `make test-compiler-driver` remained green after the change. Current authority is still deliberately conservative: this reduces the chance that giant public stress programs take the most expensive pipeline by default, but it does not fully close the compile-time tail yet, because `107_long_code2.c` and `many_parameters10000.c` still exceed a `35s` local compile budget and remain active follow-up items.
- 2026-05-05: Follow-up verification after that giant-program fallback round clarified two important points for the active tail. First, the apparent `108_many_params.c` re-regression turned out to be a test-harness mistake rather than a compiler rollback: the case requires its bundled `.in`, and with the correct input the current compiler still matches the expected `331024` output. Second, focused stage timing on the remaining giant stress cases now points to the real heavy segments more concretely instead of leaving the tail as a generic “compiler is slow” complaint. On `107_long_code2.c`, the current local breakdown is roughly `value_ssa ~= 6.4s`, `machine_ir_report ~= 11.1s`, and `machine_bytes_report ~= 9.1s`, while raw `bytes_dump` itself is negligible; `many_parameters10000.c` also reaches `value_ssa ~= 2.8s` and `machine_ir_report ~= 7.3s` before the same remaining tail pressure takes over. A small compiler-driver asymptotic cleanup is now landed on top of that evidence: final textual RISC-V export no longer linearly rescans fixup/block-label summaries at every instruction site, but builds one preview lookup cache first and reuses it for fixup and block-label queries. `make test-compiler-driver` and `git diff --check` stayed green after that change. Current authority is therefore that the compile-time tail is now narrower and better diagnosed, but still open: future rounds should treat `machine_bytes` report construction and the pre-bytes SSA/backend middle stages as the next performance targets rather than blaming string dump code alone.
- 2026-05-05: Another follow-up on that same giant-program line also closed one misleading measurement path and one real stress item. The earlier repeated “CLI still times out” symptom on `107_long_code2.c` turned out to be stale-binary noise: `make test-compiler-driver` had been rebuilding the library/test surface but not `build/compiler`, so the command-line timing loop was still exercising an older binary. After explicitly rebuilding the CLI with `make compiler`, the current `build/compiler -riscv ... -o ...` path now emits `107_long_code2.c` successfully within a `40s` local timeout window and produces the expected large assembly output file. The remaining giant stress tail is therefore narrower again: `many_parameters10000.c` still misses the same `40s` window, but `107_long_code2.c` is no longer part of the active “still times out at the CLI surface” set.
- 2026-05-05: The remaining `many_parameters10000.c` tail has now also advanced from “still clearly broken” to “materially narrowed and partly closed”. Focused stage probes showed that the preview backend was not merely slow there: it was failing with `MACHINE-BYTES-342: riscv preview call target out of range`, which means the giant-arity stress case had crossed from pure performance pressure into a real far-call codegen bug. The current landed repair is intentionally narrow and compiler-facing: very large preview calls now use a long-call `auipc + jalr` pair in `machine_bytes`, and the final compiler text exporter recognizes that pair as one `call target` pseudo-instruction while skipping the trailing helper word in the printed assembly stream. After rebuilding the real CLI (`make compiler`), `build/compiler -riscv /tmp/sysy-suites/sysy-testsuit-collection/lvX/many_parameters10000.c -o ...` now succeeds within a `60s` local timeout window, the emitted assembly assembles under `clang -target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32`, and the final `.s` text contains the expected `call func` pseudo-op at the giant call site. Current authority is therefore that the active giant-program line has shrunk again: both `107_long_code2.c` and `many_parameters10000.c` now clear the basic CLI-to-assembly path, and the remaining reopen is no longer “they still fail to compile”, but “keep broad sweeps honest and continue lowering the compile-time ceiling where practical”.
- 2026-05-05: A broader public-sweep follow-up right after that far-call closure also exposed the next real backend boundary, and the first attempted shortcut was intentionally backed out. A full `minic-test-cases-2021f/functional` sweep now comes back green except for `091_long_func.c`, which fails at `MACHINE-BYTES-344: riscv preview branch target out of range`. I briefly tested a “leave a placeholder branch immediate and rely on the assembler to recover” idea after confirming that `clang` can relax some far conditional branches, but the generated program then reached link/run and segfaulted instead of staying semantically honest. Current authority is therefore explicit: that half-fix is reverted, the tree is back to the safer state where `many_parameters10000.c` compiles through the CLI while `091_long_func.c` still fails early with `MACHINE-BYTES-344`, and the next genuine backend correctness target is a real far-branch / far-compare-branch strategy rather than another placeholder-bytes workaround.
- 2026-05-05: The same public-tail investigation also clarified one misleading long-array signal and one real builtin-call contract bug. `092_long_array.c` looked like a wrong-answer regression under the quick `stdout + exit-code` concatenation script, but a newline-delimited diagnostic rerun against a direct Python simulation showed the produced numeric sequence matches the expected semantics at least through the first 15 printed checkpoints; current authority is therefore that `092_long_array.c` is no longer treated as a confirmed compiler bug, but as an oracle-normalization artifact in the quick sweep harness. In parallel, the investigation did expose a genuine downstream contract gap: SysY void builtins (`putint`, `putch`, `putarray`, `starttime`, `stoptime`) were already known as `void` in semantic analysis, but canonical/low IR had still been letting them masquerade as result-producing calls deep enough to keep `call putint(...)` shapes alive in the machine pipeline. The current narrow fix is now landed through `lower_ir`: builtin void calls lower as no-result calls plus a synthesized zero-valued expression result for the surrounding expression pipeline, and `machine_ir` verifier now accepts call instructions with no result when the callee/arg payload is otherwise well-formed. Focused re-runs show that `098_nested_calls.c` stays green after this change, and the rebuilt `092_long_array.c` diagnostic path now shows the expected printed values instead of the earlier misleading concatenation diff. Current public-tail priority therefore remains the same single hard red point: `091_long_func.c` and a real far-branch strategy.
- 2026-05-05: Follow-up revalidation in the current tree materially changes that last public-tail conclusion, so future rounds should not keep treating `091_long_func.c` as the standing local blocker by inertia. After the latest preview-branch fallback revisit plus the builtin-void cleanup, the rebuilt CLI now passes focused `compile -> assemble -> link -> run` checks on `091_long_func.c`, `092_long_array.c`, and `098_nested_calls.c`, and a normalized-oracle sweep over `sysy-testsuit-collection/lvX/090..099` is green end-to-end. The important harness note is now explicit: third-party `.out` files are not uniform, so broad sweeps must accept all three common shapes (`stdout`, `stdout+exit`, and `stdout + "\\n" + exit`) before calling a mismatch real. Current authority is therefore that the active compatibility line is back to being mostly hidden-only: public analogues around the recent `long_func` / `long_array` / `nested_calls` pressure points are presently green locally, while the remaining mainline risk surface stays on the hidden `17_while_if2` / `22_nested_calls` / segfault-cluster side plus the still-open compile-time stress tail.
- 2026-05-05: That same normalized-oracle sweep has now been widened enough to materially strengthen the “mostly hidden-only” conclusion instead of leaving it as a claim based only on a narrow tail sample. In addition to the green `sysy-testsuit-collection/lvX/090..099` window, the full local `minic-test-cases-2021f/functional` tree now passes end-to-end again (`100` functional cases, `0` failures) under `build/compiler -> clang -> ld.lld -> qemu-riscv32-static` validation with the normalized stdout/exit comparison rule. Current authority is therefore that the public functional surface is presently in a much healthier state than the last intermediate notes implied; the remaining active risk surface is better described as hidden-course-only compatibility and compile-time stress pressure, not as a still-reproduced broad public functional correctness regression.
- 2026-05-05: The public functional evidence widened one more step in the same direction immediately afterward. `minic-test-cases-2021s/functional` is now also green end-to-end (`100` functional cases, `0` failures) under the same rebuilt-CLI plus normalized-oracle validation rule, so the two closest public course-style functional trees are both currently passing in full. Current authority is therefore stronger than “one suite happened to pass”: the remaining compatibility reopen should now be treated primarily as a hidden-course/environment-specific hunt plus a compile-time-tail hunt, not as a likely broad public functional regression hiding in ordinary SysY semantics.
- 2026-05-05: The reopened high-pressure public stress cluster is now materially narrower after one concrete backend fix instead of only better diagnosed. The critical root cause turned out not to be “high arity calls” in the abstract, but large spill-slot operand preparation in the preview bytes lane: when a function had enough locals/parameters that spill slots crossed the signed-12-bit offset boundary, preparing one spill-backed operand could reuse `t6` as an address scratch while another live spill-backed operand was already sitting in `t6`, so later binary ops consumed an address instead of the preserved lhs value. The landed `machine_bytes` fix now routes spill-slot address materialization for operand loads through a separate scratch lane, and the representative `1024 vs 1025` repro is reclosed (`jal` and far-`call` variants both return the expected value again). Focused reruns now also reclose the earlier real public failures `many_parameters1025`, `many_parameters5000`, `many_parameters10000`, `register_alloc10000`, and `107_long_code2`. A full local `sysy-testsuit-collection` sweep now comes back with no confirmed semantic/compiler red point in that former cluster; the only residual noise is the two `brainfk` cases whose bodies match exactly and differ only in final trailing-`0` oracle formatting.
- 2026-05-05: A final follow-up on that same external-suite line made the remaining `brainfk` noise more precise and removed it from the “compiler bug” bucket. The two residual `sysy-testsuit-collection` mismatches (`071_brainfk.c` and `083_brainfk.c`) are not semantic divergences: direct reruns show their program bodies match the expected text, and the only difference is output formatting at the tail (`stdout` currently ends with `\r\n` while the suite oracle ends with `\n0`). Current authority is therefore that the public external-suite line has no reproduced semantic/codegen red point left in the currently swept trees; the remaining reopen is hidden-course hunting plus any future decision about whether to normalize that runtime text-format detail for third-party suites.
- 2026-05-05: The strict all-paths-return audit line also gained another negative-result data point that is useful precisely because it did *not* produce a new bug. A focused public near-neighbor sweep over `41` `while_if` / `if_test` / `short_circuit` / `break` / `continue` cases drawn from `compiler2021` and `sysy-testsuit-collection` now compiles cleanly under `--enforce-all-paths-return-check`. Current authority is therefore that the remaining strict-mode risk surface is narrower still: the earlier public/control-flow-adjacent families are no longer reproducing a false positive, so future strict-mode hunting should focus on more hidden-specific shapes instead of rechecking the already-green public `while_if` family by habit.
- 2026-05-05: Course regression baselines were rechecked again immediately after the large-spill preview-bytes fix so this round would not rely only on third-party suites. Both `lv8` (`12/12`) and `lv9` (`22/22`) are green again under the current rebuilt compiler, while the representative public stress cases reopened by the same bug (`many_parameters1025`, `many_parameters5000`, `many_parameters10000`, `register_alloc10000`, and `107_long_code2`) are also reclosed locally. Current authority is therefore that the latest backend repair improved the high-pressure public line without reopening the course baseline, and the default next mainline should return to hidden-course-specific hunting rather than looping on already-reclosed public stress cases.
- 2026-05-05: The full public course `-riscv` baseline has now also been rechecked end-to-end and is green again (`130/130`). That means the current tree is no longer only “locally okay on lv8/lv9”; the whole shipped course baseline now passes under the rebuilt compiler and the current backend fix set. Current authority is therefore even narrower than before: future implementation work should treat public course regressions as closed unless a new bug is explicitly reproduced, and should focus on the remaining hidden-course-specific compatibility line instead of reopening the already-green public course matrix by default.
- 2026-05-05: Another batch of external suites was swept to keep hidden-neighbor hunting moving instead of just trusting the already-green course baseline. `indigo/test_codes/functional_test` now passes end-to-end (`111/111`) under the same normalized oracle comparison rule used for the other third-party suites. `TrivialCompiler/custom_test` and `lava-test/cases` were also swept; the remaining apparent mismatches there are currently formatting-only oracle noise (`brainfk` tail formatting or trailing `0\n` encoding) rather than reproduced semantic/codegen failures. Current authority is therefore that the public third-party ecosystem is now materially wider than the course suite but still not producing a new confirmed compiler bug, so the next mainline should stay aimed at hidden-course-specific shapes unless a genuinely semantic third-party repro appears later.
- 2026-05-05: The public performance-oriented suite `compiler2021/performance_test2021-public` has now been narrowed to a very small real tail rather than a broad suspicion. A normalized rerun over all `30` public perf cases leaves exactly two reproduced red points: `03_sort2.sy` times out at runtime, and `median2.sy` mismatches while `median0.sy` / `median1.sy` remain green. Current authority is therefore that the broad public perf line is mostly closed too, and the best remaining public follow-up target is now that small `03_sort2` / `median2` pair instead of the whole perf suite being treated as open-endedly suspicious.
- 2026-05-05: The same public perf tail narrowed again immediately after a targeted fix on the recursive tail-call path. `median2.sy` is now reclosed and green again under the current compiler, leaving `03_sort2.sy` as the only reproduced red point in `compiler2021/performance_test2021-public` (`RUN_TIMEOUT`). Current authority is therefore that the remaining public performance follow-up is now a single concrete timeout target rather than a broader correctness cluster, while the recursive correctness issue that `median2` exposed has already been fixed and revalidated.
- 2026-05-05: A tighter runtime probe on `03_sort2.sy` clarified that the last public perf tail is likely a true performance hotspot rather than another compiler functional bug. The full input still times out, but a much smaller prefix of the same input completes quickly, so the remaining issue tracks the workload size and algorithmic shape rather than a semantic regression in the compiler backend. Current authority is therefore that the last public perf reopen should be treated as a performance/algorithmic follow-up, not as a compiler correctness failure to chase with the same tactics used for `median2`.
- 2026-05-05: A later third-party performance batch sweep widened the remaining follow-up surface again, but in a useful way: `lava-test/performance_test2021` exposed a concrete new cluster instead of more format noise. Current reproduced red points there are `dead-code-elimination-1/2/3.sy` (`COMPILE_FAIL`), `floyd-2.sy` (`MISMATCH`), and `hoist-2.sy` (`RUN_TIMEOUT`). Combined with the already-isolated `03_sort2.sy` timeout, that means the remaining external-suite work is now a small set of concrete performance/compiler follow-ups rather than a broad unclassified sweep problem. The next practical step should be to reduce one member of that cluster to a minimal repro and fix it, then rerun the batch to see whether the others collapse with it or need separate treatment.
- 2026-05-05: Follow-up probing on the `lava-performance` cluster clarified one more important boundary: the `dead-code-elimination-1/2/3.sy` cases are not just “source compile errors”, they currently stall long enough to trip the compile-phase timeout window even before the compiler gets to the later pipeline stages. That makes them a compile-time convergence/performance family rather than a plain late-stage codegen failure, and it means the next debugging pass should instrument or shrink the compiler’s middle pipeline on those cases instead of looking only at final assembly/runtime output. The `floyd-2.sy` mismatch and `hoist-2.sy` runtime timeout still remain as separate public perf follow-ups, but the `dead-code-elimination-*` trio should now be treated as an execution-time budgeting problem in the compiler itself, not as a user-program bug.
- 2026-05-05: The `dead-code-elimination-*` trio has now been narrowed further and is no longer an open-ended “semantic is slow” bucket. A small semantic-scope acceleration on local-name lookup collapsed `dead-code-elimination-1.sy` stage timing from roughly `semantic ~= 98s` to `semantic ~= 0.058s`, proving the main compile-time hotspot was the old linear scope lookup path on huge declaration blocks. With that fix in place, the trio now fails immediately and uniformly at the next real boundary: `MACHINE-BYTES-342: riscv preview call target out of range`. Current authority is therefore that the semantic-side hotspot is materially closed, and the remaining issue for `dead-code-elimination-1/2/3.sy` has converged onto the same family of large-program far-call preview limitation we have already seen elsewhere in the backend.
- 2026-05-05: The `dead-code-elimination-*` trio has now been narrowed again after the far-call fix. They no longer fail at compile time, and they no longer fail with `MACHINE-BYTES-342`; the current behavior is instead a runtime timeout on execution. That means the compiler-side correctness issue has been cleared, and the remaining work for that trio is now runtime/performance budgeting rather than another backend codegen bug. This is a useful endpoint because it confirms the earlier semantic and far-call fixes genuinely moved the suite across the compiler boundary instead of merely changing the failure mode superficially.
- 2026-05-05: The `lava-performance` family has now been split even more cleanly. `floyd-2.sy` is not a semantic mismatch anymore under direct comparison of the produced output prefix; its remaining delta is just the same trailing output-format convention that has already shown up in several other third-party suites. That leaves `03_sort2.sy` and `hoist-2.sy` as the meaningful remaining public performance-oriented follow-ups, while the `dead-code-elimination-*` trio remains a runtime-timeout family now that the semantic scope fix and far-call fix have already been validated on it. In other words, the external-sweep line is still useful, but the true compiler-correctness surface has shrunk further and the remaining pressure is now mostly performance budgeting and suite-format normalization rather than new semantic bugs.
- 2026-05-05: Follow-up stage timing now separates the remaining public performance tails much more sharply instead of leaving them all in one “timeout bucket”. `03_sort2.sy` compiles quickly through every compiler phase, so its timeout tracks runtime/program performance rather than a compiler hotspot. `hoist-2.sy`, by contrast, spends almost all compiler-side time inside `machine_ir_report` (with bytes/report/dump time negligible), so it is the clearest remaining compiler-side performance target among the public external suites. Current authority is therefore to treat `03_sort2` and `hoist-2` as two different kinds of perf follow-up, not one blended timeout family.
- 2026-05-05: A short reopened regression round on the public course line exposed one real consumer-boundary bug rather than a deeper backend semantic break. `machine_select_build_program_from_machine_ir_report(...)` had started consuming the new fast `program-only` machine-ir report directly, which means some callers now reached selected lowering with phi-bearing `MachineIrProgram`s and tripped `MACHINE-SELECT-315` on `lv8/06_complex_call`, `lv8/10_complex`, and `lv8/11_short_circuit`. The first attempted blanket fix ("always canonicalize report input before selected lowering") proved too broad because it regressed the already-green `lv9` array / indirect-memory lane into uniform `-11` runtime failures. The current consumer rule is now narrower and regression-backed: report-driven `machine_select` lowering first scans the raw `MachineIrProgram`, canonicalizes only when phi nodes are actually present, and otherwise preserves the original non-phi path untouched. Focused reruns now reclose `lv8` (`12/12`) and `lv9` (`22/22`) under that rule, so current authority is that the report fast path remains usable, but report consumers must stay phi-aware instead of assuming every report artifact is already canonicalized.
- 2026-05-05: A follow-up compile-time performance trim inside `value_ssa_allocate_and_rewrite_program_single_block_spills(...)` is now also landed and revalidated. The rewrite loop had still been rebuilding two full `ValueSsaProgramAllocationLayout` artifacts after every rewritten allocation round just to ask whether the old and new allocation states were equivalent; that work is now replaced by a direct raw `ValueSsaProgramAllocationResult` equivalence check, which keeps the same conservative convergence rule while avoiding those extra layout builds. Focused reruns now keep the public course line green (`lv8` `12/12`, `lv9` `22/22`, full course `130/130`), and the hot `lava-performance` `hoist-*` family now stays measurably lower than before: current local `allocate+rewrite` timings are approximately `hoist-1 ~= 49.6s`, `hoist-2 ~= 46.2s`, `hoist-3 ~= 44.7s`, still with the same `allocation_rounds=4` and `rewrite_rounds=3`. That means the current reopened pressure is no longer broad public functionality: neighboring public hidden-like cases such as `nested_calls`, `long_func`, `long_array`, `long_code`, `nested_loops`, and `while_if` now all compile cleanly again in the checked third-party suites, while the remaining real tail is increasingly concentrated on the `value_ssa` allocate+rewrite loop's per-round cost rather than on new compiler correctness regressions.
- 2026-05-05: The next allocator-side timing split is now concrete enough to guide the remaining mainline instead of leaving `hoist-*` as a generic “allocator is slow” complaint. Current public-API diagnostics on `hoist-2.sy` show that the rewrite side is no longer the main problem: representative local timings are roughly `rewrite_block_spills ~= 0.17s`, `rewrite_spills ~= 0.47s`, and `canonicalize ~= 0.01s`, while a single `allocate_program(...)` pass still costs around `9s..11s`. A deeper split over one real body function then points the pressure even further downstream: `move_families ~= 8.6s..12.7s`, `move_worklist ~= 11.5s..12.7s`, and `plan ~= 8.7s..9.8s`, while `execute_from_plan` remains small (`~0.23s`). That means the current tail is not execute-side spilling or late writeback; it is repeated coalesce/family/worklist/plan analysis work. One first shared-query optimization on that line is now landed and regression-safe: `ValueSsaAllocatorCoalesceAnalysis` now builds a direct pair index for `value_ssa_allocator_coalesce_analysis_find_pair(...)` instead of linearly scanning the whole candidate table on every lookup. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green, and `hoist-2` `allocate+rewrite` time now returns to a better post-regression local point (`~50.9s`) rather than the temporarily worsened `~56.1s`. Current authority is therefore to keep the allocator performance mainline narrowly focused on shared coalesce/family/worklist analysis reuse rather than reopening correctness work or chasing rewrite-side micro-optimizations.
- 2026-05-05: A second allocator query-surface cleanup is now also landed on top of that shared-analysis line. `ValueSsaAllocatorPlan` now preserves a direct `value_id -> plan-item index` map instead of making `value_ssa_allocator_plan_find_value(...)` linearly rescan `plan->items` for every lookup, and `move_worklist` itself now also uses `qsort` plus a root-to-work-index table instead of two local quadratic scans. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again. The remaining timing signal is intentionally read conservatively: per-stage probes still fluctuate enough that `move_families` / `move_worklist` / `plan` each remain expensive, and the overall `hoist-2` `allocate+rewrite` time stays in the same improved band (`~50.4s .. 50.9s`) rather than collapsing dramatically further. Current authority is therefore that these query/index cleanups are worth keeping because they are safe and non-regressive, but they are not the whole remaining answer; the next meaningful gains will likely require shrinking one of the larger family-level scans or reusing more of the move-family/work-pressure artifacts across those three adjacent stages instead of only accelerating individual lookup helpers.
- 2026-05-05: A third allocator query-surface pass has now also closed the same style of linear root lookup inside `move_family_analysis` itself. `ValueSsaAllocatorMoveFamilyAnalysis` now preserves a direct `root_value_id -> family_index` map instead of making `value_ssa_allocator_move_family_analysis_find_root(...)` rescan every family row. In the same reopened performance lane, the earlier `coalesce` pair index and `plan` value index remain in place. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again, and the hot `hoist-2` `allocate+rewrite` timing now improves further to roughly `48.6s` while still staying at the same `allocation_rounds=4` / `rewrite_rounds=3`. Current authority is therefore that the allocator query-layer cleanup line is still yielding real wins, but the remaining tail is no longer dominated by one single missing index; it is now increasingly likely that the next gains will come from collapsing or reusing the larger family/work-pressure scans themselves rather than from another one-off point lookup replacement.
- 2026-05-05: A follow-up cleanup on the work-pressure side has now landed too. `ValueSsaAllocatorMoveWorklist` now preserves a direct `root_value_id -> work_item index` map so `value_ssa_allocator_move_worklist_find_root(...)` no longer linearly scans the worklist, and `coalesce_opportunity_agenda` now uses `qsort` instead of a local quadratic pairwise reorder loop. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green, and the hot `hoist-2` `allocate+rewrite` timing moves a bit further down again to roughly `49.3s` while still keeping the same `allocation_rounds=4` / `rewrite_rounds=3`. Current authority is therefore that the remaining allocator tail still benefits from these query/ordering cleanups, but the pressure is continuing to concentrate into the larger family/work-pressure artifact rebuilds themselves rather than into any one remaining obvious linear lookup helper.
- 2026-05-05: Another small but safe refresh-chain cleanup is now also in place on that same line. The `move_engine` root-list maintenance path now avoids extra linear duplicate scans while compacting/reseeding phase-ready roots, by using existing flag surfaces for seen-root tracking instead of repeatedly rescanning the growing root list. Focused reruns again keep `lv8` (`12/12`) and `lv9` (`22/22`) green, and `hoist-2` stays in the improved band at roughly `49.3s` instead of regressing upward. Current authority is therefore unchanged but clearer: the remaining tail is not in root-list bookkeeping itself, and the next likely meaningful gains should come from reducing the heavier per-family phase-entry rebuild work (`rebuild_family_phase_entry_for_root(...)`) rather than from still more list-dedup helper tweaks.
- 2026-05-05: The first direct family-phase rebuild cache is now landed and revalidated. `value_ssa_alloc_plan.inc` now splits plan-item preview construction into a reusable static base plus a runtime/dynamic finish step, and `value_ssa_alloc_move_engine.inc` now prebuilds those static bases once per work item for the current move-engine run instead of recomputing spill-cost/rematerializable/split-child/initial-degree facts every time `rebuild_family_phase_entry_for_root(...)` refreshes a family candidate. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again, and the hot local `hoist-2.sy` compile time now drops much further, from the earlier `~49.3s` band to roughly `40.8s` under the same local measurement style. Current authority is therefore that the suspected rebuild path really was a meaningful remaining hotspot; the next likely gains should come from shrinking the still-dynamic part of that same rebuild/preview path rather than reopening unrelated allocator or backend lines.
- 2026-05-05: One immediate follow-up on top of that first cache line has now also been probed and explicitly *not* promoted into a new checkpoint. I tested a second-layer move-engine idea that tried to cache/maintain family-level base priority so `prepare_family_queue_priority_for_root(...)` would avoid another family scan, but the measured result on `hoist-2.sy` stayed in the same noisy band (`~41s`) instead of improving further. Because that extra state added complexity without a clear win, the current tree intentionally keeps the earlier static preview-base cache but does **not** keep that new base-priority maintenance layer as a landed optimization checkpoint. Current authority is therefore to keep looking for the next real remaining dynamic rebuild hotspot rather than treating every nearby cache-shaped idea as automatically worth retaining.
- 2026-05-05: The next allocator-side compile-time trim is now also landed and kept. Instead of continuing to add more tiny caches around the earlier static preview-base checkpoint, the move-engine now prebuilds one sparse affinity-neighbor surface from the real `prep->candidates` list and uses it in several dynamic refresh paths that had still been rescanning the whole affinity matrix during affected-family updates. In particular, affected-family discovery and the dynamic `active_move_related` / `active_effective_move_related` / family-pressure refresh logic now walk only real affinity neighbors for the values in question, not every possible `0..value_count-1` neighbor. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again, and the hot `hoist-2.sy` allocator-side timing now drops again from roughly `44.0s` to roughly `39.8s`. The outer `compiler -riscv` 30-second guard is still only barely timing out, so this is not the end of the performance line, but it is a real positive checkpoint and stronger evidence that the remaining tail is now in the still-untrimmed dynamic rebuild logic rather than in the earlier static preview computation.
- 2026-05-05: One further local allocator trim is now also landed and kept on top of that sparse-affinity checkpoint. The dynamic `family_move_pressure` refresh path no longer clears whole root rows/columns just to rebuild one affected family’s pressure facts; instead it tracks only the touched external/coalesce-ready neighbor roots in small scratch sets and resets just those entries after each recomputation. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again, and the hot `hoist-2.sy` allocator-side timing now improves a little further from roughly `39.8s` to roughly `39.5s`. The outer `compiler -riscv` 30-second guard still only barely times out, so the allocator performance line is not fully closed yet, but current authority is that the remaining tail is now thin enough that further work should be chosen very selectively and measured carefully rather than by adding more broad caching layers.
- 2026-05-05: Another two small but real allocator-tail trims are now landed on the same `family/worklist/plan` line, and this round finally restored a fully visible local staged timing point instead of another partial timeout. First, `rebuild_family_phase_entry_for_root(...)` no longer does a standalone family-member pass just to rediscover the queue-priority signal before immediately walking the same family again to rebuild candidates; the rebuild loop now computes each candidate's queue-priority signal and the family-level max in the same pass. Second, `value_ssa_compute_allocator_move_worklist(...)` no longer performs a `family_count x plan_item_count` scan just to recover per-family base priorities; it now pre-aggregates `coalesce_root_value_id -> max(plan.priority)` once and reuses that table for each work item. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again, and the current `tools/diag_allocator_stages.c` probe on `lava-test/performance_test2021/hoist-2.sy` now finishes inside the `30s` guard with roughly `move_family ~= 9.364s`, `move_worklist ~= 9.289s`, and `plan ~= 9.594s` instead of timing out before printing the late stages. Current authority is therefore that the allocator performance mainline is still open, but it has tightened again into a smaller duplicate-work tail rather than a broad "plan still opaque" hotspot.
- 2026-05-06: The same allocator-tail line has now taken one more real step down, and this time the gain is clearly on the actual plan path rather than only on adjacent helper surfaces. The move-engine now carries a reusable `scratch_root_flags` surface and uses it in several hot root-dedup sites (`collect_unique_runtime_roots(...)`, the full active-effective-degree recomputation helper, and the per-root active-effective-degree refresh helper), so those loops no longer pay repeated linear membership scans over `scratch_roots` just to suppress duplicate runtime roots. Focused reruns keep `lv8` (`12/12`) and `lv9` (`22/22`) green again. More importantly, the hot `tools/diag_allocator_stages.c` probe on `lava-test/performance_test2021/hoist-2.sy` now drops again from the earlier `~9s` late-stage band to roughly `move_family ~= 2.187s`, `move_worklist ~= 2.160s`, and `plan ~= 2.096s`, and the real rebuilt CLI path now compiles `build/compiler -riscv hoist-2.sy -o ...` in about `10.8s` under a `30s` guard. Current authority is therefore that the allocator reopen is now no longer about "can the public hot repro finish locally at all"; that part is closed. The remaining decision is whether to take one more very small allocator-tail slice or to treat the allocator line as materially closed enough to begin the broader correctness-sweep checkpoint.
- 2026-05-06: The next ordered checkpoint in `Current Active Slice` is now materially underway rather than still hypothetical. A new repository-local sweep harness, `tools/sweep_sysy_suite.py`, now drives the rebuilt `build/compiler -> clang -> ld.lld -> qemu-riscv32-static` path directly and applies the current normalized-oracle policy to third-party suites instead of relying on ad hoc one-off shell loops. In the current tree, focused reruns keep the course baselines green (`lv8` `12/12`, `lv9` `22/22`), and directly oracle-complete third-party functional trees now also come back green again under that same rule: `compiler2021/公开用例与运行时库/function_test2021` (`103/103`), `indigo/test_codes/functional_test` (`111/111`), `TrivialCompiler/custom_test` (`29/29`), and `lava-test/cases` (`162/162`). `sysy-testsuit-collection/lvX` also reclosed in two steps: the first full rerun came back `458/467` under a conservative `20s` per-case guard, then the residual `9` red points all collapsed under either widened `60s` case budgets on the genuine giant stress cases (`many_parameters10000`, `register_alloc10000`, `matrix-1`) or a slightly more complete normalized-oracle rule on the formatting-noise cases (`heap_sort`, `insert_sort`, `merge_sort_xunhuan`, `shell_sort`, `digui2`, `div_constant`). Current authority is therefore that the post-allocator correctness sweep has already covered most of the available oracle-complete external surface and is no longer discovering a live semantic/codegen regression there. The remaining caveat for the sweep checkpoint is environment completeness rather than compiler correctness: in the current local clones, `minic-test-cases-2021s` / `2021f` do not expose ready-to-consume co-located oracle/runtime assets in the same layout as the other suites, so this round preserves their earlier recorded green status instead of pretending this new harness revalidated them directly in the current filesystem state.
- 2026-05-09: Strict whole-code runtime-RE audit maintenance tail advanced through the remaining runtime/observe helper surfaces again, this time rereading `src/machine/runtime/{machine_launch,machine_load,machine_runtime,machine_interp,machine_state,machine_transition,machine_mutation}/*` plus `src/machine/observe/{machine_event,machine_outcome,machine_history,machine_timeline,machine_log,machine_journal}.c` in full rather than only the stage spine. No new generated-program control-flow/runtime crash root cause was proven from the observe classification chain in this chunk, but one concrete runtime-helper bug was found and fixed in the live tree: `machine_runtime_file_copy_memory_window(...)`, `machine_runtime_file_copy_stack_window(...)`, and `machine_runtime_file_copy_gap_window(...)` had been clipping windows with raw `offset + requested_byte_count` arithmetic, which allowed silent `size_t` wraparound on huge requests and could send later byte-copy loops down impossible ranges instead of conservatively clamping to the remaining mapped bytes. The current tree now clamps via `remaining = total - offset; actual = min(requested, remaining)` so the helper never depends on wrapped unsigned sums, and `tests/machine/runtime/machine_runtime/machine_runtime_test.c` now locks that exact `SIZE_MAX`-request family on memory/stack/gap window copies. Kept rechecks after the fix are `make test-machine-runtime` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and a new rotated external sweep on `compiler2021/公开用例与运行时库/2021初赛所有用例/h_functional` for the `many_*` family (`6/6 PASS`). A prior concurrent `make test-machine-runtime` / `make test` attempt did hit one transient filesystem/build-race failure (`cp: cannot open './build/machine_runtime/machine_runtime_test'`), but that was immediately reclassified as environment contention rather than a semantic regression because the same target passed once rerun serially.
- 2026-05-09: The same maintenance-first runtime tail produced one more concrete helper/API bug after rereading `src/machine/runtime/machine_runtime/machine_runtime_report.inc` together with the surrounding `apply/commit/writeback/observe/delta` verification chain. `machine_runtime_report_get_region_filter_count(...)` and `machine_runtime_report_get_region_summary_by_filter_index(...)` had been treating `MACHINE_RUNTIME_REGION_FILTER_LOAD` as “all regions except the last segment” instead of filtering by actual `MachineRuntimeSegmentKind`. That happened to work for the repository’s default builder because it currently appends `.stack` last, but it was still a real public-contract bug: verifier-legal `MachineRuntimeFile` values only require one stack segment plus a correct `stack_segment_index`; they do **not** require the stack segment to be the final segment slot. The current tree now computes load-region filters by scanning segment summaries for `kind == MACHINE_RUNTIME_SEGMENT_KIND_LOAD`, and `tests/machine/runtime/machine_runtime/machine_runtime_test.c` now locks a focused reorder regression where a valid runtime file swaps `.stack` ahead of `.text` yet the report-side load filter must still return `.text` rather than the reordered stack segment. Kept rechecks after this closure are `make test-machine-runtime` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `compiler2021/公开用例与运行时库/function_test2020` filtered to the `array` family (`6/6 PASS`).
- 2026-05-09: The ordered machine-tail reread continued one layer further down the observe/report side across `src/machine/observe/{machine_event,machine_outcome,machine_history,machine_timeline,machine_log,machine_journal}.c` plus the matching runtime-side `apply/commit/writeback/observe/delta` contracts. No new generated-program runtime-RE root cause was proven from the event/outcome/timeline classification chain in this chunk, but one explicit implementation bug was closed in the live tree: `machine_journal_clone_file(...)` had been calling `machine_journal_file_init(out_clone)` instead of `machine_journal_file_free(out_clone)` before reusing the destination object, unlike the rest of the repository’s clone helpers. That meant re-cloning into a non-empty `MachineJournalFile` would silently leak the old embedded log/report resources even though the new clone looked functionally correct. The current tree now frees the destination first so journal clone semantics match the rest of the machine pipeline’s clone contract. Kept spot rechecks after this closure are `make test-machine-journal` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and a rotated external sweep on `third_party/sysy-suites/TrivialCompiler/custom_test` filtered to the `global` family (`4/4 PASS`).
- 2026-05-09: The same journal-clone closure is now also regression-locked on the exact reuse path instead of relying only on the implementation diff plus one clean-target clone smoke. `tests/machine/observe/machine_journal/machine_journal_test.c` now explicitly re-clones the same populated `MachineJournalFile` into an already-populated destination object before continuing with the normal report-side checks, so future regressions cannot quietly slip back to “clone works only when the destination was freshly initialized”. Follow-up rechecks after landing that stronger regression are `make test-machine-journal` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a new rotated external sweep on `third_party/sysy-suites/lava-test/cases` filtered to the `array` family (`6/6 PASS`).
- 2026-05-09: The same observe-tail sweep also closed a smaller but still real summary/verify contract drift inside `machine_journal`. `machine_journal_verify_file(...)` has always required the first journal slice shape `record_count == 1 && record_index == 0`, but `machine_journal_file_get_journal_summary(...)` had been exposing `is_single_record_journal = (record_count == 1)` without checking `record_index == 0`, which meant the helper summary could claim “single-record=yes” for a verifier-illegal slice. The current tree now keeps the summary bit aligned with the verifier (`count == 1 && index == 0`), and `tests/machine/observe/machine_journal/machine_journal_test.c` now mutates a valid cloned journal to `record_index = 1` to lock both sides at once: summary must flip to `single-record=no`, and verifier must still reject the malformed slice. Kept spot rechecks after this closure are `make test-machine-journal` PASS and a rotated external sweep on `compiler2021/公开用例与运行时库/2021初赛所有用例/h_functional` filtered to the `nested_` family (`3/3 PASS`).
- 2026-05-09: The journal-side helper closures are now also explicitly revalidated against the full local regression surface instead of only the targeted observe tests. After landing both the destination-freeing `machine_journal_clone_file(...)` repair and the `is_single_record_journal` summary/verifier alignment fix, a fresh serial `make test` run completed with `EXIT:0`, while the already-rotated course/external witnesses stayed green (`lv9` `22/22`, `lava-test/cases` `array*` `6/6`, `compiler2021/.../nested_*` `3/3`). Current authority is therefore that these journal-side maintenance-tail fixes are fully checkpointed on the repository’s local evidence surface and did not reopen neighboring runtime/observe lines.
- 2026-05-09: A further ordered reread chunk then covered the remaining observe-chain implementations and tests around `machine_event`, `machine_outcome`, `machine_history`, `machine_timeline`, and `machine_log` after the journal repairs. This chunk explicitly rechecked their clone paths, report-refresh paths, custom-step fixtures, and single-entry/tick/line summary flags against the corresponding verifier rules. No additional concrete bug was found in that span beyond the already-landed journal fixes, and the same current evidence surface remained green (`make test` already `EXIT:0`; `lv8`/`lv9` green; rotated external `nested_*` and `array*` samples green). Current authority is therefore that the observe-chain tail from `event -> log` has been reread end-to-end in maintenance-first mode and is not currently carrying another confirmed contract bug or runtime-RE lead.
- 2026-05-09: The strict whole-code runtime-RE audit advanced one more full maintenance chunk through the `machine_load -> machine_runtime -> machine_launch` boundary, this time focusing on address-range arithmetic and zero-fill/mapped-span contracts rather than only clone/report helper symmetry. One real bug cluster was closed in the live tree:
  `1)` `machine_load_verify_file(...)` had been treating `(file_byte_count > 0 && !bytes)` as the only missing-payload failure, which meant a verifier-illegal segment shape like `file_byte_count == 0 && memory_byte_count > 0 && bytes == NULL` could still fall through into the zero-fill loop and dereference `segment->bytes[zero_fill_index]` from a null pointer;
  `2)` both `machine_load`/`machine_runtime` bounds and overlap logic were still doing raw `virtual_address + byte_count` arithmetic, so crafted high-address segments could silently wrap `size_t` and corrupt span/overlap/gap/launch reasoning instead of being rejected as malformed.
  The current tree now requires backing bytes for any non-zero `memory_byte_count` load segment, rejects wrapped load/runtime segment ends through explicit checked-add helpers, protects mapped-byte accumulation from unsigned overflow, and rejects runtime stack-base / initial-SP derivations when the stack-gap or stack-size additions would wrap. The same safety line now also reaches the report/query side because the shared load/runtime memory-summary and region-summary helpers use the same checked-end arithmetic instead of open-coded wrapped sums. Focused regression coverage now locks both sides:
  `tests/machine/runtime/machine_load/machine_load_test.c` rejects the zero-fill-only null-bytes shape and a wrapped high-address load segment, while `tests/machine/runtime/machine_runtime/machine_runtime_test.c` rejects a wrapped `load -> runtime` stack-placement derivation before launch state is materialized. Kept rechecks after this closure are `make test-machine-load` PASS, `make test-machine-runtime` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` filtered to the `var` family (`7/7 PASS`; plus the separate `return` filter `1/1 PASS`). Current authority is therefore that the ordered machine-tail reread has now also reclosed one genuine runtime-address-contract bug family in `machine_load` / `machine_runtime`, not only smaller maintenance-side helper drift.
- 2026-05-09: The sequential runtime-RE audit then advanced one sibling layer further down into `machine_image` and `machine_exec`, again looking for the same class of address-span and verifier/query drift that can propagate into generated-program runtime crashes. One more concrete contract bug family was closed in the live tree: `machine_image_verify_file(...)`, `machine_image_file_find_segment_covering_virtual_address(...)`, `machine_image_segment_copy_bytes(...)`, and the corresponding report/query helpers were still using unchecked `base + size` / `offset + size` arithmetic, so a crafted high-address image segment, symbol, or relocation could silently wrap `size_t` and pollute image-span, coverage, or relocation-site reasoning instead of being rejected. The current tree now uses explicit checked-add helpers in `machine_image`, and `machine_image_verify_file(...)` now rejects wrapped segment/image-offset/symbol-address/relocation-site calculations directly. Focused regressions now lock both the image and exec sides: `tests/machine/runtime/machine_image/machine_image_test.c` rejects wrapped segment-span, symbol-address, relocation-address, and image-offset cases, while `tests/machine/runtime/machine_exec/machine_exec_test.c` rejects a wrapped image before exec materialization. Kept rechecks after this closure are `make test-machine-image` PASS, `make test-machine-exec` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` filtered to the `func` family (`12/12 PASS`). Current authority is therefore that the runtime-tail audit has now also reclosed the next downstream address-contract hole in `machine_image` / `machine_exec`, not just the earlier load/runtime span family.
- 2026-05-09: The same sequential runtime-RE audit then stepped one more sibling layer downward into `machine_launch -> machine_step -> machine_decode -> machine_payload_decode`, with the same focus on address-window and fetch/payload consistency. One more real bug was confirmed and fixed in the live tree: `machine_payload_decode_build_from_machine_decode_file(...)` had already been rejecting wrapped or too-long payload windows, but `machine_payload_decode_verify_file(...)` did not enforce the same derived-memory contract, which meant a bad clone/manually mutated file could pass verifier even though its `current_byte_memory_offset` and payload shape would read past mapped memory. The current tree now uses checked-add window validation in payload decode verification too, so the verifier and builder agree on the same read window. Focused regressions now lock the whole lower tail slice with `tests/machine/runtime/machine_payload_decode/machine_payload_decode_test.c` rejecting a wrapped payload window, while the earlier `machine_launch` / `machine_step` / `machine_decode` helpers stayed green under the same local sweep. Kept rechecks after this closure are `make test-machine-launch` PASS, `make test-machine-step` PASS, `make test-machine-decode` PASS, `make test-machine-payload-decode` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and a rotated external sweep on `third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` filtered to the `const` family (`2/2 PASS`). Current authority is therefore that the runtime-tail audit has now closed another verifier/build drift in the `launch -> step -> decode -> payload` tail, and the next review chunk should continue further along the same ordered machine/observe tail rather than reopening the already-closed window-contract families.
- 2026-05-09: The sequential audit then continued past the launch/step/decode/payload tail and into the observe chain, but in this chunk the main result was a no-bug continuation rather than a new patch. I re-read `machine_observe`, `machine_delta`, `machine_trace`, `machine_event`, `machine_outcome`, `machine_history`, `machine_timeline`, `machine_log`, and `machine_journal` at the source level together with their matching tests, and no new concrete runtime-RE root cause was proven in that span beyond the already-closed journal/observe maintenance items. The current state of the live tree remained green on the full local suite and the rotated validation surface already being tracked, so the current authority is that the observe-chain tail remains maintenance-first and should continue to be reread from the next file/stage boundary rather than revisiting the already-checked launch/decode/payload contracts. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-09: The same observe-tail restart has now also been completed as a **full sibling-file audit pass** rather than only a stage-spine reread. This follow-up explicitly reread all current `src/machine/observe/*` implementation siblings again (`machine_observe`, `machine_delta`, `machine_trace`, `machine_event`, `machine_outcome`, `machine_history`, `machine_timeline`, `machine_log`, `machine_journal`), then cross-checked the matching public headers under `include/machine/` and the matching observe tests under `tests/machine/observe/`. Current conclusion is still conservative: no additional concrete generated-program runtime-RE bug was proven in this full-source/full-test-tail pass beyond the already-closed runtime/image/decode/journal maintenance fixes, and no new verifier/build/report drift was confirmed in the remaining observe exact/preview/blocked ladders. Current sequential audit boundary for the machine tail is therefore now the **end of the current observe chain**, with this stage recorded as whole-stage/full-sibling covered rather than only partially read. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-09: The ordered machine-tail audit then advanced back into `machine_select`, this time finding and closing one concrete verifier hole rather than another no-bug pass. `machine_select_verify_program(...)` had been validating that `LOAD_LOCAL`/`LOAD_GLOBAL`/`STORE_LOCAL`/`STORE_GLOBAL` operands pointed at an in-range slot, but it was not checking that the slot kind actually matched the op family. That meant verifier-legal selected programs could still contain a semantically malformed `load_local` that pointed at a global slot, or a `store_global` that pointed at a local slot, and those bad shapes could survive into later lowering/bytes/runtime stages. The live tree now rejects those op/slot-kind mismatches explicitly, and `tests/machine/lowering/machine_select/machine_select_test.c` now mutates a valid selected program to lock both the load-mismatch and store-mismatch cases. Focused and kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` filtered to the `array` family (`6/6 PASS`). Current sequential audit boundary is now the `machine_select` verifier/cleanup line, with no long-running sweep left in flight at the end of this chunk.
- 2026-05-09: The same machine-select line then moved one more layer down into the cleanup/reuse helpers and closed a second concrete wrong-code-enabling hole. The repeated internal pure-call reuse path had been treating only direct `store_global` / `store_global_imm` as a global write conflict, so a `store_indirect` to a global address could still sit between two identical pure calls and allow the second call to be incorrectly reused even though the first result should have been invalidated. The live tree now treats `store_indirect` as a conflict whenever the candidate pure call reads globals, and the machine-select regression suite now locks a focused cross-function repro where a helper reads a global and `main` writes the same global via `addr_global + store_indirect` between two identical helper calls. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `third_party/sysy-suites/TrivialCompiler/custom_test` filtered to the `global` family (`4/4 PASS`). Current sequential audit boundary is now the `machine_select` cleanup/reuse line, and no long-running sweep remained in flight at the end of this chunk.
- 2026-05-09: The same cleanup/reuse reread then exposed one more alias hole in the repeated pure-call reuse logic. Even after the earlier global-write conflict fix, the call-argument invalidation path still only treated direct `store_local` / `store_global` as mutations of `LOCAL_VALUE` / `GLOBAL_VALUE` argument descriptors, so a repeated pure call whose argument came from `load_global g` could still be incorrectly reused across `addr_global g ; store_indirect ...` because the argument-desc invalidation step failed to connect the indirect store back to the same slot. The live tree now lets the call-argument/store matcher inspect `store_indirect` address roots too: if the address source proves to be the same `addr_local` / `addr_global` slot it invalidates precisely, and if the indirect address root cannot be proven it now invalidates conservatively for local/global value/address descriptors instead of silently assuming safety. The regression suite now locks a focused helper-reads-global repro where `main` performs `addr_global + store_indirect` between two otherwise identical helper calls, and the second call must stay materialized instead of being reused. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and a rotated external sweep on `third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` filtered to the `func` family (`111/111 PASS`). Current sequential audit boundary remains the deeper `machine_select` cleanup/reuse line, with no long-running sweep left in flight at the end of this chunk.
- 2026-05-09: The same machine-select revisit also hardened two more structural contract edges that showed up while extending the cleanup regressions. First, `machine_select_verify_program(...)` had still been missing some crash-proof table checks: malformed selected programs with `local_count > 0 && locals == NULL` or `op_count > 0 && ops == NULL` could still reach raw table walks instead of being rejected cleanly. The verifier now rejects those malformed backing-table shapes directly, `machine_select_program_free(...)` / function/block free helpers are now null-safe enough to tear down such mutated regression objects without dereferencing the missing tables again, and the machine-select suite now locks focused malformed-local-table and malformed-op-table witnesses. Second, the lowerer itself had a real control-path bug around cleanup return semantics: `machine_select_cleanup_pure_program(...)` returns a “changed or not” signal, but `machine_select_lower_program_from_machine_ir(...)` had been treating `0` as hard failure in the post-reuse reruns, so a perfectly valid “cleanup succeeded but had nothing more to do” state after internal-pure-call or spill-pure-expression reuse could incorrectly abort lowering. The current tree now treats negative cleanup results as failure and plain `0` as success-without-extra-change, which re-stabilizes the existing `live-in spill-call` reuse coverage instead of turning it into a false lowering failure. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and a rotated external sweep on `third_party/sysy-suites/lava-test/cases` filtered to the `many_` family (`6/6 PASS`). Current sequential audit boundary remains inside the remaining `machine_select` sibling detail surface, and no long-running sweep remained in flight at the end of this chunk.
- 2026-05-09: The same `machine_select` tail then closed the matching low-level query/dump crash-proof gap too. Several public helper surfaces were still trusting structural backing tables that the verifier had only just started rejecting: `machine_select_program_get_function_by_name(...)`, `machine_select_program_get_block_by_function_name_and_block_id(...)`, `machine_select_function_get_block(...)`, `machine_select_function_compute_summary(...)`, and `machine_select_dump_program(...)` could still dereference missing `functions` / `blocks` / `ops` tables, and the dump path could also still walk a call op with `arg_count > 0 && args == NULL` instead of failing cleanly. The live tree now treats those malformed selected-program shapes as query/dump failures rather than as raw pointer walks, and the machine-select regression suite now locks focused failure-path witnesses for missing function tables, missing block/op tables, and missing call-arg tables. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), plus rotated external sweeps on `third_party/sysy-suites/lava-test/cases` for `nested_calls` (`2/2 PASS`) and `register_alloc` (`1/1 PASS`). Current sequential audit boundary is still the remaining `machine_select` query/report/lower-control sibling surface, and no long-running sweep remained in flight at the end of this chunk.
- 2026-05-09: The same `machine_select` sibling-tail reread has now also closed one more runtime-facing control-fold bug plus the matching remaining lower-report table-trust gap. On the generated-code side, `machine_select_try_lower_constant_branch(...)`, `machine_select_try_lower_materialized_boolean_branch(...)`, and the later selected cleanup branch-to-jump fold had still been deciding branch truthiness on raw host `long long` immediates instead of normalized 32-bit SysY integer semantics, so shapes like `br 4294967296, then, else` or `mov r0, 4294967296 ; br r0, then, else` could fold to the wrong jump target even though SysY semantics normalize that value to `0`. The live tree now normalizes those branch-fold immediates before choosing the taken edge, and the regression suite now locks three focused witnesses: direct constant-branch folding, materialized-boolean branch folding, and the later cleanup-driven copy/immediate propagation path that folds a branch only after selected cleanup composes the immediate into the terminator. On the structural-hardening side, the remaining lower-report query/dump helpers had still been trusting `report->program.functions` / `function_summaries` / `function_block_summary_offsets` / filter-index tables too eagerly during by-name lookups and report dump assembly. The live tree now routes report-side by-name function lookup through the already-hardened selected-program query surface, validates report count/table drift before dump assembly, and refuses malformed function/filter/block-summary table shapes instead of dereferencing them. The regression suite now also locks a focused malformed-report witness where a valid selected lower report has its function table cleared before by-name lookup / filter lookup / dump. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter while third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`7/7`). Current sequential audit boundary is now effectively at the end of the remaining `machine_select` query/report/lower-control sibling surface, with no long-running sweep left in flight at the end of this chunk. One first attempt to run that rotated external slice via `autotest --filter` used the wrong CLI surface and was immediately corrected to `tools/sweep_sysy_suite.py`; that command misuse is logged only as operator/tooling noise, not as a compiler failure signal.
- 2026-05-09: The next ordered strict-audit stage has now also been reread across the full `machine_layout` sibling set, and this chunk closed one more real downstream-contract hole. `machine_layout_verify_program(...)` had still been accepting several malformed layout-family shapes that later layout consumers implicitly rely on as semantic contracts: `load_local`/`load_global` and store-local/store-global families were only checking in-range slot ids instead of matching slot kind; spill-result versus register-result call families were not enforcing the matching result operand kind; and `_IMM` binary / compare / compare-branch layout families were not enforcing rhs-immediate form. Those malformed layout programs could therefore stay verifier-legal and drift into `machine_emit` / `machine_encode` / `machine_bytes` with the wrong opcode-family meaning. The live tree now rejects those mismatches explicitly, and `tests/machine/lowering/machine_layout/machine_layout_test.c` now locks focused mutated-program witnesses for load/store slot-kind mismatch, call-result family mismatch, and compare-branch-imm rhs-shape mismatch. The same reread also hardened the remaining layout report/query/dump surface: report refresh now re-verifies the owned layout program before rebuilding summaries, by-name lookups now clear/guard outputs through the selected hardened program query path, block-summary queries now reject function-summary-count drift, and report dump now rejects missing function/filter/block-summary tables or block-summary offset drift instead of trusting malformed report backing state. The regression suite now also locks a focused malformed-layout-report witness where a valid report has its function table cleared before by-name query / dump. Kept rechecks after this closure are `make test-machine-layout` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter if_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`8/8`). Current sequential audit boundary is now the next layout consumers `src/machine/lowering/machine_emit/*` and then `src/machine/lowering/machine_encode/*`, with no long-running sweep left in flight at the end of this chunk.
- 2026-05-09: The next ordered strict-audit stage has now also been reread across the full `machine_emit` sibling set, and this chunk closed another downstream-contract cluster before `machine_encode`. `machine_emit_verify_program(...)` had still been accepting several malformed emitted-program shapes that later encode/bytes stages interpret structurally: `_IMM` binary / compare families were not enforcing rhs-immediate form; spill-result versus register-result call families were not enforcing the matching result operand kind; and load/store local/global families were still only checking in-range slot ids instead of matching slot kind. Those malformed emit programs could therefore remain verifier-legal and drift further downstream with the wrong opcode-family meaning. The live tree now rejects those mismatches explicitly, and `tests/machine/lowering/machine_emit/machine_emit_test.c` now locks focused mutated-program witnesses for load/store slot-kind mismatch, call-result family mismatch, and compare-branch-imm rhs-shape mismatch. The same reread also found one public lifecycle bug in the lowerer itself: `machine_emit_lower_program_from_machine_layout(...)` had not been clearing the destination program before rebuilding it, so lowering into a reused `MachineEmitProgram` could leave stale state/leak old blocks instead of acting like the rest of the pipeline’s overwrite-style lowerers. The current tree now frees/reinitializes the destination first, and the regression suite now also locks a focused overwrite witness where a pre-populated stale emit program is reused as the destination and must end up containing only the fresh lowered output. Finally, the remaining emit report/query/dump surface is now hardened too: report refresh re-verifies the owned emit program before rebuilding summaries, by-name/block-label query outputs are cleared/guarded more consistently, and report dump now rejects missing function/filter/block-summary tables or block-summary offset drift instead of trusting malformed report backing state. The regression suite now also locks a focused malformed-emit-report witness where a valid report has its function table cleared before by-name query / dump. Kept rechecks after this closure are `make test-machine-emit` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter short third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current sequential audit boundary is now the next ordered consumer stage `src/machine/lowering/machine_encode/*`, with no long-running sweep left in flight at the end of this chunk.
- 2026-05-09: The next ordered strict-audit stage has now also been reread across the full `machine_encode` sibling set, and this chunk closed the matching verifier/report drift at the next consumer boundary. `machine_encode_verify_program(...)` had still been treating encoded blocks mostly as offset containers and was not re-validating the embedded emitted-op / terminator-family contracts that later `machine_bytes` / object consumers interpret semantically. In that state, malformed encoded blocks with missing op tables, wrong local/global slot kinds, wrong spill-result/register-result call families, bad immediate-call families, or compare-branch-imm shapes that violate rhs-immediate form could remain verifier-legal and continue downstream with the wrong opcode-family meaning. The live tree now rechecks those embedded op/terminator invariants explicitly, and `tests/machine/lowering/machine_encode/machine_encode_test.c` now locks focused mutated-program witnesses for load/store slot-kind mismatch, call-result family mismatch, compare-branch-imm rhs-shape mismatch, and malformed encode-report query/dump on a cleared function table. The same reread also hardened the encode report/query/dump surface: report refresh now re-verifies the owned encoded program before rebuilding summaries, by-name/block-label query helpers now clear outputs more defensively, and report dump now rejects missing function/filter/block-summary tables or summary-offset drift instead of trusting malformed report backing state. Kept rechecks after this closure are `make test-machine-encode` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter sort_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`7/7`). One first rotated third-party attempt used `--filter recursion`, which produced `NO_CASES` for this suite root and was immediately replaced with the real `sort_test` slice; that is logged only as operator filtering noise, not as a compiler regression signal. Current sequential audit boundary is now back at the already-reread post-encode downstream consumers (`machine_bytes` / object / reloc / elf / runtime bridge), so the remaining strict whole-repo work is effectively endgame maintenance-first reread and cross-stage recheck rather than one obvious unread machine stage.
- 2026-05-10: The post-encode downstream cross-stage pass has now stepped back through `machine_bytes` together with the immediate object/reloc consumer tests. This chunk did **not** yet prove a new `machine_bytes` implementation bug in the generated-program runtime-RE sense; instead it exposed a real cross-stage maintenance drift after the stricter `machine_encode` verifier closures. Several bytes/object/reloc regressions had been constructing malformed emit inputs (for example missing register banks for register-bearing ops, missing local/global counts for slot users, or similar upstream metadata holes) and then expecting the later bytes/object/reloc consumer to be the first rejecting layer. With `machine_encode` now checking those contracts earlier, the downstream tests were no longer measuring the intended bytes/object/reloc behavior. The live tree now re-legalizes those downstream fixtures in `tests/machine/object/machine_bytes/machine_bytes_test.c`, `tests/machine/object/machine_object/machine_object_test.c`, and `tests/machine/object/machine_reloc/machine_reloc_test.c` so they target the real bytes/object/reloc semantics again instead of tripping on already-illegal upstream shapes. Kept rechecks after this maintenance closure are `make test-machine-bytes` PASS, `make test-machine-object` PASS, `make test-machine-reloc` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter matrix third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`4/4`). Current sequential audit boundary remains in the same post-encode downstream consumer region, with the next implementation reread focus still on `machine_object` / `machine_reloc` / `machine_elf` sibling code rather than on reopening the already-closed encode-layer contract checks.
- 2026-05-10: The same post-encode downstream pass then moved into `machine_object` and `machine_reloc` implementation rereads and closed a small but real report/query robustness cluster there. On the object side, `machine_object_file_copy_section_bytes(...)` had still been willing to copy from a non-empty section even if its `bytes` table was missing, which could turn a malformed object file into a null-source `memcpy(...)` instead of a clean failure. The current tree now rejects that shape directly. On both object and reloc report surfaces, by-name report queries now clear outputs more defensively and report dumps now reject malformed summary-table drift (missing section/symbol/fixup/relocation summary tables or summary-count mismatches) instead of trusting those backing tables. The regression suites now lock both the object-side missing-section-bytes case and malformed object/reloc report query/dump witnesses. Kept rechecks after this closure are `make test-machine-object` PASS, `make test-machine-reloc` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter add third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`5/5`). Current sequential audit boundary is now the next downstream sibling stage `src/machine/object/machine_elf/*`, with no long-running sweep left in flight at the end of this chunk.
- 2026-05-10: The next strict post-encode reread chunk has now fully covered the `machine_elf` sibling stage (`machine_elf.c`, `machine_elf_build.inc`, `machine_elf_core.inc`, `machine_elf_dump.inc`, `machine_elf_parse.inc`, `machine_elf_query.inc`, `machine_elf_verify.inc`) together with `tests/machine/object/machine_elf/machine_elf_test.c`, and this one did close two real helper-surface bugs instead of ending as a no-op pass. First, the dedicated file-section helpers (`machine_elf_file_get_text_section(...)`, `...get_strtab_section(...)`, `...get_symtab_section(...)`, `...get_rel_text_section(...)`, `...get_shstrtab_section(...)`) had still been zeroing `out_section_index` but never writing back the actual canonical section index on success, so callers using the dedicated helper family could receive the right section pointer but the wrong index (`0`) at the same time. Second, `machine_elf_file_get_first_global_symbol_index(...)` had still been rejecting the verifier-legal all-local ELF partition where `symtab.sh_info == symbol_count`, even though the rest of `machine_elf` already treats that as the canonical “there is no first global symbol” boundary. The live tree now returns the real dedicated section index on success and accepts `sh_info == symbol_count` as a valid all-local partition. The ELF regression suite now locks both surfaces: one focused helper check requires the dedicated file-section helpers to return indices `1..5` on a real built file, and another mutates that same file into an all-local symtab then requires `machine_elf_refresh_bytes(...)` plus `machine_elf_file_get_first_global_symbol_index(...)` to preserve the valid `symbol_count` boundary. Kept rechecks after this closure are `make test-machine-elf` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter if_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`8/8`). Current sequential audit boundary is now past the whole post-encode object tail (`machine_bytes -> machine_object -> machine_reloc -> machine_elf` all reread in full-sibling mode), with no long-running sweep left in flight at the end of this chunk; if the runtime-RE hunt continues from here, the next maintenance-first bridge to reopen is the already-reread downstream runtime/image boundary rather than another unread object-stage file set.
- 2026-05-10: The next maintenance-first runtime/image bridge follow-up has now reopened the query/report surface around `machine_runtime` while rereading the adjacent `machine_image` / `machine_exec` / `machine_load` helpers for the same class of malformed-table drift. This chunk did find one real robustness cluster in the live tree: several raw/report runtime helpers were still willing to walk `runtime_file->segments` or `report->segment_summaries` without first proving those backing tables existed and still matched the recorded counts. In that state, manually mutated or stale report/file objects could still turn helpers such as `machine_runtime_file_get_summary(...)`, `machine_runtime_file_get_gap_summary(...)`, `machine_runtime_file_get_segment(...)`, `machine_runtime_file_get_stack_segment(...)`, `machine_runtime_file_get_executable_segment_count(...)`, `machine_runtime_file_get_region_summary(...)`, and the corresponding report-side entry/by-name/by-address/filter helpers into null-table dereferences instead of clean failures. The current tree now rejects that malformed runtime table state consistently on both the raw and report surfaces, including segment-summary count drift and missing executable/non-executable filter tables. `tests/machine/runtime/machine_runtime/machine_runtime_test.c` now locks a focused malformed-runtime witness by building a valid runtime file/report, clearing the segment tables, and requiring the affected raw/report query helpers to fail cleanly instead of succeeding. Kept rechecks after this closure are `make test-machine-runtime` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter op_priority third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`5/5`). Current sequential audit boundary remains inside the same downstream runtime/image bridge rather than past it: `machine_runtime` query/report hardening is now reclosed, while the next maintenance-first reread should continue across the remaining `machine_image` / `machine_exec` / `machine_load` sibling helper surfaces before declaring the whole bridge fully rechecked again. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The same downstream runtime/image bridge reread has now advanced through the parallel `machine_load` / `machine_exec` helper surfaces and closed the matching malformed-table cluster there too. Both modules still had raw/report query helpers that would trust `file->segments`, `report->segment_summaries`, and report-side executable/non-executable filter index tables more than their public contracts justified. In that state, manually mutated or stale load/exec file/report objects could still leave helpers such as `machine_load_file_get_summary(...)`, `machine_load_file_get_memory_summary(...)`, `machine_load_file_get_segment(...)`, `machine_load_file_get_entry_segment(...)`, `machine_load_file_get_executable_segment_count(...)`, `machine_exec_file_get_summary(...)`, `machine_exec_file_get_segment(...)`, `machine_exec_file_get_entry_segment(...)`, and the corresponding report-side entry/by-name/by-address/filter helpers succeeding on drifted metadata instead of failing cleanly. The live tree now consistently rejects missing segment tables, segment-summary count drift, and missing executable/non-executable filter index tables on those helper surfaces too. Focused regressions now lock both layers: `tests/machine/runtime/machine_load/machine_load_test.c` and `tests/machine/runtime/machine_exec/machine_exec_test.c` each build a valid file/report, clear the backing segment/filter tables, and require the affected raw/report helpers to fail rather than dereference or silently return stale data. Kept rechecks after this closure are `make test-machine-load test-machine-exec` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter unary_op third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current sequential audit boundary is now narrowed again to the remaining `machine_image` helper surface inside the same downstream bridge: `machine_runtime`, `machine_load`, and `machine_exec` query/report maintenance are reclosed, while the next ordered reread chunk should finish `machine_image` before treating this whole bridge as maintenance-first complete again. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The final remaining `machine_image` helper reread inside that downstream runtime/image bridge is now also reclosed, and with it the bridge itself is back to maintenance-first complete. The remaining issue there matched the same structural family but at a denser report surface: many raw/report image helpers still accepted nonzero `segment/symbol/relocation` counts with missing backing tables or accepted report-side count drift without proving the cached summary/index arrays still existed. In that state, stale or hand-mutated image file/report objects could still let helpers such as `machine_image_file_get_summary(...)`, `machine_image_file_get_segment(...)`, `machine_image_file_get_symbol(...)`, `machine_image_file_get_relocation(...)`, the segment/symbol/relocation subset counters, and a broad swath of report-side overview/segment/symbol/relocation/filter/artifact helpers succeed on invalid backing state instead of failing cleanly. The live tree now has explicit raw/report table-presence guards in `machine_image`, including report-side summary-count drift and missing filter/subset index arrays. `tests/machine/runtime/machine_image/machine_image_test.c` now locks a focused malformed-image witness by building a valid image file/report, clearing the raw segment/symbol/relocation tables plus selected report summary/filter tables, and requiring the affected raw/report query helpers to fail rather than dereference or return stale data. Kept rechecks after this closure are `make test-machine-image` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter hex third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current authority is therefore that the whole downstream runtime/image bridge (`machine_image -> machine_exec -> machine_load -> machine_runtime`) has now been reread again in maintenance-first full-sibling mode and no longer contains a known open malformed-table/query-drift bug cluster. If the runtime-RE audit continues from here, the next useful mainline is no longer “finish unread bridge helpers”, but cross-stage generated-program runtime hunting or another broader downstream maintenance revisit rather than another missing file-family read. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next cross-stage generated-program runtime-RE revisit has now stepped back up from the bridge helpers into the final compiler-side RISC-V preview text peepholes, and this chunk closed one real wrong-code risk there. `compiler_optimize_riscv_preview_repeated_indexed_addr_sequences(...)` had already been blocking reuse across label boundaries, base-memory stores, base-register redefinitions, and some call-like barriers, but it was still missing one direct invalidation edge: if the previously loaded base value register itself (`first_load_rd`) was redefined in between two otherwise identical indexed-address sequences, the optimizer could still delete the second `lw + add` rebuild and leave the later address computation using the stale redefined register value instead of reloading the real base pointer from memory. That is a genuine generated-code wrong-code/runtime-RE risk rather than only a report/query robustness issue. The live tree now treats redefinition of that loaded base register as an invalidation barrier for the repeated-indexed-address sequence fold. `tests/compiler/compiler_driver_test.c` now locks a focused repro where `t6` is incremented between two otherwise identical `slli ; lw t6, ... ; add ...` sequences, and the second sequence must stay materialized instead of being elided. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter hex third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current sequential audit boundary is therefore now the remaining compiler-side / final-export generated-code line around the other preview text peepholes and nearby `machine_bytes`/driver handoff, not the already-reclosed runtime/image bridge. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: A same-line follow-up on the compiler-side preview-text peepholes immediately corrected one small process slip and refreshed the rotated validation surface without changing the code again. The previous kept recheck for the repeated-indexed-address closure had accidentally reused the `hex` third-party slice from the immediately preceding chunk, which is not the intended rotation habit for this thread. A new rotated external sweep is therefore now recorded on `tools/sweep_sysy_suite.py --filter scope third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021`, and it stays green (`1/1 PASS`). Current authority is unchanged on the code side: the loaded-base-register invalidation fix remains the active compiler-side closure, and the next useful reread chunk should continue through the remaining preview-text peepholes / `machine_bytes -> compiler_driver` handoff instead of revisiting the now-closed runtime/image bridge.
- 2026-05-10: The next compiler-side preview-text reread chunk has now advanced into the remaining `indexed_local_base_offsets` peephole and closed another real wrong-code risk there. That rewrite removes one temporary local-base materialization like `addi t6, sp, 24` and folds later `add addr, t6, idx ; lw/sw ..., 0(addr)` into `add addr, sp, idx ; lw/sw ..., 24(addr)`. The missed edge was that the implementation had been willing to do this even when the “index” operand was actually the **same register as the folded base register itself** (`add a0, t6, t6` after `addi t6, sp, 24`). In that shape the fold is not algebraically valid at all: deleting the `addi t6, sp, 24` line leaves the transformed code reading an undefined/stale `t6` while also moving the base offset to the final memory op. The live tree now explicitly rejects this fold when `index_reg == base_reg`. `tests/compiler/compiler_driver_test.c` now locks a focused repro where `t6` appears on both sides of the `add`, and the original three-line sequence must stay unchanged. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter assign_complex third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`1/1`). Current sequential audit boundary remains on the same compiler-side / final-export generated-code line, now past this second `indexed_*` peephole risk and moving next toward the remaining preview-text peepholes plus the `machine_bytes -> compiler_driver` handoff. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next same-line reread chunk then stepped back to the adjacent `stack_staged_call_args` peephole and closed one more real wrong-code risk there too. That rewrite recognized a staged stack-call-arg pattern of the form `lw tmp0, src0 ; sw tmp0, dst0 ; lw tmp1, src1 ; sw tmp1, dst1 ; lw a0, dst0 ; lw a1, dst1 ; call ...` and tried to collapse the trailing reloads into earlier `lw a0, src0 ; ... ; lw a1, src1 ; ... ; call ...`. The missing contract edge was when both final call-arg reloads read the **same destination stack slot** (`dst0 == dst1`). In that case the original code deliberately loads both `a0` and `a1` from the post-store final contents of one shared slot, but the transformed code would instead pass the two pre-store source values separately. The live tree now rejects the fold when `dst0_offset == dst1_offset`. `tests/compiler/compiler_driver_test.c` now locks a focused repro where two staged stores both target `0(sp)` before the final `lw a0, 0(sp)` / `lw a1, 0(sp)` pair, and the pattern must stay unchanged. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter comment third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current sequential audit boundary remains inside the same compiler-side / final-export generated-code line, now with both the repeated-indexed-address family and the staged-stack-call-arg family reclosed; the next useful reread chunk should continue through the remaining preview-text peepholes and then the `machine_bytes -> compiler_driver` handoff rather than revisiting these now-locked folds. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next compiler-side follow-up has now crossed the `machine_bytes -> compiler_driver` handoff and closed one matching malformed-summary robustness cluster at that boundary. Several `machine_bytes_report` helper surfaces were still willing to succeed on stale/mutated reports even when the corresponding cached summary tables or offset tables had been cleared: `machine_bytes_report_get_function_reference_summaries(...)`, `machine_bytes_report_get_function_fixup_summaries(...)`, `machine_bytes_report_get_function_byte_span(...)`, `machine_bytes_report_find_block_summary_by_program_byte_offset(...)`, and `machine_bytes_report_get_block_summary(...)` could still accept missing backing arrays or offset drift instead of failing cleanly. That matters for the current workstream because `compiler_driver` consults these report surfaces while deciding call/result preservation and while pretty-printing block labels and per-function byte spans; malformed success there can mislead the final text exporter even if the underlying byte program remains valid. The live tree now rejects missing summary/offset tables and basic offset-order drift on those report helpers, and the same summary-shape guard now also reaches the `functions_with_calls/fallthrough/branches` helper family. `tests/machine/object/machine_bytes/machine_bytes_test.c` now locks a focused malformed-report witness by building one valid bytes report, clearing the cached reference/fixup/block/offset tables, and requiring the affected report helpers to fail rather than return stale slices. Kept rechecks after this closure are `make test-machine-bytes` PASS, `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `tools/sweep_sysy_suite.py --filter comment third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`2/2`). Current sequential audit boundary therefore remains on the compiler-side / final-export line, but now the immediate `machine_bytes -> compiler_driver` summary-handoff helper cluster is also reclosed; the next useful reread chunk should move on to any remaining final-export/control-flow text transforms or broader generated-program runtime repro hunting rather than revisiting this already-hardened report bridge. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next sequential reread chunk stayed on the same compiler-side / final-export line and explicitly re-read the remaining caller-save / call-span sub-slice in `compiler_driver.c`, but this one ended as a **no-new-bug continuation** rather than a new patch. I rechecked the caller-save helpers (`compiler_append_caller_save_sequence(...)`, the `call_fixup`-driven save/restore hooks in the main text-emission loop, and the surrounding call-span integration) against the existing high-arity / nested-call / far-call tests and direct rebuilt-CLI output probes. Two concrete observations are now recorded as execution memory: `1)` the current live exporter does not actually enable the `save_caller_regs_around_call` path in the emitted-text mainline today, so that code is not the active explanation for the current generated-program runtime surface, and `2)` the existing nested-call / far-call output still matches the currently locked expectations after the recent final-export and bytes-report fixes. Current authority is therefore conservative: this specific caller-save/call-span sub-slice is now read and not currently carrying a newly proven runtime-RE root cause. The next useful reread chunk should move past it into any remaining final-export/control-flow text transforms or a broader generated-program runtime repro hunt instead of reopening this same sub-slice immediately again. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next sequential reread chunk stayed on the same compiler-side / final-export line but ended as a **no-new-bug continuation** rather than another patch. I reread the remaining caller-save / call-span / text-export path around `compiler_append_caller_save_sequence(...)`, the per-word pretty-printer path in `compiler_append_riscv_preview_instruction(...)`, and the main export loop in `compiler_emit_riscv_preview_text_from_report(...)`, then cross-checked the existing caller-save / nested-call / far-call tests in `tests/compiler/compiler_driver_test.c`. Current conclusion for this chunk is conservative: I did not prove another concrete generated-program runtime-RE bug in that specific caller-save/call-span slice after the newly landed preview-text and bytes-report fixes. The current live tree stays green on the already recorded local/course/external surfaces, and no long-running sweep remained in flight at the end of this chunk. Current sequential audit boundary therefore remains inside the same compiler-side / final-export line, with the next useful reread moving past this caller-save/call-span sub-slice into any remaining final-export/control-flow transforms or a broader generated-program runtime repro hunt rather than reopening the same now-read caller-save block immediately again.
- 2026-05-10: The next same-line compiler-side reread chunk stayed inside `src/compiler/compiler_driver.c` and did find one more real generated-code wrong-code risk in the live preview-text peepholes. `compiler_optimize_riscv_preview_stack_addr_reuse(...)` had been reusing an earlier `addi tmp, sp, imm ; sw tmp, slot(sp)` materialized stack-address write to replace later `lw reg, slot(sp)` reloads with direct `addi reg, sp, imm`, but it was only treating direct `sw ..., slot(sp)` as an invalidation barrier. That missed the equivalent overwrite shape `addi t6, sp, slot ; sw x, 0(t6)`: after such a materialized-base store, the stack slot no longer necessarily contains the original saved address, so rematerializing `addi reg, sp, imm` is wrong and can feed bad pointers into later generated loads/stores/calls. The live tree now treats that two-line materialized stack-slot store as the same overwrite barrier for `stack_addr_reuse(...)` that the neighboring indexed-address peepholes already honor. Existing regression coverage in `tests/compiler/compiler_driver_test.c` now meaningfully locks the exact witness `addi t0, sp, 24 ; sw t0, 0(sp) ; addi t6, sp, 24 ; sw a1, 0(t6) ; lw a0, 0(sp)` by requiring the final reload **not** to collapse into `addi a0, sp, 24`. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), and the rotated external sweep `python3 tools/sweep_sysy_suite.py --filter sort_test third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`7/7`). Current sequential audit boundary remains on the compiler-side / final-export generated-code line, now with the `stack_addr_reuse` overwrite barrier reclosed too; the next useful reread chunk should continue through the remaining final-export/control-flow transforms or shift into a broader generated-program runtime repro hunt if that late-export path no longer yields another concrete bug. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next same-line compiler-side reread chunk stayed in the same late preview-text reuse family and closed one broader call-barrier wrong-code cluster rather than another isolated syntax edge. Several memory-dependent final-text reuse peepholes had still been relying on register-clobber side effects to notice a call boundary: `compiler_optimize_riscv_preview_stack_addr_reuse(...)`, `compiler_optimize_riscv_preview_repeated_indexed_addr_triples(...)`, and `compiler_optimize_riscv_preview_repeated_indexed_addr_sequences(...)` would all stop when a call clobbered one of their working temporaries, but they did **not** treat the call itself as a semantic barrier. That left a real generated-program runtime risk when the helper registers happened to be callee-saved (`s*`) instead of caller-clobbered (`a*`/`t*`): the optimizer could still remove a stack-slot address save/reload pair or a repeated base-pointer reload across `call foo` even though the callee may observe or mutate the same memory and the original pre-call store/load was still semantically relevant. The live tree now treats any call-like line as a barrier for those three memory-dependent preview-text folds instead of depending on accidental register-clobber patterns. `tests/compiler/compiler_driver_test.c` now locks three focused callee-saved witnesses: `stack_addr_reuse` must preserve `addi s1, sp, 24 ; sw s1, 0(sp) ; call foo ; lw s2, 0(sp)`, `repeated_indexed_addr_triples` must keep the second `slli s3, s4, 2 ; lw s2, 24(sp) ; add s3, s2, s3` after `call foo`, and `repeated_indexed_addr_sequences` must likewise keep the second `slli s3, s4, 2 ; lw s2, 0(s1) ; add s3, s2, s3` after the call. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external sweep `python3 tools/sweep_sysy_suite.py --filter matrix third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`4/4`). Current sequential audit boundary remains on the compiler-side / final-export generated-code line, but the memory-dependent reuse peephole family is now reclosed against both materialized-slot overwrites and call-like barriers; the next useful reread chunk should move on to the remaining final-export/control-flow transforms or broaden into generated-program runtime repro hunting if this late-export line stops yielding more concrete bugs. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next sequential reread chunk stayed on the same compiler-side / final-export generated-code line but ended as a **no-new-bug continuation** rather than another patch. I re-read the remaining non-peephole export helpers around `compiler_riscv_preview_cache_build(...)`, `compiler_lookup_block_label(...)`, `compiler_append_pretty_symbol_name(...)`, `compiler_emit_global_sections(...)`, the fixup/block-label cached lookup helpers, `compiler_append_stack_adjust(...)`, `compiler_append_stack_access(...)`, and the per-word pretty-printer path in `compiler_append_riscv_preview_instruction(...)`, then spot-checked the rebuilt CLI output on a simple backward-loop witness to confirm the current live path is still resolving local loop branches to emitted labels rather than falling back to malformed raw text. Current conclusion for this chunk is conservative: I did not prove another concrete generated-program runtime-RE bug in that helper/export span after the recently closed preview-text reuse barriers. The current live tree remains green on the already recorded local/course surfaces, and the rotated external sweep `python3 tools/sweep_sysy_suite.py --filter search third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` also stayed green (`2/2 PASS`). Current sequential audit boundary therefore remains inside the same compiler-side / final-export line, but it has now moved past the cache/global-section/pretty-printer helper span; the next useful reread chunk should continue into any remaining final-export/control-flow transform detail or broaden into direct generated-program runtime repro hunting rather than immediately reopening this now-read helper slice. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next sequential reread chunk stayed on the same `compiler_driver.c` tail but again ended as a **no-new-bug continuation** rather than a new patch. I re-read the remaining export/build boundary from `compiler_emit_riscv_preview_text_from_report(...)` down through `compiler_mode_from_flag(...)`, `compiler_compile_source_text_with_options(...)`, and the file/text wrapper entrypoints, then spot-checked the rebuilt CLI output on a simple 9-argument witness to confirm the current prologue/caller-stack handoff for `param_index >= 8` is still materially sane in live output (`pick(..., a8)` returns via the stack slot as expected). Current conclusion is still conservative: I did not prove another concrete generated-program runtime-RE bug in that remaining compiler-driver entry/export span after the already landed preview-text barrier fixes. The rotated external sweep `python3 tools/sweep_sysy_suite.py --filter jump third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` also stayed green (`1/1 PASS`). Current sequential audit boundary therefore remains at the far end of the compiler-side / final-export line, and the next useful chunk is likely to shift from source reread into broader generated-program runtime repro hunting unless a fresh suspicious final-export/control-flow edge appears. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next audit chunk intentionally started that broader generated-program runtime repro hunting instead of pretending there was still a large unread compiler-driver tail. I kept the source-level context on the now-nearly-exhausted `compiler_driver.c` final-export line, but the concrete work in this chunk was a wider rotated runtime-facing sweep on `third_party/sysy-suites/lava-test/cases` filtered to the array/long-array family (`python3 tools/sweep_sysy_suite.py --filter arr --limit 12 ...`). That sample is useful for the current hidden `RE` target because it exercises address formation, stack/local array layout, long-array/global data handling, and repeated indexed memory traffic, i.e. exactly the kinds of generated-program runtime surfaces that could still hide a crash even after the final-export peephole fixes. Current result is conservative but still valuable: the whole rotated subset stayed green (`11/11 PASS`, including `104_long_array.sy` and `105_long_array2.sy`), so this chunk did **not** reproduce a new runtime crash and did **not** justify another code patch. Current authority is therefore that the compiler-driver final-export reread is now effectively near-exhausted, and the next useful mainline should keep broadening the runtime repro hunt across different external/runtime-heavy subsets unless a fresh source-level suspicion appears. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-repro chunk finally did reproduce a real generated-program failure again, and this one looped back into the still-live compiler-side preview-text line rather than reopening the deeper SSA/backend core. A rotated sweep on `third_party/sysy-suites/minic-test-cases-2021f/functional --filter long` came back `3/4 PASS` with `094_long_code.c` failing as a **runtime crash** (`ACTUAL_EXIT -11`, empty stdout). I minimized that direction to the smaller local witness `copy(arr, result); touch(result);` with two local arrays, and the rebuilt compiler was emitting the bad shape `call copy ; lw a0, 16(sp) ; call touch` without preserving the earlier `spill.0 = addr_local local.2` definition that should have kept the saved array-address slot initialized. A direct machine-stage dump on the minimized witness showed the deeper pipeline was still sane (`machine_select` / `machine_layout` / `machine_emit` / `machine_bytes` all retained `spill.0 = addr_local local.2`), so the bug again traced back to the final text peephole layer: `compiler_optimize_riscv_preview_stack_addr_reuse(...)` was willing to rematerialize one pre-call stack-slot address reload and delete the original `addi tmp, sp, imm ; sw tmp, slot(sp)` definition even when a **later** reload after the barrier still needed that same saved slot. In other words, the helper had learned to stop rewriting *across* a call barrier, but it still forgot that deleting the original saved-slot definition can break later post-barrier loads that keep reading the stack slot. The live tree now scans for later direct/materialized reloads of that same stack slot before removing the original save definition, and therefore refuses this fold when the saved slot remains live past the local rewrite window. `tests/compiler/compiler_driver_test.c` now locks a focused direct-helper witness `addi t0, sp, 24 ; sw t0, 0(sp) ; lw a1, 0(sp) ; call foo ; lw a0, 0(sp)` that must stay unchanged because the later post-call reload still needs the saved slot, and the minimized live CLI witness now emits the preserved `addi t4, sp, 8 ; sw t4, 16(sp)` pair before `call copy` instead of dropping it. Kept rechecks after this closure are `make test-compiler-driver` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the widened external follow-up `python3 tools/sweep_sysy_suite.py --filter long third_party/sysy-suites/minic-test-cases-2021f/functional` PASS (`4/4`, including the formerly crashing `094_long_code.c`). Current authority is therefore stronger than the previous no-bug runtime-hunt chunk: the repro line is now materially productive, and the compiler-driver final-export audit still had one more real runtime-facing corner case left precisely in the stack-address-rematerialization family. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next runtime-hunt chunk broadened further without finding a new red point, so this one is recorded as a **no-new-bug continuation** rather than another patch. After the `094_long_code.c` closure, I widened the runtime-facing evidence surface instead of immediately tunneling into another source-level suspicion: `python3 tools/sweep_sysy_suite.py --filter func third_party/sysy-suites/minic-test-cases-2021f/functional` revalidated the whole `minic-test-cases-2021f/functional` tree in practice (`100/100 PASS`), `--filter putarray` on the same tree stayed green (`1/1 PASS`), `python3 tools/sweep_sysy_suite.py --filter array third_party/sysy-suites/minic-test-cases-2021s/functional` stayed green (`6/6 PASS`), `--filter global` on `minic-test-cases-2021s/functional` stayed green (`1/1 PASS`), and `python3 tools/sweep_sysy_suite.py --filter long third_party/sysy-suites/lava-test/cases` stayed green (`5/5 PASS`, including both `106_long_code.sy` and `107_long_code2.sy`). Current conclusion is conservative but useful: the broadened runtime-heavy external surface did **not** reproduce another generated-program crash immediately after the last fix, so the active mainline should keep rotating to different external/runtime-heavy slices rather than repeatedly hammering the same long/array family. One attempted rotated slice on `compiler2021/.../function_test2021 --filter global` returned `NO_CASES`; that was only filter-selection noise, not a compiler signal. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next follow-up on that same many-args line then closed the first **compile-time** pathology that appeared once the wider generated batch moved past the earlier runtime wrong-code witnesses. After the two allocator/rewrite correctness fixes, `/tmp/many_args_hidden_like_batch` no longer reproduced the old runtime corruption first; instead the earliest seeds (`3001+`) started failing as `COMPILE_TIMEOUT`. Trace-guided profiling on `many_args_hidden_like_3001.sy` showed the real sink was not frontend parsing or machine lowering, but repeated **split-only allocate+rewrite loop rounds** on the large recursive `rec` function: each round re-ran the expensive coalesce analysis on a slightly larger split-expanded SSA program even though no spill-materialization rewrite happened in that round. The live rule is now more conservative there too: if a rewrite-loop iteration performed only block-local spill splitting and then successfully reallocated, we accept that reallocated result as converged instead of demanding another full split-only round. Focused rechecks after this closure are `make test-value-ssa-alloc` PASS, `make test-value-ssa-machine` PASS, `make test-machine-ir` PASS, and the first widened many-args seed `many_args_hidden_like_3001.sy` now reclassifies from `COMPILE_TIMEOUT` to `PASS`. The broader generated batch is still not fully green yet: after this compile-time closure, the next adjacent witnesses `many_args_hidden_like_3002.sy` and `many_args_hidden_like_3003.sy` remain **runtime mismatches**, so the active mainline now moves forward onto those new similar runtime wrong-code cases instead of staying on compile timeout triage.

- 2026-05-10: The follow-up on that generated many-args runtime line has now landed **two concrete allocator/rewrite fixes**, and they materially reclose the reduced witnesses instead of only improving localization notes. First, after a spill-rewrite round triggers post-rewrite canonicalization, the next allocation/layout rebuild must mark all body functions as rewritten, not only the function that originally introduced spills; otherwise sibling functions can keep pre-canonicalization placement ids against the post-canonicalization Value-SSA program. That stale-placement bug directly explained `/tmp/many_args_reduce3/caseD_callee_side_only.sy`, where `wide0` needed `t + g2 + g3` but machine lowering was effectively rebuilding `g3 + g3`. Second, later spill-rewrite rounds are now more conservative once a function already owns concrete spill locals from an earlier round: the rewriter still allows later reallocation/recoloring, but it skips another rewriteable-spill materialization pass for functions whose local table already extends past the remembered pre-rewrite spill-local floor. That prevents the next-round aliasing corruption reproduced by `/tmp/many_args_reduce3/caseG_original_no_branch_in_caller.sy`, where a correct first rewrite round was later broken by reusing existing `spill.0` / `spill.1` locals for newly spilled values and duplicating 12-arg nested-call operands. Focused kept rechecks after these two closures are now green: `make test-value-ssa-alloc` PASS, `make test-value-ssa-machine` PASS, `make test-machine-ir` PASS, `autotest -riscv -s lv8` PASS (`12/12`), `autotest -riscv -s lv9` PASS (`22/22`), and the reduced `.sy` witness pack under `/tmp/many_args_reduce3` (`caseD`, `caseE`, `caseF`, `caseG`, `caseH`, `caseI`, `caseJ`) now all PASS. The broader `/tmp/many_args_hidden_like_batch` widening sweep is still a heavy long-running / compile-pressure-adjacent probe rather than a closed correctness summary, so keep it in the “continue probing” bucket instead of claiming the whole wide-arity family is exhausted.

- 2026-05-10: The next hidden-runtime search round finally produced a **new supported-subset wrong-code witness family** instead of another all-green widening batch, and it is worth treating as the current front-most runtime-RE lead even though the first visible symptom is WA rather than a direct segfault. I first widened three generated pools with different shapes so this would not be another one-family comfort rerun: `/tmp/lv8_hidden_like_batch` (no-array Lv8-style `short-circuit + global mutation + nested calls + recursion`) stayed green `80/80`, `/tmp/array_hidden_like_batch` (array/global/call recursion mixes) stayed green `50/50`, but `/tmp/many_args_hidden_like_batch` came back **`0/60 PASS`** with a uniform many-args red cluster. That cluster is now reduced under `/tmp/many_args_reduce3/`: `caseD_callee_side_only.sy`, `caseE_callee_side_plus_caller_globals.sy`, and `caseG_original_no_branch_in_caller.sy` all still fail, while `caseF_callee_side_plus_caller_params.sy` passes. The minimized shape is: a caller with `12` scalar parameters forwards them into another `12`-arg callee in reversed order; the callee is fine when called directly from `main`, but miscompiles when its body both performs calls/branches and later recombines a pre-call accumulator with reloaded globals. Stage probes on `/tmp/many_args_reduce3/caseD_callee_side_only.sy` now narrow this further than “somewhere late”: canonical IR, lower IR, Value-SSA, and `valueopt` all still preserve the correct `t + g2 + g3` computation in `wide0`, but the first machine allocate/rewrite dump is already wrong in `wide0 bb.1`, where the value becomes `load global.2; add spill.3, reg; load global.3; add reg, reg`—effectively `g3 + g3` instead of preserving `t + g2 + g3`. Current authority is therefore that the runtime hunt has reopened a real **post-valueopt / machine allocate+rewrite** line, specifically a wide-arity nested-call plus side-effectful callee family that is very plausibly segfault-capable once the corrupted value is an address/index/call-state carrier instead of only a printed checksum. No long-running sweep remained in flight at the end of this chunk.

- 2026-05-10: The next runtime-hunt chunk continued that widened external rotation and again ended as a **no-new-bug continuation** rather than a new patch. I intentionally moved away from the already-green `long/array` witnesses and sampled different runtime-heavy stress families: `python3 tools/sweep_sysy_suite.py --filter nested third_party/sysy-suites/lava-test/cases` stayed green (`3/3 PASS` for `nested_calls*` and `nested_loops`), `python3 tools/sweep_sysy_suite.py --filter many third_party/sysy-suites/TrivialCompiler/custom_test` stayed green (`4/4 PASS` for the `many_globals*` / `many_params` line), and `python3 tools/sweep_sysy_suite.py --filter register third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` also stayed green (`1/1 PASS` for `99_register_realloc.sy`). Current conclusion is still conservative: this broader rotated runtime-facing surface did **not** reproduce another generated-program crash after the `094_long_code.c` fix, so the active repro-hunt line remains productive but not currently red on these neighboring nested/global/register stress families. The next useful chunk should keep rotating to a different external/runtime-heavy slice again instead of settling back into one comfort family. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk widened that rotation one more time and still ended as a **no-new-bug continuation** rather than another patch. I kept moving away from the already-revalidated long/array core and sampled a second ring of runtime-heavy neighbors: `python3 tools/sweep_sysy_suite.py --filter many third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` stayed green (`3/3 PASS`), `python3 tools/sweep_sysy_suite.py --filter search third_party/sysy-suites/lava-test/cases` stayed green (`4/4 PASS`), and `python3 tools/sweep_sysy_suite.py --filter loop third_party/sysy-suites/TrivialCompiler/custom_test` stayed green (`5/5 PASS`). I then also spot-checked several single-case complex-algorithm witnesses from `minic-test-cases-2021f/functional` that are useful for runtime-RE hunting because they stress deeper call/loop/memory behavior without being the same old array family: `071_brainfk.c`, `073_dijkstra.c`, `080_max_flow.c`, and `081_n_queens.c` all came back green (`1/1 PASS` each). Current conclusion remains conservative but useful: this additional rotated runtime-facing surface did **not** reproduce another generated-program crash after the last `stack_addr_reuse` closure, so the next useful runtime-hunt chunk should keep broadening sideways into other external heavy slices rather than circling back immediately to the same categories that are already green. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk continued the same widened external rotation and again ended as a **no-new-bug continuation** rather than another patch. I stayed on the runtime-facing mainline but rotated into a third ring of heavier algorithmic witnesses from `minic-test-cases-2021f/functional`: `python3 tools/sweep_sysy_suite.py --filter kmp ...` (`1/1 PASS`), `--filter hanoi ...` (`1/1 PASS`), `--filter backpack ...` (`1/1 PASS`), `--filter substr ...` (`1/1 PASS`), `--filter percol ...` (`1/1 PASS`), `--filter calc ...` (`1/1 PASS`), `--filter color ...` (`1/1 PASS`), and `--filter gcd ...` (`2/2 PASS` for `exgcd` + `gcd`). Current conclusion remains conservative but useful: even after broadening into these deeper algorithm/loop/call/memory cases, I still did **not** reproduce another generated-program crash after the `094_long_code.c` fix, so the runtime-RE hunt remains open but currently green on yet another nontrivial external slice. The next useful chunk should keep rotating to different heavy families again instead of returning to one already-green category by habit. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk widened from rotated spot checks back into one larger whole-suite revalidation and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/minic-test-cases-2021s/functional`, which came back fully green (`112/112 PASS`, including the `reserved/` tail plus the previously relevant `094_long_array.c`, `095_long_code.c`, `096_many_param_call.c`, `097_many_global_var.c`, `098_many_local_var.c`, and `099_register_realloc.c`). In the same chunk I also rotated two smaller neighboring runtime-heavy slices: `python3 tools/sweep_sysy_suite.py --filter global third_party/sysy-suites/lava-test/cases` stayed green (`1/1 PASS` for `111_many_globals.sy`), and `python3 tools/sweep_sysy_suite.py --filter matrix third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` stayed green (`4/4 PASS`). Current conclusion is therefore stronger than the recent spot-check-only chunks: after the `094_long_code.c` closure, one more entire external functional suite and several additional rotated heavy slices stayed green, so the remaining runtime-RE hunt is now less about “did the immediately adjacent family regress?” and more about continuing to search for a still-unseen hidden/runtime corner outside these already revalidated public surfaces. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk widened from “one more full suite” into “multiple whole-surface/public-heavy revalidations” and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020`, which came back fully green (`111/111 PASS`), and the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/TrivialCompiler/custom_test`, which also came back fully green (`29/29 PASS`). In the same chunk I rotated a neighboring short-circuit runtime-heavy slice on `lava-test/cases` (`python3 tools/sweep_sysy_suite.py --filter short ...` => `3/3 PASS`). Current conclusion is stronger again: after the `094_long_code.c` closure, several separate whole external functional surfaces now stay green end-to-end rather than only handpicked subsets, so the remaining runtime-RE hunt is increasingly about looking for a still-unseen hidden/runtime corner beyond these public suites rather than about repairing a regression already visible in common external trees. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk widened that whole-surface revalidation yet again and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/indigo/test_codes/functional_test`, which came back fully green (`111/111 PASS`), and the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/lava-test/cases`, which also came back fully green (`162/162 PASS`). In the same chunk I had already rotated the smaller `global` / `matrix` slices before those full-suite reruns; they stayed green too, so the larger reruns materially subsume and strengthen that evidence. Current conclusion is therefore stronger again: after the `094_long_code.c` closure, several more entire public external functional surfaces now stay green end-to-end, which pushes the remaining runtime-RE hunt even further away from “common public functional regressions” and more toward “find the hidden/runtime corner that these public suites still do not cover.” No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk pushed that “whole public surface” line one step farther and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/sysy-testsuit-collection/lvX`, which came back `464/467` under the default per-case budget, but the exact three red points were all timeout-class and matched the already-known heavy-case pattern rather than a fresh runtime crash: `many_parameters10000.c` (`COMPILE_TIMEOUT`), `matrix-1.c` (`RUN_TIMEOUT`), and `register_alloc10000.c` (`COMPILE_TIMEOUT`). I immediately reclassified that surface with the wider budget already tracked in thread memory by rerunning each of those names at `--case-timeout 60`; all of them came back green (`many_parameters10000.c 1/1 PASS`, `matrix-1.c + matrix-1_2.c 2/2 PASS`, `register_alloc10000.c 1/1 PASS`). In the same chunk I also had the full `compiler2021/function_test2021` green (`103/103 PASS`) from the still-running parallel full-suite revalidation. Current conclusion is therefore stronger again: after the `094_long_code.c` closure, even the broader `lvX` surface is now behaving like “known timeout budget artifacts, not fresh runtime crashes,” and the remaining runtime-RE hunt is increasingly about finding a hidden corner beyond what these now-revalidated public whole suites cover. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk widened those “whole public surface” reruns again and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/compiler2021/公开用例与运行时库/2021初赛所有用例/h_functional`, which came back fully green (`37/37 PASS`). I also ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/lava-test/lava_test`, which came back `19/21` under the default per-case budget, but the only two red points were the already-familiar timeout-class heavy cases `many_parameters10000.sy` and `register_alloc10000.sy`, not a new runtime crash. I immediately reclassified those with the wider budget already used elsewhere in this thread: `python3 tools/sweep_sysy_suite.py --case-timeout 60 --filter many_parameters10000 ...` came back green (`1/1 PASS`) and the same `--case-timeout 60` rerun for `register_alloc10000` also came back green (`1/1 PASS`). Current conclusion is therefore stronger again: after the `094_long_code.c` closure, yet another heavy public surface and another whole runtime-adjacent suite now reduce to “known timeout budget artifacts, not fresh REs,” so the remaining runtime-RE hunt is increasingly about searching beyond the currently available public-suite coverage rather than fixing a public regression that is already in front of us. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk pushed the same line one step farther and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `python3 tools/sweep_sysy_suite.py third_party/sysy-suites/compiler2021/公开用例与运行时库/2021初赛所有用例/functional`, which came back fully green (`103/103 PASS`). In parallel I also probed `third_party/sysy-suites/minic-test-cases-2021f/performance`; the full suite came back with exactly one red point at the default budget (`19_brainfuck-calculator.c: RUN_TIMEOUT`), but that too classified cleanly as a heavy-case budget artifact rather than a new runtime crash because a focused rerun at `--case-timeout 60` came back green (`1/1 PASS`). Current conclusion is therefore stronger again: after the `094_long_code.c` closure, yet another whole public functional surface is fully green, and another performance-side outlier collapses under the wider timeout budget instead of exposing a fresh RE, so the remaining runtime-RE hunt is now even more concentrated into whatever hidden/runtime corner still sits outside these already-revalidated public surfaces. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk deliberately pushed deeper into the performance-side classification line and still ended as a **no-new-bug continuation** rather than another patch. I ran the full `third_party/sysy-suites/minic-test-cases-2021s/performance` surface (`18/18 PASS`) and then both heavier 29-case performance trees: `third_party/sysy-suites/compiler2021/公开用例与运行时库/performance_test2021-private` came back `18/29` with timeout/compile-time pressure on `brainfuck-mandelbrot-nerf`, `dead-code-elimination-2/3`, `floyd-2`, `hoist-2/3`, `instruction-combining-1/2/3`, and `integer-divide-optimization-2/3`; `third_party/sysy-suites/lava-test/performance_test2021` came back `19/29` with the same runtime-timeout cluster except that `instruction-combining-1.sy` already stayed green there. I then immediately reclassified part of that surface with the wider budget already used elsewhere in this thread: `instruction-combining-1.sy` in the private tree cleared at `--case-timeout 60` (`1/1 PASS`), while `hoist-2.sy` in the private tree still timed out even at `--case-timeout 60` (`1/1 FAIL RUN_TIMEOUT`). Current conclusion is therefore sharper again: these heavier performance suites are still not giving me a fresh runtime-RE/crash family, but they do reconfirm that the true still-open public/perf pressure is concentrated in the older timeout witnesses (`03_sort2`, `shuffle2`, `hoist-2`, plus adjacent heavy optimizer/perf families), not in a newly reproduced generated-program crash. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk stayed on that same performance-classification line and again ended as a **no-new-bug continuation** rather than another patch. I used the wider `--case-timeout 120` budget to separate “true still-open perf tails” from “just too heavy for the default budget” on the remaining high-value public/private witnesses. On the public side, both `03_sort2.sy` and `shuffle2.sy` still timed out even at `120s`, so they remain true open perf witnesses rather than mere default-budget artifacts. On the private side, `hoist-2.sy` also still timed out even at `120s`, while `brainfuck-mandelbrot-nerf.sy` cleared under the same widened budget (`1/1 PASS`). Current conclusion is therefore sharper again: after the current runtime-RE closures, the remaining open public/private pressure is increasingly a small set of real performance tails (`03_sort2`, `shuffle2`, `hoist-2`, and nearby heavy optimizer cases), not a newly reproduced generated-program crash family. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk kept pushing that same “classify the remaining public/private perf surface” line and again ended as a **no-new-bug continuation** rather than another patch. I ran the full `third_party/sysy-suites/compiler2021/公开用例与运行时库/performance_test2021-private` and `third_party/sysy-suites/lava-test/performance_test2021` suites end-to-end instead of only targeted cases. The private tree came back `18/29` and the lava perf tree `19/29`; importantly, both failed almost entirely inside the already-known timeout/perf cluster rather than on a fresh runtime-crash family. The shared timeout set still centers on `dead-code-elimination-2/3`, `floyd-2`, `hoist-2/3`, `instruction-combining-2/3`, and `integer-divide-optimization-2/3`, while the private tree alone also showed `instruction-combining-1.sy` as a default-budget compile-time timeout. I immediately reclassified that one too: `instruction-combining-1.sy` clears at `--case-timeout 60` (`1/1 PASS`), so it joins the “budget artifact” side rather than the “true still-open pressure” side. Current conclusion is therefore even sharper: after the current runtime-RE fixes, the remaining red area in the heavier public/private performance suites is continuing to behave like the same older perf-pressure cluster, not like a new runtime-crash family. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk stayed on that same perf-classification line and again ended as a **no-new-bug continuation** rather than another patch. I did not reopen any implementation code in this chunk; instead I used the small local stage probes to refresh the still-open-witness classification. `tools/profile_compile_pipeline.py` confirmed that both `03_sort2.sy` and `shuffle2.sy` still compile in only a few milliseconds locally, and the deeper `tools/profile_compiler_stages.c` probe keeps the same story intact: `03_sort2.sy` spends only about `0.003s` in `machine_ir_report` and about `0.009s` total in the full local compile path, `shuffle2.sy` is similarly tiny, while `hoist-2.sy` remains compile-side and is still overwhelmingly dominated by `machine_ir_report` (`~3.128s` out of `~3.242s` total in the current local probe). Current conclusion therefore remains unchanged but more numerically grounded: the remaining open public witnesses are still separating cleanly into “runtime/program performance tails” (`03_sort2`, `shuffle2`) and “compiler compile-time/allocator-side pressure” (`hoist-2`), not into a newly reproduced generated-program runtime crash family. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-10: The next runtime-hunt chunk stayed on that same classification-heavy line and again ended as a **no-new-bug continuation** rather than another patch. I revalidated the full `third_party/sysy-suites/minic-test-cases-2021s/performance` surface (`18/18 PASS`) and then pushed one more public-perf classification step on `third_party/sysy-suites/compiler2021/公开用例与运行时库/performance_test2021-public`. The default sweep there still came back with several timeouts (`23/30` overall), but the new targeted reruns showed that several of those are budget artifacts rather than fresh runtime crashes: `03_sort3.sy` passed at `--case-timeout 60` (`1/1 PASS`), the `conv*` slice passed at `--case-timeout 60` (`3/3 PASS` including the previously red `conv1.sy` and `conv2.sy`), `median2.sy` passed at `--case-timeout 60` (`1/1 PASS`), and `shuffle1.sy` also passed at `--case-timeout 60` (`1/1 PASS`). Current conclusion is therefore sharper: on the current tree, that public performance surface is not proving a new runtime-RE family so much as separating into “known/open heavy perf tails” and “default-budget-only artifacts.” The clearest still-open public pressure signal remains the older `03_sort2.sy` / `shuffle2.sy` pair rather than the newly checked neighbors. No new bug was found in this chunk, and no long-running sweep remained in flight at the end of it.
- 2026-05-06: That remaining environment-completeness caveat is now also materially closed for the current round. `tools/sweep_sysy_suite.py` can now fall back to the environment's native `/opt/lib/native/libsysy.a` to synthesize host-side oracles for `.c`-based suites when a local testcase tree lacks checked-out runtime sources or co-located `.out` files. With that fallback in place, the current rebuilt compiler now revalidates both `pku-minic` functional trees directly in the present filesystem state instead of relying on older notes: `minic-test-cases-2021f/functional` is green again (`100/100`), and `minic-test-cases-2021s/functional` is green again (`112/112`, including the `reserved/` tail). The same sweep harness was then rerun once more across the previously widened `sysy-testsuit-collection/lvX` surface under a `60s` per-case budget, and that second full pass now closes the bookkeeping loop completely: the suite is all-green end-to-end (`467/467`) rather than only "458/467 plus 9 targeted spot-check closures". Current authority is therefore that the post-allocator correctness checkpoint is no longer merely "mostly complete": for the currently available public oracle-bearing suites in this environment, it is materially complete and green.
- 2026-05-06: The next stage after that correctness checkpoint has now at least been reopened enough to have fresh local baseline numbers instead of only inherited old suspicions. The rebuilt CLI still compiles the public allocator-heavy `lava-test/performance_test2021/hoist-2.sy` case quickly enough to stay well inside practical local guardrails (current direct `build/compiler -riscv ... -o ...` timing is about `12.0s`, still in the same post-allocator-improvement band as the earlier `~10.8s` reading), while the distinct public runtime-tail case `compiler2021/performance_test2021-public/03_sort2.sy` still reproduces as `RUN_TIMEOUT` under the current `60s` per-case sweep budget. Current authority is therefore that the project has now genuinely moved past the post-allocator correctness checkpoint and into the later performance-tuning reopen: the best remaining public pressure is no longer "broad functional suites still red", but the narrower compile-time/runtime-performance pair represented by `hoist-2` and `03_sort2`.
- 2026-05-06: One first direct performance-reopen experiment has now been tried, measured, and deliberately backed out. I tested a very conservative default-path Value-SSA reopen around redundant pure-call elimination and then a `memory-value`-style fallback swap for indirect-memory programs, targeting the repeated `getNumPos(...)` structure inside `03_sort2.sy`. The result was not good enough to keep: the `03_sort2` runtime tail remained a `RUN_TIMEOUT`, one variant regressed the `lv9` course line into a `-11` wrong-answer symptom, and the safer variants still pulled the `hoist-2` compile baseline upward into roughly the `15s..17s` range instead of down. Current authority is therefore explicit: that "default-path call-CSE / indirect-memory canonicalization reopen" is now a recorded failed experiment, not an active checkpoint item. The mainline stays on the rolled-back stable state (`lv8` green, `lv9` green, `hoist-2` compile roughly `15s`, `03_sort2` still `RUN_TIMEOUT`), and the next performance move should change angle rather than retrying the same hook with slightly different wiring.
- 2026-05-06: A later backend-only micro-optimization round has now also landed and is worth keeping as execution memory because it produced a real code-quality win without reopening the earlier unsafe Value-SSA directions. The current `machine_bytes` preview path now uses `x0` directly for `store_local_imm 0` / `store_global_imm 0`, lowers small `addr_local` forms to one `addi rd, sp, imm` instead of a `li + add` pair, and lets zero-valued prepared operands flow through the later bytes operand-prep path as `x0` instead of as an extra synthesized scratch register. On top of that, the final compiler-side preview-text exporter now also has one first narrow peephole that folds a one-use `li tmp, 0` followed immediately by `add dst, src, tmp` into a smaller `mv dst, src` / no-op shape instead of preserving the zero helper in the final `.s` text. Direct rebuilt-CLI reruns on `compiler2021/.../03_sort2.sy` therefore now show two concrete checkpoints on the same public hot case: first from roughly `608` total instructions with `li=139` / `add=74` down to roughly `523` with `li=54` / `add=44` / `addi=54`, and then one step further down to roughly `515` with `li=48` / `add=38` / `mv=33` while keeping the rest of the broad hot families stable. The public runtime tail itself is still open, however: the same single-case sweep under `tools/sweep_sysy_suite.py` still reproduces `03_sort2.sy: RUN_TIMEOUT` at the current `60s` per-case budget. Current authority is therefore to keep these late backend and final-text wins, but treat the remaining mainline as the narrower unresolved indexed-address / repeated-load loop family rather than as the already-closed zero-store materialization family.
- 2026-05-06: One more final-text peephole has now also landed on top of that same safe late-export line, and this one hits a visibly hotter pattern in `03_sort2.sy` than the earlier zero-add cleanup. The preview text exporter now folds a direct `li tmp, 4` followed immediately by `mul dst, src, tmp` into `slli dst, src, 2` instead of preserving the helper-register multiply in the final `.s` text. Focused reruns keep `make test-compiler-driver` green, so this still sits on the safe final-text side rather than reopening mid-pipeline semantics. On the public hot repro itself, the direct rebuilt-CLI histogram now drops one more step from the earlier `~515` band to roughly `total_instructions=482`, with `li=15`, `add=38`, and `slli=33` instead of the previous `li=48` / `mul=34` heavy indexed-address tail. The runtime result is still not closed: the same single-case `tools/sweep_sysy_suite.py` rerun continues to report `03_sort2.sy: RUN_TIMEOUT` at the current `60s` budget. Current authority is therefore that the late text/export side is still yielding real wins on the hot indexed-address family, but we are now clearly in the diminishing-returns part of that line and should treat the remaining pressure as the still-open repeated-load / control-structure runtime tail rather than as another easy one-line peephole opportunity.
- 2026-05-06: A follow-up final-text experiment on the same hot path is now also landed and measured, and it further sharpens what is and is not enough to close `03_sort2`. The preview text exporter now also rewrites direct `div/rem` by a power-of-two immediate into exact signed shift/mask/sub sequences instead of preserving the helper-register division/modulus form in the final `.s` text. On the representative hot function `getNumPos`, the emitted `base=16` path is now visibly different: `div/rem ... , 16` is exported as `srai/andi/add/srai` (for division) and `srai/andi/add/srai/slli/sub` (for remainder), while the surrounding global-const load is still folded to an immediate. This remains regression-safe on the public surface (`make test-compiler-driver` stays green). The instruction-count tradeoff is mixed rather than uniformly smaller: the full `03_sort2.sy` histogram moves from the earlier `~482` checkpoint to roughly `total_instructions=488`, with `srai` / `andi` replacing the explicit `div/rem` opcodes. More importantly, the runtime tail is still not closed: the same single-case `tools/sweep_sysy_suite.py` rerun continues to report `03_sort2.sy: RUN_TIMEOUT` at the current `60s` budget. Current authority is therefore that this power-of-two divide/mod rewrite is a safe backend/export improvement worth keeping, but it does not materially change the remaining mainline diagnosis: the unresolved pressure is still the giant-input repeated-call / repeated-load / repeated-control runtime structure itself, not one last missing arithmetic peephole.
- 2026-05-06: That same `div/rem by power-of-two` final-text rewrite was then immediately stress-checked against the real course array/sort line, and this follow-up matters because it changed the final decision on whether that experiment should stay. Focused reruns exposed real `lv9` regressions (`19_sort5`, `21_sort7`), which means the textual rewrite as first implemented was not semantics-preserving enough to keep, even though its local `03_sort2` shape looked attractive. The experiment has now been explicitly rolled back. After rollback, both `make test-compiler-driver` and `autotest -riscv -s lv9 /workspaces/compiler_lab` are green again (`22/22`), and the stable kept `03_sort2` static checkpoint returns to roughly `total_instructions=485` under the remaining safe final-text peephole set (`zero-add`, `mul-by-four`, `add/sub-by-one`, plus the safe constant-global immediate fold that still leaves the benign leading `lui`). Current authority is therefore explicit: do **not** resurrect the power-of-two `div/rem` text rewrite as-is; the current stable mainline should continue from the `~485` checkpoint rather than from the briefly lower-but-wrong intermediate variants.
- 2026-05-06: One more tiny final-text cleanup has now been landed on top of that same export-side line, and this one is deliberately modest rather than being presented as a silver bullet. The preview text exporter now also folds a direct `li tmp, 1` followed immediately by `add/sub dst, src, tmp` into `addi dst, src, +/-1` instead of keeping the helper-register arithmetic in the final `.s` text. This remains regression-safe on the public surface (`make test-compiler-driver` stays green). On `03_sort2.sy`, the instruction histogram now nudges from the previous `~488` checkpoint down to roughly `total_instructions=485`, with `li=12` and `addi=60` while the rest of the hot families remain broadly stable. Current authority is therefore that the export-side peephole tail is still yielding small, real wins, but those wins are now incremental rather than transformative; the remaining runtime pressure is still the large repeated-load / repeated-call structure rather than a single missing one- or two-instruction fold.
- 2026-05-06: The strict `returns-all-paths` reopen now has a clearer implementation map than the earlier "semantic false-positive audit" label implied. I re-read the live chain and revalidated it with focused tests after the recent local-state loop fix: `make test-compiler-driver` is green again, `make test-semantic-regression` is green again, and the current authority is that strict/default all-path behavior is jointly determined by three linked layers rather than one gate in isolation. The front gate remains `semantic_compute_function_returns_all_paths(...)`, but the same user-visible family also depends on control-flow-sensitive canonical-IR global-dependency pruning (`ir_global_dep`) and on local-state-aware loop/if fallthrough shaping in canonical IR lowering (`ir_lower_stmt`). To keep that understanding live in the suite instead of only in notes, IR regression coverage now also locks the local-state-fed loop-return family directly (`int b=1; while(b<2)...`, `for(;b<2;...)...`, and `int b=0; b=1; while(b<2)...`) instead of only through compiler-driver smoke tests. Current authority is therefore to debug future strict-mode regressions by asking "which layer first mis-proved fallthrough/reachability?" rather than by reopening semantic logic blindly.
- 2026-05-06: The next strict/local-state follow-up is now tighter again and closes the last currently reproduced alias-fed nested-loop false positive in the real compile path. The remaining bad shape was no longer semantic rejection, but canonical-IR lowering itself leaving a malformed dead exit path: constant-true outer loops that contained nested loops were still forced to keep an exit block purely because of the nested-loop marker, even when the analyzed body already proved `!may_break && !may_fallthrough` and therefore could not reach the surrounding function tail. `ir_lower_stmt` now treats those two facts separately: nested loops still block overaggressive local-state “condition stays true forever” proofs, but they no longer force a fake exit block when the body already has no path back out. That repair is now regression-locked in `tests/ir/strict_loop_return_alias.cases.inc` and the existing compiler-driver strict smoke, and focused reruns are green again on `make test-ir-regression`, `make test-compiler-driver`, the direct `ir_lower_program(...)` repro, and the rebuilt strict CLI path. Current authority is therefore that the strict audit line has moved past the old alias-fed loop family and back toward hidden/default compatibility hunting as the primary active front-end/mainline risk.
- 2026-05-06: The first correctness follow-up on that live strict/IR map is now also closed again after an initial false start. I had to rework the `ir_lower_stmt` loop-shaping again so `while/for` would only collapse their header into a direct body jump when the whole loop really has no live exit path, not merely because the condition happens to be constant-true at entry. That fixed the earlier `IR-VERIFY-010` unreachable-block regressions on the local-state loop family, and the targeted `test-ir-regression` / `test-compiler-driver` reruns are green again on the repaired tree. The global-initializer side also got a second guard: top-level initializer constant folding now treats out-of-range literals conservatively instead of folding them into a bogus `0` runtime shape, so the `LLONG_MIN`/`+1` initializer regressions stay on the explicit runtime-initializer path instead of silently being erased. Current authority is therefore that the strict/all-path map is still the same three-layer story, but the live IR correctness floor below it is now back to green after those two focused fixes.
- 2026-05-06: The next strict/local-state lowering follow-up is now materially closed too, and the final root cause turned out to be a real contract split rather than one more random condition tweak. I had to separate two notions that had drifted together inside `ir_lower_stmt`: `1)` proof-oriented local-state reasoning for the strict false-positive audit, and `2)` actual canonical-IR CFG shaping for normal branch lowering. The repaired current rule is: real CFG branch emission stays conservative and does **not** constant-fold on mutable-local current values, while the proof helper for “can this loop honestly be treated as no-exit under the current local-state model?” now imports the live lowering scope, iterates body/step effects to a short fixed point, and only then allows `while`/`for` to omit the exit block. Along the way I also fixed two state-propagation bugs inside that helper itself: compound-scope pop had been restoring the wrong flow-state snapshot, and `for` declaration scopes were being double-counted so the helper could follow the stale outer binding instead of the current loop-local one. After those fixes, the previously split symptoms now align again on the same repaired tree: the strict local-state pass family (`while(b<2){ if(b){ return 2; } }`, `for(;b<2;...)`, and the alias-fed outer-loop variant) lowers successfully again, the older non-strict `entry_seed` loop-local case restores its real conditional backedge plus final `ret`, `make test-compiler-driver` is green again, and `make test-ir-regression` is green again. Current authority is therefore that the strict local-state false-positive reopen has advanced one real layer downward: the remaining work is no longer “basic local-state loop lowering still unstable”, but “keep expanding legal-family search now that semantic, dependency, and lowering contracts are back in sync on the known families”.
- 2026-05-06: One more lowering-side reconciliation is now also complete, and it matters because it closes the last obvious mismatch between the new local-state proof helper and the older canonical-IR CFG expectations. The helper originally still mis-modeled `for` loops in two ways: it checked for fixed-point convergence before applying the `for` step, and it could duplicate the same declaration-scoped loop variable when importing existing lowering scope plus the `for` declaration itself. That combination let `for(int a=3; a; a=a-1){}` look falsely no-exit even though the step clearly drives the guard toward zero. The repaired rule is now tighter and simpler: the helper trusts the live lowering scope as the single source of declaration-state truth, applies loop-step effects before fixed-point comparison, and only treats a loop as no-exit if the post-step state actually converges with the guard still provably true and with no reachable `break`. After that cleanup, the previously red `IR-FOR-INIT-STEP` regression is green again, `make test-ir-regression` is green again, `make test-compiler-driver` is green again, `entry_seed` still emits its real conditional backedge plus final `ret`, and `git diff --check` is clean. Current authority is therefore that the local-state proof line is no longer in “active firefight” mode: the next useful move is to resume broad legal-family hunting under strict mode rather than continuing to reopen the same known local-state lowering mechanics.
- 2026-05-06: The next strict-hunting round has now actually broadened beyond repository-owned regressions instead of only promising to do so. I ran an external strict compile sweep across locally available third-party suites (`compiler2021`, both `minic-test-cases-2021*` trees, `indigo`, `TrivialCompiler`, `lava-test`, and `sysy-testsuit-collection`) and filtered for the control-flow/return-heavy filename cluster (`while`, `if`, `break`, `continue`, `short`, `loop`, `return`). That produced `198` externally sourced strict-mode compile probes, and the current rebuilt compiler passed all `198/198` under `--enforce-all-paths-return-check` with no reproduced `IR-LOWER-009`, `LOWER-IR-042`, or semantic strict false-positive surface. Current authority is therefore that the strict audit has now moved out of the “only in-repo cases are green” phase: both the repository’s known accept families and a first meaningful slice of third-party hidden-neighbor control-flow cases are aligned with the real compile pipeline. The remaining strict work should now focus on either broader non-keyword suite sweeps or genuinely new hidden-style families, not on re-litigating the already-green local-state loop cluster.
- 2026-05-06: The mainline has now intentionally pivoted back to hidden/default compatibility hunting, and this round produced one concrete runtime-family reopen instead of only more “everything still compiles” evidence. A wider default-mode third-party compile sweep over `214` control-flow-adjacent filenames (`while`, `if`, `nested`, `long`, `short`, `array`, `global`, `init`, `call`) came back `214/214` green at compile time, and a follow-up compile+assemble sweep over the higher-risk `array/global/const/init/short` cluster also came back `147/147` green through assembly. That means the current hidden/default tail is increasingly *not* about easy compile-time `CTE/AE` reproduction. However, the first sampled compile+assemble+link+run sweep over `80` high-risk array/global/short-circuit cases did reproduce a real runtime cluster: several `array_traverse*` cases timed out under a conservative `10s` budget, and more importantly `lava-test/cases/104_long_array.sy` plus `minic-test-cases-2021f/functional/092_long_array.c` both crash under `qemu-riscv32-static` with `returncode=-11` / `EXIT=139`. I then minimized that direction enough to know where to continue next: the crash is reproducible locally through the normal CLI path, and the bad shape is already visible in canonical IR / lowering rather than only in final machine bytes. In the current bad dump, the outer `while(i < N)` in `long_array` still loses its exit/return path after passing through nested loops, so this is now a concrete “large local-array / nested-loop / long-frame lowering runtime” reopen, not a vague hidden suspicion. Current authority is therefore to treat `long_array`-class runtime recovery as the next active default-mode task, with likely spillover benefit to the earlier hidden `RE` cluster around large arrays / globals / access paths once that line is understood and fixed.
- 2026-05-10: The next ordered reread/analysis chunk stepped back from broad suite-running into the still-open optimization source itself and ended as a **no-new-bug continuation** rather than another patch. I re-read `src/machine/lowering/machine_select/machine_select_cleanup.inc` around `machine_select_cleanup_reuse_internal_pure_calls_in_block(...)` and cross-checked it against a fresh `machine_select` dump of the still-open `03_sort2.sy` and `shuffle2.sy` public witnesses using `tools/dump_machine_stage.c` plus the local analysis helpers. Current concrete finding is not a correctness bug but a plausible still-open optimization boundary: in `03_sort2`'s `radixSort` hot block, one repeated `getNumPos(...)` call still survives because `machine_select_describe_call_arg_source_in_block(...)` only canonicalizes arguments rooted in `load_local`, `load_global`, `addr_local`, `addr_global`, copies/immediates, or plain live-in operands. It does **not** yet describe same-block arguments rooted in `load_indirect(addr)` as a reusable abstract source, so two equal pure calls whose repeated argument is itself a repeated indirect load currently fail the equality proof and remain materialized. That is logged here as execution-memory for the still-open perf tail, not as a landed code change: no new correctness bug was proven, and no source patch was kept in this chunk. Current authority is therefore that the remaining public runtime/perf tails may still have one more optimization avenue on the `machine_select` same-block pure-call line, but the present evidence does **not** justify calling it a runtime-RE root cause. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next continuation chunk stayed on that same source-analysis line and again ended as a **no-new-bug continuation** rather than another patch. I kept the focus on the still-open `03_sort2` / `shuffle2` line, spot-checked the companion load-reuse analyzer against the current `machine_select` dumps, and then rotated one more small external correctness witness on `compiler2021/function_test2020` filtered to the `index*` family (`40_index_complex_expr.sy`, `41_index_arithmetic_expr.sy`, `42_index_func_ret.sy`), which stayed green (`3/3 PASS`). The concrete outcome remains conservative: I did not prove a new runtime-RE bug, and the local analysis surface continues to suggest “optimization boundary” more than “correctness failure” on the surviving `machine_select` repeated-call/repeated-load tails. Current authority is therefore unchanged: the remaining `03_sort2` / `shuffle2` public witnesses still look like open perf/optimization pressure, not a newly reproduced crash family. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next chunk stayed on that same `machine_select` optimization-boundary line and this time **did** land one narrowly-scoped performance fix, though it still did not expose a new runtime-RE bug family. I extended `machine_select_describe_call_arg_source_in_block(...)` so the same-block internal pure-call reuse path can now classify an argument whose source is a same-block `load_indirect(addr)` by recording the underlying address operand, then compare and invalidate that source conservatively through the existing scan window. A focused new regression in `tests/machine/lowering/machine_select/machine_select_test.c` now locks the exact shape: two equal pure internal calls whose first arg is repeatedly loaded from the same `spill.0 = addr_local local.1` indirect address in one block must collapse to one `call_spill`, while the later dead duplicate call disappears. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external correctness sweep `python3 tools/sweep_sysy_suite.py --filter index third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` PASS (`3/3`). A fresh `machine_select` dump of `03_sort2.sy` now no longer reports the earlier repeated `getNumPos(local:1, local:0)` signature in `radixSort`, which confirms the new same-block pure-call reuse reaches that hot block. The public perf witness itself is **still** open (`03_sort2.sy -> RUN_TIMEOUT` under the current budget), so current authority is to treat this as one more small structural perf win on the open tail, not as a runtime-RE closure claim. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next continuation chunk stayed on that same `machine_select` source-analysis line and ended as a **no-new-bug continuation** rather than another patch. I re-read the neighboring indirect-load / address-root cleanup helpers in `src/machine/lowering/machine_select/machine_select_cleanup.inc`, cross-checked the current `03_sort2.sy` and `shuffle2.sy` `machine_select` dumps with the companion address/load analyzers, and then rotated one more small external correctness witness on `compiler2021/function_test2020` filtered to the `index*` family (`40_index_complex_expr.sy`, `41_index_arithmetic_expr.sy`, `42_index_func_ret.sy`), which stayed green (`3/3 PASS`). Current concrete finding is still conservative: I did **not** prove a new runtime-RE bug, and the local analysis surface still looks more like “further optimization boundary” than “correctness failure” on the surviving `machine_select` repeated-load/repeated-address tails. In particular, the current analyzers did not expose one new trivially reusable repeated indirect-load root in these two witnesses after the just-landed pure-call reuse extension, so no additional same-block indirect-load forwarding patch was justified in this chunk. Current authority is therefore unchanged: the remaining `03_sort2` / `shuffle2` public witnesses still look like open perf/optimization pressure, not a newly reproduced crash family. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next continuation chunk stayed on that same `machine_select` optimization-boundary line and again ended as a **no-new-bug continuation** rather than another patch. I explicitly re-read the still-declared-but-unimplemented `machine_select_cleanup_forward_same_block_indirect_loads_program(...)` neighborhood and the surrounding address-root helpers in `src/machine/lowering/machine_select/machine_select_cleanup.inc`, then rechecked the current `03_sort2.sy` / `shuffle2.sy` dumps with the companion indirect-load/address analyzers. Current concrete finding is still conservative: I did **not** prove a new runtime-RE bug, and the current local evidence still does not justify landing a fresh same-block indirect-load forwarding pass yet. The remaining hot patterns after the just-landed pure-call improvement still look more like “possible future optimization work needing a tighter alias/invalidaton proof” than a correctness failure or an obvious one-line reuse fold. The rotated external correctness witness `python3 tools/sweep_sysy_suite.py --filter index third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2020` stayed green (`3/3 PASS`) while doing this check. Current authority is therefore unchanged: the surviving `03_sort2` / `shuffle2` public witnesses still read as open perf/optimization pressure, not a newly reproduced crash family, and no long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next chunk stayed on that same `machine_select` optimization-boundary line and this time **did** land one more narrowly-scoped performance fix, again without exposing a new runtime-RE bug family. I implemented the previously only-declared same-block indirect-load forwarding cleanup so it now reuses a repeated `load_indirect(addr)` when the address expression is proven equal inside one block and any intervening memory writes are either absent or proven non-alias by the existing address-root helper (`machine_select_cleanup_store_cannot_alias_loaded_address(...)`). A focused regression in `tests/machine/lowering/machine_select/machine_select_test.c` now locks the exact safe shape: a second `load_indirect` from the same `spill.0 = addr_local local.0` address may be forwarded across an intervening `store_local local.1, ...`, collapsing to the earlier spill-backed load result. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, and `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`). A fresh `machine_select` dump of `03_sort2.sy` still shows no repeated pure-call signature, and the final `.s` quick histogram now also reflects one more small structural trim on that open tail (`call` count down to `11`). The public perf witness itself is **still** open (`03_sort2.sy -> RUN_TIMEOUT` under the current budget), so current authority is to treat this as another small structural perf win on the open tail, not as a runtime-RE closure claim. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: A follow-up keep/revalidate chunk has now closed the decision loop for that same-block indirect-load forwarding patch, and the conclusion is to **keep it** rather than roll it back. The practical reason is that the patch stays correctness-green across the expected post-edit surfaces while still matching the narrowed public-tail story instead of widening it: `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated external correctness slice `python3 tools/sweep_sysy_suite.py --limit 30 third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`30/30`). I also rebuilt a fresh `machine_select` dump and final `.s` witness for `03_sort2.sy`; the current dump still has no repeated pure-call signature under `tools/analyze_machine_select_repeated_calls.py`, and the rebuilt final text still sits at `call=11` on that hot case. Focused public perf reruns at `--case-timeout 120` keep the same two open witnesses and no more (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), which keeps the current interpretation stable: this patch is a small structural perf win on the open timeout tail, not evidence of a new runtime-RE family and not a full public-tail closure. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next ordered machine-select reread/repair chunk found one more safe optimization boundary and kept it. After re-reading the neighboring selected-control / cleanup code with the still-open public timeout tail in mind, I noticed one missing reuse family: a repeated cacheable-pure internal call could still survive when it reappeared not in the same block, but immediately in a **unique-predecessor successor block** with the exact same raw arg operands carried live across the edge. The live `03_sort2.sy` `radixSort bb.16 -> bb.17` path had exactly that shape (`getNumPos(spill.6, spill.7)` was recomputed in the successor after the predecessor had already produced the same pure result in `reg.0`). The kept fix adds a narrow cross-block cleanup that reuses that predecessor result only when the successor has one predecessor, the callee is already inside the existing cacheable-pure summary, the raw arg operands match exactly, and neither the args nor the result are invalidated by intervening calls/clobbers/global-write conflicts along the edge prefix. A focused regression (`test_machine_select_reuses_repeated_internal_pure_call_from_unique_predecessor`) now locks the branch-edge shape. Kept rechecks after this closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), and the rotated third-party slice `python3 tools/sweep_sysy_suite.py --limit 25 third_party/sysy-suites/indigo/test_codes/functional_test` PASS (`25/25`). A fresh selected dump confirms the redundant `bb.17` call disappears, and the rebuilt final `.s` quick histogram for `03_sort2.sy` now shows `call=10` instead of the earlier `11`. Focused public perf reruns at `--case-timeout 120` still keep the same two open witnesses (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), so current authority is to treat this as another small structural perf win on the open timeout tail, not as a runtime-RE closure claim. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: A later same-day stability-gate round was run specifically to answer whether the current whole worktree is stable enough to checkpoint/commit. The broader kept validation surface is now: `make test` PASS, full course `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`), third-party full `compiler2021/公开用例与运行时库/function_test2021` PASS (`103/103`), third-party full `indigo/test_codes/functional_test` PASS (`111/111`), and third-party full `minic-test-cases-2021f/functional` PASS (`100/100`). One intermediate parallel sweep attempt did hit a temporary environment-side `PermissionError: /workspaces/compiler_lab/build/compiler` while `build/compiler` was being rebuilt concurrently by other regression commands; that was not treated as a compiler regression, and clean reruns after the build phase finished restored the full external greens above. Focused known-witness reruns still keep exactly the same open timeout pair at `--case-timeout 120` (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT`), with no newly widened correctness surface. Current authority is therefore that the present tree is stable enough for a checkpoint commit if the user wants to preserve the current line, while still honestly carrying those same older performance/open-timeout witnesses rather than claiming a fully all-green perf surface. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: A follow-up stability pass addressed two user reminders explicitly: run the full `sysy-testsuit-collection/lvX` sweep (the live tree contains **467** cases in that suite, not 476) and clear the currently visible compile warnings. The warning cleanup is now done: the temporary unused `machine_select` helper was removed, the intentionally unused `error` parameter in the same cleanup path is now explicitly silenced, and the dead `decode_riscv_j_imm(...)` helper was removed from `tests/machine/object/machine_bytes/machine_bytes_test.c`; fresh `make test` / `autotest -riscv` logs no longer report those warnings. The full `lvX` rerun came back `464/467`, with exactly the same three heavy-case reds already remembered in thread memory rather than a widened correctness regression: `many_parameters10000.c -> COMPILE_TIMEOUT`, `matrix-1.c -> RUN_TIMEOUT`, and `register_alloc10000.c -> COMPILE_TIMEOUT`. Revalidation after the warning cleanup also kept the broader stable surfaces green again: `make test` PASS, full course `autotest -riscv /workspaces/compiler_lab` PASS (`130/130`), full `compiler2021/function_test2021` PASS (`103/103`), full `indigo/test_codes/functional_test` PASS (`111/111`), and the earlier full `minic-test-cases-2021f/functional` PASS (`100/100`). Current authority therefore stays the same but is now better locked down: the current tree remains stable on the large correctness surfaces, the known heavy-case timeout/artifact set did not widen, and the visible compile-warning surface is back to clean.
- 2026-05-10: After the user reconfirmed that the hidden `RE` still looks like generated-program **segmentation fault**, the active line was explicitly narrowed again away from public timeout classification and back onto direct segfault-capable wrong-code families. A new compact continuation file, `docs/RUNTIME_RE_SEGFAULT_PLAN.md`, is now the active small-plan authority for that line, and one first concrete closure has already come out of the renewed `machine_select_cleanup.inc` reread. The same-block indirect-load forwarding cleanup had still been proving some sibling local stores non-alias too aggressively when the loaded address came from an offset local root such as `addr_local local.0 + 4`: the address-root helper kept only the base local slot id and lost the offset, so a repeated `load_indirect(addr_local local.0 + 4)` could be forwarded across `store_local local.1, ...` even though those two names can denote the same contiguous local-slot-family element. That is exactly the sort of stale indirect-memory value that can later become a bad pointer/index and a runtime segfault instead of only WA. The live fix now distinguishes exact slot addresses from offset-derived addresses and refuses the “different local id => non-alias” shortcut once local-root arithmetic has happened. Focused regression coverage is now locked in `test_machine_select_does_not_forward_indirect_load_across_aliasing_sibling_local_store`, and kept rechecks after the closure are `make test-machine-select` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), plus a rotated third-party array slice `python3 tools/sweep_sysy_suite.py --filter array third_party/sysy-suites/compiler2021/公开用例与运行时库/function_test2021` PASS (`3/3`). No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The user also suggested an explicit stress-generation tactic for hidden runtime segfaults, and that is now part of the active line instead of just an idea. A new helper, `tools/generate_sysy_runtime_re_stress.py`, now emits large SysY programs with deep nested call chains, many-argument calls, and mixed global/local array read-write traffic, plus matching `.out` files so the generated case can go straight through `tools/sweep_sysy_suite.py`. The first smoke runs are green so far rather than immediately reproducing the hidden crash: one generated case at roughly `1.5k` lines passes, and one larger generated case at roughly `10k` lines also passes under `--case-timeout 60`. Current authority is therefore: keep using this generator as a widening tactic for segfault-family repro search, but do not mistake the initial green results for evidence that the hidden runtime `RE` is closed.
- 2026-05-10: The stress generator has now been pushed farther toward the exact shapes the user asked for: more nested helper families, more branch diversity, more local/global array mixing, more constants, and more cross-calling between branchy kernels and call-heavy wrappers. The more varied generator still has not reproduced the hidden runtime segfault on the first successful passes: a new ~2.2k-line “more varied” case passes cleanly, and the older generated bundle still stays green. One oversized branch-heavy variant did hit the separate compile-stage backend limit `MACHINE-BYTES-344: riscv preview branch target out of range`, which is useful as a reminder that blindly increasing control-flow size can hit older branch-range limitations before reaching runtime, but it is not evidence that the hidden segfault itself is closed or newly reproduced. Current authority is therefore to keep biasing generated stress cases toward address/indirect-memory/call interactions while keeping individual function/control regions just below the known far-branch cliff when the goal is runtime-segfault hunting rather than compile-failure hunting.
- 2026-05-10: The user then raised a sharper hypothesis: hidden runtime `RE` may be caused by **short-circuit plus function-call side effects**, especially when the short-circuited or nested call mutates globals. I checked both the code and the first focused repro surface. Code-side, the obvious AST->IR lowering contract still looks aligned: call arguments are lowered left-to-right, and logical `&&` / `||` branch lowering still descends the left operand before the right operand. Repro-side, the first deterministic Lv8 hidden-like bundle aimed at that family is currently green: simple short-circuit/global-side-effect cases pass, and deterministic nested-single-argument / recursive side-effect cases also pass. Current authority is therefore not “side-effect bugs are impossible”, but “the first trivial front-end short-circuit/order hypothesis has not reproduced; if the hidden segfault lives in this family, it is more likely a selective later-layer interaction such as late call-argument rewriting, caller-save misuse, or deeper global-state-sensitive call nesting.”
- 2026-05-10: I then widened that same Lv8-side-effect line with the extra `compiler2023` suite the user suggested. The most relevant targeted public/hidden-like cases are still green locally: `50_short_circuit.sy`, `51_short_circuit3.sy`, `72_hanoi.sy`, `89_many_globals.sy`, `93_nested_calls.sy`, plus hidden-like `28_side_effect2.sy` and `32_many_params3.sy` all PASS. A small `hidden_functional` prefix sweep (`12` cases) reopened only `09_BFS.sy` and `10_DFS.sy` as runtime timeouts, not a new segfault-style Lv8 repro. Current authority therefore shifts slightly but not dramatically: this extra suite strengthens the claim that the most obvious Lv8 short-circuit/global-call/recursive-call shapes are green on the current tree, while leaving the active hidden runtime-RE line still pointed at a more selective later-layer bug rather than a broad front-end side-effect-order failure.
- 2026-05-11: I continued directly from the latest `machine_ir` kept closure instead of branching into a new family too early. First, the new regression for the sibling unsafe pattern is now actually wired into the live `machine_ir` test mainline: `test_machine_ir_cleanup_keeps_nonempty_known_slot_branch_tail_local_store` is now called from `tests/machine/lowering/machine_ir/machine_ir_test.c`, so the suite no longer leaves that check as an unused static helper. Rebuild check: `make build/machine_ir/machine_ir_test` PASS. Local validation note remains the same as before: a broad `make test-machine-ir` rerun still stops earlier on the already-known allocate+rewrite expectation-noise case (`spill.3/spill.4` dump shape), so that target is still **not** a clean checkpoint signal for this runtime-RE line yet. Second, I did one more explicit downstream reread chunk on the ordered runtime-risk spine—`machine_select_lower_control.inc`, `machine_select_cleanup.inc`, `machine_bytes.c`, and `compiler_driver.c`—and did **not** find a fresh concrete generated-program runtime-RE bug in that span yet. Third, I rotated runtime-facing rechecks instead of reusing the same tiny comfort list: `/tmp/recheck_runtime_re_globaltail_rotA` (`25/25 PASS`), `/tmp/recheck_runtime_re_cmpbranch_rotA` (`16/16 PASS`), and a direct cased rerun keeping the main repaired witness alive (`case_001.c` still PASS). A symlinked c-only `noarray_hidden2` subset was only partially useful because many entries in that ad-hoc subset lacked standalone oracle context and therefore SKIPped, so it should be treated as harness noise rather than evidence of a new regression. Net result of this chunk: the latest kept `machine_ir` fix still looks stable on rotated runtime-facing surfaces, the new branch-tail sibling regression is now live in the test mainline, and the audit boundary has advanced through the next downstream source span without producing a fresh localized red point. No long-running sweep remains in flight at the end of this chunk.
- 2026-05-11: I then pushed one more direct runtime-RE hunt round instead of stopping at that reread-only checkpoint. First I re-audited the sibling always-on `machine_ir` canonicalization folds in `src/machine/lowering/machine_ir/machine_ir_cleanup.inc` (inlineable/call/thin wrapper tails, same-single-instruction branch-successor collapse, equivalent inlineable successor collapse, linear jump-block merge) under the same “non-empty predecessor / side-effect / generated-program segfault” lens. Current conclusion for this reread chunk is still conservative: I did **not** prove a fresh concrete wrong-code bug there yet. Second I widened the runtime-facing witness surface again with two new no-array/global-call families rather than reusing the same old comfort cases:
  - `/tmp/runtime_re_noarray_callmix_batch` (`40/40 PASS`), targeting `scalar globals + recursion + nested calls + short-circuit + branch gating + constant-heavy locals`
  - `/tmp/runtime_re_func_exprish_batch` (`80/80 PASS`), targeting a denser `func_expr2/global_var2`-adjacent family with repeated global mutations, nested call expressions, ternary-ish call selection, and later global reloads
  Net result of this chunk: the front-most kept closure is still the `machine_ir_slot_cleanup` known-slot-tail family; the sibling generic `machine_ir_cleanup` folds are now reread without a newly proven red point; and one more rotated no-array/runtime-facing widening round stayed fully green instead of exposing the next hidden RE witness. No long-running sweep remains in flight at the end of this chunk.
- 2026-05-11: One more continuation after that exposed an important **pipeline-boundary fact** for the still-open hidden runtime-RE line. I traced the current default/conservative compiler path end-to-end through `compiler_compile_source_text_with_options(...)` and the current `machine_ir` / `machine_select` builders. The live default build (with `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS` unset) currently does:
  - plain `value_ssa_build_from_lower_ir(...)`
  - **no** `value_ssa_optimize_perf_hotspots(...)`
  - `machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(...)`
  - `machine_select` initial lowering + verifier only, with later cleanup stack skipped
  - no final preview-text peepholes in `compiler_driver`
  This means the recently fixed `machine_ir` canonicalization bug in `machine_ir_slot_cleanup.inc` is still a real kept closure for the aggressive path / related consumers, but it is **not** an always-on explanation for the current conservative default pipeline by itself. I then immediately widened on the next likely always-on hidden-only family instead of only recording that fact: a fresh host-oracle-backed batch `/tmp/runtime_re_globalobjmix_batch` targeted `mixed initialized/uninitialized global arrays + multi-global declaration style + helper calls + 1d/2d array traffic + later global reloads`, and it stayed fully green (`60/60 PASS`). Current authority after this chunk is therefore sharper than before: the still-open hidden runtime-RE root cause is increasingly likely to live in an **always-on** surface (`machine_ir` lower without canonicalize, raw `machine_select` lower, `machine_bytes`, preview-text emission, or global/runtime layout handoff), not only in the aggressive cleanup/peephole layers that were already fixed. No long-running sweep remains in flight at the end of this chunk.
- 2026-05-11: I kept pushing on those always-on surfaces instead of treating the pipeline-boundary clarification as enough. Two more fresh rotated families stayed green:
  - `/tmp/runtime_re_runtimeinit_batch` (`40/40 PASS`), explicitly targeting the still-always-on **runtime global initializer / startup helper** line with declaration-order-dependent scalar globals, 1d/2d global arrays initialized through helper calls/branches, and a final main-path readback
  - `/tmp/runtime_re_indirect_globalmix_batch` (`50/50 PASS`), targeting **always-on indirect/global-array traffic** with array-parameter helpers, repeated indexed reads/writes, helper calls, and later global reloads
  Current authority after this chunk is therefore narrower again: the obvious runtime-global-init/startup-helper family and one more indirect global-array/address family are both green on the live conservative path, so the remaining hidden runtime-RE root cause should be sought even more specifically inside the still-always-on downstream lowering/bytes/preview-emission handoff rather than in these now-expanded green families. No long-running sweep remains in flight at the end of this chunk.
- 2026-05-11: One more follow-up on that same line clarified the current default-path boundary a little more precisely and widened one more conditional-canonicalization family. Code-side, the earlier “default path skips machine-ir canonicalization” summary needed one nuance: `compiler_emit_riscv_preview_text_from_report(...)` lowers through `machine_select_build_program_from_machine_ir_report(...)`, and that helper **does** canonicalize the whole machine-ir program first when any function in the report still carries phi nodes; only fully phi-free programs stay on the raw machine-ir -> machine_select path. I then reread the always-on / conditionally-always-on global/data/address emit surfaces (`machine_select_lower_memory.inc`, `machine_bytes` global load/store/addr emission, and the matching compiler preview global/load/store/address pretty-printer path) and still did **not** prove a fresh concrete wrong-code bug there from code inspection alone. To keep pressure on that exact conditional-canonicalization seam, I added one fresh host-oracle widening family with an **unrelated phi-bearing anchor function** plus the repaired no-array global-tail style in another function: `/tmp/runtime_re_phianchor_globaltail_batch` came back fully green (`60/60 PASS`). Current authority after this chunk is therefore: machine-ir canonicalization is still conditionally present on the default conservative path whenever the program contains phis anywhere, but this newly widened `phi-anchor + global-tail` family is green too, so the next search step should keep shrinking the remaining downstream handoff surfaces rather than reopening this now-expanded conditional-canonicalization slice immediately again.
- 2026-05-11: I pushed the next downstream slice one step later and one step sharper instead of circling the same family. First, I reread the actual `machine_layout -> machine_emit -> machine_bytes` control-handoff path with extra attention on compare-branch / fallthrough shaping, branch polarity inversion, and global/call operand cloning. Current code-read conclusion is still conservative: I did **not** prove a fresh concrete wrong-code bug directly from `machine_layout_lower.inc`, `machine_emit_lower.inc`, or the corresponding `machine_bytes` compare-branch emission path yet. Second, I widened exactly that handoff surface with a fresh host-oracle batch `/tmp/runtime_re_cmpfall_batch`, targeting `compare-branch + fallthrough-heavy control + helper calls + global mutations + loop anchors`; that batch stayed fully green (`80/80 PASS`). I also tried one more `phi + runtime-global-init + later global-tail` synthetic family to pressure the conditional-canonicalization boundary harder, but that line is **disqualified as generator/oracle-invalid rather than kept as a compiler lead**: my first generated `.out` values were wrong, and a host-equivalent reconstruction showed the mismatch came from the synthetic oracle, not from the compiler. Net result of this chunk: the compare/fallthrough-heavy downstream handoff slice is now widened and still green, the attempted stronger `phi + runtime-init` family is discarded as invalid evidence, and the remaining hidden runtime-RE root cause still looks narrower than these now-expanded green handoff families. No long-running sweep remains in flight at the end of this chunk.
- 2026-05-11: I then pushed one more adjacent downstream-control family instead of stopping at the compare/fallthrough batch. The first attempt to widen with a heavier `phi + runtime-init + later global-tail` family is now explicitly recorded as **invalid evidence** and should stay discarded: once the `.out` files were checked against a host-equivalent runtime-init reconstruction, the mismatch came from the synthetic oracle rather than from the compiler. After discarding that line, I rotated to a valid neighboring family `/tmp/runtime_re_localcmp_batch`, targeting `local arrays + compare-heavy control + helper calls + later local/global interaction` under the same always-on downstream path, and it stayed fully green (`60/60 PASS`). Current authority after this chunk is therefore that another nearby stack/local/control combination also does **not** expose the next hidden runtime-RE witness, so the remaining root cause still appears narrower than the already-expanded compare/fallthrough and local-array/control handoff families.
- 2026-05-11: I also followed through on the still-open “conditional machine-ir canonicalization + array/global hidden family” angle with a more direct valid widening family instead of the earlier invalid runtime-init synthetic line. The fresh host-oracle batch `/tmp/runtime_re_phi_arraymix_batch` combined an unrelated phi-bearing anchor function with array/global-side-effect-heavy helper code (`local arrays`, `2D locals`, global array updates, compare-heavy control, helper calls, and later global reloads) specifically so the default path still enters the report-side conditional canonicalization branch while exercising the unresolved array/global hidden neighborhood. That batch stayed fully green too (`60/60 PASS`). Current authority after this chunk is therefore stronger: even the `phi present somewhere else in the program` + `array/global side-effect family` combination is not currently exposing the hidden runtime-RE line on the live tree, so the remaining root cause looks narrower still than these now-expanded conditional-canonicalization + array/global combinations.
- 2026-05-11: I then closed the matching no-array half of that same conditional-canonicalization angle too, so we do not keep only the array/global side expanded while leaving the lv8-style family underspecified. A fresh valid host-oracle batch `/tmp/runtime_re_phi_funcexpr_batch` combined an unrelated phi-bearing anchor function with a denser `func_expr2 / global_var2`-adjacent family (`nested call expressions`, `global mutation`, `later global reload`, `short-circuit`, and helper side effects). The first draft of this batch had to be thrown away because the generator accidentally referenced `v` before declaration in a few cases, but after regenerating the batch with that oracle/source bug removed, the real runtime-facing result stayed fully green: `70/70 PASS`. Current authority after this chunk is therefore stronger again: neither the array/global hidden family nor the lv8-style `func_expr/global-side-effect` hidden family reproduces when the default path is forced into the conditional `machine_ir` canonicalization branch by an unrelated phi-bearing function elsewhere in the same program.
- 2026-05-11: I then pushed the same conditional-canonicalization pressure one step closer to the earlier BFS/DFS-style suspicion instead of stopping at `func_expr/global-side-effect`. A fresh valid host-oracle batch `/tmp/runtime_re_phi_bfsmix_batch` combined the unrelated phi-bearing anchor function with a compact `BFS/DFS-like global-array + queue/visited + short-circuit recursion/queue gating` family. That batch stayed fully green too (`40/40 PASS`). Current authority after this chunk is therefore stronger again: even the earlier “maybe the hidden line is a BFS/DFS-like control/state corruption that only appears when conditional canonicalization is active elsewhere in the program” hypothesis is not reproducing on the live tree in this widened family.
- 2026-05-11: I then widened one more still-plausible corner of that same line instead of stopping at BFS/DFS-style state machines: **address-carrier style array-parameter/local-array interactions** under cross-program phi-triggered conditional canonicalization. The fresh host-oracle batch `/tmp/runtime_re_phi_addrcarrier_batch` combined the unrelated phi-bearing anchor with local arrays, 2D local arrays, array-parameter helper calls, repeated address-carrier style read/write traffic, global-array side effects, and later reload/useback. That batch stayed fully green too (`60/60 PASS`). Current authority after this chunk is stronger again: even the address-carrier / array-parameter neighborhood is not currently exposing the hidden runtime-RE line when the default path is forced into the conditional `machine_ir` canonicalization branch elsewhere in the same program.
- 2026-05-11: I also pushed the same line into a recursion/control-state family closer to the earlier hidden `hanoi2` memory without reopening the already-green simple recursion probes. The fresh host-oracle batch `/tmp/runtime_re_phi_hanoi_batch` combined the unrelated phi-bearing anchor function with a compact `hanoi`-style recursion family (`global peg arrays`, `global counters`, recursive move scheduling, short-circuit move gating, and later score/useback`). That batch stayed fully green too (`50/50 PASS`). Current authority after this chunk is stronger again: even the `phi elsewhere + hanoi-like recursive global-state family` combination is not currently exposing the hidden runtime-RE line on the live tree.
- 2026-05-11: I then switched off the conditional-canonicalization line again and tested a different always-on downstream suspicion: **preview-text caller-save handling around void calls with live caller values and stack arguments**. The first draft batch `/tmp/runtime_re_voidcall_save_batch` initially looked alarming (`37/70` mismatches), but that line is now explicitly **disqualified as generator/oracle-invalid rather than kept as a compiler lead**: the draft allowed negative intermediate/final `%` results, so host exit-code oracles became unstable/ambiguous and did not provide a trustworthy semantic reference. I regenerated the family in a bounded nonnegative form as `/tmp/runtime_re_voidcall_save_batch2`, still targeting `void calls + live caller values + stack arguments + later reuse`, and the corrected valid batch stayed fully green (`70/70 PASS`). Current authority after this chunk is therefore that this caller-save/void-call suspicion did **not** produce a valid compiler witness once the oracle was repaired, so it should not be treated as the hidden runtime-RE explanation.
- 2026-05-11: I also completed the matching **non-void** half of that same caller-save/stack-arg suspicion instead of leaving it only half explored. A fresh bounded host-oracle family `/tmp/runtime_re_nonvoid_stackcall_batch` targeted `non-void calls + stack arguments + live caller values after the call + later reuse`, and it stayed fully green (`80/80 PASS`). Current authority after this chunk is therefore stronger again: neither the void-call nor the non-void-call version of the caller-save/stack-arg suspicion is currently reproducing the hidden runtime-RE line on the live tree.
- 2026-05-11: I then also combined that now-green non-void stack-call family with the `phi elsewhere -> conditional machine_ir canonicalization` trigger so we do not leave a cross-product gap there. The fresh bounded host-oracle batch `/tmp/runtime_re_phi_nonvoid_stackcall_batch` combined an unrelated phi-bearing anchor function with the `non-void calls + stack arguments + live caller values after the call + later reuse` family, and it stayed fully green too (`60/60 PASS`). Current authority after this chunk is therefore stronger again: neither the plain path nor the phi-triggered conditional-canonicalization path is reproducing the non-void caller-save/stack-arg suspicion on the live tree.
- 2026-05-11: I also completed the matching **return-value / local-array pressure** half of that same line instead of leaving stack-arg call pressure only on direct later-reuse shapes. A fresh bounded host-oracle batch `/tmp/runtime_re_phi_returnspill_batch` combined the unrelated phi-bearing anchor function with `non-void helper returns carried through local-array-heavy callers, many arguments, and later reuse`, and it stayed fully green too (`80/80 PASS`). Current authority after this chunk is therefore stronger again: even the return-value / local-array pressure variant of this always-on downstream suspicion is not currently exposing the hidden runtime-RE line on the live tree.
- 2026-05-11: I then tried one more family that had not been covered yet and is still plausibly “always on”: **runtime global initializer helper with many-argument non-void calls plus later array/global readback**. The first draft batch `/tmp/runtime_re_rtinit_manyargs_batch` looked like a wall of mismatches, but that line is now explicitly **disqualified as oracle-invalid rather than kept as a compiler lead**: the expected `.out` values came from an incorrect handwritten runtime-init simulation. I regenerated the family with a host-C `runtime_init()` oracle generator, deleted the helper `.c` files from the actual suite directory so only the validated `.sy + .out` pairs were swept, and the corrected valid batch `/tmp/runtime_re_rtinit_manyargs_valid_batch` stayed fully green (`30/30 PASS`). Current authority after this chunk is therefore stronger again: even the runtime-global-init + many-argument non-void-call family is not currently exposing the hidden runtime-RE line on the live tree once the oracle is made trustworthy.
- 2026-05-11: I then also combined that runtime-init + many-arg family with the `phi elsewhere -> conditional machine_ir canonicalization` trigger so this line is no longer only tested on the plain path. The fresh bounded host-oracle batch `/tmp/runtime_re_phi_rtinit_manyargs_batch` combined an unrelated phi-bearing anchor function with the same runtime-global-init helper + many-argument non-void-call family, and it stayed fully green too (`30/30 PASS`). Current authority after this chunk is therefore stronger again: neither the plain path nor the phi-triggered conditional-canonicalization path is reproducing the runtime-init + many-arg non-void-call suspicion on the live tree.
- 2026-05-11: I then tried one deliberately broader move instead of another tiny adjacent family: a mixed always-on downstream “mega-mix” batch that combined globals, local arrays, 2D locals, scan helpers, recursion, call chains, stack-arg calls, and later global/local readback. The first version `/tmp/runtime_re_megamix_batch` produced many mismatches, but that line is now explicitly **disqualified as generator-invalid rather than kept as a compiler lead**: the first inspected witness passed a `grid[2][3]` local into a helper declared as `int a[][4]`, which is incompatible-pointer UB in C and therefore not a trustworthy oracle surface. I regenerated the family with a fixed common second dimension but then found the next mismatches still sat on invalid negative-index / negative-modulo behavior, so that intermediate line `/tmp/runtime_re_megamix_valid_batch` is also disqualified as invalid evidence rather than kept as a compiler lead. Finally I rebuilt the same broad family again in a fully bounded nonnegative form as `/tmp/runtime_re_megamix_nonneg_batch`, and the corrected valid batch stayed fully green (`100/100 PASS`). Current authority after this chunk is therefore stronger again: even a much broader mixed downstream family is not currently exposing the hidden runtime-RE line once oracle-invalid UB is removed from the generated surface.
- 2026-05-10: The next fresh runtime-facing widening round finally produced one more **concrete conservative-path wrong-code witness** instead of only more all-green neighboring packs. A new no-array synthetic batch under `/tmp/noarray_hidden2/` exposed `case_001.c` as a deterministic failure on the live default/conservative tree (`expected 124`, actual generated-program exit `130`). An instrumented variant showed the first divergence already inside `f3(...)`: the taken path of `if (g1 > 2 && g0 != 3) g0 = g0 + 3; return a1 + g0;` skipped the `g0 = g0 + 3` update and returned through a stale value. Stage localization narrowed it quickly: the raw conservative allocate+rewrite machine-ir dump was still correct, but the **canonicalized machine-ir** dump was already wrong. Root cause is now localized and patched in `src/machine/lowering/machine_ir/machine_ir_slot_cleanup.inc`: known-slot inlineable return/branch tail folding was allowed to replace a non-empty jump block wholesale with a successor's rewritten inlineable wrapper, deleting predecessor instructions that still defined the rewritten operand or preserved required side effects (notably `store_global`). The live repair now distinguishes blocks that are safe to replace wholesale from non-empty predecessor blocks that must keep their body; in the unsafe case it appends the rewritten instruction + new terminator instead of deleting the predecessor instructions. I then widened the same safety rule across the sibling shared-successor fold sites in that same file as a prophylactic closure of the same unsound pattern. Focused runtime-facing rechecks after the fix are green: `/tmp/case001_dbg.c` PASS, regenerated `/tmp/noarray_hidden2` now `39/39 PASS` on the host-oracle-backed `.c` cases (remaining skips are oracle-missing generator noise), `08_global_arr_init.sy` PASS, `28_side_effect2.sy` PASS, `32_many_params3.sy` PASS, `34_multi_loop.sy` PASS, `09_BFS.sy` PASS, `10_DFS.sy` PASS, and `lava_test/short_circuit1.sy` PASS. I then widened the same repaired family again with five fresh host-oracle batches, and they all stayed fully green: `/tmp/runtime_re_globaltail_batch` (`100/100 PASS`) for `global mutation + branch-gated later return/global reload + nested helper calls`, `/tmp/runtime_re_sharedtail_batch` (`100/100 PASS`) for `shared tail / wrapper fold / global side-effect / nested call` shapes, `/tmp/runtime_re_wrappermerge_batch` (`100/100 PASS`) for duplicated then/else wrapper-merging opportunities with helper calls and global stores, `/tmp/runtime_re_callwrapper_batch` (`100/100 PASS`) for duplicated call-wrapper tails with surrounding global mutation/branch gating, and `/tmp/runtime_re_branchwrapper_batch` (`100/100 PASS`) for duplicated branch-wrapper tails with surrounding helper calls and global mutation. I also tried pushing farther into a “linear-merge-heavy” family, but the first red batch `/tmp/runtime_re_linearmerge_batch` is now explicitly **disqualified as generator UB** rather than kept as a compiler lead: those sources allowed signed-`int` overflow, so host-oracle `.c` results were not a trustworthy semantic reference. A rebuilt overflow-safe replacement batch `/tmp/runtime_re_linearmerge_safe_batch` now stays fully green (`100/100 PASS`). A separate bounded-array widening under `/tmp/runtime_re_arrmerge2_batch`, targeting `bounded array traffic + merge-heavy control flow + helper-call interleaving`, is also fully green (`50/50 PASS`), one more bounded indirect-memory/control-flow widening under `/tmp/runtime_re_indirectphi_batch`, targeting `indirect memory + merge-heavy branch joins + helper-call interleaving`, is now also fully green (`60/60 PASS`), and a compare-branch-heavy bounded batch `/tmp/runtime_re_cmpbranch_batch`, targeting `compare-branch immediate/control-flow density + helper-call interleaving`, is likewise fully green (`80/80 PASS`). Progress-memory note for the active reread spine: this closure reopens `src/machine/lowering/machine_ir/*` as the newest front-most kept bug surface above the previously exhausted `machine_select -> machine_bytes -> compiler_driver` tail. Immediate follow-up still pending: reconcile broad local golden-output/unit surfaces before treating this as a checkpoint commit.
- 2026-05-10: Follow-up inspection makes those `compiler2023 hidden_functional` BFS/DFS timeouts much harder to dismiss as normal performance noise. The two sources are small (`n=1000`, `m=5000`, about `4.5k` queries each), and a host-compiled C++ translation with trivial runtime wrappers finishes both in about `0.009s`; yet the current compiled RISC-V outputs still hit `RUN_TIMEOUT` even with `--case-timeout 60`. Current authority therefore upgrades `09_BFS.sy` / `10_DFS.sy` from “maybe just heavy” to “very likely bug leads”: they now look like wrong control-flow / wrong side-effect / state-corruption candidates whose generated code is doing far more work than the source algorithm should require.
- 2026-05-10: The next runtime-facing repro chunk immediately after that BFS/DFS diagnosis did close a fresh real wrong-code family, but it lived one stage earlier than the nearly exhausted compiler-driver tail. A minimized witness `for (; x && getint(); x = x - 1) { f = 1; } if (f) ...` reproduced a zero-iteration miscompile (`0` still printed `111` instead of `222`), which exposed that canonical-IR loop lowering was still handling post-loop local-const facts unsafely in **both** directions: `for` loops could leak body facts to the exit path, while the earlier `while` repair had been restoring pre-loop facts too bluntly for locals mutated inside the loop. The live repair in `src/ir/ir_lower_stmt.inc` now restores the pre-loop scope shape but clears constant-value facts for any scalar local mutated in the loop condition/body/step before the exit path continues. That keeps the zero-iteration witness correct, avoids the opposite “loop definitely ran but entry fact came back” fold, and still preserves outer-scope shape/symbol visibility. IR regressions now lock both sides of the family: `test_ir_keeps_pre_loop_local_fact_after_unknown_calling_while_body` now requires an explicit post-loop branch on `f` instead of an unsound fold, and the new `test_ir_clears_mutated_local_fact_after_unknown_calling_for_loop` locks the concrete `for` witness. Kept rechecks after the fix are `make test-ir-regression` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), plus focused `compiler2023 hidden_functional` reruns `09_BFS.sy` PASS and `10_DFS.sy` PASS at `--case-timeout 60`. No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next adjacent audit/repro chunk on that same `src/ir/ir_lower_stmt.inc` line immediately exposed another real wrong-code family: condition-side-effect dropping under truthiness folding. A minimized witness `int i = 0; if (i = 1) {} putint(i);` reproduced a direct miscompile (`0` instead of `1`), proving that `if` lowering had been using known condition truthiness to skip condition lowering entirely even when the condition expression itself mutated state. The same unsound shortcut also applied to `while (i = 0)` and `for (; i = 0; )` style guards. The live repair now makes condition-truthiness pruning side-effect-aware: if the condition contains assignments, compound assignments, `++/--`, calls, or nested expressions containing them, lowering keeps the real condition-evaluation path instead of taking the early known-truthiness shortcut. Focused IR regressions now lock all three surfaces: `test_ir_lowers_if_condition_side_effect_even_when_truthiness_known`, `test_ir_lowers_while_condition_side_effect_even_when_truthiness_known`, and `test_ir_lowers_for_condition_side_effect_even_when_truthiness_known`. Revalidation after the fix is green: `make test-ir-regression` PASS, `make test` PASS, `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), the local runtime minis `if_side_effect.sy` / `while_side_effect.sy` / `for_side_effect.sy` all PASS, and a rotated external `compiler2023 hidden_functional` batch (`21_union_find`, `22_matrix_multiply`, `23_json`, `24_array_only`, `25_scope3`, `26_scope4`, `27_scope5`, `28_side_effect2`, `30_many_dimensions`, `31_many_indirections`) also stays green (`10/10`). No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: One more immediate follow-up on that same canonical-IR branch-fact line exposed an adjacent join bug rather than a fresh backend/runtime layer reopen. A minimized witness `int i = 1; if (a) i = 2; if (i == 1) ...` reproduced a direct miscompile (`0` instead of `1` when `a == 0`), proving that the no-else `if` join had been restoring the post-`if` scope from the then-path facts alone whenever the then-branch fell through, instead of merging them with the false path. A condition-side-effect sibling witness `if ((i = 1) && 0) {} if (i) ...` reproduced the same family from the opposite direction, where the stale false-path fact survived even though the condition-side assignment had already executed. The live repair now merges the no-else join facts with the false path instead of taking `then_scopes` alone, and it reuses the new condition-side-effect clearing rule so side-effectful conditions conservatively clear affected locals before that merge instead of reviving stale pre-condition facts. Focused IR regressions now lock both surfaces: `test_ir_merges_no_else_branch_facts_with_false_path` and `test_ir_merges_no_else_condition_side_effect_fact_to_continue`. Revalidation after this closure is green: `make test-ir-regression` PASS, `make test` PASS, the local runtime minis `if_no_else_merge.sy`, `if_cond_scope_follow.sy`, `if_side_effect.sy`, `while_side_effect.sy`, and `for_side_effect.sy` all PASS (`5/5`), and `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`). No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next adjacent expression-side follow-up on that same canonical-IR fact/join line exposed one more real wrong-code family in `src/ir/ir_lower_expr.inc`: ternary/logical expression joins were leaving one branch's mutated-local facts alive after the value join. A minimized witness `x = a ? (i = 1) : (i = 2); if (i == 1) ...` reproduced directly as a runtime mismatch (`ternary_scope_follow.sy`), and the logical-value sibling `x = a && (i = 1); if (i == 1) ...` showed the same structural risk. The live repair now clears mutated-local constant facts conservatively after logical-value and ternary-expression joins instead of letting one branch snapshot dominate the post-expression scope. Focused IR regressions now lock both surfaces: `test_ir_clears_ternary_branch_mutation_facts_after_join` and `test_ir_clears_logical_value_branch_mutation_facts_after_join`. Revalidation after the fix is green on the targeted/runtime-facing surfaces: `make test-ir-regression` PASS, `make test` PASS, the local expression-join minis `comma_scope_follow.sy`, `logical_unknown_scope_follow.sy`, and `ternary_scope_follow.sy` all PASS (`3/3`), and the rotated external batch still keeps at least one new live lead visible rather than masking it—`compiler2023 hidden_functional/34_multi_loop.sy` now stands out as the next concrete runtime mismatch (`expected 42`, actual process exit `162`) after filtering out several obviously unsupported floating-point-style cases from the same raw folder. Current interpretation is that `34_multi_loop.sy` likely points downstream of canonical IR because the dumped canonical IR for that witness now looks structurally sane, so the next useful mainline after this closure is to trace that witness into lower/machine/backend layers rather than staying only on canonical branch-fact fixes.
- 2026-05-10: That downstream `34_multi_loop.sy` trace is now no longer just a localization note; it produced a kept fix on the Value-SSA allocate+rewrite line. Stage-by-stage dumps first confirmed that canonical IR, lower IR, and the ordinary Value-SSA graph shape were materially sane for this witness, while the first bad shape appeared in the multi-round spill-rewrite loop: once the nested-loop program had already grown spill locals from earlier rewrite rounds, `value_ssa_alloc_rewrite_block_local_spill_split_for_value(...)` was still willing to split one of those already-spilled value families again. That let a later rewrite round mix a newer allocation result with an older spill-local family, which in the live witness corrupted nested loop counters and ultimately produced `expected 42`, actual exit `162`. The kept repair now skips block-local spill splitting for values that are already materially represented through spill-named locals from an earlier rewrite round (either via a spill-local load definition or a spill-local store use). A focused allocator regression now locks that nested shape directly: `test_value_ssa_allocate_and_rewrite_program_keeps_distinct_nested_spill_families`. Revalidation after this closure is green: `make test-value-ssa-alloc` PASS, `make test` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), the focused witness `34_multi_loop.sy` PASS, and the rotated external hidden-like batch `21_union_find`, `22_matrix_multiply`, `23_json`, `24_array_only`, `28_side_effect2`, `30_many_dimensions`, `31_many_indirections`, `32_many_params3`, and `34_multi_loop` all PASS (`9/9`). No long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next runtime-RE search rounds after closing `34_multi_loop.sy` widened the external/runtime-heavy evidence surface again but did **not** immediately expose a new supported-subset red point. Fresh heavy/hidden-like batches now additionally include `compiler2025 functional` heavy cases (`62_percolation`, `63_big_int_mul`, `64_calculator`, `65_color`, `68_brainfk`, `69_expr_eval`, `70_dijkstra`, `71_full_conn`, `75_max_flow`, `76_n_queens`) all PASS (`10/10`), `compiler2026 functional` supported-subset upper-tail cases `77_substr`..`94_nested_loops` all PASS (`18/18`), and `compiler2021 h_functional` upper-tail cases `110_many_params3`..`117_nested_loops` all PASS (`8/8`). Because those fresh pools still stayed green, the active search line then pivoted—as previously planned—into stronger supported-subset stress generation instead of pretending the next witness would necessarily come from stock external archives. While doing that, I also fixed one tooling bug in `tools/generate_sysy_runtime_re_stress.py`: bridge emission no longer references undeclared `a8`/`a9`/`a10` names when `param_count < 11`; it now wraps parameter references modulo the actual configured parameter count. A fresh three-case generated stress bundle (`stress_a/b/c`) now builds oracle files successfully and stays green under `tools/sweep_sysy_suite.py` (`3/3 PASS`). Current authority is therefore that the runtime-RE hunt is now in “wider supported-subset search plus stronger generated stress” mode rather than “currently sitting on one reproduced external red point,” and no long-running sweep remained in flight at the end of this chunk.
- 2026-05-10: The next kept closure stayed on that stronger generated-stress / many-args line and finally closed the still-open `3002/3003` branch-condition sibling instead of only logging more localization. The decisive minimized witness was `/tmp/ma3002_reduce/P_postpred_bool.sy`: the full rewritten Value-SSA program was already correct again, but the final machine-IR path was still wrong because the **final allocation result no longer matched the spill family that the rewritten program had materialized**. In the concrete `wide1` shape, the rewritten program had explicitly spilled `ssa.0..ssa.3` into `spill.0..spill.3`, while the post-rewrite reallocation silently switched the spill set to `ssa.4..ssa.7`; machine lowering then paired those two inconsistent artifacts and rebuilt the `wide0(...)` call with the wrong argument carriers. The live fix is now on the allocate+rewrite loop handoff in `src/value_ssa_alloc/value_ssa_alloc_color.inc`: after a rewrite round has already materialized spill locals and the rewritten function still has the same value-count shape, a later reallocation is no longer allowed to replace that function's allocation result with a different spill family. Instead the loop preserves the earlier spill-compatible allocation result for that function so the rewritten program and final allocation stay synchronized. Focused regressions now lock the family in `tests/machine/lowering/machine_ir/machine_ir_test.c` (`test_machine_ir_allocate_and_rewrite_keeps_rewritten_many_args_spill_shape_stable`), and the kept rechecks are strong on both the reduced and widened surfaces: `make test-value-ssa-alloc` PASS, `make test-value-ssa-machine` PASS, `make test-machine-ir` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), reduced witnesses `K_postcall_side_only.sy`, `N_postcall_pred_only.sy`, `P_postpred_bool.sy` all PASS, generated seeds `3001.sy` / `3002.sy` / `3003.sy` all PASS, and the widened generated family `/tmp/many_args_hidden_like_batch` is now **`60/60 PASS`**. No long-running sweep remained in flight at the end of this chunk; the next useful runtime-RE search step should widen away from this now-closed many-args family instead of continuing to treat it as the front-most red line.
- 2026-05-10: I then immediately did that promised widen-away step instead of stopping at the many-args closure. A new adjacent generated stress batch, `/tmp/runtime_re_adjacent_batch3`, pushed a similar but not identical shape (`many-args + arrays + alias-style kernels + deeper branch nesting`) while trying to stay below the known preview-branch cliff. Current result is mixed but informative rather than a fresh runtime wrong-code closure: `adj2_a`, `adj2_b`, `adj2_c`, and `adj2_e` all PASS, while `adj2_d` and `adj2_f` fail earlier at the already-known compile-stage boundary `MACHINE-BYTES-344: riscv preview branch target out of range`. In parallel I revalidated the most relevant external `compiler2023 hidden_functional` neighbors: `09_BFS.sy` PASS, `10_DFS.sy` PASS, `28_side_effect2.sy` PASS, and `32_many_params3.sy` PASS under focused `--case-timeout 60` reruns. The one fresh reopened lead from this post-many-args widening round is `compiler2023 hidden_functional/34_multi_loop.sy`, which is now back to `RUN_TIMEOUT` even at `--case-timeout 120` on the current tree. Current authority is therefore: the newly generated adjacent batch has not yet exposed another runtime-RE family beyond compile-size pressure, but `34_multi_loop.sy` must now be treated as a re-opened active runtime/logic lead rather than an already-closed witness.
- 2026-05-10: After re-closing `34_multi_loop.sy`, I continued widening again instead of stopping at that fix, and this time the widened loop-/phi-heavy batch **did** expose a fresh direct wrong-code lead. The new generated batch `/tmp/runtime_re_loopphi_batch` is `7/8 PASS`; the one red point is `loopphi_e.sy -> MISMATCH` (`expected 445`, actual `5898`). Current localization already rules out “just another timeout/noise” interpretation: the final allocation result for `main` still assigns the same color to interfering values (for example the rewritten-program pair `ssa.91 = load_local acc.0` and `ssa.92 = load_local mix.1` both come back `color=0` even though the interference graph still reports an edge), and machine-ir lowering correspondingly emits wrong shapes like `load local.0 ; load local.1 ; add reg, reg`. The immediately preceding machine-ir-side protection work was enough to keep `34_multi_loop.sy` green, but it does **not** fix this new witness. Current authority is therefore that the next active runtime-RE slice is now an allocator/mainline correctness line: understand why the allocator can still produce same-color placements for interfering non-spilled values in rewrite-heavy functions, fix that root cause, then rerun `loopphi_e` plus the already-closed many-args / `34_multi_loop` witnesses to make sure the older closures stay shut.
- 2026-05-10: That `loopphi_e.sy` lead is now closed too, and the final root cause landed one stage later than the first allocator-color suspicion. Stage-by-stage probing showed the allocator/rewrite line still looked unstable in intermediate iterations, but the final surviving semantic corruption came from `machine_ir` lowering not protecting **same-register multi-operand uses** after allocation. Raw machine-ir could still emit shapes like `load local.0 ; load local.1 ; add reg.0, reg.0`, i.e. two distinct SSA operands colored to the same machine register but consumed as if they were still simultaneously available. The live repair is now in `src/machine/lowering/machine_ir/machine_ir_lower.inc`: protected spill-slot assignment covers branch-/phi-carried live-through values **and** same-register binary operand pairs, same-register call-argument pairs, and same-register `store_indirect(addr,value)` pairs. Revalidation after this closure is green on both the direct witness and the widened adjacent line: `loopphi_e.sy` PASS, `/tmp/runtime_re_loopphi_batch` now **`8/8 PASS`**, and a fresh post-fix adjacent widening batch `/tmp/runtime_re_loopphi_postfix_batch` also stays green (`4/4 PASS`). The older kept closures remain shut too: `make test-machine-ir` PASS, `autotest -riscv -s lv8 /workspaces/compiler_lab` PASS (`12/12`), `autotest -riscv -s lv9 /workspaces/compiler_lab` PASS (`22/22`), `/tmp/ma300x` still `3/3 PASS`, and `/tmp/compiler2023_focus2` (`28_side_effect2`, `32_many_params3`, `34_multi_loop`) stays `3/3 PASS`. Current authority is therefore that the loop-/phi-heavy family uncovered after `34_multi_loop` is now reclosed, and the next search step should rotate to a different adjacent runtime-RE family instead of continuing to sit on this line.
- 2026-05-10: I then did that rotate-away step immediately instead of stopping on the `loopphi_e` closure. The next adjacent generated batch focused on indirect-memory / array-address / repeated-index traffic (`/tmp/runtime_re_indirect_batch`), and it stayed fully green (`8/8 PASS`). The matching external `compiler2023` neighbors were also revalidated through the focused array/indirection slice and stayed green. Current authority is therefore that this adjacent indirect-memory family is not currently exposing the next runtime-RE lead, so the next useful search turn should rotate again rather than lingering on a now-all-green slice.
- 2026-05-10: I immediately rotated again after that indirect-memory all-green result instead of stopping. The next adjacent generated batch focused on **array-parameter aliasing + nested calls + branch-heavy control** (`/tmp/runtime_re_aliascall_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this alias/call-heavy adjacent family is not currently exposing the next runtime-RE lead either, so the search should continue rotating rather than reopening either the already-green indirect-memory or alias/call slices.
- 2026-05-10: I then rotated one more time instead of stopping on the alias/call all-green result. The next adjacent generated batch focused on **indirect-store addr/value overlap + same-array aliasing through helper calls** (`/tmp/runtime_re_storealias_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this store-/alias-heavy adjacent family is not currently exposing the next runtime-RE lead either, so the next useful search step should keep rotating rather than circling back to these already-green synthetic slices.
- 2026-05-10: I continued rotating again after that store-/alias-heavy all-green result. The next adjacent generated batch focused on **recursion + global-array mutation + short-circuit / branch gating** (`/tmp/runtime_re_recur_short_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this recursion/side-effect-heavy adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening these already-green synthetic slices.
- 2026-05-10: I then rotated once more after that recursion/short-circuit all-green result. The next adjacent generated batch focused on **local-array stack traffic + same-base multi-parameter aliasing + helper mutation** (`/tmp/runtime_re_localalias_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this local-array/stack-alias adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening these already-green synthetic slices.
- 2026-05-10: I rotated again after that local-array/stack-alias all-green result. The next adjacent generated batch focused on **global-scalar/global-array side effects + branch-gated helper calls** (`/tmp/runtime_re_globalside_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this global-side-effect adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening these already-green synthetic slices.
- 2026-05-10: I rotated again after that global-side-effect all-green result. The next adjacent generated batch focused on **helper return values reused as array indices / branch gates** (`/tmp/runtime_re_retindex_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this return-value-as-index adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening these already-green synthetic slices.
- 2026-05-10: After disqualifying the earlier invalid stress witness, I resumed the valid adjacent-family hunt instead of stopping on the generator fix. The next rotated batch focused on **multi-dimensional local/global array indexing + helper-returned bounded indices + same-base write/read recombination** (`/tmp/runtime_re_mdindex_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this multidimensional address-formation/index-reuse adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening the already-green return-value/index or alias-heavy slices.
- 2026-05-10: I continued widening after that multidimensional-index all-green result instead of stopping. First I reran the full supported portion of `compiler2022/公开样例与运行时库/hidden_functional`; the runtime-relevant surface stayed green again (`00..28`, `30..32`, `34` all PASS), and the only reds remained the already-known non-runtime classes: `29_long_line.sy -> COMPILE_FAIL`, `33_multi_branch.sy -> COMPILE_TIMEOUT`, plus the unsupported floating-point inputs `35..39`. Then I rotated to a fresh valid synthetic batch focused on **deep recursion + 12-argument calls + local-array stack traffic + branch-gated parameter permutation** (`/tmp/runtime_re_widerecur_batch`), and it also stayed fully green (`8/8 PASS`). Current authority is therefore that this wide-recursive stack/argument adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than reopening the already-green many-args or local-stack slices.
- 2026-05-10: I kept rotating instead of stopping there, and the next two valid synthetic batches also stayed fully green rather than reopening a runtime lead. The first one focused on **many scalar globals + nested short-circuit gating + helper side effects** (`/tmp/runtime_re_scalarshort_batch`) and passed `8/8`; the second pushed that same idea into a heavier variant with more scalar globals and deeper short-circuit stacking (`/tmp/runtime_re_scalarshort2_batch`) and also passed `8/8`. Current authority is therefore that this scalar-global / short-circuit-heavy adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than circling back to already-green control/side-effect slices.
- 2026-05-10: I then followed the user hint to bias even harder toward optimization-sensitive shapes instead of only broad side-effect mixes. A fresh valid synthetic batch focused on **constant-heavy local assignments + trivially foldable arithmetic + branch-heavy control + scalar-global side effects** (`/tmp/runtime_re_constbranch_batch`), i.e. shapes deliberately meant to exercise constant folding / branch pruning / short-circuit cleanup while still keeping observable side effects in helper calls. That batch also stayed fully green (`8/8 PASS`). Current authority is therefore that this constant-heavy optimization-trigger adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than only rephrasing the same scalar-global/short-circuit idea.
- 2026-05-10: I then followed up on the user's “try a splay/LCT-like structure” suggestion instead of staying only on scalar-control generators. The first direct `splay` batch (`/tmp/runtime_re_splay_batch`) turned out to hit the already-known preview compile-size boundary (`MACHINE-BYTES-345: riscv preview compare-branch target out of range`) rather than a runtime lead, so I shrank it into a still-meaningful but compile-safe form: `splay_small_*` keeps global pointer arrays, rotations, parent/child rewiring, lazy tags, `find`/`kth`/`splay`, and branch-heavy tree updates, while moving the operation stream into local initialized arrays to shorten branch spans. That compile-safe batch `/tmp/runtime_re_splay_small_batch` stayed green (`4/4 PASS`). Current authority is therefore that this first splay-style tree-rotation family is not currently exposing the next runtime-RE lead either, while the larger version only reiterates the already-known branch-span compile boundary rather than revealing a new runtime bug.
- 2026-05-10: I also pushed one more deliberately optimization-sensitive widening round after the scalar-global and splay checks, still following the user hint to emphasize foldable constants / branch pruning / hidden side effects without letting the whole program collapse to dead code. The fresh valid batch `/tmp/runtime_re_optstress_batch` focused on **constant-heavy locals + trivially foldable predicates + nested branch storms + scalar-global helper side effects**, and it stayed fully green (`8/8 PASS`). Current authority is therefore that this new optimization-stress adjacent family is not currently exposing the next runtime-RE lead either, so the search should keep rotating rather than staying only on one constant/branch recipe.
- 2026-05-10: I then kept following the user hint to track pass-shaped families more directly instead of only broad semantic mixes. The fresh valid batch `/tmp/runtime_re_exprreuse_batch` focused on **repeated arithmetic expressions + branch-merge joins + helper side-effect barriers**, i.e. something deliberately closer to `fold / CSE / branch-merge` pressure, and it stayed fully green (`8/8 PASS`). I then rotated once more into a more pass-specific family around **repeated pure-call-looking helpers + branch joins + impure call barriers** (`/tmp/runtime_re_purecall_batch`), and that also stayed fully green (`8/8 PASS`). Current authority is therefore that neither this expression-reuse nor pure-call-reuse adjacent family is currently exposing the next runtime-RE lead.
- 2026-05-10: I also followed through on the earlier “maybe try LCT after splay” direction instead of leaving tree-rotation structure at only one splay-shaped checkpoint. A first `LCT-lite` batch with global arrays for parent/children, `makeroot/access/link/cut/pathsum`, lazy reverse tags, and branch-heavy operation gating (`/tmp/runtime_re_lct_batch`) stayed green too (`4/4 PASS`). Current authority is therefore that this first link-cut-tree-style structural family is not currently exposing the next runtime-RE lead either.
- 2026-05-10: I kept pushing toward optimization-pass-shaped families after that instead of only rotating broad semantics. One attempted direct `reuse_addr_roots` pressure batch using raw pointer syntax was invalid for SysY and was discarded immediately; the corrected valid replacement `/tmp/runtime_re_addrreuse_batch2` used **array parameters + repeated same-base indexing + pure-expression reuse + side-effect barriers**, and it stayed green (`8/8 PASS`). I also checked the same valid cases under the current `machine_select` skip flags for `reuse-addr-roots` and `cleanup-pure`; there was no observed output drift between the normal compiler and those skip-flag variants on this batch. Current authority is therefore that this first differential addr-root / pure-cleanup family is not currently exposing the next runtime-RE lead either.
- 2026-05-10: In parallel with the witness hunt, I also prepared one **diagnostic conservative submission mode** at the user's request so we can bisect whether the remaining hidden red points come from optimization layers at all. In the current tree, aggressive optimization is now **disabled by default** unless the environment variable `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS=1` is set. The conservative path does three things: `1)` it lowers `lower_ir -> value_ssa` through the plain `value_ssa_build_from_lower_ir(...)` path instead of the default canonicalized / memory-SSA-heavy build, `2)` it skips `value_ssa_optimize_perf_hotspots(...)`, and `3)` it returns early from `machine_select` right after the initial verifier instead of running the later cleanup stack; final preview-text peepholes are skipped for the same conservative mode too. This is intentionally a **diagnostic branch**, not a claimed fix. Focused smoke rechecks on the conservative build stayed viable (`34_multi_loop.sy` PASS, `92_register_alloc.sy` PASS, `short_circuit1.sy` PASS), so the tree is ready if we want to commit/push this conservative diagnostic version and compare judge results.
- 2026-05-10: After the all-spill conservative submission still did **not** change the hidden result, I explicitly shifted the search away from “optimizer main culprit” and into non-optimization mainline surfaces. The first library/ABI-adjacent batch on that line, `/tmp/runtime_re_libabi_batch2`, focused on **`getarray`/`putarray` + local/global arrays + 2D accumulation + side-effectful scalar globals**, and it stayed fully green (`8/8 PASS`). Current authority is therefore that this first library-function / array-ABI adjacent family is not currently exposing the next runtime-RE lead either, so the search should continue shrinking elsewhere in the downstream mainline.
- 2026-05-10: I then prepared one more downstream diagnostic variant instead of only writing new witnesses. In addition to the existing conservative/all-spill defaults, global-object emission now also falls back to **plain `.bss/.data`** instead of `.sbss/.sdata` unless `COMPILER_ENABLE_AGGRESSIVE_OPTIMIZATIONS=1` is set. This is another diagnostic-only shrink intended to test whether small-data section policy or adjacent global-layout assumptions are part of the hidden runtime issue. Focused smoke rechecks on the relevant array/global surfaces stayed viable (`07_arr_init_nd.sy` PASS, `08_global_arr_init.sy` PASS, `24_array_only.sy` PASS), so this `.data/.bss` conservative variant is ready to submit if we want to compare judge results against a less small-data-specific backend configuration.
- 2026-05-10: I also pushed one more direct downstream witness family after that instead of only toggling diagnostic modes. The fresh valid batch `/tmp/runtime_re_bigframe_batch` focused on **large stack frames + many local slots + repeated indirect/local memory traffic + helper-call mixing**, i.e. a shape meant to stress the always-on local/spill memory path in the backend rather than the optimizer. That batch stayed fully green (`8/8 PASS`). Current authority is therefore that this big-frame/local-slot adjacent family is not currently exposing the next runtime-RE lead either, which further weakens the earlier suspicion that the remaining hidden RE is just a generic “large frame / many locals” backend failure.
- 2026-05-10: I then prepared one more **calling-convention diagnostic variant** instead of only shrinking optimizations or writing new witness families. In the current conservative mode, functions with calls now also reserve an explicit caller-save area and save/restore the caller-clobbered register set around each call site. This is intentionally a diagnostic mask, not a claimed fix, and it specifically tests whether the still-open hidden RE is really a residual call-clobber / caller-save hole that our existing `machine_ir` call-crossing protections failed to cover. Focused smoke rechecks on the caller-heavy/public-adjacent surfaces stayed viable (`34_multi_loop.sy` PASS, `92_register_alloc.sy` PASS, `short_circuit1.sy` PASS), so this caller-save diagnostic variant is ready to submit if we want another judge-side comparison.

## Historical Note

The authority sections above are the current policy surface.

The execution log is intentionally retained below them as historical record, not as a second competing roadmap.

- 2026-05-07: A fresh public-perf recheck on the live tree changed the current surface in a way that needs to stay visible in roadmap memory. Rebuilt-path `tools/sweep_sysy_suite.py` reruns over `compiler2021/公开用例与运行时库/performance_test2021-public` no longer reproduce only the long-lived `03_sort2.sy` timeout; on the current tree they now reproduce `03_sort2.sy -> RUN_TIMEOUT` and `shuffle2.sy -> RUN_TIMEOUT` (`28/30` overall in the full public sweep, and the same pair on focused one-case reruns). In the same round I also tried one very narrow `machine_select` cleanup experiment around block-local reuse of repeated non-address-taken `load_local` results, but it was explicitly *not* kept: the representative `03_sort2` static witness stayed flat at `total_instructions=428`, while the public-perf surface did not improve and still carried the `shuffle2` timeout. The experiment has been backed out again, and post-backout spot checks keep `make test-machine-select` green plus `autotest -riscv -s lv9 /workspaces/compiler_lab` green (`22/22`). Current authority is therefore that the public runtime/perf line on the live tree is now `03_sort2 + shuffle2`, not only `03_sort2`, and that this specific selected-cleanup load-reuse direction is recorded as a failed/unkept trial rather than a new checkpoint.
- 2026-05-07: A later same-day selected-side retry also did **not** earn checkpoint status. I tried a more conservative `machine_select` cleanup that only removed repeated small pure-expression redefinitions when they rewrote the **same** result register/spill. That direction looked attractive because the live `shuffle2` selected dump still contained repeated `cnt*4`-style address-index expressions inside `insert(...)`. The practical result was not clean enough to keep: runtime behavior stayed unchanged on both public witnesses (`03_sort2.sy -> RUN_TIMEOUT`, `shuffle2.sy -> RUN_TIMEOUT` under the same `60s` single-case budget), while the broader tree stayed correctness-green (`make test-machine-select`, `make test-value-ssa-regression`, `make test-compiler-driver`, and `autotest -riscv -s lv9` all re-passed after the trial). Because that selected-side retry did not close a runtime tail or produce a clearly dominant public tradeoff, it has been backed out again. Current authority is therefore to continue from the earlier kept Value-SSA direct-cleanup checkpoint, not from this selected-side same-result-expression retry.
- 2026-05-06: A small but important post-experiment cleanup is now also landed on the Value-SSA side so the current tree better matches the recorded mainline policy. The classic lower-IR -> Value-SSA canonicalization bridge no longer runs `value_ssa_canonicalize_program(...)` twice in a row, and the previously tried but intentionally unpromoted redundant-pure-call elimination helpers have been removed again from `src/value_ssa_pass/value_ssa_cse.inc` instead of lingering as dead experimental scaffolding. Focused reruns keep both `make test-value-ssa-regression` and `make test-value-ssa-oracle` green, so current authority is that the failed default-path pure-call/CSE reopen is now not only “off the mainline in spirit” but also materially cleaned out of the live Value-SSA pass surface.
- 2026-05-06: The strict all-paths-return audit line has now closed one more real false-positive family instead of only rerunning old near-neighbors. In the representative shape `while(1){ if(a){return 1;} for(;a<2;a=a+1){ if(a){return 2;} } a=1; }`, the older nested-loop helper only treated inner-loop conditions as syntactic constant-true or constant-false tests, so it missed the already-known guard-state fact that `a<2` is definitely true on entry to the inner `for` and incorrectly rejected the function with `SEMA-CF-001`. The landed fix now lets that nested-loop proof path reuse the current guard-constant state before deciding whether the inner loop is definitely entered, and the new focused regression `CF-82` locks the repaired case in `tests/semantic/semantic_regression_scope_cf.inc`. `make test-semantic-regression` is green again after the change. Current authority is therefore that the strict-mode residual risk surface has narrowed a little further on the “guard flip plus nested loop plus next-iteration return” side, while the broader hidden/default audit line still remains open.
- 2026-05-06: That same strict audit line has now also widened from one isolated `CF-82` shape into a small locked subfamily instead of stopping at the first win. The current helper now also revalidates several nearby “guard becomes true before an inner loop that then returns” variants: representative `for(;1;a=1){ if(a){return 1;} while(a<2){ if(a){return 2;} } }`, `for(;1;a=1){ if(a){return 1;} for(;a<2;a=a+1){ if(a){return 2;} } }`, and `while(1){ if(a){return 1;} a=1; for(;a<2;a=a+1){ if(a){return 2;} } }` are now locked as `CF-83..CF-85` in `tests/semantic/semantic_regression_scope_cf.inc`, and `make test-semantic-regression` remains green after adding them. Current authority is still deliberately conservative: two deeper near-neighbor variants remain intentionally open for a later pass, namely the pure nested-`while` inner-return shape and the local-alias/declaration shape (`int b=a; b=1; for(;b<2;...)`), which suggests the next residual strict-mode work is less about plain current-state condition evaluation and more about deeper nested-body state propagation / alias-like declaration tracking.
- 2026-05-06: A follow-up probe immediately after that `CF-82..CF-85` closure made the remaining strict residual even more explicit. After rebuilding the real CLI and rechecking targeted repros, the “pure nested while” branch turned out to have already collapsed under the same earlier fix, so it should no longer be tracked as an open item. The currently reproduced strict-only residual is now narrower and cleaner: simple local-state-fed loop families such as `int f(){ int b=1; while(b<2){ if(b){ return 2; } } }`, `int f(){ int b=1; for(;b<2;b=b+1){ if(b){ return 2; } } }`, and the outer-loop alias-fed variant `int f(int a){ while(1){ if(a){ return 1; } int b=a; b=1; for(;b<2;b=b+1){ if(b){ return 2; } } } }` still reproduce `SEMA-CF-001` under `--enforce-all-paths-return-check`, while `make test-semantic-regression` remains green after the exploratory checks. Current authority is therefore that the next strict-mode slice should focus specifically on local declaration/assignment state feeding later loop conditions, not on reopening the already-closed outer-guard nested-loop family.
- 2026-05-06: One more follow-up on that same residual line clarified what *didn't* move yet, which is still useful execution memory for the next round. I tried a small state-aware must-return scaffold on compound statements and then rebuilt the real CLI before rechecking the targeted repros, but the observed behavior did not change: the current strict-mode residual still rejects the same pure local-state-fed loop family (`int b=1; while(b<2){...}`, `int b=1; for(;b<2;...){...}`, `int b=0; b=1; while(b<2){...}`, `int b=0; b=1; for(;b<2;...){...}`) plus the alias-fed outer-loop variant. Current authority is therefore sharper than before: the remaining strict issue is not just "nested loops with conditions" in general, and it is not solved by a shallow compound-level must-return helper alone; the next useful slice must explicitly address local declaration/assignment state as input to later loop-condition proofs.
- 2026-05-06: A final rebuild-and-rerun in this round changed the remaining boundary one more time in a way that matters for the next implementation slice. After rebuilding the real CLI and rechecking the same local-state-fed loop cases directly through `build/compiler --enforce-all-paths-return-check`, the alias-fed outer-loop variant stopped reproducing `SEMA-CF-001`, and the still-red simple local-state cases now fail one layer later with `IR-LOWER-009` / `LOWER-IR-042` ("function body does not terminate on all lowered paths"). Current authority is therefore that the next useful work on this family should move out of `src/semantic/semantic_core_flow.inc` and into the canonical-IR / lower-IR fallthrough analysis path (`src/ir/ir_lower_stmt.inc`, `src/ir/ir_global_dep.inc`, and the matching lowering-entry checks), because semantic analysis is already starting to accept more of these shapes while the downstream lowering-flow contract still lags behind.
- 2026-05-06: That IR-lowering residual has now materially advanced instead of remaining only a diagnosis. The canonical IR lowering path now has a first narrow local-state-aware truthiness slice for `while` / `for` termination shaping: scalar-local declarations with constant initializers now record current known values for lowering flow, direct scalar-local stores now update or clear that known-value metadata, and the `while` / `for` lowering flow checks now reuse that metadata when deciding whether a loop condition is already provably true/false. Focused CLI repros that previously failed at `IR-LOWER-009` now compile successfully under `--enforce-all-paths-return-check`, including `int b=1; while(b<2){ if(b){ return 2; } }`, `int b=1; for(;b<2;b=b+1){ if(b){ return 2; } }`, `int b=0; b=1; while(b<2){ if(b){ return 2; } }`, `int b=0; b=1; for(;b<2;b=b+1){ if(b){ return 2; } }`, and the alias-fed outer-loop variant. That same family is now also regression-locked at the user-facing entrypoint through `tests/compiler/compiler_driver_test.c` (`test_compiler_handles_strict_local_state_loop_returns`). Current authority is therefore that the old "simple local declaration/assignment state feeding later loop conditions" residual is no longer open on the CLI path, and the next remaining strict/default work should return to the still-open hidden/default compatibility line or any deeper canonical-IR / lower-IR cases that survive beyond this first scalar-local slice.
