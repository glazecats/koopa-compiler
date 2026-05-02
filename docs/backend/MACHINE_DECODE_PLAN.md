# Machine Decode / Tag-Decode Plan

## Goal

The next backend-facing mainline after `machine_step` is to turn the current
fetch-state artifact into a **conservative byte-tag decode layer**.

This stage should answer:

- what current byte-tag class does the fetch byte represent?
- is that current byte already recognizable as one current op tag or one
  current terminator tag under the repository's provisional byte encoding?
- how can later decode / interpreter experiments inspect that first tag
  meaning without re-deriving raw tag math by hand?

In short:

- `machine_step` decides the first explicit execution-position / fetch-state
- `machine_decode` decides the first explicit byte-tag meaning over that fetch
  state

## Module Boundary

Create a new sibling module:

- `include/machine/decode.h`
- `src/machine/runtime/machine_decode/`
- `tests/machine/runtime/machine_decode/`

Do not fold this into:

- `src/machine/runtime/machine_step/`
- `src/machine/object/machine_bytes/`

`machine_step` should remain the fetch-state authority.
`machine_decode` should become the first explicit decode-tag authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineStepFile`
- preserve the current fetch-state / runtime view as-is
- classify the current fetch byte as `op`, `terminator`, or `none`
- surface the decoded tag value and current known-name when the tag is
  recognized
- expose one first raw/report decode-tag query surface

Do not start with:

- operand / payload decode
- multi-byte instruction decode
- stepping or execution side effects
- architectural interpreter semantics

## First Public APIs

- `machine_decode_build_from_machine_step_file(...)`
- `machine_decode_build_from_machine_step_report(...)`
- `machine_decode_build_from_machine_ir_report(...)`
- `machine_decode_verify_file(...)`
- `machine_decode_dump_file(...)`

## Staging

### Stage MDC1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MDC2: First Lowering From `machine_step`

- explicit current tag-class shaping
- current tag value / known-name surfacing
- first raw/report query helpers

### Stage MDC3: First Upstream Bridge

- `machine_ir -> ... -> machine_step -> machine_decode`

## Current Position

- `MDC1 representation skeleton`: now effectively **~100%**
  - the first `machine_decode` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MDC2 first lowering from machine_step`: now effectively **~100%**
  - current lowering preserves the step/runtime-owned state and surfaces one
    first explicit tag-decode snapshot over the current fetch byte
  - current decode shaping now classifies the fetch byte as `op`,
    `terminator`, or `none`, and surfaces the current tag value plus known
    tag name when recognized
- `MDC3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineStepFile` / `MachineStepReport`
  - the same bridge family already reaches `MachineLaunchFile`,
    `MachineLaunchReport`, and profile-aware `MachineIrAllocateRewriteReport`
    variants for raw decode files, decode reports, direct dumps, and report
    dumps
  - that same first bridge/lifecycle surface is now also regression-locked:
    clone, report refresh, raw/report dump wrappers, step/launch bridge
    entrypoints, and profiled `machine_ir` bridge entrypoints are all now
    exercised under `tests/machine/runtime/machine_decode/`

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_decode/`
- keep all public declarations under `include/machine/decode.h`
- keep tests under `tests/machine/runtime/machine_decode/`

## Current Stop Condition

- Treat the current first machine-decode slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-decode`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now currently satisfied for the present first
  slice, so this line should now be treated as checkpoint-ready /
  maintenance-first unless a later payload-decode or interpreter consumer
  need is chosen explicitly.
- Do not keep expanding this line by default straight into operand decode or
  interpreter semantics once the first slice already covers:
  - direct `machine_step -> machine_decode`
  - explicit current tag-class shaping
  - first raw/report decode-tag query/dump surface
- Reopen active expansion for:
  - a concrete richer payload-decode consumer need
  - a confirmed byte-tag classification correctness bug
  - the next deliberately chosen interpreter / decode-payload consumer layer
