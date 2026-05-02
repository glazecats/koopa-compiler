# Machine IR Plan

## Goal

This document starts the next real backend-facing mainline after the
`value_ssa_machine` checkpoint.

The intent is:

- keep `value_ssa`, allocator, and `value_ssa_machine` as analysis/planning
  layers
- add one independent `machine_ir` representation that later instruction
  selection / target lowering can actually consume

In short:

- `value_ssa_alloc` answers where values live
- `value_ssa_machine` explains what those abstract colors mean in a machine
  register world
- `machine_ir` becomes the first standalone IR that carries those decisions
  as explicit operands

## What This Layer Is

The first `machine_ir` slice is intentionally conservative.

It is **not** final assembly, and it is **not** a full target instruction
selector yet.

It is:

- a program/function/block IR
- still CFG- and phi-aware
- still allowed to mention local/global slots
- machine-placement-aware at the operand level

That means values stop being referenced as:

- `ssa.N`

and start being referenced as:

- `reg.N`
- `spill.N`
- immediates

This is already a useful boundary shift even before:

- phi elimination
- parallel copy resolution
- precolored constraints
- instruction-class constraints
- final target op selection

## Why Not Jump Straight To Instruction Selection

Because we still want one stable layer where later backend work can ask:

- which SSA value ended up in which machine register?
- which values are already forced through spill slots?
- what does a CFG with phi, branch, and call look like after those placement
  choices become explicit?

That gives us one reusable authority layer before target-specific lowering
starts mutating control-flow edges and introducing copy resolution.

## First Minimal Surface

### Public module boundary

- `include/machine/ir.h`
- `src/machine/lowering/machine_ir/`
- `tests/machine/lowering/machine_ir/`

Current repository layout note:

- machine public headers now live under `include/machine/`
- `machine_ir` is the lowering-tier entry inside `src/machine/lowering/`
- later machine directories are grouped by role rather than left as dozens of
  flat top-level `src/machine_*` siblings

### First program model

- register bank copied into the IR program
- globals copied from Value-SSA program metadata
- functions/blocks copied from Value-SSA CFG shape
- phi nodes retained
- instructions retained in a still-generic form:
  - `mov`
  - `binary`
  - `call`
  - `load_local`
  - `store_local`
  - `load_global`
  - `store_global`
- terminators retained:
  - `ret`
  - `jmp`
  - `br`

### First operand model

- immediate
- machine register
- spill slot

This is the main point of the new layer.

## First Bridge

The first lowering bridge should consume:

- `ValueSsaProgram`
- `ValueSsaProgramMachineAllocationView`
- `ValueSsaMachineRegisterBank`

and produce:

- `MachineIrProgram`

This means first bridge authority lives *after* allocation and machine-view
projection, not before.

## Verification Scope

The verifier should initially lock:

- dense register/global/block ids
- valid register references
- valid spill-slot references
- valid local/global slot references
- per-block required terminators
- phi predecessor validity and uniqueness
- entry reachability of all blocks

## Staging

### Stage MIR1: Representation Skeleton

- public `machine_ir` structs
- init/free lifecycle
- manual builder helpers
- verifier
- dump

### Stage MIR2: First Value-SSA Bridge

- lower allocated Value-SSA program into machine IR
- retain CFG shape
- lower SSA values into `reg/spill/imm` operands
- keep phi nodes as-is

### Stage MIR3: First Consumer Migration

- begin preferring `machine_ir` over raw machine reports for the first real
  backend-facing consumer
- likely future home: instruction selection / target lowering staging

### Stage MIR4: Machine Cleanup Pressure

Only after MIR2/3 are stable:

- phi elimination
- copy resolution
- register-class-aware machine instruction constraints
- tighter call-clobber interactions
- public machine-ir canonicalization / cleanup surface that downstream
  consumers can ask for directly, rather than only through the older
  `cleanup_after_phi_elimination` naming

## Progress Snapshot

- `Stage MIR1 representation skeleton`: **~96%**
  - the first public header, lifecycle, manual builder helpers, verifier, and
    dump line are all landed as a real sibling module
  - focused `machine_ir` tests now cover both manual construction and
    verifier-backed rejection on malformed spill-slot usage
