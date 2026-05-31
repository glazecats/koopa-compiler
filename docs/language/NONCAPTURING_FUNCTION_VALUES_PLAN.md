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
- one first reassignment step is now landed too:
  a local function-typed binding may be reassigned to another compatible
  non-capturing top-level/local target and then called through the updated
  binding, for example `int f(int)=add1; f=add2; return f(40);`
- one first dynamic-dispatch step is now landed too:
  a simple local branch-rebinding shape such as
  `int f(int)=add1; if(c) f=add2; return f(40);`
  now dispatches to the runtime-selected target instead of being forced to one
  statically converged callee
- one further dynamic-alias-dispatch step is now landed too:
  that same runtime-selected local function value may now flow through one
  alias hop before the direct call, for example
  `int f(int)=add1; if(c) f=add2; int g(int)=f; return g(40);`,
  and the call through `g` still dispatches according to the copied runtime
  tag instead of collapsing back to one static target
- one first returned-function-value step is now landed too:
  a function may return a compatible non-capturing top-level function value,
  and the caller may bind and call it, for example
  `int pick()(int){ return add1; }` followed by
  `int f(int)=pick(); return f(41);`
- the immediate-call sibling of that same returned-function-value path is now
  landed too: `return pick()(41);`
- one further returned local-producer step is now landed too:
  the returning function may first bind a local non-capturing function value,
  and may even rebind it in straight-line flow before returning it, for example
  `int pick()(int){ int f(int)=add1; return f; }` and
  `int pick()(int){ int f(int)=add1; f=add2; return f; }`
- one further dynamic returned-function-value step is now landed too:
  that same returned non-capturing path now also preserves one first
  runtime-selected family across the return boundary instead of collapsing
  back to one static callee, for example
  `int pick(int c)(int){ int f(int)=add1; if(c) f=add2; return f; }`
  followed by either `return pick(1)(40);` or
  `int g(int)=pick(1); return g(40);`
- one further dynamic forwarding-into-parameter step is now landed too:
  that same runtime-selected non-capturing function value may now also flow
  through one existing function-valued-parameter call surface instead of
  stopping at direct local/immediate call, for example
  `if((putint(0), c)) g=add2; return apply(g, 40);` and
  `int g(int)=pick(1); return apply(g, 40);`
  now branch at the caller into `apply__fv_0_add1` / `apply__fv_0_add2`
- that same caller-side dynamic forwarding bridge now also covers the
  zero-argument `int` and `void` siblings, for example
  `if((putint(0), c)) g=next2; return apply0(g);`,
  `void g()=ping1; if((putint(0), c)) g=ping2; apply0(g);`, and
  `int g()=pick(1); return apply0(g);`
- the next direct returned-result sibling is now also open under the same
  conservative single-family boundary: `return apply(pick(), 4);`,
  `return apply0(pick());`, and the returned-closure siblings
  `return apply(make(3), 4);`, `return apply0(make(3));`, and
  `apply0(make(7));` now lower through the existing specialization path
  directly from the returned call result instead of requiring an intermediate
  local bind first
- one further returned-parameter follow-up is now landed too:
  a function-valued parameter that is already statically bound under the
  specialization model may itself be returned and then called immediately, for
  example `int id(int f(int))(int){ return f; }` followed by
  `return id(add1)(41);`
- that same returned-parameter line now also reaches one first caller-side
  local bind and one first dynamic family sibling:
  `int g(int)=id(add1); return g(41);`,
  `int f(int)=add1; if(c) f=add2; return id(f)(40);`, and
  `int g(int)=id(f); return g(40);`
- the next closure-backed sibling is now also landed on the same narrow
  returned-parameter line:
  `int main(){ int x=3; int f(int)=closure [x] int (int y){ return x+y; }; return id(f)(4); }`
  and `int g(int)=id(f); return g(4);`
- that same closure-backed returned-parameter line now also reaches one first
  dynamic family sibling:
  `if(c) f=g; return id(f)(4);` and
  `if(c) f=g; int h(int)=id(f); return h(4);`
- the next dynamic direct returned-result sibling is now also open on the
  noncapturing line: `return apply(pick(1), 40);`, `return apply0(pick(1));`,
  and `apply0(pick(1));` now reuse the same caller-side dynamic specialization
  bridge directly from the returned call result
- runtime-selected non-capturing function values now also survive one first
  ordinary local assignment, for example
  `h=f; return h(40);`, `h=f; return apply(h, 40);`, and
  `h=f; return h;`
- one further ordinary wrapper step is now landed too:
  parenthesized, comma-result, and the current conservative
  assignment-result wrappers such as `(f)(41)`, `(0, g)(41)`,
  `apply((0, g), 40)`, `(h=f)(41)`, and `apply((h=f), 40)`
  now preserve function-value flow under the same narrow non-capturing model
