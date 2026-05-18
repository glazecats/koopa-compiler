# General Optimization Passes Todo

## Scope

- This file tracks **general-purpose optimization passes** that are still
  missing, partial, or intentionally underpowered across the repository.
- It is deliberately broader than the current witness-driven
  `docs/OPTIMIZATION_PLAN.md`.
- Use this file when deciding whether the next optimization step should be:
  - a one-off hotspot pass for a measured testcase, or
  - a reusable pass with meaningful value across multiple workloads.

## Current Repository Baseline

- Canonical IR already has a small classic pass pipeline:
  - immediate binary folding
  - temp constant propagation
  - temp copy propagation
  - CFG simplification
  - dead temp-def elimination
- ValueSSA already has a broader but still mostly local/mid-strength pass set:
  - trivial simplification
  - local/global load forwarding
  - redundant/dead store cleanup
  - algebraic identity simplification
  - constant folding
  - redundant binary elimination
  - CFG simplification
  - dead value-def elimination
  - a growing family of hotspot-specific perf passes
- MemorySSA already has a meaningful memory-optimization surface:
  - load forwarding
  - store cleanup
  - local/global scalar replacement
  - memory CSE
  - memory-value bridge work
- Backend / machine layers currently have cleanup and canonicalization pieces,
  but not yet a mature late-optimization pipeline.

## Selection Rule

- Prefer a new **general pass** when all of the following are true:
  1. the same structural waste appears in more than one workload family
  2. the transform can be expressed without hard-coding one testcase/function
  3. the transform is easier to validate at one stable IR layer than via
     repeated backend/text patching
- Prefer a **hotspot pass** when the pattern is clearly real and important,
  but still too narrow or too semantics-sensitive to justify immediate
  generalization.

## Priority Bands

### Band A: Strong General-Purpose Candidates

1. Full LICM
   - What is missing:
     the tree currently has hotspot-specific loop-hoist logic, but not a
     proper general loop-invariant code motion pass driven by reusable loop
     discovery, dominance, and safety checks.
   - Why it matters:
     repeated loop-invariant address/load/bounds work is a direct pressure
     source in `fft`, `mm`, `spmv`, and some third-party compile-time-heavy
     kernels.
   - Best landing layer:
     `value_ssa_pass`, with optional later extension into `memory_ssa_pass`
     once alias-sensitive hoisting is needed.
   - Suggested first scope:
     start with pure `mov` / `binary` / `addr_*` / safe `load_*` hoisting,
     leaving call motion and alias-heavy indirect-load motion out of the
     first cut.

