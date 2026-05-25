# Function-Exit Defer Plan

## Goal

- Add a second defer-like extension feature that runs on **function exit**
  rather than on ordinary block/scope exit.
- Keep it clearly separated from the existing `defer` semantics so users can
  choose between:
  - block/scope-exit cleanup
  - function-exit cleanup
- Avoid the full generality of closures/captures in the first version.

## Recommended Surface

- First concrete syntax proposal:

```c
fndefer stmt;
```

- Example:

```c
int main() {
  int fd = open();
  fndefer close(fd);

  if (fd < 0) return 1;
  return 0;
}
```

- Meaning:
  `stmt` is registered now, but executes only when the **current function**
  is about to return.

## Why This Is Different From `defer`

Current `defer`:

- binds to the innermost compound scope
- runs when that scope exits
- inside a loop body, can run every iteration

Function-exit defer (`fndefer`):

- binds to the current function
- runs only when the function exits
- does **not** fire at the end of every intermediate block or loop iteration

This makes it suitable for "whole-function cleanup" rather than "current block
cleanup".

## User-Facing Semantics

### Basic meaning

- `fndefer stmt;` registers `stmt` to run when the **current function exits**.
- Registered statements execute in **LIFO** order.
- They run on every function exit path:
  - explicit `return expr;`
  - explicit `return;`
  - fallthrough to implicit final return if that path is otherwise legal

### Ordering relative to ordinary `defer`

Recommended first-version rule:

- ordinary `defer` still runs when each scope is unwound
- after all ordinary scope unwinding relevant to the chosen return path is
  complete, the function's `fndefer` stack runs
- then the final return completes

So on an inner-block `return`, the order should be:

1. inner/block `defer`s
2. outer/block `defer`s
3. function-exit `fndefer`s
4. actual return

### Value-observation model

- Like current `defer`, `fndefer` should observe **exit-time state**, not a
  registration-time snapshot.
- This stays simple and consistent with the existing "as if copied onto exit
  edge" mental model.

## Scope / Variable Safety Rule

This is the key simplification for the first version.

`fndefer` payloads may reference only:

1. global variables
2. parameters
3. variables declared in the **function's top-level compound scope**
4. only those top-level-scope variables that were already declared **before**
   the `fndefer` registration point

### Explicitly rejected in first version

- block-local variables declared inside nested `{ ... }`
- loop-local variables such as `for (int i = ... )`
- declarations that appear later in the function than the `fndefer`
- any future capture-like semantics

### Why this rule helps

It avoids dangling-scope problems entirely:

- block locals may already be dead by the time the function exits
- later declarations do not yet exist at registration time
- top-level function locals and parameters stay live for the whole function

So this rule gives us a strong first semantic boundary without inventing a real
capture environment.

## Examples

### Allowed

```c
int g;

int f(int x) {
  int y = 0;
  fndefer putint(x);
  fndefer putint(y);
  fndefer putint(g);
  y = 3;
  return 0;
}
```

### Rejected: nested block local

```c
int f() {
  {
    int x = 1;
    fndefer putint(x); // reject
  }
  return 0;
}
```

### Rejected: later declaration

```c
int f() {
  fndefer putint(x); // reject
  int x = 1;
  return 0;
}
```

### Rejected: loop-local

```c
int f() {
  for (int i = 0; i < 3; i = i + 1) {
    fndefer putint(i); // reject
  }
  return 0;
}
```

## Mode Boundary

- First version should be accepted only under `-extension`.
- `-riscv` and `-perf` should reject it.

## Implementation Strategy

### Frontend

- Add a new keyword, recommended: `fndefer`
- Parse `fndefer stmt;` into its own AST statement kind or extension-origin
  marker

### Semantic

Need a dedicated checker for the payload statement that:

1. walks all referenced identifiers
2. classifies each referenced symbol as:
   - global
   - parameter
   - function-top-level local
   - nested/block local
3. verifies declaration order for function-top-level locals

Recommended first-version policy for payload contents:

- allow expression statements, assignments, calls, compounds, local `if`
- reject nested `return`, `break`, `continue`, and nested `fndefer` until a
  stronger story is chosen

### Lowering

Unlike ordinary `defer`, this should not attach to every compound scope.

Recommended lowering model:

- maintain a **function-exit defer list** in lowering context
- `fndefer` appends payloads to that function-level list
- on every final return path:
  - first run any ordinary scope `defer`s being unwound
  - then run function-exit `fndefer`s in reverse order
  - then emit final `ret`

This means the implementation can stay explicit and verifier-friendly without
building a runtime callback stack.

### Current implementation note

- The first landed slice now uses hidden per-site counters plus exit-time
  replay loops, so a dynamically re-executed `fndefer` site inside `if` /
  `while` / `for` now enqueues one function-exit action per execution.
- The current conservative boundary is still narrower than a full closure
  system:
  - `capdefer` dynamic registration remains rejected
  - multi-site loop-nested `fndefer` growth stays conservative for now
- The "Recommended first answer: yes" note in `Nested registration` is now
  realized for the single-site `fndefer` loop case that the live tree supports.

## Testing Plan

### Parser

- parse `fndefer putint(1);`
- parse `fndefer if (x) putint(1);`

### Semantic

- accept globals/parameters/top-level locals
- reject nested block locals
- reject later declarations
- reject loop-local captures
- reject outside `-extension`

### IR

- `fndefer` runs on explicit return
- multiple `fndefer`s are LIFO
- ordinary `defer` runs before `fndefer` on the same return path

### Runtime

- top-level local observed at function exit
- parameter observed at function exit
- mixed `defer` + `fndefer` ordering

## Non-Goals For First Version

- full lexical capture
- heap-lifted environments
- escaping callbacks/closures
- function-exit defers that can reference arbitrary nested locals
- exception/panic integration

## Open Questions

### Naming

- `fndefer` is explicit and implementation-friendly
- alternatives like `defer_return`, `onreturn`, or `finally` are possible, but
  they should be chosen deliberately before implementation begins

### Nested registration

- Should `fndefer` be allowed anywhere in the function as long as the payload
  only references permitted variables?
- Recommended first answer: yes
  because registration itself is conditional/execution-dependent, but payload
  references still stay safe under the top-level-only variable rule

### Interaction with `defer`

- The recommended ordering is:
  scope `defer`s first, function-exit `fndefer`s second
- This should be kept explicit in tests before landing implementation
