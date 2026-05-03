# Machine Layout / Branch-Lowering Plan

## Goal

The next backend-facing mainline after `machine_select` is to turn selected
CFG-shaped programs into a **layout-aware backend entry layer**.

This stage should answer:

- in what order will blocks appear?
- which control transfers become fallthrough?
- which branches still need two explicit targets?

In short:

- `machine_select` decides selected operation families
- `machine_layout` decides linear block order and branch/fallthrough shape

## Module Boundary

Create a new sibling module:

- `include/machine/layout.h`
- `src/machine/lowering/machine_layout/`
- `tests/machine/lowering/machine_layout/`

Do not fold this into:

- `src/machine/lowering/machine_select/`
- `src/machine/lowering/machine_ir/`

`machine_select` should remain the selected-CFG authority.
`machine_layout` should become the first linear-layout authority.

## First Minimal Contract

The first version should stay conservative:

- consume verifier-legal `MachineSelectProgram`
- preserve function structure
- linearize blocks in an explicit layout order
- lower control transfers into:
  - return families
  - jump
  - fallthrough
  - conditional branch with explicit taken/fallthrough roles
  - compare-branch with explicit taken/fallthrough roles

Do not start with:

- real label/address assignment
- final machine encoding
- relaxation / branch-distance rewriting

## First Public APIs

- `machine_layout_lower_program_from_machine_select(...)`
- `machine_layout_lower_program_from_machine_ir(...)`
- `machine_layout_verify_program(...)`
- `machine_layout_dump_program(...)`

## Staging

### Stage ML1: Representation Skeleton

- public structs
- lifecycle
- verifier
- dump
- isolated tests

### Stage ML2: First Lowering From `machine_select`

- explicit layout order
- jump-to-fallthrough lowering
- branch-to-fallthrough lowering
- compare-branch-to-fallthrough lowering

### Stage ML3: First Upstream Bridge

- `machine_ir canonicalize -> machine_select -> machine_layout`

## Current Position

- `ML1 representation skeleton`: now effectively **~82%**
  - public `machine_layout` structs, lifecycle, verifier, dump, and isolated
    tests are landed as a real sibling module
  - the layout layer now also preserves the selected-op stream itself, not
    only control terminators, so dumps/summaries/verification can observe a
    whole linearized selected block instead of only its tail branch shape
  - program-level machine metadata needed by those selected ops now also
    survives the boundary, including register-bank and global-slot tables
