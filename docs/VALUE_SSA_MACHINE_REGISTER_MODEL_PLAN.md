# Value SSA Machine Register Model Plan

## Goal

This document starts the next allocator-adjacent mainline after the current
allocator-result-shaping checkpoint:

- keep the current allocator's abstract-color world intact
- add one explicit machine-facing register-model layer above it
- do that before real instruction selection / emission

In short:

- today we can allocate SSA values to abstract colors and spill slots
- next we want one formal surface that says what those colors *mean* in a
  machine/register world

This is intentionally the first slice, not the whole machine backend.

## What This Phase Is

This phase is about introducing a small public description of the target
register universe that later backend consumers and later allocator policy can
both share.

The first useful questions this layer should answer are:

- how many allocatable machine registers exist?
- what are their stable ids and printable names?
- which register class does each machine register belong to?
- which registers are caller-clobbered vs callee-preserved?
- given an abstract allocator color, which machine register does it currently
  correspond to under one chosen register bank description?

That is enough to start turning "color 0/1/2" into something more explicit
without yet forcing the allocator itself to understand all machine
constraints.

## Non-Goals

This first slice does **not** yet do:

- precolored SSA values in the allocator mainline
- class-constrained coloring
- call-aware recoloring/retry
- fixed-operand instruction constraints
- parallel copy resolution
- frame layout
- instruction selection
- final machine code emission

Those belong later.

## Proposed First Surface

Introduce one new public artifact family as a sibling module beside
`value_ssa_alloc`, rather than hiding it inside the allocator's own source
split forever.

Working shapes:

- `ValueSsaMachineRegisterClass`
- `ValueSsaMachineRegisterDesc`
- `ValueSsaMachineRegisterBank`
- `ValueSsaFunctionMachineAllocationView`
- `ValueSsaProgramMachineAllocationView`

### 1. Register Bank

This is the machine-side static description:

- register count
- class count
- stable register ids
- printable register names
- class membership
- allocatable flag
- caller-clobber flag
- callee-preserve flag

The first bank can stay very small and generic, for example one flat general
purpose class:

- `r0`
- `r1`
- `r2`
- ...

The important part is not target realism yet, but having a stable machine
vocabulary.

### 2. Machine Allocation View

This is the projection from shaped allocation results into one concrete
register bank:

- if value is colored with abstract color `C`, map that to machine register
  `R[C]`
- if value is spilled, keep spill-slot information
- preserve function/program summaries in machine-facing terms

This layer should remain additive:

- shaped abstract layout stays valid
- machine-facing projection becomes the preferred next consumer surface once a
  bank is supplied

## Why This Layer Helps

### A. Clear Boundary

It creates a cleaner step between:

- allocator result shaping
- actual target lowering

without forcing target lowering to understand abstract colors directly.

### B. Shared Vocabulary

Later work on:

- precolored values
- call clobbers
- register classes
- instruction constraints

can point at one shared machine register bank instead of inventing ad hoc
tables in each subsystem.

### C. Conservative First Move

We can start machine-facing work without reopening allocator mainline
semantics too aggressively.

## Staging

### Stage M1: Static Register Bank

Add:

- public machine register structs
- init/free helpers
- query helpers
- dump helpers
- one simple flat register bank builder

No allocator behavior change yet.

### Stage M2: Shaped Machine Allocation Projection

Add:

- builder from `ValueSsaFunctionAllocationLayout` + register bank
- builder from `ValueSsaProgramAllocationLayout` + register bank
- dump/query helpers

Still no allocator behavior change yet.

### Stage M3: First Consumer Migration

Move one downstream-ish consumer to prefer:

- machine-facing allocation view

instead of:

- raw abstract colors

Good candidates:

- allocation reports meant for later backend work
- machine-facing debug dumps

### Stage M4: Feed Later Machine Constraints

Only after the projection layer is stable:

- begin using register-class/call-clobber facts in later allocator or backend
  work

## File Plan

Expected implementation files:

- `include/value_ssa_machine.h`
  - machine-facing APIs
- `src/value_ssa_machine/value_ssa_machine.c`
  - machine module aggregator / local helpers
- `src/value_ssa_machine/value_ssa_machine_core.inc`
  - register-bank + machine-view builders / queries
- `src/value_ssa_machine/value_ssa_machine_dump.inc`
  - dump helpers
- `tests/value_ssa/value_ssa_machine_test.c`
  - focused machine-register-model coverage

Shared data types may still temporarily live in `include/value_ssa_alloc.h`
while this slice is young, but the ownership line for implementation and
public entrypoints is now the sibling `value_ssa_machine` module.

## Progress Snapshot

- `Stage M1 static register bank`: **100%**
  - landed as a real sibling module under `src/value_ssa_machine/`
  - flat-bank construction, init/free helpers, and dump coverage are green
- `Stage M2 shaped machine allocation projection`: **80-85%**
  - function/program machine-view builders and dumps are landed
  - focused machine-view regression coverage is split into its own test target
  - next natural work inside M2 is to add a few direct query helpers instead
    of forcing all later consumers through dump text or raw struct walks
- `Stage M3 first consumer migration`: **0-10%**
  - not meaningfully started yet
  - the module boundary is now ready for one downstream-ish consumer to switch
    to machine-facing views
- `Stage M4 feed later machine constraints`: **0%**
  - intentionally not started yet

## Immediate Next Step

Implement **Stage M1 + the smallest useful part of M2**:

- define one flat machine register bank
- add register-bank dump/query helpers
- add one function/program machine allocation projection from shaped layout
- lock it with focused regression coverage
