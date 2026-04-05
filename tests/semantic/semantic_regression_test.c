#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Split fragment inventory (keep this list in sync with includes below):
 * - semantic_regression_callable_flow.inc
 * - semantic_regression_scope_cf.inc
 */

static int parse_source_to_ast(const char *source,
    TokenArray *tokens,
    AstProgram *program,
    ParserError *parse_err) {
    lexer_init_tokens(tokens);
    ast_program_init(program);

    if (!lexer_tokenize(source, tokens)) {
        fprintf(stderr, "[semantic-reg] FAIL: lexer failed for input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(tokens, program, parse_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: parser failed at %d:%d: %s\n",
            parse_err->line,
            parse_err->column,
            parse_err->message);
        lexer_free_tokens(tokens);
        ast_program_free(program);
        return 0;
    }

    return 1;
}

#ifndef SEMANTIC_CALLABLE_DIAG_CASE_TYPEDEF
#define SEMANTIC_CALLABLE_DIAG_CASE_TYPEDEF
typedef struct {
    const char *case_id;
    const char *source;
    const char *expected_code;
    const char *expected_snippet;
    int expected_line;
    int expected_column;
} CallableDiagCase;
#endif

#ifndef SEMANTIC_CALLABLE_PASS_CASE_TYPEDEF
#define SEMANTIC_CALLABLE_PASS_CASE_TYPEDEF
typedef struct {
    const char *case_id;
    const char *source;
} CallablePassCase;
#endif

static int run_callable_diag_case(const CallableDiagCase *c) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!c || !c->source || !c->expected_code) {
        return 0;
    }

    if (!parse_source_to_ast(c->source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should fail semantic analysis\n",
            c->case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, c->expected_code) == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected %s, got: %s\n",
            c->case_id,
            c->expected_code,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (c->expected_snippet && strstr(sema_err.message, c->expected_snippet) == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected diagnostic snippet '%s', got: %s\n",
            c->case_id,
            c->expected_snippet,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (c->expected_line > 0 && c->expected_column > 0 &&
        (sema_err.line != c->expected_line || sema_err.column != c->expected_column)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected location %d:%d, got %d:%d\n",
            c->case_id,
            c->expected_line,
            c->expected_column,
            sema_err.line,
            sema_err.column);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int run_callable_pass_case(const CallablePassCase *c) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!c || !c->source) {
        return 0;
    }

    if (!parse_source_to_ast(c->source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should pass semantic analysis, got: %s\n",
            c->case_id,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

/* Split for maintainability: keep regression execution order in main, move case bodies to fragments. */
#include "semantic_regression_callable_flow.inc"
#include "semantic_regression_scope_cf.inc"
int main(void) {
    int ok = 1;

    ok &= test_semantic_accepts_valid_program();
    ok &= test_semantic_rejects_duplicate_function();
    ok &= test_semantic_allows_function_declaration_then_definition();
    ok &= test_semantic_allows_duplicate_function_declarations();
    ok &= test_semantic_rejects_function_declaration_count_mismatch();
    ok &= test_semantic_rejects_declaration_definition_count_mismatch();
    ok &= test_semantic_rejects_function_variable_conflict();
    ok &= test_semantic_rejects_variable_function_conflict();
    ok &= test_semantic_allows_repeated_declaration();
    ok &= test_semantic_rejects_duplicate_initialized_top_level_definition();
    ok &= test_semantic_allows_initialized_definition_with_redeclaration();
    ok &= test_semantic_allows_unnamed_external_contract();
    ok &= test_semantic_rejects_null_program_contract();
    ok &= test_semantic_rejects_function_definition_without_full_return();
    ok &= test_semantic_accepts_function_definition_with_full_return();
    ok &= test_semantic_prioritizes_scope_diagnostic_over_return_flow();
    ok &= test_semantic_prioritizes_depth_diagnostic_over_return_flow();
    ok &= test_semantic_rejects_unreachable_return_after_while_true();
    ok &= test_semantic_rejects_unreachable_return_after_for_true();
    ok &= test_semantic_rejects_call_guard_return_only_without_tail_return();
    ok &= test_semantic_accepts_reachable_return_after_break_in_while_true();
    ok &= test_semantic_rejects_call_to_undeclared_function();
    ok &= test_semantic_rejects_call_to_non_function_symbol();
    ok &= test_semantic_reports_call_site_for_callable_diagnostic();
    ok &= test_semantic_rejects_call_argument_count_mismatch();
    ok &= test_semantic_call_argument_mismatch_long_name_not_truncated();
    ok &= test_semantic_rejects_missing_function_body_during_ast_primary_phase();
    ok &= test_semantic_rejects_chained_call_as_unsupported_indirect_call();
    ok &= test_semantic_rejects_parenthesized_call_result_forms();
    ok &= test_semantic_rejects_parenthesized_non_identifier_callee_form();
    ok &= test_semantic_rejects_metadata_only_definition_during_ast_primary_phase();
    ok &= test_semantic_callable_diagnostic_matrix();
    ok &= test_semantic_callable_accept_matrix();
    ok &= test_semantic_accepts_call_to_declared_function();
    ok &= test_semantic_rejects_call_before_visible_declaration();
    ok &= test_semantic_top_level_initializer_semantic_matrix();
    ok &= test_semantic_rejects_excessive_expression_depth();
    ok &= test_semantic_rejects_duplicate_local_declaration_same_scope();
    ok &= test_semantic_rejects_undeclared_local_identifier_use();
    ok &= test_semantic_rejects_call_with_undeclared_argument_identifier();
    ok &= test_semantic_rejects_undeclared_call_argument_across_statement_slots();
    ok &= test_semantic_allows_block_shadowing();
    ok &= test_semantic_allows_for_init_declaration_visibility();
    ok &= test_semantic_rejects_parameter_redeclaration_in_function_body();
    ok &= test_semantic_rejects_for_init_identifier_use_outside_loop();
    ok &= test_semantic_allows_for_init_shadowing_outer_identifier();
    ok &= test_semantic_rejects_duplicate_parameter_names();
    ok &= test_semantic_rejects_block_local_use_after_block_end();
    ok &= test_semantic_rejects_for_body_local_use_after_loop();
    ok &= test_semantic_allows_parameter_shadowing_in_inner_block();
    ok &= test_semantic_rejects_undeclared_identifier_in_declaration_initializer();
    ok &= test_semantic_rejects_undeclared_identifier_in_for_init_declaration_initializer();
    ok &= test_semantic_rejects_same_declaration_forward_reference();
    ok &= test_semantic_allows_same_declaration_reverse_reference();
    ok &= test_semantic_rejects_for_init_same_declaration_forward_reference();
    ok &= test_semantic_allows_for_init_same_declaration_reverse_reference();
    ok &= test_semantic_ast_primary_return_flow_ignores_tampered_metadata();
    ok &= test_semantic_ast_primary_callable_ignores_tampered_metadata();
    ok &= test_semantic_ast_primary_chained_call_ignores_tampered_metadata();
    ok &= test_semantic_accepts_call_with_prior_prototype_then_later_definition();
    ok &= run_must_pass_or_fail_now_group();

    /*
     * Run the control-flow policy groups explicitly so conservative-accepting
     * loop behavior stays visible in one place.
     */
    ok &= run_conservative_loop_acceptance_group();

    ok &= run_deterministic_loop_rejects_group();

    if (!ok) {
        return 1;
    }

    printf("[semantic-reg] All semantic regressions passed.\n");
    return 0;
}
