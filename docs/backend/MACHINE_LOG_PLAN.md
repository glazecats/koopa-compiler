# Machine Log Plan

## Goal

The next backend-facing mainline after `machine_timeline` is to turn the
current single-tick timeline artifact into a **conservative single-line log
layer**.

This stage should answer:

- which timeline families can already expose one exact log line?
- which preview timeline families can expose one honest preview log line
  without pretending a full replay log already exists?
- how can later consumers inspect one explicit log artifact instead of
  restaging text-oriented temporal logging from `machine_timeline` summaries?

In short:

- `machine_timeline` decides the first explicit exact-timeline versus
  preview-timeline single-tick timeline meaning
- `machine_log` decides the first explicit exact-log versus preview-log
  single-line log meaning

## Module Boundary

Create a new sibling module:

- `include/machine/log.h`
- `src/machine/observe/machine_log/`
- `tests/machine/observe/machine_log/`

Do not fold this into:

- `src/machine/observe/machine_timeline/`
- `src/machine/observe/machine_history/`

`machine_timeline` should remain the single-tick-timeline authority.
`machine_log` should become the first explicit single-line log authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineTimelineFile`
- preserve the current timeline / history / outcome / event / trace / delta /
  observe / apply / commit / writeback / mutation / state / transition /
  interp / payload / decode / step / runtime view as-is
- materialize one exact log line for exact timeline families
- materialize one preview log line for preview timeline families
- keep blocked-control and blocked-unsupported states explicit
- classify one log family such as `program-stop-log`, `value-log`,
  `local-update-log`, `global-update-log`, `call-log`,
  `blocked-control-log`, or `blocked-unsupported-log`
- keep the current first slice honest as a **single-line** log artifact with
  `line_count=1` and `line_index=0`

Do not start with:

- pretending preview log lines are exact replay transcripts
- inventing multi-line ordering or replay causality the current backend line
  does not yet honestly know
- simulating external call completion beyond the current timeline surface
- pretending blocked-control paths already expose a resolved replay log

## First Public APIs

- `machine_log_build_from_machine_timeline_file(...)`
- `machine_log_build_from_machine_timeline_report(...)`
- `machine_log_build_from_machine_step_file(...)`
- `machine_log_build_from_machine_step_report(...)`
- `machine_log_build_from_machine_ir_report(...)`
- `machine_log_verify_file(...)`
- `machine_log_dump_file(...)`

## Staging

### Stage MLOG1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MLOG2: First Log From `machine_timeline`

- explicit exact-log / preview-log / blocked-control /
  blocked-unsupported shaping
- first single-line log-family classification surface
- preserved timeline/history/outcome/event/trace context surface for later
  consumers

### Stage MLOG3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history -> machine_timeline -> machine_log`

## Current Position

- `MLOG1 representation skeleton`: now effectively **~100%**
  - the first `machine_log` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MLOG2 first log from machine_timeline`: now effectively **~100%**
  - current lowering materializes one exact log line for exact timeline
    families, one preview log line for preview timeline families, preserves
    blocked control/unsupported states explicitly, and classifies first
    log-family names above the timeline record
- `MLOG3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineTimelineFile` /
    `MachineTimelineReport`
  - the same bridge family already reaches `MachineHistoryFile` /
    `MachineHistoryReport`, `MachineOutcomeFile` / `MachineOutcomeReport`,
    `MachineEventFile` / `MachineEventReport`, `MachineTraceFile` /
    `MachineTraceReport`, `MachineDeltaFile` / `MachineDeltaReport`,
    `MachineObserveFile` / `MachineObserveReport`, `MachineApplyFile` /
    `MachineApplyReport`, `MachineCommitFile` / `MachineCommitReport`,
    `MachineStepFile` / `MachineStepReport`, and profile-aware
    `MachineIrAllocateRewriteReport` variants for raw log files, reports,
    direct dumps, and report dumps
  - the log layer now also keeps `MachineElfArtifactSummary` queryable on
    both raw files and report artifacts, and prints
    `elf_origin` / `elf_semantics` / `elf_source` in dump surfaces so the
    downstream provenance chain stays explicit past `machine_timeline`

## File Management Rules

- keep all implementation under `src/machine/observe/machine_log/`
- keep all public declarations under `include/machine/log.h`
- keep tests under `tests/machine/observe/machine_log/`

## Current Stop Condition

- Treat the current first machine-log slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-log`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake multi-line
  replay once the first slice already covers:
  - direct `machine_timeline -> machine_log`
  - explicit exact-log vs preview-log vs blocked-state shaping
  - first single-line log-family classification surface above the timeline
    layer
- Reopen active expansion for:
  - a concrete replay / log consumer need
  - a confirmed log classification bug
  - the next deliberately chosen richer execution/log layer
