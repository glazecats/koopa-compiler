# Machine State Plan

## Goal

The next backend-facing mainline after `machine_transition` is to turn the
current transition artifact into a **conservative applied execution-state
layer**.

This stage should answer:

- when can the current transition result be turned into one honest state
  snapshot without violating `machine_launch` / `machine_step` launch-time
  invariants?
- how can downstream experiments distinguish between:
  - transitions that already become a real next execution state now
  - control transfers that still need a later target-to-`pc` mapping layer
  - unsupported cases that still cannot materialize a real state honestly?
- how can later stateful execution work inspect one explicit state artifact
  instead of restaging `next pc / halted / deferred` logic from
  `machine_transition` summaries?

In short:

- `machine_transition` decides the first explicit next-state transition meaning
- `machine_state` decides the first explicit applied state-snapshot meaning

## Module Boundary

Create a new sibling module:

- `include/machine/state.h`
- `src/machine/runtime/machine_state/`
- `tests/machine/runtime/machine_state/`

Do not fold this into:

- `src/machine/runtime/machine_transition/`
- `src/machine/runtime/machine_step/`

`machine_transition` should remain the transition-resolution authority.
`machine_state` should become the first explicit applied-state authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineTransitionFile`
- preserve the current transition / interp / payload / decode / step / runtime
  view as-is
- materialize one explicit ready state for resolved `next-fetch`
- materialize one explicit halted state for resolved `halt`
- keep deferred control transfer explicit when it still lacks a later
  block-target-to-runtime-`pc` mapping layer
- keep unsupported transition results explicit rather than fabricating a state

Do not start with:

- mutating `MachineStepFile` in place to pretend it is a next-state artifact
- inventing new `pc` values for deferred branch / jump targets
- full register or memory mutation
- multi-step scheduling or loop execution

## First Public APIs

- `machine_state_build_from_machine_transition_file(...)`
- `machine_state_build_from_machine_transition_report(...)`
- `machine_state_build_from_machine_step_file(...)`
- `machine_state_build_from_machine_step_report(...)`
- `machine_state_build_from_machine_ir_report(...)`
- `machine_state_verify_file(...)`
- `machine_state_dump_file(...)`

## Staging

### Stage MSTA1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MSTA2: First Applied State From `machine_transition`

- explicit ready / halted / deferred / unsupported shaping
- real ready-state recovery for resolved `next-fetch`
- real halted state without pretending `MachineStepFile` can be rewritten
- first raw/report query helpers

### Stage MSTA3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state`

## Current Position

- `MSTA1 representation skeleton`: now effectively **~100%**
  - the first `machine_state` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MSTA2 first applied state from machine_transition`: now effectively
  **~100%**
  - current lowering turns resolved `next-fetch` into a real ready-state
    snapshot, turns resolved `halt` into a real halted-state snapshot, and
    preserves deferred control-transfer / unsupported cases as unresolved
    state results instead of fabricating fake next-state files
- `MSTA3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineTransitionFile` /
    `MachineTransitionReport`
  - the same bridge family already reaches `MachineInterpFile` /
    `MachineInterpReport`, `MachinePayloadDecodeFile` /
    `MachinePayloadDecodeReport`, `MachineDecodeFile` /
    `MachineDecodeReport`, `MachineStepFile` / `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw state
    files, reports, direct dumps, and report dumps
  - that same applied-state bridge now also preserves upstream ELF
    provenance instead of flattening it away at state-snapshot time: raw
    state files, state reports, and dump/report-dump helpers surface the
    embedded source-ELF artifact summary carried through `machine_transition`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_state/`
- keep all public declarations under `include/machine/state.h`
- keep tests under `tests/machine/runtime/machine_state/`

## Current Stop Condition

- Treat the current first machine-state slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-state`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake control-target
  `pc` recovery or full register/memory mutation once the first slice already
  covers:
  - direct `machine_transition -> machine_state`
  - explicit resolved-vs-deferred applied-state shaping
  - first raw/report state query/dump surface
- Reopen active expansion for:
  - a concrete target-block-to-runtime-`pc` consumer need
  - a confirmed ready/halt state correctness bug
  - the next deliberately chosen richer execution-state / mutation layer
