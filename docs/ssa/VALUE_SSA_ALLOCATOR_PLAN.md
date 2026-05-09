# Value SSA Allocator Roadmap

## Goal

This document captures the allocator roadmap after the first real
`value_ssa_alloc` landing.

Current reality is no longer "allocator-prep only", but it is also not yet a
full textbook graph-coloring backend.

The allocator now already has:

- a public allocator module boundary
- deterministic coloring under a color budget `K`
- interference- and affinity-driven planning
- a remove/select-shaped plan with explicit `freeze` and `spill-remove`
- spill intent vs. confirmed spill result tracking
- spill slot assignment
- real spill rewrite with canonicalization + fixed-point driver
- meaningful CFG-aware reload/store placement over a growing set of shapes

So the right question is no longer "should there be an allocator?"

The right question is:

- how far to push spill rewrite as a short-term delivery track
- when to switch from "make current spills land on more CFGs" to
  "avoid more spills in the first place"
- what prerequisites are needed before target / machine constraints enter

This document is the working roadmap for that transition.

## Current Status

The current allocator should be understood as:

- already a real downstream consumer
- already capable of end-to-end allocate + rewrite on representative SSA CFGs
- still pre-machine and still lighter than full Briggs/Chaitin

### Already Landed

The following surfaces are already in place:

- `include/value_ssa_alloc.h`
- `src/value_ssa_alloc/`
- `tests/value_ssa/value_ssa_alloc_test.c`

And the following behavioral slices are already real:

- deterministic coloring under budget `K`
- affinity-biased color choice
- a first conservative explicit coalescing analysis with class-aware select
- a first coalescing-aware plan/worklist pressure model above raw affinity bits
- explicit allocation plan over `simplify` / `freeze` / `spill`
- reverse-select coloring
- spill-intended vs. spill-confirmed result state
- spill slot sharing across non-interfering spilled values
- same-block spill rewrite
- cross-block spill rewrite
- phi-input spill rewrite
- branch-edge split blocks for phi-input reload placement
- split-block reuse across repeated phi-edge spill families
- same-block reload reuse
- unique-predecessor reload reuse
- unique-predecessor-chain reload reuse
- dominating-reload reuse through reconverged CFG when no spill-slot clobber
  intervenes
- fixed-point allocate + rewrite driver with canonicalization

### Current Structural Strength

Today the allocator is strong enough that it should not be described as
"allocator-prep" anymore.

It already has:

- a real remove/select discipline
- a real spill rewrite path
- enough CFG sensitivity that reload placement is no longer straight-line only
- enough observability that policy changes are inspectable

### Current Structural Weakness

What it does **not** yet have is equally clear:

- no full move-worklist coalescing mainline
- no George/Briggs merge-safety decision layer
- no full move-worklist machinery
- no full optimistic-coloring retry discipline
- no rich spill-cost model
- no live-range splitting
- no machine/precolored/register-class constraints
- no call-clobber-aware register model
- no parallel-copy resolution surface

That means current work should still be framed as:

- "SSA allocator with real spill landing"

not yet:

- "full machine register allocator"

## Non-Goals Right Now

The next few slices should **not** jump directly to:

- target instruction constraints
- callee/caller-saved policy
- final frame layout
- stack-slot home assignment beyond current spill locals
- register classes
- fixed physical registers
- full machine lowering

Those belong after allocator mainline policy is stronger.

## What Is Missing

This is the current missing-core list in priority order.

### 1. Explicit Coalescing

Current allocator now has a first conservative coalescing layer, and select can
prefer colors from coalesced classes.

It also now has one first coalescing-aware plan step:

- the remove/freeze/simplify plan can see coalesced representative families
- plan items record both raw pressure and coalesced "effective" pressure
- family-internal accepted move edges no longer force extra `freeze` churn
- `was_frozen` is now inspectable, so later mainline work can tell whether a
  value really needed freeze or only looked move-related before coalescing
- class-level external move pressure is now propagated across the whole current
  family instead of only the one member that owns the immediate affinity edge
- the current `freeze` step now freezes a whole coalesced family together,
  which is a better fit for the current "families are starting to act like one
  allocator object" direction
- select may now also defend a preferred class/affinity color under simple
  pressure: when there is no free color and the preferred color is blocked by
  exactly one lower-priority value, the allocator may evict that specific
  blocker instead of giving up on the preferred color immediately
- a narrow artifact-driven allocation entrypoint now exists for experiments and
  regression locking around these policy choices, so later mainline work does
  not need to force every targeted scenario through full program-shape liveness
- select decisions are now also first-class observable artifacts: allocation
  results remember preferred-color source/partner and targeted-eviction details,
  which gives later move-worklist/coalescing work a stable policy-inspection
  surface instead of forcing every experiment back through ad hoc trace logic
- a first family-level move-state analysis now exists above coalescing + plan:
  one item per coalesced family records external move pressure, external
  affinity weight, removal counts, and whether the family ended up `internal`,
  `released`, or `frozen`
- a first explicit move/coalesce worklist surface now exists above that family
  analysis, with stable family buckets (`coalesce-ready`, `freeze-pending`,
  `released`, `internal`) plus deterministic priority and query/dump helpers
- that move/coalesce worklist is now also partially feeding back into the main
  planner: plan items record family move-work class/priority plus current
  family move pressure, and the current `freeze` tie-break now uses that
  priority to freeze lighter-pressure families before heavier ones
- plan items now also expose one explicit family-work transition snapshot:
  `move_work_class -> move_work_next_class` plus family pressure before/after
  the removal step, which gives the current allocator its first real
  state-machine-shaped artifact above plain static worklist classification
- there is now also a public move-transition trace surface above the plan,
  which makes those family-work transitions queryable and dumpable as one
  explicit event sequence (`freeze` plus `remove`) rather than only as fields
  embedded in plan items, and that trace is now produced by a small dedicated
  family-state simulation over current allocation facts/worklist policy instead
  of a plain field copy
- that trace/runtime line now also has a first explicit coalesce-action event
  instead of stopping at pair-agenda ordering. The move engine can now consume
  one live mutual-best pair as a real `coalesce` transition, record it in the
  transition trace with both family endpoints, and then locally refresh
  move-related / safe-ready family state before later `remove` events continue.
  This is still conservative family-state consumption rather than true merged
  node allocation, but it is the first place where a surfaced pair is actually
  acted on instead of only ranking neighbors.
- that coalesce-action layer now also has its first formal alias artifact above
  trace-only state. `ValueSsaAllocatorPlan` now carries final runtime merged
  family roots/class sizes, later plan items are previewed against that runtime
  alias map, and artifact-driven select can now consume the final merged-family
  surface instead of being limited to the older static coalesce roots. This is
  still not a full merged-node worklist allocator, but it is the first point
  where dynamic coalesce actions feed a downstream allocation consumer.

It does **not** yet:

- merge move-related nodes through the main worklist model
- maintain alias/coalesced representatives
- apply George/Briggs safety
- remove redundant move families structurally

This is the biggest missing "allocator proper" feature.

### 2. Formal Move Worklists

Current `freeze` is real, but still simplified.

Missing pieces include:

- active moves
- worklist moves
- frozen moves
- direct move-family transitions tied to coalescing decisions

### 3. Full Optimistic Coloring Retry

Current allocator already distinguishes:

- spill-intended
- spill-confirmed

But it still lacks a full policy layer for:

- optimistic spill-remove scheduling
- retrying candidates after neighborhood collapse
- separating "removed as spill risk" from "must truly spill"

### 4. Rich Spill-Cost Model

Current spill cost is intentionally light:

- use count
- live block count
- affinity sum

Still missing:

- loop depth / loop weight
- call-density or call-pressure effects
- rematerialization friendliness
- split opportunity awareness
- local-vs-global live-range asymmetry

### 5. Stronger Spill Rewrite Coverage

This is the current short-term delivery line.

Still missing or still intentionally narrow:

- more general edge-specific placement families
- more loop-carried spill families
- deeper multi-join reload placement
- more complicated split/split-reuse compositions
- more cases that future live-range splitting will depend on

### 6. Live-Range Splitting

Current spills are still essentially whole-value spills.

There is not yet a framework for:

