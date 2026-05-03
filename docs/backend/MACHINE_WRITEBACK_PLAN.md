# Machine Writeback Plan

## Goal

The next backend-facing mainline after `machine_mutation` is to turn the
current deferred-effect artifact into a **conservative writeback-commit layer**.

This stage should answer:

- which already-classified mutation families can be treated as committed now,
  even if that commitment is only a no-op control completion?
- which mutation families still cannot be honestly written back because the
  current backend line does not yet carry a full architectural register/storage
  model?
- how can later richer execution-state work inspect one explicit writeback
  artifact instead of restaging commit-vs-deferred logic from
  `machine_mutation` summaries?

In short:

- `machine_mutation` decides the first explicit deferred-effect meaning
- `machine_writeback` decides the first explicit commit-vs-defer writeback meaning

## Module Boundary

Create a new sibling module:

- `include/machine/writeback.h`
- `src/machine/runtime/machine_writeback/`
- `tests/machine/runtime/machine_writeback/`

Do not fold this into:

- `src/machine/runtime/machine_mutation/`
- `src/machine/runtime/machine_state/`

`machine_mutation` should remain the deferred-effect authority.
`machine_writeback` should become the first explicit writeback-commit authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineMutationFile`
- preserve the current mutation / state / transition / interp / payload /
  decode / step / runtime view as-is
- commit `no-mutation` families as real committed no-op writeback
- keep blocked-control and blocked-unsupported states explicit
- keep register/local/global/call writeback families explicit as deferred
  commitment work until a later architectural model exists

Do not start with:

- rewriting `MachineLaunchFile` register tables in place
- rewriting runtime memory bytes for local/global slot stores
- simulating external call side effects
- pretending slot ids already define a real architectural memory layout

## First Public APIs

- `machine_writeback_build_from_machine_mutation_file(...)`
- `machine_writeback_build_from_machine_mutation_report(...)`
- `machine_writeback_build_from_machine_step_file(...)`
- `machine_writeback_build_from_machine_step_report(...)`
- `machine_writeback_build_from_machine_ir_report(...)`
- `machine_writeback_verify_file(...)`
- `machine_writeback_dump_file(...)`

## Staging

### Stage MWB1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MWB2: First Writeback Commitment From `machine_mutation`

- explicit committed-no-op / deferred-register / deferred-slot / deferred-call /
  blocked-control / blocked-unsupported shaping
- first raw/report query helpers

### Stage MWB3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback`

## Current Position

- `MWB1 representation skeleton`: now effectively **~100%**
  - the first `machine_writeback` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MWB2 first writeback commitment from machine_mutation`: now effectively
  **~100%**
  - current lowering commits `no-mutation` control-only families as real
    committed no-op writeback, preserves blocked control/unsupported states
    explicitly, and keeps register/local/global/call writeback families as
    deferred commitment work until a later architectural model lands
- `MWB3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineMutationFile` /
    `MachineMutationReport`
  - the same bridge family already reaches `MachineStateFile` /
    `MachineStateReport`, `MachineTransitionFile` /
    `MachineTransitionReport`, `MachineInterpFile` / `MachineInterpReport`,
    `MachinePayloadDecodeFile` / `MachinePayloadDecodeReport`,
    `MachineDecodeFile` / `MachineDecodeReport`, `MachineStepFile` /
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw writeback files, reports, direct dumps, and report dumps
  - that same writeback bridge now also preserves upstream ELF provenance
    instead of flattening it away at writeback-classification time: raw
    writeback files, writeback reports, and dump/report-dump helpers surface
    the embedded source-ELF artifact summary carried through
    `machine_mutation`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_writeback/`
- keep all public declarations under `include/machine/writeback.h`
- keep tests under `tests/machine/runtime/machine_writeback/`

## Current Stop Condition

- Treat the current first machine-writeback slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-writeback`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  writeback once the first slice already covers:
  - direct `machine_mutation -> machine_writeback`
  - explicit committed-no-op vs deferred-writeback vs blocked-state shaping
  - first raw/report writeback query/dump surface
- Reopen active expansion for:
  - a concrete architectural register or slot-writeback consumer need
  - a confirmed writeback-commit classification bug
  - the next deliberately chosen richer register/memory-application layer
