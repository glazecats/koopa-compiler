# Machine Outcome Plan

## Goal

The next backend-facing mainline after `machine_event` is to turn the current
step-event artifact into a **conservative step-outcome layer**.

This stage should answer:

- which event families can already expose one exact step outcome?
- which preview event families can expose one honest preview step outcome
  without pretending a full architectural event history already exists?
- how can later consumers inspect one explicit outcome-family artifact instead
  of restaging consequence classification from `machine_event` summaries?

In short:

- `machine_event` decides the first explicit exact-event versus preview-event
  family meaning
- `machine_outcome` decides the first explicit exact-outcome versus
  preview-outcome consequence meaning

## Module Boundary

Create a new sibling module:

- `include/machine/outcome.h`
- `src/machine/observe/machine_outcome/`
- `tests/machine/observe/machine_outcome/`

Do not fold this into:

- `src/machine/observe/machine_event/`
- `src/machine/observe/machine_trace/`

`machine_event` should remain the event-family authority.
`machine_outcome` should become the first explicit step-outcome authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineEventFile`
- preserve the current event / trace / delta / observe / apply / commit /
  writeback / mutation / state / transition / interp / payload / decode /
  step / runtime view as-is
- materialize one exact outcome for exact event families
- materialize one preview outcome for preview event families
- keep blocked-control and blocked-unsupported states explicit
- classify one outcome family such as `program-stopped`, `value-available`,
  `local-updated`, `global-updated`, `call-issued`, `blocked-control`, or
  `blocked-unsupported`

Do not start with:

- pretending preview outcomes are exact architectural histories
- inventing multi-step history, rollback, or replay details the current
  backend line does not yet honestly know
- simulating external call completion beyond the current event surface
- pretending blocked-control paths already expose one resolved execution
  history outcome

## First Public APIs

- `machine_outcome_build_from_machine_event_file(...)`
- `machine_outcome_build_from_machine_event_report(...)`
- `machine_outcome_build_from_machine_step_file(...)`
- `machine_outcome_build_from_machine_step_report(...)`
- `machine_outcome_build_from_machine_ir_report(...)`
- `machine_outcome_verify_file(...)`
- `machine_outcome_dump_file(...)`

## Staging

### Stage MOUT1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MOUT2: First Outcome From `machine_event`

- explicit exact-outcome / preview-outcome / blocked-control /
  blocked-unsupported shaping
- first outcome-family classification surface
- preserved event/trace context surface for later consumers

### Stage MOUT3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event -> machine_outcome`

## Current Position

- `MOUT1 representation skeleton`: now effectively **~100%**
  - the first `machine_outcome` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MOUT2 first outcome from machine_event`: now effectively **~100%**
  - current lowering materializes one exact outcome for exact event families,
    one preview outcome for preview event families, preserves blocked
    control/unsupported states explicitly, and classifies first outcome-family
    names above the event record
- `MOUT3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineEventFile` /
    `MachineEventReport`
  - the same bridge family already reaches `MachineTraceFile` /
    `MachineTraceReport`, `MachineDeltaFile` / `MachineDeltaReport`,
    `MachineObserveFile` / `MachineObserveReport`, `MachineApplyFile` /
    `MachineApplyReport`, `MachineCommitFile` / `MachineCommitReport`,
    `MachineStepFile` / `MachineStepReport`, and profile-aware
    `MachineIrAllocateRewriteReport` variants for raw outcome files, reports,
    direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_outcome/`
- keep all public declarations under `include/machine/outcome.h`
- keep tests under `tests/machine/observe/machine_outcome/`

## Current Stop Condition

- Treat the current first machine-outcome slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-outcome`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake multi-step
  history once the first slice already covers:
  - direct `machine_event -> machine_outcome`
  - explicit exact-outcome vs preview-outcome vs blocked-state shaping
  - first outcome-family classification surface above the event layer
- Reopen active expansion for:
  - a concrete execution-history / outcome consumer need
  - a confirmed outcome classification bug
  - the next deliberately chosen richer execution/history layer
