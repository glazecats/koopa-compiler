# Machine Journal Plan

## Goal

The next backend-facing mainline after `machine_log` is to turn the current
single-line log artifact into a **conservative single-record journal layer**.

This stage should answer:

- which log families can already expose one exact journal record?
- which preview log families can expose one honest preview journal record
  without pretending a full replay journal already exists?
- how can later consumers inspect one explicit journal artifact instead of
  restaging slightly more consumer-facing narrative state from
  `machine_log` summaries?

In short:

- `machine_log` decides the first explicit exact-log versus preview-log
  single-line log meaning
- `machine_journal` decides the first explicit exact-journal versus
  preview-journal single-record journal meaning

## Module Boundary

Create a new sibling module:

- `include/machine/journal.h`
- `src/machine/observe/machine_journal/`
- `tests/machine/observe/machine_journal/`

Do not fold this into:

- `src/machine/observe/machine_log/`
- `src/machine/observe/machine_timeline/`

`machine_log` should remain the single-line-log authority.
`machine_journal` should become the first explicit single-record journal
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineLogFile`
- preserve the current log / timeline / history / outcome / event / trace /
  delta / observe / apply / commit / writeback / mutation / state /
  transition / interp / payload / decode / step / runtime view as-is
- materialize one exact journal record for exact log families
- materialize one preview journal record for preview log families
- keep blocked-control and blocked-unsupported states explicit
- classify one journal family such as `program-stop-journal`,
  `value-journal`, `local-update-journal`, `global-update-journal`,
  `call-journal`, `blocked-control-journal`, or
  `blocked-unsupported-journal`
- keep the current first slice honest as a **single-record** journal artifact
  with `record_count=1` and `record_index=0`

Do not start with:

- pretending preview journal records are resolved replay transcripts
- inventing multi-record sequencing or replay causality the current backend
  line does not yet honestly know
- simulating external call completion beyond the current log surface
- pretending blocked-control paths already expose a resolved multi-record
  journal

## First Public APIs

- `machine_journal_build_from_machine_log_file(...)`
- `machine_journal_build_from_machine_log_report(...)`
- `machine_journal_build_from_machine_step_file(...)`
- `machine_journal_build_from_machine_step_report(...)`
- `machine_journal_build_from_machine_ir_report(...)`
- `machine_journal_verify_file(...)`
- `machine_journal_dump_file(...)`

## Staging

### Stage MJR1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MJR2: First Journal From `machine_log`

- explicit exact-journal / preview-journal / blocked-control /
  blocked-unsupported shaping
- first single-record journal-family classification surface
- preserved log/timeline/history/outcome/event/trace context surface for
  later consumers

### Stage MJR3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history -> machine_timeline -> machine_log -> machine_journal`

## Current Position

- `MJR1 representation skeleton`: now effectively **~100%**
  - the first `machine_journal` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MJR2 first journal from machine_log`: now effectively **~100%**
  - current lowering materializes one exact journal record for exact log
    families, one preview journal record for preview log families, preserves
    blocked control/unsupported states explicitly, and classifies first
    journal-family names above the log record
- `MJR3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineLogFile` / `MachineLogReport`
  - the same bridge family already reaches `MachineTimelineFile` /
    `MachineTimelineReport`, `MachineHistoryFile` / `MachineHistoryReport`,
    `MachineOutcomeFile` / `MachineOutcomeReport`, `MachineEventFile` /
    `MachineEventReport`, `MachineTraceFile` / `MachineTraceReport`,
    `MachineDeltaFile` / `MachineDeltaReport`, `MachineObserveFile` /
    `MachineObserveReport`, `MachineApplyFile` / `MachineApplyReport`,
    `MachineCommitFile` / `MachineCommitReport`, `MachineStepFile` /
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw journal files, reports, direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_journal/`
- keep all public declarations under `include/machine/journal.h`
- keep tests under `tests/machine/observe/machine_journal/`

## Current Stop Condition

- Treat the current first machine-journal slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-journal`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake multi-record
  replay once the first slice already covers:
  - direct `machine_log -> machine_journal`
  - explicit exact-journal vs preview-journal vs blocked-state shaping
  - first single-record journal-family classification surface above the log
    layer
- Reopen active expansion for:
  - a concrete replay / journal consumer need
  - a confirmed journal classification bug
  - the next deliberately chosen richer execution/journal layer
