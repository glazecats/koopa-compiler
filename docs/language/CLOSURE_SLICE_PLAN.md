# Closure Slice Plan

## Goal

- Start the first **real closure** design line only after the current
  non-capturing function-value groundwork is stable enough.
- Keep the first closure landing much narrower than “full first-class
  functions”.
- Prefer one honest vertical slice over a half-open generic model.

## Current Readiness

The live tree now already has several useful prerequisites:

- function-valued parameters with specialization-based direct calls
- local function-typed aliases bound to known top-level functions or builtins
- local alias direct calls
- local alias forwarding into function-valued parameter calls
- local alias chaining through another already-bound local alias

This means the missing pieces for closures are no longer “basic callable value
syntax”, but specifically:

- capture environment representation
- captured-variable binding semantics
- call-object model once the callee is no longer just a known global symbol

## Phase-1 Decision

The first implementation slice should now be treated as concretely chosen:

1. exact surface form:

```c
int main() {
  int x = 3;
  int f(int) = closure [x] int (int y) { return x + y; };
  return f(4);
}
```

2. closure literal is an expression
3. capture list is explicit and identifier-only in phase 1
4. closure return type is explicit in the literal
5. closure parameter list is explicit in the literal
6. phase 1 use sites are limited to:
   - local initialization of a function-typed local
   - direct call through that local

Why this syntax:

- it is expression-shaped, so it can later grow into return/argument use
- it avoids needing `auto`
- it keeps captures explicit
- it avoids pretending closure literals are just top-level helper names
- it is parseable with one dedicated `closure` primary-expression branch

Phase-1 semantic properties:

1. closure value is created only in a function body
2. captures are **by value** only
3. captured values are limited to scalar `int`
4. closure call sites are direct through one explicit runtime object model,
   not through “pretend it is just a function name”
5. phase 1 does **not** include returning closures

## Non-Goals For First Closure Slice

- capture-by-reference
- mutable shared environments
- heap escape analysis
- closure creation in global initializers
- nested closure chains capturing other closures
- closure values in globals
- closure arrays/struct fields
- generic closure equality/comparison
- float/aggregate captures
- returned closures
- closure-valued parameters

## Minimal Object Model

The first closure-capable value should be modeled explicitly as:

- callee code target identity
- environment payload storage

Concretely, the internal model can be:

1. a hidden helper function for the closure body
2. a hidden environment object carrying copied scalar captures
3. one explicit closure object value carrying:
   - helper target id/name
   - environment slot/address

The key design rule is:

- do not pretend a closure is just a plain function name
- do not pretend a closure is just an integer

## Phase Order

### Phase 1

- closure literal expression
- immediate local storage in a function-typed local
- direct call from that local
- copied `int` captures only

### Phase 2

- return a closure from a function
- detailed next-stage authority:
  [docs/language/RETURNED_CLOSURE_PLAN.md](/workspaces/compiler_lab/docs/language/RETURNED_CLOSURE_PLAN.md)

### Phase 3

- pass closures as arguments to closure-aware formals

The current recommendation is to stop after phase 1 until the environment and
call-object model prove stable.

## Semantic Rules

First slice should require:

1. capture names must resolve to globals, parameters, or function-top-level
   locals declared earlier in the function
2. captures are copied at closure creation time
3. only explicitly captured names are available in the closure body
4. closure body may not mutate captured bindings in phase 1
5. closure values may only be used in:
   - local initialization
   - direct call
6. closure declarations themselves should be limited to the function's
   top-level compound scope in phase 1
7. closure body should be expression/return-only in phase 1:
   - allow one explicit `return expr;`
   - reject nested loops / if / defer / fndefer / capdefer for the first cut

## Lowering Direction

The current function-value specialization path should **not** be stretched into
closures. Closures need a distinct lowering boundary.

Recommended first lowering:

1. create a hidden helper for the closure body
2. materialize hidden environment storage for copied captures
3. materialize an explicit closure object
4. lower closure calls by:
   - loading helper target
   - loading environment
   - passing environment as hidden first argument

That hidden-environment-first-argument rule is the most likely clean first
bridge into existing function lowering.

### Environment policy decision

Phase 1 should be **stack/local-only**:

- environment storage is materialized in the creating function's local frame
- because closures cannot yet be returned or stored globally, no heap
  environment is needed
- this keeps lifetime reasoning simple and verifier-friendly

## Why Not Start With Full Generality

Because the following concerns arrive immediately once captures are real:

- lifetime of returned closures
- storage location of environments
- aliasing of captured state
- closure-call lowering below canonical IR
- interaction with defer/capdefer/fndefer

The first slice should therefore prove:

- copied scalar captures
- explicit closure object
- one direct call path

before broader environments or mutable captures are attempted.

## Immediate Implementation Order

1. parser:
   - new `closure` keyword expression
   - capture list parsing
   - explicit return-type + parameter-list parsing
   - brace-body parsing restricted to phase-1 closure body shape
2. semantic:
   - capture-name scope/order checks
   - copied-`int` capture-only rule
   - function-typed local initializer compatibility checks
   - local direct-call-only use rule
3. lowering:
   - hidden helper generation
   - hidden environment local-slot materialization
   - explicit closure-object local representation
   - direct closure-call lowering via hidden environment first arg

## Current Recommendation

The originally recommended phase-1-only boundary has now moved a little
further on the live tree:

- function-top-level local initialization still works
- direct local call still works
- local alias / forwarding / returned-closure follow-ups are already landed
- direct closure literals may now also appear in function-valued actual
  argument positions under the same conservative specialization model

The next implementation turn should therefore prefer continuing from this
checkpoint toward broader, but still explicit, closure/function-value
transport rather than revisiting the original phase-1-only local-call surface.

Current additional checkpoint:

- direct closure literals may now also appear in function-valued actual
  argument positions under the same conservative specialization model, for
  example `apply(closure [y] int (int z) { return y + z; }, 4)`,
  `apply0(closure [y] int () { return y; })`, and the `void` sibling
  `apply0(closure [y] void () { putint(y); return; })`
- this is still intentionally narrower than opening closure literals in plain
  value position everywhere
- local closure bindings now also have one first real runtime-selected family
  transport slice instead of only static straight-line helper identity:
  same-signature same-payload local closures may merge through simple local
  rebinding flow such as `if(c) f=g; return f(4);` and
  `if(c) f=g; return apply(f, 4);`, with runtime dispatch/tagging hidden
  behind the current conservative local-only object model

Longer-term authority:

- once the project moves beyond the current conservative local/runtime-tag
  closure transport and starts implementing one real generic object model,
  use
  [docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md)
  as the design authority for explicit function objects, explicit environment
  objects, and one generic indirect-call ABI
