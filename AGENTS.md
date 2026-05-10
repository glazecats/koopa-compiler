# AGENTS

## Startup Contract

- Before starting work, read `AGENTS.md`.
- Then read `docs/ir-conventions.md`.
- Then read `docs/NEXT_STEPS.md`.
- If the active target in this thread is the still-open hidden runtime
  **RE/segmentation-fault** line, then also read
  `docs/RUNTIME_RE_SEGFAULT_PLAN.md` before making changes. Treat that file
  as the compact active-plan authority for this specific line when
  `docs/NEXT_STEPS.md` has grown too large for efficient continuation.
- If the task touches lower IR / downstream IR planning or implementation, then also read `docs/ir/LOWER_IR_DESIGN.md` before making changes.
- If the task touches SSA / allocator / machine-register-model planning or implementation, then also read the relevant authority under `docs/ssa/`:
  - `VALUE_SSA_DESIGN.md`
  - `VALUE_SSA_ALLOCATOR_PLAN.md`
  - `VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md`
  - `MEMORY_SSA_DESIGN.md`
- If the task touches backend machine-stage planning or implementation, then also read the relevant stage authority under `docs/backend/`:
  - `MACHINE_IR_PLAN.md`
  - `MACHINE_SELECT_PLAN.md`
  - `MACHINE_LAYOUT_PLAN.md`
  - `MACHINE_EMIT_PLAN.md`
  - `MACHINE_ENCODE_PLAN.md`
  - `MACHINE_BYTES_PLAN.md`
  - `MACHINE_OBJECT_PLAN.md`
  - `MACHINE_RELOC_PLAN.md`
  - `MACHINE_CONTAINER_PLAN.md`
  - `MACHINE_ELF_PLAN.md`
  - `MACHINE_IMAGE_PLAN.md`
  - `MACHINE_EXEC_PLAN.md`
  - `MACHINE_LOAD_PLAN.md`
  - `MACHINE_RUNTIME_PLAN.md`
  - `MACHINE_LAUNCH_PLAN.md`
  - `MACHINE_STEP_PLAN.md`
  - `MACHINE_DECODE_PLAN.md`
  - `MACHINE_PAYLOAD_DECODE_PLAN.md`
  - `MACHINE_INTERP_PLAN.md`
  - `MACHINE_TRANSITION_PLAN.md`
  - `MACHINE_STATE_PLAN.md`
  - `MACHINE_MUTATION_PLAN.md`
  - `MACHINE_WRITEBACK_PLAN.md`
  - `MACHINE_COMMIT_PLAN.md`
  - `MACHINE_APPLY_PLAN.md`
  - `MACHINE_OBSERVE_PLAN.md`
  - `MACHINE_DELTA_PLAN.md`
  - `MACHINE_TRACE_PLAN.md`
  - `MACHINE_EVENT_PLAN.md`
  - `MACHINE_OUTCOME_PLAN.md`
  - `MACHINE_HISTORY_PLAN.md`
  - `MACHINE_TIMELINE_PLAN.md`
  - `MACHINE_LOG_PLAN.md`
  - `MACHINE_JOURNAL_PLAN.md`

## Quick Read Checklist

- Default restart reading order:
  1. `AGENTS.md`
  2. `docs/ir-conventions.md`
  3. `docs/NEXT_STEPS.md`
- If the active line is the hidden runtime-RE / generated-program segfault
  hunt, then read `docs/RUNTIME_RE_SEGFAULT_PLAN.md` immediately after
  `docs/NEXT_STEPS.md` and use it as the compact continuation file for that
  line.
- If the user explicitly asks for a fresh whole-project reread / restart from
  the beginning, treat that as a **sequential source audit mode** and restart
  from the front of the implementation pipeline instead of jumping only to the
  currently suspicious layer:
  1. `src/lexer/lexer.c`
  2. `src/parser/*`
  3. `src/semantic/*`
  4. `src/ir/*`
  5. `src/ir_pass/*`
  6. `src/lower_ir/*`
  7. `src/value_ssa/*` and `src/value_ssa_pass/*`
  8. `src/memory_ssa/*` and `src/memory_ssa_pass/*`
  9. downstream `src/machine/*`
