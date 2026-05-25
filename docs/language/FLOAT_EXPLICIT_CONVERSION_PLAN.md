# Float Explicit Conversion Plan

## Goal

- Define the next deliberate step after the current conservative float
  checkpoint.
- Add **explicit** scalar conversion first, without opening implicit
  conversion.
- Keep the feature honest with the already-landed helper-backed float model
  and the current `-extension` policy.

## Why This Next

- The current transport / condition / comparison / helper-arithmetic slices
  are now broad enough that the next useful capability is not “one more
  reject-matrix corner”.
- Users now predictably hit boundaries like:
  - `int x = g;`
  - `int h = id(g);`
  - `sink(add3(...));`
  - `return (g ? h : h) == 0;`
- Those boundaries are now regression-locked and understood. The next step
  should let users cross some of them intentionally rather than continuing to
  poke accidental openings.

## Non-Goals

- No implicit `int <-> float` conversion.
- No promotion lattice.
- No “usual arithmetic conversions”.
- No attempt to claim full C cast semantics yet.
- No pointer/aggregate/function-value casts.
- No silent backend-native float ALU story.

## Proposed Surface

First version syntax:

```c
int x = int(g);
float y = float(3);
return int(add3(1.0, 2.0, 3.0));
```

Treat this as a builtin conversion-expression form, not as a general function
 call and not as a declaration.

## First Supported Conversions

- `int(expr_float)`:
  - source must resolve to scalar `float`
  - result type is scalar `int`
- `float(expr_int)`:
  - source must resolve to scalar `int`
  - result type is scalar `float`

Everything else still rejects for the first version:

- `int(expr_int)`:
  - either reject as redundant for now, or accept only if it stays trivial
  - recommendation: reject in v1 to keep the feature surface crisp
- `float(expr_float)`:
  - recommendation: reject in v1 for the same reason
- aggregate/function/void arguments
- multiple arguments

## Recommendation

For the first landing, support only:

- `int(float_expr)`
- `float(int_expr)`

and reject:

- redundant same-type conversions
- non-scalar conversions

This keeps every successful use semantically meaningful.

## Semantic Contract

### Explicitness

- Conversions only happen when the source explicitly uses `int(...)` or
  `float(...)`.
- Existing mismatch boundaries stay in place without that syntax.

### Placement

Converted expressions may then participate in already-supported contexts as
their **result type**:

- `int(g)` may flow into:
  - assignment to `int`
  - return from `int` function
  - call argument to `int` parameter
  - integer arithmetic/comparison/control as already supported
- `float(i)` may flow into:
  - assignment to `float`
  - return from `float` function
  - call argument to `float` parameter
  - existing float transport/comparison/helper-arithmetic paths

### Derived-Float Interaction

- Explicit conversion is the sanctioned bridge from a currently unsupported
  derived float value context into `int` value contexts.
- Example:

```c
int x = int(add3(1.0, 2.0, 3.0));
int y = int(g ? h : h);
sink(int((-id(1.0) ? 1.0 : 2.0)));
```

These should become valid only because the programmer opted in.

## Lowering Strategy

### `float(int_expr)`

- Lower through an explicit helper call, for example:
  - `__builtin_i2f32`

### `int(float_expr)`

- Lower through an explicit helper call, for example:
  - `__builtin_f2i32`

## Why Helpers

- Matches the current honesty story for helper-backed float arithmetic.
- Avoids pretending canonical IR / lower IR / machine layers already have
  native float/int conversion ops.
- Keeps backend impact narrow and testable.

## IR / Lowering Shape

Representative intent:

```c
float y = float(x + 1);
int z = int(add3(a, b, c));
```

should lower to explicit helper calls in the same way current float arithmetic
 lowers to `__builtin_fadd32` / `__builtin_fsub32` / `__builtin_fmul32` /
 `__builtin_fdiv32`.

Suggested helper names:

- `__builtin_i2f32`
- `__builtin_f2i32`

## Diagnostics

Keep diagnostics explicit and conservative.

Suggested new family:

- `SEMA-EXT-038`:
  - explicit scalar conversion family unsupported in this context/signature

Example uses:

- wrong arity
- non-scalar argument
- redundant same-type conversion if we reject it in v1

Do **not** reuse the generic `SEMA-EXT-035` once the syntax is recognized;
after opt-in syntax appears, failures should point at the conversion feature
itself rather than at the old “derived float values are closed” story.

## Testing Plan

### Positive semantic / compiler / downstream

- `int x = int(g);`
- `int x = int(id(g));`
- `int x = int(add3(1.0, 2.0, 3.0));`
- `int x = int(g ? h : h);`
- `sink(int((-id(1.0) ? 1.0 : 2.0)));`
- `float y = float(3);`
- `float y = float(x + 1);`

### Negative

- `int(g, h)`
- `float()`
- `int(pair_expr)`
- `float(void_call())`
- `int(3)` if redundant same-type stays rejected
- `float(1.0)` if redundant same-type stays rejected

### Downstream shape locks

- translation-only IR / lower-IR / ValueSSA should show explicit helper-call
  lowering
- default `machine_ir` / `machine_select` report paths should preserve the
  helper-call story unless later legal optimization collapses a trivial case

## Recommended Landing Order

1. Parser/AST support for conversion-expression syntax.
2. Semantic recognition plus conservative diagnostics.
3. `int(float_expr)` helper lowering.
4. `float(int_expr)` helper lowering.
5. Focused regression locks across compiler / IR / lower-IR / ValueSSA /
   machine-report surfaces.

## Current Decision

- Prefer this explicit-conversion step before any implicit conversion work.
- Keep the current reject matrix in place as the default behavior.
- Treat successful conversion syntax as an opt-in bridge, not as proof that
  the language now has a general scalar-conversion system.

## Current Live Status

- parser/AST support is now landed for `int(expr)` / `float(expr)`
- semantic now also recognizes that syntax as a dedicated family
- current implementation split:
  - `int(float_expr)` is now landed and lowers through explicit helper call
    `__builtin_f2i32`
  - `float(int_expr)` is now also landed and lowers through explicit helper
    call `__builtin_i2f32`
  - both directions now also have focused default-path regression locks on
    `ValueSSA`, `machine_ir`, and `machine_select`
  - focused front-half bridge locks now also cover representative
    post-conversion value-context crossings such as `int(g ? h : h)`,
    `sink(int(add3(1.0, 2.0, 3.0)))`, `int x = int(add3(...));`,
    `x = int(add3(...));`, `int(g ? h : h) == 2`, and
    `int(add3(...)) + 1`
  - the symmetric `float(int_expr)` side now also has a first richer
    front-half matrix beyond the seed conversion witness:
    `float z = float(add3(...));`, `y = float(add3(...));`,
    `float(add3(...)) == float(6)`, and `float(add3(...)) + 1.25`
  - those same `float(int_expr)` richer families now also have focused
    default-path regression locks on `ValueSSA`, `machine_ir`, and
    `machine_select`
  - redundant same-type conversions still reject through `SEMA-EXT-038`
- lowering/runtime support is now present for the intended first bidirectional
  scalar pair rather than partial/one-sided
