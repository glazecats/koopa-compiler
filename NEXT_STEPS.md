# Compiler Lab Next Steps

## Current Guidance (agreed)

1. Milestone A is closed for implementation tracking (semantic-authority retirement complete for callable/return/statement-counter metadata).
2. Milestone B is closed for implementation tracking (parser/AST decoupling and parser-side metadata-retention cleanup completed).
3. Milestone C baseline validation is complete (`-fanalyzer`/ASan/strict-warning reruns are green).
4. Milestone D front-end control-flow semantics is in maintenance mode only (reopen for concrete bug-driven findings).
5. Milestone E canonical IR bootstrap and precision-hardening work is considered complete for current scope.
6. Active implementation focus moves to pass-stage work on the existing canonical IR (analysis + transformation passes), not to a new peer IR layer.
7. If a lower IR is ever needed later, it must be introduced as serial lowering (`AST -> canonical IR -> lower/target IR`), never as multiple parallel AST-to-IR pipelines.

## Why this order

- Front-end authority and decoupling work (A/B/C/D) are already stable enough to avoid roadmap re-opening.
- Canonical IR v1 now has explicit CFG, verifier contracts, declaration/global semantics, and broad regression locks.
- `IR-LOWER-022` dependency analysis has moved beyond syntactic scanning and now includes several rounds of reachability precision tightening, reducing false positives.
- Highest-value next work is no longer adding IR surface area; it is controlled pass-stage evolution on top of the current IR to improve optimization readiness while preserving contracts.
- Delaying a second IR layer avoids premature complexity and keeps diagnostics, tests, and ownership concentrated around one canonical representation.

## Near-Term Milestone Plan

### Milestone A: Semantic + AST expansion (closed)

- Status snapshot:
- S1 statement AST landing is complete.
- S2 callable/control-flow AST-primary cutover is complete.
- S3 scope semantics and semantic-authority retirement stabilization is complete.
- Closure objective achieved:
- Semantic callable/return/scope behavior is AST-primary in active paths with full regression locks green.

### Milestone B: Internal decoupling (closed)

- Reduce parser dependence on internal AST helpers.
- Retire parser-side metadata retention touchpoints (`called_function_*`, `returns_on_all_paths`, statement counters) in controlled slices.
- Preserve old API link behavior and test coverage.
- Keep changes minimal and non-semantic.

### Milestone C: Static-analysis cleanup

- Status: completed baseline and rerun verification.
- `test-fanalyzer`, `test-asan`, and `test-strict-warnings` are green with zero warnings in current runs.

### Milestone D: Conservative Control-Flow Semantics (stabilized)

- Keep `SEMA-CF-001` focused on reliably provable must-not-return shapes.
- Preserve strict branch requirements for `if/else` and plain `if` without turning value-dependent loops into false positives.
- Status: stable enough for IR handoff; continue only as bug-driven semantic maintenance.

### Milestone E: Canonical IR v1 (closed for current scope)

- Canonical block-based non-SSA IR is established as the post-semantic handoff surface.
- Verifier baseline/hardening, declaration-awareness, global/runtime-init semantics, and dependency diagnostics are in place.
- Regression matrix for callable + CFG combinations and dependency precision guardrails is broad and green.

### Milestone F: Pass Pipeline on Canonical IR (active)

- F0: add a minimal pass execution framework over canonical IR (deterministic ordering, pass-level fail-fast diagnostics, no behavior change by default).
- F1: land first low-risk analysis pass(es) to support later transforms (for example local const-value/use info or side-effect classification).
- F2: land first low-risk transform pass(es) with strict safety rails (for example trivial constant folding / dead expression cleanup).
- F3: expand regression/verifier locks to keep pass outputs structurally valid and behavior-preserving.
- F4: only after measurable pass-stage pressure, evaluate whether SSA/lower IR is justified as a serial downstream stage.

## Guardrails

- Do not prioritize low-risk hygiene over semantic feature progress.
- Every functional change should come with a regression test.
- Keep `make test` green at each step.
- Keep IR work behind successful semantic analysis; IR generation must not become a second semantic authority surface.
- Do not build multiple peer IRs in parallel. If a lower IR is needed later, lower from the canonical IR rather than generating both from AST.
- Use stable internal numeric IDs for blocks, locals, and temporaries; human-friendly names such as `bb.1`, `a.2`, and `tmp.3` belong in dumps, not core identity.
- Pass-stage work must preserve canonical IR contracts: verifier must remain green before and after passes.
- Optimization passes must be introduced incrementally with behavior locks (regression cases proving no semantic drift).

## IR Design Direction

- IR v1 should be block-based, three-address, and non-SSA.
- Every function should own an explicit CFG of basic blocks, and every block should end in a terminator.
- Source-level shadowing must be resolved into distinct local entities before printing; different declarations with the same source name are not the same IR object.
- SSA-style versioning is a later optimization/normalization concern, not a v1 requirement.
- Optimization should first happen as passes over canonical IR; only introduce an additional lower IR if canonical-pass evolution shows clear structural limits.
- If the project later grows optimization or backend-specific needs beyond canonical IR limits, introduce an additional lower IR as a serial stage rather than replacing or bypassing the canonical IR.

## Document Scope Note

