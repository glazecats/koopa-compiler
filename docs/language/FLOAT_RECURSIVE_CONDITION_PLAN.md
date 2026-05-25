# Float Recursive Condition Plan

## Goal

- Define a possible next narrow float opening after the current recursive
  pure-float tree checkpoints.
- Specifically evaluate whether helper-backed recursive pure-float trees such
  as `(x + y) + z` and `-a * (b / c)` should be allowed in control-flow
  condition positions.

## Why This Next

- The current recursive pure-float tree line is now broadly checkpointed in
  value-producing contexts:
  - `return`
  - same-type float call arguments
  - same-type float local assignment / initializer
  - same-type float comparisons, including wider operator coverage
- The most obvious still-closed nearby boundary is control-flow conditions.
- Current direct probes already show that this boundary is honest and stable:
  - `if ((x + y) + z)` rejects
  - `while (-a * (b / c))` rejects
  - both currently report `SEMA-EXT-036`

## Current Boundary

Today float control-flow conditions accept only the already-landed
direct-transport family:

- identifier
- direct call
- float literal
- logical condition composition built from those already-open condition roots

Current recursive or helper-backed float value trees do **not** count as
condition roots, even if they are already accepted elsewhere as float values.

Representative current rejects:

```c
int f(float x, float y, float z) {
  if ((x + y) + z) return 1;
  return 0;
}
```

```c
int f(float a, float b, float c) {
  while (-a * (b / c)) return 1;
  return 0;
}
```

Both should remain on `SEMA-EXT-036` until an explicit decision is made.

## Recommendation

If this line is opened, keep it **narrower than “all float expressions are
condition roots.”**

Recommended first opening:

- allow only helper-backed recursive pure-float trees already accepted as
  same-type float values
- reuse the same truthiness lowering rule already used for direct-float
  conditions:
  - truthy iff `(bits & 0x7fffffff) != 0`
- keep mixed-root, ternary-derived arithmetic neighbors, and unsupported
  float-expression families closed

## Proposed Accepted Family

Possible first accepted shapes:

```c
if ((x + y) + z) { ... }
while (-a * (b / c)) { ... }
for (; (x + y) + z; ) { ... }
```

Only when:

- every recursive subtree is already in the accepted same-type float
  arithmetic family
- no implicit conversion is required
- no ternary-derived arithmetic reopening is needed

## Explicit Non-Goals

Even if this slice opens, these should stay closed:

- `if ((g ? h : h) + h)`
- `while ((g ? h : h) * h)`
- mixed scalar arithmetic roots such as `if (x + 1)`
- accidental generalization to arbitrary float-valued expression roots

That keeps the opening aligned with the current honesty boundary instead of
silently converting `SEMA-EXT-036` into “almost anything float works in
conditions now.”

## Diagnostics

No new diagnostic family is needed initially.

- accepted shapes simply compile
- still-unsupported condition roots continue to reject with `SEMA-EXT-036`

This preserves one honest user-facing story:

- direct / explicitly-open float condition roots compile
- everything else remains outside the current feature

## Lowering

No new truthiness model is needed.

The current direct-float condition lowering rule should be reused:

- evaluate the float value to its 32-bit payload
- normalize truthiness through:
  - `(bits & 0x7fffffff) != 0`

This keeps:

- `+0.0` false
- `-0.0` false
- any nonzero payload true

## Testing Plan

If implemented, lock one representative per surface:

- semantic
- compiler
- IR
- lower-IR
- default `ValueSSA`
- `machine_ir`
- `machine_select`

Suggested initial witnesses:

```c
int f(float x, float y, float z) {
  if ((x + y) + z) return 1;
  return 0;
}
```

```c
int f(float a, float b, float c) {
  while (-a * (b / c)) return 1;
  return 0;
}
```

Suggested explicit neighbor rejects to keep:

```c
int f() {
  float g = 1.25;
  float h = 2.5;
  if ((g ? h : h) + h) return 1;
  return 0;
}
```

## Landing Decision

- Do **not** implement this blindly.
- Prefer waiting until the current long-running backend/default aggregation
  confidence is clearer.
- Once the current float line is judged plateau-stable, this is the most
  natural next narrow value-producing opening to consider.
