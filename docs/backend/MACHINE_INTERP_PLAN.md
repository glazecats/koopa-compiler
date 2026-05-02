# Machine Interp Plan

## Goal

The next backend-facing mainline after `machine_payload_decode` is to turn the
current payload-decode artifact into a **conservative execute-current-
instruction / interpreter layer**.

This stage should answer:

- what first minimal execution result can we derive from the current decoded
  instruction without pretending the byte encoding is richer than it is?
- when can we conservatively advance to the next `pc`, halt immediately, or
  surface block-target control transfer facts?
- how can downstream experiments inspect one explicit execution-result summary
  over the current decoded instruction instead of re-deriving the same control
  rules by hand?

In short:

- `machine_payload_decode` decides the first explicit payload-byte meaning
- `machine_interp` decides the first explicit execution-result meaning over the
  current decoded instruction

## Module Boundary

Create a new sibling module:

- `include/machine/interp.h`
- `src/machine/runtime/machine_interp/`
- `tests/machine/runtime/machine_interp/`

Do not fold this into:

- `src/machine/runtime/machine_payload_decode/`
- `src/machine/runtime/machine_step/`

`machine_payload_decode` should remain the payload-byte authority.
`machine_interp` should become the first explicit execute-current-instruction
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachinePayloadDecodeFile`
- preserve the current payload-decode / decode / step / runtime view as-is
- surface one explicit execution-result kind:
  - `advance`
  - `halt`
  - `control-transfer`
  - `unsupported`
- recover the linear next `pc` for ordinary op tags
- recover one first immediate halt result for `return-imm`
- recover block-index control targets for current jump/branch families

Do not start with:

- full register or memory mutation
- complete operand execution for selected ops
- precise compare/branch condition evaluation
- scheduler / syscall / thread semantics

## First Public APIs

- `machine_interp_build_from_machine_payload_decode_file(...)`
- `machine_interp_build_from_machine_payload_decode_report(...)`
- `machine_interp_build_from_machine_step_file(...)`
- `machine_interp_build_from_machine_step_report(...)`
- `machine_interp_build_from_machine_ir_report(...)`
- `machine_interp_verify_file(...)`
- `machine_interp_dump_file(...)`

## Staging

### Stage MIP1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MIP2: First Execution Result From `machine_payload_decode`

- explicit action-kind shaping
- linear next-`pc` recovery for ordinary op tags
- conservative halt/control-transfer shaping for current terminator families
- first raw/report query helpers

### Stage MIP3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode -> machine_interp`

## Current Position

- `MIP1 representation skeleton`: now effectively **~100%**
  - the first `machine_interp` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MIP2 first execution result from machine_payload_decode`: now effectively
  **~100%**
  - current lowering preserves the payload/decode/step/runtime-owned state and
    surfaces one first explicit execution-result snapshot over the current
    decoded instruction
  - current execution shaping now advances ordinary op tags by instruction byte
    count, halts current return families conservatively, decodes one first
    immediate return result from `return-imm`, surfaces block-index targets for
    current jump/branch families, and explicitly marks undecidable cases as
    `unsupported`
- `MIP3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachinePayloadDecodeFile` /
    `MachinePayloadDecodeReport`
  - the same bridge family already reaches `MachineDecodeFile`,
    `MachineDecodeReport`, `MachineStepFile`, `MachineStepReport`, and
    profile-aware `MachineIrAllocateRewriteReport` variants for raw interp
    files, reports, direct dumps, and report dumps
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, payload/decode/step bridge
    entrypoints, and profiled `machine_ir` bridge entrypoints are all now
    exercised under `tests/machine/runtime/machine_interp/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_interp/`
- keep all public declarations under `include/machine/interp.h`
- keep tests under `tests/machine/runtime/machine_interp/`

## Current Stop Condition

- Treat the current first machine-interp slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-interp`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now the current target for this first slice, so
  this line should be treated as the active downstream backend mainline rather
  than as a maintenance-only historical sibling.
- Do not keep expanding this line by default straight into full register/memory
  execution once the first slice already covers:
  - direct `machine_payload_decode -> machine_interp`
  - explicit current execution-result shaping
  - first raw/report interp query/dump surface
- Reopen active expansion for:
  - a concrete richer operand-execution consumer need
  - a confirmed next-`pc` / halt / block-target correctness bug
  - the next deliberately chosen state-mutation / real step semantics layer
