# Machine History Plan

## Goal

The next backend-facing mainline after `machine_outcome` is to turn the current
step-outcome artifact into a **conservative single-entry history layer**.

This stage should answer:

- which outcome families can already expose one exact history entry?
- which preview outcome families can expose one honest preview history entry
  without pretending a full replay engine already exists?
- how can later consumers inspect one explicit history artifact instead of
  restaging single-entry history shape from `machine_outcome` summaries?

In short:

- `machine_outcome` decides the first explicit exact-outcome versus
  preview-outcome consequence meaning
- `machine_history` decides the first explicit exact-history versus
  preview-history single-entry history meaning

## Module Boundary

Create a new sibling module:

- `include/machine/history.h`
- `src/machine/observe/machine_history/`
- `tests/machine/observe/machine_history/`

Do not fold this into:

- `src/machine/observe/machine_outcome/`
- `src/machine/observe/machine_event/`

`machine_outcome` should remain the outcome-family authority.
`machine_history` should become the first explicit single-entry history
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineOutcomeFile`
- preserve the current outcome / event / trace / delta / observe / apply /
  commit / writeback / mutation / state / transition / interp / payload /
  decode / step / runtime view as-is
- materialize one exact history entry for exact outcome families
- materialize one preview history entry for preview outcome families
- keep blocked-control and blocked-unsupported states explicit
- classify one history family such as `program-stop-history`,
  `value-history`, `local-update-history`, `global-update-history`,
  `call-history`, `blocked-control-history`, or
  `blocked-unsupported-history`
- keep the current first slice honest as a **single-entry** history artifact,
  not a multi-step replay

Do not start with:

- pretending preview history entries are exact architectural replays
- inventing multi-step replay, rollback, or causality details the current
  backend line does not yet honestly know
- simulating external call completion beyond the current outcome surface
- pretending blocked-control paths already expose a resolved multi-entry
  history

## First Public APIs

- `machine_history_build_from_machine_outcome_file(...)`
- `machine_history_build_from_machine_outcome_report(...)`
- `machine_history_build_from_machine_step_file(...)`
- `machine_history_build_from_machine_step_report(...)`
- `machine_history_build_from_machine_ir_report(...)`
- `machine_history_verify_file(...)`
- `machine_history_dump_file(...)`

## Staging

### Stage MHIS1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MHIS2: First History From `machine_outcome`

- explicit exact-history / preview-history / blocked-control /
  blocked-unsupported shaping
- first single-entry history-family classification surface
- preserved outcome/event/trace context surface for later consumers

### Stage MHIS3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome -> machine_history`

## Current Position

- `MHIS1 representation skeleton`: now effectively **~100%**
  - the first `machine_history` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MHIS2 first history from machine_outcome`: now effectively **~100%**
  - current lowering materializes one exact history entry for exact outcome
    families, one preview history entry for preview outcome families,
    preserves blocked control/unsupported states explicitly, and classifies
    first history-family names above the outcome record
- `MHIS3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineOutcomeFile` /
    `MachineOutcomeReport`
  - the same bridge family already reaches `MachineEventFile` /
    `MachineEventReport`, `MachineTraceFile` / `MachineTraceReport`,
    `MachineDeltaFile` / `MachineDeltaReport`, `MachineObserveFile` /
    `MachineObserveReport`, `MachineApplyFile` / `MachineApplyReport`,
    `MachineCommitFile` / `MachineCommitReport`, `MachineStepFile` /
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw history files, reports, direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_history/`
- keep all public declarations under `include/machine/history.h`
- keep tests under `tests/machine/observe/machine_history/`

## Current Stop Condition

- Treat the current first machine-history slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-history`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake replay once
  the first slice already covers:
  - direct `machine_outcome -> machine_history`
  - explicit exact-history vs preview-history vs blocked-state shaping
  - first single-entry history-family classification surface above the
    outcome layer
- Reopen active expansion for:
  - a concrete replay / history consumer need
  - a confirmed history classification bug
  - the next deliberately chosen richer execution/history layer
