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
- 2026-03-21: Milestone A continued: AST now records per-function break/continue statement counts (`break_statement_count`, `continue_statement_count`) with parser regression coverage and dump support.
- 2026-03-21: Milestone A continued: AST now records per-function declaration statement count (`declaration_statement_count`) with parser regression coverage and dump support.
- 2026-03-21: Milestone A (Expression Tree A1) started: added minimal standalone expression AST parsing for primary + parenthesized expressions with dedicated parser regressions and no semantic rule changes.
- 2026-03-21: Milestone A (Expression Tree A2) continued: standalone expression AST now covers multiplicative/additive binary expressions with precedence + left-associativity regressions.
- 2026-03-21: Milestone A (Expression Tree A3-partial) continued: standalone expression AST now covers relational expressions (`<`, `<=`, `>`, `>=`) with precedence/associativity regressions; equality/assignment remained deferred at this checkpoint.
- 2026-03-21: Milestone A (Expression Tree A3) continued: standalone expression AST now covers equality expressions (`==`, `!=`) and assignment expressions (`=`) with precedence/associativity regressions.
- 2026-03-21: Milestone A (Expression Tree A4) continued: standalone expression AST now covers unary prefix expressions (`+`, `-`) with unary-chain and precedence regressions.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary edge-case regressions for assignment RHS (`a=-b`), parenthesized additive operand (`-(a+b)`), and multiplicative right unary (`a*-b`).
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary negative regressions for incomplete operands (`a=+`, `a*-`, `-(a+)`) with primary-expression diagnostics.
- 2026-03-21: Milestone A hygiene: expression regression helper now targets assignment main entrypoint directly; added explicit equality-alias forwarding compatibility lock.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added assignment left-hand-side boundary regressions (`(a)=b`, `1=b`, `a+b=c`) to lock current identifier-only assignment acceptance.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added assignment/equality mix boundary regressions (`a=b==c`, `a==(b=c)`, reject `a==b=c`) to lock precedence and boundary behavior.
- 2026-03-21: Milestone A (Expression Tree A4) continued: expanded assignment LHS boundary locks with nested/parenthesized cases (`(a=b)=c`, `((a))=b`, `a=(b==c)=d`) to prevent accidental lvalue broadening.
- 2026-03-21: Milestone A hygiene: standardized assignment-boundary diagnostics in regressions to assert `EOF` + `ASSIGN` together for clearer failure localization.
- 2026-03-21: Milestone A hygiene: lightly grouped expression regression sections (positive/compatibility/negative) for easier maintenance without behavior changes.
- 2026-03-21: Milestone A hygiene: extracted shared helper for assignment-failure expression regressions to reduce duplicated tokenize/parse/diagnostic boilerplate.
- 2026-03-21: Milestone A hygiene: migrated remaining expression failure templates (trailing tokens and deep-parentheses recursion-limit case) to the shared helper.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary logical-not (`!`) support in lexer/parser expression paths with parser/lexer regression coverage (`!a`, `a=!b`, and reject `a=!`).
- 2026-03-21: Milestone A hygiene: kept smoke fixtures in sync with new operator support by updating both `tests/lexer/test.c` and `tests/parser/test.c` to include unary `!` usage.
- 2026-03-21: Milestone A (Expression Tree A4) continued: added unary bitwise-not (`~`) support in lexer/parser expression paths with parser/lexer regression coverage (`~a`, `a=~b`, and reject `a=~`), and synced smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A5) continued: added logical-and/logical-or (`&&`, `||`) expression support with precedence/associativity/boundary regressions; lexer still rejects single `&` and `|` by design until bitwise layers are introduced.
- 2026-03-21: Milestone A (Expression Tree A6) continued: added bitwise-and/bitwise-xor/bitwise-or (`&`, `^`, `|`) expression support with precedence/boundary regressions, updated lexer tokenization for single-character bitwise operators, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A7) continued: added shift expression support (`<<`, `>>`) with additive/relational precedence and associativity regressions, lexer shift tokenization, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A8) continued: added conditional expression support (`?:`) with right-associativity and precedence regressions, added lexer tokens for `?`/`:`, and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A9) continued: added comma expression support (`,`) as lowest-precedence expression layer with precedence/associativity/boundary regressions and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A10) continued: added unary prefix increment/decrement support (`++`, `--`) in lexer/parser expression paths with regression coverage and synchronized smoke fixtures.
- 2026-03-21: Milestone A (Expression Tree A11) continued: added postfix increment/decrement support (`a++`, `a--`) in parser expression paths with AST/precedence/negative regressions and synchronized smoke fixtures.
- 2026-03-23: Milestone A hardening: tightened `++/--` operand acceptance to identifier-lvalue in both AST and translation-unit parser paths, added non-lvalue rejection regressions, and added lexer boundary-sequence locks for `<<=`, `>>=`, `&=`, `|=`, `^=` prior to compound-assignment support.

## Current Milestone A Focus

- Keep extending AST metadata in small verified steps.
- Pair each AST extension with parser regression coverage and `make test` verification.
- Expand from top-level metadata toward minimal function-body semantic signals.

## Expression Tree Increment Plan (next)

1. Step A1: add minimal expression AST nodes for primary + parenthesized expressions only.
2. Step A1 tests: add parser regressions that lock grouping behavior for `(a)` and nested parentheses like `((a))`.
3. Step A2: extend expression AST to multiplicative/additive layers while preserving current parser control flow and semantics.
4. Step A2 tests: add precedence/associativity regressions (`a+b*c`, `(a+b)*c`, `a-b-c`) and keep AST dump readable.
5. Step A3: extend to relational/equality/assignment expression nodes with the same one-layer-per-commit discipline.
6. Step A3 tests: add focused regressions for each newly enabled operator family before moving to the next layer.
7. Rule: expression-tree milestones must not change current semantic pass/fail rules unless explicitly planned in Milestone D.
8. Validation gate per step: `make test` must stay green before starting the next expression layer.

## Known Limitations (Current Behavior)

- Return all-path analysis is still a structured approximation, not a full path solver.
- Example currently accepted by design: `int f(int a){while(1){if(a){break;}}return 1;}`.
- Rationale: parser flow marks loop body as potentially breaking, so a fallthrough path to trailing `return` is considered reachable.
- Semantic regression matrix lock: CF-02 / CF-03 / CF-06 are currently expected to pass and are tagged as known-limitation guardrails.
- Future strict-path migration rule: CF-02 / CF-03 / CF-06 should be flipped to expected-fail together when Milestone D is implemented.
- If we later adopt stricter semantics that treat non-termination paths as non-returning failures, this loop/break interaction is a priority tightening point.
- `loop_statement_count` / `if_statement_count` are syntactic occurrence counters, not reachability-aware path counters.
- `break_statement_count` / `continue_statement_count` follow the same syntactic counting model.
- `declaration_statement_count` follows the same syntactic counting model.
- `declaration_statement_count` currently counts only declarations parsed as compound-block statements; `for(int i=...)` init declarations are excluded.
- Unreachable loop/if/break/continue/declaration statements are still counted by current metadata collection.
- Current lvalue policy is intentionally identifier-only in parser expression checks (`=` and `++/--`); parenthesized identifiers and other lvalue forms (e.g., `++(a)`, `(a)++`) are rejected in this subset.
