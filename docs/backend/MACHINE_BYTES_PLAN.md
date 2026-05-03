# Machine Bytes / Byte-Bearing Encoding Plan

## Goal

The next backend-facing mainline after `machine_encode` is to turn
offset-aware encoded programs into a **byte-bearing artifact layer**.

This stage should answer:

- what concrete bytes belong to each encoded block?
- what byte span does each function/block occupy once bytes exist as data?
- how can later relocation/object/debug consumers inspect bytes directly
  without re-deriving them from abstract unit counts?

In short:

- `machine_encode` decides abstract code-unit offsets and offset-aware shape
- `machine_bytes` decides first concrete byte arrays and byte-bearing spans

## Module Boundary

Create a new sibling module:

- `include/machine/bytes.h`
- `src/machine/object/machine_bytes/`
- `tests/machine/object/machine_bytes/`

Do not fold this into:

- `src/machine/lowering/machine_encode/`
- `src/machine/lowering/machine_emit/`
- `src/machine/lowering/machine_layout/`

`machine_encode` should remain the offset-aware encoding-prep authority.
`machine_bytes` should become the first byte-bearing post-encode consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineEncodeProgram`
- preserve encoded function structure, block order, labels, and target shape
- materialize one concrete opcode-tag byte per selected op plus a small first
  structural payload for immediate/call families
- materialize one concrete terminator-tag byte per terminator plus a small
  first structural payload for return/target/compare families
- compute stable byte `start/end` offsets from those byte arrays
- dump bytes in hex while still surfacing target labels and target byte offsets

Do not start with:

- ISA-specific operand payload bytes
- real relocations/fixups
- variable-width branch relaxation
- object-file containers

## First Public APIs

- `machine_bytes_lower_program_from_machine_encode(...)`
- `machine_bytes_lower_program_from_machine_ir(...)`
- `machine_bytes_verify_program(...)`
- `machine_bytes_dump_program(...)`

## Staging

### Stage MB1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MB2: First Lowering From `machine_encode`

- preserve encoded block order and metadata
- materialize byte arrays and byte spans
- surface bytes and target byte offsets in dumps

### Stage MB3: First Upstream / Artifact Bridges

- `machine_ir canonicalize -> machine_select -> machine_layout -> machine_emit -> machine_encode -> machine_bytes`
- direct artifact bridges from `MachineEncodeReport` and
  `MachineIrAllocateRewriteReport`

## Current Position

- `MB1 representation skeleton`: now effectively **~98%**
  - the first `machine_bytes` module is landed as a real sibling with public
    structs, lifecycle, verifier, dump, report, and isolated regression
    coverage
  - bytes are now first-class per-block data rather than implicit counts:
    each block carries byte offsets plus a concrete byte array
- `MB2 first lowering from machine_encode`: now effectively **~99%**
  - one first lowering path from verifier-legal `machine_encode` is landed
  - current lowering now also has a first minimal variable-length byte model
    instead of flattening every op/terminator to one byte: several op and
    terminator families already materialize different byte lengths, which is
    still target-agnostic but meaningfully closer to real encoding shape
  - those payload bytes are now also less placeholder-like than the earliest
    byte-bearing checkpoint: immediate families, call arg counts/arg kinds,
    target pairs, compare ops, and `branch_on_true` state now surface into the
    emitted bytes directly instead of falling back to generic filler markers
  - direct dumps now surface byte arrays in hex alongside target labels and
    target byte offsets
  - this stage is now also deliberately reopened for the first target-aware
    consumer need: profile-aware lowering/report entrypoints can select a
    preview byte lane explicitly, and `riscv32-preview` now emits fixed-width
    4-byte RISC-V instruction skeletons for the landed subset instead of only
    reusing the generic tag-byte format under a different later-stage header
  - that same `riscv32-preview` lane now also patches known internal
    PC-relative displacements directly into emitted `jal` / branch words for
    local control-flow edges and same-program direct calls instead of leaving
    all target immediates as zero-filled skeleton placeholders
  - that same preview lane is now also less likely to silently truncate
    immediate-heavy RV32IM-shaped input: larger immediates and nontrivial
    compare-immediate forms may now expand into multi-instruction preview
    sequences (`lui/addi`, materialize-then-alu/cmp/branch, materialize-then-
    store/return) instead of pretending one narrow immediate instruction was
    always sufficient
  - that same preview lane now also starts to surface a first minimal call
    convention instead of treating every call as bare control transfer only:
    preview call args may now be materialized/copied into `a0..a7`, and
    preview call results may now stay in `a0` or be copied/spilled after the
    `jal` when the current selected op shape requires it
  - that same call-lowering slice is now also less "eight args only" than
    the earlier preview checkpoint: when a selected call carries more than
    eight args, the caller now reserves/restores a first aligned stack-arg
    area around the `jal`, places overflow args there, and can also load
    spill-backed call args from the caller frame instead of pretending those
    values were already sitting in a convenient temporary register
  - that same preview lane now also consumes a broader slice of spill-backed
    value flow honestly instead of routing it through pseudo-register
    stand-ins: selected `copy`, `materialize_imm`, `load_*`, `store_*`,
    `alu`, `alui`, `return_spill`, plain branch, and compare-branch paths may
    now load spill operands from the caller frame, compute through real
    scratch registers, and write spill results back to stack slots, while
    branch PC-relative immediates are now based on the real branch/jump word
    position even when spill/immediate setup bytes appear earlier in the same
    terminator region
  - that same spill-backed value slice now also reaches selected compare
    result production instead of stopping one layer early: `cmp` / `cmpi`
    may now prepare spill operands from stack, produce their boolean result
    in a real scratch/result register, and spill that result back to the
    caller frame when the selected op result lives in `spill.N`
  - that same preview memory slice is now also more honest about large
    local/global slot offsets instead of only large spill/frame offsets:
    `load_local`, `load_global`, `store_local`, and `store_global` no longer
    rely on slot offsets accidentally fitting the signed 12-bit immediate
    field, because large slot offsets now materialize the byte offset, add it
    to the base register, and then issue the final zero-offset load/store
  - that same preview lane now also has one explicit register-bank honesty
    boundary instead of silently aliasing larger logical register files
    through modulo mapping: current `riscv32-preview` support is intentionally
    capped at 8 logical selected machine registers (`reg.0..reg.7` mapping to
    `a0..a7`), and larger register banks now fail explicitly at the
    `machine_bytes` boundary
  - that same preview arithmetic slice is now also regression-locked for one
    first explicit RV32M-shaped subset rather than only implicitly supported
    in encoder code: focused coverage now checks register-register
    `mul` / `div` / `mod` lowering all the way to the expected
    `funct7=0x01` R-type preview words, so the current preview lane can more
    honestly be described as covering a first **RV32IM-shaped arithmetic**
    subset rather than only RV32I-style ALU/control forms
  - preview control lowering is now also more honest about layout-level
    fallthrough structure instead of always spelling every transfer as
    branch-plus-jump: `fallthrough` now carries zero emitted bytes, preview
    `brft.*` / `cmpbrft.*` families now emit only the taken-side branch word,
    `branch_on_true` now affects the actual preview branch encoding rather
    than only dump/report metadata, and preview control fixup patch spans now
    track the real branch/jump word positions after any local setup bytes
  - that same preview control/call patching line now also has one first
    explicit honest-failure boundary instead of silently truncating internal
    PC-relative targets that do not fit the current instruction form:
    direct preview lowering now rejects out-of-range internal branch/jump/call
    displacements rather than wrapping them into malformed `B`/`J` immediates
- `MB3 first upstream / artifact bridges`: now effectively **~100%**
  - the canonicalized
    `machine_ir -> machine_select -> machine_layout -> machine_emit -> machine_encode -> machine_bytes`
    bridge is landed as a first real public entrypoint
  - direct artifact bridges are also landed from `MachineEmitLowerReport`,
    `MachineEncodeReport`, and `MachineIrAllocateRewriteReport`
  - the first structured byte-report layer already includes per-function byte
    summaries, per-block byte summaries, and high-level function index sets
    for call/fallthrough/branch families
  - that same report side now also has a first whole-program byte-layout
    view: callers can ask for total program byte count, per-function byte
    spans, and direct block lookup by whole-program byte offset rather than
    only function-local byte offsets
  - report-side summaries now also count real op/terminator byte totals rather
    than only raw op/terminator item counts, so artifact consumers can read
    byte-bearing summaries without re-deriving encoded widths by hand
  - raw byte access is now also less dump-only than before: callers can read
    one block's byte slice directly, copy one function/program/report into a
    flat byte image, and resolve whole-program byte offsets on the program side
    instead of needing the report artifact first
  - that raw artifact side now also has first direct program/report convenience
    helpers for total byte count, absolute per-function byte spans on the raw
    program surface, and per-function/per-block byte copying without forcing
    later consumers to rebuild those slices by hand
  - that same consumer-facing bytes surface now also has a first explicit
    target-policy summary instead of leaving current preview capabilities
    implicit in the lowering code: callers can query profile/program/report
    policy facts such as the current logical-register cap, internal
    PC-relative patch honesty, direct-fallthrough honesty, paired global
    address formation, and the current RV32M-shaped arithmetic slice
  - the byte-bearing report surface now also has a first structured reference
    layer above raw bytes: call sites and control-flow targets are surfaced as
    reference summaries with owner offsets, patch offsets, and resolved target
    labels/offsets so later relocation/fixup consumers do not need to reverse-
    engineer those sites from dump text or ad hoc byte scans
  - that report-side consumer surface now also has a first explicit fixup
    artifact above those references: call/control target sites can now be
    enumerated directly as fixup summaries with target kind, owner span, patch
    span, and resolved target label/offset metadata instead of forcing later
    relocation work to reinterpret the reference layer manually every time
  - the report artifact now also has a first minimal symbol layer above raw
    bytes/fixups: defined function and block symbols plus unresolved external
    call symbols are surfaced directly, and fixups can now resolve one target
    symbol index instead of only carrying raw target text
  - that object-facing shape now also has a first explicit section summary:
    the current byte-bearing report can surface one minimal `.text` section
    with byte span, function/block counts, defined-symbol count, and fixup
    count, so downstream relocation/object work no longer needs to invent the
    first section container from scratch
  - section-oriented consumers can now also stay on that same artifact instead
    of restaging symbol/fixup grouping manually: the report can expose the
    symbol slice and fixup slice belonging to `.text`, and the report dump now
    prints section, symbol, and fixup summaries together as one object-facing
    snapshot
  - the direct upstream bridge is now also no longer “profile-blind until
    ELF”: `machine_ir`-report-side profile-aware builds can carry the selected
    byte lane through bytes/object/reloc/container so later consumers see the
    preview `.text` image rather than only a preview header policy

## File Management Rules

- keep all implementation under `src/machine/object/machine_bytes/`
- keep all public declarations under `include/machine/bytes.h`
- keep tests under `tests/machine/object/machine_bytes/`

## Current Stop Condition

- Treat the current first machine-bytes slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-bytes`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the initial
  machine-bytes skeleton after direct lowering, report/query, and upstream
  artifact bridges landed together.
- The current first machine-bytes slice should now be read as
  **checkpoint-ready**: byte arrays, payload-aware lowering, byte-report/query
  surfaces, reference/fixup summaries, and minimal symbol summaries are all
  landed strongly enough that default next work can reasonably switch from
  “finish the first byte artifact” to “choose the next relocation/object-file
  mainline” unless a concrete byte/fixup bug appears.
- Do not keep expanding this line by default into relocations/object files
  once the first slice already covers:
  - direct `machine_encode -> machine_bytes`
  - canonicalized `machine_ir` bridge through encode
  - concrete byte arrays and byte offsets in dumps
  - verifier-backed structural checks on byte-bearing block surfaces
- Reopen active expansion for:
  - confirmed byte/target correctness bugs
  - a concrete relocation/object/debug consumer need
  - the next deliberately chosen relocation/object-file mainline