- `Current Guidance`, `Current Milestone Focus`, `Active Implementation Plan`, and `Known Limitations` are the current authority.
- Older milestone slices and the long execution log below are retained as historical reference, not as current policy.

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
- 2026-03-29: Milestone A (Semantic S3-1) started: parser now records function parameter names plus local declaration names on statement AST nodes (including `for(int i=...)` init declarations), and semantic now enforces baseline scope rules (`SEMA-SCOPE-001` duplicate-local-in-scope reject, `SEMA-SCOPE-002` undeclared-identifier-use reject) while preserving block shadowing acceptance; added parser + semantic regression locks and kept full `make test` green.
- 2026-03-29: Milestone A (Semantic S3-2) continued: aligned function-scope integration so parameters and function-body top-level declarations share the same scope frame (rejecting parameter redeclaration in the function body), and expanded scope regressions for `for` init lifetime and shadowing interactions; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: expanded scope regression locks for duplicate function-parameter names, block-local lifetime boundary (post-block use reject), for-body local lifetime boundary (post-loop use reject), and inner-block parameter shadowing acceptance; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) bugfix: declaration initializer expressions are now preserved in statement AST for both block declarations and `for(int ... )` init declarations, so scope traversal correctly rejects undeclared-identifier uses inside initializer expressions (`SEMA-SCOPE-002`); added parser + semantic regression locks for this path and kept full `make test` green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added callable/scope boundary enforcement so direct-identifier callees shadowed by local declarations (including parameters, local block declarations, and `for` init declarations) are rejected as `SEMA-CALL-003` (non-function symbol), and expanded callable diagnostic/pass matrices in one batch to lock these shadowing paths; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added a callable shadow precheck pass so local non-function callee shadowing is diagnosed as `SEMA-CALL-003` before top-level callable lookup (closing `CALL-001/002` precedence drift in local-shadow cases), and batch-expanded callable matrix coverage for no-top-level/later-declaration shadow variants plus non-identifier stability (`SEMA-CALL-006` unchanged); full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: added diagnostic-ordering regression locks for mixed-error call sites (ensuring `SEMA-CALL-003/005/006` remain prioritized when applicable before scope undeclared-identifier checks, while declared-callee + undeclared-arg still reports `SEMA-SCOPE-002`), keeping callable/scope layering deterministic; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) hardening: batch-expanded callable/scope interaction matrix for declaration-initializer and loop-slot expressions (`for` condition/step), including ordering locks that keep `SEMA-CALL-003/004/005/006` precedence stable in those slots and fallback locks that keep declared-callee + undeclared-arg paths on `SEMA-SCOPE-002`; full `make test` remained green.
- 2026-03-29: Milestone A (Semantic S3-2) bugfix/hardening: scope traversal now evaluates each declarator initializer before introducing that declarator name (including `for(int ... )` init declarations), closing same-declaration forward-reference leaks such as `int x=y,y=1` and `for(int i=j,j=0;...)`; parser initializer slots are now aligned 1:1 with declarator order (nullable for no-initializer declarators), and parser+semantic regressions were batch-expanded for reject/accept ordering variants; full `make test` remained green.
- 2026-03-29: Milestone A (Parser ergonomics) low-priority hardening: clarified expression recursion-limit diagnostic wording to explicitly state it is call-depth-frame based (`%zu call-depth frames`) so deep-parentheses failures are interpreted correctly; behavior unchanged and parser regressions remained green.
- 2026-03-29: Milestone A stage-gate: revalidated full suite (`make test`) with scope/callable/CF matrices green after latest S3-2 hardening slices, and marked S3-2 complete for milestone tracking; active work now transitions to S3-3 cleanup.
- 2026-03-30: Milestone A (Semantic S3-3) first cleanup slice landed: retired callable metadata parity rails from active semantic callable path by removing parser `called_function_*` consistency cross-check (`SEMA-INT-001/002/003`) for function-definition callable analysis; callable rules now remain AST-primary only in active flow, and full `make test` stayed green.
- 2026-03-30: Milestone A (Semantic S3-3) callable-retirement closure: verified no remaining `called_function_*` references in semantic implementation (`src/semantic/semantic.c`) and revalidated full suite (`make test`) with callable/scope/CF behavior unchanged; mark `called_function_*` semantic-authority retirement complete and transition active cleanup to S3-4 (`returns_on_all_paths`).
- 2026-03-30: Milestone A (Semantic S3-4) first cleanup slice landed: removed `returns_on_all_paths` parser-metadata parity gate from semantic return-path analysis (`SEMA-INT-008` rail retired), keeping return enforcement AST-primary from statement-flow results only; revalidated full suite (`make test`) with unchanged callable/scope/CF outcomes.
- 2026-03-30: Milestone A (S3 post-cleanup verification) completed: confirmed semantic implementation has zero references to callable metadata (`called_function_*`), return metadata (`returns_on_all_paths`), and statement-counter metadata (`loop/if/break/continue/declaration counts`); S3 semantic-authority retirement goals are complete and handoff focus moves to Milestone B decoupling.
- 2026-03-30: Milestone A tracking correction (historical, superseded by next clarification): S3 remains in progress; semantic-authority retirement is complete, but parser-side metadata retention cleanup (callable/return/counter legacy collection and compatibility touchpoints) is still pending before S3 closure.
- 2026-03-30: Milestone A tracking clarification: S3 closure criterion follows semantic-authority retirement scope; parser-side metadata retention cleanup is classified as Milestone B decoupling work and does not block S3 completion.
- 2026-03-30: Milestone transition checkpoint: Milestone A is closed for implementation tracking (semantic-authority retirement complete and validated), active implementation is switched to Milestone B, and a detailed B execution plan is recorded below as the next working contract.
- 2026-03-30: Roadmap consistency sweep: normalized top-level guidance and near-term milestone states so document-wide wording reflects the same boundary (`A closed`, `B active`).
- 2026-03-30: Milestone B (B0.5) completed: split oversized regression suites into aggregator + themed include fragments (`tests/parser/parser_regression_test.c` + `tests/parser/parser_regression_cases_*.inc`, `tests/semantic/semantic_regression_test.c` + `tests/semantic/semantic_regression_*.inc`) while preserving original case execution order; full `make test` remained green.
- 2026-03-30: Milestone B (B0.5) refinement: further split parser expression-heavy fragment into smaller themed files (`parser_regression_cases_expr_ast_a.inc`, `parser_regression_cases_expr_ast_b.inc`, `parser_regression_cases_ast_meta.inc`) and updated aggregator includes; full `make test` remained green.
- 2026-03-30: Milestone B (B0.5) maintainability refinement: unified parser split-fragment IntelliSense prelude into one shared include (`parser_regression_intellisense_prelude.inc`), removed duplicated local macro/type guards across parser fragments, and added explicit split-fragment inventory comments in parser/semantic regression entry files for safer review and future edits; full `make test` remained green.
- 2026-03-30: Milestone B (B1) completed: retired parser-side callable metadata retention interfaces by removing `called_function_*` fields from `AstExternal`, removing parser AST-output plumbing for callable metadata in translation-unit AST assembly, and updating semantic regressions to stop tampering retired callable metadata fields while preserving AST-primary callable behavior assertions; full `make test` remained green.
- 2026-03-30: Milestone B (B2) completed: retired parser-side return all-path metadata retention by removing `returns_on_all_paths` from `AstExternal`, removing parser return-flow write plumbing from function-external/TU-AST assembly, and aligning parser/semantic regressions plus parser AST dump expectations to the AST-primary return-flow contract; full `make test` remained green.
- 2026-03-30: Milestone B (B3) completed: retired parser-side statement-counter retention by removing `loop/if/break/continue/declaration` counter fields from `AstExternal`, removing parser accounting/write paths and parser dump output for these counters, and pruning parser regressions that specifically asserted retired metadata while keeping AST-primary parse/semantic behavior checks; full `make test` remained green.
- 2026-03-30: Milestone B (B4) completed: removed parser dependency on `ast_internal.h` by introducing parser-local AST lifecycle/add-external helpers (`parser_ast_*`) and routing parser AST ownership operations through that local compatibility layer, preserving legacy parser-only link behavior (`parser_legacy_link_test`) while reducing parser/AST internal coupling; full `make test` remained green.
- 2026-03-30: Milestone B (B5) completed and milestone closed: executed retirement verification bundle (`make test` green + grep checks confirming retired metadata families absent from `AstExternal`, parser external write paths, active semantic implementation, and test assertions), and finalized handoff focus to Milestone C static-analysis cleanup.
- 2026-03-30: Large-file split follow-up (Phase S parser slice) completed: split `src/parser/parser.c` into function-oriented fragments (`parser_ast_compat.inc`, `parser_core_expr.inc`, `parser_stmt_decl_tu.inc`) with `parser.c` as the aggregator entry, preserving parser behavior and legacy-link compatibility; full `make test` remained green.
- 2026-03-30: Milestone C baseline started: re-ran full suite with GCC `-fanalyzer` and ASan instrumentation; all suites stayed green. Scoped suppression was added for one GCC `-Wanalyzer-fd-leak` false positive in `tests/lexer/lexer_regression_test.c` (`run_with_stderr_suppressed` stderr redirection helper), while preserving normal warning coverage for the rest of the codebase.
- 2026-03-30: Milestone C strict-warning sweep: re-ran full suite under stricter warning flags (`-Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2`) and confirmed zero additional warnings with all regression suites green.
- 2026-03-30: Milestone C ergonomics: added reusable Makefile targets `test-fanalyzer`, `test-asan`, and `test-strict-warnings` and validated all three targets end-to-end to keep static/runtime analysis checks one-command reproducible.
- 2026-03-30: Large-file split follow-up (Phase S semantic slice) completed: split `src/semantic/semantic.c` into function-oriented fragments (`semantic_core_flow.inc`, `semantic_callable_rules.inc`, `semantic_scope_rules.inc`, `semantic_entry.inc`) with `semantic.c` as aggregator entry; behavior preserved and full `make test` remained green (commit `116af1b`).
- 2026-03-30: Milestone C closure checkpoint: re-ran `test-fanalyzer`, `test-asan`, and `test-strict-warnings`; all three targets passed with zero warnings and semantic/parser/lexer regression suites green.
- 2026-03-30: Milestone transition checkpoint: switched active implementation focus from Milestone C to Milestone D strict-path semantics.
- 2026-03-30: Milestone D (D1) tests-first slice landed: flipped semantic CF matrix expectations for `CF-02`/`CF-03`/`CF-06` from accept to reject in strict-path target group.
- 2026-03-30: Milestone D (D2) semantic slice landed: tightened loop return-path analysis for `while(1)`/`for(;;)` when loop bodies can both break and reiterate (`fallthrough` or `continue`), preventing trailing returns from masking non-termination paths; full `make test` and all analysis targets remained green.
- 2026-03-30: Milestone D (D3) coverage slice landed: expanded strict-path CF matrix with nested-loop and mixed break/continue edge locks (`CF-11..CF-18`) and kept full `make clean && make test` plus analysis targets green.
- 2026-03-30: Milestone D (findings hardening) landed: closed equivalent-true condition bypasses by extending constant-true flow recognition to parenthesized/unary/comma constant expressions (for example `(1)`, `+1`, `(0,1)`), added dedicated regressions (`CF-19..CF-25`), and stabilized reject assertions with error code `SEMA-CF-001`; full `make clean && make test` and analysis targets remained green.
- 2026-03-30: Milestone D (D4 build reliability) landed: added explicit parser/semantic `.inc` dependency tracking in `Makefile` so include-fragment edits trigger rebuilds without manual clean.
- 2026-03-30: Milestone D (parser boundary hardening) landed: soft-retired parser callable metadata collection in active path (`PARSER_ENABLE_CALL_METADATA_TRACKING=0` default), clarified parser-flow ownership comments (parser flow is syntax-local; semantic flow is authority), and added parser regression lock proving parser does not enforce semantic all-path return policy.
- 2026-03-30: Milestone D (low-risk guardrail) landed: added explicit maintenance checklist note in parser AST compatibility layer to keep lifecycle helpers synchronized with `include/ast_internal.h` when AST ownership fields evolve.
- 2026-03-31: Milestone D (D6 parser flow convergence) landed: removed parser-side `guaranteed_return` flow output/equations from `parse_*_with_flow` internal interfaces, keeping only syntax-local `may_fallthrough`/`may_break` propagation plus loop-context validation; full `make clean && make test` and analysis targets remained green.
- 2026-03-31: Milestone D (D7 callable metadata hard-retire) landed: removed remaining parser callable metadata state/plumbing from active parser paths and converged call parsing interfaces away from parser-side callable retention; `make test` remained green.
- 2026-03-31: Milestone D (D8 AST lifecycle de-dup) landed: introduced shared lifecycle template instantiation for AST internal and parser compatibility layers, added parser regression lock for complex AST clear-on-failure behavior, and kept `make test` green.
- 2026-03-31: Milestone D (post-D8 convergence cleanup) landed: parser function-external return counting now activates only when explicitly requested by caller output (`out_return_statement_count`), with parser tracking state restored after function-body parse to keep non-AST parse path state-minimal; `make test` remained green.
- 2026-03-31: Milestone D (D9 strict-path constant-condition expansion) landed: semantic flow constant evaluation now recognizes ternary and key binary expression families (logical/equality/relational/bitwise plus short-circuit handling for `&&`/`||`) for loop-condition strict-path reasoning, with CF matrix expansion (`CF-26..CF-31`) and `make test` green.
- 2026-03-31: Milestone D (D9 arithmetic/shift guard expansion) landed: strict-path constant-condition evaluation now also covers arithmetic and shift binary forms (`+ - * / % << >>`) with conservative safety guards for division-by-zero, signed-overflow-sensitive cases, and invalid shift widths, plus CF matrix expansion (`CF-32..CF-40`); `make test` remained green.
- 2026-03-31: Milestone D (D9 strict-path boundary matrix expansion) landed: added nested-loop plus mixed `break`/`continue` with ternary/constant-operator condition combinations (`CF-41..CF-48`) to lock conservative all-path semantics at deeper loop nesting boundaries; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail) landed: retired parser-global function return tracking state (`track_function_return_count` / `function_return_count`) and removed return-statement side-effect counting from parse flow; function `return_statement_count` is now derived from function-body AST traversal only when requested, keeping local-control as the sole parser statement-control path; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail follow-up) landed: removed retired callable-name token compatibility plumbing from expression parsing (`token_slice_callable_name_token` and `out_callable_name_token` path in `parse_primary_with_flags`/`parse_postfix`), leaving callable checks on active local parse flags only; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail batch cleanup) landed: retired legacy expression AST alias APIs (`parser_parse_expression_ast_primary/additive/relational/equality`) and switched regression coverage to the canonical assignment-level entrypoint; also deduplicated parser public-entry setup (`parser_prepare`) across TU/TU-AST/expression parse entrypoints to remove compatibility-era duplicated state/bootstrap branches; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail function-external split) landed: split function external parsing into dedicated signature/body helpers (`parse_function_external_signature`, `parse_function_external_definition_body`) and centralized parameter-name ownership commit, reducing compatibility-era branching in `parse_function_external` while preserving AST contract outputs; `make test` remained green.
- 2026-03-31: Milestone D (parser convergence tail TU-loop dedup) landed: introduced shared translation-unit external-or-declaration helper (`parse_translation_unit_external_or_declaration`) to unify TU and TU-AST loops that previously duplicated "try function external, then rewind and fallback to declaration" control flow; behavior preserved and `make test` remained green.
- 2026-03-31: Milestone D (D11 top-level initializer semantic closure) landed: parser now persists file-scope declaration initializer AST on `AstExternal`, semantic entry now applies callable/scope checks to top-level initializer expressions (closing prior bypass where `has_initializer` existed without semantic expression traversal), and regressions now lock reject/pass matrix coverage for undeclared identifier, call-to-non-function, call-before-declaration, and same-declaration forward-reference top-level initializer cases; `make test` remained green.
- 2026-03-31: Milestone D (D12 top-level definition + allocator hardening) landed: semantic entry now rejects duplicate top-level variable definitions when repeated declarations both carry initializer definitions (`SEMA-TOP-001`) while preserving declaration + single-definition compatibility; regressions now lock both reject and pass forms. In parallel, dynamic growth paths in lexer/parser/semantic scope stack/AST external storage now use overflow-safe capacity expansion guards to prevent `size_t` wraparound on very large inputs; `make test` remained green.
- 2026-03-31: Milestone D (D13 initializer self-visibility + unary-overflow guard + strict-target reliability) landed: top-level initializer semantic checks now exclude the current external from visibility while still allowing prior declarations, fixing same-declaration self-reference/call false accepts and non-function misdiagnostics (`int x=x;`, `int x=x();`); callable and scope regressions were expanded (`TOPINIT-SCOPE-003`, `TOPINIT-CALL-003`). Strict-path constant folding now guards unary minus on `LLONG_MIN` to avoid signed-overflow UB during semantic flow evaluation, with CF coverage expansion (`CF-49`). Makefile test execution now runs copied test binaries (`*.run.$$`) to mitigate environment-specific `Text file busy` failures in strict-warning runs; `make test` and `make test-strict-warnings` are green.
- 2026-04-01: Milestone D (D14 modulo-overflow guard + runner suffix fix) landed: strict-path constant folding now also guards modulo on `LLONG_MIN % -1` to avoid semantic-flow `SIGFPE`/UB from legal source in loop conditions, with CF coverage expansion (`CF-50`). Makefile test runner temp-copy suffix escaping was corrected from `.$$` to `.$$$$` so shell PID expansion is actually applied per run, avoiding fixed `.run.$` collisions across concurrent sessions; `make test` and `make test-strict-warnings` are green.
- 2026-04-01: Milestone D (D15 semantic expression-depth guard) landed: semantic recursive expression traversals (callable postorder walk, scope/call-shadow visibility checks, strict-path constant folding) now enforce a shared depth ceiling and report `SEMA-INT-012` instead of crashing on extremely deep left-associative expression trees. Regression coverage now includes generated deep-chain input to lock non-crashing failure behavior; `make test` is green.
- 2026-04-01: Milestone D (D16 constant-if flow narrowing) landed: `AST_STMT_IF` flow merge now narrows on constant condition truthiness using the same constant evaluator family used by loop strict-path checks, so constant-true/constant-false `if` branches no longer conservatively merge unreachable paths into false `SEMA-CF-001` rejects. CF matrix coverage now includes `if(1)`, `if((1))`, `if(+1)`, `if((0,1))`, `if(0) ... else ...`, and loop-contained constant-true break paths (`CF-51..CF-56`); `make test` is green.
- 2026-04-01: Milestone D (D18 loop CF guard-stability heuristic) landed: non-constant `while`/`for` flow now assumes the loop may execute at least once, but no longer treats first-iteration exits implied only by reusing the entry condition as proof of fallthrough. Loops are rejected only when control flow can reliably prove a stable-guard dead-loop shape: the exit depends on identifiers that never change in the body/step and are not plausibly affected by calls. Mutation-driven exits and call-driven exits remain accepted, while shapes such as `while(a){}`, `while(a){if(a){break;}}`, `while(1){if(a){break;}}`, and `for(;a;){continue;}` are rejected; `make test` is green.
- 2026-04-01: Milestone D (D19 loop guard shadow-sensitivity) landed: loop CF guard tracking now resolves identifiers against local declaration scopes, so same-name block or `for`-init shadowing inside loop bodies no longer counts as mutating or exiting the outer loop guard by name alone. Per-iteration body declarations now also contribute their initializer dependencies when a shadowed break guard is rebuilt from outer state, so mutation-driven exits like `int a=b; if(a) break; b--;` remain accepted while stable shadowed dead loops stay rejected. Regression coverage now locks both representative dead-loop forms (`CF-64..CF-66`) and rebuilt-guard accept cases (`CF-67..CF-68`); `make test` is green.
- 2026-04-01: Roadmap consistency cleanup: top-level Milestone D wording, D10/D17/D18 status text, and known-limitations language were reconciled with the current conservative CF policy so historical strict-path slices are clearly archival only.
- 2026-04-03: Milestone D bugfix landed: non-constant `while`/`for` conditions no longer inherit all-path return from a return-only loop body without proving first-iteration entry. Regression coverage now locks zero-iteration holes such as `while(a){return 1;}`, `for(;a;){return 1;}`, call-guard variants, and `break; return ...` unreachable-tail forms; `make test` is green.
- 2026-04-03: Milestone D documentation alignment landed: CF matrix commentary was synchronized with the actual stable-guard policy so value-dependent loops are no longer described as unconditionally accepted.
- 2026-04-03: Milestone transition checkpoint: front-end control-flow/diagnostic work is considered sufficiently stabilized for handoff, and active implementation focus shifts to Milestone E IR generation bootstrap.
- 2026-04-03: Milestone E planning decision: the first IR will be a single canonical block-based three-address IR with explicit basic blocks/terminators, non-SSA locals, unique internal IDs for blocks/locals/temps, and pretty-printed names only at dump time.
- 2026-04-03: Milestone E implementation slice landed: IR core was split into focused include fragments (`ir_core`, scope lowering, expression lowering, statement lowering, verify, dump) with IntelliSense-safe aggregation, and the build now tracks split IR dependencies.
- 2026-04-03: Milestone E implementation slice landed: canonical CFG lowering now covers explicit `ret/jmp/br` terminators, `if/while/for`, and `break/continue`, using stable block IDs in lowering state to avoid realloc-driven stale block pointers.
- 2026-04-03: Milestone E implementation slice landed: expression lowering now covers compare/equality, short-circuit logical forms, bitwise/shift/comma, direct calls, compound assignment, prefix/postfix `++/--`, and ternary in both value and branch contexts.
- 2026-04-03: Milestone E verifier slice landed: public `ir_verify_program` now enforces structural and flow invariants (mandatory terminators, target validity, reachability, known ops, temp definition coverage, all-path temp availability at use sites, and direct-call arity checks).
- 2026-04-03: Milestone E declaration-awareness slice landed: IR now preserves function declarations as signature-only entries, merges declaration+definition by function name, validates declaration-only shape constraints, and allows declaration-driven call arity verification.
- 2026-04-03: Milestone E verifier hardening landed: verifier now rejects duplicate temp definitions inside the same basic block (`IR-VERIFY-031`) while preserving cross-block join-temp materialization patterns used by branch-merging lowering.
- 2026-04-03: Milestone E callable-boundary slice landed: declaration/definition merges now tolerate parameter-name drift (arity remains authoritative), definition lowering rejects duplicate body-lowering attempts, and verifier rejects unknown direct-call targets (`IR-VERIFY-032`) in addition to call-arity checks.
- 2026-04-03: Milestone E verifier hardening landed: call instruction payload consistency is now validated (`IR-VERIFY-033` rejects `arg_count>0` with `args=NULL`), preventing malformed-IR null-dereference risk during verification.
- 2026-04-03: Milestone E large-slice coverage expansion landed: declaration-aware IR regressions now lock repeated declaration merge behavior and declared-call CFG lowering paths (including call-driven `if` conditions), while verifier regressions now lock duplicate-function-name rejection (`IR-VERIFY-027`) and declaration-only invariant failures for temps/non-parameter-locals (`IR-VERIFY-029`/`IR-VERIFY-030`).
- 2026-04-03: Milestone E verifier resilience hardening landed: verifier now guards malformed function/block payload tables (`IR-VERIFY-034/035/036`) and rejects non-canonical zero-arg call payloads (`IR-VERIFY-037`), with dedicated malformed-IR regression coverage.
- 2026-04-03: Milestone E declaration-aware matrix expansion landed: regressions now lock unnamed-declaration to named-definition merge behavior and declared-call lowering under while-condition CFG paths, extending mixed callable+CFG coverage beyond if-only cases.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regressions now also lock declared-call behavior under logical short-circuit condition lowering, logical value materialization, and ternary value lowering, reinforcing call+CFG interactions across branch-producing expression families.
- 2026-04-03: Milestone E verifier metadata-consistency expansion landed: verifier now rejects malformed count/capacity relationships for locals/blocks/instructions (`IR-VERIFY-038/039/040`) before deep traversal, and regression coverage now locks those invariant failures explicitly.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regression matrix now also locks declared-call behavior in ternary branch-context conditions, extending call+CFG interaction coverage for branch-producing ternary forms.
- 2026-04-03: Milestone E verifier metadata-consistency expansion landed: verifier now also enforces program-level function table consistency (`IR-VERIFY-041/042` for function_count/capacity and NULL function-table payload violations), with dedicated malformed-program regression locks.
- 2026-04-03: Milestone E declaration-aware mixed-path expansion landed: regression matrix now locks declared-call behavior in comma-expression sequencing, extending declaration-call coverage beyond branch-only expression contexts.
- 2026-04-03: Milestone E global-symbol slice landed: IR data model now includes first-class globals (`IR_VALUE_GLOBAL`, global symbol table in `IrProgram`), lowering now records top-level declarations as globals (with constant initializer evaluation and dependency support on prior initialized globals), and expression lowering now resolves identifier lvalues as local-first then global (enabling global `=`, compound assignment, and prefix/postfix `++/--` in function bodies).
- 2026-04-03: Milestone E global-verifier/dump coverage landed: verifier now validates global value references and global-table metadata/name/id invariants (`IR-VERIFY-043..047`), dump output now emits `global ...` declarations, and IR regression/verifier suites were expanded to lock global read/write/update/shadowing behavior plus malformed-global metadata cases.
- 2026-04-03: Milestone E runtime-global-init slice landed: non-constant top-level initializers are now lowered into a reserved internal IR function (`__global.init`) that evaluates initializer expressions at runtime and writes results into target globals in source order.
- 2026-04-03: Milestone E runtime-init integration landed: when runtime global initialization is present, lowering now injects a startup call from lowered `main` entry to `__global.init`, and verifier now enforces constant/runtime initializer-flag exclusivity on globals (`IR-VERIFY-048`) with dedicated regression locks.
- 2026-04-03: Milestone E runtime-init entrypoint fallback landed: when runtime global initialization exists but no lowered `main` definition is available, lowering now auto-exports `__program.init` that calls `__global.init` once and returns, replacing the previous hard error path.
- 2026-04-03: Milestone E global-init dependency hardening landed: runtime top-level initializer lowering now collects referenced global dependencies and rejects initializers that depend on globals before those globals are initialized (`IR-LOWER-022`), preventing self/loop-style uninitialized dependency chains.
- 2026-04-03: Milestone E global-operator matrix expansion landed: IR regressions now explicitly lock global shift/bitwise compound assignment paths and runtime-global-init dependency rejection behavior in addition to existing global read/write/update coverage.
- 2026-04-03: Milestone E runtime-init verifier contract hardening landed: verifier now enforces that runtime global initialization implies a valid `__global.init` helper and a startup entry call path from either `main` entry or `__program.init` (`IR-VERIFY-049/050`), with dedicated malformed-IR regression locks.
- 2026-04-03: Milestone E runtime-init contract tightening landed: verifier now enforces fixed internal-init function shapes (`__global.init` / `__program.init` must be zero-parameter, zero-local, single-block, `ret 0`) and `__program.init` entry must begin with `call __global.init()` (`IR-VERIFY-051..054`), with dedicated no-main fallback tamper negative coverage.
- 2026-04-03: Milestone E runtime-init contract tightening follow-up landed: verifier now additionally requires `__program.init` entry to contain exactly one instruction before `ret 0` (`call __global.init()` only), and rejects reserved internal helper names when runtime global initialization is absent (`IR-VERIFY-055/056`), with dedicated malformed-IR negative locks.
- 2026-04-03: Milestone E runtime-global dependency closure landed: top-level runtime initializer dependency analysis now traverses function-body call chains (with scope-aware global-read collection) instead of only direct initializer AST identifiers, so indirect reads of not-yet-initialized globals are rejected under existing `IR-LOWER-022` policy; IR regressions now include indirect-call dependency negative coverage.
- 2026-04-03: Milestone E runtime-global dependency precision follow-up landed: function-body dependency collection is now reachability-aware for obvious dead-code shapes (statement fallthrough after `return`/`break`/`continue`, constant-condition pruning for `if(1)` / `if(0)` / `while(0)` / `for(...;0;...)`), reducing `IR-LOWER-022` false positives from syntactic-only traversal while keeping conservative behavior for non-constant loop reachability.
- 2026-04-03: Milestone E runtime-global dependency diagnostics follow-up landed: lowering now predeclares global symbol slots before initializer lowering and, on `IR-LOWER-022` failures, attempts dependency-graph cycle reconstruction to emit concrete chain diagnostics (for example `a -> b -> a`) when the failure is cycle-driven; regression coverage now locks this chain output.
- 2026-04-04: Milestone E runtime-global-init error-contract alignment landed: overflow/unsupported-constant top-level initializer forms now mark `invalid_constant` and attempt runtime-init fallback instead of surfacing a dedicated top-level overflow code; `IR-LOWER-023` is retired from active diagnostics, while hard-stop behavior remains on `IR-LOWER-018` (for example divide/mod by zero or unsupported constant operator), `IR-LOWER-019` (shift-count out of range), and `IR-LOWER-022` (not-yet-initialized global dependency).
- 2026-04-04: Milestone E runtime-global cycle diagnostics upgrade landed: `IR-LOWER-022` cycle output is now callee-aware by stitching per-edge dependency paths (for example `a -> read_b() -> b -> read_a() -> a`) instead of only global-level node chains; IR regression coverage now locks this callee-aware cycle-chain form.
- 2026-04-04: Milestone E verifier value-legality hardening slice landed: verifier now enforces that `IR_INSTR_BINARY` and `IR_INSTR_CALL` results must be temporaries (`IR-VERIFY-057/058`), preventing malformed IR from writing compute/call results directly into non-temp destinations; verifier regressions now include dedicated negative locks for both constraints.
- 2026-04-04: Milestone E callable-boundary hardening slice landed: call lowering now performs pre-emit callable validation against the IR function symbol table and rejects malformed call sites early with source-local diagnostics (`IR-LOWER-025` unknown callee, `IR-LOWER-026` arity mismatch) instead of relying only on post-lowering verifier checks; IR regressions now include semantic-skipped negative locks to keep these lowering guardrails stable.
- 2026-04-04: Milestone E callable declaration/definition boundary follow-up landed: IR regressions now also lock lowering-side declaration/definition interaction guardrails under semantic-skipped mode (`IR-LOWER-020` declaration arity mismatch and `IR-LOWER-021` duplicate function body lowering), ensuring these safety checks remain enforced even when semantic gating is bypassed.
- 2026-04-04: Milestone E/E7 regression matrix expansion slice landed: declaration-aware call coverage now explicitly includes `for` condition + step call paths (for example `for(;pred(x);x=step(x))`) so mixed control-flow/call lowering remains stable beyond existing `if`/`while`/ternary/comma call-shape locks.
- 2026-04-04: Milestone E/E7 regression matrix follow-up landed: declaration-aware `for` coverage now also locks init-position call lowering (for example `for(x=init(x);pred(x);x=step(x))`), extending mixed control-flow/call stability from condition/step-only to full init+condition+step call shapes.
- 2026-04-04: Milestone E/E7 regression matrix expansion follow-up landed: declaration-aware call coverage now also locks logical-OR short-circuit shapes in both condition and value contexts (for example `if(pred(x)||pred(0))` and `return pred(x)||pred(1)`), plus standalone `for` init-call lowering (`for(x=init(x);x;x=x-1)`) to close remaining mixed short-circuit/loop call-form gaps.
- 2026-04-04: Milestone E/E7 regression matrix large-batch follow-up landed: declaration-aware `for` call coverage now additionally locks logical-AND and logical-OR loop-condition short-circuit forms plus comma-expression call shapes in both step and init positions (for example `for(;pred(x)&&gate(x);x=step(x))`, `for(;pred(x)||gate(x);x=step(x))`, `x=(step1(x),step2(x))`, `x=(init1(x),init2(x))`).
- 2026-04-04: Milestone E/E7 regression matrix completion slice landed: declaration-aware mixed control-flow/call coverage now also locks nested short-circuit compositions in value/condition/loop contexts (for example `(pred(x)&&gate(x))||tail(x)` and `if((pred(x)||gate(x))&&tail(x))`), closing the remaining nested-mix edge cases identified for E7.
- 2026-04-04: Milestone E callable-boundary policy follow-up landed: verifier now enforces reserved internal initializer helper call-site policy (`IR-VERIFY-059/060`): `__global.init` is only legal at startup entry call sites (`main` entry or `__program.init` entry), and `__program.init` is rejected as a normal in-body call target; verifier tests now include both acceptance and negative locks for these policy boundaries.
- 2026-04-04: Milestone E/E8 verifier metadata-consistency slice landed: verifier now rejects duplicate global symbol entries (`IR-VERIFY-062`) so malformed IR with colliding global names cannot pass structural checks; verifier coverage now includes dedicated negative lock for duplicate global-name collisions.
- 2026-04-04: Milestone E/E8 verifier symbol-namespace consistency slice landed: verifier now rejects function/global symbol-name collisions (`IR-VERIFY-063`) so malformed IR cannot bind one identifier to both a function entry and a global entry in the same program namespace; verifier coverage now includes dedicated negative lock for this collision shape.
- 2026-04-04: Milestone E runtime-global dependency precision follow-up landed: loop dependency collection now uses loop-control reachability effects to prune dead post-loop paths and unreachable `for` step reads (for example `while(1){return ...}` and `for(;1;step){return ...}` no longer over-collect), while preserving reachable-step dependencies via `continue`; IR regressions now include both dead-loop acceptance and live-step-via-continue negative locks.
- 2026-04-04: Milestone E runtime-global dependency precision guard follow-up landed: IR regressions now additionally lock `for`-step reachability boundaries for unconditional `break` (step dead, should not count) versus infinite-loop fallthrough (step live, should count), reducing future risk of both over-pruning and under-pruning around `IR-LOWER-022` loop-step dependency collection.
- 2026-04-04: Milestone E runtime-global dependency nested-loop guard follow-up landed: IR regressions now lock constant-true `while` nested-branch loop-exit precision in both directions (`if(0) break` keeps post-loop reads dead; `if(1) break` keeps post-loop reads live), further reducing `IR-LOWER-022` false positives without hiding real reachable dependencies.
- 2026-04-04: Milestone E runtime-global dependency inner-loop control guard follow-up landed: regressions now additionally lock that inner-loop `break` does not incorrectly make outer-loop post-read/step paths live unless outer control actually exits via outer `break`/`continue` (outer-break keeps step dead, outer-continue keeps step live), tightening `IR-LOWER-022` precision around nested loop-control composition.
- 2026-04-04: Milestone E runtime-global dependency consistency guard follow-up landed: regressions now explicitly lock that unreachable `continue` edges in constant-true loops do not leak into post-loop dependency reachability (`while(1){if(0) continue; return 1;} return b;` keeps post-loop `b` dead), reinforcing alignment between dependency collection and diagnostic-path tracing.
- 2026-04-04: Roadmap transition checkpoint: Milestone E is marked closed for current scope (canonical IR bootstrap + hardening complete), and active roadmap focus is switched to Milestone F pass-stage work on canonical IR.

