# AGENTS

## Startup

- Before starting work, read:
  1. `AGENTS.md`
  2. `docs/ENGINEERING_MEMORY.md`
  3. `docs/NEXT_STEPS.md`
- If the active target is the current optimization-focused round, also read:
  4. `docs/OPTIMIZATION_PLAN.md`
- If the active target is the current optimization-focused round and you are
  deciding whether to build a reusable pass rather than another
  witness-specific transform, also read:
  5. `docs/GENERAL_OPT_PASSES_TODO.md`
- If the active target is the hidden runtime-RE / generated-program crash
  line, also read:
  4. `docs/RUNTIME_RE_SEGFAULT_PLAN.md`
- Read deeper stage plans only when the task actually touches that layer:
  - lower IR / downstream IR:
    `docs/ir/LOWER_IR_DESIGN.md`
  - SSA / allocator / memory SSA:
    relevant files under `docs/ssa/`
  - backend machine stages:
    relevant files under `docs/backend/`

## Scope

- Default role in this thread is **Implementation Agent**.
- Own compiler implementation work under:
  - `src/`
  - `include/`
  - `tests/`
  - `docs/`
- Do not modify `lesson/` unless the user explicitly says so.
- Prefer placing new implementation-facing design or planning notes under
  `docs/`.

## Shared Authority

- `AGENTS.md` defines startup order, work rules, and scope boundaries.
- `docs/ENGINEERING_MEMORY.md` is working memory for engineering facts and safety
  notes.
- `docs/NEXT_STEPS.md` is the main roadmap and execution-log authority.
- `docs/OPTIMIZATION_PLAN.md` is the compact active-plan authority for the
  current optimization-focused round.
- `docs/GENERAL_OPT_PASSES_TODO.md` is the reusable-pass backlog and
  prioritization note for missing general-purpose optimization passes.
- `docs/RUNTIME_RE_SEGFAULT_PLAN.md` is the compact active-plan authority for
  the current hidden runtime-RE line.
- `docs/language/DEFER_FEATURE_PLAN.md` is the compact active-plan authority
  for the current language-feature round when `defer` is the active target.
- Stage-specific design docs under `docs/ir/`, `docs/ssa/`, and
  `docs/backend/` are authoritative only when that layer is active.

## Current Stage

- Unless the user explicitly redirects the thread, the current default stage is
  the **language-feature round**.
- In practice this means:
  - keep correctness-closed surfaces green
  - treat `docs/language/DEFER_FEATURE_PLAN.md` as the compact active-plan
    authority while `defer` is the active feature target
  - prefer parser / semantic / lowering changes that can be regression-locked
    incrementally
  - treat optimization work as paused unless the user explicitly reopens it
  - treat backend stage-expansion, allocator redesign, and hidden runtime-RE
    investigation as secondary unless the user explicitly reopens one of those
    lines

## Optimization Rules

- For the current optimization line, the primary goal is **OJ runtime
  reduction**.
- Static code size, instruction counts, and prettier asm are only secondary
  diagnostics.
- For course perf work, treat `/opt/bin/testcases/perf` with `-perf` as the
  default local witness surface unless the user explicitly says otherwise.
- If a testcase has a real `.in` file, use the real input. Do **not** use an
  empty-stdin qemu run as evidence for keep/backout decisions.
- Prefer timing with millisecond precision or better.
- Do proper A/B when deciding whether to **keep** an optimization candidate or
  commit a performance change. Do not rely on memory.
- A/B is a **checkpoint gate**, not a requirement after every intermediate
  implementation turn. While a new pass is still incomplete, it is acceptable
  to spend several rounds on:
  - correctness rechecks
  - dump / asm shape inspection
  - compile-time sanity checks
  - small focused witness probes
  before running a formal baseline-vs-candidate A/B.
- Once a pass or optimization candidate reaches a coherent "could plausibly be
  kept" state, run formal A/B before treating it as a kept result.
- If the first formal A/B result is small or mixed, rerun before deciding.
- Compare with `clang -O2/-O3` when useful, but use it as structural
  inspiration, not as automatic proof that a transform belongs here.
- Prefer a **general-purpose pass** when:
  1. the same structural waste appears across multiple witnesses
  2. the transform can be expressed cleanly at one IR layer
  3. the pass is easier to validate than repeated one-off rewrites
- Prefer a **hotspot-specific transform** when the opportunity is real but
  still too narrow or too semantics-sensitive to justify immediate
  generalization.
- Current high-value missing general pass families are:
  - LICM
  - induction-variable simplification / strength reduction
  - global value numbering / cross-block CSE
  - SCCP
  - tiny-function inlining
- Recursive-transform rule:
  - tail-recursion elimination is a real general-pass candidate
  - arbitrary recursion-to-iteration is not the default first choice
  - patterned recursion-to-iteration rewrites for specific helper families are
    in scope later, but should usually follow the stronger generic passes
    above
- Compile-time regressions matter too. Do not casually keep a runtime
  optimization if it materially worsens compile time unless the runtime win is
  large enough to justify it.

## Runtime-RE Rules

- In this thread, hidden/course `RE` means the **generated program/runtime**
  failed on the judge, not merely that the compiler process crashed.
- Keep multiple hypotheses open:
  - control-flow corruption
  - dead loop / non-termination
  - bad call/return lowering
  - stack/save-restore corruption
  - invalid address formation / invalid memory access
  - unsafe backend or text peepholes
- Do not dismiss small-source `RUN_TIMEOUT` cases as “just slow” without
  checking whether they look semantically plausible.

## Progress Memory

- When a substantial work chunk changes the real roadmap position, update
  `docs/NEXT_STEPS.md` before finishing the chunk.
- Keep `docs/OPTIMIZATION_PLAN.md` synchronized too when the active line is
  the optimization round.
- If reusable-pass priorities change, update
  `docs/GENERAL_OPT_PASSES_TODO.md`.
- At the end of each conversation, report:
  - which workstreams are active
  - rough percentages
  - what is planned next

## Correctness Rechecks

- Treat correctness rechecks as required workflow, not optional cleanup.
- On an active optimization line, recheck after every `1-3` substantial edit
  chunks, and always again before treating a change as a kept checkpoint.
- Rotate testcase samples instead of reusing the exact same tiny batch every
  round.
- Prefer mixing:
  - course-facing surfaces such as `lv8`, `lv9`, and focused `-perf`
    witnesses
  - external surfaces when locally available

## Long-Running Commands

- Treat long-running commands as stateful work, not disposable probes.
- Prefer letting an already-started long command finish instead of repeatedly
  restarting it.
- If a long command must be interrupted, say so explicitly and record why.
- When a long command remains relevant at the end of a work chunk, record:
  - what is still running or still needs completion
  - whether the next turn should continue waiting or rerun for a concrete
    reason
- Prefer writing long sweep output to a log file when direct polling would be
  awkward.

## Versioning Rules

- Do not commit known-broken code.
- Do not commit on vague memory that “it felt faster”; use measured A/B.
- Formal A/B is required before commit for performance-motivated changes, but
  not after every intermediate development round while the pass is still under
  construction.
- Docs-only updates do not need a commit.
- When a candidate optimization is unstable or clearly unhelpful, revert it
  promptly instead of letting it silently pollute the working tree.
