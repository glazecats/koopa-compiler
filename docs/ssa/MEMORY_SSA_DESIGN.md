# Memory SSA Design Draft

## Goal

This draft captures the next likely structural step after the current `value_ssa` checkpoint:

`AST -> semantic -> canonical IR -> lower IR -> value SSA -> memory SSA -> later backend/codegen`

The emphasis here is on a **small slot-based memory SSA**, not on a fully general pointer-aware memory SSA and not on register allocation.

Current status:

- lower IR is already explicit about `load_*` / `store_*`
- value SSA is already established as the current downstream optimization layer
- current `value_ssa` work has also accumulated enough shared CFG/dataflow infrastructure that a separate memory-state layer is now realistic
- this file is a planning/design document only; it is not itself an implementation checkpoint

## Why Now

The current IR stack has a particularly favorable property:

- there are explicit storage slots
- there are no pointers yet
- there is no address-taking
- there is no alias analysis burden yet

That means the first version of memory SSA does **not** need to answer general questions like:

- does `*p = ...` alias `x`
- does this call clobber an arbitrary indirect memory region
- which abstract memory object does this load read from

Instead, the first version can be much smaller:

- `store_local a.0, ...` updates only `a.0`
- `load_local a.0` reads only `a.0`
- `store_global g.0, ...` updates only `g.0`
- `load_global g.0` reads only `g.0`

That is a very good time to land the **shape** of memory SSA before pointer-related complexity arrives later.

## Recommendation

The next layer should be:

- downstream from `value_ssa`
- explicit about memory versions for tracked slots
- narrow and conservative
- separate from the current value-SSA representation module

In short:

- let `value_ssa` continue to model value flow
- introduce a sibling `memory_ssa` layer to model slot-state flow
- do not jump directly from the current state into allocator/backend work

## First-Version Boundary

The first version should be **slot-based memory SSA** only.

Tracked memory objects:

- local slots
- global slots

Tracked operations:

- `load_local`
- `store_local`
- `load_global`
- `store_global`
- `call` as a conservative memory barrier / kill point where needed

Not in scope for the first version:

- pointers
- indirect loads/stores
- alias analysis
- heap objects
- address-taken locals/globals
- physical registers
- spilling
- machine IR

## Input Contract

The first memory-SSA builder should accept:

- verifier-legal `value_ssa`

Not:

- raw canonical IR
- raw lower IR

Rationale:

- `value_ssa` is already the current downstream optimization authority
- the existing shared CFG/dominance/dataflow helpers already live there
- memory SSA should build on the current value-facing layer instead of bypassing it

## Core Model

The simplest useful model is:

- one memory-version namespace per function
- one tracked memory object per slot
- memory phi nodes at block entries when multiple predecessor memory versions can reach the block
- explicit memory uses/defs attached to the existing slot operations

Conceptually:

- `store_*` produces a new memory version for one tracked slot
- `load_*` consumes the current memory version for that tracked slot
- a join/header may need a memory phi for that tracked slot

This is intentionally not a full "one giant memory token for the entire world" design.
Because slots are explicit and non-aliasing today, the first version should keep that precision.

## Slot Identity

The first version should treat slot identity as already solved by the existing IR:

- local slots are identified by local id
- global slots are identified by global id

That means the tracked memory-object layer can stay explicit and stable:

- `local a.0` and `local b.1` are distinct memory objects
- `global g.0` and `global h.1` are distinct memory objects

No new alias partitioning logic is needed yet.

## Call Modeling

Calls should start conservative.

Recommended first-version rule:

- calls kill all known global-slot versions
- local-slot versions are preserved unless the IR/language contract later introduces by-reference local access

This is intentionally simple and deliberately conservative.

It matches the current world:

- globals are the plausible ambient shared state
- locals are explicit function-owned slots, not arbitrary pointer-reachable memory

If later language/runtime features change that assumption, the call model can widen.

## Suggested Folder Shape

The smallest clean layout is:

```text
include/memory_ssa.h
src/memory_ssa/memory_ssa.c
src/memory_ssa/memory_ssa_verify.inc
src/memory_ssa/memory_ssa_dump.inc
src/memory_ssa/memory_ssa_analysis.inc
src/memory_ssa/memory_ssa_from_value_ssa.inc
tests/memory_ssa/memory_ssa_regression_test.c
tests/memory_ssa/memory_ssa_verifier_test.c
```

This should remain a sibling module rather than being folded into:

- `src/value_ssa/`
- `src/value_ssa_pass/`
- `src/value_ssa_interp/`

Rationale:

- the layer is adjacent to value SSA, but not the same thing
- mixing them now would create another long-lived representation/pass grab-bag

## Builder Direction

The recommended first builder is:

