# Aggregate Parameter / Return Plan

## Goal

- Define the first conservative `-extension` path for aggregate parameters and
  aggregate return values.
- Reuse the already-landed hidden-scalar-slot aggregate model instead of
  introducing a full runtime object ABI.
- Make the next aggregate step explicit before implementation starts.

## Why Now

- Local `pair` / `struct` support is now mature enough that the next missing
  capability is no longer field access or local copy flow.
- Nested local aggregates already survive through semantic, IR, lower IR,
  ValueSSA, machine-ir, machine-select, and runtime regressions.
- The main remaining aggregate gap is function boundary flow:
  - parameters
  - returns
  - calls

## Current Boundary

- Now supported on the live `-extension` tree:
  - `pair` parameters
  - known `struct Name` parameters
  - returning `pair`
  - returning known `struct Name`
  - aggregate call-result local copy-init / assignment
  - parameter-to-return forwarding such as `pair id(pair p){ return p; }`
  - nested aggregate returns representable by the current recursive slot model
  - first direct aggregate-returning call chains into aggregate parameters such
    as `sum(mk())`
  - first two-hop aggregate return/forwarding chains such as
    `wrap(){ return id(mk()); }`
    with the current pair witness plus one nested-struct sibling now also
    checked on the downstream machine-report surfaces
- Still intentionally bounded today:
  - direct aggregate call-result member access such as `mk().second`
    and parenthesized / nested-member variants such as `(mk()).second`
    or `mk().p.second`
  - arbitrary plain aggregate value positions
    including scalar initializer / scalar assignment contexts such as
    `int x = mk();` or `x = wrap();`, plus control conditions such as
    `if(wrap())`
  - aggregate globals / arrays / generic aggregate object flow

## Non-Goals

- Full C aggregate ABI compatibility.
- User-visible pointers or references.
- Aggregate globals.
- Arrays of aggregates.
- Float-valued aggregate fields.
- Variadics.
- Closure environments or captured aggregate objects.

## Recommended Landing Order

### Phase 1: Aggregate Parameters

- Allow function parameters of type:
  - `pair`
  - known `struct Name`
- Keep return values unchanged for this phase.
- Support:
  - declarations
  - definitions
  - direct calls
  - local copies from aggregate parameters
  - field access on aggregate parameters

### Phase 2: Aggregate Returns

- Allow functions to return:
  - `pair`
  - known `struct Name`
- Support:
  - `return p;`
  - `return s;`
  - aggregate call result copy-init / assignment
  - aggregate call result member access when the aggregate result first lands in
    a local

### Phase 3: Boundary Cleanup

- Add deeper mixed cases:
  - more nested aggregate return families
  - more multi-hop aggregate call chains
  - reject-matrix cleanup for unsupported aggregate value positions
  - mismatch diagnostics across declarations and calls
  - direct call-result member-access policy if it is ever intentionally opened

## First-Version Safety Boundary

### Allowed

- Aggregate parameters and returns only under `-extension`.
- Only direct named aggregate types already supported locally:
  - `pair`
  - known `struct Name`
- Nested aggregates only when they are already representable by the current
  local flattening model.

### Rejected

- Aggregate parameters/returns outside `-extension`.
- Any aggregate arrays.
- Unknown struct types.
- Mismatched aggregate signatures.
- Plain aggregate values escaping into undeclared generic runtime object form.
- Direct aggregate call-result member access before first landing in a local.
  This includes parenthesized and nested-member call-result chains too.

## Lowering Direction

## Parameter Strategy

- Do not treat aggregate parameters as opaque runtime objects.
- Instead, expand each aggregate parameter into a deterministic hidden scalar
  parameter sequence at lowering time.
- Examples:
  - `pair p` becomes two hidden scalar parameter locals
  - `struct Point p` becomes one hidden scalar local per field
  - nested `struct Mid` follows the same recursive flattening rule already used
    for locals

## Return Strategy

- Prefer an internal hidden-result-slot convention.
- The caller provides destination scalar slots for the aggregate result.
- The callee writes its aggregate result into those slots before returning.
- Source-level aggregate return syntax remains unchanged even though the lowered
  calling convention is slot-based.

## Why This Direction

- It matches the current local aggregate implementation model.
- It avoids inventing a new general aggregate object representation too early.
- It keeps lower layers scalar-oriented and easier to validate.

## Frontend Work

### Parser

- Remove the current hard reject for aggregate parameters/returns once the
  implementation slice starts.
- Preserve honest diagnostics for still-unsupported neighboring forms:
  - aggregate arrays
  - unsupported field kinds

### Semantic

- Extend function type descriptors with aggregate parameter/return kinds.
- Validate:
  - declaration compatibility
  - definition compatibility
  - call argument compatibility
  - return expression compatibility
- Keep mismatch diagnostics aligned with the existing aggregate vocabulary.

## IR / Lower IR Work

- Record the source-level aggregate signature metadata explicitly.
- Expand aggregate boundary flow into hidden scalar params / return slots.
- Preserve current nested aggregate slot order exactly, so local and call-boundary
  aggregate layout stay aligned.

## ValueSSA / Machine Implications

- ValueSSA should see only scalarized slots on the first implementation slice.
- Machine lowering does not need a new aggregate object family if the scalarized
  lowering contract is preserved.
- The main verification risk is not codegen novelty, but keeping caller/callee
  slot order exactly consistent.

## Testing Plan

### Parser / Semantic

- Accept:
  - `int f(pair p){ return p.first; }`
  - `struct Point id(struct Point p){ return p; }`
  - nested aggregate parameter / return shapes already representable locally
  - parameter-to-return forwarding
- Reject:
  - mismatched declarations
  - mismatched calls
  - aggregate arrays
  - unknown struct types
  - direct aggregate call-result member access such as `mk().second`
    including parenthesized or nested-member variants

### Compiler / Runtime

- Parameter-first witnesses:
  - read fields from aggregate params
  - copy param into local
  - nested aggregate param field access
  - direct aggregate-returning call result into aggregate param
- Return witnesses:
  - return local aggregate
  - assign call result into local aggregate
  - nested aggregate return forwarding
  - parameter-to-return forwarding
  - first two-hop aggregate return/forwarding chains

### Downstream

- Add focused IR / lower IR / ValueSSA / machine-ir / machine-select locks that
  check:
  - stable hidden slot order
  - stable caller/callee slot counts
  - no accidental opaque aggregate object introduction

## Implementation Risks

- Signature compatibility drift between source-level aggregate signatures and
  lowered hidden scalar signatures.
- Hidden result-slot order mismatch on nested aggregates.
- Call lowering bugs where argument evaluation order and aggregate slot copy
  order diverge.
- Over-eager optimization folding that obscures real caller/callee slot bugs in
  early regression coverage.

## Exit Criteria For Phase 1

- Aggregate parameters compile and run for basic and nested cases.
- Parser / semantic / compiler / IR / lower IR / ValueSSA / machine-ir /
  machine-select / runtime regressions are all present.
- No new aggregate object representation exists below the current scalar-slot
  lowering boundary.

## Exit Criteria For Phase 2

- Aggregate returns compile and run for basic and nested cases.
- Caller/callee slot contracts are regression-locked.
- The current aggregate locals, aggregate parameters, and aggregate returns all
  share one documented recursive slot-layout rule.
- The kept non-goals around direct call-result member access and other plain
  aggregate value positions are regression-locked too.
