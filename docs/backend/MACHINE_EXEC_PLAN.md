# Machine Exec / Executable-Prep Plan

## Goal

The next backend-facing mainline after `machine_image` is to turn the current
single-image view into a **conservative executable-preparation layer**.

This stage should answer:

- does the current image already have a valid entry address?
- does the current image still carry unresolved relocation obligations?
- which image segments should be treated as executable-ready code segments?

In short:

- `machine_image` decides first load-image structure plus unresolved image
  obligations
- `machine_exec` decides whether that single image is already executable-ready
  enough to hand to later runtime/loader experiments

## Module Boundary

Create a new sibling module:

- `include/machine/exec.h`
- `src/machine/runtime/machine_exec/`
- `tests/machine/runtime/machine_exec/`

Do not fold this into:

- `src/machine/runtime/machine_image/`
- `src/machine/object/machine_elf/`

`machine_image` should remain the load-image authority.
`machine_exec` should become the first executable-candidate authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineImageFile`
- require one known entry symbol/address
- require zero unresolved relocations
- preserve the current image-owned segment order and virtual addresses
- surface one first executable-segment permission view

Do not start with:

- multi-object linking
- relocation patching back into ISA-accurate final bytes
- OS-specific program headers or loader metadata
- process startup/runtime semantics beyond "entry exists and relocations are
  resolved"

## First Public APIs

- `machine_exec_build_from_machine_image_file(...)`
- `machine_exec_build_from_machine_image_report(...)`
- `machine_exec_build_from_machine_ir_report(...)`
- `machine_exec_verify_file(...)`
- `machine_exec_dump_file(...)`

## Staging

### Stage MEX1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MEX2: First Lowering From `machine_image`

- entry-required executable candidate
- unresolved-relocation rejection
- executable-segment permission surfacing
- first raw/report query helpers over executable segments

### Stage MEX3: First Upstream Bridge

- `machine_ir -> ... -> machine_image -> machine_exec`

## Current Position

- `MEX1 representation skeleton`: now effectively **~100%**
  - the first `machine_exec` sibling is landed with public structs,
    lifecycle, verifier, dump, summary/query helpers, and isolated
    regression coverage
  - the first surface now also has a report artifact above the raw exec file,
    including cached header and segment summaries plus executable-segment
    index subsets for later consumers
- `MEX2 first lowering from machine_image`: now effectively **~99%**
  - one first lowering path from verifier-legal `machine_image` is landed
  - current lowering requires one known entry address and rejects images that
    still carry unresolved relocations
  - current exec verification now also requires that the chosen entry address
    is actually covered by an executable segment instead of only checking
    that the image had some entry address at all
  - current executable shaping preserves image segment order / virtual
    addresses while surfacing one first executable permission policy over the
    current single-image segments
  - raw/report-side query helpers now also expose executable-segment counts,
    lookup by name, filtered executable-segment traversal, direct entry-
    segment lookup, and segment lookup by covered virtual address instead of
    forcing downstream consumers to rescan every segment manually
- `MEX3 first upstream bridge`: now effectively **~99%**
  - a first direct bridge is landed from `MachineIrAllocateRewriteReport`
    through the current image pipeline into `machine_exec`
  - that same bridge story now also reaches existing `MachineImageFile` and
    `MachineImageReport` artifacts for raw exec files, exec reports, and
    direct dump/report-dump helpers
  - that same downstream bridge boundary now also has a first small artifact
    lifecycle of its own through exec-file clone, report refresh, and
    direct dump-from-file helpers, so later executable-facing consumers can
    keep one exec artifact alive and re-summarize it locally instead of
    rebuilding the whole bridge stack every time
  - that same bridged report boundary is now also more consumer-facing than
    the first summary-only cut: callers can start from a report overview
    artifact, recover the entry segment directly, query segment artifacts by
    name or covered virtual address, and walk executable / non-executable /
    entry segment filter views without dropping back to ad hoc table scans
  - that same upstream bridge family now also reaches the serialized and
    pre-image ELF side directly: callers can continue from `MachineElfFile`,
    `MachineElfReport`, ELF bytes, and profile-aware `machine_ir` bridge
    variants into raw exec files, exec reports, direct dumps, and report
    dumps without hand-staging a separate `machine_image` artifact first
  - the current bridge already proves that a post-image executable-prep
    consumer can be exercised end-to-end without reopening earlier backend
    sibling layers

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_exec/`
- keep all public declarations under `include/machine/exec.h`
- keep tests under `tests/machine/runtime/machine_exec/`

## Current Stop Condition

- Treat the current first machine-exec slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-exec`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into full linker /
  loader / runtime semantics once the first slice already covers:
  - direct `machine_image -> machine_exec`
  - entry-required executable-candidate shaping
  - unresolved-relocation rejection
  - first executable-segment query/report surface
- Reopen active expansion for:
  - a concrete multi-image/linker consumer need
  - a confirmed executable-entry / relocation-readiness correctness bug
  - the next deliberately chosen runtime / loader mainline
