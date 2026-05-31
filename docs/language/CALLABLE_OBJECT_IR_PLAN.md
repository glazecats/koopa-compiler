# Callable Object IR Plan

## Goal

- Turn the current lowering-side unification work on function values and
  returned function values into one explicit IR-facing plan.
- Define the bridge from today's internal callable views to a future real
  `call_indirect`-capable IR model.
- Keep this as the immediate next design authority on the path toward
  generic first-class function values.

## Why This Now

The live tree has now reached a different kind of checkpoint:

- identifier-root function values have one shared internal view
- returned function values now also have explicit internal view helpers
- direct-callee and actual-argument consumers are beginning to share those
  views instead of each re-deriving target/tag/capture metadata

This means we are no longer blocked on "can we describe callable values at
all?".

The next missing piece is:

- how that internal callable-object view should surface at the IR layer

If we do not define that now, we will keep extending lowering-only object
logic without a stable downstream contract.

## Current Internal Object Shape

Today the lowering layer effectively knows how to describe a callable value as:

1. direct target identity
2. environment/capture payload view
3. dynamic family tag, when applicable
4. optional two-target family set, when applicable

This is already close to an object model, but it is still:

- split across helper structs
- not an explicit IR value
- not consumable by a generic call instruction

## IR-Facing Design Decision

The future IR-facing callable object should be explicit.

Do not keep it as:

- only lowering-context metadata
- only hidden local naming conventions
- only "if tag then direct call A else direct call B" CFG expansion

Instead, add one explicit callable-object representation boundary.

## Proposed IR Object

### Conceptual fields

At the IR-facing level, one callable object should eventually expose:

1. `code`
   - lowered callable target handle
2. `env`
   - address/slot/payload handle
3. `shape`
   - function-shape id

Conceptually:

```text
callable = {
  code,
  env,
  shape
}
```

### Mapping from current tree

Current noncapturing dynamic family:

- `code` is currently simulated by tag + two target names
- `env` is null
- `shape` is implicit in semantic checks

Current closure dynamic family:

- `code` is currently simulated by tag + two helper names
- `env` is currently simulated by copied capture payload locals
- `shape` is again implicit

The IR plan should convert these implicit contracts into explicit IR values.

## Proposed IR Operations

### 1. Materialize callable object

Potential operation family:

- `fn_make_direct target, shape`
- `fn_make_closure target, env, shape`

For an earlier smaller step, one generic constructor is fine too:

- `fn_make code, env, shape`

### 2. Project callable object

Potential operation family:

- `fn_code x`
- `fn_env x`
- `fn_shape x`

Even if later passes fold these away, making them explicit helps keep the
 representation honest.

### 3. Generic indirect call

Potential operation:

- `call_indirect fnobj, args...`

Lowered call ABI meaning:

- `call fn_code(fnobj)(fn_env(fnobj), args...)`

This is the real endpoint that replaces today's repeated caller-side
branch-selected direct calls.

## Recommended Staging

### Phase I0: document and freeze

- accept the current lowering-side view structs as the pre-IR checkpoint
- use this document as authority before creating new IR instructions

### Phase I1: one internal lowering aggregate

- add one internal "callable object value" abstraction in lowering code
- it may still lower to hidden locals/temps first
- do not require new public IR opcodes yet

This phase is basically:

- unify `IrLowerFunctionValueView`
- unify `IrLowerReturnedFunctionValueView`
- optionally introduce one common `IrLowerCallableObjectView`

Current checkpoint:

- the ordinary callable-view line and the returned callable-view line now each
  have multiple real call-site consumers
- returned direct-callee closure and noncapturing dynamic families now share
  the same returned-payload consumer model
- the next implementation move should therefore be to actually introduce and
  adopt one common `IrLowerCallableObjectView`, rather than continuing to grow
  the two older view structs in parallel forever
- that next implementation move is now also partially landed:
  `IrLowerCallableObjectView` exists in code and initializer-binding
  resolution is its first real consumer
- the next consumer is now landed too:
  direct-callee lowering now consumes `IrLowerCallableObjectView`, so the
  common callable-object abstraction has started serving real call-path
  lowering instead of only declaration/initialization plumbing
- the next returned-call checkpoint is now landed too:
  returned dynamic actual-argument and returned direct-callee dynamic siblings
  now both consume the returned callable-object model, so the next step is no
  longer more local lowering special-cases but explicit IR-level callable
  object operations

### Phase I2: explicit IR object ops

- add explicit canonical-IR operations for callable-object creation/projection
- keep them verifier-backed
- keep course-facing modes isolated if needed

### Phase I3: generic indirect call

- add one explicit call-through-object instruction
- teach lowering/backend how to consume it
- keep existing direct-call fast path for statically known callees

### Phase I4: transport expansion

- move remaining source-position-specific lowering paths onto the explicit
  callable-object IR
- retire old tag-branch special paths once green

## First IR Consumer Candidates

The best first consumers are the ones already partly unified in lowering:

1. returned direct-callee dynamic family
2. dynamic function-valued actual-argument forwarding
3. local dynamic direct-callee path

These already expose the core problem:

- a callable value exists
- its target may be runtime-selected
- its environment may be non-empty
- the consumer wants to call it

That is exactly the shape `call_indirect fnobj, ...` is meant to express.

## Safety Boundaries

The first IR-facing callable-object model should still stay conservative:

- no arbitrary integer casts to callable object
- no shape-erasing transport
- no aggregate-field storage until object semantics are verifier-checked
- no varargs
- no cross-module ABI promises

## Relation to Existing Plans

- This plan depends on:
  [docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md)
- The next IR-facing downstream authority now also exists in:
  [docs/ir/CALLABLE_OBJECT_IR_DESIGN.md](/workspaces/compiler_lab/docs/ir/CALLABLE_OBJECT_IR_DESIGN.md)
- This plan does not replace the current conservative feature-slice histories
  in:
  [docs/language/NONCAPTURING_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/NONCAPTURING_FUNCTION_VALUES_PLAN.md)
  [docs/language/CLOSURE_SLICE_PLAN.md](/workspaces/compiler_lab/docs/language/CLOSURE_SLICE_PLAN.md)
  [docs/language/RETURNED_CLOSURE_PLAN.md](/workspaces/compiler_lab/docs/language/RETURNED_CLOSURE_PLAN.md)

## Immediate Next Recommendation

After landing this note, the next implementation step should be:

1. introduce one common internal `IrLowerCallableObjectView`
   - merge ordinary and returned callable-value views structurally
2. migrate one more returned-call consumer and one more forwarding consumer
   onto that common view
3. then sketch the first verifier-friendly canonical IR op family for
   `fn_make` / `call_indirect`
