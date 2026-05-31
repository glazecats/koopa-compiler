# Returned Closure Plan

## Goal

- Define the next conservative closure step after the now-stable
  non-escaping local-closure slice.
- Make **returned closures** the next closure milestone before any broader
  “generic closure value everywhere” implementation starts.
- Keep the design honest:
  no generic indirect-call ABI, no heap-first runtime, and no fake claim that
  closures are already ordinary scalar values.

Relationship to the longer-term end-state:

- this document remains the authority for the conservative returned-closure
  phase
- the forward-looking generic end-state authority now also lives in
  [docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md)
  for the later shift to explicit function objects, explicit environment
  objects, and one generic indirect-call ABI

## Why This Next

- The live tree already supports:
  - direct local closure call
  - local closure alias chains
  - non-escaping local closure alias forwarding into existing
    function-valued-parameter specialization calls
- The next real missing capability on the path to full closures is no longer
  “call a closure at all”, but:
  - let a closure survive the end of its creating scope
  - move a closure across one function boundary
  - still keep the callee target statically knowable

This is the smallest next step that forces the project to answer the real
lifetime/object-model question without immediately opening full first-class
closure flow.

## Current Boundary

Supported today under `-extension`:

- copied-by-value `int` captures only
- closure literal in local function-typed declaration initializer only
- direct local call through that bound local
- local alias chains used only for later direct calls
- non-escaping local alias forwarding into existing function-valued-parameter
  specialization calls

Still intentionally rejected today:

- direct closure literal use in ordinary value position
- closure literal as a direct actual argument
- closure assignment after a separate function-typed local declaration
- expression-wrapped closure forwarding such as comma/value-position wrapping
- returned closures
- escaping closures in globals/arrays/aggregate fields
- closure-valued parameters in the general sense

## Phase-2 Target

The next implementation slice should make this kind of source shape work:

```c
int make(int x)(int) {
  return closure [x] int (int y) { return x + y; };
}

int main() {
  int f(int) = make(3);
  return f(4);
}
```

Zero-arg sibling:

```c
int make(int x)() {
  return closure [x] int () { return x; };
}
```

`void` sibling:

```c
void make(int x)() {
  return closure [x] void () { putint(x); return; };
}
```

Important note:

- this plan chooses the next **semantic capability**
  “return a closure from a function and bind it into one local at the caller”
- the exact parser/declarator spelling above should be treated as the current
  recommended direction because it matches the already-landed nested
  function-type notation for parameters/locals
- if parser pressure later forces a different but equivalent spelling, keep the
  runtime/object-model decisions below unchanged

## Design Decision

### Do not start with heap allocation

The first returned-closure slice should **not** begin with:

- malloc-backed environments
- hidden runtime free rules
- reference-counting
- capture-by-reference environments

Instead, the first returned-closure slice should still rely on:

- copied-by-value captures only
- scalar `int` captures only
- a fixed closure payload layout

### Treat the returned closure as a tiny aggregate-like object

The key decision:

- the first returned closure should lower like a conservative, fixed-layout
  aggregate payload
- captured scalar values travel as explicit hidden slots
- helper target identity remains **statically known metadata** for the kept
  slice rather than becoming a fully dynamic runtime code pointer

This means the first returned closure object is conceptually:

1. one known helper family identity
2. zero or more copied capture payload slots

The helper identity is *not* yet a generic runtime value.
For the first slice, it should stay a compile-time-known family attached to the
source function/local binding.

## Kept Conservatism

To avoid needing true dynamic dispatch immediately, the first returned-closure
slice should require:

1. one returned closure family per source function
2. all `return` sites in that function must agree on:
   - closure helper target
   - closure parameter count / return type
   - capture slot count and order
3. returning different closure helpers across CFG merges remains rejected

Example still rejected for the first slice:

```c
int make(int x, int flag)(int) {
  if (flag) return closure [x] int (int y) { return x + y; };
  return closure [x] int (int y) { return x - y; };
}
```

Even though both literals have the same apparent signature, they represent
different helper families and would require runtime helper selection.

## Phase-2 Allowed Surface

### Allowed

- top-level function definitions whose return type is function-valued under the
  extension nested declarator syntax
- `return closure [...] ...;` from those functions
- assigning/copy-initializing that returned closure into one local
  function-typed declaration at the caller
- direct call through that local
- zero-arg and `void` siblings
- one-hop call chains such as:
  - `int f(int)=make(3); return f(4);`
  - `return make(3)(4);`

Current checkpoint:

- both `int f(int)=make(3); return f(4);` and `return make(3)(4);` are now
  landed under the same conservative copied-`int` / single-family returned
  closure boundary
- the next local-binding producer sibling is now also landed under that same
  boundary: `int pick(int x)(int){ int f(int)=closure [x] int (int y){ return x+y; }; return f; }`
  may now feed either `int h(int)=pick(3); return h(4);` or `return pick(3)(4);`
- the next existing supported caller position is now also open under that same
  boundary: `int f(int)=make(3); return apply(f, 4);` may forward through the
  existing function-valued-parameter specialization path
- the first zero-arg and `void` siblings of that same forwarding path are now
  landed too: `int f()=make(3); return apply0(f);` and
  `void f()=make(7); apply0(f);`
- the first one-hop alias sibling of that same forwarding path is now also
  landed: `int f(int)=make(3); int g(int)=f; return apply(g, 4);`
- the next ordinary local-assignment sibling is now also open under that same
  boundary: `int f(int)=make(3); int g(int)=make(5); g=f; return g(4);` and
  the returned sibling `g=f; return g;` now preserve the same helper family
  plus copied capture payload transport across local assignment