- `Stage MIR2 first Value-SSA bridge`: **~80%**
  - a real bridge now lowers `ValueSsaProgram +
    ValueSsaProgramMachineAllocationView + ValueSsaMachineRegisterBank` into
    `MachineIrProgram`
  - regression now covers both:
    - allocate+lower smoke from real Value-SSA
    - hand-shaped machine-view lowering that explicitly locks register/spill
      operand projection across phi/instruction/terminator sites
  - the module now also has a first public query surface plus direct
    lower+dump convenience entrypoints, so higher-level consumers no longer
    need to restage the same `machine_view -> machine_ir -> dump` plumbing by
    hand
- `Stage MIR3 first consumer migration`: **~86%**
  - a first real consumer-style entrypoint now exists above the raw bridge:
    callers can request `allocate + rewrite + machine view rebuild + machine_ir`
    directly, or the matching direct dump artifact, without stopping at the
    older machine-report layer
  - that line now also has a first formal artifact lifecycle of its own:
    `MachineIrAllocateRewriteReport` carries allocate/rewrite stats plus the
    final `MachineIrProgram`, with query/dump helpers and direct flat-bank
    convenience entrypoints
  - the report line now also has first program-level navigation/filter
    surfaces (`functions-with-phi`, `functions-with-call`,
    `functions-with-spills`, `functions-register-only`), so higher-level
    consumers can start triaging/reporting directly on machine-ir artifacts
    instead of falling back immediately to older machine-facing report layers
  - that same report layer now also exposes first function-level shape
    summaries (`phi`, `call`, spill-slot count, local/global load/store
    counts, total memory-op count), so downstream consumers can inspect the
    machine-ir shape of one function without rescanning raw instruction lists
  - report shape now also extends one level deeper to block summaries, so the
    artifact can answer not only "which functions are phi/memory-heavy?" but
    also "which blocks inside that function carry those shapes?" without
    forcing later consumers to immediately rescan raw machine-ir blocks
  - this is still intentionally a partial migration rather than a full
    replacement of all machine-facing report consumers
