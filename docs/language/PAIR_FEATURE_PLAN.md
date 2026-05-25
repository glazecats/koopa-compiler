# Pair Feature Plan

## Goal

- Land a first tiny composite-type slice under `-extension`.
- Keep the implementation conservative enough that it does not force a new
  ABI object model, backend aggregate lowering, or generic struct support.
- Use it as the bridge from control-flow-oriented features (`defer`,
  `fndefer`) into richer type-oriented language work.

## Current Landed Slice

- `pair` is accepted only under `-extension`.
- Supported surface:
  - local declarations such as `pair p;`
  - local initializer lists such as `pair p = {1, 2};`
  - local copy initialization such as `pair q = p;`
  - field reads/writes through `.first` and `.second`
  - local pair-to-pair assignment such as `q = p;`
  - field use in ordinary integer expressions, assignments, and loop/control
    expressions
- Conservative lowering strategy:
  - each `pair` local lowers into two hidden scalar local slots
  - field access rewrites onto those scalar slots
  - no new backend aggregate representation is introduced

## Current Restrictions

- still rejected in this first slice:
  - non-`-extension` use
  - globals of type `pair`
  - function parameters of type `pair`
  - returning `pair`
  - arrays of `pair`
  - field names other than `.first` and `.second`
  - plain `pair` values in ordinary integer expression positions
  - non-local/captured pair flow

## Why This Shape

- It gives the language line one real composite type without forcing full
  `struct` support yet.
- It keeps `-extension` on the conservative translation path.
- It provides a practical stepping stone toward:
  - richer type checking
  - general `struct`
  - future captured environments / closure payloads

## Testing Authority

- parser regression remains green after adding `pair` syntax
- semantic regression remains green after adding `pair` extension gates and
  field validation
- IR regression remains green after pair lowering through hidden scalar slots
- compiler-driver regression now includes:
  - reject outside `-extension`
  - accept the basic extension slice
  - accept local copy initialization/assignment
  - reject plain pair values in scalar contexts
  - reject invalid fields
- `tools/test_extension_runtime.sh` now includes one end-to-end `pair`
  runtime case

## Likely Follow-Ups

1. unify `pair` and `struct` under one aggregate/type-check layer
2. broaden the scalar type system only after that shared layer is stable
3. widen the type system beyond `int`/`void`/function-valued parameters
