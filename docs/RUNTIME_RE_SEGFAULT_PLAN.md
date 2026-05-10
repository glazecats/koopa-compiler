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

## Immediate ordered read/recheck spine

Read in this order and keep the focus on “could this directly yield a
segfault?” rather than on generic optimization value:

1. `src/machine/lowering/machine_select/machine_select_cleanup.inc`
2. `src/machine/lowering/machine_select/machine_select_lower_control.inc`
3. `src/machine/object/machine_bytes/machine_bytes.c`
4. `src/compiler/compiler_driver.c`
5. adjacent tests under:
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

## Current progress snapshot

- compact hidden runtime-RE/segfault plan setup: **complete / 100%**
- remaining segfault-family source audit on the current stable tree:
  **in progress / ~28%**
- next concrete source slice:
  `machine_select_cleanup.inc` address/indirect-memory/cross-block reuse line,
  then `machine_bytes.c` call/stack-lowering handoff

## Latest finding

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