- `Stage MIR4 machine cleanup pressure`: **~99%+**
  - the first real cleanup consumer is now landed: `machine_ir` has an
    initial phi-elimination pass instead of leaving all phis permanently
    intact
  - this first slice is already stronger than a toy single-phi rewrite:
    current coverage locks multi-phi parallel-copy emission plus cycle
    breaking via conservative scratch spill slots on predecessor edges
  - that same first slice now also covers a true critical-edge family rather
    than only non-critical joins: the current implementation can split the
    phi-carrying branch edge conservatively and keep the transformed CFG
    verifier-legal
  - scratch-slot policy is now also one notch less noisy than the first cut:
    phi elimination no longer grows an unrelated fresh scratch slot for every
    predecessor edge by default, and current cycle-heavy coverage now locks
    shared scratch-slot reuse across the participating predecessor copies
  - the current split policy is now also better aligned with real need:
    a true critical-edge case is regression-locked, while a trivial
    self-copy-only critical-edge family is now also locked to avoid creating
    an unnecessary split block in the first place
  - that same cleanup line now also reaches the current report artifact
    surface: `MachineIrAllocateRewriteReport` can be phi-eliminated in place
    and then refresh its program/function/block summaries against the
    post-cleanup machine-ir shape
  - phi elimination is now also a self-verifying cleanup boundary rather than
    a "best effort" shape rewrite: the pass re-runs the machine-ir verifier on
    its own output before returning success
  - a first post-phi cleanup stack is now landed too, not only raw phi
    removal: current cleanup can eliminate trivial self-moves and compact
    empty jump-only join blocks after phi elimination while keeping report
    summaries in sync
  - that post-phi cleanup stack now also acts like a real tiny CFG-cleanup
    pipeline rather than one-shot local deletion: trivial same-target branches
    and immediate-condition branches can now collapse to `jmp`, and the
    follow-up cleanup iterates with empty-block compaction plus unreachable
    block removal until the machine-ir CFG stabilizes again
  - that same CFG-cleanup line now also reaches more structural block
    reshaping rather than stopping at local rewrites: jump targets can thread
    across trivial empty jump chains, and single-predecessor linear jump
    blocks can now merge into their predecessor once the CFG has been cleaned
    enough for that merge to be safe
  - the cleanup line is now also starting to become machine-specific rather
    than purely CFG-shaped: a first conservative local copy cleanup can
    propagate register/spill copies within one block, rewrite later block-local
    uses and terminators through those aliases, and then delete dead move
    results using machine-ir block live-out facts
  - that same copy-cleanup line now also extends one conservative step across
    CFG boundaries instead of stopping strictly inside one block: current
    machine-ir cleanup can carry copy aliases through straight-line
    single-predecessor jump chains while still refusing to treat join-shaped
    paths as if they carried one unique incoming copy fact
  - the current copy-cleanup implementation boundary is now also more explicit
    than before: block-exit alias facts are materialized first and then
    consumed on the next rewrite pass, so the present straight-line cross-block
    rule is no longer relying on one fragile intra-pass ordering accident even
    though the broader branch-side edge-sensitive extension is still intentionally
    conservative rather than turning into full CFG-wide copy propagation
  - that broader edge-sensitive extension now also has a first landed slice:
    branch-side unique-successor propagation is now in scope, so a successor
    reached from exactly one predecessor edge may consume that predecessor's
    copy facts even when the predecessor itself ended in a branch, while
    join-shaped targets no longer have to remain entirely out of scope either:
    if all incoming predecessor edges agree on the same copy fact for one
    resource, the current cleanup can now propagate that fact through the join;
    disagreement across predecessors still conservatively blocks propagation
  - current regression authority now locks that stronger cleanup shape
    explicitly, including a representative family where
    `phi-elim -> trivial-move cleanup -> empty-jump compaction` collapses a
    join block away entirely
  - current regression authority now also locks the newer CFG-cleanup chain
    directly, including both `same-target br -> jmp` and
    `imm-branch -> jmp -> unreachable/empty-block cleanup`
  - current regression authority now also locks the new structural cleanup
    follow-through, including representative `jump-thread -> further cleanup`
    and `linear merge after cleanup` families whose final form is a tighter
    machine CFG than the earlier intermediate states
  - current regression authority now also locks the first machine-specific
    copy-cleanup benefits directly, including local copy propagation into later
    block-local instructions/terminators and dead move elimination after those
    rewrites
  - current regression authority now also locks the first cross-block copy
    boundary directly: a straight-line chain should inherit copy facts, while
    a branch/join family with one side clobbering the copied resource must
    stay conservative at the join
  - in other words, the current authority line is now explicit about both
    sides of that boundary: straight-line propagation, branch-side
    unique-successor propagation, and join-aware must-agree propagation are in
    scope today; conflicting multi-predecessor facts still block propagation
  - the cleanup line is now also beginning to consume real machine register
    facts rather than only CFG/value shape: call sites now kill copy facts in
    caller-clobbered registers while still allowing callee-preserved registers
    to retain their propagated facts across the call boundary
  - the CFG cleanup side now also folds one more real backend-style branch
    family instead of only tail chains: when the two branch successors are
    structurally identical blocks, the branch may now collapse to one direct
    path and let later cleanup consume the shared tail
  - that same branch-side cleanup now also reaches the common "same thin
    single-instruction wrapper" family, so two branch successors that both do
    the same one-instruction wrapper work before the same tail no longer need
    to survive as separate blocks
  - branch-side cleanup is now also one step more semantic than exact-shape
    matching alone: equivalent inlineable return wrappers may merge even when
    the wrapper result register names differ, as long as the wrapped operation
    and returned meaning still agree
  - split-block cleanup now also reaches branch-wrapper tails rather than only
    return wrappers: a `jmp` into an empty `branch` wrapper or a thin
    `mov; branch` wrapper may now fold back into the predecessor and feed the
    later branch/tail cleanup stages
  - that branch-wrapper line is now also more semantic than exact-shape
    matching alone: equivalent inlineable branch wrappers may merge even when
    their result register ids differ, as long as the wrapped operation and
    successor behavior still agree
  - the cleanup stack now also has its own machine-ir-native value layer above
    CFG cleanup: immediate binary folding and a small conservative identity
    simplification set can rewrite machine-ir binaries to `mov`, and the
    existing copy/tail cleanup layers now consume those new `mov`/`ret`
    opportunities in the same iterative pass
  - that same cleanup stack is now also surfaced as a formal public
    machine-ir canonicalization boundary:
    `machine_ir_canonicalize_program(...)`,
    `machine_ir_canonicalize_program_in_report(...)`, and matching dump/report
    builders are now the preferred API names for the full iterative cleanup
    pipeline, while the older `cleanup_after_phi_elimination` entrypoints stay
    as compatibility aliases
  - the cleanup stack now also has a first machine-ir-native slot/value
    consumer above raw CFG/value cleanup: conservative known-slot cleanup may
    forward same-block local/global `load_*` from earlier known `store_*`
    facts, remove redundant same-value `store_*`, kill known global slot facts
    at `call`, and then feed the existing machine-value/value-tail cleanup
    stages
  - that machine-ir-native value layer is no longer limited to `mov` cleanup
    only: dead pure defs (`mov`, safe `binary`, `load_local`, `load_global`)
    may now be removed outright when their result resource is unused, while
    effectful instructions such as `call` and `store_*` remain preserved
  - terminal return-wrapper cleanup is now also more general than "only
    `mov; ret`": one safe inlineable pure instruction followed by `ret` may be
    folded back into the predecessor tail, while dangerous binary wrappers
    (`div/mod/shift`) remain intentionally out of scope
  - the current cleanup line now also reaches the terminal CFG boundary rather
    than stopping at interior copies/blocks: a `jmp` into an empty `ret` block
    may now fold into a direct `ret`, and a branch whose two empty successor
    tails both return the same value may now fold into that same direct `ret`
  - that terminal cleanup now also eats one more realistic backend-style tail
    wrapper shape instead of requiring the tail block to be literally empty: a
    thin `mov; ret` tail and the matching same-valued branch-pair form may now
    fold away to the wrapped direct `ret`
  - the current implementation should still be read as a first safe lowering
    step, not yet the final machine cleanup stack: copy resolution is still
    conservative, scratch-slot growth is intentionally simple, and later
    machine constraint work has not started yet