## Current Milestone Focus

- Milestone A closure is complete under agreed scope: semantic-authority retirement for callable/return/counter metadata is done and verified.
- Milestone B closure is complete under agreed scope: parser/AST internal decoupling and parser-side metadata-retention cleanup are done and verified.
- Milestone C baseline is complete and validated (`test-fanalyzer`/`test-asan`/`test-strict-warnings` are green with zero warnings).
- Milestone D is maintenance-mode only: reopen it for concrete semantic bugs, but do not spend roadmap budget on generalized hygiene.
- Milestone E is closed for current scope: canonical IR v1 bootstrap + verifier hardening + dependency precision guardrails are landed and regression-locked.
- Active implementation focus is now Milestone F: pass-stage analysis/transformation work on the existing canonical IR.
- Large-file split follow-up for parser + semantic implementation files is completed.
- Keep parser recursion-limit behavior as an accepted guardrail (call-depth based, not raw parenthesis-depth based) unless future work introduces a dedicated structural-depth limiter.
- IR v1 shipped baseline remains the core handoff surface: block-based CFG, explicit terminators, unique IDs for blocks/locals/temps, non-SSA locals, broad lowering matrix, dedicated IR regressions, and verifier coverage.
- Current roadmap emphasis is pass-stage evolution without adding a second peer IR pipeline.
- IR v1 non-goals for this phase: SSA phi placement, register allocation, backend-specific lowering, or multiple parallel AST-to-IR families.

