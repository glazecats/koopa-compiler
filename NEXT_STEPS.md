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

## Guardrails

- Do not prioritize low-risk hygiene over semantic feature progress.
- Every functional change should come with a regression test.
- Keep `make test` green at each step.
