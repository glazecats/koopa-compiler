# Value SSA Design Draft

## Goal

This draft captures the current downstream direction after the lower-IR checkpoint:

`AST -> semantic -> canonical IR -> lower IR -> value SSA -> later backend/codegen`

The emphasis is on **value SSA**, not memory SSA and not machine/register IR.

Current status:

- phase S0 design boundary is frozen
- phase S1 skeleton is landed
- phase S2 has started with a very small conversion slice from lower IR
- a separate narrow trivial-values simplify pass is also landed for local use-site cleanup after construction/conversion
- a separate narrow CFG simplify pass is also landed for trivial branch folding plus dead-block/value-id compaction after CFG rewrites
- a separate narrow dead-def cleanup pass is also landed for removing unused pure value definitions after the earlier cleanup stages
- a first shared CFG analysis/helper layer is also landed for predecessor/reachability/dominator-tree/frontier facts, iterated phi-placement closure, dominator-tree preorder, and block-level liveness that later rename-oriented work and dataflow-driven passes can consume directly
- that shared analysis layer now also has a first real consumer: a conservative interference-graph builder can consume Value-SSA liveness directly, which is useful for future live-range / backend preparation without committing this stage to register allocation yet
- that interference layer now also has a first small consumer above itself: a copy-affinity graph can record non-interfering `mov` relationships as coalescing/allocator-prep hints without starting actual register allocation
- those allocation-oriented facts can now also be bundled into one summary surface, so future backend-prep/live-range experiments can consume per-value def/use, live-block coverage, degree/affinity, and candidate data directly instead of restitching the underlying analyses by hand
- that summary can now also be organized into a stable allocation worklist, so future live-range/coalescing experiments can start from shared value classes plus a deterministic priority order instead of inventing a new queue shape each time
- that allocator-prep layer is now also easy to inspect directly through dump helpers for the summary and worklist, which makes this stage more debuggable without promoting it into a real allocator yet
- a few narrow query helpers on top of that debug surface are also reasonable at this stage, because they let later allocator-prep experiments ask focused questions without turning every caller into a hand-written array decoder
- a separate dominator-tree walk scaffold is also landed for rename-oriented enter/leave traversal, so later stack-based renaming can stay split from raw tree-walk plumbing
- a first scoped rename-state stack is also landed, so later rename work can thread current bindings through dominator-tree enter/leave without reimplementing shadowing and rollback
- a first block-local use-site rewrite helper is also landed, so later rename work can separate “what is currently bound” from “rewrite this block’s uses under those bindings”
- a first predecessor-specific phi-input rewrite helper is also landed, so later rename work can update successor-edge phi operands independently from block-local instruction/terminator rewriting
- a first real function-level alpha-renaming orchestration is also landed for existing SSA functions, composing dominator walk, scoped bindings, local use rewriting, local def renaming, and successor-edge phi-input rewriting into one verifier-safe pass
- a first program-level canonicalization entrypoint is also landed, composing trivial SSA cleanup plus alpha-renaming into one small “stabilize the result layer” pipeline
- a first direct execution-oriented sibling module is also landed for SSA-level testing/oracle use, without changing the representation or optimization boundaries
- strict dominator-tree construction from lower IR is now landed and should be treated as the stable construction baseline for the next stage

## Immediate Next Step

Now that strict construction is landed, the next implementation target should move above conversion:

- shared SSA def/use facts
- explicit use-list facts
- the first truly SSA-native cleanup/optimization passes that consume those facts
- the next passes should stay narrow and value-only, but it is now reasonable to grow beyond pure immediate/immediate folding into safe algebraic identities that still do not require memory SSA

This next slice should stay value-only:

- no memory SSA yet
- no backend/register work yet
- no reopening conversion unless a new correctness bug appears

The first concrete shared-analysis target above conversion should be:

- per-value def-site facts
- explicit use-site lists

That surface is a better foundation for early SSA-native passes than continuing to hide use-count or use-scan logic inside individual transforms.

