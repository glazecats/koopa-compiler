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

static int run_extension_semantic_expect_accepts(const char *case_id, const char *source);
static int run_extension_semantic_expect_rejects(const char *case_id,
    const char *source,
    const char *expected_code);

/* Split for maintainability: keep regression execution order in main, move case bodies to fragments. */
#include "semantic_regression_callable_flow.inc"
#include "semantic_regression_scope_cf.inc"
int main(void) {
    const char *filter = getenv("SEMANTIC_REG_FILTER");
    int ok = 1;

    if (filter && filter[0] != '\0') {
        if (strstr("SEMANTIC-FLOAT-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_float_function_signature_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-LITERAL-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-RETURN-GLOBAL", filter) != NULL) {
            return test_semantic_accepts_float_return_transport_from_global_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-RETURN-GLOBAL-CALL", filter) != NULL) {
            return test_semantic_accepts_float_return_transport_from_global_call_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-PARAM-FORWARD", filter) != NULL) {
            return test_semantic_accepts_float_parameter_forward_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-PARAM-LOCAL-FORWARD", filter) != NULL) {
            return test_semantic_accepts_float_parameter_local_forward_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-GLOBAL-CALL-CHAIN", filter) != NULL) {
            return test_semantic_accepts_float_global_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-LOCAL-CALL-CHAIN", filter) != NULL) {
            return test_semantic_accepts_float_local_call_chain_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-IF-COND-ACCEPT", filter) != NULL) {
            return test_semantic_rejects_float_if_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-WHILE-COND-ACCEPT", filter) != NULL) {
            return test_semantic_rejects_float_while_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-FOR-COND-ACCEPT", filter) != NULL) {
            return test_semantic_rejects_float_for_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NOT-COND-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_not_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-AND-COND-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_and_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-OR-COND-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_or_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-COND-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_ternary_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_same_type_float_ternary_value_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-GLOBAL-OP-REJECT", filter) != NULL) {
            return test_semantic_rejects_global_float_operator_expression_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-GLOBAL-OP-INIT-REJECT", filter) != NULL) {
            return test_semantic_rejects_global_float_operator_expression_in_initializer_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-ASSIGN-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_float_assignment_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-ASSIGN-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-RETURN-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-RETURN-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_return_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EQ-COMPARE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_equality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NE-COMPARE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_inequality_compare_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-LT-COMPARE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_relational_compare_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-LITERAL-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_negative_float_literal_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-IDENT-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_unary_minus_float_identifier_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-CALL-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_unary_minus_float_call_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-POS-IDENT-TRANSPORT", filter) != NULL) {
            return test_semantic_accepts_unary_plus_float_identifier_transport_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-ZERO-COND-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_zero_float_condition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_relational_compare_against_zero_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-LT-NEGATIVE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_relational_compare_against_negative_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-ZERO-LT-ZERO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_zero_lt_zero_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-ZERO-LE-ZERO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_zero_le_zero_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-ADD-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-SUB-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_subtraction_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-ADD-COMBO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_addition_combo_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-SUB-COMBO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_subtraction_combo_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-MUL-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_multiplication_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-DIV-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_division_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-MUL-COMBO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_multiplication_combo_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-DIV-COMBO-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_negative_float_division_combo_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-CHAIN-ADD-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_chained_float_addition_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NESTED-MUL-DIV-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_nested_float_mul_div_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_mixed_float_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-LITERAL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_literal_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NEG-CALL-ARITH-INT-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_negative_float_call_int_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-COMPARE-INT-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NESTED-TREE-PLUS-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_nested_float_tree_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-NESTED-MULDIV-PLUS-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_nested_float_muldiv_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-PLUS-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-PLUS-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_plus_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-PLUS-FLOAT-CALLARG-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-ASSIGN-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_assignment_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_same_type_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-ASSIGN-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_unary_call_same_type_float_ternary_value_assignment_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-INIT-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_same_type_float_ternary_value_initializer_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-COMPARE-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-COMPARE-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_compare_against_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-COMPARE-FLOAT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_compare_against_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-CALLARG-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-CALLARG-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_call_argument_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_same_type_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-CALLARG-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_unary_call_same_type_float_ternary_value_call_argument_to_float_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-HELPER-TERNARY-CALL-ARITH-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_float_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-HELPER-TERNARY-CALL-COMPARE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-TERNARY-VALUE-INIT-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_float_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-UNARY-CALL-TERNARY-INIT-INT-REJECT", filter) != NULL) {
            return test_semantic_rejects_unary_call_ternary_value_initializer_to_int_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-FLOAT-DIRECT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-INT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_float_from_int_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-SAME-TYPE-REJECT", filter) != NULL) {
            return test_semantic_rejects_redundant_explicit_same_type_conversion_for_now() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-FLOAT-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_float_conversion_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-FLOAT-TERNARY-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_float_ternary_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-CALLARG-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-FLOAT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_float_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-INT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-INIT-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ASSIGN-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-INT-COMPARE-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_float_from_int_compare_bridge_under_extension() ? 0 : 1;
        }
        if (strstr("SEMANTIC-FLOAT-EXPLICIT-FLOAT-FROM-RECURSIVE-ARITH-BRIDGE-ACCEPT", filter) != NULL) {
            return test_semantic_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension() ? 0 : 1;
        }
        fprintf(stderr, "[semantic-reg] FAIL: unknown filter '%s'\n", filter);
        return 1;
    }

    ok &= test_semantic_accepts_valid_program();
    ok &= test_semantic_rejects_duplicate_function();
    ok &= test_semantic_allows_function_declaration_then_definition();
    ok &= test_semantic_allows_duplicate_function_declarations();
    ok &= test_semantic_rejects_function_declaration_count_mismatch();
    ok &= test_semantic_rejects_declaration_definition_count_mismatch();
    ok &= test_semantic_rejects_function_declaration_parameter_kind_mismatch();
    ok &= test_semantic_rejects_function_declaration_nested_signature_mismatch();
    ok &= test_semantic_rejects_function_variable_conflict();
    ok &= test_semantic_rejects_variable_function_conflict();
    ok &= test_semantic_allows_repeated_declaration();
    ok &= test_semantic_rejects_repeated_top_level_declaration_type_mismatch();
    ok &= test_semantic_rejects_repeated_top_level_array_rank_mismatch();
    ok &= test_semantic_rejects_repeated_top_level_array_extent_mismatch();
    ok &= test_semantic_accepts_repeated_top_level_array_extent_constant_equivalence();
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
    ok &= test_semantic_rejects_partial_return_inside_while_true();
    ok &= test_semantic_rejects_partial_return_inside_for_true();
    ok &= test_semantic_accepts_all_path_return_inside_while_true();
    ok &= test_semantic_accepts_all_path_return_inside_for_true();
    ok &= test_semantic_accepts_nested_all_path_return_inside_while_true();
    ok &= test_semantic_rejects_nested_partial_return_inside_while_true();
    ok &= test_semantic_accepts_mixed_nested_all_path_return_inside_while_true();
    ok &= test_semantic_rejects_mixed_nested_partial_return_inside_while_true();
    ok &= test_semantic_accepts_nested_all_path_return_inside_for_true();
    ok &= test_semantic_rejects_nested_partial_return_inside_for_true();
    ok &= test_semantic_accepts_mixed_nested_all_path_return_inside_for_true();
    ok &= test_semantic_rejects_mixed_nested_partial_return_inside_for_true();
    ok &= test_semantic_accepts_mixed_nested_break_guard_set_true_in_while_true();
    ok &= test_semantic_rejects_mixed_nested_break_guard_reset_false_in_while_true();
    ok &= test_semantic_accepts_mixed_nested_break_guard_set_true_in_for_true();
    ok &= test_semantic_rejects_mixed_nested_break_guard_reset_false_in_for_true();
    ok &= test_semantic_accepts_else_break_guard_reset_false_in_while_true();
    ok &= test_semantic_rejects_else_break_guard_stays_true_in_while_true();
    ok &= test_semantic_accepts_else_break_guard_reset_false_in_for_true();
    ok &= test_semantic_rejects_else_break_guard_stays_true_in_for_true();
    ok &= test_semantic_rejects_constant_true_break_guard_reset_false_in_while_true();
    ok &= test_semantic_rejects_constant_false_guard_before_break_in_while_true();
    ok &= test_semantic_accepts_constant_true_break_guard_set_true_in_while_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_in_while_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_in_for_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_in_while_true_else_path();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_return_same_iteration_in_while_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_return_same_iteration_in_for_true();
    ok &= test_semantic_accepts_nested_loop_guard_set_true_then_outer_return_in_while_true();
    ok &= test_semantic_accepts_nested_loop_guard_set_true_then_outer_return_in_for_true();
    ok &= test_semantic_accepts_nested_for_guard_set_true_then_outer_return_in_while_true();
    ok &= test_semantic_accepts_nested_loop_guard_set_true_then_return_same_iteration_in_while_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_continue_in_while_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_continue_in_for_true();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_continue_in_while_true_else_path();
    ok &= test_semantic_accepts_constant_true_return_guard_set_true_then_continue_in_for_true_else_path();
    ok &= test_semantic_accepts_nested_inner_while_break_else_continue_then_outer_return();
    ok &= test_semantic_accepts_nested_inner_for_break_else_continue_then_outer_return();
    ok &= test_semantic_accepts_outer_for_nested_inner_while_break_else_continue_then_outer_return();
    ok &= test_semantic_accepts_outer_for_nested_inner_for_break_else_continue_then_outer_return();
    ok &= test_semantic_rejects_path_refined_zero_guard_in_while_true();
    ok &= test_semantic_rejects_constant_true_break_guard_reset_false_in_for_true();
    ok &= test_semantic_rejects_constant_true_break_guard_step_reset_false_in_for_true();
    ok &= test_semantic_accepts_constant_true_break_guard_step_set_true_in_for_true();
    ok &= test_semantic_rejects_path_refined_zero_guard_in_for_true_step();
    ok &= test_semantic_rejects_constant_true_break_guard_body_true_but_step_reset_false_in_for_true();
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
    ok &= test_semantic_accepts_subscript_identifier_visibility();
    ok &= test_semantic_rejects_undeclared_identifier_in_subscript_index();
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
    ok &= test_semantic_rejects_const_local_without_initializer();
    ok &= test_semantic_rejects_const_top_level_without_initializer();
    ok &= test_semantic_rejects_assignment_to_const();
    ok &= test_semantic_rejects_assignment_to_const_parameter();
    ok &= test_semantic_rejects_assignment_to_const_array_element();
    ok &= test_semantic_rejects_increment_of_const_array_element();
    ok &= test_semantic_accepts_simple_defer_under_extension();
    ok &= test_semantic_accepts_multiple_simple_defers_under_extension();
    ok &= test_semantic_rejects_defer_outside_extension_mode();
    ok &= test_semantic_rejects_defer_return_under_extension();
    ok &= test_semantic_rejects_defer_break_under_extension();
    ok &= test_semantic_rejects_defer_continue_under_extension();
    ok &= test_semantic_accepts_nested_defer_under_extension();
    ok &= test_semantic_accepts_nested_multi_defer_in_defer_body_under_extension();
    ok &= test_semantic_rejects_nested_return_in_defer_body_under_extension();
    ok &= test_semantic_rejects_nested_break_in_defer_body_under_extension();
    ok &= test_semantic_rejects_nested_continue_in_defer_body_under_extension();
    ok &= test_semantic_accepts_defer_body_internal_break_under_extension();
    ok &= test_semantic_accepts_defer_body_internal_continue_under_extension();
    ok &= test_semantic_accepts_unless_under_extension();
    ok &= test_semantic_rejects_unless_outside_extension_mode();
    ok &= test_semantic_accepts_fndefer_with_stable_names_under_extension();
    ok &= test_semantic_rejects_fndefer_outside_extension_mode();
    ok &= test_semantic_rejects_fndefer_nested_block_local_under_extension();
    ok &= test_semantic_rejects_fndefer_loop_local_under_extension();
    ok &= test_semantic_rejects_fndefer_nested_defer_under_extension();
    ok &= test_semantic_rejects_fndefer_loop_body_under_extension();
    ok &= test_semantic_rejects_fndefer_later_local_declaration_under_extension();
    ok &= test_semantic_accepts_fndefer_conditional_registration_under_extension();
    ok &= test_semantic_accepts_capdefer_with_stable_names_under_extension();
    ok &= test_semantic_accepts_float_declaration_under_extension();
    ok &= test_semantic_accepts_float_function_signature_transport_under_extension();
    ok &= test_semantic_accepts_float_literal_transport_under_extension();
    ok &= test_semantic_accepts_float_return_transport_from_global_under_extension();
    ok &= test_semantic_accepts_float_return_transport_from_global_call_under_extension();
    ok &= test_semantic_accepts_float_parameter_forward_transport_under_extension();
    ok &= test_semantic_accepts_float_parameter_local_forward_transport_under_extension();
    ok &= test_semantic_accepts_float_global_call_chain_transport_under_extension();
    ok &= test_semantic_accepts_float_local_call_chain_transport_under_extension();
    ok &= test_semantic_rejects_float_if_condition_under_extension();
    ok &= test_semantic_rejects_float_while_condition_under_extension();
    ok &= test_semantic_rejects_float_for_condition_under_extension();
    ok &= test_semantic_accepts_float_not_condition_under_extension();
    ok &= test_semantic_accepts_float_and_condition_under_extension();
    ok &= test_semantic_accepts_float_or_condition_under_extension();
    ok &= test_semantic_accepts_float_ternary_condition_under_extension();
    ok &= test_semantic_accepts_same_type_float_ternary_value_under_extension();
    ok &= test_semantic_rejects_global_float_operator_expression_under_extension();
    ok &= test_semantic_rejects_global_float_operator_expression_in_initializer_under_extension();
    ok &= test_semantic_accepts_float_assignment_transport_under_extension();
    ok &= test_semantic_rejects_float_assignment_to_int_under_extension();
    ok &= test_semantic_accepts_float_equality_compare_under_extension();
    ok &= test_semantic_accepts_float_inequality_compare_under_extension();
    ok &= test_semantic_accepts_float_relational_compare_under_extension();
    ok &= test_semantic_accepts_negative_float_literal_transport_under_extension();
    ok &= test_semantic_accepts_unary_minus_float_identifier_transport_under_extension();
    ok &= test_semantic_accepts_unary_minus_float_call_transport_under_extension();
    ok &= test_semantic_accepts_unary_plus_float_identifier_transport_under_extension();
    ok &= test_semantic_accepts_negative_zero_float_condition_under_extension();
    ok &= test_semantic_accepts_negative_float_relational_compare_against_zero_under_extension();
    ok &= test_semantic_accepts_negative_float_relational_compare_against_negative_under_extension();
    ok &= test_semantic_accepts_negative_zero_lt_zero_under_extension();
    ok &= test_semantic_accepts_negative_zero_le_zero_under_extension();
    ok &= test_semantic_accepts_float_addition_under_extension();
    ok &= test_semantic_accepts_float_subtraction_under_extension();
    ok &= test_semantic_accepts_negative_float_addition_combo_under_extension();
    ok &= test_semantic_accepts_negative_float_subtraction_combo_under_extension();
    ok &= test_semantic_accepts_float_multiplication_under_extension();
    ok &= test_semantic_accepts_float_division_under_extension();
    ok &= test_semantic_accepts_negative_float_multiplication_combo_under_extension();
    ok &= test_semantic_accepts_negative_float_division_combo_under_extension();
    ok &= test_semantic_accepts_chained_float_addition_under_extension();
    ok &= test_semantic_accepts_nested_float_mul_div_under_extension();
    ok &= test_semantic_rejects_mixed_float_int_arithmetic_under_extension();
    ok &= test_semantic_rejects_float_literal_int_arithmetic_under_extension();
    ok &= test_semantic_rejects_float_call_int_arithmetic_under_extension();
    ok &= test_semantic_rejects_negative_float_call_int_arithmetic_under_extension();
    ok &= test_semantic_rejects_float_compare_against_int_under_extension();
    ok &= test_semantic_rejects_nested_float_tree_plus_int_under_extension();
    ok &= test_semantic_rejects_nested_float_muldiv_plus_int_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_plus_int_under_extension();
    ok &= test_semantic_rejects_unary_call_ternary_value_plus_int_under_extension();
    ok &= test_semantic_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_semantic_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_semantic_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_assignment_to_int_under_extension();
    ok &= test_semantic_accepts_same_type_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_semantic_accepts_unary_call_same_type_float_ternary_value_assignment_to_float_under_extension();
    ok &= test_semantic_accepts_same_type_float_ternary_value_initializer_to_float_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_compare_against_int_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_compare_against_float_under_extension();
    ok &= test_semantic_rejects_unary_call_ternary_value_compare_against_float_under_extension();
    ok &= test_semantic_rejects_float_ternary_value_call_argument_to_int_under_extension();
    ok &= test_semantic_rejects_unary_call_ternary_value_call_argument_to_int_under_extension();
    ok &= test_semantic_accepts_same_type_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_semantic_accepts_unary_call_same_type_float_ternary_value_call_argument_to_float_under_extension();
    ok &= test_semantic_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_semantic_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension();
    ok &= test_semantic_accepts_float_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_semantic_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension();
    ok &= test_semantic_accepts_explicit_float_from_int_conversion_under_extension();
    ok &= test_semantic_rejects_redundant_explicit_same_type_conversion_for_now();
    ok &= test_semantic_accepts_explicit_int_from_float_conversion_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_float_ternary_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_float_compare_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_float_from_int_compare_bridge_under_extension();
    ok &= test_semantic_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension();
    ok &= test_semantic_rejects_capdefer_outside_extension_mode();
    ok &= test_semantic_rejects_capdefer_nested_block_local_under_extension();
    ok &= test_semantic_rejects_capdefer_loop_registration_under_extension();
    ok &= test_semantic_rejects_function_valued_parameter_extension_slice_for_now();
    ok &= test_semantic_rejects_function_valued_parameter_outside_extension_mode();
    ok &= test_semantic_accepts_function_valued_parameter_declaration_under_extension_when_unused();
    ok &= test_semantic_rejects_calling_function_with_function_valued_parameter_under_extension_for_now();
    ok &= test_semantic_rejects_non_function_arg_to_function_valued_parameter_call_under_extension_for_now();
    ok &= test_semantic_rejects_top_level_function_value_in_plain_value_position_under_extension_for_now();
    ok &= test_semantic_accepts_direct_call_of_function_valued_parameter_under_extension_for_now();
    ok &= test_semantic_accepts_pair_copy_under_extension();
    ok &= test_semantic_rejects_plain_pair_value_in_int_context_under_extension();
    ok &= test_semantic_rejects_pair_assignment_from_scalar_under_extension();
    ok &= test_semantic_rejects_pair_init_from_scalar_under_extension();
    ok &= test_semantic_rejects_pair_init_list_too_many_items_under_extension();
    ok &= test_semantic_accepts_struct_copy_under_extension();
    ok &= test_semantic_rejects_unknown_struct_type_under_extension();
    ok &= test_semantic_rejects_mismatched_struct_assignment_under_extension();
    ok &= test_semantic_rejects_struct_init_from_scalar_under_extension();
    ok &= test_semantic_rejects_mismatched_struct_copy_init_under_extension();
    ok &= test_semantic_rejects_pair_struct_assignment_mismatch_under_extension();
    ok &= test_semantic_rejects_plain_struct_value_in_int_context_under_extension();
    ok &= test_semantic_rejects_unknown_struct_field_under_extension();
    ok &= test_semantic_accepts_pair_and_struct_member_lookup_under_extension();
    ok &= test_semantic_accepts_float_local_declaration_under_extension();
    ok &= test_semantic_rejects_float_array_local_declaration_under_extension();
    ok &= test_semantic_rejects_float_array_global_declaration_under_extension();
    ok &= test_semantic_rejects_float_array_parameter_under_extension();
    ok &= test_semantic_rejects_pair_array_local_declaration_under_extension();
    ok &= test_semantic_rejects_struct_array_local_declaration_under_extension();
    ok &= test_semantic_accepts_forwarding_function_valued_parameter_under_extension_for_now();
    ok &= test_semantic_accepts_multiple_function_valued_parameters_under_extension_for_now();
    ok &= test_semantic_accepts_void_function_valued_parameter_with_builtin_binding_under_extension();
    ok &= test_semantic_accepts_zero_arg_function_valued_parameter_under_extension();
    ok &= test_semantic_accepts_zero_arg_void_function_valued_parameter_under_extension();
    ok &= test_semantic_rejects_nonvoid_function_for_void_function_valued_parameter_under_extension();
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
