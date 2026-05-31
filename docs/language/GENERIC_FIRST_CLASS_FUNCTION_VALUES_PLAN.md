# Generic First-Class Function Values Plan

## Goal

- Define the first honest end-state design for:
  - generic first-class function values
  - generic closure values
  - indirect calls through runtime function objects
- Stop treating the current `-extension` function-value work as an open-ended
  pile of local exceptions.
- Give later implementation turns one stable target model even while the live
  tree continues to land conservative vertical slices first.

## Current Position

The live tree already has a wide conservative surface under `-extension`:

- non-capturing function-valued parameters
- local function-typed bindings
- returned function values
- caller-side dynamic specialization bridges
- local closure bindings with copied scalar captures
- returned dynamic closure families under narrow boundaries

But the current implementation is still **not** generic first-class function
values.

Today we still rely on:

- helper specialization instead of one generic runtime call object
- runtime tags plus branch-selected direct calls instead of generic indirect
  call ABI
- family-specific payload transport instead of one universal function-value
  object representation

This plan defines how to move from the current conservative slice collection to
one real general model.

## End-State Semantic Model

The target user-facing model should eventually allow:

1. function values in ordinary value position
2. closure values in ordinary value position
3. assignment / return / argument / aggregate-field transport through one
   shared value model
4. indirect calls through any value whose static type is function-typed
5. runtime-selected callee identity without source-position-specific special
   lowering rules

Illustrative target surface:

```c
int apply(int f(int), int x) {
  return f(x);
}

int make_adder(int x)(int) {
  return closure [x] int (int y) { return x + y; };
}

int main() {
  int f(int) = make_adder(3);
  int g(int) = add1;
  int h(int) = cond ? f : g;
  return apply(h, 4);
}
```

The important meaning is:

- `h` is one ordinary first-class function value
- it may carry either a plain function target or a closure target+environment
- `apply(h, 4)` uses one generic indirect call path

## Core Design Decision

The generic model should use one explicit **function object** representation.

Do not continue growing special cases that treat function values as:

- plain function names
- plain integers
- pure compile-time metadata
- one-off hidden slot packs with source-position-specific meanings

Instead, every first-class function value should lower through one uniform
runtime object layout.

## Runtime Object Model

### Canonical function object

The canonical runtime representation should be a small fixed-layout object:

1. `code`
   - target helper / function entry identity
2. `env`
   - environment address or null
3. `shape`
   - compact metadata key for verifier/lowering/runtime consistency

Conceptually:

```text
fnobj = {
  code: pointer-or-symbol-handle,
  env: pointer-or-null,
  shape: function-shape-id
}
```

### Why three fields

`code`

- tells the runtime call path where to jump

`env`

- supports both non-capturing and capturing values under one call ABI
- null for non-capturing top-level functions

`shape`

- lets semantic/lowering keep a clear compatibility contract
- avoids pretending every function object with two machine words is freely
  interchangeable

### Non-capturing values

Non-capturing function values use:

- `code = function entry`
- `env = null`
- `shape = static function type shape`

### Closure values

Closure values use:

- `code = closure helper entry`
- `env = environment object address`
- `shape = same static function type shape`

This keeps the generic call ABI identical for both.

## Environment Model

### Generic environment object

A closure environment should become an explicit runtime object too.

Phase-1 generic representation:

```text
env = {
  refcount-or-lifetime-kind: optional later field,
  capture_0,
  capture_1,
  ...
}
```

Important design rule:

- function objects point to environments
- they do not inline arbitrary capture payload directly forever

That is the main shift away from the current transport-specific hidden-slot
families.

### Lifetime policy

The target generic model will eventually need two storage classes:

1. stack/local environment
   - for clearly non-escaping closures
2. heap-lifted environment
   - for escaping/returned/stored closures

But the first generic implementation step does **not** need both at once.

Recommended staged policy:

- first generic object model may still begin with always-stable heap-backed
  environments for simplicity
- later optimization may recover stack-only environments for proven
  non-escaping cases

Reason:

- correctness and uniformity matter more than early allocation cleverness
- the current tree already has many conservative feature slices; the generic
  step should reduce model complexity, not increase it

## Generic Call ABI

The generic indirect call ABI should be:

1. load `code`
2. load `env`
3. pass `env` as hidden first argument
4. pass ordinary source arguments after that
5. branch/call indirectly through `code`

Conceptually:

```text
call fnobj.code(fnobj.env, arg0, arg1, ...)
```

### Helper signature rule

Every function that may appear as a first-class value should have a lowered
callable entry compatible with this ABI.

That means:

- plain top-level functions may need wrapper thunks
- closure helpers already naturally fit

Recommended uniform lowering rule:

1. source function `int add1(int x)`
2. lowered callable form behaves like:

```text
int add1$arity1(env, x)
```

where:

- `env` is unused / ignored for non-capturing functions
- closure helpers actually consume it

This avoids having two incompatible indirect-call ABIs.

## Shape / Type Model

The static type system needs one real notion of **function shape**.

Minimum required fields:

1. return kind
2. parameter count
3. parameter kinds
4. optional named aggregate type references

Recommended next-step internal artifact:

- canonical `function_shape_id`

Every function object carries one `shape` compatible with:

- assignment
- return
- argument passing
- aggregate field storage
- indirect call validation

This is also the right bridge into a richer future type system.

## IR Direction

The current canonical IR is still direct-call-oriented.

A generic first-class model likely needs new IR concepts:

1. materialize function object
   - `fn_make code, env, shape`
2. project function object fields
   - `fn_code`
   - `fn_env`
   - `fn_shape`
3. indirect call op
   - `call_indirect fnobj, args...`

