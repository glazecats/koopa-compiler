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
- `Stage M2 shaped machine allocation projection`: **90-95%**
  - function/program machine-view builders and dumps are landed
  - focused machine-view regression coverage is split into its own test target
  - the machine module now also has a first real query/helper surface:
    register-bank summary/direct lookup/name lookup, function-view summary and
    placement lookup, and program-view summary plus by-index/by-name function
    view lookup
  - a report-to-machine-view bridge is now landed too, so higher layers no
    longer need to peel the report apart manually before building a machine
    projection
- `Stage M3 first consumer migration`: **97-99%**
  - no allocator semantics have changed yet, but the first downstream-ish
    consumer path is now real: machine-facing allocate+rewrite report dumping
    can build a program machine-allocation view directly from the structured
    report artifact plus a machine register bank
  - that same line now also has a first proper artifact shape of its own:
    `ValueSsaAllocateRewriteMachineReport` carries machine-facing
    allocate+rewrite stats + program view together, with its own
    init/free/build/query/dump lifecycle instead of forcing every caller to
    re-stage a temporary machine projection ad hoc
  - in other words, `value_ssa_machine` is no longer only a standalone
    projection/dump layer; it now owns one genuine report-facing artifact path
    above the raw projection builders
  - the machine-facing function/program view line is now also materially more
    backend-friendly than a flat placement table: it carries used-register
    lists, per-register value groups, caller-clobbered/callee-preserved value
    buckets, and program-level function filters for machine-colored, spilled,
    caller-clobbered, and callee-preserved usage
  - those same higher-level buckets now also surface directly in the textual
    machine-facing dump/report layer, so later consumers and regression
    triage do not need to choose between "structured API only" and "flat dump
    only"; the machine artifact now exposes register usage, register groups,
    caller-clobbered value buckets, and program-level function filters in both
    query form and dump form
  - there is now also a higher-level direct entrypoint from
    `program + color budget + machine bank` straight to machine-facing
    allocate+rewrite report, so callers no longer need to stage
    `layout-report -> machine-report` by hand when they already want the
    machine-facing artifact as the end product
  - that entry line now also has a flat-bank convenience path, so callers who
    only want "use N generic machine registers" no longer need to manually
    construct a temporary register bank before reaching the machine-facing
    report surface
  - the same direct-entry treatment now also exists one layer lower for raw
    machine views, not only machine reports: callers can directly request
    function/program machine views under either an explicit register bank or a
    flat-bank convenience path, without first staging separate allocation
    layout artifacts by hand
  - and those same direct-entry lines now also have matching direct dump
    entrypoints, so higher-level callers can choose between
    `artifact -> query/dump`, `direct build`, and `direct dump` without
    changing machine-facing semantics or re-staging intermediate allocator
    artifacts by hand
  - at this point the machine-facing line is very close to checkpoint-closed:
    function/program machine views and machine reports can all be reached
    through staged, direct, and flat-bank convenience paths, and those paths
    are regression-locked against one another
- `Stage M4 feed later machine constraints`: **99% / stage-close checkpoint**
  - the first real machine-aware consumer is now landed instead of remaining
    only a plan bullet: machine call-clobber risk reporting crosses
    machine-register placement with existing allocation-prep call-survival
    facts (`call_crossing_counts`, plus call-density signals)
  - this is intentionally still conservative and observational rather than
    allocator-driving, but it is the first point where machine facts are
    consumed together with SSA allocation facts to answer a genuinely
    downstream-flavored question: "which values currently sit in
    caller-clobbered registers and also live across calls?"
  - that same line now also exists at program/report scale rather than only as
    a single-function observation: program-level machine call-clobber risk
    reports can summarize total risky values, filter functions with risky
    placements, and expose per-function risk subreports under one machine-aware
    artifact family
  - that consumer is now also more structured than a flat "risky item" list:
    machine call-clobber risk items carry a small risk class
    (`crossing/repeated/hot`), function/program summaries count those classes,
    and program-level reports can filter functions by risk class directly
  - in other words, M4 is no longer only answering "is this value risky at
    all?"; it has started to expose a machine-aware severity surface that a
    later allocator/back-end policy could consume without reparsing free-form
    dumps
  - there is now also a first machine-aware "protection agenda" consumer
    above that severity surface: for caller-clobbered values that actually
    survive across calls, the machine layer can now build a per-function and
    per-program priority agenda that ranks which values are most worth
    protecting according to call-crossing, call-density, loop/use hotness,
    cross-block shape, and rematerializability
  - that means M4 has crossed one more boundary from "observational report"
    into "decision-shaped consumer": later allocator/back-end work can now ask
    not only "what is risky?" but also "which risky values should I care about
    first?"
  - on top of that value-level agenda, the machine layer now also has a first
    register-level pressure aggregation: it can summarize which machine
    registers are currently carrying the most protection burden, how much
    priority mass lands on each register, and which values contribute to that
    burden
  - so the current M4 line now spans three levels of machine-aware consumer
    shape: raw risky items, prioritized protection agenda, and register-level
    protection-pressure aggregation
  - there is now also a first "hotspot" consumer above register-pressure:
    later consumers can directly ask which register is the dominant protection
    hotspot in one function, or summarize those hotspots program-wide, without
    first re-scanning every pressured register
  - above that hotspot layer, there is now also a first conservative
    preservation-hint surface: when a function's dominant hotspot sits on a
    caller-clobbered register and the bank has allocatable callee-preserved
    alternatives, the machine layer can now surface a structured "consider
    moving from X to Y" hint instead of leaving later consumers to derive that
    implication ad hoc
  - that preservation-hint line is now also more like a real planning surface
    than a single hardcoded recommendation: one source hotspot can expose a
    ranked-looking candidate list of callee-preserved alternatives, while
    still keeping the current "best" suggestion as the headline field
  - that candidate-planning line now also carries explicit target-side
    tradeoff facts and an explicit reason surface; target occupancy/pressure
    affect ordering, reasons are public bucketed facts rather than only dump
    prose, and both preservation-hint reports and the top-level planning
    report can summarize/filter by those reasons
  - the machine layer also now has a top-level planning artifact above the
    individual pressure/hotspot/hint surfaces. Program planning reports can
    summarize and filter all three at once, and that top layer's query/filter
    surface is now materially symmetric enough to be treated as a stage-close
    checkpoint rather than a sketch
  - the implementation boundary has also been cleaned up materially: the old
    oversized machine call-clobber implementation file is now split so
    call-clobber risk stays separate from the newer protection/pressure/
    hotspot consumer stack

## Next Mainline

Treat the current machine-register-model line as a stage-close checkpoint.

The next more natural mainline is no longer "one more small machine report",
but rather one of:

1. machine register constraints that actually feed allocation policy
   - precolored nodes
   - register classes entering allocator decisions
   - call-clobber-aware allocation constraints

2. instruction selection / target lowering
   - now that machine-facing placement/planning vocabulary exists

For the current repository shape, the cleaner immediate continuation is still:

- keep machine-facing planning maintenance-first unless a concrete gap appears
- switch active implementation attention toward the next larger backend-facing
  mainline rather than continuing to widen report-only surfaces here

## Immediate Next Step

Implement **Stage M1 + the smallest useful part of M2**:

- define one flat machine register bank
- add register-bank dump/query helpers
- add one function/program machine allocation projection from shaped layout
- lock it with focused regression coverage