- If the user says to read/check **all code**, **the whole repository**, or
  says not to only inspect the main/framework path, then sequential source
  audit mode must be interpreted strictly:
  - read **all implementation files under `src/`**, not only the obvious
    entrypoints or the currently suspicious mainline
  - every pass-layer directory under `src/` is mandatory, including
    `src/ir_pass/*`, `src/value_ssa_pass/*`, `src/memory_ssa_pass/*`, and
    any other `*pass*` implementation sibling directories that exist in the
    source tree
  - do not stop at representative files such as only `*.c` aggregators or one
    “core” `*.inc`; sweep the sibling implementation files too
  - use the pipeline order only as the traversal spine; within each stage,
    widen the audit to adjacent/support modules before declaring the stage
    covered
  - when reporting progress, distinguish between “mainline read” and
    “whole-stage/all-files read” instead of treating them as the same thing
- Then read only the authority that matches the task:
  - lower / downstream IR -> `docs/ir/LOWER_IR_DESIGN.md`
  - SSA / allocator -> relevant `docs/ssa/*`
  - backend machine stage -> relevant `docs/backend/*`
- Default role in this thread is **Implementation Agent**.
- Do not modify `lesson/` unless the user explicitly says so.
- Before ending a work chunk, always check:
  - progress memory is updated in `docs/NEXT_STEPS.md`
  - the user gets active blocks plus rough percentages
  - long-running command state is explicitly recorded
  - correctness recheck cadence has not been silently skipped
  - if sequential source audit mode is active, the latest audited file range /
    stage boundary is written into `docs/NEXT_STEPS.md` as progress memory

## Shared Authority

- `AGENTS.md` defines agent roles, startup rules, and scope boundaries.
- `docs/ir-conventions.md` is working memory for current engineering facts and safety/convention notes.
- `docs/NEXT_STEPS.md` is the roadmap, execution log, and current-plan authority.
- `docs/RUNTIME_RE_SEGFAULT_PLAN.md` is the compact active-plan authority for
  the current hidden runtime-RE / generated-program segfault investigation
  line. Keep it in sync with `docs/NEXT_STEPS.md` when this line is active.
