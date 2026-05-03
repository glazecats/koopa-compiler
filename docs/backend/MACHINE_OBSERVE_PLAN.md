# Machine Observe Plan

## Goal

The next backend-facing mainline after `machine_apply` is to turn the current
application-plan artifact into a **conservative observed-state layer**.

This stage should answer:

- which application families already produce one exact observed state?
- which pending application families can still produce one honest preview
  observed state even though the current backend line still lacks a full
  architectural register/storage mutation engine?
- how can later execution-state work inspect one explicit observed-state
  artifact instead of restaging exact-vs-preview logic from `machine_apply`
  summaries?

In short:

- `machine_apply` decides the first explicit applied-state versus
  pending-application-plan meaning
- `machine_observe` decides the first explicit exact-state versus
  preview-state observation meaning

## Module Boundary

Create a new sibling module:

- `include/machine/observe.h`
- `src/machine/observe/machine_observe/`
- `tests/machine/observe/machine_observe/`

Do not fold this into:

- `src/machine/runtime/machine_apply/`
- `src/machine/runtime/machine_state/`

`machine_apply` should remain the application-plan authority.
`machine_observe` should become the first explicit observed-state authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineApplyFile`
- preserve the current apply / commit / writeback / mutation / state /
  transition / interp / payload / decode / step / runtime view as-is
- materialize one exact observed state for applied-state families
- materialize one preview observed state for pending application families when
  the current apply layer already exposes a preview
- keep blocked-control and blocked-unsupported states explicit
- preserve payload/immediate hints and control/return summaries as companion
  context on the observation artifact

Do not start with:

- claiming preview observations are exact architectural mutations
- rewriting launch/runtime registers or memory for pending application plans
- simulating external call effects
- pretending blocked-control paths already expose one resolved observed state

## First Public APIs

- `machine_observe_build_from_machine_apply_file(...)`
- `machine_observe_build_from_machine_apply_report(...)`
- `machine_observe_build_from_machine_step_file(...)`
- `machine_observe_build_from_machine_step_report(...)`
- `machine_observe_build_from_machine_ir_report(...)`
- `machine_observe_verify_file(...)`
- `machine_observe_dump_file(...)`

## Staging

### Stage MOB1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MOB2: First Observed State From `machine_apply`

- explicit exact-state / preview-state / blocked-control /
  blocked-unsupported shaping
- first observed-state summary surface
- preserved payload/immediate hint context

### Stage MOB3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply -> machine_observe`

## Current Position

- `MOB1 representation skeleton`: now effectively **~100%**
  - the first `machine_observe` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MOB2 first observed state from machine_apply`: now effectively **~100%**
  - current lowering materializes one exact observed state for applied-state
    families, one preview observed state for pending register/local/global/call
    application families, preserves blocked control/unsupported states
    explicitly, and keeps payload/immediate hints plus control summaries
    attached to the observation artifact
- `MOB3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineApplyFile` / `MachineApplyReport`
  - the same bridge family already reaches `MachineCommitFile` /
    `MachineCommitReport`, `MachineStepFile` / `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw observe
    files, reports, direct dumps, and report dumps
  - that same observed-state bridge now also preserves upstream ELF
    provenance instead of flattening it away at observe time: raw observe
    files, observe reports, and dump/report-dump helpers surface the embedded
    source-ELF artifact summary carried through `machine_apply`

## File Management Rules

- keep all implementation under `src/machine/observe/machine_observe/`
- keep all public declarations under `include/machine/observe.h`
- keep tests under `tests/machine/observe/machine_observe/`

## Current Stop Condition

- Treat the current first machine-observe slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-observe`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  mutation once the first slice already covers:
  - direct `machine_apply -> machine_observe`
  - explicit exact-state vs preview-state vs blocked-state shaping
  - first unified observed-state artifact above the application-plan layer
- Reopen active expansion for:
  - a concrete architectural mutation / execution consumer need
  - a confirmed observed-state classification bug
  - the next deliberately chosen richer architectural-execution layer
