# Machine Payload-Decode Plan

## Goal

The next backend-facing mainline after `machine_decode` is to turn the current
tag-decode artifact into a **conservative payload-decode layer**.

This stage should answer:

- how many immediate payload bytes belong to the current decoded tag under the
  repository's provisional byte encoding?
- which raw payload bytes are currently recoverable from runtime memory without
  stepping execution?
- what first minimal immediate-oriented facts can downstream interpreter
  experiments inspect without re-deriving byte-count policy by hand?

In short:

- `machine_decode` decides the first explicit byte-tag meaning
- `machine_payload_decode` decides the first explicit payload-byte meaning over
  that current tag

## Module Boundary

Create a new sibling module:

- `include/machine/payload_decode.h`
- `src/machine/runtime/machine_payload_decode/`
- `tests/machine/runtime/machine_payload_decode/`

Do not fold this into:

- `src/machine/runtime/machine_decode/`
- `src/machine/runtime/machine_step/`

`machine_decode` should remain the tag-decode authority.
`machine_payload_decode` should become the first explicit payload-decode
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineDecodeFile`
- preserve the current decode / step / runtime view as-is
- recover up to three payload bytes immediately following the current tag
- surface one first payload-byte count plus one minimal immediate value summary
- expose one first raw/report payload-decode query surface

Do not start with:

- full operand reconstruction
- variable-length argument-descriptor decode beyond the current conservative
  byte-count policy
- stepping or execution side effects
- architectural interpreter semantics

## First Public APIs

- `machine_payload_decode_build_from_machine_decode_file(...)`
- `machine_payload_decode_build_from_machine_decode_report(...)`
- `machine_payload_decode_build_from_machine_step_file(...)`
- `machine_payload_decode_build_from_machine_step_report(...)`
- `machine_payload_decode_build_from_machine_ir_report(...)`
- `machine_payload_decode_verify_file(...)`
- `machine_payload_decode_dump_file(...)`

## Staging

### Stage MPD1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MPD2: First Lowering From `machine_decode`

- explicit current payload-byte-count shaping
- raw payload-byte recovery from runtime memory
- first immediate-summary surfacing
- first raw/report query helpers

### Stage MPD3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode -> machine_payload_decode`

## Current Position

- `MPD1 representation skeleton`: now effectively **~100%**
  - the first `machine_payload_decode` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MPD2 first lowering from machine_decode`: now effectively **~100%**
  - current lowering preserves the decode/step/runtime-owned state and
    surfaces one first explicit payload-decode snapshot over the current tag
  - current payload shaping now recovers up to three payload bytes from mapped
    runtime memory and exposes one first immediate summary from the leading
    payload byte when present
  - current byte-count policy intentionally stays conservative and only covers
    the currently encoded immediate-bearing op/terminator families rather than
    claiming full operand reconstruction
- `MPD3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineDecodeFile` /
    `MachineDecodeReport`
  - the same bridge family already reaches `MachineStepFile`,
    `MachineStepReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw payload-decode files, reports, direct dumps, and report
    dumps
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, decode/step bridge
    entrypoints, and profiled `machine_ir` bridge entrypoints are all now
    exercised under `tests/machine/runtime/machine_payload_decode/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_payload_decode/`
- keep all public declarations under `include/machine/payload_decode.h`
- keep tests under `tests/machine/runtime/machine_payload_decode/`

## Current Stop Condition

- Treat the current first machine-payload-decode slice as checkpoint-worthy
  once the following stay green together:
  - `make test-machine-payload-decode`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now the current target for this first slice, so
  this line should be treated as the active downstream backend mainline rather
  than as a maintenance-only historical sibling.
- Do not keep expanding this line by default straight into full operand decode
  or interpreter semantics once the first slice already covers:
  - direct `machine_decode -> machine_payload_decode`
  - explicit current payload-byte-count shaping
  - first raw/report payload query/dump surface
- Reopen active expansion for:
  - a concrete richer operand / interpreter consumer need
  - a confirmed payload-byte-count or mapped-read correctness bug
  - the next deliberately chosen interpreter / execute-current-instruction layer
