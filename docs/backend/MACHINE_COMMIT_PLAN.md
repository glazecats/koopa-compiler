# Machine Commit Plan

## Goal

The next backend-facing mainline after `machine_writeback` is to turn the
current writeback-classification artifact into a **conservative committed
execution artifact layer**.

This stage should answer:

- which already-classified writeback families can now be turned into one real
  committed execution artifact?
- which writeback families still cannot be honestly committed because the
  current backend line still lacks a full architectural register/storage
  application model?
- how can later richer execution-state work inspect one explicit committed
  artifact instead of restaging commit-vs-deferred logic from
  `machine_writeback` summaries?

In short:

- `machine_writeback` decides the first explicit commit-vs-defer writeback meaning
- `machine_commit` decides the first explicit committed-artifact meaning

## Module Boundary

Create a new sibling module:

- `include/machine/commit.h`
- `src/machine/runtime/machine_commit/`
- `tests/machine/runtime/machine_commit/`

Do not fold this into:

- `src/machine/runtime/machine_writeback/`
- `src/machine/runtime/machine_state/`

`machine_writeback` should remain the writeback-commit authority.
`machine_commit` should become the first explicit committed-artifact authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineWritebackFile`
- preserve the current writeback / mutation / state / transition / interp /
  payload / decode / step / runtime view as-is
- materialize one real committed artifact for committed no-op state families
- keep blocked-control and blocked-unsupported states explicit
- keep register/local/global/call commit families explicit as deferred
  commitment work until a later architectural application model exists

Do not start with:

- rewriting launch/runtime registers for deferred register commits
- rewriting runtime memory bytes for deferred slot commits
- simulating external call effects
- pretending deferred commit families already produce one honest committed
  runtime artifact

## First Public APIs

- `machine_commit_build_from_machine_writeback_file(...)`
- `machine_commit_build_from_machine_writeback_report(...)`
- `machine_commit_build_from_machine_step_file(...)`
- `machine_commit_build_from_machine_step_report(...)`
- `machine_commit_build_from_machine_ir_report(...)`
- `machine_commit_verify_file(...)`
- `machine_commit_dump_file(...)`

## Staging

### Stage MCOM1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MCOM2: First Committed Artifact From `machine_writeback`

- explicit committed-state / deferred-register / deferred-slot / deferred-call /
  blocked-control / blocked-unsupported shaping
- first raw/report query helpers

### Stage MCOM3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp -> machine_transition -> machine_state -> machine_mutation -> machine_writeback -> machine_commit`

## Current Position

- `MCOM1 representation skeleton`: now effectively **~100%**
  - the first `machine_commit` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MCOM2 first committed artifact from machine_writeback`: now effectively
  **~100%**
  - current lowering materializes one real committed artifact for committed
    no-op state families, preserves blocked control/unsupported states
    explicitly, and keeps register/local/global/call commit families as
    deferred until a later architectural application model lands
- `MCOM3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineWritebackFile` /
    `MachineWritebackReport`
  - the same bridge family already reaches `MachineMutationFile` /
    `MachineMutationReport`, `MachineStateFile` / `MachineStateReport`,
    `MachineTransitionFile` / `MachineTransitionReport`, `MachineInterpFile` /
    `MachineInterpReport`, `MachinePayloadDecodeFile` /
    `MachinePayloadDecodeReport`, `MachineDecodeFile` /
    `MachineDecodeReport`, `MachineStepFile` / `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw commit
    files, reports, direct dumps, and report dumps
  - that same commit bridge now also preserves upstream ELF provenance
    instead of flattening it away at committed-artifact time: raw commit
    files, commit reports, and dump/report-dump helpers surface the embedded
    source-ELF artifact summary carried through `machine_writeback`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_commit/`
- keep all public declarations under `include/machine/commit.h`
- keep tests under `tests/machine/runtime/machine_commit/`

## Current Stop Condition

- Treat the current first machine-commit slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-commit`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into fake architectural
  register/memory application once the first slice already covers:
  - direct `machine_writeback -> machine_commit`
  - explicit committed-state vs deferred-commit vs blocked-state shaping
  - first raw/report commit query/dump surface
- Reopen active expansion for:
  - a concrete architectural register or slot-application consumer need
  - a confirmed committed-artifact classification bug
  - the next deliberately chosen richer register/memory-application layer
