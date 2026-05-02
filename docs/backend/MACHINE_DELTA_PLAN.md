# Machine Delta Plan

## Goal

The next backend-facing mainline after `machine_observe` is to turn the
current observed-state artifact into a **conservative before/after delta
layer**.

This stage should answer:

- which observed-state families can already expose one exact before/after
  state delta?
- which preview observed-state families can expose one honest preview delta
  without pretending a full architectural mutation engine already exists?
- how can later execution-state work inspect one explicit origin-versus-
  observed change artifact instead of restaging those comparisons from
  `machine_step` plus `machine_observe` every time?

In short:

- `machine_observe` decides the first explicit exact-state versus preview-state
  observation meaning
- `machine_delta` decides the first explicit exact-delta versus preview-delta
  change meaning

## Module Boundary

Create a new sibling module:

- `include/machine/delta.h`
- `src/machine/observe/machine_delta/`
- `tests/machine/observe/machine_delta/`

Do not fold this into:

- `src/machine/observe/machine_observe/`
- `src/machine/runtime/machine_step/`

`machine_observe` should remain the observed-state authority.
`machine_delta` should become the first explicit before/after delta authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineObserveFile`
- preserve the current observe / apply / commit / writeback / mutation / state
  / transition / interp / payload / decode / step / runtime view as-is
- materialize one exact state delta for exact observed-state families
- materialize one preview state delta for preview observed-state families
- keep blocked-control and blocked-unsupported states explicit
- surface first coarse change flags for status / pc / stack / fetch

Do not start with:

- pretending preview deltas are exact architectural mutations
- surfacing bytewise memory-store diffs that the current backend line does not
  yet honestly know
- simulating external call side effects
- pretending blocked-control paths already expose one resolved delta artifact

## First Public APIs

- `machine_delta_build_from_machine_observe_file(...)`
- `machine_delta_build_from_machine_observe_report(...)`
- `machine_delta_build_from_machine_step_file(...)`
- `machine_delta_build_from_machine_step_report(...)`
- `machine_delta_build_from_machine_ir_report(...)`
- `machine_delta_verify_file(...)`
- `machine_delta_dump_file(...)`

## Staging

### Stage MDEL1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MDEL2: First Delta From `machine_observe`

- explicit exact-delta / preview-delta / blocked-control /
  blocked-unsupported shaping
- first origin-vs-observed summary surface
- first coarse status/pc/stack/fetch change flags

### Stage MDEL3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe -> machine_delta`

## Current Position

- `MDEL1 representation skeleton`: now effectively **~100%**
  - the first `machine_delta` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MDEL2 first delta from machine_observe`: now effectively **~100%**
  - current lowering materializes one exact delta for exact observed-state
    families, one preview delta for preview observed-state families, preserves
    blocked control/unsupported states explicitly, and surfaces coarse
    status/pc/stack/fetch change flags plus payload/immediate context
- `MDEL3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineObserveFile` /
    `MachineObserveReport`
  - the same bridge family already reaches `MachineApplyFile` /
    `MachineApplyReport`, `MachineCommitFile` / `MachineCommitReport`,
    `MachineStepFile` / `MachineStepReport`, and profile-aware
    `MachineIrAllocateRewriteReport` variants for raw delta files, reports,
    direct dumps, and report dumps

## File Management Rules

- keep all implementation under `src/machine/observe/machine_delta/`
- keep all public declarations under `include/machine/delta.h`
- keep tests under `tests/machine/observe/machine_delta/`

## Current Stop Condition

- Treat the current first machine-delta slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-delta`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  memory/register mutation once the first slice already covers:
  - direct `machine_observe -> machine_delta`
  - explicit exact-delta vs preview-delta vs blocked-state shaping
  - first origin-vs-observed change summary surface
- Reopen active expansion for:
  - a concrete architectural mutation / trace consumer need
  - a confirmed delta classification bug
  - the next deliberately chosen richer execution/mutation layer
