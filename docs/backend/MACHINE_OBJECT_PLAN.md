# Machine Object / Relocation-Ready Artifact Plan

## Goal

The next backend-facing mainline after `machine_bytes` is to turn the current
byte/report/fixup state into a **minimal object-facing artifact layer**.

This stage should answer:

- how are byte-bearing sections exposed as object-facing containers?
- how do symbols and fixups hang off those sections as first-class data?
- how can later relocation/object-file work consume one stable artifact
  instead of reinterpreting `machine_bytes` report arrays directly?

In short:

- `machine_bytes` decides concrete bytes, byte spans, and first fixup facts
- `machine_object` decides first object-facing section/symbol/fixup containers

## Module Boundary

Create a new sibling module:

- `include/machine/object.h`
- `src/machine/object/machine_object/`
- `tests/machine/object/machine_object/`

Do not fold this into:

- `src/machine/object/machine_bytes/`
- `src/machine/lowering/machine_encode/`

`machine_bytes` should remain the byte-bearing authority.
`machine_object` should become the first object-facing post-bytes consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineBytesReport`
- preserve current `.text` byte image as one object section
- preserve current defined function/block symbols plus unresolved external call
  symbols
- preserve current call/control fixups while attaching them to section/symbol
  containers
- dump sections, symbols, and fixups as one object-facing snapshot

Do not start with:

- target-specific relocation encodings
- multi-section layout policy beyond current `.text`
- archive/linker semantics

## First Public APIs

- `machine_object_build_from_machine_bytes_report(...)`
- `machine_object_build_from_machine_bytes_program(...)`
- `machine_object_build_from_machine_ir_report(...)`
- `machine_object_verify_file(...)`
- `machine_object_dump_file(...)`

## Staging

### Stage MO1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MO2: First Lowering From `machine_bytes`

- preserve `.text` section bytes
- preserve symbol/fixup surfaces
- section-local symbol/fixup queries

### Stage MO3: First Upstream Bridge

- `machine_ir -> ... -> machine_bytes -> machine_object`

## Current Position

- `MO1 representation skeleton`: now effectively **~100%**
  - the first `machine_object` sibling is landed with public structs,
    lifecycle, verifier, dump, and isolated regression coverage
- `MO2 first lowering from machine_bytes`: now effectively **~100%**
  - the first object file can now be built directly from `MachineBytesReport`
    or `MachineBytesProgram`
  - current lowering preserves one `.text` section, object symbols, and object
    fixups as first-class arrays rather than leaving consumers on raw
    `machine_bytes` report arrays forever
  - section-local symbol/fixup queries and section-byte copying are landed
- `MO3 first upstream bridge`: now effectively **~100%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current byte-bearing pipeline into `machine_object`
  - current object dump now exposes sections, symbols, and fixups together as
    one object-facing snapshot
  - the first full-repo integration checkpoint is now landed too: the module
    is wired into `Makefile`, included in `make test`, and the bridge path is
    covered by isolated regression tests
  - the object artifact now also preserves the upstream
    `MachineBytesTargetProfile`, so later relocation/container/image layers
    can tell whether they are consuming generic bytes or the current
    `riscv32-preview` lane instead of silently dropping that backend choice
  - that same object-facing surface now also includes one first target-policy
    summary plus one first fixup-family summary above the raw object fixup
    array, so consumers can recover preview-honesty policy and direct
    `call` / `primary-control` / `secondary-control` / data-side fixup counts
    without hand-restaging those scans outside `machine_object`
  - that same consumer-facing surface now also has one first structured
    report artifact above the raw object file, so later consumers can stay on
    cached target-policy / family / section / symbol / fixup summaries
    instead of rewalking raw object tables every time
  - preview target-byte-offset / fallthrough honesty is now also verifier-
    backed at the object boundary itself instead of being only an upstream
    bytes-layer convention: malformed preview object artifacts with drifted
    target byte offsets or fake zero-patch secondary control fixups are now
    rejected directly by `machine_object_verify_file(...)`

## File Management Rules

- keep all implementation under `src/machine/object/machine_object/`
- keep all public declarations under `include/machine/object.h`
- keep tests under `tests/machine/object/machine_object/`

## Current Stop Condition

- Treat the current first machine-object slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-object`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the initial
  machine-object skeleton after direct lowering, object-query, upstream
  bridge, and repository integration landed together.
- The current first machine-object slice should now be read as
  **checkpoint-ready**: one `.text` section container, section-local
  symbol/fixup attachment, verifier-backed object checks, dump/query helpers,
  and the first upstream `machine_ir` bridge are all landed strongly enough
  that default next work can reasonably switch from “finish the first object
  artifact” to the now-landed `machine_reloc` mainline unless a concrete
  object-surface bug appears.
- Do not keep expanding this line by default into full target relocation
  encoding or final object-file serialization once the first slice already
  covers:
  - direct `machine_bytes -> machine_object`
  - one `.text` section container
  - symbol and fixup arrays attached to that section
  - verifier-backed object-facing artifact checks
