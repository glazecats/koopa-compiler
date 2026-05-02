# Machine Emit / Label-Surfacing Plan

## Goal

The next backend-facing mainline after `machine_layout` is to turn linearized
layout programs into a **label-aware emission-prep layer**.

This stage should answer:

- what stable emitted label name does each laid-out block receive?
- how do control-transfer dumps surface those labels instead of raw layout
  indices?
- what is the first narrow post-layout consumer that later real encoding can
  build on?

In short:

- `machine_layout` decides block order and fallthrough shape
- `machine_emit` decides the first emitted block labels and label-facing dump
  surface

## Module Boundary

Create a new sibling module:

- `include/machine/emit.h`
- `src/machine/lowering/machine_emit/`
- `tests/machine/lowering/machine_emit/`

Do not fold this into:

- `src/machine/lowering/machine_layout/`
- `src/machine/lowering/machine_select/`
- `src/machine/lowering/machine_ir/`

`machine_layout` should remain the layout/branch-lowering authority.
`machine_emit` should become the first label-aware post-layout consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineLayoutProgram`
- preserve function structure and laid-out block order
- assign one stable globally unique emitted label per laid-out block
- keep selected ops and lowered terminator families structurally intact
- dump control-transfer targets by emitted label name rather than layout index

Do not start with:

- final binary encoding
- physical address assignment
- branch-distance relaxation
- relocation/object-file surfaces

## First Public APIs

- `machine_emit_lower_program_from_machine_layout(...)`
- `machine_emit_lower_program_from_machine_ir(...)`
- `machine_emit_verify_program(...)`
- `machine_emit_dump_program(...)`

## Staging

### Stage ME1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage ME2: First Lowering From `machine_layout`

- preserve layout order and block payloads
- assign stable label names
- preserve selected ops
- preserve lowered terminator families while surfacing targets by label

### Stage ME3: First Upstream Bridge

- `machine_ir canonicalize -> machine_select -> machine_layout -> machine_emit`

## Current Position

- `ME1 representation skeleton`: now effectively **~99%**
  - public `machine_emit` structs, lifecycle, verifier, dump, and isolated
    tests are landed as a real sibling module
  - the first representation keeps block order explicit while adding one
    emitted label surface per block, so later consumers can stop talking in
    raw `layout.N` indices only
  - the public surface now also has first query helpers for whole-program and
    per-function summary/navigation, so later backend work does not need to
    rescan raw structs just to answer basic inventory questions
  - that same query layer now also reaches emitted blocks themselves:
    current authority includes block lookup by emit index, block lookup by
    emitted label name, and block-level summary over emitted id/origin/label
    metadata plus terminator kind
  - emitted labels are now treated as globally unique artifact names, so
    the current query surface also includes program-level lookup by emitted
    label rather than only function-local label lookup
  - `machine_emit` now also has a first deep-clone surface for whole emitted
    programs, so later post-emit experiments do not need to choose between
    mutating one shared artifact in place or rebuilding from upstream every
    time
- `ME2 first lowering from machine_layout`: now effectively **~97%**
  - one first lowering path from verifier-legal `machine_layout` is landed
  - current lowering preserves selected ops, lowered terminator families, and
    original block ids while assigning stable globally unique labels such as
    `F0.L0`, `F0.L1`, ... in emit order
  - current dump authority now surfaces control transfers by emitted label
    name rather than by raw layout index
  - the surface now also has direct `lower + dump` convenience entrypoints,
    so later consumers/tests can ask for emitted artifacts without restaging
    a separate lifecycle wrapper by hand
  - a first report artifact is now also landed on direct layout input:
    current authority includes per-function shape summaries, per-function
    block-summary offsets, per-block emitted shape summaries, and a few
    ready-made function index lists for call-bearing, fallthrough-bearing,
    and branch-bearing emitted functions
  - that same direct-side artifact surface now also has a first exact dump
    sibling: direct `machine_layout` input can now produce one structured
    emitted report dump that includes report header/index facts plus the full
    emitted program text below it
  - that report surface is now no longer only upstream-driven:
    an existing `MachineEmitProgram` can itself be cloned into a fresh
    report artifact and re-summarized, which makes later local
    emission-prep transforms much easier to stage without bouncing back to
    `machine_layout`
- `ME3 first upstream bridge`: now effectively **~95%**
  - the canonicalized `machine_ir -> machine_select -> machine_layout ->
    machine_emit` bridge is landed as a first real public entrypoint
  - current bridge authority is intentionally still structured rather than
    encoded: it proves that a post-layout consumer can now exist above
    `machine_layout` without forcing final machine-code emission yet
  - bridge-side coverage now also includes direct `machine_ir -> machine_emit`
    dump authority, not only the longer-lived program object path
  - bridge/query authority now also explicitly covers lookup-friendly emitted
    block metadata, so later backend experiments can navigate bridge output
    by function name and emitted block label without reparsing dump text
  - the bridge side now also has a first report-oriented sibling:
    callers can build either a plain emitted program or a full emitted report
    directly from `MachineIrAllocateRewriteReport` input, rather than
    extracting the machine-ir program by hand before continuing downstream
  - that bridge-side report surface now also has a first exact dump sibling,
    so downstream debugging/report consumers can ask for
    `MachineIrAllocateRewriteReport -> machine_emit report dump` directly
    without manually staging the intermediate report object
  - bridge/report lookup now also reaches emitted-block shape by globally
    unique label, so report consumers can navigate by artifact name rather
    than only by `(function_index, block_index)`
  - report shape refresh is now a first explicit public operation rather than
    only an internal helper, so if a later post-emit transform mutates the
    report-owned emitted program, it can recompute block/function summaries
    without rebuilding the whole artifact from upstream first

## File Management Rules

- keep all implementation under `src/machine/lowering/machine_emit/`
- keep all public declarations under `include/machine/emit.h`
- keep tests under `tests/machine/lowering/machine_emit/`

## Current Stop Condition

- Treat the current machine-emit first slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-emit`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default into final encoding once the
  first slice already covers:
  - direct `machine_layout -> machine_emit`
  - canonicalized `machine_ir` bridge through layout
  - stable emitted labels in dumps and control-transfer targets
  - verifier-backed structural checks on emitted block/label surfaces
- Reopen active expansion for:
  - confirmed label/target correctness bugs
  - a concrete downstream consumer need
  - the next deliberately chosen final-encoding mainline
