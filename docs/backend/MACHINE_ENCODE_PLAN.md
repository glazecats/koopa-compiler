# Machine Encode / Offset-Assignment Plan

## Goal

The next backend-facing mainline after `machine_emit` is to turn emitted,
label-stable programs into an **offset-aware encoding-prep layer**.

This stage should answer:

- where does each emitted block begin in code-unit order?
- what code-unit span does each emitted block occupy?
- how can later encoding/debug consumers resolve one branch target by both
  label and abstract offset without re-walking the emitted program?

In short:

- `machine_emit` decides stable emitted labels and emitted block identity
- `machine_encode` decides first abstract code-unit offsets and per-block spans

## Module Boundary

Create a new sibling module:

- `include/machine/encode.h`
- `src/machine/lowering/machine_encode/`
- `tests/machine/lowering/machine_encode/`

Do not fold this into:

- `src/machine/lowering/machine_emit/`
- `src/machine/lowering/machine_layout/`
- `src/machine/lowering/machine_select/`

`machine_emit` should remain the label-aware emitted artifact authority.
`machine_encode` should become the first offset-aware post-emit consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineEmitProgram`
- preserve function structure and emitted block order
- preserve selected ops and emitted terminator families structurally intact
- assign one abstract code-unit slot per op and one per terminator
- compute stable block `start/end` offsets from that abstract unit model
- dump control targets by both emitted label and target start offset

Do not start with:

- final byte encoding
- ISA-specific instruction bytes
- real relocation records
- branch relaxation or variable-width instruction sizing

## First Public APIs

- `machine_encode_lower_program_from_machine_emit(...)`
- `machine_encode_lower_program_from_machine_ir(...)`
- `machine_encode_verify_program(...)`
- `machine_encode_dump_program(...)`

## Staging

### Stage MC1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MC2: First Lowering From `machine_emit`

- preserve emitted block order and payloads
- assign abstract block offsets/spans
- surface target offsets in dumps

### Stage MC3: First Upstream Bridge

- `machine_ir canonicalize -> machine_select -> machine_layout -> machine_emit -> machine_encode`

## Current Position

- `MC1 representation skeleton`: now effectively **~100%**
  - public `machine_encode` structs, lifecycle, verifier, dump, and isolated
    tests are landed as a real sibling module
  - the first representation keeps emitted labels and block identity while
    adding explicit `start_offset`, `end_offset`, and `terminator_offset`
    fields for later consumers
  - the first surface now also has a small query/summary layer over encoded
    functions and blocks, so later consumers can inspect offset-bearing
    artifacts without immediately reparsing dumps
- `MC2 first lowering from machine_emit`: now effectively **~100%**
  - one first lowering path from verifier-legal `machine_emit` is landed
  - current lowering uses one abstract code unit per selected op and one per
    terminator, which is intentionally simple but already good enough to give
    later consumers stable block spans and target offsets
  - direct dump authority now also surfaces targets by both emitted label and
    abstract target offset, so the first offset-aware consumer layer is now
    visibly distinct from plain `machine_emit`
  - a first report artifact is now also landed on direct emit input:
    current authority includes per-function encoded summaries plus per-block
    encoded offset summaries, rather than requiring later consumers to
    recompute those spans from the raw encoded program every time
  - the direct query side is now also more consumer-ready than the first
    dump-only/report-only checkpoint: callers can look up encoded functions by
    name, find encoded blocks by globally unique emitted label at the program
    level, and resolve which encoded block covers a given abstract offset
  - target-aware consumer authority is now also structured instead of
    dump-only: callers can query a block terminator's resolved target labels
    and abstract target offsets directly, and plain `branch` /
    `compare-branch` families now dump those targets explicitly rather than
    collapsing back to generic `term=N`
  - verifier-side authority is now also less "offset-only metadata" and more
    artifact-real: encoded terminators are checked for target-index validity
    against the encoded function's block table instead of only trusting that
    earlier stages got target ranges right
  - the direct bridge side now also accepts an existing `MachineEmitLowerReport`
    artifact instead of only a raw `MachineEmitProgram`, so downstream
    consumers that already live on the emitted-report layer do not need to
    peel the program back out by hand before continuing into encode
- `MC3 first upstream bridge`: now effectively **~100%**
  - the canonicalized
    `machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode`
    bridge is landed as a first real public entrypoint
  - current authority now also includes a first deep-clone surface for encoded
    programs, so later encoding-prep experiments do not have to rebuild from
    `machine_emit` after every local mutation
  - that same bridge-side line now also has a first report-oriented sibling:
    callers can build or dump a structured `machine_encode` report directly
    from `machine_ir` input instead of only from the already-encoded program
  - the report/query side now also has first structured lookup helpers for
    encoded block summaries by emitted label and by abstract offset within one
    function, so downstream consumers can stay on the report artifact instead
    of rebuilding ad hoc scans
  - report-side consumers can now also ask for one block's structured
    terminator-target summary directly, so bridge users do not need to drop
    back to raw program traversal or parse the textual dump just to recover
    target labels/offsets
  - program/report query sides now also have a first combined
    `function-name + abstract offset -> encoded block/block summary` path, so
    downstream bridge consumers can resolve one location without manually
    splitting the lookup into separate function and block-search steps
  - that same bridge line now also reaches direct `MachineEmitLowerReport ->
    machine_encode report/dump` convenience entrypoints, so the artifact chain
    can stay report-oriented across the emit/encode boundary instead of
    bouncing back through raw-program plumbing
  - the same artifact-oriented bridge story now also reaches
    `MachineIrAllocateRewriteReport` directly: callers can continue from an
    existing machine-ir report into encode program/report/dump entrypoints
    without manually rebuilding intermediate machine-emit artifacts first
  - the encoded report side now also has first high-level function-index
    sets for call-bearing, fallthrough-bearing, and branch-bearing encoded
    functions, so later consumers can stay on one structured report artifact
    instead of rescanning all function summaries by hand

## File Management Rules

- keep all implementation under `src/machine/lowering/machine_encode/`
- keep all public declarations under `include/machine/encode.h`
- keep tests under `tests/machine/lowering/machine_encode/`

## Current Stop Condition

- Treat the current machine-encode first slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-encode`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied again after the
  first report/query integration, emitted-report bridge, and machine-ir-report
  bridge helpers.
- The current first machine-encode slice should now be treated as
  **checkpoint-ready**: direct-lowering, report/query surfaces, artifact
  bridges, and verifier-backed target/span checks are all landed strongly
  enough that default next work can reasonably switch from "finish the first
  encode artifact" to "choose the next deliberate real-byte-encoding mainline"
  unless a concrete encode-prep bug appears.
- Do not keep expanding this line by default into real byte encoding once the
  first slice already covers:
  - direct `machine_emit -> machine_encode`
  - canonicalized `machine_ir` bridge through emit
  - stable abstract block offsets and target offsets in dumps
  - verifier-backed structural checks on emitted offset/span surfaces
- Reopen active expansion for:
  - confirmed offset/target correctness bugs
  - a concrete downstream encoding consumer need
  - the next deliberately chosen real-byte-encoding mainline