## Current MIR4 Position

- `MIR4.a phi elimination`: effectively complete for the current first slice
  and now verifier-backed
- `MIR4.b immediate post-phi cleanup`: now in progress at roughly **99%**
  - trivial self-move elimination is landed
  - empty jump-only join compaction is landed
  - report-integrated cleaned artifacts are landed
  - trivial branch simplification is landed
  - unreachable-block cleanup is landed
  - jump-target threading across trivial jump chains is landed
  - conservative single-predecessor linear block merge is landed
  - conservative local copy propagation is landed
  - dead move elimination from machine-ir block liveness is landed
  - straight-line cross-block copy propagation is landed
  - branch-side unique-successor copy propagation is landed
  - join-aware must-agree copy propagation is landed
  - current cross-block propagation is now implemented through an explicit
    block-exit-fact plus block-entry-meet flow rather than a more brittle
    one-pass ordering dependence
  - jump-to-return tail folding is landed
  - same-return branch tail folding is landed
  - jump-to-thin-return-wrapper folding is landed
  - same thin-return-wrapper branch tail folding is landed
  - jump-to-thin-jump-wrapper folding is landed
  - jump-to-thin-jump-wrapper-then-thin-return folding is landed
  - caller-clobbered registers now kill copy facts across calls
  - callee-preserved registers may keep copy facts across calls
  - identical branch successors may now fold to one direct path
  - same single-instruction wrapper branch successors may now fold to one
    direct path
  - equivalent inlineable return-wrapper branch successors may now fold to one
    direct path even when their temporary/result ids differ
  - empty branch-wrapper tails may now fold into the predecessor
  - thin `mov; branch` wrapper tails may now fold into the predecessor
  - equivalent inlineable branch-wrapper successors may now fold to one direct
    path even when their temporary/result ids differ
  - machine-ir-native immediate binary folding is landed
  - machine-ir-native conservative identity simplification is landed
  - dead pure def cleanup over `mov` / safe `binary` / `load_*` is landed
  - inlineable single-instruction return-wrapper folding is landed
  - public canonicalization entrypoints are now landed above the current
    cleanup stack, with regression locking that the new canonicalization
    surface matches the older cleanup behavior
  - first machine-ir-native known-slot cleanup is now landed: same-block
    local/global load forwarding, redundant same-value store cleanup, global
    call-kill of known slot facts, and value-fold follow-through from those
    forwarded loads are all regression-locked
  - what is still missing here is less "basic CFG hygiene" and more the next
    machine-specific cleanup pressure, for example tighter split-edge/block
    cleanup rules and later machine-constraint-aware cleanup hooks