- `ML2 first lowering from machine_select`: now effectively **~99%**
  - one first lowering path from verifier-legal `machine_select` is landed
  - the layout layer now also has a first real block-ordering policy rather
    than preserving source block order blindly
  - current ordering is still intentionally greedy/local: start at entry and
    visit preferred successors first so direct fallthrough opportunities are
    created early
  - that local ordering policy is now also one step more selective than
    generic DFS: on two-way branches it may prefer a single-predecessor,
    chain-extending successor as the fallthrough path instead of following a
    fixed raw successor order
  - that same branch policy now also has a first explicit trace-span tie-break:
    when both sides look similarly local-profitable, layout may prefer the
    successor that leads into the longer single-predecessor chain rather than
    choosing only by one-hop shape
  - branch-local trace scoring now also respects the shared-tail defer rule
    more literally: a successor that only jumps immediately into a
    multi-predecessor tail that local growth would defer anyway no longer gets
    to claim that shared tail as if it were an ordinary local trace extension
    for branch-ordering purposes
  - the current ordering policy now also has a first explicit "defer shared
    merge tail" rule: when one branch arm reaches a multi-predecessor merge by
    a straight jump, layout may postpone that shared tail until after the
    branch arms have been placed, instead of sinking into the merge
    immediately and breaking the emerging local trace
  - that shared-tail defer rule is now also stronger inside one local trace:
    once layout chooses to defer a multi-predecessor merge tail from a
    single-predecessor jump arm, it keeps that shared tail for the later
    seed-selection phase rather than re-consuming it immediately just because
    the final missing predecessor happened to be placed meanwhile
  - seed selection is now also beginning to do one small function-level
    stitching step above those local branch rules: when a deferred shared tail
    has all predecessors already laid out, it may now win as the next seed
    ahead of an unrelated longer-but-still-unready deferred trace
  - that seed selection policy is now also explicit in structure rather than
    only implicit in comparator arithmetic: current seed classes distinguish
    ready shared-merge tails, ready continuations, attached-but-unready work,
    and fully detached traces before later tie-breaks consult visited
    predecessor counts, trace span, and block id
  - attached/ready seed frontiers now also have a first explicit freshness
    tie-break of their own: when multiple non-detached seeds compete,
    function-level stitching may prefer the one whose visited predecessor was
    touched most recently in the current layout, so layout can continue the
    freshest deferred region before falling back to broader visited-count,
    trace-length, or block-id tie-breaks
  - that freshness rule now has explicit authority on both sides of the
    current function-level split:
    newer ready shared-merge tails may beat older ready shared tails, and a
    fresher ready continuation may also beat an older continuation even when
    the older one still advertises a longer deferred trace
  - the current heuristics now also share one function-level trace-span view
    rather than each open-coding fresh recursive estimates on demand, so
    branch ordering and seed ordering are starting to consume one common
    trace-length substrate instead of drifting apart implementation-wise
  - current authority should still be read conservatively: that shared-tail
    defer rule wins over a more aggressive "merge becomes ready, consume it
    immediately" policy. In other words, the layout layer is currently more
    committed to preserving local branch-arm trace shape than to stitching a
    shared merge tail back in at the earliest possible moment
  - `jmp` to next-layout-block lowers to `fallthrough`
  - plain `br` lowers to either `brft.*` or stays two-target `br`
  - `cmpbr` / `cmpbri` lower to either explicit fallthrough forms or stay
    two-target compare-branch forms
  - the current lowering boundary now also keeps selected ops attached to
    their layout block rather than dropping the block body on the floor
  - current direct-layout authority now also locks that a branch may reorder
    later source blocks to make its preferred fallthrough successor adjacent,
    rather than only exploiting source order when it already happens to help
  - that direct-layout authority now also has explicit compare-family
    siblings for some earlier branch-only heuristics, not only later bridge
    families: the current direct matrix now locks compare-side behavior for
    the most local branch-reorder preference and for the
    "multiple ready shared merges compete" freshness family too
  - that direct compare-family matrix now also reaches several older local
    branch-shaping heuristics that used to be branch-only: the
    single-predecessor fallthrough preference, the local trace-span tie-break,
    and the "do not count an immediately deferred shared tail as ordinary
    local trace" scoring rule now all have explicit compare-side siblings too
  - that same direct-side surface now also has a first explicit preview-lane
    compatibility summary plus verifier entrypoint, so the current preview
    register-cap / spill-preservation / global-slot-preservation /
    fallthrough-shape facts are no longer visible only above layout in
    `machine_select` or only below it in later emit/bytes layers
  - that same layout side now also has a first direct program/function/block
    query surface instead of leaving consumers with only dumps plus
    `compute_summary`: callers can now query whole-program counts, look up one
    function by index or name, inspect one function summary, and recover one
    layout block summary directly from the structured artifact
  - that same layout side now also has a first structured report artifact
    instead of forcing later consumers to choose between raw program structs
    and textual dumps: callers can now build/refresh/dump a layout report,
    recover cached function/block summaries plus policy facts from it, and
    stay on one artifact boundary before entering `machine_emit`
  - that same layout-side compatibility surface now also has a first deeper
    downstream bytes-bridge check instead of only local shape screening:
    callers can now ask `machine_layout` to validate current
    `riscv32-preview` compatibility by actually attempting the
    `machine_layout -> machine_emit -> machine_encode -> machine_bytes`
    preview path, so bytes-side range failures surface several stages earlier
    than before
  - that same direct compare-family coverage now also reaches one seed-class
    ordering boundary that used to be locked only through later bridge work:
    `ready continuation > attached-but-unready` now has an explicit
    compare-side sibling on direct `machine_select -> machine_layout` input
