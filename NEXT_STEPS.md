# Compiler Lab Next Steps

## Current Guidance (agreed)

1. Keep moving on semantic analysis and AST expansion first.
2. Defer parser/AST internal decoupling to the next small milestone.
3. Defer `-fanalyzer` leak-warning cleanup to a later static-analysis pass.

## Why this order

- `-fanalyzer` warning around `external.name` is currently treated as likely false positive.
- Runtime tests and ASan runs did not reproduce a real leak.
- Parser including internal AST helpers is a maintainability risk, not a functional blocker.

## Near-Term Milestone Plan

### Milestone A: Semantic + AST expansion (now)

- Expand AST coverage beyond top-level externals.
- Add semantic checks incrementally with regression tests.
- Keep parser/legacy-link behavior stable while expanding features.

### Milestone B: Internal decoupling (next small milestone)

- Reduce parser dependence on internal AST helpers.
- Preserve old API link behavior and test coverage.
- Keep changes minimal and non-semantic.

### Milestone C: Static-analysis cleanup

- Re-run strict static analysis (`-fanalyzer` and strict warning sets).
- Address confirmed warnings and document any accepted false positives.
- Verify with ASan/regression suite.

### Milestone D: Strict Path Semantics (deferred)

- Define strict return-path policy for non-termination paths.
- Replace current structured approximation with stricter path reasoning.
- Add dedicated regressions for loop non-termination/break/continue edge paths.

## Guardrails

- Do not prioritize low-risk hygiene over semantic feature progress.
- Every functional change should come with a regression test.
- Keep `make test` green at each step.

## Execution Log

- 2026-03-19: Milestone checkpoint committed (`b73b248`) with AST foundation, parser AST API, semantic pipeline, legacy-link test, and semantic regressions.
- 2026-03-19: Milestone A continued: expanding AST detail by recording function parameter counts in top-level AST externals.
- 2026-03-19: Milestone A continued: recording declaration initializer metadata (`has_initializer`) for top-level AST declaration externals.
- 2026-03-19: Milestone A continued: parser now accepts function declarations (`int f(int);`) and AST records function declaration/definition metadata (`is_function_definition`).
- 2026-03-19: Milestone A continued: AST now records function-body `return_statement_count`, and semantic analysis rejects function definitions without any `return`.
- 2026-03-19: Milestone A continued: strengthened return semantic from "at least one return" to "all control-flow paths return", with parser metadata (`returns_on_all_paths`) and regressions.
- 2026-03-19: Milestone A continued: refined all-path return approximation with fallthrough tracking and conservative handling of syntactic infinite loops (`while(1)`, `for(;;)`) so unreachable trailing `return` does not satisfy the rule.
- 2026-03-19: Milestone A continued: added `break`/`continue` keyword and parser support, including loop-context validation and flow propagation so `while(1){break;} return ...` is treated as reachable while break/continue outside loops are rejected.
- 2026-03-19: Milestone A continued: AST now records per-function loop statement count (`loop_statement_count`) with parser regression coverage and dump support.
- 2026-03-19: Milestone A continued: AST now records per-function if statement count (`if_statement_count`) with parser regression coverage and dump support.

## Current Milestone A Focus

- Keep extending AST metadata in small verified steps.
- Pair each AST extension with parser regression coverage and `make test` verification.
- Expand from top-level metadata toward minimal function-body semantic signals.

## Known Limitations (Current Behavior)

- Return all-path analysis is still a structured approximation, not a full path solver.
- Example currently accepted by design: `int f(int a){while(1){if(a){break;}}return 1;}`.
- Rationale: parser flow marks loop body as potentially breaking, so a fallthrough path to trailing `return` is considered reachable.
- If we later adopt stricter semantics that treat non-termination paths as non-returning failures, this loop/break interaction is a priority tightening point.
- `loop_statement_count` / `if_statement_count` are syntactic occurrence counters, not reachability-aware path counters.
- Unreachable loop/if statements are still counted by current metadata collection.