2. Induction-variable simplification and strength reduction
   - What is missing:
     there is no general pass that turns repeated
     `base + i*scale`, `row<<k`, `j<<2`, or carried add trees into explicit
     recurrent carriers.
   - Why it matters:
     this is one of the clearest cross-witness dynamic-hot-path gaps in the
     current tree, especially for `mm1/mm2/mm3`, `spmv*`, and parts of
     `fft*`.
   - Best landing layer:
     `value_ssa_pass`
   - Suggested first scope:
     integer induction variables with one loop header phi, affine recurrence,
     and constant stride/scale only.
   - Current 2026-05-18 status:
     a first narrow `ValueSSA` slice is now real code and regression-locked:
     the perf-hotspot path can already rewrite a minimal
     `i = phi(0, i+1)` plus `i << const` loop-carried shape into a derived
     carried phi with `+ (1 << const)` recurrence.
     This is intentionally only a first foothold, not yet a full
     address-carrier pass: current real-witness rereads show that it has not
     yet hit the hotter `mm1` / `spmv1` address trees, which still involve
     extra `add` composition around the scaled induction result.
     Current authority is therefore:
     keep this first slice as the opening implementation step, and continue by
     broadening it from plain induction-shift recurrence to affine
     `shl/mul + add` address-carrier recurrence rather than abandoning the
     general induction line.
   - Current 2026-05-18 follow-up:
     the first slice has now also been widened one safe step further:
     seed values no longer have to be literal immediates as long as they come
     from the loop preheader, and the transform now uses a simple use-count
     gate so it does not rewrite every light single-use `shl` it sees.
     Real `value-ssa-perf` rereads confirm that the pass now reaches genuine
     course witnesses instead of only the minimal regression case:
     it hits the `03_mm1` zero-fill / inner-`j` address carriers and the
     `09_spmv1` main update loops' carried `+4` index chains.
     The current limitation is now sharper too:
     the still-missing part is not "can it hit real code at all", but
     "can it keep the `mm1` gains while avoiding the remaining `spmv2/3`
     regressions and without over-widening into unstable affine rewrites."
   - Current later 2026-05-18 follow-up:
     the use-count gate has now been tightened again from "at least two uses"
     to "at least three uses", and the minimal regression witness was updated
     accordingly so it still models a meaningfully reused carried scale.
     Focused A/B after this tightening currently reads:
     `03_mm1` run ms `7727.860 -> 7152.769`,
     `09_spmv1` `12484.632 -> 12418.484`,
     `10_spmv2` `7343.576 -> 7429.339`,
     `11_spmv3` `9670.617 -> 9777.935`,
     `13_fft1` `7854.056 -> 7833.304`.
     Current authority is therefore:
     the reusable induction line is now clearly helping `mm1`, is roughly
     neutral/slightly positive on `spmv1`, but still not clean enough on
     `spmv2/3` to checkpoint. The next refinement should narrow by use-role /
     structure, not just by another blind numeric threshold.
   - Current later same-day follow-up:
     after the first use-role/address-chain gate landed, a fresh focused A/B
     still shows the same broad direction:
     `03_mm1` remains strongly positive,
     `09_spmv1` stays near-flat/slightly positive,
     but `10_spmv2` / `11_spmv3` still remain small negatives.
     Current authority is therefore even sharper:
     another blind threshold tweak is unlikely to be the final answer.
     The next useful refinement should distinguish the "good `mm1`-style
     inner address carrier" from the still-harmful `spmv2/3` candidates by
     a stronger structural rule than raw use count alone.
   - Current later same-day follow-up:
     a stronger structure/use-role gate is now in place, and rebuilt
     `value-ssa-perf` rereads show the intended qualitative shape:
     the `spmv1/2/3` light candidates are back to the baseline-style shape,
     while the useful `mm1` inner `+4` carrier still survives.
     Focused A/B after that gate currently reads:
     `03_mm1` run ms `7843.760 -> 7029.414`,
     `09_spmv1` `12588.526 -> 12453.328`,
     `10_spmv2` `7276.768 -> 7328.863`,
     `11_spmv3` `9817.794 -> 9732.411`,
     `13_fft1` `7789.258 -> 7890.946`.
     Current authority is therefore:
     this reusable induction line is now much closer to a checkpoint
     candidate. It still needs one more confirmation rerun before commit,
     especially because compile time remains materially higher and `fft1`
     drifted slightly negative on this sample, but the old `spmv2/3` tail is
     no longer the primary blocker.
   - Current later same-day confirmation rerun:
     the confirmation rerun with
     `03_mm1`, `09_spmv1`, `10_spmv2`, `11_spmv3`, `13_fft1`, and `14_fft2`
     now reads:
     `03_mm1` strongly positive,
     `09_spmv1` slightly negative,
     `10_spmv2` slightly negative,
     `11_spmv3` slightly positive,
     `13_fft1` roughly flat/slightly negative,
     `14_fft2` slightly positive.
     The important new structural note is that rebuilt `value-ssa-perf`
     rereads now show `09/10/11_spmv*` back at a baseline-style shape, so the
     remaining small runtime drift there is more likely to be noise or a
     second-order backend effect than a large remaining mid-IR misfire.
     Current authority is therefore:
     this reusable induction line is now best treated as a near-checkpoint
     candidate whose next decision should come from one more deliberate
     keep/backout judgment, not from reopening another large transform family
     immediately.
   - Current later same-day wider confirmation:
     one wider same-environment guard sample over
     `03_mm1`, `09_spmv1`, `13_fft1`,
     `18_brainfuck-bootstrap`, and `19_brainfuck-calculator`
     plus one reversed-order confirmation over
     `14_fft2`, `13_fft1`, `11_spmv3`, `10_spmv2`, `09_spmv1`, and `03_mm1`
     now sharpens the verdict:
     the line remains strongly positive on `mm1`, sometimes positive on
     `spmv2/3` or `brainfuck-calculator`, but it is not stable enough across
     `spmv1` and `fft1/2`, and compile time is consistently higher.
     Current authority is therefore:
     do **not** checkpoint this reusable induction line in its current form.
     Keep the code if more near-term refinement is planned immediately;
     otherwise treat it as a valuable branch of evidence rather than a stable
     checkpoint candidate.

3. Real global value numbering / cross-block CSE
   - What is missing:
     current redundant-binary cleanup is useful but still not a full
     dominance-aware GVN pass.
   - Why it matters:
     many present wins are being recovered via narrow repeated-expression or
     repeated-call special cases that a stronger generic CSE/GVN framework
     could subsume.
   - Best landing layer:
     `value_ssa_pass`
   - Suggested first scope:
     pure scalar values only, no memory-value unification in the first cut.

4. SCCP
   - What is missing:
     the tree folds constants and simplifies CFG, but does not yet have a
     sparse conditional constant propagation pass that jointly reasons about
     lattice values and executable edges.
   - Why it matters:
     SCCP is often a compact force multiplier for later DCE / CFG cleanup and
     can expose simplifications that local folding cannot reach.
   - Best landing layer:
     `value_ssa_pass`
   - Suggested first scope:
     integer immediates, branch reachability, and simple phi lattices.