- `MIR4.c later machine cleanup pressure`: now effectively **~98%**
  - first machine-ir-native slot/value cleanup is landed
  - that same slot-aware line is now no longer block-local only:
    block-entry/block-exit known-slot facts, straight-line cross-block
    propagation, must-agree join propagation, and register-redefinition /
    call-clobber invalidation of remembered slot values are all landed and
    regression-locked
  - that slot-aware line now also has the other half of a real cleanup family:
    conservative dead-slot-store elimination is landed for same-block
    overwrite, straight-line cross-block overwrite, and still-observed join
    families, while global stores before `call` remain preserved as observed
    side effects
  - slot-aware cleanup now also consumes the current copy-alias surface more
    deeply instead of tracking slot values as raw unreduced operands only:
    stored slot facts are canonicalized through current `mov` alias chains,
    later forwarded loads can reuse those alias-normalized values directly,
    and regression now locks that slot facts survive benign register-copy glue
  - machine-ir now also has a first reusable internal global call-effect
    summary feeding cleanup instead of treating every internal call as an
    all-global black box: slot cleanup can preserve known facts for globals a
    callee provably does not write and can avoid inventing fake read barriers
    for globals a callee provably does not read, while still keeping final
    return-observed global state conservative
  - slot-aware cleanup now also folds a first edge-sensitive wrapper family
    directly from predecessor-local facts instead of waiting for a join-wide
    meet: `jmp -> load_slot; ret` wrappers may collapse to direct `ret` from
    the jumping block's own known-slot exit facts, and branch-side siblings
    can now fold to different direct returns on different predecessors when
    their outgoing slot values differ
  - that same edge-sensitive slot-wrapper line now also reaches
    `load_slot; br` wrappers rather than only `load_slot; ret`: if one
    predecessor already knows the slot condition value on exit, cleanup may
    collapse the wrapper to a predecessor-local branch and then let the
    existing branch simplification / CFG cleanup consume the now-constant path
  - the edge-sensitive slot-wrapper line now also reaches branch nodes whose
    two successors are themselves load-slot wrappers for the same slot
    family: cleanup may now collapse both `load_slot; ret` and `load_slot; br`
    successor pairs directly from the current block's own known-slot exit
    facts, not only from one predecessor feeding one jumped-to wrapper
  - the cleanup line now also has a first stable staged rewrite substrate for
    “replace current block with one reconstructed instruction plus a new
    terminator”, and that substrate is already consumed by the first
    inlineable slot-wrapper slice: wrappers like `load_slot; add ...; ret`
    and `load_slot; xor ...; br` can now fold through the known slot value
    instead of only the earlier bare `load_slot; ret/br` families
  - that staged slot-wrapper line now also reaches shared successor families,
    not only one jumped-to wrapper at a time: if both branch successors are
    equivalent inlineable wrappers over the same loaded slot, cleanup may now
    collapse the pair through the known slot value and then hand the result to
    the existing value/CFG cleanup stages
  - slot-wrapper cleanup no longer needs to wait for later CFG passes to
    expose only direct wrapper targets first: it now threads through trivial
    jump chains itself before matching wrapper families, so split-edge /
    jump-chain noise can still collapse at the slot-wrapper stage
  - the shared-successor line now also composes cleanly with both threaded
    jump chains and the staged inlineable-wrapper substrate: equivalent
    `load_slot + pure op + ret/br` successor families can now collapse even
    when each successor is reached through a trivial jump chain rather than as
    a direct edge
  - that same shared inlineable line is now also slightly more semantic than
    “the two rebuilt instructions must be byte-for-byte identical”: shared
    slot-derived wrappers whose rebuilt pure instruction is semantically
    equivalent but names a different result register may now still collapse
  - internal-call global-effect facts are now also exercised by the wrapper
    folding mainline itself, not only by raw slot fact maintenance: unrelated
    globals can still feed return/inlineable wrapper collapse across internal
    calls that do not touch them, while wrappers reading globals a callee may
    write still stay conservative
  - machine-ir now also has a first shared internal machine-constraint helper
    layer rather than leaving resource-slot and result-placement facts buried
    inside one cleanup subpass: resource indexing and result-placement
    compatibility can now be reused across later machine-aware cleanup
    consumers directly
  - the first explicit machine-constraint-aware cleanup hook is now landed:
    equivalent inlineable wrapper collapse no longer crosses mismatched
    result-placement families such as caller-clobbered versus
    callee-preserved register results, and regression now locks that
    conservative boundary directly
  - that same machine-constraint line now also has a first positive folding
    consumer above the old byte-for-byte wrapper check: same single-
    instruction branch-successor cleanup may now collapse semantically
    equivalent wrappers whose result registers differ, as long as their
    result-placement family is still compatible, which now reaches real
    `call; ret` wrapper pairs in addition to the older literal-equality cases
  - the wrapper-tail mainline now also reaches the first non-inlineable
    machine-facing `call` families directly instead of stopping at pure
    `mov/load/binary` wrappers: `jmp -> call; ret` and `jmp -> call; br`
    wrappers may now collapse into the predecessor block while preserving the
    explicit machine-facing call/result shape
  - that same `call`-wrapper line no longer depends on later CFG cleanup to
    expose only direct targets first: trivial jump-chain noise may now be
    threaded away before matching `call; ret/br` wrappers, so the first
    machine-facing call-wrapper folds also survive split-edge / jump-chain
    clutter instead of only pristine direct-edge shapes
  - the generic inlineable branch-wrapper tail line has now also been closed
    as a real cleanup path rather than only a nominal hook: the pass now
    matches true `instr; br` wrappers instead of accidentally reusing the
    return-wrapper matcher, and regression now locks one non-constant-driven
    `binary; br` predecessor-fold family directly
  - generic branch-successor folding no longer needs perfectly direct
    successor edges before it can see equivalent wrappers: the current
    `same single-instruction` and `equivalent inlineable branch` successor
    folds now resolve through trivial jump chains first, so the shared
    machine-facing cleanup family also survives threaded CFG noise instead of
    only direct-edge shapes
  - that same alignment now also reaches the older literal-identical branch
    successor fold, so the current generic branch-successor cleanup family is
    no longer split between “some rules are threaded-target-aware” and “the
    simplest identical-wrapper rule still wants only direct edges”
  - the same cleanup-family alignment now also reaches the generic return-side
    wrapper rules: thin `mov; ret` and inlineable pure `instr; ret` tail
    folds now resolve through trivial jump chains before matching, instead of
    leaving only the newer slot- and call-wrapper return rules threaded-aware
  - likely next line: either keep pushing those explicit constraint hooks into
    later copy/slot cleanup consumers where real machine-facing pressure
    appears, or checkpoint MIR4.c and begin the next larger machine/backend
    consumer line

## Current Mainline

This is now the preferred active backend-facing mainline.

`value_ssa_machine` should be treated as checkpoint-close and
maintenance-first unless a concrete gap blocks `machine_ir` progress.
