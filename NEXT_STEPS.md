# Compiler Lab Next Steps

## Current Guidance (agreed)

1. Treat Semantic S2 migration as complete and move active implementation to S3 scope semantics.
2. Keep callable/return behavior and diagnostics stable while introducing scope rules incrementally.
3. Keep parser/AST internal decoupling and static-analysis hygiene as follow-up milestones after S3 baseline lands.

## Why this order

- S1+S2 AST-primary cutover is now in place for active definition paths (callable + return-flow gates).
- Next highest-value gap is missing scope semantics (duplicate locals / undeclared local use / block shadowing behavior locks).
- `-fanalyzer` warning around `external.name` is currently treated as likely false positive.
- Runtime tests and ASan runs did not reproduce a real leak.
- Parser including internal AST helpers is a maintainability risk, not a functional blocker.

## Near-Term Milestone Plan

### Milestone A: Semantic + AST expansion (now)

- Status snapshot:
- S1 statement AST landing is complete.
- S2 callable/control-flow AST-primary cutover is complete.
- Active subphase is S3 scope semantics and metadata retirement stabilization.
- Current objective:
- Land minimal block-scope semantic rules with strict regression locks and no callable/CF behavior drift.

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
- 2026-03-23: Milestone A (Expression Tree A12) continued: added postfix function-call expression support in AST and translation-unit parser paths (including argument-list parsing), added positive/negative parser regressions, and synchronized lexer/parser smoke fixtures.
- 2026-03-23: Milestone A (Expression Tree A13) continued: added compound assignment operator support (`+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `^=`, `|=`) in lexer and assignment parser layers, added translation-unit/expression-AST regressions, and synchronized smoke fixtures.
- 2026-03-24: Milestone A (Semantic A1) continued: added baseline callable semantic check by recording called function names from function bodies and rejecting calls to undeclared functions in semantic analysis, with dedicated semantic regressions.
- 2026-03-24: Milestone A (Semantic A2) continued: refined callable diagnostics to distinguish `undeclared function` from `call to non-function symbol`, with dedicated semantic regression coverage.
- 2026-03-24: Milestone A (Semantic A3) continued: callable semantic diagnostics now report call-site line/column by carrying parser call metadata (`name + line + column`) into AST externals, with regression lock for location accuracy.
- 2026-03-24: Milestone A (Semantic A4) continued: added minimal callable argument-count check (call arity must match declared function parameter count), with dedicated semantic regression coverage.
- 2026-03-24: Milestone A (Semantic A5) hardening: fixed parser call-metadata realloc failure safety (no stale-pointer cleanup path), and locked current chained-call semantic behavior as known limitation.
- 2026-03-24: Milestone A (Semantic A6) continued: tightened minimal callable semantics to reject indirect/chained call-result forms (e.g., `f()(1)`) as `SEMA-CALL-005: call result is not callable`, with regression lock.
- 2026-03-24: Milestone A (Semantic A7) hardening: fixed parenthesized call-result bypass so `(f())()`, `(f(1))(2)`, and `((f(1)))(2)` are consistently rejected under the `SEMA-CALL-005` callable policy, with dedicated semantic regression locks.
- 2026-03-24: Milestone A (Semantic A8) continued: callable checks now enforce declaration-before-use visibility order (prior declarations/prototypes required), with forward-call regression locks.
- 2026-03-24: Milestone A (Semantic A9) continued: refined forward-call diagnostics to distinguish `before declaration` from truly `undeclared function`.
- 2026-03-24: Milestone A (Semantic A10) continued: forward-call diagnostics now include first declaration location (`line:column`) for faster triage.
- 2026-03-24: Milestone A (Semantic A11) continued: split indirect-call diagnostics into two categories: `Call result is not callable` and `Non-identifier callee not supported`, with regression coverage.
- 2026-03-24: Milestone A (Semantic A12) continued: unified callable diagnostics under stable error-code prefixes (`SEMA-CALL-001..006`) for IDE/script matching.
- 2026-03-24: Milestone A (Semantic A13) continued: aligned TU callable-shape recognition with expression parsing for parenthesized call-result chaining forms (`((f)(1))(2)`, `((f)())()`), with parser+semantic regression locks.
- 2026-03-24: Milestone A (Semantic A14) continued: enabled parser-to-semantic routing for parenthesized non-identifier callees (for example `(f+1)()`), so these forms now fail with `SEMA-CALL-006` in normal analysis flow rather than requiring synthetic metadata-only coverage.
- 2026-03-25: Milestone A (Semantic A15) continued: added boundary regressions that lock parser/semantic split for non-identifier callees: parenthesized forms reach semantic and report `SEMA-CALL-006`, while non-parenthesized forms remain parser-rejected.
- 2026-03-25: Milestone A (Semantic A16) continued: added a table-driven callable diagnostic matrix lock (`SEMA-CALL-001..006`) covering representative direct-call, call-result, and non-identifier callee forms.
- 2026-03-25: Milestone A (Semantic A17) continued: expanded table-driven `SEMA-CALL-006` coverage with additional parenthesized non-identifier callee shapes (nested parentheses and logical-expression forms) to further harden parser/semantic boundary guarantees.
- 2026-03-25: Milestone A (Semantic A18) continued: added a table-driven callable accept matrix for semantic-pass baselines (direct calls, prior prototypes, and parenthesized direct-identifier callees) to complement the existing `SEMA-CALL-001..006` failure matrix.
- 2026-03-25: Milestone A (Parser A19) continued: added table-driven parser callable entry matrices for both translation-unit and expression-AST entrypoints, locking accepted vs rejected callee-shape boundaries in a single consolidated suite.
- 2026-03-25: Milestone A (Semantic A20) continued: upgraded callable failure matrix assertions from code-only checks to `error-code + key diagnostic snippet + call-site line/column` checks for stronger IDE/script stability guarantees.
- 2026-03-25: Milestone A (Semantic A21) continued: enriched `SEMA-CALL-004` payload to include explicit expected vs actual argument counts (`expected N, got M`) and locked this wording in matrix regressions.
- 2026-03-25: Milestone A (Semantic A22) continued: enriched `SEMA-CALL-002` with stable declaration-location key fields (`decl_line`, `decl_col`) while preserving existing human-readable `first declaration at L:C` wording.
- 2026-03-25: Milestone A (Semantic A23) continued: unified `SEMA-CALL-001..006` diagnostics with stable key-value payload fragments (`callee`, `decl_line/decl_col`, `expected/got`, `callee_kind`) and updated matrix assertions accordingly.
- 2026-03-25: Milestone A (Semantic A24) hardening: increased semantic diagnostic buffer capacity, removed redundant `SEMA-CALL-004` payload duplication, and added long-identifier regression lock to prevent message-tail truncation (`expected/got` fields must survive).
- 2026-03-25: Milestone A (Statement AST S1-1) continued: landed minimal function-body statement AST structure (`compound/declaration/expression/if/while/for/return/break/continue`) and attached `function_body` to function-definition externals while preserving existing parser metadata behavior.
- 2026-03-25: Milestone A (Statement AST S1-2) continued: statement AST nodes now capture key expression subtrees (`expression`, `return`, `if` condition, `while` condition, `for` init/condition/step) via token-slice expression-AST reconstruction, with no semantic behavior changes.
- 2026-03-25: Milestone A (Statement AST S1-3) continued: added explicit statement expression-slot metadata (`primary`, `condition`, `for_init`, `for_step`) and parser regression locks for slot/index stability; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-boot) started: semantic now performs statement-AST callable shadow traversal and cross-checks parser callable metadata (`name/line/column/arg_count/callee_kind`) for internal consistency (`SEMA-INT-001..003`), while keeping existing callable diagnostics/behavior unchanged; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-2) continued: callable semantic checks now consume an AST-primary call view (with parser-metadata fallback when statement AST body is unavailable), preserving existing `SEMA-CALL-001..006` behavior and diagnostics; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-3) continued: tightened AST-primary migration gate so function definitions must provide statement AST body (`SEMA-INT-005` on contract breach), and updated semantic regressions accordingly; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-4) continued: made callable-view source explicit in semantic internal data flow (`AST-primary` vs `metadata-fallback`) and added a definition-path guard (`SEMA-INT-006`) to ensure function-definition callable checks never run on fallback view; full `make test` remained green.
- 2026-03-25: Milestone A (Semantic S2-5) continued: relaxed hard dependency on parser callable metadata for AST-primary function definitions by making metadata parity check a shadow toggle, and added semantic regression lock proving AST-primary callable results remain correct even when parser metadata is tampered; full `make test` remained green.
- 2026-03-27: Milestone A (Semantic S2-6) continued: replaced single macro parity toggle with explicit migration-strategy constant (`STRICT_PARITY` / `AST_PRIMARY_ONLY`) so S2 policy switching is centralized and does not require scattered preprocessor edits; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-7) continued: introduced reusable semantic AST post-order walk helpers and moved AST-primary callable rule evaluation to visitor-driven checks (keeping metadata fallback path for non-body contexts), reducing intermediate call-list coupling while preserving existing `SEMA-CALL-001..006` behavior; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-7) hardening: added regression lock proving AST-primary visitor callable checks are authoritative for chained-call rejection (`SEMA-CALL-005`) even when parser callable metadata is intentionally tampered.
- 2026-03-29: Milestone A (Semantic S2-8) continued: switched function all-path-return semantic gating to AST-primary statement-flow evaluation (metadata used only as optional strict-parity guard), and added tamper regression lock proving return rejection remains correct when `returns_on_all_paths` parser metadata is intentionally forced; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-9) cleanup: removed currently unreachable callable metadata-fallback/guard branches (`SEMA-INT-004`, `SEMA-INT-006`) from active callable-check path and made function-definition callable checks explicitly AST-primary, reducing maintenance ambiguity without behavior change; added explicit regression lock that direct chained form `a()()` remains rejected as `SEMA-CALL-005`; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S2-10) consistency: converged callable migration notes with current implementation by documenting function-definition callable checks as AST-primary-only and marking `SEMA-INT-004`/`SEMA-INT-006` as retired from active callable path (historical-only), reducing roadmap/code interpretation drift; full `make test` remained green.
- 2026-03-29: Milestone planning refresh: marked S2 as closed for implementation tracking, shifted active plan to S3 scope semantics, and rewrote near-term plan blocks to separate completed work from remaining tasks.

## Current Milestone A Focus

- Implement S3-1 scope baseline on statement AST traversal:
- reject duplicate local declarations in same block scope,
- reject undeclared local variable uses in expression paths,
- allow block shadowing by default and lock behavior in tests.
- Keep callable matrix and control-flow matrix behavior unchanged while scope rules are introduced.
- Maintain AST-primary authority for definitions and keep parser metadata use non-authoritative.

## S2 Closure Snapshot

- Completed: AST-primary callable checks for definitions, AST-primary return-path gate, migration strategy constants, visitor-based callable traversal, chained-call behavior locks (`a()()` included).
- Retired from active definition-path callable flow: `SEMA-INT-004`, `SEMA-INT-006` (historical only).
- Remaining under S3+ scope: introduce scope rules, then continue controlled metadata dependency retirement.

## Active Implementation Plan (S3 next)

1. S3-1 (scope core): introduce block scope stack in semantic traversal and enforce duplicate-local rejection in same scope.
2. S3-1 (scope core): enforce undeclared local variable rejection for identifier expression uses that are not covered by current top-level callable checks.
3. S3-1 (behavior lock): explicitly allow inner-block shadowing and lock with targeted semantic regressions.
4. S3-2 (integration): run scope checks over statement AST traversal while preserving current callable and CF diagnostic outputs.
5. S3-3 (cleanup): retire first metadata family only after scope + callable + CF matrices are green (`called_function_*` semantic dependency remains parity-only).
6. S3-4 (cleanup): retire `returns_on_all_paths` semantic dependency usage where still present in active paths.
7. Validation gate: `make test` must remain green at each substep before proceeding.
8. Deferred tightening (Milestone D handoff): flip CF-02/CF-03/CF-06 expectations only after S3 stabilization.

## Metadata Migration Schedule (anti-coupling plan)

1. S1 status: complete (metadata kept as compatibility rails).
2. S2 status: complete for active definition paths (AST-primary callable + return-flow).
3. S3 rule: new scope semantics must be computed from statement AST traversal, not parser metadata.
4. S3 cleanup gate: begin retirement only after scope/callable/CF matrices are simultaneously green.
5. Retirement order (current):
6. first retire `called_function_*` semantic authority (keep optional parity checks only if needed),
7. then retire `returns_on_all_paths` semantic authority,
8. then retire statement counters (`loop/if/break/continue/declaration`) from semantic dependencies.
9. Safety rule: retire one metadata family per change slice and run full `make test` before next retirement.
10. Rollback rule: if drift appears, restore previous gate and keep parity rails until root cause is fixed.

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
- Current assignment-lvalue policy is intentionally identifier-only in parser expression checks (`=` and compound assignments); parenthesized identifiers and other non-identifier lvalue forms for assignment remain rejected in this subset.
- Increment/decrement policy currently allows identifier and parenthesized-identifier operands (e.g., `++(a)`, `(a)++`) but still rejects broader lvalue forms.
- Low-priority accepted limitation: in translation-unit parsing, assignment-style forms rejected by identifier-only lvalue policy (e.g., `(a)=b`, `(a)+=b`) may surface syntax-oriented diagnostics such as `Expected ';'` instead of semantic-style `lvalue` wording.
- Callable semantic limitation (current policy): minimal callable analysis only supports direct identifier callee forms. Call-result/chained forms (for example `f()(1)`, `a()()`, `((f)(1))(2)`) are explicitly rejected as `SEMA-CALL-005` with `callee_kind=call_result`.
- Scope note for `SEMA-CALL-006`: this code applies to the parser-accepted subset of non-identifier callees that reaches semantic callable checks (for example `(f+1)()`), reported with `callee_kind=non_identifier`. Non-parenthesized non-identifier callees outside this subset can still be rejected earlier by parser syntax checks.