- `docs/ir/LOWER_IR_DESIGN.md` is the current design authority for lower-memory / downstream IR planning unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/ssa/` holds the current SSA-side design authorities:
  - `VALUE_SSA_DESIGN.md` for the next planned post-lower-IR step
  - `VALUE_SSA_ALLOCATOR_PLAN.md` for allocator planning/staging
  - `VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md` for allocator-to-machine projection and machine-register-model work
  - `MEMORY_SSA_DESIGN.md` for memory-SSA work
- `docs/backend/` holds the backend stage authorities from `MACHINE_IR_PLAN.md` through `MACHINE_JOURNAL_PLAN.md`; use the stage that matches the active machine layer the task touches.
- `docs/` holds implementation-facing design notes and proposals; unless explicitly promoted by `NEXT_STEPS.md`, treat them as discussion/design authority rather than execution-log authority.

## Runtime-RE Interpretation

- In this thread, when the user says hidden/course **`RE`**, interpret it as:
  **the generated program/runtime crashed or otherwise failed while running
  the testcase on the judge**, not merely that the compiler process itself
  crashed.
- Therefore, when the active target is `RE`, the primary audit surface should
  be wrong-code / runtime-risk causes such as:
  - wrong control flow / wrong branch polarity / wrong jump target
  - dead loops or unintended non-termination
  - broken call/return lowering
  - stack-frame / save-restore / calling-convention corruption
  - bad address formation / invalid memory access in generated code
  - unsafe backend or final-text peepholes that mutate correct code into
    crashing code
- Compiler self-crash, malformed internal metadata, or report/query helper
  segfaults are still worth fixing as robustness issues, but they must **not**
  be mistaken for the main hidden-`RE` explanation unless the user explicitly
  changes the target error class.
- Thread-specific runtime-RE memory:
  - the **primary target** in this thread is runtime `RE` in the generated
    testcase program, not compiler-process `RE`
  - user reminder: do **not** collapse runtime `RE` to only one cause such as
    dead loop; dead loop / non-termination is only one important subclass
  - user reminder: runtime `RE` analysis should keep multiple active
    hypotheses open, including control-flow corruption, bad return/call
    lowering, stack/save-restore mistakes, broken address formation, and
    invalid memory accesses in generated code
  - thread memory from the user: the earlier generated-program **`RTLE`**
    mainline is believed to be already fixed in a previous session, but this
    should be treated as a working memory note rather than a proof that all
    loop/control-flow risk is gone; runtime `RE` may still come from a dead
    loop or adjacent wrong-code in the same family
  - thread memory from the user: some testcase red points are already-known
    preexisting `RUN_TIMEOUT` tails and may **not** be caused by the current
    code edit. Before blaming a new patch, consult `docs/NEXT_STEPS.md` (and,
    if needed, the prior session history) for the currently remembered
    baseline timeout set
  - currently remembered stable public timeout examples include at least
    `03_sort2.sy -> RUN_TIMEOUT` and `shuffle2.sy -> RUN_TIMEOUT`; additional
    external/private perf timeout clusters are tracked in `docs/NEXT_STEPS.md`
    and should be treated as historical baseline pressure unless a new change
    clearly broadens or shifts that set
  - when restarting from scratch after any misunderstanding of `RE`,
    reinterpret earlier compiler-crash fixes as useful robustness work, but
    do not let them substitute for a fresh generated-program runtime audit

## Agent Roles

### Implementation Agent

- This agent is the default code-changing agent for the current workstream.
- Owns compiler implementation work under `src/`, `include/`, `tests/`, `docs/`, and implementation-facing project notes.
- May update `docs/NEXT_STEPS.md`, `docs/ir-conventions.md`, and implementation-facing design documents such as `docs/ir/LOWER_IR_DESIGN.md` and `docs/ssa/VALUE_SSA_DESIGN.md` when implementation work changes roadmap facts, working memory, or downstream-IR design direction.
- Must not modify files under `lesson/` unless the user explicitly says otherwise.

### Lesson Agent

- Owns files under `lesson/`.
- Must not modify compiler implementation files under `src/`, `include/`, or `tests/` unless the user explicitly says otherwise.

### Review Agent

- Owns review-only work across the repository.
- Default behavior is read, analyze, and report findings rather than edit files.
- Focus review on bugs, soundness risks, regressions, and missing tests.
- Must not make code changes unless the user explicitly asks for fixes.

## Coordination Rules

- Prefer keeping each agent inside its owned area.
- If a task spans implementation and lesson content, split ownership by directory.
- Prefer placing new implementation design/proposal documents under `docs/` instead of the repository root.
- If review finds issues outside its owned scope, report them rather than editing.
- Do not overwrite, revert, or refactor another agent's owned area without explicit user approval.
- Repository sync memory for this thread:
  - when creating a kept checkpoint commit for the active workstream, prefer remembering that the repository has **two** intended remotes: GitHub (`origin`) and GitLab (`gitlab`)
  - after a commit that the user expects to preserve/share, do not stop at only one remote by habit; unless the user explicitly says otherwise, treat the default push checklist as:
    1. `git push origin <branch>`
    2. `git push gitlab <branch>`
  - do not assume one forge auto-syncs from the other; GitHub and GitLab should be treated as independently updated remotes unless an explicit mirroring setup is confirmed later

## Progress Reporting

- At the end of every conversation, explicitly tell the user which active
  implementation blocks/workstreams are currently being advanced or planned
  next, and include an approximate percentage for each one.
- Treat progress-memory upkeep as part of the implementation task itself, not
  as optional narration. When a substantial work chunk changes the real
  roadmap position, update `docs/NEXT_STEPS.md` before finishing the chunk.
- In particular, keep `docs/NEXT_STEPS.md`'s `Current Active Slice` section
  current as the default top-level progress snapshot, rather than letting it
  lag behind conversational status updates.
- Also keep the currently relevant referenced progress document in sync when
  it is the canonical deeper status source for the active line (for example a
  stage plan under `docs/backend/` or `docs/ssa/`). Do not update only one of
  `Current Active Slice` or the referenced plan if both are being used as the
  active progress memory for the same workstream.
- In sequential source audit mode, every substantial reread chunk should leave
  a short log entry in `docs/NEXT_STEPS.md` recording:
  - which source files/stage span were reread
  - whether a concrete bug was found
  - whether any long-running sweep is still in flight
- In strict whole-code audit mode, the log should also say whether the chunk
  covered only the stage spine or the full set of sibling implementation files
  for that stage, so a later restart does not mistake partial coverage for a
  full-module read.
- When reporting implementation progress at the end of a work chunk, prefer locating the work inside the current roadmap/staging document rather than giving only a generic status line.
- For staged workstreams such as Value-SSA allocator mainline, report progress in layered form when possible, for example:
  - which named stage/substage is complete
  - which named stage/substage is currently being advanced
  - which next named stage/substage has not started yet
- When the user asks "做到哪了" / "现在在哪", prefer giving an approximate position inside the active plan plus a rough percentage for:
  - the current substage
  - the larger active stage
  - optionally the overall roadmap when that estimate is still meaningful
- If the active roadmap/staging authority already contains an explicit progress snapshot or percentage markers, prefer reusing and updating those documented numbers instead of improvising fresh percentages only from conversational memory.
- For allocator work in particular, prefer keeping the current percentages and active-line status in `docs/ssa/VALUE_SSA_ALLOCATOR_PLAN.md`, and treat that snapshot as the default source when reporting allocator position.
- For backend work such as `machine_select`, `machine_layout`, `machine_emit`, `machine_encode`, `machine_bytes`, `machine_object`, `machine_reloc`, `machine_container`, `machine_elf`, `machine_image`, `machine_exec`, `machine_load`, `machine_runtime`, `machine_launch`, `machine_step`, `machine_decode`, `machine_payload_decode`, `machine_interp`, `machine_transition`, `machine_state`, `machine_mutation`, `machine_writeback`, `machine_commit`, `machine_apply`, `machine_observe`, `machine_delta`, `machine_trace`, `machine_event`, `machine_outcome`, `machine_history`, `machine_timeline`, `machine_log`, or `machine_journal`, prefer keeping the current percentages and active-line status in `docs/backend/MACHINE_SELECT_PLAN.md`, `docs/backend/MACHINE_LAYOUT_PLAN.md`, `docs/backend/MACHINE_EMIT_PLAN.md`, `docs/backend/MACHINE_ENCODE_PLAN.md`, `docs/backend/MACHINE_BYTES_PLAN.md`, `docs/backend/MACHINE_OBJECT_PLAN.md`, `docs/backend/MACHINE_RELOC_PLAN.md`, `docs/backend/MACHINE_CONTAINER_PLAN.md`, `docs/backend/MACHINE_ELF_PLAN.md`, `docs/backend/MACHINE_IMAGE_PLAN.md`, `docs/backend/MACHINE_EXEC_PLAN.md`, `docs/backend/MACHINE_LOAD_PLAN.md`, `docs/backend/MACHINE_RUNTIME_PLAN.md`, `docs/backend/MACHINE_LAUNCH_PLAN.md`, `docs/backend/MACHINE_STEP_PLAN.md`, `docs/backend/MACHINE_DECODE_PLAN.md`, `docs/backend/MACHINE_PAYLOAD_DECODE_PLAN.md`, `docs/backend/MACHINE_INTERP_PLAN.md`, `docs/backend/MACHINE_TRANSITION_PLAN.md`, `docs/backend/MACHINE_STATE_PLAN.md`, `docs/backend/MACHINE_MUTATION_PLAN.md`, `docs/backend/MACHINE_WRITEBACK_PLAN.md`, `docs/backend/MACHINE_COMMIT_PLAN.md`, `docs/backend/MACHINE_APPLY_PLAN.md`, `docs/backend/MACHINE_OBSERVE_PLAN.md`, `docs/backend/MACHINE_DELTA_PLAN.md`, `docs/backend/MACHINE_TRACE_PLAN.md`, `docs/backend/MACHINE_EVENT_PLAN.md`, `docs/backend/MACHINE_OUTCOME_PLAN.md`, `docs/backend/MACHINE_HISTORY_PLAN.md`, `docs/backend/MACHINE_TIMELINE_PLAN.md`, `docs/backend/MACHINE_LOG_PLAN.md`, or `docs/backend/MACHINE_JOURNAL_PLAN.md`, and treat the relevant plan as the default source when reporting backend position.
- When the backend progress reference has already advanced past the load-prep side, prefer the `MRT1`-`MRT3` snapshot in `docs/backend/MACHINE_RUNTIME_PLAN.md` over older `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- If the backend has already advanced through several sibling stages and the furthest downstream line is now checkpoint-ready / maintenance-first, report that furthest downstream snapshot directly instead of falling back to older stage names just because they were active earlier in the project.
- When a backend line is transitioning from one sibling stage to the next, prefer saying both:
  - which earlier stage is effectively checkpointed / maintenance-first
  - which next sibling stage is now the active mainline
