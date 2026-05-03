# Machine Runtime / Process-Prep Plan

## Goal

The next backend-facing mainline after `machine_load` is to turn the current
loader-prep artifact into a **conservative runtime / process-preparation
layer**.

This stage should answer:

- what initial PC and SP would a minimal runtime launch use?
- what first synthetic runtime memory spans exist beyond the load map itself?
- how can downstream experiments inspect one unified runtime memory view
  without pretending that full OS/runtime semantics already exist?

In short:

- `machine_load` decides the explicit load-map view
- `machine_runtime` decides the first explicit launch-ready runtime view over
  that load map

## Module Boundary

Create a new sibling module:

- `include/machine/runtime.h`
- `src/machine/runtime/machine_runtime/`
- `tests/machine/runtime/machine_runtime/`

Do not fold this into:

- `src/machine/runtime/machine_load/`
- `src/machine/runtime/machine_exec/`

`machine_load` should remain the loader-preparation authority.
`machine_runtime` should become the first runtime / process-preparation
authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineLoadFile`
- preserve existing load segments and entry PC
- add one explicit synthetic stack segment under a profile-owned policy
- surface one initial stack pointer for that stack segment
- expose one first unified runtime memory view over load + stack segments

Do not start with:

- argv / env / auxv materialization
- syscall / scheduler / thread semantics
- page tables or virtual-memory permissions beyond the current segment flags
- signal frames, TLS, or dynamic loader behavior

## First Public APIs

- `machine_runtime_build_from_machine_load_file(...)`
- `machine_runtime_build_from_machine_load_report(...)`
- `machine_runtime_build_from_machine_ir_report(...)`
- `machine_runtime_verify_file(...)`
- `machine_runtime_dump_file(...)`

## Staging

### Stage MRT1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage MRT2: First Lowering From `machine_load`

- preserve load-backed runtime segments
- add one synthetic stack segment
- explicit initial stack-pointer policy
- first raw/report runtime memory query helpers

### Stage MRT3: First Upstream Bridge

- `machine_ir -> ... -> machine_load -> machine_runtime`

## Current Position

- `MRT1 representation skeleton`: now effectively **~100%**
  - the first `machine_runtime` sibling is landed with public structs,
    lifecycle, verifier, dump, report, and isolated regression coverage
- `MRT2 first lowering from machine_load`: now effectively **~100%**
  - current lowering preserves load-backed runtime segments and entry PC
  - the first synthetic stack segment is now added under an explicit profile
    policy (`stack_alignment`, `stack_byte_count`, `stack_gap_byte_count`)
  - current runtime memory surfacing now also exposes initial SP, memory-span
    summary, mapped-byte lookup, and flat whole-image copy helpers over the
    unified runtime view
  - that same runtime-memory surface is now also more downstream-consumer-
    facing than the first cut: callers can resolve segment ownership by flat
    runtime-memory offset, read one mapped byte by offset, and copy one
    segment's owned byte slice directly without rebuilding offset math
  - current runtime launch / stack facts are now also promoted into explicit
    summary artifacts instead of staying implicit in raw fields only:
    callers can recover stack-window and launch (`pc` / `sp`) summaries on
    both raw-file and report sides, plus stack-byte lookup/copy helpers
  - current runtime state surfacing now also reaches bounded window-copy
    consumers directly: callers can copy a runtime-memory window, stack
    window, or initial top-of-stack window without rebuilding those slices
    by hand
  - current runtime span surfacing now also has one first unified region
    model above those separate pieces: callers can enumerate load/gap/stack
    regions and locate one region by index, virtual address, or flat offset
  - the current first-launch surface now also includes one explicit minimal
    initial-stack image summary and copy view just below the initial stack top:
    `argc=0`, `argv` null terminator, `envp` null terminator, and auxv
    terminator pair are now surfaced as a conservative zero-filled image on
    both raw-file and report sides instead of being left completely implicit
- `MRT3 first upstream bridge`: now effectively **~100%**
  - direct bridges are landed from `MachineLoadFile` / `MachineLoadReport`
  - the same bridge family already reaches `MachineExecFile`,
    `MachineExecReport`, `MachineImageFile`, `MachineImageReport`,
    `MachineElfFile`, `MachineElfReport`, ELF bytes, and profile-aware
    `MachineIrAllocateRewriteReport` variants for raw runtime files, reports,
    dumps, and report dumps
  - the same report-side bridge boundary now also exposes flat-offset segment
    lookup/artifact recovery, so downstream consumers can stay inside runtime
    report artifacts instead of manually translating through the raw file
  - that same bridged report boundary now also exposes direct launch-summary
    and stack-summary artifacts, so downstream consumers no longer need to
    restage `pc` / `sp` / stack-window interpretation from separate fields
  - that same bridged artifact layer now also exposes first-class stack and
    launch artifacts, so downstream consumers can start from runtime-state
    objects instead of stitching summaries plus segment lookups manually
  - that same bridged report boundary now also exposes region artifacts and
    load/gap/stack region filters, so downstream consumers can stay in one
    uniform runtime-region query surface
  - that same bridged report/overview boundary now also exposes first-class
    initial-stack summary/artifact views, and report dumps now print the
    conservative initial-stack image shape directly
  - that same bridge family now also preserves upstream ELF provenance
    instead of flattening it away at runtime-prep time: raw runtime files,
    runtime reports, and dump/report-dump helpers surface the embedded
    source-ELF artifact summary carried through `machine_load`
  - that same bridge/lifecycle surface is now also regression-locked across
    the current wrapper matrix instead of being only API-declared: raw-file,
    report, dump, and report-dump entrypoints are now exercised from
    `MachineLoad*`, `MachineExec*`, `MachineImage*`, `MachineElf*`, ELF bytes,
    and profile-aware ELF / `machine_ir` variants, and the current runtime
    clone / report-refresh lifecycle also stays green inside that matrix

## File Management Rules

- keep all implementation under `src/machine/runtime/machine_runtime/`
- keep all public declarations under `include/machine/runtime.h`
- keep tests under `tests/machine/runtime/machine_runtime/`

## Current Stop Condition

- Treat the current first machine-runtime slice as checkpoint-worthy once the
  following stay green together:
  - `make test-machine-runtime`
  - `make test`
  - `git diff --check`
- That checkpoint condition is now currently satisfied for the present first
  slice, so this line should now be treated as checkpoint-ready /
  maintenance-first unless a concrete runtime-memory / launch-surface consumer
  need reopens it.
- Do not keep expanding this line by default straight into full process /
  kernel / runtime semantics once the first slice already covers:
  - direct `machine_load -> machine_runtime`
  - one synthetic stack segment
  - one explicit initial SP policy
  - one unified runtime memory/report/dump surface
- Reopen active expansion for:
  - a concrete argv/env/auxv consumer need
  - a confirmed runtime-memory / stack-placement correctness bug
  - the next deliberately chosen launch/execution consumer layer
