# Compiler Lab Next Steps

## Current Guidance (agreed)

1. Milestone A is closed for implementation tracking (semantic-authority retirement complete for callable/return/statement-counter metadata).
2. Milestone B is closed for implementation tracking (parser/AST decoupling and parser-side metadata-retention cleanup completed).
3. Milestone C baseline validation is complete (`-fanalyzer`/ASan/strict-warning reruns are green).
4. Active implementation is Milestone D (conservative control-flow semantics hardening plus bug-driven semantic fixes).

## Why this order

- S1+S2 AST-primary cutover is in place, and S3 semantic-authority retirement has been completed and validated.
- Structural coupling cleanup and static-analysis baselines are now complete.
- The next high-value semantic risk is control-flow false-positive drift, semantic crash resistance, and bug closure around return-path reasoning.
- Milestone D flow policy has converged on the conservative D17/D18 rules: avoid `SEMA-CF-001` false positives, reject only reliably proven must-not-return loop shapes, and keep mutation-driven or call-driven exits accepted unless a stable dead-loop shape is provable.

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

### Milestone D: Conservative Control-Flow Semantics (active)

- Keep `SEMA-CF-001` focused on reliably provable must-not-return shapes.
- Preserve strict branch requirements for `if/else` and plain `if` without turning value-dependent loops into false positives.
- Continue bug-driven hardening for semantic diagnostics, crash resistance, and control-flow narrowing.

## Guardrails

- Do not prioritize low-risk hygiene over semantic feature progress.
- Every functional change should come with a regression test.
- Keep `make test` green at each step.

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

## Current Milestone Focus

- Milestone A closure is complete under agreed scope: semantic-authority retirement for callable/return/counter metadata is done and verified.
- Milestone B closure is complete under agreed scope: parser/AST internal decoupling and parser-side metadata-retention cleanup are done and verified.
- Milestone C baseline is complete and validated (`test-fanalyzer`/`test-asan`/`test-strict-warnings` are green with zero warnings).
- Milestone D is now the active implementation phase (conservative CF hardening, semantic bug closure, and regression/documentation convergence).
- Large-file split follow-up for parser + semantic implementation files is completed.
- Keep parser recursion-limit behavior as an accepted guardrail (call-depth based, not raw parenthesis-depth based) unless future work introduces a dedicated structural-depth limiter.

## S2 Closure Snapshot

- Completed: AST-primary callable checks for definitions, AST-primary return-path gate, visitor-based callable traversal, and chained-call behavior locks (`a()()` included).
- Retired from active definition-path callable flow: `SEMA-INT-004`, `SEMA-INT-006` (historical only).
- S3 semantic-authority retirement has completed; active remaining work is Milestone D conservative CF hardening and related semantic bug closure.

## Active Implementation Plan (Milestone D status)

1. D0 status: complete; roadmap status is synchronized to `C complete -> D active` and current baselines are recorded.
2. D1 status: complete as a historical tests-first slice; its temporary CF-02/CF-03/CF-06 reject stance has been superseded by the later D17/D18 conservative loop policy.
3. D2 status: complete; loop return-path logic is tightened for cond-true loops that may both break and reiterate.
4. D3 status: complete; strict-path edge coverage is expanded with nested-loop and mixed break/continue locks.
5. D4 status: complete; validation bundle is green and include-fragment dependency tracking is hardened in Makefile.
6. D5 status: complete; strict-path behavior changes and accepted limitations are recorded in this document.
7. D6 status: complete; parser local-flow interfaces are converged to syntax-local fallthrough/break outputs (no parser return-path authority surface).
8. D7 status: complete; parser callable metadata retention/plumbing is retired from active parser flow.
9. D8 status: complete; AST lifecycle helper logic is de-duplicated via shared template between AST core and parser compatibility layer.
10. D9 status: complete; strict-path constant-condition reasoning now covers ternary plus logical/equality/relational/bitwise/arithmetic/shift binary forms, with conservative safety guards and dedicated CF matrix locks (`CF-26..CF-48`).
11. D10 status: complete; parser convergence tail cleanup retired parser-global return counting state, unused callable-name token plumbing, and legacy expression alias entrypoints, split function-external parsing into signature/body helpers, and unified TU/TU-AST external-vs-declaration fallback through a shared helper while preserving AST contract outputs and keeping local-control as the only statement-control channel.
12. D11 status: complete; top-level declaration initializer path is now AST-primary semantic-authoritative (initializer AST persisted on `AstExternal` and validated with callable + scope checks in semantic entry), with dedicated regression matrix coverage and `make test` green.
13. D12 status: complete; top-level declaration/declaration duplicate-definition hole is closed (`SEMA-TOP-001` when both declarations define with initializer), and core dynamic array growth paths now have overflow-safe capacity guards in lexer/parser/semantic-scope/AST lifecycle storage.
14. D13 status: complete; top-level initializer visibility now excludes current external during callable/scope checks (closing same-declaration self-reference/call holes), unary-minus constant-fold path now guards `LLONG_MIN` to avoid UB in strict-flow evaluation, and strict-warning test target reliability is improved via copied-binary execution to avoid transient `Text file busy` runner races.
15. D14 status: complete; strict-path modulo constant-fold path now guards `LLONG_MIN % -1` to avoid `SIGFPE`/UB in semantic evaluation, and Makefile copied-binary temp suffix escaping is fixed (`.$$$$`) so test runners use real PID-suffixed paths under concurrent executions.
16. D15 status: complete; semantic expression recursion depth is now guarded across callable/scope/flow walkers, converting deep expression stack-overflow crashes into deterministic semantic failures (`SEMA-INT-012`) with dedicated regression coverage.
17. D16 status: complete; semantic `AST_STMT_IF` flow analysis now applies constant-condition narrowing (true/false) before branch merge, eliminating conservative false negatives on constant-if return paths and loop-local break/return patterns, with dedicated CF locks (`CF-51..CF-56`).
18. D17 status: complete; control-flow policy is explicitly conservative and false-positive-averse: `if/else` still requires all reachable branches to return, plain `if` still requires the surrounding path to return, and loop handling is no longer treated as a strict path solver for arbitrary value-dependent exits.
19. D18 status: complete; loop CF now treats non-constant `while`/`for` conditions as possibly entering once, rejects only reliably proven stable-guard dead loops, and does not accept fallthrough merely because the first iteration could reuse the entry condition. Mutation-driven exits and plausibly state-changing calls remain accepted; stable same-guard shapes such as `while(a){if(a){break;}}` are intentionally rejected under the current policy because the guard is unchanged and the first-iteration implication is not used as proof.
20. D19 status: complete; loop guard tracking is now scope-sensitive for direct local declaration shadowing, so inner `int a=...;` declarations inside loop bodies or `for` init scopes no longer masquerade as mutations or exits of an outer guard with the same spelling. Per-iteration body declarations also propagate initializer dependencies to a local fixpoint within each compound body, which keeps simple multi-hop rebuilt-guard exits accepted (`int c=b; int a=c; if(a) break; b--;`) without relaxing stable shadowed dead-loop rejects.
21. Future reminder: if the language grows indirect mutation forms such as address-of/pointer writes (`&`, dereference-based stores), reference semantics, or lambda/closure-style callable bodies (including `auto` lambda-like forms), revisit D18/D19's guard-tracking heuristic. It is intentionally shallow today and will become unsound once mutation can escape direct assignment/call visibility.

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