## Milestone D Closure Snapshot

- Completed: conservative branch/loop return-path policy, constant-if narrowing, stable-guard loop rejection, shadow-sensitive guard tracking, and structural semantic diagnostic unification.
- Recent bug closure: non-constant zero-iteration loops no longer falsely satisfy all-path return just because the loop body returns when entered.
- Regression baseline now covers constant-condition loops, stable-guard value-dependent loops, rebuilt-guard accept cases, zero-iteration return-only holes, and unreachable-return-after-break shapes.
- Remaining D work is bug-driven only; it is no longer the roadmap's main implementation track.

## Active Implementation Plan (Milestone F status)

1. E-close status: canonical IR v1 implementation + hardening is complete for current roadmap scope, including lowering baseline, verifier baseline, declaration/global/runtime-init support, and dependency precision follow-ups.
2. F0 in progress: establish a minimal pass pipeline scaffold on canonical IR (ordered execution, pass failure propagation, and stable test hooks) without changing default program behavior.
3. F1 planned: introduce first analysis pass utilities that later transforms can depend on (for example local const/use or effect classification), with tests that lock analysis output contracts.
4. F2 planned: introduce first low-risk transform pass (for example trivial constant fold / dead expression cleanup) with strict side-effect and control-flow safety guards.
5. F3 planned: strengthen verifier + regression coupling around pass outputs so transformed IR remains structurally valid and semantically equivalent.
6. F4 planned: evaluate CFG cleanup/canonicalization opportunities only after F0-F3 prove stable in repeated full-suite runs.
7. Future reminder: if optimization or backend requirements eventually exceed canonical IR passability, introduce SSA/lower IR only as serial downstream lowering, not as a parallel AST-to-IR track.