- splitting one live range around hot pressure points
- inserting local copies/reloads/stores only where needed
- re-running allocation over split subranges

### 7. Machine Register Model

Current colors are abstract.

Missing machine-facing constraints include:

- precolored nodes
- register classes
- fixed-operand constraints
- call-clobber sets
- physical register availability rules
- parallel copy resolution

## Roadmap

The roadmap has three layers:

1. short-term spill-rewrite completion
2. allocator-mainline strengthening
3. machine-facing backend transition

## Current Execution Order

If we order the remaining work from "today's allocator" toward "machine code",
the intended sequence is:

1. allocator-mainline strengthening
2. live-range splitting
3. allocator-result shaping for later backend consumers
4. machine register model
5. frame / stack-slot policy consolidation
6. instruction selection / target lowering
7. block layout / branch lowering
8. encoding / emission

That should be read in one important way:

- before machine code, the first real milestone is still "make the allocator
  formally stronger"
- machine-facing work belongs after allocator mainline is much more explicit
- target lowering / emission belong after machine register constraints exist

## Current Active Step

The allocator is currently in:

- **machine register model**

### Progress Snapshot

Current approximate position inside that larger stage:

- `explicit coalescing mainline`: roughly **94-96%**
  - near-closed for now
  - no longer the primary active line unless retry / Phase C work exposes a
    concrete gap in the merged-family/coalesce-action path
- `B2 more formal optimistic coloring retry`: roughly **99%**
  - effectively at a near-closed checkpoint
  - retry now already has explicit family field / phase protocol, step-result
    status, unified phase-entry artifacts, and a field-driven outer loop shape
- `Phase C / Briggs-Chaitin-style mainline consolidation`: roughly **96-99%**
  - this is now best read as a **reclosed near-closed stage**
  - recent work pulled allocator orchestration into nested result-bearing
    drivers across move engine, select side, execute side, ordinary-entry
    artifact build, program-level entry, and allocate+rewrite fixed-point
  - the move-engine phase-entry/state surface is now also one notch more
    representative-owned on the write side: rebuilding/collecting phase-entry
    state now resolves representative roots up front instead of leaning as
    heavily on later root-indexed normalization
  - the latest reopened allocator-maintenance tail has now been reclosed in the
    current tree. Focused witnesses around `family-freeze`,
    `released-simplify-family`, `coalesce-opportunity`, explicit
    move-transition coalesce/freeze traces, spill-family tie-breaking,
    targeted/weighted recolor, and the reopened retry-family path are green
    again, and the aggregate allocator suite
    `./build/value_ssa/value_ssa_alloc_test` is green too
  - the key implementation lesson from that tail is now recorded in the tree:
    hand-built allocator artifacts must keep graceful fallback lookup paths
    even when their derived index tables are absent. Current authority is that
    both manual coalesce-pair lookup and manual move-work root lookup now have
    that fallback, and the no-move allocator-plan fast path also preserves
    coalesced effective-degree/class facts instead of flattening them away
- `live-range splitting`: roughly **99%**
  - this is now best read as a **checkpointed near-closed stage**
  - the current landed slice now covers conservative block-local spill splits
    before whole-value materialization, including same-block instruction
    clusters, mixed instruction/terminator local use clusters, and multiple
    eligible use blocks for one spilled value in the same rewrite pass
  - the current step-1 substage now also covers defining-block local clusters
    for instruction-defined spilled values, while still intentionally leaving
    phi-defined values and broader structural split families for later slices
  - split rewriting now also has a first explicit change-reporting protocol
    into the allocate+rewrite loop, so split-only rewrites are part of the
    main convergence decision instead of being an unsurfaced pre-pass mutation
  - the current rewrite-stage boundary is now also explicit about one real
    semantic difference between lanes: split-only rewrites are currently kept
    out of automatic post-rewrite canonicalization, because eager cleanup can
    erase the split child itself before the next allocation round has a chance
    to consume the shorter live-range shape
  - split children now also have a first real downstream policy effect in the
    spill-rewrite layer: a split child that is only a local `mov` view of an
    already-spilled parent value no longer needs to grow a second independent
    whole-value spill family during later rewrite
  - that same split-family signal now also reaches the planner surface in one
  first narrow place: spill-phase tie situations can prefer sacrificing a
  split child before a non-child peer, so the parent value is preserved a
  little more deliberately instead of leaving child-vs-parent choice to ids
  - the current split-family line now spans three connected layers:
    block-local split formation, downstream rewrite-family reuse, and a first
    planner-side spill tie-break signal
  - split-child edge reuse now also has one extra honesty boundary: an
    ordinary branch arm that merely ends in `mov ...; jmp successor` is no
    longer treated as a reusable split child unless it is actually the unique
    predecessor split block for that branch path
  - split-family structure now also begins to affect later select/retry
  decisions: equal-pressure blocker choice and equal-pressure retry recovery
  choice can both prefer split children in one first narrow slice
  - retry-family surfacing now also knows about that structure: retry agenda
  entries can explicitly report whether their representative is a split child
  - in other words, the split-family line now spans formation, rewrite reuse,
  planner spill preference, select/retry choice, and surfaced retry-family
  summaries rather than stopping inside one subsystem
  - result layout now also participates: spill-slot assignment can actively
  collapse a split child onto its parent's spill slot instead of leaving that
  family relationship implicit until later rewrite
  - the current split-family line is no longer limited to one-hop child/parent
  facts: allocation prep now carries a first `split family root` notion, and
  later slot/layout logic can consume that root directly
  - rewrite-family reuse has now also started to consume that root-style idea
  more directly: chained `mov` split descendants can reuse one root spill
  family instead of only the immediate parent's family
  - split-family policy is now also starting to distinguish depth, not only
    membership: deeper descendants can be preferred as the cheaper sacrifice
    when split-family candidates otherwise tie
  - the current block-local split lane is now also locked on one phi-defined
    loop-header family that had exposed a real bug: after inserting one local
    split child in the header, rewrite must ignore same-block phi uses that do
    not belong to the local instruction/terminator rewrite lane instead of
    misclassifying them as invalid rewritten local uses
  - split-only rewrite now also allows one spilled value to compose
    block-local splitting and edge-phi splitting in the same round instead of
    forcing an artificial either/or between those two lanes; current
    regression authority now locks a branch predecessor where one spilled
    value gets one local child for repeated tail uses and a separate edge child
    for downstream phi consumers on the same CFG branch
  - that same structural split line now also lets one spilled value continue
    across multiple eligible edge families in the same rewrite round instead
    of stopping after the first rewritten edge; current regression authority
    now locks one two-branch/two-join family where the same spilled value gets
    one split child on each eligible phi edge in one pass
  - that structural split line now also composes with unique-predecessor jump
    edges more deliberately: one spilled value may get a local child for
    repeated uses in the jump predecessor block and still receive a separate
    edge child for downstream phi consumers on that same jump edge in the same
    round
  - and in the narrower single-phi jump-edge case, the current line is now
    stronger than "always open one more edge child": if the jump predecessor
    already formed one local split child for repeated local uses, that same
    child may now satisfy the lone downstream phi consumer directly without
    opening a separate split-edge block at all
  - current branch-side boundary remains stricter than that jump-side endpoint:
    branch split blocks are still kept as the default carrier for branch-edge
    phi consumers, while the stronger "reuse the predecessor local child
    directly" rule is only locked today for the unique-predecessor jump-edge
    case
