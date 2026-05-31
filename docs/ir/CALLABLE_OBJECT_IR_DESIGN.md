# Callable Object IR Design

## Goal

- Define the first canonical-IR-facing design for explicit callable objects.
- Turn the current lowering-only callable-object work into a verifier-worthy IR
  contract.
- Prepare the next implementation jump from:
  - lowering-time callable views
  - branch-selected direct-call expansion
  to:
  - explicit callable-object construction
  - explicit callable-object projection
  - explicit `call_indirect`

## Position

The live tree now already has enough internal groundwork to justify an IR
design step:

- ordinary function-value lowering has one shared callable-view family
- returned function-value lowering has one shared returned-payload consumer
  family
- returned direct-callee dynamic closure and noncapturing siblings already
  share the same returned callable-object consumer model
- `IrLowerCallableObjectView` now exists and has real consumers

So the compiler is no longer blocked on "can we describe callable values?".
It is blocked on "what IR contract should represent them?".

## Design Decision

Canonical IR should gain one explicit callable-object value family before
growing a real generic indirect-call path.

Do not continue expressing generic callable semantics only as:

- hidden locals
- hidden payload slot conventions
- caller-side `tag == 1 ? call A : call B` CFG expansion
- lowering-context metadata

Those are still acceptable staging tools, but they are no longer the right
long-term contract.

## Proposed Canonical IR Concepts

### Callable object values

One canonical callable object should conceptually carry:

1. `code`
2. `env`
3. `shape`

Conceptually:

```text
callable = {
  code,
  env,
  shape
}
```

### Why canonical IR should care

Because all of the following are now real source-level patterns:

- local function-typed storage
- returned function values
- closure payload transport
- runtime-selected call families

Without explicit callable-object IR, every new feature keeps reintroducing the
same transport/call problem in another local lowering shape.

## Minimal Op Family

The first IR-level callable-object family does not need to be huge.

Recommended first operations:

### `fn_make`

Create one callable object value.

Conceptual forms:

- `tmp = fn_make_direct target, shape`
- `tmp = fn_make_closure target, env, shape`

Or a single normalized form:

- `tmp = fn_make code, env, shape`

### `fn_code`

Project callable code handle:

- `tmp = fn_code callable`

### `fn_env`

Project callable environment handle:

- `tmp = fn_env callable`

### `fn_shape`

Project callable shape id:

- `tmp = fn_shape callable`

### `call_indirect`

Call through a callable object:

- `tmp = call_indirect callable(args...)`

Semantic lowering rule:

- load `code`
- load `env`
- invoke `code(env, args...)`

## Verifier Contract

The first verifier rules should stay narrow and explicit.

### Callable construction

- `fn_make*` must produce a temp result
- `shape` must be a known valid function-shape id
- `code` must refer to a valid callable entry or canonical callable handle

### Callable projection

- `fn_code`, `fn_env`, `fn_shape` consume only callable-object temps
- their results are temps

### `call_indirect`

- callee operand must be a callable-object temp
- arg count must match the callable shape
- result presence/kind must match the callable shape return kind

The verifier should reject shape-mismatched indirect calls explicitly, rather
than relying on downstream backend accidents.

## Shape Representation

Canonical IR needs one explicit function-shape table or stable shape-id space.

Minimum shape fields:

1. return kind
2. parameter count
3. parameter kinds
4. optional named aggregate references where relevant

This shape table is the semantic bridge between source typing and indirect-call
verification.

## Environment Representation

For the first canonical callable-object slice:

- canonical IR does not need to model heap policy yet
- it only needs a stable notion of "environment handle/value"

That means:

- `env` may initially be represented as one address-like temp or one payload
  slot root
- the backend/lower stages may still treat it conservatively

The key is to make the existence of `env` explicit, not to perfect its storage
strategy on day one.

## Relationship To Current Lowering

The current lowering should be viewed as a staging layer that already knows how
to derive:

- target family
- capture payload
- runtime-selected family tag

The canonical IR step should not throw that away.
Instead it should redirect it into explicit object construction.

Example migration:

Current lowering style:

```text
if tag == 1:
  call helper_a(captures..., args...)
else:
  call helper_b(captures..., args...)
```

Target callable-object style:

```text
tmp.0 = fn_make helper?, env?, shape
tmp.1 = call_indirect tmp.0(args...)
```

The exact first construction path may still use hidden helper selection
internally, but the public canonical IR contract should already be object-like.

## Staging Recommendation

### Stage C0

- this document only
- no code changes yet

### Stage C1

- add shape-id tracking support if not already explicit enough
- keep old lowering behavior, but add one internal path that can materialize a
  callable-object temp conceptually

### Stage C2

- add explicit canonical IR opcodes for `fn_make*`
- add verifier support
- add dump support

### Stage C3

- add explicit `call_indirect`
- migrate one dynamic direct-callee family to it
- keep old branch-selected direct-call lowering as fallback until green

### Stage C4

- migrate returned dynamic family consumers
- migrate forwarding bridges
- retire old ad hoc tag-branch consumers gradually

## First Migration Targets

The best first real IR consumers remain:

1. returned direct-callee dynamic families
2. dynamic function-valued actual-argument forwarding
3. local dynamic direct-callee path

Reason:

- they already have the strongest callable-object semantics in today's tree
- they are the least naturally expressible as plain direct calls
- they currently duplicate the most object-like transport logic

## Non-Goals

- machine-level ABI finalization
- register allocation implications
- cross-module binary compatibility
- optimizer-friendly perfect object folding
- aggressive devirtualization

Those come later.

## Immediate Next Recommendation

After landing this note, the next implementation turn should do one of:

1. add one canonical IR design stub for function-shape ids and verifier shape
   lookup
2. prototype the concrete `fn_make` / `call_indirect` instruction structs and
   dump syntax without yet wiring all lowering paths

## Authority

- This file is now the IR-level design authority for the callable-object step.
- It depends on:
  [docs/language/CALLABLE_OBJECT_IR_PLAN.md](/workspaces/compiler_lab/docs/language/CALLABLE_OBJECT_IR_PLAN.md)
  and
  [docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md)
