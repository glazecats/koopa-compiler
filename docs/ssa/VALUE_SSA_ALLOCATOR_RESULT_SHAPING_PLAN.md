# Value SSA Allocator Result Shaping Plan

## Goal

This document defines the next mainline after the current allocator /
live-range-splitting checkpoint:

- take the existing allocation result surface
- reshape it into a more backend-facing, machine-consumer-friendly artifact
- do that **before** introducing real machine register classes / precolored
  nodes / call clobbers

In short:

- today we have an allocator that can color, spill, rewrite, and explain its
  choices
- next we want one result layer that downstream backend stages can consume
  without reverse-engineering allocator internals from many parallel arrays

## Current Situation

Today `ValueSsaAllocationResult` is rich, but it is still allocator-internal in
flavor.

It exposes:

- final abstract color per SSA value
- spill / intended / confirmed / recovered flags
- spill slots
- recovery / retry / eviction metadata
- preferred-color and blocker metadata

That is excellent for allocator debugging, regression locking, and policy work.
But it is not yet the cleanest shape for later backend consumers.

What later consumers will eventually want is closer to:

- one per-value placement summary
- one per-function allocation layout summary
- explicit partition of values into colored vs spilled vs recovered-spilled
- a stable "allocation class" view independent from allocator debugging traces
- a result surface that can survive later machine-register lowering without
  forcing those stages to understand retry/eviction implementation details

## Non-Goals

This phase does **not** yet introduce:

- machine register classes
- precolored nodes
- call-clobber constraints
- physical register names
- final frame layout
- parallel-copy lowering

This is still abstract-color world.

## Proposed Result Surface

Introduce one new public artifact family above `ValueSsaAllocationResult`.

Working names:

- `ValueSsaAllocatedValuePlacement`
- `ValueSsaFunctionAllocationLayout`
- `ValueSsaProgramAllocationLayout`

These names can still be adjusted during implementation, but the intended shape
is:

### 1. Per-Value Placement

For each SSA value, record one normalized placement summary:

- value id
- placement kind:
  - `unallocated`
  - `color`
  - `spill`
- abstract color when colored
- spill slot when spilled
- split-family metadata when already available and meaningful
- whether the value is effectively recovered / optimistic / directly colored

This is intentionally a *summary* layer, not a trace layer.

### 2. Per-Function Layout

For each function, record:

- color budget
- spill slot count
- colored value count
- spilled value count
- value placement table
- maybe small grouped indices/lists for:
  - colored values
  - spilled values

This should be the primary backend-facing artifact for one function.

### 3. Program Layout

For whole-program allocate+rewrite / allocate-program consumers, record:

- function count
- one per-function layout

## Why This Layer Helps

It solves three real problems:

### A. Stable Consumer Contract

Later backend code can ask:

- where does `ssa.N` live?
- is it colored or spilled?
- if colored, what abstract color?
- if spilled, what spill slot?

without knowing allocator retry / eviction / recovery internals.

### B. Cleaner Machine Transition

When machine register modeling arrives, we can map:

- abstract color -> register class / physical register candidate

without forcing machine code to read allocator policy metadata arrays directly.

### C. Better Separation Of Concerns

Keep:

- `ValueSsaAllocationResult` as allocator-policy/debug/result-detail authority

and add:

- shaped layout artifact as downstream-consumer authority

That separation should make later backend work calmer.

## Staging

### Stage R1: Read-Only Shaping Artifact

Add:

- public structs in `include/value_ssa_alloc.h`
- init/free helpers
- builder from existing `ValueSsaAllocationResult`
- dump/query helpers

No allocator behavior change yet.

This stage should only *project* existing results into a cleaner consumer view.

### Stage R2: Queries And Dump Surface

Add stable helpers such as:

- get placement for `ssa.N`
- count colored / spilled placements
- dump one function layout
- dump one program layout

This stage makes the artifact useful to tests and future consumers.

### Stage R3: First Consumer Migration