- When the backend progress reference has already advanced past the exec-prep side, prefer the `MLD1`-`MLD3` snapshot in `docs/backend/MACHINE_LOAD_PLAN.md` over older `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the image-prep side, prefer the `MEX1`-`MEX3` snapshot in `docs/backend/MACHINE_EXEC_PLAN.md` over older `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced to the image-prep side, prefer the `MI1`-`MI3` snapshot in `docs/backend/MACHINE_IMAGE_PLAN.md` over older `MELF*`, `MS*`, or `ML*` names unless the work actually reopened those earlier layers.
- When the backend progress reference has already advanced past the transition side, prefer the `MSTA1`-`MSTA3` snapshot in `docs/backend/MACHINE_STATE_PLAN.md` over older `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the applied-state side, prefer the `MMUT1`-`MMUT3` snapshot in `docs/backend/MACHINE_MUTATION_PLAN.md` over older `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the deferred-mutation side, prefer the `MWB1`-`MWB3` snapshot in `docs/backend/MACHINE_WRITEBACK_PLAN.md` over older `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the writeback side, prefer the `MCOM1`-`MCOM3` snapshot in `docs/backend/MACHINE_COMMIT_PLAN.md` over older `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the commit side, prefer the `MAP1`-`MAP3` snapshot in `docs/backend/MACHINE_APPLY_PLAN.md` over older `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the apply side, prefer the `MOB1`-`MOB3` snapshot in `docs/backend/MACHINE_OBSERVE_PLAN.md` over older `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the observe side, prefer the `MDEL1`-`MDEL3` snapshot in `docs/backend/MACHINE_DELTA_PLAN.md` over older `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the delta side, prefer the `MTR1`-`MTR3` snapshot in `docs/backend/MACHINE_TRACE_PLAN.md` over older `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the trace side, prefer the `MEV1`-`MEV3` snapshot in `docs/backend/MACHINE_EVENT_PLAN.md` over older `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the event side, prefer the `MOUT1`-`MOUT3` snapshot in `docs/backend/MACHINE_OUTCOME_PLAN.md` over older `MEV*`, `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the outcome side, prefer the `MHIS1`-`MHIS3` snapshot in `docs/backend/MACHINE_HISTORY_PLAN.md` over older `MOUT*`, `MEV*`, `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the history side, prefer the `MTIM1`-`MTIM3` snapshot in `docs/backend/MACHINE_TIMELINE_PLAN.md` over older `MHIS*`, `MOUT*`, `MEV*`, `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the timeline side, prefer the `MLOG1`-`MLOG3` snapshot in `docs/backend/MACHINE_LOG_PLAN.md` over older `MTIM*`, `MHIS*`, `MOUT*`, `MEV*`, `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- When the backend progress reference has already advanced past the log side, prefer the `MJR1`-`MJR3` snapshot in `docs/backend/MACHINE_JOURNAL_PLAN.md` over older `MLOG*`, `MTIM*`, `MHIS*`, `MOUT*`, `MEV*`, `MTR*`, `MDEL*`, `MOB*`, `MAP*`, `MCOM*`, `MWB*`, `MMUT*`, `MSTA*`, `MTN*`, `MRT*`, `MLD*`, `MEX*`, `MI*`, `MELF*`, `MS*`, or `ML*` names unless the work actually reopened one of those earlier layers.
- Prefer concrete wording such as "X is complete; Y is in progress at roughly N%; Z is the next mainline" over vague wording such as "still working on it".

## Correctness Rechecks

- Treat periodic correctness rechecks as a required part of the implementation
  workflow, not as optional cleanup after coding.
- The current agent should proactively choose a recurring recheck cadence,
  instead of waiting for the user to ask every time. A good default is:
  - using a simple countdown-style trigger is encouraged, for example
    "recheck after N user turns" or "recheck after N code-edit chunks",
    as long as the cadence stays proactive and regular
  - after every `1-3` substantial code-edit chunks on an active optimization /
    correctness line
  - or after every `3-5` user turns of continuous implementation on the same
    line
  - and always again before treating a new optimization or bug fix as a kept
    checkpoint
- Recheck coverage should rotate rather than repeating one tiny comfort suite
  forever. Prefer varying the mix across rounds so the evidence surface keeps
  widening instead of clustering on the same few cases.
- Thread-specific recheck memory from the user:
  - avoid reusing the exact same tiny sample batch every round
  - prefer **rotated** testcase picks from round to round
  - prefer **broader** sampled batches by default; “a few” cases are usually
    not enough evidence on this line
  - thread memory from the user: the external testcase pool is on the order
    of roughly **1000** cases, so a good default sampled recheck is to take
    about **1/10 of the pool per round** rather than only a handful of
    convenience cases
  - when a bug fix targets runtime `RE`, prefer at least one rotated
    course-facing surface and one rotated third-party surface before treating
    the round as stable
- When choosing recheck samples, avoid reusing the exact same tiny batch on
  every round. Prefer rotating the concrete testcase set so repeated
  validations keep exercising nearby families instead of becoming one
  memorized witness list.
- Prefer larger sample batches when doing third-party or course rechecks.
  A few cases are not enough by default; choose a broader handful so the
  sweep covers more nearby shapes before concluding the round.
- The recurring recheck menu should be drawn from both course-facing and
  external surfaces when they are available locally. Typical examples include:
  - course baselines such as `lv8`, `lv9`, and relevant `-perf` / public perf
    witnesses
  - external testcase trees already present in the environment, such as
    `minic-test-cases-2021s`, `minic-test-cases-2021f`, `indigo`,
    `TrivialCompiler`, `lava-test`, and
    `jokerwyt/sysy-testsuit-collection`
- Prefer not to run exactly the same correctness batch every round unless the
  active risk is very narrow and explicitly calls for that exact repro.
- When choosing a recheck batch, prefer explaining it implicitly through the
  work itself: cover the current risk first, then rotate one or more adjacent
  suites so the overall confidence keeps increasing.
- When a recheck does uncover a regression, treat restoring that previously
  green surface as part of the active implementation task before pressing
  further on the optimization line.

## Long-Running Commands

- Treat long-running commands and sweeps as stateful work, not as disposable
  probes that may be casually restarted every turn.
- Prefer letting an already-started long command finish and reporting its
  result, instead of ending the conversation early and rerunning the same
  command from scratch in the next turn.
- If a long command must be interrupted because it is clearly hung, timed out,
  or has already yielded enough information, say so explicitly and record the
  reason rather than silently restarting it later.
- When ending a conversation while a long command or sweep is still relevant,
  explicitly record:
  - which command is still in flight or still needs completion
  - whether the next turn should continue waiting for that result or rerun it
    for a concrete reason
- Prefer splitting work into:
  - long sweeps / full regressions that should usually be allowed to finish
  - short diagnostics / focused repros that may be rerun cheaply
  instead of repeatedly restarting the same expensive full sweep.
- When a long-running command is likely to outlive the current turn or may
  produce enough output that direct polling becomes awkward, prefer sending
  its stdout/stderr to a concrete log file and then reading or tailing that
  file for progress/result collection, rather than relying only on an active
  terminal session buffer.

## Current Default Assignment

- In this thread, treat the current agent as the Implementation Agent unless the user explicitly reassigns the role.
