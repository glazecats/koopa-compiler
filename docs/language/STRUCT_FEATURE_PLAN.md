# Struct Feature Plan

## Goal

- Land a first named composite-type slice under `-extension`.
- Keep it intentionally narrower than full C `struct`.
- Reuse the conservative hidden-slot lowering style already proven by `pair`.

## Current Landed Slice

- top-level type definitions such as:

```c
struct Point { int x; int y; int z; };
```

- supported object surface:
  - local declarations such as `struct Point p;`
  - local initializer lists such as `struct Point p = {1, 2, 3};`
  - local copy initialization such as `struct Point q = p;`
  - local struct-to-struct assignment such as `q = p;`
  - field reads/writes through the declared field names

## Conservative Boundaries

- current slice accepts only:
  - top-level struct type definitions
  - one or more `int` fields per struct definition
  - local struct objects only
- still rejected:
  - non-`-extension` use
  - globals of struct type
  - struct parameters
  - returning struct values
  - arrays of structs
  - plain struct values in ordinary scalar expression positions
  - assignment/copy between different struct types

## Lowering Strategy

- lower each local struct object into two hidden scalar locals
- lower each local struct object into one hidden scalar local per declared
  field, laid out contiguously
- resolve field access by struct-definition field order
- keep backend/object lowering aggregate-free for now

## Why This Shape

- It is a real step beyond builtin `pair`, because field names are now
  user-declared and type-named.
- It stays compatible with the conservative `-extension` pipeline.
- It lays groundwork for:
  - richer type checking
  - more general struct support
  - future environment/object-like feature work

## Testing Authority

- parser regression covers:
  - struct type-definition metadata
  - local struct declaration/copy/member metadata
- semantic regression covers:
  - accepted local struct copy
  - unknown struct-type rejection
  - mismatched struct-copy rejection
- compiler-driver regression covers one compiling struct-copy witness
- `tools/test_extension_runtime.sh` covers one end-to-end struct runtime case

## Likely Follow-Ups

1. unify `pair` and `struct` under one aggregate/type-check layer
2. consider non-`int` fields only after the scalar type system broadens
3. consider struct parameters/returns only after the calling convention story is explicit