## Milestone B Detailed Execution Plan (historical reference)

1. B0 checkpoint freeze (documentation + baseline)
- Goal: lock Milestone A closure context before B-side structural edits.
- Touchpoints: `NEXT_STEPS.md` only.
- Validation: `make test` green baseline already captured before B edits; no functional behavior changes in this step.

2. B0.5 regression-suite modularization slice (execute before metadata retirement)
- Goal: reduce blast radius by splitting oversized regression sources into themed files before parser/semantic decoupling slices.
- Primary touchpoints: `tests/parser/parser_regression_test.c`, `tests/semantic/semantic_regression_test.c`, `Makefile` (suite aggregation entrypoints).
- Safety constraints: no diagnostic/rule/parse behavior changes; assertions and coverage must remain equivalent.
- Validation gate: full `make test`, plus a suite-level count sanity check to ensure no regression groups were dropped.

3. B1 callable metadata retention cleanup slice
- Goal: remove parser-side `called_function_*` retention fields and collection plumbing that are no longer semantic-authoritative.
- Primary touchpoints: `src/parser/parser.c`, `include/ast.h`, parser dump/test adapters in `tests/parser`.
- Safety constraints: keep parser parse/semantic behavior unchanged; only remove unused retention channels.
- Validation gate: full `make test` plus direct grep check that `called_function_*` is retired from active parser/AST interfaces or limited to explicitly documented compatibility stubs.

