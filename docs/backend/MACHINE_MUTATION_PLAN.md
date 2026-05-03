# Machine Mutation Plan

## Goal

The next backend-facing mainline after `machine_state` is to turn the current
applied-state artifact into a **conservative instruction-mutation layer**.

This stage should answer:

- when does the current instruction already need no further non-control
  mutation once `machine_state` has applied the next execution-state snapshot?
- when can downstream experiments distinguish between:
  - instructions whose state change is already complete
  - instructions whose data/result/store/call mutation is still deferred
  - states that are still blocked on unresolved control transfer or
    unsupported execution?
- how can later richer execution-state work inspect one explicit mutation
  artifact instead of restaging op-family and deferred-effect logic from
  `machine_state` / `machine_interp` summaries?

In short:

- `machine_state` decides the first explicit applied-state snapshot meaning
- `machine_mutation` decides the first explicit non-control mutation meaning

## Module Boundary

Create a new sibling module:

- `include/machine/mutation.h`
- `src/machine/runtime/machine_mutation/`
- `tests/machine/runtime/machine_mutation/`

Do not fold this into:

- `src/machine/runtime/machine_state/`
- `src/machine/runtime/machine_interp/`

`machine_state` should remain the applied-state authority.
`machine_mutation` should become the first explicit data-mutation authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineStateFile`
- preserve the current state / transition / interp / payload / decode / step /
  runtime view as-is
- classify halted control-only instructions as complete `no-mutation`
- classify control-transfer-blocked states as explicitly blocked on control
- classify ready op instructions into first deferred mutation families:
  - register/value-result
  - local-slot store
  - global-slot store
  - call effect
- keep unsupported states explicit rather than pretending a mutation exists

Do not start with:

- full architectural register writeback
- full slot/global memory mutation
- call-side external state simulation
- path selection for deferred control transfer

## First Public APIs

- `machine_mutation_build_from_machine_state_file(...)`
- `machine_mutation_build_from_machine_state_report(...)`
- `machine_mutation_build_from_machine_step_file(...)`
- `machine_mutation_build_from_machine_step_report(...)`
- `machine_mutation_build_from_machine_ir_report(...)`
- `machine_mutation_verify_file(...)`
- `machine_mutation_dump_file(...)`

## Staging

### Stage MMUT1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MMUT2: First Mutation Classification From `machine_state`

- explicit no-mutation / deferred-register / deferred-slot / deferred-call /
  blocked-control / blocked-unsupported shaping
- first raw/report query helpers

### Stage MMUT3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation`

## Current Position

- `MMUT1 representation skeleton`: now effectively **~100%**
  - the first `machine_mutation` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MMUT2 first mutation classification from machine_state`: now effectively
  **~100%**
  - current lowering classifies halted control-only instructions as
    `no-mutation`, classifies unresolved control transfers and unsupported
    states explicitly, and classifies ready op families into first deferred
    register/local/global/call mutation buckets
- `MMUT3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineStateFile` / `MachineStateReport`
  - the same bridge family already reaches `MachineTransitionFile` /
    `MachineTransitionReport`, `MachineInterpFile` / `MachineInterpReport`,
    `MachinePayloadDecodeFile` / `MachinePayloadDecodeReport`,
    `MachineDecodeFile` / `MachineDecodeReport`, `MachineStepFile` /
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw mutation files, reports, direct dumps, and report dumps
  - that same mutation bridge now also preserves upstream ELF provenance
    instead of flattening it away at mutation-classification time: raw
    mutation files, mutation reports, and dump/report-dump helpers surface
    the embedded source-ELF artifact summary carried through `machine_state`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_mutation/`
- keep all public declarations under `include/machine/mutation.h`
- keep tests under `tests/machine/runtime/machine_mutation/`

## Current Stop Condition

- Treat the current first machine-mutation slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-mutation`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into full architectural
  data writeback once the first slice already covers:
  - direct `machine_state -> machine_mutation`
  - explicit no-mutation vs deferred-mutation vs blocked-state shaping
  - first raw/report mutation query/dump surface
- Reopen active expansion for:
  - a concrete architectural register or slot-writeback consumer need
  - a confirmed mutation-family classification bug
  - the next deliberately chosen richer execution-state / writeback layer