- `allocator-result shaping for later backend consumers`: roughly **99%**
  - this is now best read as a **near-closed checkpoint**
  - Stage `R1` read-only shaping artifact is landed:
    `ValueSsaAllocatedValuePlacement`,
    `ValueSsaFunctionAllocationLayout`, and
    `ValueSsaProgramAllocationLayout` now exist as public additive result
    surfaces above `ValueSsaAllocationResult`
  - Stage `R2` queries and dump surface is also landed in its first complete
    slice: builders, init/free helpers, per-value placement lookup,
    per-program/per-function layout lookup, and dedicated layout dump helpers
    are all real and regression-locked
  - Stage `R3` is now underway in a more structural way: allocation result
    counting, legacy function allocation dump, and the program-layout dump
    surface all now route through the shaped layout line instead of depending
    purely on raw allocator arrays
  - the function-layout artifact is also now stronger for later backend
    consumers than "one placement table": it carries grouped colored/spilled
    value lists plus a used-color summary, so downstream code can consume
    allocation partitions directly instead of rescanning the placement table
  - that grouped summary line now also includes explicit `color-group` and
    `spill-slot-group` ownership, so later backend code can ask "which SSA
    values currently occupy abstract color C?" or "which spilled values share
    slot S?" without rebuilding those partitions by hand
  - the program-level artifact is now also more than a plain function-layout
    array wrapper: it carries whole-program totals, max budget/slot summaries,
    and direct "which functions currently have colors/spills" filters, so
    downstream entrypoints can triage program allocation state without
    rescanning every function layout manually
  - result shaping now also starts to expose a more semantic outcome layer
    above plain placement: optimistic-colored, confirmed-spilled, and
    recovered-colored partitions are all surfaced in function/program summaries
    and dumps, and there is now a first explicit allocate+rewrite layout
    report that joins convergence stats with the shaped allocation view
  - whole-program result shaping now also has explicit multi-function outcome
    filters, not only plain totals: downstream entrypoints can directly ask
    which functions currently contain optimistic colors, confirmed spills, or
    recovered colors, which is very close to the kind of triage a later
    backend pipeline/reporting stage will want
  - even the legacy program allocation dump path is now beginning to route
    through program-layout authority internally rather than independently
    rescanning raw function results one function at a time, so the old textual
    contract and the new backend-facing authority are drifting less
  - Stage `R4` authority split has now started in one first narrow slice:
    downstream callers can allocate directly into
    `ValueSsaFunctionAllocationLayout` /
    `ValueSsaProgramAllocationLayout` through layout-first entrypoints instead
    of being forced to allocate raw results first and project later
  - that same `R4` line now also has a first structured report surface above
    those entrypoints: allocate+rewrite can return one explicit
    `ValueSsaAllocateRewriteLayoutReport` artifact instead of forcing callers
    to manage layout + stats as separate parallel outputs
  - report ownership itself is now starting to move over too: the
    allocate+rewrite layout-report dump can consume the structured report
    artifact directly instead of only rebuilding from loose raw arguments
  - the shaped artifacts are also beginning to own a little more of their own
    identity metadata, so layout/report dumping no longer has to lean quite as
    heavily on an external `ValueSsaProgram *` or `ValueSsaFunction *` just to
    recover basic naming context
  - that naming/identity line is now explicit in the contract too: layout
    artifacts produced directly by layout-first allocation entrypoints can
    carry recovered function names, while raw-result-to-layout projection is
    still allowed to remain name-light (`<anon>`) unless a caller chooses to
    reattach source program context
  - that same distinction now also reaches structured reporting: report-first
    allocate+rewrite entrypoints and report dumps preserve naming context
    through the report/layout artifact, while bare projection-only paths still
    keep the lighter "names optional" contract
  - the "reattach context later" path is now also explicit public authority
    rather than only an internal trick: callers can project raw results into a
    name-light layout and then deliberately attach `ValueSsaProgram` naming
    context afterward through a stable helper when they need richer reporting
  - that same staged-context protocol now also exists one layer higher for the
    structured allocate+rewrite report artifact itself, so callers no longer
    need to crack open the report and manually reach into its layout just to
    reattach naming context
  - in other words, the staged context protocol is now complete across both
    main shaped artifacts: function/program layout and allocate+rewrite report
    can each exist in a lighter context-free form first and then be promoted
    into richer named reporting artifacts through explicit public helpers
  - the report artifact now also has a first public build protocol of its own,
    not only entrypoint-specific production: callers can assemble a structured
    allocate+rewrite report from existing raw program allocation results +
    rewrite stats, then optionally attach naming context later
  - so the full staged artifact protocol is now present for allocate+rewrite
    too: `build -> attach context -> dump/report` is a first-class supported
    path, not only `run one monolithic entrypoint and immediately print`
  - this also clarifies one subtle but important contract line: semantic
    equivalence of shaped artifacts is now intentionally separable from naming
    completeness, so tests and downstream callers can choose whether they care
    about allocation semantics only or about full reporting context too
  - raw `ValueSsaAllocationResult` is still retained as the detailed
    allocator-policy/debug surface, but the new layout layer now has its first
    real claim as a default backend-facing result path rather than only a
    helper projection
- `machine register model`: roughly **25-30%**
  - this is now the **current active mainline**
  - the machine-facing line has been promoted out of `src/value_ssa_alloc/`
    into a sibling module:
    `include/value_ssa_machine.h` plus `src/value_ssa_machine/`
  - current implementation covers a real `Stage M1` and most of the smallest
    useful `Stage M2`: flat register-bank construction, function/program
    machine-allocation projection, machine dumps, and a dedicated
    `test-value-ssa-machine` target are all landed and green
  - shared data structs still temporarily live in `include/value_ssa_alloc.h`,
    but ownership of the implementation/API boundary now sits with the sibling
    `value_ssa_machine` module rather than allocator internals
  - the next natural step is not more file movement; it is adding a small
    query/helper surface and then migrating one downstream consumer to prefer
    machine-facing views over raw abstract-color interpretation

#### Near-Term Splitting Plan

Current intended sub-order inside live-range splitting:

1. extend block-local splitting from non-def blocks into def-block local
   clusters when the defining instruction is followed by multiple local uses
2. keep block-local splitting conservative around phi-defined values and other
   non-instruction definitions
3. let split children participate more deliberately in the later
   allocate+rewrite loop instead of behaving only as a pre-rewrite local
   convenience
4. only then widen to more structural split families such as edge-shaped or
   loop-pressure-driven subranges

The immediate next implementation target is therefore:

- **def-block local spill splitting**
  - current status: roughly **complete for the first conservative slice**

- **split-to-rewrite-loop protocol**
  - current status: roughly **meaningful early slice landed**
  - split helpers now report whether they changed the program, and the main
    allocate+rewrite loop consumes that signal as a real rewrite/convergence
    event
  - current safety note: split-only rounds intentionally skip automatic
    canonicalization for now, because the split child must survive long enough
    for the next allocation round to see it as a real live-range boundary

- **split child downstream policy participation**
  - current status: roughly **late sixth slice / near-closed for the current root-aware family-summary line**
  - rewriteable-spill gating and direct spill rewrite now both recognize one
    conservative parent/child family rule: when a spilled value is just a
    split-child `mov` of another spilled value, later rewrite reuses the
    parent spill family instead of materializing a second one
  - the planner now also records a first explicit `split_child` fact and uses
    it in spill-phase tie-breaks, which is the first point where split-family
    structure affects spill choice before rewrite begins
  - main select and retry recovery now also consume that same `split_child`
    fact in one first narrow place, so the signal has crossed from planning
    into later allocation choice rather than stopping at spill-remove order
  - retry-family agenda surfacing now also exposes a representative-level
    `split_child` fact, so later policy/debug consumers no longer have to infer
    that family shape indirectly from low-level defs
  - spill-slot assignment now also honors a conservative parent/child family
  override, so the allocation result itself can preserve that family
  relationship without waiting for later rewrite reuse to rediscover it
  - allocation prep now also computes a first explicit split-family root, so
  that downstream consumers no longer have to reason only in one-hop parent
  links when preserving split-family structure
  - rewriteable-spill gating now also follows a small `mov` chain toward a
  spilled ancestor, so chained split descendants can stay on one root spill
  family instead of reopening second/third spill families at each hop
  - allocation prep now also tracks first split-child depth, and current
    planner/select/retry tie-breaks can use that depth when choosing between
    otherwise-equal split descendants
  - retry-family agenda and active phase-entry materialization now also carry
    a first formal split-family summary (`split root`, `split member count`,
    `split intended count`) instead of surfacing only whether the chosen
    representative itself is a split child
  - that means the retry/recovery side now exposes one actual split-family
    object summary for the current recovery entry, not merely a representative
    decoration layered over scalar family recovery
  - retry-family collection itself is now also more representative-owned than
    before: one coalesced family first collects recoverable candidates by
    split-family root, picks one representative per split-family subgroup, and
    only then chooses the family-level recovery representative across those
    subgroup representatives
  - in other words, the current split-family summary is no longer a
    first-seen/scan-order byproduct inside retry-family collection; it now
    follows the representative that actually wins the family-level retry race
  - split-only rewrite is now also beginning to widen beyond block-local
    clusters into a first structural edge-shaped family: when one spilled value
    feeds multiple phi inputs along the same CFG edge, the current allocator
    can materialize one shared split child on that edge and rewrite those phi
    inputs through the child instead of keeping the family purely block-local
  - that first structural slice currently covers both a branch edge and a loop
    backedge shape, so live-range splitting is no longer only "local use
    cluster carving" even though broader edge/loop-pressure-driven subranges
    remain future work
  - the branch-edge slice now also has a clearer edge-local subrange boundary:
    predecessor-tail uses stay local to the predecessor block, while the
    shared split child is reserved for downstream consumers on the split edge
    itself, so the edge-shaped family is no longer just "shared phi glue" but
    a first real edge-local live-range fragment with a stable boundary
  - that same edge-shaped line now also begins to treat the split edge as a
    reusable container rather than a one-value-only artifact: once one value
    has opened a valid split-child edge block, later eligible values on that
    same edge can add their own split children into the existing block instead
    of forcing a second edge container
  - that reusable-container slice now covers both a branch edge and a loop
    backedge family, so the current structural split line is starting to look
    less like a branch-only trick and more like a reusable edge-family
    capability
  - on the branch side, that reusable-container rule is now also regression-
    locked in the stronger "two values, two predecessor tail uses" shape, so
    we have a better guardrail that edge-local children can accumulate in one
    container without accidentally absorbing unrelated predecessor-local tail
    work