- that same narrow wrapper family now also reaches one first
  function-typed local-initializer and function-valued return slice:
  `int h(int)=(0, f); return h(41);`, `return (h=f);`, and the dynamic
  returned sibling `if(c) f=add2; return (h=f);` now preserve the same
  non-capturing function-value family through local initialization and return
  payload lowering
- the next expression-shaped function-valued return follow-up is now also
  landed on the non-capturing line: a function-valued return site may now
  return one matching identifier ternary such as
  `return ((putint(0), c) ? f : g);`, and the caller may use either the
  immediate-call sibling `return pick(1)(40);` or the bind-and-call sibling
  `int h(int)=pick(1); return h(40);`, with the selected branch lowering
  through the existing returned runtime-tag family path instead of requiring a
  prior local wrapper assignment
- the matching closure-backed ternary-return sibling is now landed too under
  the same conservative returned-family boundary: when `f` and `g` are
  matching local closures, `return ((putint(0), c) ? f : g);` now preserves
  the selected branch's tag plus copied capture payload through the returned
  hidden-slot path, and the caller may use either the immediate-call sibling
  `return pick(1)(4);` or the bind-and-call sibling
  `int h(int)=pick(1); return h(4);`
- the next expression-shaped local-initializer follow-up is now also landed:
  a function-typed local may initialize from one matching identifier ternary
  over already-supported non-capturing function values, for example
  `int h(int)=c ? f : g; return h(40);`, with the selected branch copied into
  the same runtime-tag local binding model instead of opening plain generic
  function values in arbitrary expression position
- the matching assignment-position sibling is now landed too under that same
  conservative boundary: a function-typed local may be reassigned from one
  matching identifier ternary such as
  `h=((putint(0), c) ? f : g); return h(40);`, and the matching closure-backed
  sibling now also works when `f` and `g` are matching local closures,
  preserving the selected branch's tag plus copied capture payload through the
  existing local binding model
- the next function-valued actual-argument follow-up is now also landed on the
  non-capturing line: an existing function-valued-parameter call may now take
  one matching identifier ternary actual argument under the current
  caller-side dynamic specialization bridge, for example
  `return apply(((putint(0), c) ? f : g), 40);`, with the selected branch
  lowering to a runtime tag and caller-side dispatch into
  `apply__fv_0_add1` / `apply__fv_0_add2` rather than opening generic plain
  function values in arbitrary expression position
- that same ternary actual-argument line now also reaches one first
  closure-backed sibling under the same conservative boundary, for example
  `return apply(((putint(0), c) ? f : g), 4);` where `f` and `g` are matching
  local closures with copied scalar captures; lowering now preserves the
  selected branch's tag plus copied capture payload through the existing
  caller-side specialization bridge instead of requiring a prior local bind
- that same ternary local-initializer line now also reaches one first
  closure-backed sibling under the same conservative boundary, for example
  `int h(int)=c ? f : g; return h(4);` where `f` and `g` are matching local
  closures with copied scalar captures; lowering now copies the selected
  branch's tag plus capture payload into the destination local binding rather
  than opening generic closure values in arbitrary expression position
- the next expression-shaped direct-callee follow-up is now also landed on the
  same conservative line: a call may now use one matching identifier ternary
  directly in callee position, for example
  `return (((putint(0), c) ? f : g))(40);`, with lowering branching into the
  selected direct target call instead of requiring a prior local bind or
  function-valued-parameter wrapper
- that same ternary direct-callee line now also reaches one first
  closure-backed sibling under the same conservative boundary, for example
  `return (((putint(0), c) ? f : g))(4);` where `f` and `g` are matching
  local closures with copied scalar captures; lowering now preserves the
  selected branch's tag plus copied capture payload through a branch-selected
  direct closure-helper call instead of opening generic plain closure values
  in arbitrary expression position
- the latest implementation follow-up is now also internal-structure facing:
  ternary local-init, actual-argument, direct-callee, and returned-payload
  lowering now share a first common branch-materialization helper family for
  runtime tag plus capture payload emission, instead of keeping four
  independent emit blocks
- the next internal follow-up is now landed too:
  direct-callee lowering for returned/local dynamic two-target families now
  also shares one common dispatch-call helper for the `tag == then ? call A :
  call B` CFG skeleton, instead of duplicating that branch/join shape in each
  caller path
- the next internal transport follow-up is now landed too:
  ternary function-value reassignment now also shares the same common
  branch-materialization helper family for runtime tag plus capture payload
  emission, instead of keeping a separate ternary reassignment emit block
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
- the forward-looking end-state authority for the real generic goal now also
  lives in
  [docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md](/workspaces/compiler_lab/docs/language/GENERIC_FIRST_CLASS_FUNCTION_VALUES_PLAN.md):
  one explicit function-object representation, one generic indirect-call ABI,
  and one staged migration path away from source-position-specific tag/specialization
  bridges

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