The key recommendation:

- do not encode the generic model as “just more lowering context metadata”
- add explicit IR operations once the model becomes truly generic

The immediate IR-facing follow-up authority now also lives in:

- [docs/language/CALLABLE_OBJECT_IR_PLAN.md](/workspaces/compiler_lab/docs/language/CALLABLE_OBJECT_IR_PLAN.md)

That note defines the bridge from today's lowering-side callable views to a
future explicit IR-level callable-object and `call_indirect` model.

## Lowering Phases

### Phase G1: design-locked transport object

Land one explicit internal function-object representation but keep the current
source surface narrow.

Allowed first uses:

- local init
- assignment
- return
- argument
- direct call through local/returned value

The difference from today:

- all of those use one explicit object model
- not five separate special-lowering families

### Phase G2: generic indirect call

Add one real indirect-call lowering path using the unified ABI:

- `call_indirect fnobj(args...)`

Keep closure environments conservative if needed, but stop using caller-side
tag branching for every dynamic family.

### Phase G3: aggregate transport

Allow function objects inside:

- struct fields
- pair fields
- arrays, if/when supported

### Phase G4: heap/lifetime refinement

Improve environment storage policy:

- stack-only for non-escaping closures
- heap for escaping closures

### Phase G5: optimization cleanup

Only after correctness:

- collapse direct-known `call_indirect` back into direct call when proven
- elide unused env loads
- specialize wrappers/thunks when profitable

## Migration Strategy From Current Tree

The safest path is not “flip everything at once”.

Recommended migration:

1. keep current specialized path green
2. add one internal generic function-object representation alongside it
3. migrate one source-position family at a time onto the new object model
4. keep old specialized bridges only as fallback
5. delete old branch/tag special paths after the generic route proves stable

Suggested migration order:

1. local storage/assignment/return transport
2. direct local indirect call
3. argument passing into function-valued parameters
4. returned closures across boundaries
5. aggregate storage

Current live-tree checkpoint:

- the first code-level step toward that migration is now present:
  lowering has one explicit internal function-value view abstraction for
  identifier-root callable values, and initializer-binding resolution is the
  first migrated consumer
- the next code-level step is now present too:
  direct-callee static identifier-root resolution now also consumes that same
  shared view abstraction instead of keeping a separate local/parameter/builtin
  target lookup path
- the next migration step is now also present:
  direct-callee lowering now reads one first layer of dynamic callable
  metadata through that same shared view abstraction too, including closure
  family-ness, capture payload view, and dynamic tag local id
- the next internal-object step is now also present:
  that shared view abstraction now also carries the current identifier-root
  dynamic family target-set view for bound local function/closure values,
  moving it one step closer to the eventual explicit function-object
  representation instead of leaving family membership split across separate
  lookup paths
- the next dispatch migration step is now also present:
  direct-callee two-target dynamic dispatch now consumes that shared view's
  family-set data directly instead of requerying the binding-owned target set,
  which is the first real call-site consumer of the view's dynamic-family
  payload rather than only its basic target/tag/capture metadata
- the next forwarding migration step is now also present:
  the local dynamic noncapturing function-valued actual-argument bridge now
  also consumes the shared view's dynamic family data instead of separately
  reloading the source binding plus its owned target set
- the next returned-value migration step is now also present:
  lowering now has a first returned-function-value view abstraction, and the
  dynamic returned actual-argument bridge is its first consumer rather than
  rediscovering returned-family payload and two-target metadata entirely by
  hand
- the next discovered boundary is now explicit too:
  the current returned-function-value view helper is only safe when the
  consumer intentionally owns the returned-payload materialization step; a
  direct substitution into the existing returned direct-callee closure path
  double-materialized the producer call and had to be backed out, so the next
  migration there needs a companion abstraction for consuming already-lowered
  returned payload
- the next returned-call migration step is now present:
  that companion “consume already-lowered returned payload” abstraction now
  exists too, and the returned direct-callee dynamic-closure path is its first
  consumer
- the next sibling is now migrated too:
  the returned direct-callee dynamic noncapturing path now consumes that same
  already-lowered returned-payload view, so the returned direct-callee family
  now has both closure and noncapturing dynamic consumers on one returned
  callable-object model

## Explicit Non-Goals For First Generic Step

- polymorphic callable values
- varargs
- cross-module stable ABI
- user-visible reflection on function objects
- equality semantics richer than identity or explicit reject
- mutable capture-by-reference semantics

## Safety Boundaries

Even after the design is chosen, the first implementation should still reject:

- incompatible function shapes
- arbitrary casts between integers and function objects
- aggregate/function-object layout punning
- calling through an object with unknown or mismatched shape

## Recommended Immediate Follow-Up

After landing this design note, the next implementation work should be:

1. add one internal representation note to the frontend/lowering path:
   - explicit “function object” abstraction in lowering code
2. choose whether `code` is first implemented as:
   - symbolic function id
   - backend-later lowered pointer-like value
3. prototype one generic `call_indirect`-style IR design on paper before
   changing parser/semantic surfaces further

## Current Authority

- The current conservative feature history and boundaries remain in:
  [docs/language/NONCAPTURING_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/NONCAPTURING_FUNCTION_VALUES_PLAN.md)
- Current closure-phase history remains in:
  [docs/language/CLOSURE_SLICE_PLAN.md](/workspaces/compiler_lab/docs/language/CLOSURE_SLICE_PLAN.md)
- Current returned-closure phase history remains in:
  [docs/language/RETURNED_CLOSURE_PLAN.md](/workspaces/compiler_lab/docs/language/RETURNED_CLOSURE_PLAN.md)

This document is the forward-looking authority for the real generic end-state.