4. B2 return metadata retention cleanup slice
- Goal: remove parser-side `returns_on_all_paths` retention field from `AstExternal` and corresponding parser write paths.
- Primary touchpoints: `src/parser/parser.c`, `include/ast.h`, parser regressions/dump expectations.
- Safety constraints: semantic return enforcement must remain AST-primary with no behavior drift.
- Validation gate: full `make test` plus targeted return-matrix checks in semantic regressions.

5. B3 statement-counter retention cleanup slice
- Goal: remove parser-side statement counters (`loop/if/break/continue/declaration`) from external metadata and related parser accounting paths.
- Primary touchpoints: `src/parser/parser.c`, `include/ast.h`, parser regression assertions, parser dump output.
- Safety constraints: do not alter statement AST construction or scope/callable traversal semantics.
- Validation gate: full `make test` plus parser smoke/regression confirmation that diagnostics and parse success behavior are unchanged.

6. B4 parser/AST internal decoupling slice
- Goal: reduce parser dependence on AST-internal helper coupling and keep only stable parser-facing contracts.
- Primary touchpoints: parser include boundaries and AST helper visibility points.
- Safety constraints: preserve old parser API link behavior and existing external call sites.
- Validation gate: full `make test`, including legacy-link parser smoke.

7. B5 cleanup verification and handoff record
- Goal: close Milestone B with explicit evidence bundle and handoff notes for Milestone C static-analysis pass.
- Evidence checklist: updated roadmap notes, `make test` green run, grep-based retirement checks for all B metadata families.
- Exit criterion: no functional regression introduced; B changes remain structural/compatibility-focused only.