Find one existing downstream-ish path that currently reads raw allocation
results and migrate it to consume the shaped layout artifact instead.

Good candidates:

- program allocation dump / debug surface
- allocate+rewrite result reporting
- later spill-slot-facing summary code

This stage proves the new layer is not dead weight.

### Stage R4: Authority Split

Once one consumer exists:

- treat shaped layout as the default backend-facing allocation result
- keep raw allocation result for allocator-policy inspection

## Current Progress

Current approximate position inside this line:

- `Stage R1: Read-Only Shaping Artifact`: roughly **100%**
  - public layout structs, init/free helpers, and projection builders are
    landed
- `Stage R2: Queries And Dump Surface`: roughly **100%**
  - placement/function/program lookup helpers and dedicated layout dumps are
    landed
- `Stage R3: First Consumer Migration`: roughly **99%**
  - legacy function allocation dump and allocation-result count helpers now
    consume the shaped layout projection internally
  - program-level layout dump and grouped result summaries now also exist as
    real consumer-facing surfaces above the raw placement table
  - per-function layout now also owns explicit color-group and spill-slot-group
    partitions, so backend-facing consumers can read allocation clustering
    directly rather than rescanning placements
  - program layout now also owns whole-program totals, maximum budget/slot
    summaries, and direct function-index filters for "has colors" / "has
    spills", which is a much closer fit to real backend-entry consumption
  - the shaped result surface now also carries outcome-flavored partitions
    (`optimistic-colored`, `confirmed-spilled`, `recovered-colored`) plus a
    first allocate+rewrite layout report that joins convergence stats with the
    backend-facing layout view
  - program-level shaping now also has multi-function outcome filters, so
    downstream code can directly identify which functions in a program still
    carry optimistic colors / confirmed spills / recovered colors
  - the old program allocation dump path is now also internally closer to the
    shaped authority: it rebuilds its legacy text from program/function layout
    information instead of staying a fully separate raw-result walk