The first concrete consumer should be small and mechanical rather than ambitious. The current direction is:

- move existing trivial SSA cleanup to the shared def/use surface first
- move existing SSA dead-def cleanup onto the same surface as well
- then add new SSA-native passes on top of the same substrate

The first new pass above that substrate should stay deliberately narrow. The current implementation direction is:

- immediate/immediate constant folding for safe value operations
- safe algebraic identity reduction for value-only binaries (`x+0`, `x*1`, `x&-1`, `x/1`, `x%1`, reflexive compares, ...)
- narrow operand normalization for value-only binaries so later cleanup sees a stable shape (`1 < x` -> `x > 1`, commutative immediates on the RHS, ...)
- narrow dominator-aware redundant-binary cleanup for safe value-only binaries, so repeated dominated recomputation can collapse before later trivial-value cleanup and DCE
- narrow local-slot load forwarding when a same-block `store_local` or earlier same-block `load_local` already determines the slot value
- narrow global-slot load forwarding when an earlier straight-chain `store_global` or earlier forwarded `load_global` already determines the slot value, with `call` treated as a conservative global kill and joins still treated as stop points
- narrow redundant-store cleanup when a same-block or straight-chain known slot value already equals the value being written, with joins still treated as stop points and `call` still killing known globals
- narrow dead store elimination: `store_local` / `store_global` can be dropped when later overwritten before any intervening read/observation in the same block, and that overwritten-store cleanup may also cross a straight `jmp` chain with a unique idom successor; joins, branch exits, and `call` should still stop the analysis
- local-slot forwarding can now also cross straight dominator-chain block boundaries when there is a single idom predecessor, but it still intentionally stops at join points
- narrow CFG cleanup after value cleanup/DCE has exposed empty jump chains, jumps to empty returns, same-return diamonds, or a non-phi successor block with exactly one predecessor that can be merged into its jumping predecessor
- no memory-sensitive folding yet
- no dangerous `div`/`mod`/shift folding yet

## Why This Is Next

The current lower IR now has the boundary we wanted:

- value refs are temps/immediates only
- locals/globals are explicit slots behind `load_*` / `store_*`
- CFG shape is stable and verifier-backed
- representative lowering coverage is broad enough for a checkpoint

That means the next worthwhile structural step is no longer "more lower-IR verifier hardening by default".
It is to decide whether value flow should now be made explicit through SSA.

## Recommendation

The next layer should be:

- built **on top of lower IR**
- restricted to **temps / computed values**
- deliberately **not** responsible for direct local/global storage semantics

In other words:

- `load_*` stays a value-producing instruction
- `store_*` stays an effect instruction
- SSA names track value flow only
- memory state remains explicit and outside SSA renaming

## Non-Goals

The first SSA slice should not try to do any of the following:

- memory SSA
- alias analysis
- register allocation
- physical register modeling
- stack-slot allocation
- target calling convention work
- broad optimization passes
- replacing lower IR as the current dump/verifier authority

## Proposed Input Contract

The SSA builder should accept:

- verifier-legal lower IR
- not just one specially pre-cleaned lower-IR shape

That keeps the layer boundary clean in the same way lower IR accepts any verifier-legal canonical IR.

## Proposed Scope Boundary

### Lower IR remains responsible for

- storage slots (`local/global`)
- explicit `load_*` / `store_*`
- startup/helper lowering
- current dump/verifier authority

### Value SSA becomes responsible for

- SSA names for temp-producing instructions
- phi insertion at value joins
- use rewriting from temp ids to SSA values
- dominance-based value availability

### Execution Support Can Live Beside Value SSA

Execution support for this layer should stay a sibling module, not be folded into optimization code:

- keep direct SSA interpretation under `src/value_ssa_interp/`
- keep optimizer code under `src/value_ssa_pass/`
- keep representation / verifier / analysis under `src/value_ssa/`

The first interpreter slice should stay narrow:

- execute verifier-legal `value_ssa` directly
- support `mov`, `binary`, `load_*`, `store_*`, `jmp`, `br`, `ret`
- support internal calls plus optional external-call callbacks for declaration-only callees
- reject unsupported/uninitialized runtime states instead of inventing new IR semantics

## First-Version SSA Model

The simplest useful first version is:

- one SSA value namespace per function
- phi nodes at block entries
- non-phi instructions remain close to lower-IR instruction kinds
- block/terminator CFG shape stays the same

That means the first SSA layer should feel like:

- lower IR CFG
- plus SSA value naming
- plus explicit phi nodes

Not:

- a totally different control-flow representation
- a machine IR
- a pass framework on day one

## Naming And Dump Boundary

SSA value identity should stay in a compiler-internal namespace and must not reuse user/source names as semantic identifiers.

Recommended dump convention:

- keep lower-IR slots/source-near entities readable as `a.0`, `x.1`, `g.0`
- print SSA values with a clearly separate internal prefix such as `ssa.0`, `ssa.1`, ...

This avoids visual confusion between:

- user/source-facing slot names
- internal SSA value ids

The exact printable prefix is not semantically important, but the namespace split is.

## What Produces SSA Values

The initial rule should be:

- `mov` produces an SSA value
- `binary` produces an SSA value
- `call` produces an SSA value when it has a result
- `load_local` produces an SSA value
- `load_global` produces an SSA value
- phi produces an SSA value

The following do **not** produce SSA values:

- `store_local`
- `store_global`
- `jmp`
- `br`
- `ret`

## What Needs Phi Nodes

Phi nodes should only be needed for values that can arrive from multiple predecessors.

Typical first-version examples:

```text
bb.0:
  br cond, bb.1, bb.2
bb.1:
  tmp.0 = mov 1
  jmp bb.3
bb.2:
  tmp.0 = mov 2
  jmp bb.3
bb.3:
  ret tmp.0
```

becomes conceptually:

```text
bb.0:
  br v0, bb.1, bb.2
bb.1:
  v1 = mov 1
  jmp bb.3
bb.2:
  v2 = mov 2
  jmp bb.3
bb.3:
  v3 = phi [bb.1: v1], [bb.2: v2]
  ret v3
```

## What Does Not Need Memory SSA Yet

This is important:

- if two predecessors both `store_local x.0, ...`, first-version value SSA does **not** try to model the store itself as SSA
- instead, later `load_local x.0` still remains an explicit operation in the SSA layer

That keeps the first version small and avoids pretending we already solved memory versioning.

## Relationship To Existing Join-Temp Lowering

Today lower IR sometimes materializes the same logical join through verifier-legal multi-def temps on mutually exclusive predecessors.

Value SSA should eventually become the cleaner authority for those shapes:

- current lower IR keeps them legal
- SSA conversion should normalize them into explicit phi-based value joins

That means SSA is not being introduced to replace a bug.
It is being introduced to replace a currently tolerated but less explicit value-join shape.

## Suggested Representation Choices

### Option A: SSA as a new IR layer

Pros:

- cleanest abstraction boundary
- easiest to reason about later backend/optimization consumers
- avoids mutating lower IR into a dual-mode representation

Cons:

- one more data model to define

### Option B: SSA as annotations over lower IR

Pros:

- smaller initial implementation

Cons:

- easier to blur authority boundaries
- harder to keep dump/verifier stories clean

### Recommendation

Prefer **Option A**:

- create a distinct value-SSA layer
- keep it structurally close to lower IR
- avoid mutating lower IR into a mixed “sometimes SSA, sometimes not” representation

## Proposed Folder Shape

If this work starts, the smallest clean layout is:

```text
include/value_ssa.h
include/value_ssa_pass.h
src/value_ssa/value_ssa.c
src/value_ssa/value_ssa_dump.inc
src/value_ssa/value_ssa_verify.inc
src/value_ssa/value_ssa_from_lower_ir.inc
src/value_ssa_pass/value_ssa_pass.c
src/value_ssa_pass/
tests/value_ssa/value_ssa_regression_test.c
tests/value_ssa/value_ssa_verifier_test.c
```