- `ML3 first upstream bridge`: now effectively **~99%**
  - the `machine_ir canonicalize -> machine_select -> machine_layout` bridge
    is landed as a first real public entrypoint
  - current bridge authority is intentionally based on canonicalized
    `machine_select` output, so layout tests must not assume upstream block
    counts are preserved when `machine_select` cleanup can already collapse a
    CFG to one block
  - direct `machine_select` tests and bridge-from-`machine_ir` tests should
    now be treated as separate authorities: one locks layout behavior itself,
    the other locks what survives the upstream canonicalized bridge
  - bridge-side authority now also explicitly locks that the current
    freshness-driven seed stitching survives upstream canonicalization even
    when `machine_ir` cleanup folds linear jump chains and compacts the CFG
    before `machine_select` / `machine_layout` see it
  - bridge-side authority now also has one more explicit "what actually
    survives canonicalization" regression for effectful shared-tail
    micro-shapes: when jump arms are kept alive by real effectful ops instead
    of empty wrappers, tests now lock the canonicalized layout that reaches
    `machine_layout`, rather than assuming the bridge must preserve the same
    block decomposition as the direct `machine_select` authority
  - bridge-side authority now also explicitly covers the compare-branch
    sibling of that same family: effectful compare/shared-tail micro-shapes
    now have a canonicalized `machine_ir -> machine_select -> machine_layout`
    regression too, including the case where upstream cleanup legally folds
    the shared return tail back into each effectful arm before layout runs
  - current bridge authority should therefore be read in two layers:
    some effectful shared-tail shapes still survive as multi-block local
    layout problems, while some compare/shared-tail shapes are now
    intentionally locked in their more aggressively canonicalized
    post-cleanup form where `machine_layout` receives compare-plus-effectful
    arms with the old shared return already sunk into those arms
  - bridge-side authority now also reaches one step further into
    function-level stitching itself, not only branch-local fallthrough shape:
    there is now a canonicalized effectful-armed regression for the
    "multiple ready shared merges compete" family, so the current
    ready-shared-tail freshness/seed ordering policy is no longer locked only
    on direct `machine_select` input
  - that bridge-side stitching authority now also covers the compare-family
    sibling of the same shape, not only plain branch input: compare-driven
    effectful arms can now preserve the "multiple ready shared merges
    compete" family through canonicalization strongly enough that layout tests
    lock the current compare-side ready-merge freshness behavior too
  - current bridge-side stitching authority therefore now spans both major
    sibling families of the current layout heuristics:
    branch-driven and compare-driven effectful CFGs can both lock
    canonicalized shared-tail bridge behavior, including ready-merge
    freshness/seed ordering rather than only local fallthrough shape
  - compare-family bridge coverage now also reaches the ready-continuation
    side of that same function-level story, not only the ready-shared-merge
    side: both direct and canonicalized bridge regressions now lock compare
    sibling behavior for fresher-ready-continuation preference as well
  - bridge-side authority now also reaches the branch-local scoring side of
    the same authority stack on both major sibling families, not only one:
    canonicalized effectful branch/shared-tail and compare/shared-tail shapes
    now lock that a successor which only runs into a deferred shared tail
    does not win local fallthrough choice merely by claiming that deferred
    tail as if it were ordinary local trace
  - bridge-side seed authority now also reaches the "ready shared merge beats
    longer-but-not-ready trace" family in an effectful canonicalized CFG, so
    the current function-level seed policy is no longer bridge-locked only for
    freshness tie-breaks among already-ready candidates
  - that older seed-priority family now also has an explicit compare-family
    bridge sibling, not only a plain branch/effectful one: canonicalized
    compare-driven bridge coverage now locks the same "ready shared merge
    beats longer-but-not-ready trace" rule while still treating the
    post-cleanup CFG decomposition, rather than the direct `machine_select`
    block identities, as the bridge authority
  - bridge-side authority now also reaches the older branch-local trace-span
    tie-break itself, not only shared-tail defer scoring: canonicalized
    effectful branch-driven and compare-driven CFG families now both lock
    that when the local successor scores tie, layout may still prefer the
    successor whose surviving post-cleanup trace stays longer
  - bridge-side seed authority now also explicitly reaches the seed-class
    boundary between ready continuations and attached-but-unready work:
    canonicalized branch-driven and compare-driven effectful CFG families now
    both lock that a ready continuation may still outrank an older attached
    deferred trace, even when upstream cleanup has already folded some local
    wrapper blocks away and merged surviving effectful tails
  - bridge-side authority now also explicitly reaches the older
    single-predecessor / chain-extending successor preference itself:
    canonicalized branch-driven and compare-driven effectful CFG families now
    both lock that layout may still choose the successor whose surviving
    post-cleanup block continues into a single-predecessor chain, even when
    cleanup folds that chain into fewer concrete blocks than the direct
    `machine_select` shape exposed
  - bridge-side authority now also explicitly reaches the most local branch
    reorder preference itself, not only the richer chain/tie-break families:
    canonicalized branch-driven and compare-driven CFG families now both lock
    that layout may still reorder the immediate branch targets to make the
    preferred successor the direct fallthrough block, even when the bridge
    arrives through compare canonicalization rather than raw branch input

## File Management Rules

- keep all implementation under `src/machine/lowering/machine_layout/`
- keep all public declarations under `include/machine/layout.h`
- keep tests under `tests/machine/lowering/machine_layout/`

## Maintenance Stance

- `ML2` should now be treated as checkpoint-ready and maintenance-first:
  reopen it for confirmed lowering/verifier regressions, newly introduced
  selected-op families, or a concrete downstream consumer pressure.
- `ML3` should now also be treated as checkpoint-ready rather than open-ended:
  the current bridge matrix is already broad enough to lock the surviving
  canonicalized behavior for the main local branch-ordering, local shared-tail
  scoring, and function-level seed/stitching families on both branch-driven
  and compare-driven shapes.
- Further default work on this line should prefer:
  - bug-driven fixes
  - newly discovered bridge canonicalization mismatches
  - a concrete downstream consumer requirement
  over speculative heuristic proliferation.

## Current Stop Condition

- Treat the current machine-layout line as checkpoint-ready once the following
  stay green together:
  - `make test-machine-layout`
  - `make test`
  - `git diff --check`
- Those checkpoint conditions are now currently satisfied for the current
  direct-lowering and canonicalized-bridge layout matrix.
- Do not keep expanding the layout matrix by default once it already covers:
  - direct `machine_select` and canonicalized `machine_ir` bridge authorities
  - branch-driven and compare-driven sibling families
  - local branch-reorder, single-predecessor, and trace-span tie-break choices
  - shared-tail defer / local-trace scoring boundaries
  - function-level seed-class and freshness-order boundaries
- Reopen active expansion only for a concrete missed family that is not yet
  represented, a confirmed correctness bug, or a new consumer need that makes
  the current checkpoint insufficient.