- `value_ssa -> memory_ssa`

The builder should:

1. collect tracked slot objects
2. compute definition blocks per tracked slot
3. place memory phis per slot at joins/headers
4. walk the dominator tree
5. maintain current per-slot memory-version bindings
6. rewrite each memory use/def site to reference those versions

This should mirror the same overall mental model already used for SSA construction:

- placement
- current binding
- dominator traversal
- successor-edge phi input fill

## Verifier Boundary

The first verifier should check at least:

- dense memory-version ids
- well-formed tracked-slot tables
- each non-entry memory phi has valid predecessor coverage
- each memory use refers to an available version
- each memory def/version is defined exactly once
- CFG reachability and dense block ids remain intact

The verifier should also stay crash-proof on malformed metadata, matching the current project style.

## Dump Boundary

The first dump should prioritize readability over theoretical completeness.

Recommended style:

- keep slots readable as `a.0`, `g.0`
- print memory versions in a separate namespace such as `mem.0`, `mem.1`, ...
- make memory phi nodes visually distinct from value phi nodes

Example shape:

```text
bb.3:
  mem.4 = phi a.0 [bb.1: mem.2], [bb.2: mem.3]
  ssa.7 = load_local a.0 @ mem.4
  ret ssa.7
```

The exact concrete syntax can change, but the namespace split should stay clear.

## Representative First-Version Examples

### Straight Local Chain

```text
bb.0:
  store_local a.0, 1
  store_local a.0, 2
  ssa.0 = load_local a.0
  ret ssa.0
```

The memory-SSA view should show:

- two successive defs for `a.0`
- the load reading the second one

### Local Diamond Join

```text
bb.0:
  br cond, bb.1, bb.2
bb.1:
  store_local a.0, 1
  jmp bb.3
bb.2:
  store_local a.0, 2
  jmp bb.3
bb.3:
  ssa.0 = load_local a.0
  ret ssa.0
```

The join should need a memory phi for `a.0`.

### Loop-Carried Local

```text
bb.0:
  store_local a.0, 0
  jmp bb.1
bb.1:
  br cond, bb.2, bb.3
bb.2:
  store_local a.0, 1
  jmp bb.1
bb.3:
  ssa.0 = load_local a.0
  ret ssa.0
```

The loop header should need a memory phi for `a.0`.

### Global + Call Barrier

```text
bb.0:
  store_global g.0, 1
  call touch()
  ssa.0 = load_global g.0
  ret ssa.0
```

The first version should treat the call conservatively enough that the post-call load does not pretend to read the earlier known global version.

## First Implementation Slice

The first implementation should be split into three small stages.

### M0: Skeleton

Land:

- public header
- core init/free
- verifier
- dump
- manual-construction helpers if needed
- isolated tests

This stage is only about representation/debuggability.

### M1: Real Construction

Land:

- tracked-slot discovery
- per-slot definition-block collection
- memory-phi placement
- dominator-tree construction walk
- per-slot current-version binding
- successor-edge memory-phi input fill

Coverage target:

- straight local/global chains
- diamond joins
- simple loop-carried slots

### M2: Conservative Effects + Mixed Shapes

Land:

- `call` handling for globals
- mixed local/global examples
- stronger regression matrix
- one-shot builder convenience entrypoint if needed

This should still stay well short of pointer-aware memory SSA.

## Relationship To Future Pointer Support

The first slot-based memory-SSA implementation is not expected to become the final universal memory-SSA model.

Later pointer support will likely require:

- broader memory-object partitioning
- alias-aware clobber rules
- indirect memory uses/defs
- stronger call-mod/ref modeling

That means some future restructuring is expected.

However, the first version should still be valuable because it lands the hard conceptual parts early:

- memory phi shape
- versioning discipline
- rename/binding flow
- verifier/dump conventions
- pass-consumer expectations for memory facts

So later pointer support should be treated as:

- an extension and widening of the memory-object model

not:

- proof that the first slot-based memory-SSA step was wasted

## Non-Goals

The first memory-SSA slice should not try to do any of the following:

- pointer-aware memory SSA
- alias analysis
- escape analysis
- register allocation
- full coalescing
- spill heuristics
- machine lowering
- replacing value SSA as the current optimization authority

## Recommendation Summary

If implementation begins now, the recommended path is:

1. create a separate `memory_ssa` sibling module
2. build it on top of verifier-legal `value_ssa`
3. track only explicit local/global slots
4. place per-slot memory phis
5. treat calls conservatively, especially for globals
6. regression-lock straight, diamond, loop, and call-barrier shapes

This is the smallest version that is:

- structurally real
- useful later
- well-matched to the current no-pointer IR world
- unlikely to drag the project prematurely into allocator/backend concerns
