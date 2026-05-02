# Machine Container / Final Object Serialization Plan

## Goal

The next backend-facing mainline after `machine_reloc` is to turn the current
relocation/object state into a **minimal serialized object-file container**.

This stage should answer:

- how is the relocation-facing artifact serialized into one stable byte image?
- how are section/symbol/relocation tables surfaced inside that container?
- how can later target-specific object-file work consume one final serialized
  artifact instead of reassembling bytes from `machine_reloc` every time?

In short:

- `machine_reloc` decides explicit relocation tables/records
- `machine_container` decides first final serialized object container bytes

## Module Boundary

Create a new sibling module:

- `include/machine/container.h`
- `src/machine/object/machine_container/`
- `tests/machine/object/machine_container/`

Do not fold this into:

- `src/machine/object/machine_reloc/`
- `src/machine/object/machine_object/`

`machine_reloc` should remain the relocation-facing authority.
`machine_container` should become the first final serialized post-reloc consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineRelocFile`
- preserve the owned relocation artifact as embedded state
- serialize a stable custom byte container with header, section table, symbol
  table, relocation table, string table, and payload area
- preserve section data ordering and section-local relocation grouping
- dump container layout and serialized section spans as one container-facing
  snapshot

Do not start with:

- ELF/COFF/Mach-O compatibility
- target-specific relocation opcode encoding
- linker/archive semantics

## First Public APIs

- `machine_container_build_from_machine_reloc_file(...)`
- `machine_container_build_from_machine_ir_report(...)`
- `machine_container_verify_file(...)`
- `machine_container_dump_file(...)`

## Staging

### Stage MCN1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MCN2: First Serialization From `machine_reloc`

- build stable byte container
- preserve section payload spans
- expose layout/query helpers

### Stage MCN3: First Upstream Bridge

- `machine_ir -> ... -> machine_reloc -> machine_container`

## Current Position

- `MCN1 representation skeleton`: now effectively **~100%**
  - the first `machine_container` sibling is landed with public structs,
    lifecycle, verifier, dump, and isolated regression coverage
- `MCN2 first serialization from machine_reloc`: now effectively **~100%**
  - the first serialized container can now be built directly from
    `MachineRelocFile`
  - current serialization preserves header/table offsets, string table, and
    section payload spans as one stable custom byte image
  - layout summaries, section lookup, and owned byte-copy helpers are landed
- `MCN3 first upstream bridge`: now effectively **~100%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current relocation pipeline into `machine_container`
  - the module is wired into `Makefile`, included in `make test`, and covered
    by focused regression tests

## File Management Rules

- keep all implementation under `src/machine/object/machine_container/`
- keep all public declarations under `include/machine/container.h`
- keep tests under `tests/machine/object/machine_container/`

## Current Stop Condition

- Treat the current first machine-container slice as checkpoint-worthy once
  the following stay green together:
  - `make test-machine-container`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the initial
  machine-container skeleton after direct serialization, layout-query,
  upstream bridge, and repository integration landed together.
- The current first machine-container slice should now be read as
  **checkpoint-ready**: serialized container bytes, verifier-backed layout
  checks, dump/query helpers, and the first upstream `machine_ir` bridge are
  all landed strongly enough that default next work can reasonably switch from
  “finish the first final serialized container” to the now-landed
  `machine_elf` mainline unless a concrete container-surface bug appears.
- Do not keep expanding this line by default into full ELF/COFF/Mach-O
  compatibility once the first slice already covers:
  - direct `machine_reloc -> machine_container`
  - stable header/table/string/payload serialization
  - owned byte-image copy/query helpers
  - verifier-backed container artifact checks
