# Machine Trace Plan

## Goal

The next backend-facing mainline after `machine_delta` is to turn the current
before/after delta artifact into a **conservative step-trace layer**.

This stage should answer:

- which delta families can already expose one exact step trace?
- which preview delta families can expose one honest preview step trace
  without pretending a full architectural mutation engine already exists?
- how can later execution-state work inspect one explicit execution record
  instead of restaging event classification from `machine_delta` summaries?

In short:

- `machine_delta` decides the first explicit exact-delta versus preview-delta
  change meaning
- `machine_trace` decides the first explicit exact-trace versus preview-trace
  execution-record meaning

## Module Boundary

Create a new sibling module:

- `include/machine/trace.h`
- `src/machine/observe/machine_trace/`
- `tests/machine/observe/machine_trace/`

Do not fold this into:

- `src/machine/observe/machine_delta/`
- `src/machine/observe/machine_observe/`

`machine_delta` should remain the before/after-delta authority.
`machine_trace` should become the first explicit execution-record authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineDeltaFile`
- preserve the current delta / observe / apply / commit / writeback / mutation
  / state / transition / interp / payload / decode / step / runtime view
  as-is
- materialize one exact trace for exact delta families
- materialize one preview trace for preview delta families
- keep blocked-control and blocked-unsupported states explicit
- surface one coarse trace change class above the raw status/pc/stack/fetch
  flags

Do not start with:

- pretending preview traces are exact architectural executions
- inventing bytewise memory/register trace details the current backend line
  does not yet honestly know
- simulating external call side effects
- pretending blocked-control paths already expose one resolved execution trace

## First Public APIs

- `machine_trace_build_from_machine_delta_file(...)`
- `machine_trace_build_from_machine_delta_report(...)`
- `machine_trace_build_from_machine_step_file(...)`
- `machine_trace_build_from_machine_step_report(...)`
- `machine_trace_build_from_machine_ir_report(...)`
- `machine_trace_verify_file(...)`
- `machine_trace_dump_file(...)`

## Staging

### Stage MTR1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MTR2: First Trace From `machine_delta`

- explicit exact-trace / preview-trace / blocked-control /
  blocked-unsupported shaping
- first execution-record summary surface
- first coarse trace change-class surface

### Stage MTR3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta -> machine_trace`

## Current Position

- `MTR1 representation skeleton`: now effectively **~100%**
  - the first `machine_trace` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MTR2 first trace from machine_delta`: now effectively **~100%**
  - current lowering materializes one exact trace for exact delta families,
    one preview trace for preview delta families, preserves blocked
    control/unsupported states explicitly, and surfaces one coarse
    change-class view above the raw status/pc/stack/fetch flags
- `MTR3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineDeltaFile` /
    `MachineDeltaReport`
  - the same bridge family already reaches `MachineObserveFile` /
    `MachineObserveReport`, `MachineApplyFile` / `MachineApplyReport`,
    `MachineCommitFile` / `MachineCommitReport`, `MachineStepFile` /
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw trace files, reports, direct dumps, and report dumps
  - the trace layer now also keeps `MachineElfArtifactSummary` queryable on
    both raw files and report artifacts, and prints
    `elf_origin` / `elf_semantics` / `elf_source` in dump surfaces so the
    downstream provenance chain stays explicit past `machine_delta`

## File Management Rules

- keep all implementation under `src/machine/observe/machine_trace/`
- keep all public declarations under `include/machine/trace.h`
- keep tests under `tests/machine/observe/machine_trace/`

## Current Stop Condition

- Treat the current first machine-trace slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-trace`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  mutation logs once the first slice already covers:
  - direct `machine_delta -> machine_trace`
  - explicit exact-trace vs preview-trace vs blocked-state shaping
  - first consumer-facing execution-record surface above the delta layer
- Reopen active expansion for:
  - a concrete architectural mutation / trace consumer need
  - a confirmed trace classification bug
  - the next deliberately chosen richer execution/mutation-record layer