Checkpoint for that slice:

- a spilled instruction-defined value can get one local split child inside its
  own defining block when a later local use cluster exists there
- the same helper still works for non-def blocks and mixed instruction /
  terminator clusters
- focused regressions lock both def-block and non-def-block behavior together
- overall `allocator-mainline strengthening`: roughly **99%**

Practical reading:

- if a future turn asks "what are we doing right now?", the default answer
  should be **live-range splitting**
- if it asks "what was the last major line before that?", the answer should be
  **allocator-mainline strengthening / Phase C consolidation, now near-closed**
- if it asks "what larger consolidation line already started but is not the
  current focus?", the answer should be **explicit coalescing mainline,
  near-closed**

Within that larger step, the current sub-order is:

1. live-range splitting
2. allocator-result shaping for later backend consumers
3. machine register model
4. only then selective spill-rewrite follow-up for gaps that the new split
   machinery exposes

So if a future turn asks "what are we doing right now?", the answer should be:

- we are still in the allocator-mainline strengthening stage
- more specifically, we are currently pushing the broader Phase C
  consolidation line
- the previous major line, optimistic retry, is now near-closed rather than
  the default active target
- today's recent work is on moving from pair-agenda ordering into the first
  explicit coalesce-action layer inside the shared move engine, plus the first
  runtime merged-family alias artifact that downstream select can consume
- the more current consolidation-side continuation of that same structural
  style is to keep pulling allocator orchestration toward nested explicit
  drivers/results, so move-engine, select, retry, and execute surfaces read
  less like adjacent helpers and more like one staged machine
- the current sub-focus inside that step is to keep pulling queue/phase-entry
  behavior toward representative-owned runtime family state, so `best_partner`,
  external pressure, move-work class, and queue-side after-state are less often
  read as raw root-indexed arrays and more often read as "state of the
  surviving merged representative"
