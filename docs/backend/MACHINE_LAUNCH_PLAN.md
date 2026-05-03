# Machine Launch / CPU-Launch Plan

## Goal

The next backend-facing mainline after `machine_runtime` is to turn the
current process-preparation artifact into a **conservative CPU-launch
state layer**.

This stage should answer:

- what explicit `pc` / `sp` launch state would the current runtime view start
  with?
- how can later execution experiments inspect one first-class launch register
  snapshot without re-deriving it from runtime memory every time?
- how does the current minimal initial-stack image relate to that launch
  register state?

In short:

- `machine_runtime` decides the first launch-ready runtime memory view
- `machine_launch` decides the first explicit CPU launch state over that
  runtime view

## Module Boundary

Create a new sibling module:

- `include/machine/launch.h`
- `src/machine/runtime/machine_launch/`
- `tests/machine/runtime/machine_launch/`

Do not fold this into:

- `src/machine/runtime/machine_runtime/`
- `src/machine/runtime/machine_exec/`

`machine_runtime` should remain the runtime/process-preparation authority.
`machine_launch` should become the first explicit CPU-launch authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineRuntimeFile`
- preserve the current runtime memory / stack placement as-is
- surface one explicit launch register snapshot for `pc` and `sp`
- keep the current minimal initial-stack image as a referenced launch input
- expose one first raw/report launch-state query surface

Do not start with:

- instruction stepping or interpretation
- argv/env/auxv expansion beyond the current minimal runtime image
- syscall / scheduler / thread semantics
- full architectural register-file seeding beyond the first `pc` / `sp`
  boundary

## First Public APIs

- `machine_launch_build_from_machine_runtime_file(...)`
- `machine_launch_build_from_machine_runtime_report(...)`
- `machine_launch_build_from_machine_ir_report(...)`
- `machine_launch_verify_file(...)`
- `machine_launch_dump_file(...)`

## Staging

### Stage MLA1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MLA2: First Lowering From `machine_runtime`

- explicit `pc` / `sp` launch register shaping
- target-profile launch-register naming
- first raw/report launch query helpers
- initial-stack linkage into launch state

### Stage MLA3: First Upstream Bridge

- `machine_ir -> ... -> machine_runtime -> machine_launch`

## Current Position

- `MLA1 representation skeleton`: now effectively **~100%**
  - the first `machine_launch` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MLA2 first lowering from machine_runtime`: now effectively **~100%**
  - current lowering preserves the runtime-owned memory/stack view and
    surfaces one first explicit launch register snapshot over it
  - current launch shaping now seeds target-profile register names for the
    current program counter and stack pointer (`pc`/`sp` on generic,
    `eip`/`esp` on i386 preview)
  - the current launch state is now also explicitly linked back to the
    runtime launch summary, runtime memory summary, and current minimal
    initial-stack image instead of leaving that relationship implicit
- `MLA3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineRuntimeFile` /
    `MachineRuntimeReport`
  - the same bridge family already reaches `MachineLoadFile`,
    `MachineLoadReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw launch files, launch reports, direct dumps, and
    report dumps
  - that same launch-facing bridge now also preserves upstream ELF
    provenance instead of flattening it away at launch-state time: raw launch
    files, launch reports, and dump/report-dump helpers surface the embedded
    source-ELF artifact summary carried through `machine_runtime`
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, runtime/load bridge
    entrypoints, and profiled `machine_ir` bridge entrypoints are all now
    exercised under `tests/machine/runtime/machine_launch/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_launch/`
- keep all public declarations under `include/machine/launch.h`
- keep tests under `tests/machine/runtime/machine_launch/`

## Current Stop Condition

- Treat the current first machine-launch slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-launch`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now currently satisfied for the present first
  slice, so this line should now be treated as checkpoint-ready /
  maintenance-first unless a later execution / stepping consumer need is
  chosen explicitly.
- Do not keep expanding this line by default straight into execution,
  interpretation, or scheduler semantics once the first slice already covers:
  - direct `machine_runtime -> machine_launch`
  - explicit `pc` / `sp` launch shaping
  - first raw/report launch query/dump surface
- Reopen active expansion for:
  - a concrete richer register-seeding consumer need
  - a confirmed launch-state / stack-image correctness bug
  - the next deliberately chosen execution / stepping consumer layer