- broader allocate+rewrite/backend-entry consumers are now starting to migrate
- `Stage R4: Authority Split`: roughly **98-99%**
  - direct layout-first allocation entrypoints are now landed for function,
    program, and allocate+rewrite entry paths
  - allocate+rewrite now also has a first structured layout-report artifact,
    so callers can consume one report object instead of manually pairing stats
    with a separate layout result
  - that report artifact now also has its own dump/reporting authority, so it
    is beginning to act like a first-class result surface rather than only a
    transit container
  - function/program layout artifacts are also starting to carry a little more
    of their own identity metadata, which reduces how much artifact-owned
    dumps need to borrow naming context from external program objects
  - the current contract is now explicit that layout-first entrypoints can
    preserve function naming context directly, while bare raw-result projection
    may still yield name-light artifacts until a caller deliberately reattaches
    program context
  - report-first allocate+rewrite paths now follow that stronger line too:
    once a caller enters through the report/layout-first surface, naming
    context can survive all the way into artifact-owned report dumping
  - and the opposite path is now explicit too: callers that first build a
    name-light layout from raw results can later reattach program naming
    context through a public helper instead of relying on private
    implementation-only behavior
  - the structured allocate+rewrite report now follows that same public
    protocol: callers can reattach program naming context at the report level
    rather than having to manually drill into `report.layout`
  - that makes the staged-context story effectively complete for the current
    shaped-artifact family: callers can stay name-light while doing purely
    structural result work, then explicitly opt into richer named reporting at
    either the layout or report layer
  - the allocate+rewrite report now also has a public build path, so callers
    are no longer forced to reach it only through one monolithic entrypoint;
    they can assemble, enrich, and dump that report artifact in explicit
    stages just like the lower layout artifacts
  - in other words, the report artifact now supports the same full staged
    lifecycle as the lower shaped layers: build it from existing results,
    attach context later if needed, then consume/dump the enriched artifact
  - corresponding regressions now distinguish "semantic equivalence ignoring
    names" from "fully enriched named reporting", which makes the authority
    split sharper instead of fuzzier
  - that semantic-equivalence line is now also a more formal public contract
    at the artifact layer itself: function/program/report surfaces can compare
    shaped allocation meaning while deliberately ignoring attached naming
    context, and the current comparison now covers grouped value partitions
    and program-level outcome filters rather than only coarse placement totals
  - the artifact-owned query surface is now also wider than plain grouped
    placement lists: program layouts can expose direct summary queries,
    function-name lookup by index, and name-to-index lookup, while allocate
    + rewrite reports can expose stats, the embedded program layout, and one
    combined report summary query without forcing downstream code to peel
    those facts back out of public struct fields by hand
  - the report layer is now also beginning to act like a true authority-owned
    facade over shaped allocation state rather than a passive `{stats,layout}`
    pair: callers can query function layouts, function names, name-to-index
    lookup, and whole-program outcome-filter slices directly from the report
    itself, which reduces the need for downstream code to reach through
    `report.layout` for routine allocation-report consumption
  - that facade line now also reaches per-function structural consumption
    directly: function-layout summary queries, function-layout lookup by name,
    and per-function placement lookup can all be sourced through
    program-layout/report-level APIs instead of requiring downstream code to
    first pull out one nested function layout and then restage the same local
    queries by hand
  - and the migration is no longer only "new APIs exist": some legacy/high
    level dump consumers now also internally prefer the shaped query surface
    for program summaries and per-function lookup, so the authority split is
    starting to show up in actual consumer behavior rather than only in the
    public header
  - that consumer-migration line now also reaches one narrower legacy dump
    surface below the program-wide layer: function-shaped allocation dumps can
    now be projected through a dedicated artifact-style helper, and the
    legacy program allocation dump reuses that shaped function dump path
    internally instead of formatting every function entirely from ad hoc raw
    scans
  - the regression/testing line is now also starting to honor that same
    authority split more directly: several representative layout/report
    assertions have been rewritten to prefer shaped summary / placement /
    report-query helpers over direct field inspection, so even the locked
    external contract is less coupled to raw struct layout details than it was
    earlier in this stage
  - the dump/report implementation line is now also converging on that same
    authority boundary: function/program/report dump paths now consume a much
    larger share of their own shaped summary/query helpers internally instead
    of leaning as directly on nested struct fields, so the public artifact
    surface is increasingly the real formatting substrate as well as the query
    substrate
  - even the repeated partition/group traversal story is now less coupled to
    raw layout arrays than it was earlier in the stage: representative dump
    and regression paths can enumerate color groups and spill-slot groups
    through explicit query helpers instead of assuming those internal backing
    arrays are the only consumer surface
  - the remaining obvious high-level "just read the fields directly" sites in
    dump/report/count helpers are now largely gone as well: representative
    function/program/report dump paths and small count helpers now route
    through shaped summary/query accessors instead of treating nested layout
    fields as their primary read interface
  - raw `ValueSsaAllocationResult` still remains the detailed allocator-policy
    authority, but layout is no longer only a post-hoc projection layer

## File Plan

Expected implementation files:

- `include/value_ssa_alloc.h`
  - new public structs / APIs
- `src/value_ssa_alloc/value_ssa_alloc_layout.inc`
  - shaping builders / init/free / queries
- `src/value_ssa_alloc/value_ssa_alloc_layout_dump.inc`
  - dump helpers
- `tests/value_ssa/value_ssa_alloc_test.c`
  - focused layout artifact coverage

If the dump stays tiny, it may live in the same file as layout helpers, but the
default preference is to split behavior from dump if the file starts to bloat.

## Immediate Next Step

Implement **Stage R1 + R2 together**:

- define the shaped layout structs
- add builder from `ValueSsaAllocationResult`
- add dump/query helpers
- add focused regression coverage

Then immediately migrate one small existing consumer in **Stage R3**.
