# Machine Apply Plan

## Goal

The next backend-facing mainline after `machine_commit` is to turn the current
committed-artifact classification into a **conservative application-plan
layer**.

This stage should answer:

- which already-classified commit families are already honest applied state?
- which deferred commit families can now expose one first explicit application
  obligation even though the current backend line still lacks a full
  architectural register/storage application engine?
- how can later richer execution-state work inspect one explicit application
  plan instead of restaging payload/state-preview logic from `machine_commit`
  summaries?

In short:

- `machine_commit` decides the first explicit committed-artifact meaning
- `machine_apply` decides the first explicit applied-state versus
  pending-application-plan meaning

## Module Boundary

Create a new sibling module:

- `include/machine/apply.h`
- `src/machine/runtime/machine_apply/`
- `tests/machine/runtime/machine_apply/`

Do not fold this into:

- `src/machine/runtime/machine_commit/`
- `src/machine/runtime/machine_state/`

`machine_commit` should remain the committed-artifact authority.
`machine_apply` should become the first explicit application-plan authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineCommitFile`
- preserve the current commit / writeback / mutation / state / transition /
  interp / payload / decode / step / runtime view as-is
- materialize one real applied-state artifact for committed-state families
- keep blocked-control and blocked-unsupported states explicit
- surface register/local/global/call application families as pending
  application plans rather than pretending the full architectural write has
  already happened
- surface payload/immediate hints and one honest state preview for deferred
  next-fetch families when that preview is already available from the existing
  state layer

Do not start with:

- rewriting launch/runtime registers for pending register application
- rewriting runtime memory bytes for pending slot application
- simulating external call effects
- pretending pending application plans already mutate one honest architectural
  state artifact

## First Public APIs

- `machine_apply_build_from_machine_commit_file(...)`
- `machine_apply_build_from_machine_commit_report(...)`
- `machine_apply_build_from_machine_step_file(...)`
- `machine_apply_build_from_machine_step_report(...)`
- `machine_apply_build_from_machine_ir_report(...)`
- `machine_apply_verify_file(...)`
- `machine_apply_dump_file(...)`

## Staging

### Stage MAP1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MAP2: First Application Plan From `machine_commit`

- explicit applied-state / pending-register / pending-slot / pending-call /
  blocked-control / blocked-unsupported shaping
- first payload/immediate hints
- first state-preview surfacing for deferred next-fetch families

### Stage MAP3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit -> machine_apply`

## Current Position

- `MAP1 representation skeleton`: now effectively **~100%**
  - the first `machine_apply` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MAP2 first application plan from machine_commit`: now effectively
  **~100%**
  - current lowering materializes one real applied-state artifact for
    committed-state families, preserves blocked control/unsupported states
    explicitly, and surfaces register/local/global/call pending-application
    plans with payload/immediate hints plus state previews when the current
    state layer already exposes them
- `MAP3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineCommitFile` / `MachineCommitReport`
  - the same bridge family already reaches `MachineStepFile` /
    `MachineStepReport` and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw apply files, reports, direct dumps, and report dumps
  - that same application-plan bridge now also preserves upstream ELF
    provenance instead of flattening it away at apply-plan time: raw apply
    files, apply reports, and dump/report-dump helpers surface the embedded
    source-ELF artifact summary carried through `machine_commit`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_apply/`
- keep all public declarations under `include/machine/apply.h`
- keep tests under `tests/machine/runtime/machine_apply/`

## Current Stop Condition

- Treat the current first machine-apply slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-apply`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  register/memory mutation once the first slice already covers:
  - direct `machine_commit -> machine_apply`
  - explicit applied-state vs pending-application vs blocked-state shaping
  - first payload/immediate-hint and state-preview surfacing
- Reopen active expansion for:
  - a concrete architectural register or slot-application execution need
  - a confirmed application-plan classification bug
  - the next deliberately chosen richer architectural-application layer
