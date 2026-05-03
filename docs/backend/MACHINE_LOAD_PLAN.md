# Machine Load / Loader-Prep Plan

## Goal

The next backend-facing mainline after `machine_exec` is to turn the current
single-image executable candidate into a **conservative loader-preparation
layer**.

This stage should answer:

- which executable segments become explicit in-memory load mappings?
- what byte-bearing memory spans would a minimal loader map for the current
  program?
- which mapped segment contains the entry PC?

In short:

- `machine_exec` decides whether a single image is executable-ready enough
- `machine_load` decides the first explicit load-map view over that executable

## Module Boundary

Create a new sibling module:

- `include/machine/load.h`
- `src/machine/runtime/machine_load/`
- `tests/machine/runtime/machine_load/`

Do not fold this into:

- `src/machine/runtime/machine_exec/`
- `src/machine/runtime/machine_image/`

`machine_exec` should remain the executable-candidate authority.
`machine_load` should become the first explicit loader-map authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineExecFile`
- preserve the current segment order, permissions, and virtual addresses
- materialize owned byte-bearing load segments from the current image bytes
- keep `memory_byte_count == file_byte_count` for now
- surface the entry-containing load segment explicitly

Do not start with:

- page-size rounding or virtual-memory allocation policy
- zero-fill / BSS growth beyond current file bytes
- stack / argv / env / auxv setup
- process scheduler / runtime semantics

## First Public APIs

- `machine_load_build_from_machine_exec_file(...)`
- `machine_load_build_from_machine_exec_report(...)`
- `machine_load_build_from_machine_ir_report(...)`
- `machine_load_verify_file(...)`
- `machine_load_dump_file(...)`

## Staging

### Stage MLD1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MLD2: First Lowering From `machine_exec`

- byte-bearing load segments
- explicit file-byte vs memory-byte accounting
- entry-segment load mapping
- first raw/report load-segment query helpers

### Stage MLD3: First Upstream Bridge

- `machine_ir -> ... -> machine_exec -> machine_load`

## Current Position

- `MLD1 representation skeleton`: now effectively **~100%**
  - the first `machine_load` sibling is landed with public structs,
    lifecycle, verifier, dump, summary/query helpers, and isolated
    regression coverage
  - the first surface now also has a report artifact above the raw load file,
    including cached header / segment summaries plus executable and
    non-executable segment index subsets for later consumers
- `MLD2 first lowering from machine_exec`: now effectively **~100%**
  - one first lowering path from verifier-legal `machine_exec` is landed
  - current lowering preserves segment order, permissions, and virtual
    addresses while materializing explicit owned load-segment byte payloads
  - current load accounting now also surfaces both file-byte and memory-byte
    totals instead of leaving the mapped-memory view implicit inside
    `machine_exec`
  - current load mapping is no longer only `mem == file`: memory spans are
    now rounded up by a profile-owned load alignment policy, and the tail
    beyond file bytes is explicitly zero-filled inside the owned load-segment
    payload
  - that same alignment/zero-fill rule is now also promoted into an explicit
    target-policy surface instead of living only as hidden implementation
    behavior: raw files, reports, overview artifacts, and report dumps can
    all surface the active load mapping policy directly
  - the same load-map layer now also exposes one first explicit memory-image
    consumer surface above the segment table: callers can recover a cached
    memory-span summary, read one mapped byte by virtual address, and copy
    the current whole mapped memory image into one owned flat byte buffer
  - raw/report-side query helpers now also expose entry-segment lookup,
    executable-segment traversal, and covered-virtual-address lookup over the
    explicit load-map artifact
- `MLD3 first upstream bridge`: now effectively **~99%**
  - a first direct bridge is landed from `MachineExecFile` /
    `MachineExecReport` into `machine_load`
  - that same upstream bridge family already reaches `MachineImageFile`,
    `MachineImageReport`, `MachineElfFile`, `MachineElfReport`, ELF bytes,
    and profile-aware `machine_ir` bridge variants for raw load files, load
    reports, direct dumps, and report dumps
  - that same bridged consumer line now also preserves upstream ELF
    provenance instead of flattening it away at load-prep time: raw load
    files, load reports, and dump/report-dump helpers surface the source ELF
    artifact summary carried through `machine_image`
  - that same bridged report boundary is now also more consumer-facing than
    the first summary-only cut: callers can start from a load overview
    artifact, recover the entry segment directly, query segment artifacts by
    name or covered virtual address, and walk executable / non-executable /
    entry load-segment filter views without dropping back to ad hoc scans

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_load/`
- keep all public declarations under `include/machine/load.h`
- keep tests under `tests/machine/runtime/machine_load/`

## Current Stop Condition

- Treat the current first machine-load slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-load`
  - `make test`
  - `git diff --check`
- Do not keep expanding this line by default straight into full process /
  runtime semantics once the first slice already covers:
  - direct `machine_exec -> machine_load`
  - explicit byte-bearing load segments
  - entry-segment load mapping
  - first bridged load report / filter / dump surface
- Reopen active expansion for:
  - a concrete zero-fill / page-map / runtime-launch consumer need
  - a confirmed load-byte / entry-map correctness bug
  - the next deliberately chosen runtime / process mainline
