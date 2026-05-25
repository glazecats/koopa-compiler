# Function-Value Callee Lowering Plan

## Goal

- Record the conservative landed step that extended the `-extension`
  non-capturing-function-value slice from:
  - function-valued parameter declarations
  - passing a top-level function name as an otherwise-unused argument
- to the first real callee-use step:
  - calling through a function-valued parameter inside the callee body via
    specialization lowering

This document now serves as both the landed-step note and the follow-up
boundary for what still remains out of scope.

## Current Stable Checkpoint

The live tree already supports this narrow vertical slice:

```c
int add1(int x) { return x + 1; }

int apply(int f(int), int x) {
  return x;
}

int main() {
  return apply(add1, 41);
}
```

Current authority:

- parser accepts first function-valued parameter syntax
- semantic accepts top-level function-name actual arguments only in narrow
  supported positions
- lowering/runtime support only the case where the callee never reads the
  function-valued parameter
- semantic now also accepts the next direct-callee source shape `f(x)` for the
  supported binding model, so the remaining missing piece is now squarely the
  specialization lowering itself rather than parser/semantic shape acceptance

## Landed Target

Make this work:

```c
int add1(int x) { return x + 1; }

int apply(int f(int), int x) {
  return f(x);
}

int main() {
  return apply(add1, 41);
}
```

Current live-tree status:

- this shape is now supported end-to-end under `-extension`
- lowering keeps calls direct by specializing the callee for each supported
  top-level binding
- repeated bindings to different top-level functions are intended to produce
  separate specialized helpers rather than generic indirect calls
- one more chained specialization step is now supported too:
  if a specialized helper forwards its bound function-valued parameter into
  another compatible function-valued parameter, lowering continues to resolve
  that target statically and emits the downstream specialized helper
- the same helper-naming / rebinding model also now works for multiple bound
  function-valued parameters in one callee, such as `compose(f, g, x)`
- the same model now also handles `void`-return function-valued parameters and
  builtin targets like `putint`, as long as the signature still matches
- zero-argument function-valued parameters now also work under that same
  helper-specialization model
- zero-argument `void`-return function-valued parameters are now covered too

## Conservative Constraints

The next landing should still reject:

- local-stored function values
- returned function values
- nested/capturing functions
- non-top-level actual function sources
- passing different function values through merges/conditionals/loops that
  would require true runtime function-value representation

## Landed Strategy

### Do not introduce full indirect-call lowering yet

Current backend/IR is direct-call shaped, verifier-backed, and course-facing
stability matters more than generality.

So the next step should **not** be:

- generic code pointers
- arbitrary indirect call ops
- pretending function values are plain integers

### Specialization by bound top-level function

When a call site passes a known top-level function into a function-valued
parameter, create a specialized callee view where the function-valued
parameter is replaced by that concrete function target.

Conceptually:

```c
apply(add1, 41)
```

can lower as if we had:

```c
int apply__bound_add1(int x) {
  return add1(x);
}
```

and then:

```c
return apply__bound_add1(41);
```

## Why This Strategy Fits

- Keeps calls direct in canonical IR.
- Avoids inventing runtime function pointers too early.
- Preserves the conservative `-extension` policy.
- Lets us grow real feature coverage on one important higher-order pattern.

## Required Frontend/IR Conditions

This step is sound only when all are true:

1. The actual function argument is a visible top-level function name.
2. The callee parameter is function-typed.
3. The function-typed parameter is only used as a direct callee in supported
   call positions.
4. No control-flow merge requires the callee body to treat the function value
   as dynamic.

## Implementation Sketch

### Option A: AST-guided specialization before ordinary IR lowering

- Detect eligible calls during lowering.
- Materialize a specialized callee symbol name such as:
  - `apply__fnarg0_add1`
- Re-lower the callee body under a context mapping:
  - parameter name `f` -> concrete target `add1`
- Rewrite direct `f(x)` calls in the specialized lowering context to
  `add1(x)`.

### Option B: lightweight AST clone + rename layer

- Clone the callee AST/body.
- Substitute uses of the function-valued parameter in callee position.
- Lower the cloned body as a separate internal helper.

### Recommendation

- Prefer **Option A** if the substitution can stay local to lowering context.
- Avoid a deep AST clone layer unless the lowering-context substitution becomes
  too fragile.

## IR-Layer Changes Needed

Minimum likely changes:

1. Lowering context support for:
   - active function-value parameter bindings
   - lookup of a bound concrete target name when lowering `call`
2. Internal helper naming scheme for specialized clones/helpers
3. Cache or dedup layer so repeated `apply(add1, ...)` sites do not generate
   duplicate specialized bodies unnecessarily

## Safety Notes

- Do not specialize when the function-valued parameter appears in plain value
  position.
- Do not specialize when the same callee parameter would need multiple targets
  inside one lowered body without a stronger scheme.
- Keep the first landing limited to one-layer top-level known target binding.

## Testing Plan

### Semantic

- accept `return f(x);` only when the surrounding call sites fit the supported
  top-level-function binding slice
- continue rejecting plain `return f;`

### Compiler / Runtime

- `apply(add1, 41)` returns `42`
- two different wrappers with two different bound top-level functions
- repeated same binding should not duplicate incompatible helpers

### Negative

- reject local variable passed as function value
- reject non-function actual
- reject plain function value expression position

## Non-Goals For This Step

- generic indirect calls
- first-class function values across CFG merges
- storing function values in locals/globals
- returning function values
- closures/captures

## Likely Follow-Up After This

1. allow local storage of a bound function value
2. allow returning a bound function value
3. only after those, consider whether a real indirect-call/value
   representation is worth introducing