8. Cross-slice operating rules (apply to B0.5-B5)
- Rule 1: retire one metadata family per code-change slice; do not batch callable/return/counter removals together.
- Rule 2: after each slice, run full `make test` before proceeding.
- Rule 3: if any semantic behavior drift appears, pause and restore prior compatibility surface for that slice before continuing.

## Large-File Split Schedule (historical reference)

1. Phase T (immediate): split regression suites first.
- Targets: `tests/parser/parser_regression_test.c` and `tests/semantic/semantic_regression_test.c`.
- Reason: lowest semantic risk and highest payoff for reviewability/merge conflict reduction.

2. Phase S (after B1-B3): split semantic/parser implementation files.
- Targets: `src/semantic/semantic.c`, then `src/parser/parser.c`.
- Status: completed for both targets.

3. Phase thresholds and guardrails.
- Soft threshold: start split planning when a C file exceeds ~1500 lines or mixes more than one major concern.
- Hard rule: each split commit must preserve behavior (`make test` green) and avoid API drift unless explicitly planned.

## Metadata Migration Schedule (anti-coupling plan)

1. S1 status: complete (metadata kept as compatibility rails).
2. S2 status: complete for active definition paths (AST-primary callable + return-flow).
3. S3 rule: new scope semantics must be computed from statement AST traversal, not parser metadata.
4. S3 cleanup gate: begin retirement only after scope/callable/CF matrices are simultaneously green.
5. Retirement order (current):
6. first retire `called_function_*` semantic authority (completed in semantic active paths),
7. then retire `returns_on_all_paths` semantic authority (completed in semantic active paths),
8. then retire statement counters (`loop/if/break/continue/declaration`) from semantic dependencies (semantic-authority usage verified absent in active paths).
9. parser-side retention cleanup is Milestone B decoupling scope (compatibility/test-oriented, non-semantic-authority).
10. Safety rule: retire one metadata family per change slice and run full `make test` before next retirement.
11. Rollback rule: if drift appears, restore previous gate and keep parity rails until root cause is fixed.

