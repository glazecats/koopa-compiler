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
  - the current relocation artifact is now also slightly target-aware for the
    active RISC-V preview line: when `machine_object` carries the
    `riscv32-preview` target profile and an internal target byte offset is
    already known, the relocation record preserves the matching PC-relative
    preview addend instead of forcing every relocation addend to stay `0`
  - that same reopened RISC-V preview line is now also more honest about
    fallthrough-shaped control in direct profile-aware builds: if the preview
    bytes layer records a zero-byte secondary edge for a layout fallthrough,
    `machine_reloc` now sees only the one real primary control relocation
    that still has a concrete patch span, instead of reviving a second fake
    relocation for the semantic-but-unpatched fallthrough edge

## Reopened RISC-V Slice

- For the current reopened backend-honesty mainline, treat `machine_reloc` as
  roughly **~94%** complete.
- The landed slice now covers:
  - preview addend preservation for same-program internal call/control targets
  - direct preview fallthrough-control relocation honesty
    direct `riscv32-preview` builds now keep only the relocation whose
    branch/jump word actually exists in `.text`
  - one first data-side global-access relocation slice for the active
    RISC-V preview line: small preview `load_global` / `store_global`
    families may now survive as paired explicit data-address plus
    data-load/store relocations that target the matching global-object
    symbols instead of disappearing back into raw address-forming code bytes
    only
  - that same relocation-facing surface now also reports those data-side
    relocations through the consumer-facing family summary instead of forcing
    callers to rediscover them from raw relocation rows manually
  - that same preview-honesty slice is now also public/queryable instead of
    being latent only in individual relocation records and dump strings:
    callers can ask `machine_reloc` for a small target-policy summary that
    explicitly says whether the current artifact preserves preview
    PC-relative addends and direct-fallthrough honesty for the selected
    target profile
  - that same relocation-facing surface now also exposes one first relocation-
    family summary above raw row iteration, so consumers can recover direct
    `call` / `primary-control` / `secondary-control` counts without rescanning
    the relocation array manually
  - that same consumer-facing surface now also has one first structured
    report artifact above the raw relocation file, so later consumers can
    stay on cached target-policy / family / section / relocation summaries
    rather than rewalking raw relocation arrays every time
  - that same report-facing artifact now also exposes one first section-local
    relocation-summary slice instead of forcing later consumers to bounce
    back down through the raw file just to recover "the relocation summaries
    that belong to this one relocation section"; current report helpers may
    now return the cached summary rows for one section directly
  - preview addend/fallthrough honesty is now also verifier-backed at the
    relocation boundary itself instead of remaining only a build-path habit:
    malformed preview relocations with drifted addends or fake zero-patch
    secondary control entries are rejected directly by
    `machine_reloc_verify_file(...)`
- The remaining downstream pressure is mostly no longer inside raw relocation
  construction itself, but in keeping later consumers such as
  `machine_container` / `machine_elf` explicit about the information boundary
  between:
  - direct preview builds, which still know which control edges had
    `patch_byte_count == 0`
  - reparsed or reprofiled generic ELF, which may preserve two control
    relocation records because that zero-patch distinction is not recoverable
    from the imported relocation table alone

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