- the next direct returned-result sibling is now also open under that same
  boundary: `return apply(make(3), 4);`, `return apply0(make(3));`, and
  `apply0(make(7));` now forward the returned closure call result directly
  into the existing function-valued-parameter specialization path without an
  intermediate local bind first
- the matching dynamic direct returned-result sibling is now also open on the
  noncapturing side of the same broader function-value story:
  `return apply(pick(1), 40);`, `return apply0(pick(1));`, and
  `apply0(pick(1));` now branch through the existing caller-side dynamic
  specialization bridge directly from the returned call result
- the first narrow dynamic returned-closure sibling is now also landed:
  when one function returns a local closure binding chosen from up to two
  same-signature same-capture-shape local closure helpers, for example
  `if(c) f=g; return f;`, the caller may now use either
  `int h(int)=pick(3, 1); return h(4);` or `return pick(3, 1)(4);`
  through a hidden `tag + captures` returned payload plus caller-side helper
  dispatch. This is still intentionally narrower than opening arbitrary
  multi-site closure-family returns in general

### Rejected

- returning a closure literal directly from conditional branches that produce
  different helper families
- storing returned closures in globals
- storing returned closures in arrays/aggregate fields
- closure-valued parameters on ordinary source functions
- generic closure equality/comparison
- capture-by-reference
- mutable shared environments
- aggregate/float captures
- returned closures escaping beyond one immediate local binding/call slice

## Runtime/Object Model

### Closure family metadata

For the first returned-closure slice, each supported returned-closure-producing
function should have one statically known closure family descriptor:

- helper target symbol
- capture slot count
- capture slot value kinds

This descriptor is compile-time metadata, not a user-visible runtime object.

### Returned payload transport

Returned closure payload should lower through hidden slots, analogous to the
aggregate-return strategy already used elsewhere:

- the caller provides destination slots for the closure payload
- the callee writes copied capture values into those slots
- ordinary return control then completes

Because helper identity is static for the kept slice, the payload only needs to
carry captures, not a dynamic code pointer.

### Caller-side local binding

At the call site:

- a local function-typed declaration initialized from `make(...)` binds:
  - known helper target from the callee family descriptor
  - local payload slots containing the copied captures returned by the callee

After that, direct local closure call lowering can reuse the already-landed
phase-1 call path:

- prepend payload capture slots
- call the known helper directly

## Semantic Rules

The first returned-closure slice should require:

1. returned closure functions exist only under `-extension`
2. the source function’s declared function-valued return type must match the
   returned closure literal signature exactly
3. every reachable `return` in the function must either:
   - return the same closure family shape
   - or reject
4. captures remain copied-by-value `int` only
5. caller use remains narrow:
   - local initialization from the returned closure
   - direct call through that local
6. plain value-position use of the returned closure still rejects

## Parser / AST Work

Minimum parser expansion likely needed:

- allow function-valued return declarators at top level under `-extension`
- preserve enough AST metadata to distinguish:
  - scalar return
  - aggregate return
  - function-valued return

Keep parser honesty:

- non-closure neighboring forms that are not yet implemented should still
  reject with explicit diagnostics rather than silently parsing into unrelated
  declaration shapes

## Lowering Direction

### Callee side

- lower each returned closure literal to one hidden helper as today
- reserve hidden return payload slots for the copied captures
- on `return closure ...`, materialize the copied captures into those payload
  slots

### Caller side

- when lowering `int f(int)=make(3);`, detect that the source callee returns a
  closure family
- create the local binding with:
  - known helper target
  - local payload slots filled by the call result

### Why not generic indirect call yet

Because this kept slice can still stay entirely in the direct-call world:

- helper target remains static
- payload slots are scalar
- no runtime function-pointer operation is needed

## Testing Plan

### Positive

- return one closure, bind into one local, call it
- zero-arg sibling
- `void` sibling
- capture snapshot survives creator-function exit
- repeated calls to `make(3)` and `make(7)` yield distinct copied payloads
- one-hop returned-closure call chain via another helper if intentionally kept

### Negative

- different closure families across returns
- direct literal actual-argument use still rejects
- plain returned-closure value position still rejects
- returned closure assigned after a separate local function declaration still
  rejects unless intentionally opened
- aggregate/float capture rejects remain intact

### Downstream

- focused IR/lower-IR regression locks for:
  - hidden closure-return payload slot count/order
  - caller-side rebinding of payload slots into local closure bindings
  - no accidental dynamic code-pointer introduction

## Implementation Order

1. parser/type metadata for function-valued returns
2. semantic closure-family compatibility for returned closure functions
3. IR hidden-slot return transport for returned closure payloads
4. caller-side local binding from closure-returning call result
5. runtime/compiler regressions
6. reject-matrix cleanup for unsupported neighboring positions

## Exit Criteria

Treat this slice as landed only when all are true:

- returned closure local-bind-and-call works for `int`, zero-arg, and `void`
  siblings
- copied captures survive creator-function exit correctly
- all-return-sites-same-family restriction is enforced and regression-locked
- no heap allocation or generic indirect-call ABI has been introduced
- broader closure-valued parameter and escaping-storage positions still reject

## Non-Goals For This Step

- heap environments
- closure-valued parameters in the general sense
- storing returned closures in globals/arrays/aggregate fields
- returning different helper families behind one source signature
- capture-by-reference
- mutable shared environments
- closure equality/comparison
- fully generic first-class closures across arbitrary CFG merges
