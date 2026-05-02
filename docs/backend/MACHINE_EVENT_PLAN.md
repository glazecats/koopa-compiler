# Machine Event Plan

## Goal

The next backend-facing mainline after `machine_trace` is to turn the current
execution-record artifact into a **conservative step-event layer**.

This stage should answer:

- which trace families can already expose one exact step event?
- which preview trace families can expose one honest preview step event
  without pretending a full architectural mutation engine already exists?
- how can later consumers inspect one explicit event-family artifact instead
  of restaging event classification from `machine_trace` summaries?

In short:

- `machine_trace` decides the first explicit exact-trace versus preview-trace
  execution-record meaning
- `machine_event` decides the first explicit exact-event versus preview-event
  family meaning

## Module Boundary

Create a new sibling module:

- `include/machine/event.h`
- `src/machine/observe/machine_event/`
- `tests/machine/observe/machine_event/`

Do not fold this into:

- `src/machine/observe/machine_trace/`
- `src/machine/observe/machine_delta/`

`machine_trace` should remain the execution-record authority.
`machine_event` should become the first explicit step-event authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineTraceFile`
- preserve the current trace / delta / observe / apply / commit / writeback /
  mutation / state / transition / interp / payload / decode / step / runtime
  view as-is
- materialize one exact event for exact trace families
- materialize one preview event for preview trace families
- keep blocked-control and blocked-unsupported states explicit
- classify one event family such as `control-halt`, `register-result`,
  `local-store`, `global-store`, `call-effect`, `blocked-control`, or
  `blocked-unsupported`

Do not start with:

- pretending preview events are exact architectural executions
- inventing bytewise memory/register event details the current backend line
  does not yet honestly know
- simulating external call side effects
- pretending blocked-control paths already expose one resolved architectural
  mutation event

## First Public APIs

- `machine_event_build_from_machine_trace_file(...)`
- `machine_event_build_from_machine_trace_report(...)`
- `machine_event_build_from_machine_step_file(...)`
- `machine_event_build_from_machine_step_report(...)`
- `machine_event_build_from_machine_ir_report(...)`
- `machine_event_verify_file(...)`
- `machine_event_dump_file(...)`

## Staging

### Stage MEV1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MEV2: First Event From `machine_trace`

- explicit exact-event / preview-event / blocked-control /
  blocked-unsupported shaping
- first event-family classification surface
- preserved change-class/context surface from trace

### Stage MEV3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace -> machine_event`

## Current Position

- `MEV1 representation skeleton`: now effectively **~100%**
  - the first `machine_event` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MEV2 first event from machine_trace`: now effectively **~100%**
  - current lowering materializes one exact event for exact trace families,
    one preview event for preview trace families, preserves blocked
    control/unsupported states explicitly, and classifies first event-family
    names above the trace record
- `MEV3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineTraceFile` /
    `MachineTraceReport`
  - the same bridge family already reaches `MachineDeltaFile` /
    `MachineDeltaReport`, `MachineObserveFile` / `MachineObserveReport`,
    `MachineApplyFile` / `MachineApplyReport`, `MachineCommitFile` /
    `MachineCommitReport`, `MachineStepFile` / `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw event
    files, reports, direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_event/`
- keep all public declarations under `include/machine/event.h`
- keep tests under `tests/machine/observe/machine_event/`

## Current Stop Condition

- Treat the current first machine-event slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-event`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  mutation logs once the first slice already covers:
  - direct `machine_trace -> machine_event`
  - explicit exact-event vs preview-event vs blocked-state shaping
  - first event-family classification surface above the trace layer
- Reopen active expansion for:
  - a concrete architectural mutation / event consumer need
  - a confirmed event classification bug
  - the next deliberately chosen richer execution/event-history layer
