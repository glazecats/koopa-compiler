# Unless Feature Plan

## Goal

- Add a small, low-risk extension-only control-flow feature: `unless`.
- Treat `unless (cond) stmt` as syntax sugar for `if (!cond) stmt`.
- Use this feature as a deliberately conservative post-`defer` checkpoint
  feature.

## User-Facing Semantics

- `unless (cond) stmt;` executes `stmt` exactly when `cond` is false.
- `unless` has no special runtime semantics beyond ordinary `if`.
- Condition evaluation timing matches ordinary `if`: evaluate `cond` at
  statement execution time.
- Side effects in `cond` are preserved exactly as they would be in
  `if (!cond)`.

### Examples

```c
unless (x) return 1;
```

Equivalent to:

```c
if (!x) return 1;
```

```c
unless (ready()) {
  putint(0);
}
```

Equivalent to:

```c
if (!ready()) {
  putint(0);
}
```

## Mode Boundary

- `unless` is accepted only under `-extension` in the first version.
- `-riscv` and `-perf` reject `unless`.

## Implementation Strategy

### Parser-first desugar

- Do **not** create a downstream-only new control-flow semantic if it can be
  avoided.
- Parse `unless (cond) stmt` and immediately desugar it in the AST to an
  ordinary `if` statement whose condition is unary `!cond`.
- Record that the resulting statement came from an extension-origin construct
  so semantic mode gating can still reject it outside `-extension`.

### Why this strategy

- Reuses existing `if` semantic flow analysis.
- Reuses existing IR lowering for `if`.
- Reuses existing backend/control-flow behavior without introducing a second
  control-flow statement family.
- Minimizes risk to `RETURN_CHECK` and loop/control-flow analyses.

## AST / Semantic Notes

- Lexer adds keyword `unless`.
- Parser lowers to:
  - statement kind: ordinary `AST_STMT_IF`
  - condition: unary `!` over the parsed original condition expression
- Statement metadata preserves an extension-origin marker so semantic analysis
  can reject non-extension use.

## Testing Plan

### Parser

- `unless(x) return 1;` becomes an `if` AST node with unary-`!` condition.

### Semantic

- accept under `-extension`
- reject outside `-extension`

### IR

- `unless(0) putint(1);` lowers like `if(!0) putint(1);`
- `unless(1) putint(1);` does not execute the body

### Compiler/runtime

- compile under `-extension`
- reject under non-extension modes
- runtime output matches `if (!cond)` expectation

## Non-Goals

- no standalone `AST_STMT_UNLESS` lower layers unless later evidence justifies
  it
- no custom backend lowering path
- no special optimization behavior