- the newest extension of that same direction is on the select/retry side too:
  runtime merged-family alias is now starting to shape family color choice as a
  family-level state question ("what color has this representative family
  already converged on?") rather than only as repeated scans of individual
  sibling values
- the next piece of that same line is now also real in blocker/retry choice:
  the allocator has started to compare current runtime merged-family activity
  when deciding which blocker family to sacrifice or which spilled family to
  recover, so "family state during select" is beginning to matter alongside the
  older static family-pressure snapshot from plan time
- and the generic blocker path itself is now one notch more family-object-like:
  one merged family first surfaces one blocker representative, and only those
  family representatives compete globally, instead of letting every sibling
  blocker participate as an equal first-class candidate
- the same structural move has now started on retry entry too:
  one merged family first surfaces one spilled recovery representative, and
  only those family representatives compete globally, instead of treating every
  spilled sibling as an independent top-level retry candidate
- that retry-family representative is now also the actual top-level entry unit
  of the recovery race, not just a downstream comparison idea layered over a
  scalar scan
- the comparison lanes are also starting to care about family color cohesion:
  not just "how active is this family?" but "how many currently colored members
  of this family already hold the candidate color?", which is a small step
  toward treating merged families as real coloring objects rather than only
  grouped scalar values
- that cohesion idea now also influences local recolor choice:
  if a blocker can legally move to several colors, recolor now starts by asking
  which target color best preserves the merged family's current color support,
  instead of treating all legal spare colors as equivalent
- and that recolor rule now applies on the preferred-color targeted branch too,
  not only the generic recolor path
- and that targeted recolor behavior is now explicitly regression-locked on a
  direct `coalesce-direct` fixture, not only assumed from generic recolor tests
- recolor is now also slightly less "support-count only" inside one merged
  family: when two legal recolor targets are equally occupied by family
  siblings, the allocator may now keep following the stronger direct
  coalescing partner rather than collapsing immediately to lower color order
- that recolor-target information now also starts to matter one step earlier in
  generic victim choice itself: when two uniquely-blocking low-priority
  blocker families are both recolorable, the allocator may now prefer evicting
  the blocker whose legal recolor destination is better supported by its own
  merged family, instead of falling back immediately to blocked-color order
- that same recolor-target signal is now also beginning to reach retry-stage
  recovery choice: once two recovery candidates both need an eviction and both
  blocker sides are recolorable, retry no longer has to treat those blocker
  recolor destinations as equivalent before falling through to older scalar
  blocker/value tie-breaks
- the allocator result surface is now also starting to remember blocker
  aftermath explicitly, not only that an eviction happened: targeted/retry
  eviction metadata can now tell whether the blocker was recolored away and, if
  so, which color it moved to
- that blocker-aftermath surface now also extends to main generic eviction, not
  only targeted or retry-side eviction paths, so post-select result queries can
  describe one uniform "which blocker did we move, and did it recolor?" story
  across the allocator's major bounded-eviction cases
- that in turn means later retry-order regressions no longer need to reverse
  engineer blocker fate from the final color map alone; they can ask the result
  surface directly whether the deciding blocker was recolored or sacrificed
- the newest structural extension is that main select and late retry now also
  share one explicit internal `select query` surface instead of each lane
  assembling blocked colors, blocker counts, and preferred-option state ad hoc.
  Main select reuses one query buffer across the reverse-select walk, retry
  reuses the same protocol while scanning spilled-family candidates, and both
  lanes now form their option from the same query object before later claim
  materialization/application happens. This is still internal structure rather
  than a public allocator API, but it is a real step from "shared helper
  fragments" toward one merged-family select protocol.
- the move-engine side now also has one more explicit representative-owned
  runtime surface instead of letting later queue/phase-entry consumers keep
  reading the family state only through a wide set of root-indexed parallel
  arrays. A dedicated internal `family_runtime_facts` array now mirrors the
  current merged-family member/pressure/partner/queue/work-class state, phase
  entry rebuilds and plan previews consume that unified family surface, and
  obsolete loser-root cleanup now clears that representative-owned runtime
  state together with the queue identity. This is still not a full replacement
  for every lower helper array, but it means the allocator's later
  representative-owned consumers now read one family-state object surface
  rather than stitching the same facts back together repeatedly.
- that same runtime-family surface is now also starting to be maintained from
  the write side instead of being only a later mirror. Family member counts,
  spill-removed counts, move-work class, queue priority, and the main
  move-pressure / best-partner facts now update `family_runtime_facts` as the
  move engine recomputes them, rather than waiting for every later consumer to
  restitch or resync the same fields after the fact. This is still not the end
  of the root-indexed arrays below, but it is a real shift toward treating the
  representative-owned family state as a live runtime object rather than a
  derived view.
- that writer-side line is now also starting to consolidate into explicit
  family-state setters instead of ad hoc parallel assignments. The move engine
  now has one small internal setter surface for representative-owned family
  counts / queue priority / move-work class / move-pressure partner facts, and
  some "is this merged family still active?" checks are beginning to read the
  representative-owned state directly. So the direction is no longer only
  "keep one more mirror updated"; it is becoming "use one live family-state
  surface to drive later representative-owned behavior."
- those family-state setters now also cover more of the move-engine boundary
  transitions themselves instead of only mid-pipeline recompute points. Reset,
  merge, queue-reset, and partner-mutual reset paths are starting to flow
  through the same representative-owned family-state update surface too, which
  makes the merged-family runtime object feel less like an optional overlay and
  more like the state protocol that later queue/phase-entry logic is expected
  to obey.
- the same consolidation is now also starting to reach the read side for a few
  internal decisions, not only the writes. Some merged-family checks now ask
  for active-member / spill-removed / coalesce-ready-partner facts through
  small family-state getters instead of reading the raw root-indexed arrays
  directly. That is still a narrow slice, but it means the runtime object is
  beginning to act as an internal fact surface in both directions, not only a
  synchronized write target.
- the next step of that same line is now also real in a few internal guard
  rails: some move-engine helpers no longer treat the raw family arrays as the
  only admissible backing store, and instead accept the representative-owned
  family runtime surface as a valid source for liveness / pressure facts too.
  This is still an incremental step rather than a full deletion of the older
  arrays, but it reduces the number of places that conceptually require the old
  root-indexed tables to remain the "real" state.
- that guard-rail idea now also reaches a few more queue/update helpers rather
  than stopping at local fact reads. Some move-engine utilities can now operate
  as long as the representative-owned family runtime surface exists, even when
  the callsite no longer conceptually treats the raw family arrays as the only
  authority. This is still a staged transition, but the runtime family object
  is starting to become the thing helpers are written *against*, not only the
  thing they politely keep synchronized.
- the newest slice of that same line is that family-phase candidate selection
  itself is now more explicitly representative-owned instead of repeatedly
  re-deriving the same normalized root/candidate/pair facts inside several
  picker helpers. The move engine now reads one explicit phase-candidate view
  and one explicit phase-pair view for the surviving representative root, and
  the family-phase/ready-queue side can now proceed as long as
  `family_runtime_facts` exists even when the raw root-indexed family tables
  are no longer the conceptual consumer surface. A focused regression now also
  locks that, after a coalesce event, the later remove trace for the merged
  partner does not resurface under the obsolete loser root identity.
- the newest select/spill extension of that same mainline is that runtime
  family-color support is no longer only a flat member-count idea. The
  allocator can now collect one family-color support view with current member
  count, direct coalesce-weight sum, and strongest direct partner for a given
  runtime family/color pair, and the blocker-recolor-target comparison lane now
  consumes that richer support surface when it chooses between otherwise legal
  eviction candidates. The current boundary is deliberate: main value selection
  stays more conservative about analysis-only family-color hints, while blocker
  recolor and retry recovery may still consume a broader family-color view. So
  the allocator is now a little closer to treating "how well can this merged
  family absorb recolor onto color C?" as a first-class mainline question,
  without letting that broader question leak indiscriminately into every
  ordinary free-color choice.
- the newest move-engine consolidation is that phase selection now carries the
  chosen representative-family entry snapshot down into the action object
  itself. Freeze/remove/coalesce execution no longer has to rediscover that
  family entry by indexing back through the root-keyed phase-entry table after
  a decision has already been made. That is still a small internal structural
  step, but it moves the engine one notch closer to "operate on a selected
  merged-family object" rather than "select first, then look the family up
  again through older tables during execution."

## Current Priority Meaning

The next big chunks should be interpreted like this:

### 1. Explicit Coalescing Mainline

This is the current top priority.

Goal:

- move from "family-aware pressure and preference" toward a real coalescing
  control surface
- make the allocator say not only "this family is valuable" but also
  "this is the next coalesce opportunity worth trying"
- then consume that surfaced opportunity as an explicit transition before any
  later merged-node / alias-maintenance step exists
- keep that surface deterministic and inspectable

### 2. More Formal Optimistic Retry

This remains the next step after explicit coalescing becomes more structural.

Goal:

- make spill-intended values look more explicitly provisional
- make retry more like one formal allocator phase and less like a local repair
- preserve the distinction between "temporary spill risk" and "true final spill"

### 3. More Complete Worklist Discipline

This is the consolidation step after coalescing + retry are stronger.

Goal:

- push simplify / freeze / spill / retry toward one more explicit state machine
- reduce the remaining gap between the current allocator and a textbook
  Briggs/Chaitin-style mainline
- keep family-state transitions authoritative rather than duplicated across
  independent heuristics

## Machine-Facing Sequence

Only after the allocator-mainline step is stronger should the work switch to:

1. live-range splitting
2. allocator-result shaping for backend consumers
3. machine register model
4. frame / stack-slot policy consolidation
5. instruction selection / target lowering
6. block layout / branch lowering
7. encoding / emission

This ordering is intentional:

- splitting wants a stronger allocator policy surface first
- machine register modeling wants coalescing/retry/splitting to already be
  real enough to consume
- instruction selection and emission belong after machine constraints, not
  before them

## Phase A: Finish the Short-Term Spill-Rewrite Line

This is the immediate short-term sub-mainline.

Reason:

- it directly improves `allocate + rewrite` on real CFGs
- it makes current allocator results land on more programs
- it is the most user-visible functionality still on the table before a
  larger architecture switch

### A1. Broader Edge-Specific Placement

Continue extending CFG-sensitive spill placement for:

- more phi-input families
- more split-edge reuse families
- cases where one predecessor edge needs reload materialization but sibling
  edges must remain untouched

Checkpoint:

- edge-local spill reload placement works beyond the current branch-only
  families without fragile special-casing

### A2. Loop-Carried Spill Families

Extend spill rewrite to more loop families:

- loop header consumers
- loop backedge-fed phi inputs
- loop-local reload reuse when spill-slot state is unchanged

Checkpoint:

- representative loop-carried spill CFGs survive allocate + rewrite without
  ad hoc manual handling

### A3. Multi-Join / Reconverged Reload Placement

Push the current reload reuse line through more reconverged CFG families:

- multi-join descendants
- repeated dominated uses after one materialized reload
- negative cases with intervening spill-slot clobber

Checkpoint:

- current rewrite can reuse one spill reload through more than just straight
  chains while staying conservative around clobbers

### Exit Condition For Phase A

Do **not** keep extending spill rewrite forever.

Phase A is done when:

- the most obvious CFG-placement gaps are covered
- representative loops and multi-join families are regression-locked
- new spill-rewrite work starts feeling incremental rather than foundational

At that point, switch to allocator mainline policy.

## Phase B: Strengthen the Allocator Mainline

This is the next larger mainline after the short-term spill-rewrite checkpoint.

### B1. Explicit Coalescing Mainline

This should be the next major feature after Phase A.

Target:

- consume copy-affinity as true coalescing input, not just color preference
- introduce merge decisions with conservative safety
- keep the implementation deterministic and reviewable

Likely ingredients:

- move-family representation
- coalesced representative tracking
- conservative George/Briggs-style merge safety
- fallback to freeze when merge is not safe

Desired outcome:

- fewer moves survive into rewrite/select
- fewer values need separate color pressure when safe to merge

### B2. More Formal Optimistic Coloring Retry

After coalescing starts to exist structurally, the select side should be
upgraded into a more formal optimistic-coloring discipline.

Target:

- treat some spill-remove values as provisional risks
- retry them after neighborhood collapse
- make "removed as spill candidate" and "actually spilled" diverge more often
  in a principled way

Desired outcome:

- fewer confirmed spills
- cleaner interpretation of spill-intended vs. spill-confirmed

Current early foothold:

- retry and blocker-recolor policy now already consume a slightly richer
  runtime family-color support surface than ordinary main select does
- main select stays more conservative about analysis-only family-color hints,
  while retry/recolor may still ask the broader merged-family question
  "how well can this family absorb recolor onto color C?"
- so the first real optimistic-retry direction is no longer only "add more
  observability around spill-intended"; it is starting to separate
  provisional-family recovery policy from ordinary main-pass color choice
- retry-family entry now also tracks whether a recoverable family still has
  `spill-intended` members, and top-level retry pick prefers those
  provisional families before it falls back to later scalar representative
  tie-breaks
- so late retry is now slightly less "which spilled scalar is most expensive"
  and slightly more "which provisional spill family should be repaired first
  while it is still recoverable"
- that same retry line now also has a first explicit "stay on the chosen
  provisional family" behavior instead of always bouncing straight back to a
  fresh global scalar rescan after every single recovery
- in other words, optimistic retry is beginning to act like a small family
  phase: once a provisional family wins, later recoverable intended siblings
  from that same family may be repaired before the allocator returns to the
  broader retry field
- the result surface now also records retry recovery order explicitly, so
  those phase-like choices are observable as allocator facts rather than only
  guessed indirectly from final color maps
- that family-phase line is now also no longer limited to `spill-intended`
  members only: once intended-count ties, retry-family comparison can still
  prefer the family with more recoverable members overall
- and once such a family wins, the active-family retry phase may continue
  through later recoverable confirmed-spill siblings too, instead of stopping
  the moment the intended subset is exhausted
- that same observability line now also carries explicit retry-phase identity:
  recovered values can record both "which recovery step was I?" and
  "which retry family phase did I belong to, rooted at which family root?"
- that makes the family-phase boundary itself part of the allocator result
  surface, not only a control-flow fact hidden inside the retry loop
- the result surface now also records each recovered value's position inside
  its retry family phase, so "global recovery order" and "local within-phase
  drain order" are now separate observable facts
- the retry implementation itself is now also a little more protocol-shaped:
  there is an explicit "begin phase" step and an explicit "drain active phase"
  step, rather than keeping that structure only as ad hoc state threaded
  through the top-level retry loop
- in other words, the allocator is now beginning to distinguish three
  separate retry facts:
  global recovery order, family-phase identity, and within-phase member order
- there is now also a first explicit "phase size" summary fact on recovered
  values, so one value can say not only "I was phase-step 1" but also
  "that phase drained 2 recoveries in total"
- the retry side is now also beginning to own an explicit family-agenda shape
  rather than only an ephemeral root-indexed candidate scratch array
- and the phase result surface now captures not only how a phase drained, but
  also which family candidate originally entered it and with what recoverable
  family summary
- there is now also a first public retry-family agenda surface, so later
  retry work can inspect the outer field directly instead of inferring it only
  from final recovered values
- that public agenda is now also beginning to act like real retry mainline
  input instead of remaining a read-only side artifact
- the front agenda entry now carries enough phase-entry shape itself
  (`free` vs `preferred-evict` vs `generic-evict`, preferred partner, blocker,
  blocker recolor lane) that the real retry loop can enter a phase directly
  from the surfaced agenda item rather than first publishing one family winner
  and then privately rescanning the same family just to rediscover the same
  representative choice
- so the retry protocol is now one step closer to a true
  "field -> entry -> phase-drain" mainline: the public family field is not yet
  the whole retry engine, but it is already the authoritative phase-entry
  surface
- the allocator now also owns a more explicit internal retry-field protocol
  around that surfaced entry: one long-lived field object prepares query
  scratch once, rebuilds the outer family field, enters the chosen phase from
  the front agenda item, refreshes the active family inside that phase, and
  then returns to the outer field again
- so the current retry loop is less "one outer `for (;;)` with a handful of
  locals" and more a real `rebuild field -> begin phase -> drain phase ->
  finish phase` discipline
- that same protocol now also carries an explicit progress/accounting check at
  phase finish: a retry phase must actually recover at least one member, and
  the phase's before/after spill counts must match the number of recovered
  members minus the subset of recovery claims that still left a replacement
  spilled blocker behind
- in other words, retry is no longer only shaped like a protocol; it is also
  starting to enforce protocol-level invariants about what a successful phase
  is allowed to do to the colored/spilled frontier
- the retry field now also owns a first explicit termination guard above that
  phase accounting: it records deterministic frontier signatures before each
  phase entry, stops retry if the same frontier would be re-entered again, and
  also enforces a bounded phase budget derived from function/value pressure
- so the optimistic-retry line is now a little closer to a real bounded
  protocol instead of an "assume the local heuristics always keep making new
  states forever" loop
- that frontier guard is now also less blunt than a global "seen state means
  stop everything" rule: when the same frontier reappears, retry can still
  skip roots already attempted in that frontier and continue with a later
  untried family entry if one exists
- so the field is now beginning to act like a small bounded retry worklist for
  one frontier, not merely a cycle detector that may cut off the rest of the
  current repair field too early
- there is now also a first public retry-phase trace surface, so later retry
  work can inspect whole phases directly instead of reconstructing them only
  from per-value metadata
- the newest retry-field tightening is that the surfaced `active_phase_entry`
  is no longer only the thing phase-begin glances at once before the drain
  loop goes back to root-based rescans. While one retry family phase is active,
  refresh now keeps that active entry synchronized to the currently recoverable
  representative/member summary for the same family, and phase-begin now
  starts directly from the field-owned active entry rather than pretending the
  external candidate object is the real authority. That is still a structural
  protocol step, not a new retry heuristic, but it pulls the implementation
  closer to a true `field -> active entry -> phase-drain` mainline.
- the newest slice of that same retry line is that the phase-drain loop now
  also consumes the field-owned active entry more directly instead of carrying
  one separate `best_candidate` object all the way through the phase as shadow
  authority. Retry begin now only activates the field/phase pair, and each
  drain iteration rehydrates the current candidate from the field's active
  entry before attempting recovery. This is again a structural step rather
  than a new policy rule, but it means the retry loop is now materially closer
  to being field-driven rather than "field-shaped on the outside, candidate-
  threaded on the inside."
- that same retry protocol now also has a clearer split between the two entry
  notions it actually needs:
  - `field.active_phase_entry` is the live, refreshable family entry for the
    current frontier
  - `phase.entry` is the fixed phase-begin snapshot that recovery metadata and
    phase traces should keep reporting for the whole phase
  So the implementation is no longer drifting toward "one object that both
  mutates with the live field and also pretends to be the stable phase-entry
  summary." That boundary matters because later retry work can now refresh the
  live field aggressively without corrupting the phase-entry facts attached to
  recovered values and phase traces.
- the next small consolidation step of that same line is now also real on the
  result/trace side: retry phase-entry metadata is no longer stored only as
  three unrelated scalar lanes (`entry value`, `recoverable count`, `intended
  count`) that each consumer has to keep in sync by hand. The result surface
  now stores one phase-entry summary object in the same family-entry shape,
  and retry phase traces read that object back directly. This is still a small
  structural cleanup, but it makes retry metadata feel more like one coherent
  protocol artifact and less like three parallel bookkeeping arrays.
- the newest follow-on of that same cleanup is that more retry-side consumers
  now also read the family-root meaning from that same phase-entry summary
  object instead of insisting on a separate parallel root lane first. The
  public query surface has not been fully collapsed yet, but internally the
  retry recovery path and retry phase-trace builder are now closer to treating
  "phase entry summary" as the primary artifact and the older parallel root
  bookkeeping as compatibility scaffolding rather than the conceptual source
  of truth.
- the next step of that same line is now also real on the write side: retry
  recovery no longer needs to write a separate recovery-family-root lane in
  parallel just to keep later readers alive. Internal readers and recovered
  metadata now rely primarily on the phase-entry summary object's own
  `root_value_id`, while the remaining public recovery-phase query still
  exposes the same information through compatibility accessors. So the retry
  metadata surface is now closer to "one stored artifact plus compatibility
  views" than "two equally-primary storage shapes."
- and that compatibility step has now crossed one more meaningful boundary:
  the old dedicated recovery-family-root storage lane is no longer kept around
  as dead baggage underneath. The public recovery-phase query still returns the
  same root fact, but it now derives that answer from the phase-entry summary
  object itself rather than from a parallel root array that internal code no
  longer used. This is a real structural completion step for the retry
  metadata cleanup, not only a read-side preference tweak.
- the newest tidy-up after that storage cleanup is that the artifact's own
  reset/invalid state is no longer hand-open-coded in several places with long
  field-by-field sequences. The allocator now has one shared helper-level way
  to clear retry family-entry artifacts on the production side, and the tests
  mirror the same invalid shape locally. This is not a new allocator policy,
  but it matters because it removes another little pocket of duplicated shape
  maintenance right before the retry mainline is close enough to hand back to
  broader Phase C work.
- the next structural step after that cleanup is now also real in the retry
  driver itself: the outer retry loop no longer hand-stitches
  `rebuild field -> begin phase -> drain phase -> finish phase` directly every
  time. It now drives one explicit retry phase-step helper/result, which means
  B2 is starting to use the same "small step driver over explicit result
  objects" flavor that later Phase C allocator-mainline consolidation already
  uses on the move-engine side. This does not finish the unification, but it
  materially narrows the stylistic gap between the retry engine and the
  action-machine side.
- and that retry phase-step surface is now also a little more protocol-shaped
  instead of carrying extra local booleans as shadow narration. The outer loop
  now primarily branches on one explicit phase-step status (`completed`,
  `exhausted`, `no-progress`) rather than consulting a handful of side booleans
  about whether the phase began or drained. This is a small cleanup, but it
  means the retry side is now one notch closer to the same "driver over result
  status" discipline that Phase C already uses on the move-engine path.
- the next cross-connection after that is now also real on the main-select
  side: reverse select no longer has to keep all of
  `prepare query -> resolve claim -> apply/mark spill` open-coded directly in
  the outer loop. It now drives one explicit main-select step helper/result.
  That means the allocator now has step-driver flavor on both sides:
  - retry phase-step on the optimistic-retry path
  - main-select step on the reverse-select path
  This is still not the full single allocator machine yet, but it is a real
  bridge step from late-B2 cleanup toward broader Phase C consolidation.
- that bridge step now also has one more layer above it on the select-side
  driver itself. `select_colors(...)` is no longer the place that directly
  hand-stitches "run all reverse-select steps, then run all retry phase-steps";
  it now delegates to one shared select-phase driver/result protocol above
  those two step families. That means the allocator now has:
  - main-select step
  - retry phase-step
  - select-phase driver
  which is a much clearer launching point for broader Phase C unification than
  the earlier state where only the leaf loops had been step-shaped.
- the next layer above that is now also explicit at the allocation execute
  boundary. The top-level allocation path no longer has to directly stitch
  "prepare result -> run select side -> assign spill slots" as just three
  neighboring calls in each entrypoint; it now drives one explicit
  execute-step result above the select-phase driver. So the allocator now has
  a visibly nested protocol stack on the select side:
  - main-select step
  - retry phase-step
  - select-phase driver
  - execute-step driver
  This is not yet the end of Phase C consolidation, but it is a real shift
  from "adjacent helpers" toward "one machine assembled out of explicit
  result-bearing stages."
- the next layer above that is now also starting to appear on the full
  allocation path's artifact-building side. Ordinary allocation no longer has
  to treat "build allocator plan, then execute select/slot-assign" as two
  unrelated top-level calls; it now drives one explicit pipeline-step result
  over those stages. The artifact-driven entrypoint still intentionally starts
  one level lower because it is handed a plan already, but the ordinary
  end-to-end allocator path is now more clearly a staged pipeline driver than
  a long sequence of unrelated calls.
- that build-side consolidation has now advanced one more layer too. Ordinary
  allocation no longer hand-stitches the analysis/artifact build chain
  (`def_use -> cfg -> liveness -> interference -> affinity -> prep ->
  worklist -> coalesce`) directly inside `value_ssa_allocate_function(...)`;
  it now runs through one explicit build-step result/driver before entering the
  shared entry-step/pipeline-step protocol. A focused regression also now
  locks that the full ordinary allocation entrypoint and the artifact-driven
  `from_plan(...)` entrypoint produce the same allocation dump on the same
  representative function when they are fed the same built artifacts.
- that same ordinary-entry path has now also absorbed the owned-artifact
  lifecycle framing above build-step. `value_ssa_allocate_function(...)` no
  longer directly owns and hand-threads all analysis/plan objects through
  `init -> build -> entry -> cleanup` itself; it now delegates that whole
  ordinary-entry stack to one explicit full-step driver over an owned-artifact
  state object plus `build-step -> entry-step` results, and then performs only
  the thin top-level contract check plus final cleanup gate. This is still a
  structural mainline step rather than a new allocation heuristic, but it
  pushes the public ordinary allocator entry much closer to a true staged
  orchestrator instead of a large resource-owning wrapper.
- that same top-level consolidation now also reaches the public program-level
  caller instead of stopping at one function. `value_ssa_allocate_program(...)`
  no longer acts only as a raw loop with ad hoc result preparation/cleanup; it
  now carries one explicit program-step result/completion gate above the
  per-function allocator calls. Focused regression coverage also now locks that
  the program-level path and a direct function-level allocation on the same
  representative single-function program produce identical allocation dumps.
  This is still modest compared with later machine-facing work, but it means
  both public allocator entry layers now speak much more of the same staged
  protocol dialect.
- that same Phase C protocol line now also reaches the public allocate+rewrite
  fixed-point driver. `value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(...)`
  no longer keeps its whole convergence loop as one ad hoc `for (;;)` body;
  one explicit rewrite-loop step now owns "record spill-local floors -> run
  rewrite/canonicalize -> maybe reallocate -> report iteration outcome", and
  the outer driver mainly sequences those step results until convergence.
  Focused regression coverage now also locks that the stats-bearing path and
  the thin non-stats wrapper converge to identical final allocation dumps on
  the same representative program. This is still the existing conservative
  spill-rewrite policy rather than a new heuristic, but structurally it means
  the public allocator driver stack is now much closer to speaking one staged
  protocol all the way up.
- the move-engine action-cycle itself is now also one notch less "hidden
  local loop" than before. Completing one current plan item no longer means
  "run current phase steps until some internal boolean finally returns true"
  with no shaped result at that layer; it now reports one explicit
  plan-item-completion result above the per-step protocol, including whether
  the current item actually completed and how many step-cycles were consumed.
  Existing allocator/move-transition coverage remains green, so this is
  another structural consolidation step inside the allocator proper rather than
  a new heuristic. It pushes Phase C a bit closer to a fully layered
  `plan item -> step -> choose/execute/refresh/observe` mainline.
- that same line now also reaches the whole move-engine plan driver instead of
  stopping at one current item. Completing the scheduled worklist is no longer
  only an outer `for (item)` loop that returns one raw success bit; it now
  produces one explicit plan-driver result that reports how many plan items and
  step-cycles were completed, and the top-level move-engine run now validates
  that this driver actually consumed the full planned item count before it
  publishes the final plan/trace artifacts. Tests remain green, so this is
  still structural consolidation rather than a policy change, but it means the
  move-engine stack is now layered more explicitly as
  `plan driver -> plan item -> step -> choose/execute/refresh/observe`.
- that move-engine stack now also has one explicit run-result layer above the
  plan driver rather than asking public plan/trace consumers to trust a bare
  boolean success bit. `value_ssa_alloc_run_move_engine(...)` now reports
  whether it completed, whether final plan artifacts were published, whether
  trace artifacts were published, and how many trace steps were emitted; the
  public plan and move-transition consumers validate that run-result before
  accepting the artifacts. While landing that layer we also closed a real
  contract hole in the zero-value fast path: functions that converge to
  `next_value_id == 0` now still publish a completed run-result with empty
  plan/trace artifacts instead of silently skipping the new publication marks.
  Existing allocator tests are green again, including allocate+rewrite cases
  that converge to immediate-only endpoints.
- that top-level run-result is now also starting to carry a real execution
  summary instead of stopping at "did we publish plan/trace artifacts?". The
  move-engine currently counts how many coalesce / freeze / simplify-remove /
  spill-remove step results it executed while driving the plan, so the run
  result is beginning to describe not only that the allocator finished, but
  what kind of allocator machine it behaved like during the run. This is still
  intentionally internal and small, but it is a more allocator-proper kind of
  consolidation than yet another outer wrapper: the staged mainline now has a
  first real action summary at the top.
- the move-engine decision surface is now also one notch less absolute about
  coalescing than before. Surfaced `coalesce_action` no longer wins
  unconditionally just because a mutual ready pair exists; it now has to stand
  up against the strongest currently surfaced phase-local queue signal on the
  competing simplify/freeze/spill side. The current comparison is still
  intentionally conservative and queue-priority-based rather than a full
  George/Briggs-style policy, but it is a real allocator-proper step: coalesce
  has crossed from "special lane that preempts everything when present" toward
  "first-class action that participates in the current decision surface." The
  existing allocator suite remains green across that change.
- that same coalesce-vs-other decision surface is now also regression-locked on
  a focused simplify-first family. When a mutual-ready coalesce pair is
  present but a clearly stronger simplify queue signal also exists, the move
  engine now keeps that stronger simplify remove as the first transition
  instead of letting coalesce preempt it by default. This is still far from a
  full textbook coalescing policy, but it is the clearest current sign that
  coalescing is being pulled into the same decision arena as the other action
  kinds rather than living as a structurally privileged lane.
- and the same line now also reaches coalesce-vs-coalesce competition across
  phases. The move engine no longer picks "the simplify-side pair if any,
  otherwise maybe the freeze-side pair" by hardcoded phase order; it now
  compares the best surfaced simplify-phase and freeze-phase coalesce actions
  and lets the stronger one win. Focused regression coverage now locks a case
  where a stronger freeze-phase pair must beat a weaker simplify-phase pair as
  the first transition. This is still a conservative priority comparison rather
  than the end of explicit coalescing, but it removes another structural bias
  that made coalescing look less like one real decision surface and more like a
  collection of phase-local exceptions.
- importantly, that last step also clarified one boundary for this phase. We
  explored letting one-sided ready pairs be consumed directly as coalesce
  actions too, but the current family analysis/runtime pressure model still
  treats coalesce-ready partner structure in a fundamentally symmetric
  family-pressure form. So for now the landed, regression-backed checkpoint is:
  mutual/safe pairs participate in one unified decision surface without hard
  phase-order bias, while broader one-sided consumption remains deferred until
  the surrounding move-work/coalesce semantics are made more directional on
  purpose.
- the next consolidation after that is now also real across the two public
  allocation entrypoints themselves. The artifact-driven entrypoint no longer
  looks like a different orchestration species that happens to skip straight to
  execute; it now also runs through a pipeline-flavored step, just one that
  marks "plan already built" instead of "plan built here". So the two major
  entrypoints are now more obviously siblings in one staged allocator
  pipeline, differing by where they enter the stack rather than by speaking
  unrelated control-flow dialects.
- the next layer above that is now also starting to absorb the entrypoint-local
  boilerplate itself. Both public allocation entries now share one explicit
  entry-step driver that owns `prepare result -> choose the right pipeline
  flavor -> validate stage completion -> cleanup on failure`, instead of each
  entrypoint hand-owning that same framing logic. At this point the allocator's
  top-level control flow is much closer to a true staged orchestrator than to a
  family of wrappers that merely happen to call the same helpers.
- the current cleanup edge after that is now also starting to flatten a little:
  entry-side "unknown step state" handling and failure cleanup no longer need
  to be restated separately at each public allocation entrypoint. There is now
  a shared completion-or-cleanup gate for the entry-step layer, which is small
  in code size but useful structurally because it trims one more pocket of
  entrypoint-local boilerplate near the Phase C checkpoint boundary.

### B3. Full Spill-Cost Model

Upgrade spill choice from the current local score to a more realistic model.

Candidate additions:

- loop weighting
- call-aware penalties
- rematerialization friendliness
- split opportunity bias
- stronger use-density weighting

Desired outcome:

- spill-remove choices align better with rewrite cost and future splitting

### Exit Condition For Phase B

Phase B is done when:

- coalescing is a real mainline, not just affinity preference
- optimistic retry is policy, not just observability
- spill choice is no longer driven by a very small local heuristic only

At that point it will be fair to describe the allocator as entering a more
formal Briggs/Chaitin-style mainline.

## Phase C: Briggs/Chaitin-Style Mainline Consolidation

This phase is not one feature.
It is the point where the previous slices cohere into a proper allocator
discipline.

Expected ingredients:

- explicit simplify / coalesce / freeze / spill worklists
- real move-set state transitions
- more formal optimistic select/retry behavior
- coalescing safety integrated with worklist transitions
- spill candidates managed as part of one coherent policy
- a clear "current phase decision" protocol inside the allocator main loop,
  so the engine decides one explicit action object (`coalesce`, `freeze`,
  `simplify-remove`, or `spill-remove`) before it mutates runtime state,
  rather than open-coding that phase arbitration ad hoc inside the loop body
- the next structural step after that decision surface is to make the loop
  actually execute through one decision-dispatch path, so the mainline reads
  more like `choose action -> execute action -> refresh` and less like
  "choose an action object, then immediately re-expand into another large
  branch ladder"
- once that dispatch path is live, the shadow branch ladder underneath should
  be removed rather than kept around indefinitely, so the move engine truly
  has one action protocol instead of one real path plus one dead fallback
  implementation still squatting in the loop body
- after the dead ladder is gone, the remaining loop glue should also shrink
  behind one explicit "run current phase step" helper, so the outer driver is
  no longer responsible for hand-threading action selection, execution, and
  underflow handling itself
- from there, the next cleanup step is to let the outer driver hand off
  "finish the current plan item" as one unit too, so the main loop is closer
  to a pure plan-item driver and less a nested control-flow skeleton
- once that is in place, the boolean "did this finish the current item?"
  signal should itself become a more formal step-result surface, so later
  mainline work can grow richer action-cycle outcomes without returning to
  ad hoc control flags
- after the step-result surface exists, post-action refresh routing should
  also be treated as part of that protocol rather than remaining buried inside
  each action lane, so coalesce-pair refresh versus ordinary post-freeze /
  post-remove refresh becomes explicit step metadata too
- there is one deliberate near-term compromise here: trace/plan "after-state"
  capture still currently assumes refresh has already been applied before the
  action helper finishes, so the refresh *policy* can be surfaced through step
  results before the refresh *execution point* itself is fully hoisted out of
  those helpers
- once that coupling point is isolated, the next real consolidation step is to
  let the step driver itself own the full sequence
  `choose -> execute -> refresh -> observe`,
  so action helpers only mutate runtime state plus fill step-result metadata,
  while observation/trace/plan-after capture happens in one shared post-step
  path

This phase should be treated as a consolidation checkpoint rather than a
single patch series.

## Phase D: Live-Range Splitting

Only after Phase B/C should live-range splitting begin.

Why later:

- it multiplies rewrite complexity
- it wants a stronger spill-cost model
- it wants more mature select/coalesce policy
- it benefits from already-good CFG-aware spill placement

Target:

- split values locally around pressure points
- avoid whole-value spills when only a region is expensive
- reuse existing spill rewrite machinery as the lowering substrate for split
  subranges

## Phase E: Target / Machine Register Model

This is the final major transition.

Only begin this once the allocator is already strong in the abstract-color
world.

Expected additions:

- precolored nodes
- register classes
- call-clobber constraints
- fixed instruction constraints
- parallel copy resolution
- physical register policy

At that point the allocator stops being only an SSA-coloring experiment and
becomes a real machine allocator.

## Recommended Near-Term Order

This is the practical near-term sequence:

1. finish one more focused round of spill-rewrite CFG coverage
2. switch to explicit coalescing mainline
3. strengthen optimistic coloring retry
4. expand spill-cost model
5. consolidate into a more formal Briggs/Chaitin-style worklist discipline
6. only then start live-range splitting
7. only after that begin target / machine register modeling

## Guidance For Future Turns

When deciding what to do next, prefer these rules:

- if the next change makes `allocate + rewrite` work on more representative SSA
  CFGs, it still belongs to short-term Phase A
- if the next change mostly helps avoid spills or remove moves before rewrite,
  it belongs to Phase B
- if the next change requires precolored registers or physical constraints, it
  belongs to Phase E and is probably too early

## Short Version

The allocator is now in this position:

- already real enough to keep using
- not yet formal enough to be called complete

So the plan is:

- one more short spill-rewrite completion stretch
- then coalescing
- then optimistic retry
- then richer spill cost
- then full allocator consolidation
- then splitting
- then machine constraints
