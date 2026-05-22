# Defer Feature Plan

## Goal

- Add a first non-trivial language feature with clear semantic value:
  `defer`.
- Treat this as a **language-feature round**, not an optimization round.
- Keep the first landing deliberately scoped, verifier-friendly, and easy to
  test across parser / semantic / IR lowering boundaries.
- Land new language features behind the dedicated `-extension` command-line
  mode first, so `-riscv` / `-perf` remain course-compatible while the
  feature line is still evolving.
- Keep the `-extension` pipeline deliberately **more conservative** than the
  optimization-oriented mainline, because later feature work such as closures
  will likely be more sensitive to aggressive rewriting / inlining / shape
  folding.

## Mode / Pipeline Policy

### User-facing mode boundary

- `defer` and later feature-round syntax are intended to be accepted only
  under `-extension` for the first landing.
- `-riscv` and `-perf` should continue to reject feature-round syntax until a
  feature is declared stable enough to merge into the course-facing modes.

### Conservative extension pipeline

- The first `-extension` goal is:
  **correctly translate feature-bearing programs**, not maximize performance.
- Prefer a pipeline that is close to:
  - parse
  - semantic
  - canonical IR lowering
  - minimal correctness-preserving cleanup
  - backend lowering / emit
- Avoid depending on aggressive mid-pipeline optimization to make feature
  lowering valid.

### Pass decoupling reminder

- Some existing cleanup/optimization helpers are currently fused into the
  ordinary mainline pipeline rather than being optional layers.
- Before `defer` lowering grows beyond parser-only support, audit those
  fused stages and separate:
  - required translation / verifier-preserving cleanup
  - optional optimization / reshaping passes
- `-extension` should be able to run with a smaller, more predictable pass
  set than `-riscv` / `-perf`.
- Keep this same policy in mind for later feature-round work too, especially
  closures or any feature that introduces richer control/data capture.

## User-Facing Semantics

### Basic meaning

- `defer stmt;`
  registers `stmt` to run when the **current scope exits**.
- Deferred statements execute in **LIFO** order inside the same scope.
- A deferred statement runs on every normal scope exit path:
  - fallthrough out of a block
  - `return`
  - `break`
  - `continue`

### Minimal examples

```c
int main() {
  putint(1);
  defer putint(3);
  putint(2);
  return 0;
}
```

- observable order:
  `1 2 3`

```c
int main() {
  defer putint(1);
  defer putint(2);
  defer putint(3);
  return 0;
}
```

- observable order:
  `3 2 1`

```c
int f(int x) {
  defer putint(9);
  if (x) return 1;
  return 2;
}
```

- `putint(9)` runs before either return.

### Scope rule

- `defer` binds to the **innermost enclosing compound scope**.
- Nested blocks maintain independent defer stacks.

```c
{
  defer putint(1);
  {
    defer putint(2);
  }
}
```

- execution order:
  inner `2` first on inner-scope exit, then outer `1` on outer-scope exit.

## First-Landing Restrictions

To keep the first version honest and bounded:

1. Allow only:
   - `defer <statement>;`
   - `defer { ... }`
2. Disallow:
   - `goto`-like nonlocal control transfer if ever added later
3. Keep existing function semantics:
   - no closure capture model
   - deferred code observes ordinary local/global variables exactly as if the
     code were copied onto each scope-exit edge
   - in particular, deferred code reads the value visible at scope-exit time,
     not a registration-time snapshot
4. Defer bodies may contain:
   - expression statements
   - assignments
   - calls
   - compound statements
   - local `if` / loops only if the existing frontend already supports them
5. For the first cut, semantic analysis may reject a deferred body that tries
   to contain `break` / `continue` targeting an outer loop.

## Syntax Plan

## Lexer

- Add keyword: `defer`

## Parser

- Add a new statement kind:
  - `AST_STMT_DEFER`
- Grammar sketch:

```text
statement
  := ...
   | "defer" statement
```

- Parser note:
  `defer if (...) ...` should parse as deferring that whole statement.
  `defer { ... }` should work naturally because compound statements are already
  statements.

## AST / Semantic Model

### AST

- Add a defer statement node containing one child statement.

### Semantic checks

- Reject `defer` outside function bodies.
- Reject null/empty payloads.
- Reject deferred bodies containing `return`.
- Reject deferred bodies whose `break` / `continue` would target a loop
  outside the deferred body itself.

## Lowering Strategy

## Core principle

- Do **not** treat `defer` as a runtime stack feature in the first version.
- Lower it structurally in canonical IR by **cloning deferred bodies onto each
  scope-exit edge**.

This is simpler, explicit, and fits the current block-based verifier model.

## Scope-lowering model

For each compound scope, maintain:

- a list of deferred statements registered in that scope
- insertion order

When lowering an exit from that scope:

- emit the deferred bodies in reverse order before the real exit action

### Exit kinds to rewrite

1. fallthrough to parent scope successor
2. `return`
3. `break`
4. `continue`

### Example rewrite

Source:

```c
{
  stmt_a;
  defer stmt_b;
  stmt_c;
  return x;
}
```

Canonical lowering intent:

```text
stmt_a
stmt_c
stmt_b
return x
```

## Implementation shape

Best first implementation point:

- canonical IR lowering in `src/ir/ir_lower_stmt.inc`

Suggested internal helper additions:

- enter-scope / leave-scope tracking for deferred statements
- a helper that emits all active deferred statements for one scope in reverse
  order onto the current builder edge
- a helper that emits deferred statements for all scopes being unwound by
  `return` / `break` / `continue`

## Correctness boundaries

- Deferred code is duplicated structurally, so:
  - no runtime defer stack is needed
  - existing CFG / verifier infrastructure stays authoritative
- The first cut should prefer explicitness over code-size cleverness.

## IR / CFG Notes

- Defer lowering may increase block count significantly.
- CFG simplification can clean up afterward.
- This feature should be paired with regression tests that check:
  - order
  - execution on early return
  - nested-scope unwind behavior
  - loop break/continue unwind behavior

## Testing Plan

### Parser / AST

1. `defer putint(1);`
2. `defer { putint(1); putint(2); }`
3. nested defers in nested blocks

### Semantic

1. reject malformed defer payload
2. first-cut rejection cases for cross-scope `break` / `continue` inside
   deferred bodies

### IR lowering

1. single defer before fallthrough
2. single defer before return
3. multiple defers in one scope, verify LIFO order
4. nested scopes, verify inner-before-outer unwind
5. defer inside loop body with `continue`
6. defer inside loop body with `break`

### End-to-end

Small source-based output probes that print observable order.

## Suggested Landing Stages

### Stage 1

- keyword + parser + AST node
- semantic acceptance/rejection skeleton
- no lowering yet; parser/semantic tests only

### Stage 2

- canonical IR lowering for:
  - fallthrough
  - `return`
- first `-extension`-only semantic gate:
  - `defer` accepted under `-extension`
  - rejected under `-riscv` / `-perf`
- first conservative pipeline split:
  - ensure feature-bearing programs can bypass non-essential optimization
    stages

## Current Implementation Notes

- The command-line mode `-extension` now exists as a dedicated feature-round
  entrypoint.
- The semantic layer now explicitly gates `defer` behind `-extension`; using
  it under `-riscv` / `-perf` is rejected intentionally.
- The compiler driver now has a first explicit split between:
  - frontend/lowering translation stages
  - ValueSSA translation-core construction
  - optional optimization stages
  - backend text export
- Current `-extension` policy is the first conservative version:
  it builds ValueSSA from lower IR through the lower-level translation entry
  (`value_ssa_build_from_lower_ir(...)`) and skips the extra mainline
  optimization passes for now.
- This is only the first decoupling checkpoint, not the final one:
  even some currently retained translation-core stages should continue to be
  audited and separated into smaller “required for lowering” vs “optional
  reshaping” units where practical.
- First defer-lowering checkpoint is now real on the live tree:
  - `defer` registers inside compound scopes
  - current scope defers are emitted on block fallthrough
  - active defers are emitted before `return`
  - current-scope defers are also emitted on loop `break` / `continue`
- Current first-version semantic boundary is intentionally narrower than the
  parser surface:
  - `defer` only under `-extension`
  - nested `defer` inside defer bodies is now allowed
  - defer bodies containing `return` are rejected
  - defer bodies containing `break` / `continue` are allowed only when those
    transfers target loops nested inside the deferred body itself
- Current regression-locked behavior:
  - compiler-level ordering checks cover simple return ordering, multi-defer
    LIFO ordering, nested-block ordering, break unwind, continue unwind,
    `for`-body-before-step ordering, `for` return/break unwind without step,
    and one nested+multi combined shape plus nested-defer-in-defer-body
    ordering
  - runtime-level extension regression script now also covers the same first
    behavior slice end-to-end through compile -> link -> qemu execution:
    `tools/test_extension_runtime.sh`
  - nested `defer` registration inside a deferred body follows ordinary scope
    exit timing for the body being executed:
    inner blocks flush first, then the surrounding deferred-body scope flushes
    on its own exit. In other words, current semantics match "execute the
    deferred body as ordinary code at unwind time", not "prepend every nested
    defer ahead of the whole outer deferred body".
  - local/global reads inside deferred code also follow that same model:
    values are read at unwind/exit time from the current state then visible,
    not from a snapshot captured when the `defer` was registered
  - that same rule applies to deferred control-flow statements too:
    `defer if (cond) ... else ...` evaluates `cond` at unwind time, because
    the whole `if` statement is what gets deferred
  - `return` from inside nested blocks/branches/loops unwinds every active
    surrounding defer scope on the path out, from innermost to outermost
  - `return expr;` evaluates and freezes the return value before deferred side
    effects run; deferred code may still observe/mutate locals afterward, but
    it does not retroactively change the already-computed return result
  - side effects inside `return expr` itself still happen before defer unwind,
    because they are part of evaluating the return expression
  - in `for` loops, a defer registered in the loop body runs before the loop's
    step expression updates the next-iteration state
  - if a `for` body exits via `return` or `break`, body/outer defers unwind
    normally and the loop step does not run afterward

### Stage 3

- canonical IR lowering for:
  - `break`
  - `continue`

### Stage 4

- widen allowed deferred-body statement families if needed

## Non-Goals For First Version

- runtime-style defer stack
- closures/captures
- exceptions/panic interaction
- arbitrary nonlocal jump support
- optimizer-aware defer sinking/merging

## Why This Feature

- New enough to be interesting
- Not just “missing C homework syntax”
- Exercises parser, semantic checks, and real CFG lowering
- Can be landed incrementally with strong regression coverage
