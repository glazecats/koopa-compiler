# AGENTS

## Startup Contract

- Before starting work, read `AGENTS.md`.
- Then read `docs/ir-conventions.md`.
- Then read `docs/NEXT_STEPS.md`.
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

## Shared Authority

- `AGENTS.md` defines agent roles, startup rules, and scope boundaries.
- `docs/ir-conventions.md` is working memory for current engineering facts and safety/convention notes.
- `docs/NEXT_STEPS.md` is the roadmap, execution log, and current-plan authority.
- `docs/ir/LOWER_IR_DESIGN.md` is the current design authority for lower-memory / downstream IR planning unless and until `docs/NEXT_STEPS.md` supersedes part of it.
- `docs/ssa/` holds the current SSA-side design authorities:
  - `VALUE_SSA_DESIGN.md` for the next planned post-lower-IR step
  - `VALUE_SSA_ALLOCATOR_PLAN.md` for allocator planning/staging
  - `VALUE_SSA_MACHINE_REGISTER_MODEL_PLAN.md` for allocator-to-machine projection and machine-register-model work
  - `MEMORY_SSA_DESIGN.md` for memory-SSA work
- `docs/backend/` holds the backend stage authorities from `MACHINE_IR_PLAN.md` through `MACHINE_JOURNAL_PLAN.md`; use the stage that matches the active machine layer the task touches.
- `docs/` holds implementation-facing design notes and proposals; unless explicitly promoted by `NEXT_STEPS.md`, treat them as discussion/design authority rather than execution-log authority.

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

## Progress Reporting

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

## Current Default Assignment

- In this thread, treat the current agent as the Implementation Agent unless the user explicitly reassigns the role.
