# Machine Transition Plan

## Goal

The next backend-facing mainline after `machine_interp` is to turn the current
execution-result artifact into a **conservative next-state transition layer**.

This stage should answer:

- when can the current interpreter result be turned into one explicit next
  fetch-state snapshot without guessing hidden metadata?
- how can downstream experiments distinguish between:
  - transitions that are fully resolved now
  - control transfers that still need a later target-to-`pc` mapping layer
  - unsupported cases that still cannot mutate state honestly
- how can later stateful execution work inspect one explicit transition result
  instead of restaging next-`pc` / halt bookkeeping from `machine_interp`
  summaries?

In short:

- `machine_interp` decides the first explicit execution-result meaning
- `machine_transition` decides the first explicit next-state transition meaning

## Module Boundary

Create a new sibling module:

- `include/machine/transition.h`
- `src/machine/runtime/machine_transition/`
- `tests/machine/runtime/machine_transition/`

Do not fold this into:

- `src/machine/runtime/machine_interp/`
- `src/machine/runtime/machine_step/`

`machine_interp` should remain the current-instruction execution-result
authority.
`machine_transition` should become the first explicit next-state transition
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineInterpFile`
- preserve the current interp / payload / decode / step / runtime view as-is
- resolve one explicit next-fetch snapshot for linear `advance`
- resolve one explicit halted next-state for `halt`
- keep current control-transfer targets visible but explicitly deferred when
  they still need a later block-target-to-`pc` mapping layer
- keep unsupported execution results explicit rather than fabricating a state

Do not start with:

- full register or memory mutation
- target-block-to-runtime-`pc` recovery for branch/jump families
- real compare/branch decision evaluation
- multi-step scheduling or loop execution

## First Public APIs

- `machine_transition_build_from_machine_interp_file(...)`
- `machine_transition_build_from_machine_interp_report(...)`
- `machine_transition_build_from_machine_step_file(...)`
- `machine_transition_build_from_machine_step_report(...)`
- `machine_transition_build_from_machine_ir_report(...)`
- `machine_transition_verify_file(...)`
- `machine_transition_dump_file(...)`

## Staging

### Stage MTN1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MTN2: First Next-State Transition From `machine_interp`

- explicit resolution-kind shaping
- linear next-fetch recovery for resolved `advance`
- explicit halted next-state for resolved `halt`
- deferred control-transfer surfacing without fake target `pc`
- first raw/report query helpers

### Stage MTN3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition`

## Current Position

- `MTN1 representation skeleton`: now effectively **~100%**
  - the first `machine_transition` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MTN2 first next-state transition from machine_interp`: now effectively
  **~100%**
  - current lowering preserves the interp/payload/decode/step/runtime-owned
    state and surfaces one first explicit next-state transition snapshot above
    it
  - current transition shaping now resolves a real next fetch for linear
    `advance`, resolves a real halted next-state for `halt`, explicitly
    carries control-transfer targets forward as deferred work instead of
    inventing a fake next `pc`, and preserves unsupported cases as unresolved
- `MTN3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineInterpFile` / `MachineInterpReport`
  - the same bridge family already reaches `MachinePayloadDecodeFile`,
    `MachinePayloadDecodeReport`, `MachineDecodeFile`, `MachineDecodeReport`,
    `MachineStepFile`, `MachineStepReport`, and profile-aware
    `MachineIrAllocateRewriteReport` variants for raw transition files,
    reports, direct dumps, and report dumps
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, interp/payload/decode/step
    bridge entrypoints, and profiled `machine_ir` bridge entrypoints are all
    exercised under `tests/machine/runtime/machine_transition/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_transition/`
- keep all public declarations under `include/machine/transition.h`
- keep tests under `tests/machine/runtime/machine_transition/`

## Current Stop Condition

- Treat the current first machine-transition slice as checkpoint-worthy once
  the following stay green together:
  - `make test-machine-transition`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now the current target for this first slice, so
  this line should be treated as the active downstream backend mainline rather
  than as a maintenance-only historical sibling.
- Do not keep expanding this line by default straight into full control-flow
  target resolution or register/memory mutation once the first slice already
  covers:
  - direct `machine_interp -> machine_transition`
  - explicit resolved-vs-deferred transition shaping
  - first raw/report transition query/dump surface
- Reopen active expansion for:
  - a concrete target-block-to-`pc` consumer need
  - a confirmed next-fetch / halt transition correctness bug
  - the next deliberately chosen state-mutation / real step semantics layer
