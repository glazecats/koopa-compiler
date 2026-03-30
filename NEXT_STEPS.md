# Compiler Lab Next Steps

## Current Guidance (agreed)

1. Milestone A is closed for implementation tracking (semantic-authority retirement complete for callable/return/statement-counter metadata).
2. Milestone B is closed for implementation tracking (parser/AST decoupling and parser-side metadata-retention cleanup completed).
3. Active implementation is Milestone C (`-fanalyzer`/ASan cleanup and warning triage).
4. Keep Milestone C (`-fanalyzer`/ASan cleanup) and Milestone D (strict path semantics) as follow-up phases.

## Why this order

- S1+S2 AST-primary cutover is in place, and S3 semantic-authority retirement has been completed and validated.
- The remaining high-value risk is structural coupling and parser-side legacy retention, not missing semantic baseline rules.
- `-fanalyzer` warning around `external.name` is currently treated as likely false positive.
- Runtime tests and ASan runs did not reproduce a real leak.
- Parser dependence on AST internals and legacy metadata retention is now the primary maintainability target.

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

## Current Milestone Focus

- Milestone A closure is complete under agreed scope: semantic-authority retirement for callable/return/counter metadata is done and verified.
- Milestone B closure is complete under agreed scope: parser/AST internal decoupling and parser-side metadata-retention cleanup are done and verified.
- Milestone C is now the active implementation phase (`-fanalyzer`/ASan cleanup and warning triage).
- Large-file split is now scheduled as a B-support track: split oversized regression tests first, then split large implementation files after compatibility touchpoints shrink.
- Keep parser recursion-limit behavior as an accepted guardrail (call-depth based, not raw parenthesis-depth based) unless future work introduces a dedicated structural-depth limiter.

## S2 Closure Snapshot

- Completed: AST-primary callable checks for definitions, AST-primary return-path gate, visitor-based callable traversal, and chained-call behavior locks (`a()()` included).
- Retired from active definition-path callable flow: `SEMA-INT-004`, `SEMA-INT-006` (historical only).
- S3 semantic-authority retirement has completed; active remaining work is Milestone C static-analysis cleanup.

## Active Implementation Plan (Milestone B kickoff)

1. B0 status: complete (A closure and B handoff context recorded in this document).
2. B0.5 status: complete; oversized regression tests are split into suite aggregators plus themed include fragments (no behavior change).
3. B1 status: complete; parser-side `called_function_*` retention interfaces are retired from AST/parser output paths and tests are aligned.
4. B2 status: complete; parser-side `returns_on_all_paths` retention field and parser write paths are retired.
5. B3 status: complete; parser-side statement-counter retention fields and parser accounting/write paths are retired.
6. B4 status: complete; parser no longer depends on `ast_internal.h`, and parser-only legacy link compatibility is preserved.
7. B5 status: complete; retirement verification bundle is recorded and Milestone B is closed.
8. Validation gate: `make test` must remain green after every B-slice change.
9. Deferred tightening (Milestone D handoff): flip CF-02/CF-03/CF-06 expectations only in Milestone D strict-path work.

## Milestone B Detailed Execution Plan (active)

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

## Large-File Split Schedule (B support track)

1. Phase T (immediate): split regression suites first.
- Targets: `tests/parser/parser_regression_test.c` and `tests/semantic/semantic_regression_test.c`.
- Reason: lowest semantic risk and highest payoff for reviewability/merge conflict reduction.

2. Phase S (after B1-B3): split semantic/parser implementation files.
- Targets: `src/semantic/semantic.c`, then `src/parser/parser.c`.
- Reason: perform after metadata-retention cleanup to avoid moving code that is about to be deleted.

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
- Example currently accepted by design: `int f(int a){while(1){if(a){break;}}return 1;}`.
- Rationale: parser flow marks loop body as potentially breaking, so a fallthrough path to trailing `return` is considered reachable.
- Semantic regression matrix lock: CF-02 / CF-03 / CF-06 are currently expected to pass and are tagged as known-limitation guardrails.
- Future strict-path migration rule: CF-02 / CF-03 / CF-06 should be flipped to expected-fail together when Milestone D is implemented.
- If we later adopt stricter semantics that treat non-termination paths as non-returning failures, this loop/break interaction is a priority tightening point.
- Current assignment-lvalue policy is intentionally identifier-only in parser expression checks (`=` and compound assignments); parenthesized identifiers and other non-identifier lvalue forms for assignment remain rejected in this subset.
- Increment/decrement policy currently allows identifier and parenthesized-identifier operands (e.g., `++(a)`, `(a)++`) but still rejects broader lvalue forms.
- Low-priority accepted limitation: in translation-unit parsing, assignment-style forms rejected by identifier-only lvalue policy (e.g., `(a)=b`, `(a)+=b`) may surface syntax-oriented diagnostics such as `Expected ';'` instead of semantic-style `lvalue` wording.
- Parser recursion-limit note: expression recursion protection is implemented on parser call-depth frames; in deeply parenthesized inputs, the guard can trigger before reaching the same numeric parenthesis depth.
- Callable semantic limitation (current policy): minimal callable analysis only supports direct identifier callee forms. Call-result/chained forms (for example `f()(1)`, `a()()`, `((f)(1))(2)`) are explicitly rejected as `SEMA-CALL-005` with `callee_kind=call_result`.
- Scope/callable layering note: undeclared checks intentionally skip call-callee identifier subtrees so callable diagnostics remain sourced from `SEMA-CALL-*` rules.
- Scope note for `SEMA-CALL-006`: this code applies to the parser-accepted subset of non-identifier callees that reaches semantic callable checks (for example `(f+1)()`), reported with `callee_kind=non_identifier`. Non-parenthesized non-identifier callees outside this subset can still be rejected earlier by parser syntax checks.
