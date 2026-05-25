# Captured Defer Plan

## Goal

- Add a later defer variant that snapshots selected values at registration
  time.
- Keep it separate from the current `defer` and `fndefer` slices.

## Why It Exists

- It would support per-iteration cleanup patterns without relying on
  loop-local lifetime at unwind time.
- It is closer to capture semantics than to ordinary scope-exit defer.

## First Intended Shape

- first landed slice now uses explicit capture-by-value bindings:

```c
capdefer (y = x, z = a[i]) putint(y + z);
```

- current first-version meaning:
  - capture expressions are evaluated immediately at registration time
  - each captured value is stored in a hidden function-local snapshot slot
  - the payload still runs at function exit, in the same function-exit unwind
    phase as `fndefer`
  - payload reads of capture names observe the captured snapshot values, not
    later mutations of the original source expressions
- current first-version safety boundary:
  - capture expressions may reference only globals, parameters, and
    function-top-level locals
  - payload currently supports only expression / `if` / compound statements
  - payload may not contain nested `defer` / `fndefer` / `capdefer`,
    `return`, `break`, or `continue`

## Non-Goals

- full closures
- capture-by-reference environments
- escaping callbacks
- runtime heap environments for now

## Suggested Order

1. finish the current `defer` / `fndefer` slices
2. finish the current `TYPE_SYSTEM_PLAN` follow-ups
3. revisit this only after the capture/closure story is ready

## Clarified Use Case

- the motivating loop example is expected to keep each iteration's value
  snapshot alive until function exit
- that requires registration-time capture, not exit-time lookup

## Current Status

- parser / semantic / canonical-IR lowering now support the first
  `capdefer(...) stmt` slice under `-extension`
- current landed slice is intentionally narrower than closures:
  it snapshots only explicit scalar capture bindings and replays them through
  hidden function-local snapshot slots at function-exit unwind time
