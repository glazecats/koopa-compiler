# Machine Reloc / Relocation Artifact Plan

## Goal

The next backend-facing mainline after `machine_object` is to turn the current
object fixup/symbol state into a **minimal relocation-facing artifact layer**.

This stage should answer:

- how are object fixups surfaced as explicit relocation records?
- how do section-local relocation tables hang off the current object sections?
- how can later object-file serialization consume a stable relocation artifact
  instead of reinterpreting `machine_object` fixups directly?

In short:

- `machine_object` decides first object-facing section/symbol/fixup containers
- `machine_reloc` decides first explicit relocation tables/records

## Module Boundary

Create a new sibling module:

- `include/machine/reloc.h`
- `src/machine/object/machine_reloc/`
- `tests/machine/object/machine_reloc/`

Do not fold this into:

- `src/machine/object/machine_object/`
- `src/machine/object/machine_bytes/`

`machine_object` should remain the object-facing authority.
`machine_reloc` should become the first relocation-facing post-object consumer.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineObjectFile`
- preserve the owned object artifact as embedded state
- surface one relocation table per object section
- preserve current call/control fixups as explicit relocation records
- preserve target-symbol attachment and section-local patch metadata
- dump relocation tables and records as one relocation-facing snapshot

Do not start with:

- target-specific relocation opcode encodings
- ELF/COFF/Mach-O serialization
- archive/linker semantics

## First Public APIs

- `machine_reloc_build_from_machine_object_file(...)`
- `machine_reloc_build_from_machine_ir_report(...)`
- `machine_reloc_verify_file(...)`
- `machine_reloc_dump_file(...)`

## Staging

### Stage MR1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MR2: First Lowering From `machine_object`

- preserve section-local relocation grouping
- preserve target-symbol references and patch spans
- relocation query helpers

### Stage MR3: First Upstream Bridge

- `machine_ir -> ... -> machine_object -> machine_reloc`

## Current Position

- `MR1 representation skeleton`: now effectively **~100%**
  - the first `machine_reloc` sibling is landed with public structs,
    lifecycle, verifier, dump, and isolated regression coverage
- `MR2 first lowering from machine_object`: now effectively **~100%**
  - the first relocation file can now be built directly from
    `MachineObjectFile`
  - current lowering preserves one relocation table per current object
    section, plus explicit relocation records with patch-span and target
    symbol metadata
  - section-local relocation queries and relocation summary helpers are landed
- `MR3 first upstream bridge`: now effectively **~100%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current object pipeline into `machine_reloc`
  - the module is wired into `Makefile`, included in `make test`, and covered
    by focused regression tests

## File Management Rules

- keep all implementation under `src/machine/object/machine_reloc/`
- keep all public declarations under `include/machine/reloc.h`
- keep tests under `tests/machine/object/machine_reloc/`

## Current Stop Condition

- Treat the current first machine-reloc slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-reloc`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the initial
  machine-reloc skeleton after direct lowering, relocation-query, upstream
  bridge, and repository integration landed together.
- The current first machine-reloc slice should now be read as
  **checkpoint-ready**: explicit relocation tables, relocation records,
  verifier-backed relocation checks, dump/query helpers, and the first
  upstream `machine_ir` bridge are all landed strongly enough that default
  next work can reasonably switch from “finish the first relocation artifact”
  to the now-landed `machine_container` mainline unless a concrete
  relocation-surface bug appears.
- Do not keep expanding this line by default into full target relocation
  encoding or final object-file serialization once the first slice already
  covers:
  - direct `machine_object -> machine_reloc`
  - one relocation table per current object section
  - explicit relocation records attached to current target symbols
  - verifier-backed relocation artifact checks
