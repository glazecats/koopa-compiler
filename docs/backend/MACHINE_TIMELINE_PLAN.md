# Machine Timeline Plan

## Goal

The next backend-facing mainline after `machine_history` is to turn the current
single-entry history artifact into a **conservative single-tick timeline
layer**.

This stage should answer:

- which history families can already expose one exact timeline entry?
- which preview history families can expose one honest preview timeline entry
  without pretending a full replay timeline already exists?
- how can later consumers inspect one explicit ordered timeline artifact
  instead of restaging temporal ordering from `machine_history` summaries?

In short:

- `machine_history` decides the first explicit exact-history versus
  preview-history single-entry history meaning
- `machine_timeline` decides the first explicit exact-timeline versus
  preview-timeline single-tick timeline meaning

## Module Boundary

Create a new sibling module:

- `include/machine/timeline.h`
- `src/machine/observe/machine_timeline/`
- `tests/machine/observe/machine_timeline/`

Do not fold this into:

- `src/machine/observe/machine_history/`
- `src/machine/observe/machine_outcome/`

`machine_history` should remain the single-entry-history authority.
`machine_timeline` should become the first explicit single-tick timeline
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineHistoryFile`
- preserve the current history / outcome / event / trace / delta / observe /
  apply / commit / writeback / mutation / state / transition / interp /
  payload / decode / step / runtime view as-is
- materialize one exact timeline entry for exact history families
- materialize one preview timeline entry for preview history families
- keep blocked-control and blocked-unsupported states explicit
- classify one timeline family such as `program-stop-timeline`,
  `value-timeline`, `local-update-timeline`, `global-update-timeline`,
  `call-timeline`, `blocked-control-timeline`, or
  `blocked-unsupported-timeline`
- keep the current first slice honest as a **single-tick** timeline artifact
  with `entry_count=1` and `entry_index=0`

Do not start with:

- pretending preview timeline entries are exact architectural replay streams
- inventing multi-entry ordering or replay causality the current backend line
  does not yet honestly know
- simulating external call completion beyond the current history surface
- pretending blocked-control paths already expose a resolved replay timeline

## First Public APIs

- `machine_timeline_build_from_machine_history_file(...)`
- `machine_timeline_build_from_machine_history_report(...)`
- `machine_timeline_build_from_machine_step_file(...)`
- `machine_timeline_build_from_machine_step_report(...)`
- `machine_timeline_build_from_machine_ir_report(...)`
- `machine_timeline_verify_file(...)`
- `machine_timeline_dump_file(...)`

## Staging

### Stage MTIM1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MTIM2: First Timeline From `machine_history`

- explicit exact-timeline / preview-timeline / blocked-control /
  blocked-unsupported shaping
- first single-tick timeline-family classification surface
- preserved history/outcome/event/trace context surface for later consumers

### Stage MTIM3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history -> machine_timeline`

## Current Position

- `MTIM1 representation skeleton`: now effectively **~100%**
  - the first `machine_timeline` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MTIM2 first timeline from machine_history`: now effectively **~100%**
  - current lowering materializes one exact timeline entry for exact history
    families, one preview timeline entry for preview history families,
    preserves blocked control/unsupported states explicitly, and classifies
    first timeline-family names above the history record
- `MTIM3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineHistoryFile` /
    `MachineHistoryReport`
  - the same bridge family already reaches `MachineOutcomeFile` /
    `MachineOutcomeReport`, `MachineEventFile` / `MachineEventReport`,
    `MachineTraceFile` / `MachineTraceReport`, `MachineDeltaFile` /
    `MachineDeltaReport`, `MachineObserveFile` / `MachineObserveReport`,
    `MachineApplyFile` / `MachineApplyReport`, `MachineCommitFile` /
    `MachineCommitReport`, `MachineStepFile` / `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw timeline
    files, reports, direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_timeline/`
- keep all public declarations under `include/machine/timeline.h`
- keep tests under `tests/machine/observe/machine_timeline/`

## Current Stop Condition

- Treat the current first machine-timeline slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-timeline`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake multi-entry
  replay once the first slice already covers:
  - direct `machine_history -> machine_timeline`
  - explicit exact-timeline vs preview-timeline vs blocked-state shaping
  - first single-tick timeline-family classification surface above the
    history layer
- Reopen active expansion for:
  - a concrete replay / timeline consumer need
  - a confirmed timeline classification bug
  - the next deliberately chosen richer execution/timeline layer
