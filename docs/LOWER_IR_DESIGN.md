# Lower IR Design Draft

## Goal

This draft captures the next likely IR direction after the current canonical-IR pass layer.

The current recommendation is:

- keep canonical IR stable as the front-end output surface
- do not start with register allocation or machine-like IR
- do not force SSA directly onto the current canonical IR
- first make memory/value flow more explicit

In short:

`AST -> semantic -> canonical IR -> lower-memory IR -> SSA (later, if needed) -> machine/codegen`

## Current Position

Canonical IR is now in a good maintenance state:

- verifier contracts are broad and stable
- the first pass pipeline is landed and regression-hardened
- F0-F3 are effectively soft-closed unless bug-driven work reopens them

That means the next step should not be "more local cleanup passes by default".
It should be a small design and implementation step that clarifies whether the compiler now needs a more explicit lower stage.

## Recommendation

The next design target should be a small lower IR with explicit memory reads/writes.

Not:

- physical registers
- spilling
- calling convention details
- machine scheduling

Also not yet:

- full SSA construction

The immediate purpose of the next layer is simpler:

- separate "storage location" from "computed value"
- make reads and writes explicit
- make later SSA/codegen easier

## Why Not Start With Registers

Register-aware IR pulls in backend concerns too early:

- physical register limits
- spills and stack slots
- caller/callee-save policy
- target calling convention
- interference / allocation pressure

Those are real backend problems, but they are not the compiler's current bottleneck.

## Why Not Start With SSA Directly

SSA works best when value flow is already clean.

If locals/globals are still used as if they were ordinary values, SSA construction becomes awkward because:

- memory-like state is mixed with register-like state
- reads and writes are not explicit
- later alias/memory policy becomes harder to explain

So the better order is:

1. make memory operations explicit
2. then consider SSA for temp/value flow

## Proposed Boundary

Canonical IR should remain:

- source-near
- variable-aware
- friendly for verifier-backed front-end lowering

The lower IR should become:

- value-oriented
- explicit about reads/writes
- easier to lower further toward SSA or codegen

The main semantic shift is:

- canonical IR can still talk in terms of locals/globals as abstract program entities
- lower IR should talk in terms of explicit loads/stores of those entities

## Phase 0 Freeze

The first-version lower IR is now frozen around these decisions:

- lower IR is a serial downstream stage from canonical IR, not a mutation of canonical IR in place
- lower IR keeps the existing function/basic-block/terminator CFG shape
- first version uses slot-specific memory operations, not generic address-taking
- value instructions consume only temps/immediates
- locals/globals remain storage slots and are not legal value operands
- `call` arguments and results follow the same temp/immediate-only value boundary
- SSA and register allocation remain out of scope for phase 1

This means the lower-IR contract is not "canonical IR with a few extra opcodes".
It is a different representation boundary: values and slots are split on purpose.

## Smallest Useful Design

The smallest useful lower-memory design is not a full address machine.

Phase 1 should prefer explicit slot operations:

- `load_local dst, local_id`
- `store_local local_id, value`
- `load_global dst, global_id`
- `store_global global_id, value`

Keep existing temp/value-style computation:

- `mov`
- `binary`
- `call`
- `ret`
- `jmp`
- `br`

This is intentionally smaller than a full address-based model with:

- `addr_local`
- `addr_global`
- generic `load addr`
- generic `store addr, value`

That more generic address model may come later, but it is not the best first step unless pointer-like behavior becomes a real requirement.

## First-Version Value And Slot Model

The first-version lower IR should make the split visible in both verifier rules and data model:

- value references: temps and immediates only
- slot references: locals and globals only

That means lower IR should not let one generic operand kind cover both value flow and storage slots.
The type boundary is part of the design.

## Suggested Lower IR Invariants

In the first lower-memory version:

- arithmetic/compare instructions operate on temps and immediates only
- branch conditions are temps/immediates only
- returns are temps/immediates only
- call arguments are temps/immediates only
- call results, when present, are temps only
- loads always produce temps
- stores never produce result temps
- loads/stores are the only instructions that directly mention locals/globals

This creates a cleaner split:

- storage state lives behind load/store
- computation lives on temps

## Example

Canonical-style shape:

```text
tmp.0 = add a.0, 1
ret tmp.0
```

Lower-memory shape:

```text
tmp.0 = load_local a.0
tmp.1 = add tmp.0, 1
ret tmp.1
```

Assignment:

```text
a = b + 1;
```

Lower-memory shape:

```text
tmp.0 = load_local b.1
tmp.1 = add tmp.0, 1
store_local a.0, tmp.1
```

Global write:

```text
g = a;
```

Lower-memory shape:

```text
tmp.0 = load_local a.0
store_global g.0, tmp.0
```

Call argument preparation:

```text
tmp.0 = load_local a.0
tmp.1 = add tmp.0, 1
tmp.2 = call f(tmp.1, 7)
ret tmp.2
```

## Verifier Impact

If this lower layer is introduced, verifier rules should cover:

- `load_*` slot references must point at valid locals/globals
- `store_*` slot references must point at valid locals/globals
- `load_*` results must be temps
- `store_*` instructions must not produce result temps
- `mov`/`binary`/`call` value operands must be temps/immediates only
- `ret` / `br` must consume legal lower-IR values only
- CFG structure remains block-based with required terminators and valid targets

The verifier should enforce this as a distinct contract for the lower layer, not as an informal lowering convention.

## Lowering Scope

The first lowering step should stay intentionally narrow:

- local reads become `load_local`
- local writes become `store_local`
- global reads become `load_global`
- global writes become `store_global`
- arithmetic keeps using temp/immediate operands
- existing CFG/terminator structure stays unchanged in shape

That keeps the control-flow model stable while only changing the value/storage model.

## Relationship To Canonical IR Passes

Lower IR should accept any verifier-legal canonical IR.

That means:

- lower IR must not require the canonical default pass pipeline as a semantic precondition
- it should be legal to lower directly from unoptimized canonical IR
- it should also be legal to lower after canonical IR passes have simplified the same program

Canonical `ir_pass` output may become the preferred input later for cleaner shapes, but it must not be the only accepted input contract.

## Why This Comes Before SSA

Once load/store boundaries exist, SSA becomes much cleaner:

- temps represent value flow
- memory state stays explicit
- phi placement decisions are about values, not ambiguous variable/storage uses

So the likely order is:

1. explicit memory lower layer
2. SSA on temp/value flow if later optimization pressure justifies it

## Why This Comes Before Register Allocation

Register allocation belongs closer to codegen and machine constraints.

The project is not there yet.

If a lower-memory IR is added first, later backend work will have a much more regular input surface.

## Non-Goals For The First Slice

- physical register modeling
- spills / stack slot allocation
- target calling convention encoding
- memory SSA
- alias analysis
- aggressive optimization
- replacing canonical IR as the front-end authority

## Proposed Folder Layout

If this work starts, the smallest clean repository shape is:

```text
include/lower_ir.h
src/lower_ir/lower_ir.c
src/lower_ir/lower_ir_verify.inc
src/lower_ir/lower_ir_dump.inc
tests/lower_ir/lower_ir_regression_test.c
tests/lower_ir/lower_ir_verifier_test.c
```

Later, phase 2 should add:

```text
src/lower_ir/lower_from_ir.inc
```

The intent is:

- `src/ir/` remains canonical IR
- `src/lower_ir/` becomes the downstream lower-memory layer
- `tests/lower_ir/` keeps the new stage isolated from canonical-IR tests

This should stay small at first.
Do not split further until the layer proves it is worth keeping.

## Recommended First-Version Choice

For phase 1, prefer slot-specific memory operations:

- `load_local`
- `store_local`
- `load_global`
- `store_global`

Recommendation:

- do not introduce generic `addr + load/store` yet
- do not introduce SSA yet
- do not make pass pipeline changes depend on lower IR yet

Why this is the recommended first step:

- it is enough to separate storage from value flow
- it keeps verifier rules simple
- it avoids premature address/value complexity
- it gives a clean serial target for later SSA if that becomes necessary

## First-Version Instruction Set

Phase 1 should stay close to canonical IR and only add the memory-explicit pieces:

- `mov`
- `binary`
- `call`
- `load_local`
- `store_local`
- `load_global`
- `store_global`
- `ret`
- `jmp`
- `br`

This is intentionally conservative.
The value/memory split matters more than growing a large new opcode surface.

## Implementation Plan

### Phase 0: Design Freeze

Objective:

- lock the first-version abstraction before writing implementation code

Deliverables:

- this design doc records the first-version choice
- confirm slot-specific `load/store` over locals/globals
- confirm lower IR is serial downstream from canonical IR
- confirm lower IR keeps canonical block/terminator CFG structure
- confirm first-version value instructions consume only temps/immediates

Exit criteria:

- no unresolved disagreement about whether phase 1 is slot-specific or address-based
- no unresolved disagreement about whether SSA is in scope for phase 1
- no unresolved disagreement about whether lower IR should accept arbitrary legal canonical IR

### Phase 1: Lower IR Skeleton

Objective:

- create a separate lower-IR module boundary without broad lowering changes yet

Expected files:

- `include/lower_ir.h`
- `src/lower_ir/lower_ir.c`
- `src/lower_ir/lower_ir_verify.inc`
- `src/lower_ir/lower_ir_dump.inc`
- `tests/lower_ir/lower_ir_regression_test.c`
- `tests/lower_ir/lower_ir_verifier_test.c`

Scope:

- define lower-IR program/function/block/instruction data model
- make the value/slot split explicit in that data model
- define the first four memory instructions
- add init/free/dump/verify support
- make lower IR manually constructible in tests

Do not do yet:

- full lowering from canonical IR
- SSA
- codegen integration

Exit criteria:

- lower IR can be constructed manually in tests
- dump output is stable enough for regression locks
- verifier can reject malformed lower-IR programs

### Phase 2: Minimal Serial Lowering

Objective:

- prove that canonical IR can lower into the new memory-explicit layer for a tiny useful subset

Expected file:

- `src/lower_ir/lower_from_ir.inc`

Initial supported subset:

- local read
- local write
- global read
- global write
- simple arithmetic on temps/immediates
- simple return
- simple assignment

Examples to support first:

- `return a;`
- `a = b; return a;`
- `a = b + 1; return a;`
- `g = a; return g;`

Do not do yet:

- every expression form
- all control-flow shapes
- optimizer support

Exit criteria:

- serial path exists: `canonical IR -> lower IR`
- representative examples above dump correctly
- lower-IR verifier remains green on lowered output

### Phase 3: Lower IR Test Baseline

Objective:

- make the new layer regression-safe before broadening coverage

Expected files:

- `tests/lower_ir/lower_ir_regression_test.c`
- `tests/lower_ir/lower_ir_verifier_test.c`

Coverage targets:

- valid dumps for representative manually built and later lowered programs
- malformed `load/store` negatives
- invalid local/global id negatives
- illegal result-shape negatives
