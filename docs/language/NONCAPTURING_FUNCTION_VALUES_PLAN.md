# Non-Capturing Function Values Plan

## Goal

- Add a first higher-order-function slice without implementing closures yet.
- Keep the feature on the conservative `-extension` pipeline first.
- Prefer a feature that is meaningfully new, but does not force heap
  environments, capture rewriting, or aggressive backend cleverness.

## Recommended First Slice

- Support **non-capturing function values** only.
- Allow:
  - passing a function as a value
  - storing a function value in a local/global
  - returning a function value
  - indirect calls through a function-typed value
- Do not support captures or nested closures in the first version.

## Example Surface

```c
int add1(int x) { return x + 1; }

int apply(int f(int), int x) {
  return f(x);
}

int main() {
  return apply(add1, 41);
}
```

## Why This Feature Next

- It opens the door toward higher-order programming, which matches the
  project direction.
- It is much more novel than adding another small syntax sugar.
- It avoids the hardest closure problems for now:
  - capturing locals
  - lifetime extension
  - escape analysis
  - capture-sensitive optimization bugs

## First-Version Safety Boundary

### Allowed

- Top-level function names appearing in value position.
- Function values passed/assigned/returned when signatures match.
- Indirect calls through validated function-typed values.

### Rejected

- Capturing lambdas/closures.
- Nested functions.
- Signature mismatches.
- Arbitrary integer-to-function reinterpretation.
- Compiler-reserved helper symbols as user-facing values.

## Type-System Implications

- This is a good forcing function for a slightly richer type layer.
- Minimum useful addition:
  - function return kind
  - parameter count
  - parameter kinds

This still stops well short of a full rich type system.

## Frontend Plan

### Parser

- Extend declarator grammar enough to express function-typed parameters and
  locals.
- Allow direct function identifiers in value position.

### Semantic

- Distinguish “function as callee” from “function as value”.
- Verify signature compatibility on:
  - assignment
  - call arguments
  - return
  - indirect call

## Lowering Direction

- Keep the first lowering explicit and symbolic.
- Prefer a dedicated symbolic function-value representation over pretending
  function values are plain integers.
- Add indirect-call support only as needed for the first slice.

## `-extension` Policy

- First landing should be `-extension` only.
- Keep correctness independent from aggressive mid-end cleanup.
- Do not require mainline text peepholes or inlining to make it work.

## Testing Plan

### Parser

- Parse function-typed parameters/locals.
- Parse function identifiers in value position.

### Semantic

- Accept matching signatures.
- Reject mismatched signatures.
- Reject non-function indirect callees.

### Compiler / Runtime

- `apply(add1, 41)`
- store a function value in a local then call it
- return a function value then call it later

## Current Checkpoint

- parser accepts first function-valued parameter syntax
- semantic and compiler now support one real conservative end-to-end slice:
  passing a top-level function name into a function-valued parameter, including
  direct callee use like `return f(x);` when the binding can be specialized to
  a known top-level function target
- current lowering strategy is still conservative:
  it does not introduce generic indirect calls, and instead specializes the
  callee into helper names such as `apply__fv_0_add1`
- repeated specializations for different top-level bindings are now part of the
  intended landed slice too
- one further conservative forwarding step is now landed too:
  a function-valued parameter that is already bound to a known top-level target
  may be forwarded into another compatible function-valued parameter, still by
  chained specialization rather than by a runtime function-value object
- the same conservative specialization model now also covers multiple
  simultaneously bound function-valued parameters, for example
  `compose(f, g, x) { return f(g(x)); }`
- the current conservative slice now also covers `void`-return function-valued
  parameters, including binding a builtin such as `putint`
- zero-argument function-valued parameters now also work under the same
  specialization model, for example `int apply0(int f()){ return f(); }`
- that same zero-argument support now also covers `void`-return callees, such
  as `void apply0(void f()){ f(); }`
- still intentionally rejected:
  plain function values in ordinary value position, non-function actual
  arguments, local/global storage of function values, returning function
  values, and CFG-merged dynamic function-value flow
- deeper implementation notes for the landed `f(x)` slice and its follow-up
  boundaries remain documented in
  [docs/language/FUNCTION_VALUE_CALLEE_LOWERING_PLAN.md](/workspaces/compiler_lab/docs/language/FUNCTION_VALUE_CALLEE_LOWERING_PLAN.md)

## Non-Goals

- Capturing closures
- Heap-lifted environments
- Nested function declarations
- Full polymorphic typing
- `float`
- `struct` integration

## Follow-Up Options After This

1. richer type checking infrastructure
2. simple composite types such as `pair` / `struct`
3. `float`
4. real closures / capturing lambdas