Current bridge status:
- `value_ssa_build_from_lower_ir(...)` remains the raw conversion entrypoint.
- `value_ssa_build_from_lower_ir_with_canonicalization(...)` is now the preferred policy-selecting high-level bridge above raw conversion; callers that want to choose among classic, memory-value, or full memory-aware canonicalization should prefer that mode-based entrypoint over hand-composing conversion plus cleanup.
- `value_ssa_build_default_from_lower_ir(...)` is now the preferred ordinary-caller helper when no explicit policy choice is needed; it currently selects the repository default lower-IR -> Value-SSA canonicalization policy.
- The older named wrappers remain valid explicit shorthands for fixed call sites. In particular, `value_ssa_build_canonicalized_from_lower_ir(...)` still names the classic result-layer bridge and still acts as a stabilized-output boundary: it runs an extra canonicalization convergence step internally, so representative classic-bridge authority shapes should already be fixed-point if a caller chooses to canonicalize them once more.
- The current result-layer canonicalization pipeline is expected to be fixed-point on representative authority shapes; tests lock this on the scrambled-diamond canonicalization case.
- The canonicalization entrypoint is also expected to be self-verifying: malformed SSA should be rejected on entry, and successful canonicalization should return verifier-legal SSA.
- Current result-layer cleanup policy also treats effectful instructions as non-removable by shared classification, so canonicalization may erase dead pure value defs but must preserve dead-result `call` and all `store_*` instructions.
- The same preservation policy also applies to the one-shot lower-IR bridges: conversion followed by any supported canonicalization policy must preserve dead-result `call` instructions rather than treating them as removable cleanup noise.
- Under the current safety policy, dangerous binary ops also stay on the non-removable side of that boundary: dead-result `div`/`mod`/shift instructions must survive result-layer cleanup and bridge-level canonicalization just like other effectful/trapping operations.
- With the recent reviewer-driven safety/contract fixes in place, the current result-layer hardening slice is considered clear enough for continued implementation; further result-layer-only review is now need-driven rather than a standing stop gate.
- The conversion mainline has now also begun shifting onto lower-IR-side facts: current conversion still preserves the same representative SSA outputs, but predecessor and phi-candidate decisions are now sourced from the dedicated lower-IR CFG/phi-placement analysis rather than only from SSA-side reconstruction helpers.
- The same bridge now also consumes lower-IR's batch temp-phi-candidate surface directly. `value_ssa` should treat `lower_ir_compute_temp_phi_candidates(...)` as the input-layer authority for per-temp phi-candidate matrices rather than re-deriving that matrix inside SSA-local conversion code.
- The same bridge now also consumes lower-IR's compact predecessor-list surface directly while computing entry maps and finalizing phi inputs. This is still a transitional step, not the end state of conversion, but it removes another SSA-local re-scan of CFG structure and keeps more traversal facts anchored in the input-layer analysis.
- Lower-IR input analysis now also carries a compact successor-list view. Current conversion does not consume that surface yet, but it is now part of the formal input-layer CFG facts and is available for later cleanup that removes more ad hoc successor decoding from the bridge.
- Current bridge entry-map construction now also leans on lower-IR immediate dominators more directly: block-entry state for non-phi temps is inherited from the idom first, and predecessor-state merging is retained only for actual phi-candidate temps. This is still not the final rename-driven design, but it is a real reduction in how much of the old predecessor-ready machinery every temp has to pass through.
- The bridge now also consumes a block-local phi-temp view from lower-IR analysis rather than rescanning all temp ids at each phi-capable block. This is still a transitional step, but it narrows phi-related iteration to the actual candidate set and moves another slice of phi bookkeeping into the input-layer analysis surface.
- The bridge now also stores pending-phi creation state with one fewer auxiliary structure: `pending_phi_ids == SIZE_MAX` is the only "phi not created yet" sentinel, so the old `pending_phi_present` bitmatrix is gone. This does not solve the bigger predecessor-state dependency by itself, but it is another concrete shrink in the old bridge-local state.
- The bridge no longer carries the old `out_valid/current_valid` bitsets. Temp availability is now represented by current rename bindings plus `SIZE_MAX` sentinels inside the remaining predecessor-facing snapshot surfaces.
- The bridge also no longer keeps the flat `phi_candidate_blocks` matrix alive through conversion. Lower IR still computes that matrix as an intermediate fact, but the bridge now retains only the compact block-local phi-temp lists during the actual dominator walk and phi finalization.
- Lower IR now also exposes a direct `lower_ir_compute_temp_phi_candidate_lists(...)` helper, and the bridge consumes that API directly instead of first materializing a raw phi-candidate bitmatrix inside value-SSA conversion. This keeps the "which temps are phi candidates in this block" fact on the input-analysis side and removes one more matrix-shaped temporary from the bridge.
- The bridge state itself now carries one `const LowerIrCfgAnalysis *` instead of separate copied pointers for immediate dominators and predecessor lists. That does not finish the transition away from predecessor-state conversion, but it keeps the remaining bridge-local state focused on genuine transition machinery instead of duplicated analysis facts.
- The bridge now also derives block-completion state directly from the partially built SSA function instead of carrying a separate `processed_blocks` bitset. That still is not the final rename-driven construction, but it removes one more explicit predecessor-readiness lattice from the transition-state machinery.
- Lower IR now also exposes the stricter input facts needed for pure SSA construction: temp live-in blocks and pruned block-local phi-candidate lists. Downstream SSA construction should prefer those pruned phi lists over the older conservative candidate matrix whenever it wants strict dominator-tree generation instead of a backfill-oriented bridge.
- Precision note: the persistent conversion state now only keeps the compact pruned phi list, but the current setup path still uses a temporary `raw_phi_candidate_temps` scratch buffer before compacting into that list. So the long-lived bridge state is already strict/compact even though setup scratch is not yet fully compacted away.
- The conversion mainline has now been switched to that strict construction style. It precreates all pruned candidate phis before traversal, binds phi results on block entry, rewrites all lower-ir temp uses/defs through the dominator-scope rename state during DFS, and fills successor phi incoming edges directly on predecessor leave.
- The strict conversion path no longer uses deferred successor snapshots or entry-time backfill to build the normal SSA graph. Its finalize phase is only a completeness check, and it runs a verifier-safe alpha-renaming pass afterward so emitted SSA ids remain dense and stable in dominator order.
- Representative loop coverage is no longer limited to a single-backedge carried-temp example: the current bridge also covers a multi-backedge loop-header phi shape where one carried temp can re-enter the header from multiple dominated backedge blocks.
- Representative loop coverage now also includes a loop header carrying multiple independent temps at once: the current bridge can materialize parallel header phis for those temps while keeping the same raw-bridge output style.
- The matching lower-IR input-layer authority now also covers that family at the phi-candidate stage, so the bridge's parallel-header-phi behavior is no longer ahead of the formal lower-IR analysis contract.
- Representative loop coverage now also includes a nested-loop carried-temp family. Current conversion may preserve an outer carried temp through an inner loop header without forcing a bogus extra phi there, as long as lower-IR input analysis marks that header as non-phi-candidate for the inherited temp and the already-processed predecessors agree on the same carried value.
- Current bridge behavior is also now explicitly insensitive to supported-family sibling block ordering: a join/header may be visited before some predecessors in dominator-tree child order, and conversion may still pre-create the needed pending phi as long as no already-processed predecessor has proven the temp invalid on that edge. The scrambled-diamond authority case now locks this.
- The matching lower-IR input-layer authority now also covers that family: nested-loop temp-phi candidates are part of the contract, including the current conservative behavior where an inner-loop-carried temp may still mark the outer header through iterated frontier closure even though an outer carried temp does not require a phi at the inner header.
- The same alignment now also holds for the "same temp carried through both outer and inner loops" family: lower-IR temp-phi candidates expose both loop headers, and current conversion materializes the corresponding nested phis without needing a special-case bridge-only rule.
- The same alignment now also holds for a stronger nested same-temp family where the inner loop itself has multiple backedges. Current lower-IR temp-phi candidates still expose the same outer/inner header pair, and current conversion still materializes the corresponding nested phis without a bridge-only escape hatch.
```

This mirrors the current lower-IR module shape on purpose.

## First Implementation Slice

The first slice should be intentionally tiny.

### Phase S0: Design Freeze

Freeze:

- value SSA is downstream from lower IR
- value SSA does not attempt memory SSA
- phi nodes are first-class
- CFG shape is inherited from lower IR

### Phase S1: Skeleton

Deliver:

- data model
- init/free helpers
- dump
- verifier
- manual-construction tests

Recommended first deliverables inside the skeleton:

- block-level phi representation
- internal SSA value ids
- dump naming that stays visually distinct from lower-IR slot names

Status:

- landed

Still intentionally out of scope for S1:

- lower-IR-to-SSA conversion
- SSA-driven optimization
- memory SSA

### Phase S2: Minimal Conversion

Deliver a conversion path for a narrow family:

- straight-line blocks
- single-join diamond with value phi
- simple loop header phi when needed

No broad optimization yet.

Current status:

- partially landed
- current implementation supports:
  - straight-line lower-IR bodies
  - single-join diamond shapes that require explicit phi nodes
  - simple slot-carried loops where carried state stays in explicit `load_* / store_*`
  - a first representative loop-carried temp join through loop-header phi materialization
- current implementation also has a separate trivial-values simplify pass that can rewrite use sites through trivial `mov` chains and same-value `phi` nodes
- current implementation also has a separate narrow CFG simplify pass that can fold same-target branches and immediate-condition branches, then compact newly unreachable blocks and surviving SSA value ids to preserve the current verifier contract
- current implementation also has a separate narrow dead-def cleanup pass that can remove unused pure value definitions (`phi`, `mov`, `binary`, `load_*`) while preserving effectful instructions such as `call` and `store_*`, followed by SSA value-id compaction
- current implementation also has a first shared CFG analysis/helper layer that computes predecessor counts, reachability, the full dominator relation, immediate dominators, explicit dominator-tree children, and dominance frontiers for verifier-legal SSA functions, plus iterated phi-placement closure from a definition-block set and direct dominator-tree preorder traversal
- current implementation still keeps broader cyclic CFG families intentionally out of scope for now, so more general loop handling remains a follow-up slice

### Phase S3: Authority Checkpoint

Only after conversion is stable should the project decide whether:

- SSA-backed optimization is worth starting
- lower IR should remain the main pre-backend authority
- another backend-facing layer is needed after SSA

## Open Questions

These are the places where user discussion is still useful before phase-S2 conversion starts:

1. Should phi nodes be modeled as block-entry instructions, or as a dedicated block-level phi list?

Recommendation:

- use a dedicated phi list on each block

Why:

- cleaner SSA verifier rules
- avoids normal-instruction ordering weirdness

2. Should first-version SSA keep `load_*` as ordinary value-producing instructions, or try to special-case them immediately?

Recommendation:

- keep `load_*` as ordinary value-producing instructions

Why:

- smallest implementation
- no fake memory SSA pressure yet

3. Should the first SSA converter support loops immediately, or only straight-line + diamonds?

Recommendation:

- support diamonds first, loops second in the same design but not necessarily the same patch

Why:

- loop phi placement is the first real complexity jump

4. Should lower IR remain the default dump/debug authority while SSA is immature?

Recommendation:

- yes

Why:

- lower IR is already checkpointed and regression-backed
- SSA should earn authority gradually

## Current Recommendation

The best next move is:

1. keep lower IR checkpointed
2. keep the landed value-SSA skeleton stable
3. implement a very small phase-S2 conversion slice from lower IR
4. only then decide how much broader SSA authority is justified
