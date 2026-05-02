# Machine Step / Fetch-State Plan

## Goal

The next backend-facing mainline after `machine_launch` is to turn the current
CPU-launch artifact into a **conservative fetch-state / execution-entry
layer**.

This stage should answer:

- what explicit execution position would the current launch state begin at?
- which mapped runtime segment currently owns that position?
- what first fetch byte is visible at the current program counter before any
  real decoding or stepping exists?

In short:

- `machine_launch` decides the first explicit launch `pc` / `sp` state
- `machine_step` decides the first explicit execution-position / fetch-state
  view over that launch state

## Module Boundary

Create a new sibling module:

- `include/machine/step.h`
- `src/machine/runtime/machine_step/`
- `tests/machine/runtime/machine_step/`

Do not fold this into:

- `src/machine/runtime/machine_launch/`
- `src/machine/runtime/machine_runtime/`

`machine_launch` should remain the launch-state authority.
`machine_step` should become the first explicit fetch-state authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineLaunchFile`
- preserve the current launch / runtime memory view as-is
- surface one explicit execution status (`ready` / `halted`)
- surface one explicit current segment and one current fetch byte at `pc`
- expose one first raw/report fetch-state query surface

Do not start with:

- ISA decode
- instruction stepping
- architectural side effects beyond current-state surfacing
- scheduler / syscall / thread semantics

## First Public APIs

- `machine_step_build_from_machine_launch_file(...)`
- `machine_step_build_from_machine_launch_report(...)`
- `machine_step_build_from_machine_ir_report(...)`
- `machine_step_verify_file(...)`
- `machine_step_dump_file(...)`

## Staging

### Stage MST1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MST2: First Lowering From `machine_launch`

- explicit current `pc` fetch-state shaping
- current segment ownership
- current fetch-byte surfacing
- first raw/report query helpers

### Stage MST3: First Upstream Bridge

- `machine_ir -> ... -> machine_launch -> machine_step`

## Current Position

- `MST1 representation skeleton`: now effectively **~100%**
  - the first `machine_step` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MST2 first lowering from machine_launch`: now effectively **~100%**
  - current lowering preserves the launch/runtime-owned state and surfaces one
    first explicit execution-position snapshot over it
  - current fetch-state shaping now records the current mapped segment plus
    one current fetch byte at the launch program counter
  - the current step state is now also explicitly linked back to runtime
    launch, runtime memory, and current minimal initial-stack facts instead
    of leaving that relationship implicit
- `MST3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineLaunchFile` /
    `MachineLaunchReport`
  - the same bridge family already reaches `MachineRuntimeFile`,
    `MachineRuntimeReport`, `MachineLoadFile`, `MachineLoadReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw step
    files, step reports, direct dumps, and report dumps
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, launch/runtime/load
    bridge entrypoints, and profiled `machine_ir` bridge entrypoints are all
    now exercised under `tests/machine/runtime/machine_step/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_step/`
- keep all public declarations under `include/machine/step.h`
- keep tests under `tests/machine/runtime/machine_step/`

## Current Stop Condition

- Treat the current first machine-step slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-step`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now currently satisfied for the present first
  slice, so this line should now be treated as checkpoint-ready /
  maintenance-first unless a later decode / interpreter consumer need is
  chosen explicitly.
- Do not keep expanding this line by default straight into decode or
  interpreter semantics once the first slice already covers:
  - direct `machine_launch -> machine_step`
  - explicit current fetch-state shaping
  - first raw/report step query/dump surface
- Reopen active expansion for:
  - a concrete decode / stepping consumer need
  - a confirmed fetch-state / mapped-byte correctness bug
  - the next deliberately chosen decode / interpreter consumer layer