5. Tiny-function inlining
   - What is missing:
     there is no general small-helper inliner.
   - Why it matters:
     some current hotspots, especially `fft1/fft2`, are still paying for
     recursive or helper-driven structure that blocks later simplification.
   - Best landing layer:
     likely canonical IR or lower IR for a first simple inliner, though a
     ValueSSA-side version is also possible.
   - Suggested first scope:
     internal-only, small-body, no-varargs, one-entry helpers with a strict
     code-growth budget.
   - Current 2026-05-18 structure follow-up:
     recursive/helper-oriented implementation is now being given its own
     `ValueSSA` pass file
     (`src/value_ssa_perf/value_ssa_recursive_helper.inc`) rather than being
     left mixed into the generic perf-hotspot file.
     Current authority is to continue helper-specific evolution there
     if this line broadens into multi-block helper inlining or more
     structured recursive-helper rewrites.

### Band B: High-Value but More Delicate

6. Tail-recursion elimination
   - What is missing:
     a general pass for self tail-calls is not present.
   - Why it matters:
     useful in principle, but current known `fft` recursive helpers are not
     plain tail recursion, so this is not the first answer for the current
     hottest witnesses.
   - Best landing layer:
     canonical IR or lower IR before the later SSA pipeline.

7. Patterned recursion-to-iteration rewrites
   - What is missing:
     no structured pass exists for binary-recursive helper families such as
     exponentiation-by-squaring or Russian-peasant multiply.
   - Why it matters:
     this could be extremely valuable for `fft1/fft2` if done correctly.
   - Why it is not Band A yet:
     it is closer to a semantics-aware transform family than a classic
     widely-applicable pass, so it should probably follow, not precede,
     stronger generic loop/value passes.
   - Current 2026-05-18 note:
     `fft1/2` still show a concrete recursive-helper opportunity in
     `multiply()` / `power()`. A fresh reread confirms that the current tree
     already benefits from several smaller helper-side optimizations
     (`div/mod 2 -> shr/and`, repeated `power(...)` reuse, and
     `power(d, mod-2)` hoisting), but the helper bodies themselves still keep
     the recursive structure.
     A brief same-day retry to widen the final-text tail-call peephole from
     `jal ra, target` wrappers to plain `call target` wrappers was **not
     kept**: it reopened unrelated `compiler_driver` regression surfaces and
     therefore is not a safe first landing point for this line.
     Current authority is therefore:
     the recursive-helper line remains promising, but it should reopen at a
     more semantic layer (canonical IR / lower IR / ValueSSA) rather than by
     broadening the current final-text peephole blindly.

8. Memory PRE / stronger memory redundancy elimination
   - What is missing:
     MemorySSA has useful cleanup already, but not a broader partial
     redundancy elimination or stronger alias-aware memory motion layer.
   - Why it matters:
     it could subsume some current ad hoc repeated-load work later.
   - Best landing layer:
     `memory_ssa_pass`

9. Loop unswitch / loop peeling / bounded unrolling
   - What is missing:
     only witness-driven ideas exist; there is no reusable framework.
   - Why it matters:
     can be strong on carefully chosen loops, but code growth risk is real.
   - Suggested policy:
     do after LICM + induction + GVN, not before.

### Band C: Later or Backend-Oriented

10. Late machine copy propagation and dead-copy cleanup
    - Current status:
      bits of cleanup exist, but not a clear standalone late machine pass
      pipeline.
    - Why it matters:
      useful once instruction selection and legalization are more settled.

11. Late machine peephole framework
    - Current status:
      there are already some text/backend cleanup lines, but not a clean,
      staged, target-aware late peephole framework.
    - Why it matters:
      still worthwhile, but less important than upstream structural wins for
      the current workload mix.

12. Branch-layout / fallthrough optimization
    - Current status:
      some control cleanup exists, but not a dedicated branch-layout pass.
    - Why it matters:
      likely secondary for current OJ pressure compared with SSA-side hot-path
      simplification.

## Recommended Implementation Order

1. Full LICM
2. Induction / strength-reduction
3. Global value numbering / cross-block CSE
4. SCCP
5. Tiny-function inlining
6. Tail-recursion elimination
7. Patterned recursion-to-iteration rewrites
8. Stronger MemorySSA redundancy elimination
9. Loop peeling / unswitch / bounded unrolling
10. Late machine copy/peephole/layout cleanup

## Current OJ-Oriented Recommendation

- For the current repository state and witness mix, the strongest general-pass
  line is:
  1. LICM
  2. induction-strength-reduction
  3. GVN/CSE
- For `fft1/fft2`, the most interesting later generalization beyond those is:
  - tiny-function inlining
  - patterned recursion-to-iteration rewrites for recursive helper families
- For `mm1/mm2/mm3`, the strongest general answer is still induction-strength
  reduction rather than more backend peepholes.
- For compile-time-heavy third-party cases, a stronger generic pass portfolio
  should also reduce the need for repeated hotspot-only transforms, but pass
  compile cost must be watched carefully when landing any Band A transform.

## Non-Goals For The First Cut

- Do not start by building a giant all-purpose pass manager redesign.
- Do not try to solve arbitrary recursion-to-iteration before proving value on
  tail recursion or one narrow recursive family.
- Do not prioritize backend peepholes ahead of SSA-side structural passes
  unless a concrete measured witness proves otherwise.