## Known Limitations (Current Behavior)

- Return all-path analysis is still a structured approximation, not a full path solver.
- Loop CF policy is intentionally false-positive-averse rather than fully symbolic: mutation-driven or call-driven exits (for example `CF-02`/`CF-03`/`CF-06`) remain accepted, while only reliably proven stable-guard dead loops are reject expectations.
- Loop CF deliberately does not treat first-iteration implications from reusing the entry condition as proof of fallthrough. Under the current policy, shapes such as `while(a){if(a){break;}}` or `for(;a;){if(a){break;}}` remain reject expectations unless the guard is mutated or a call could plausibly mutate it.
- Equivalent-true condition rewrites (for example `(1)`, `+1`, `(0,1)`, `for(;(1);...)`) are now treated as strict-path true in flow analysis and covered by regressions.
- Parser `parse_*_with_flow` interfaces are explicitly syntax-local for parser constraints/AST shaping and now expose only `may_fallthrough`/`may_break`; parser return-path authority equations are removed from parser flow outputs and remain semantic-owned.
- Current control-flow analysis remains intra-function and statement-structured (it is not a full symbolic path solver for arbitrary value-dependent loops).
- Loop CF's current "variable changed" heuristic only tracks direct body/step mutation plus conservative function-call side effects. If future language work adds indirect writes (`&`/pointer/reference mutation) or lambda/closure-style callables with captured state, this heuristic must be redesigned before trusting D18-style dead-loop proofs.
- Loop CF guard tracking is now scope-sensitive for direct local declaration shadowing inside loop bodies and `for` init scopes, so same-name inner declarations no longer count as mutations or exit guards for the outer loop condition. It also follows a shallow local fixpoint over per-iteration declaration initializers when a shadowed guard is rebuilt from outer state, including simple multi-hop rebuilt-guard chains (for example `int c=b; int a=c; if(a) break; b--;`). The heuristic is still intentionally shallow: it does not model aliasing, address-taken mutation, pointer/reference writes, closure-captured indirect state, or broader value reasoning beyond these local declaration-initializer dependency chains.
- Include-fragment rebuild reliability is hardened: parser/semantic `.inc` and regression fragment edits now trigger rebuilds via explicit Makefile prerequisites.
- Current assignment-lvalue policy is intentionally identifier-only in parser expression checks (`=` and compound assignments); parenthesized identifiers and other non-identifier lvalue forms for assignment remain rejected in this subset.
- Increment/decrement policy currently allows identifier and parenthesized-identifier operands (e.g., `++(a)`, `(a)++`) but still rejects broader lvalue forms.
- Low-priority accepted limitation: in translation-unit parsing, assignment-style forms rejected by identifier-only lvalue policy (e.g., `(a)=b`, `(a)+=b`) may surface syntax-oriented diagnostics such as `Expected ';'` instead of semantic-style `lvalue` wording.
- Parser recursion-limit note: expression recursion protection is implemented on parser call-depth frames; in deeply parenthesized inputs, the guard can trigger before reaching the same numeric parenthesis depth.
- Callable semantic limitation (current policy): minimal callable analysis only supports direct identifier callee forms. Call-result/chained forms (for example `f()(1)`, `a()()`, `((f)(1))(2)`) are explicitly rejected as `SEMA-CALL-005` with `callee_kind=call_result`.
- Scope/callable layering note: undeclared checks intentionally skip call-callee identifier subtrees so callable diagnostics remain sourced from `SEMA-CALL-*` rules.
- Scope note for `SEMA-CALL-006`: this code applies to the parser-accepted subset of non-identifier callees that reaches semantic callable checks (for example `(f+1)()`), reported with `callee_kind=non_identifier`. Non-parenthesized non-identifier callees outside this subset can still be rejected earlier by parser syntax checks.
