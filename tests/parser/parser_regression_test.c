#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_translation_unit_parse_failure(const char *source,
                                                 const char *case_name,
                                                 const char *required_msg_a,
                                                 const char *required_msg_b);

/*
 * Split fragment inventory (keep this list in sync with includes below):
 * - parser_regression_cases_core.inc
 * - parser_regression_cases_expr_ast_a.inc
 * - parser_regression_cases_expr_ast_b.inc
 * - parser_regression_cases_ast_meta.inc
 */
#include "parser_regression_cases_core.inc"
#include "parser_regression_cases_expr_ast_a.inc"
#include "parser_regression_cases_expr_ast_b.inc"
#include "parser_regression_cases_ast_meta.inc"
int main(void) {
    if (!test_reject_stream_without_eof()) {
        return 1;
    }
    if (!test_backtracking_error_not_polluted()) {
        return 1;
    }
    if (!test_farthest_failure_preferred()) {
        return 1;
    }
    if (!test_success_clears_error_state()) {
        return 1;
    }
    if (!test_translation_unit_accepts_conditional_then_comma_in_return()) {
        return 1;
    }
    if (!test_ast_accepts_conditional_then_comma_initializer_declarator_list()) {
        return 1;
    }
    if (!test_translation_unit_rejects_prefix_increment_non_lvalue()) {
        return 1;
    }
    if (!test_translation_unit_rejects_postfix_increment_non_lvalue()) {
        return 1;
    }
    if (!test_translation_unit_accepts_function_call_expressions()) {
        return 1;
    }
    if (!test_translation_unit_rejects_non_callable_number_callee()) {
        return 1;
    }
    if (!test_translation_unit_accepts_parenthesized_callable_callee()) {
        return 1;
    }
    if (!test_translation_unit_accepts_parenthesized_non_identifier_callee()) {
        return 1;
    }
    if (!test_translation_unit_rejects_non_parenthesized_non_identifier_callee()) {
        return 1;
    }
    if (!test_translation_unit_accepts_parenthesized_call_result_chaining()) {
        return 1;
    }
    if (!test_translation_unit_accepts_parenthesized_zero_arg_call_result_chaining()) {
        return 1;
    }
    if (!test_translation_unit_accepts_parenthesized_identifier_increment()) {
        return 1;
    }
    if (!test_translation_unit_accepts_compound_assignments()) {
        return 1;
    }
    if (!test_translation_unit_rejects_compound_assignment_missing_rhs_plus()) {
        return 1;
    }
    if (!test_translation_unit_rejects_compound_assignment_missing_rhs_shift_left()) {
        return 1;
    }
    if (!test_translation_unit_rejects_compound_assignment_number_lhs()) {
        return 1;
    }
    if (!test_translation_unit_rejects_compound_assignment_parenthesized_identifier_lhs()) {
        return 1;
    }

    /* Translation-unit call-argument negative boundaries. */
    if (!test_translation_unit_rejects_call_missing_first_argument()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_missing_argument_after_comma()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_missing_closing_paren()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_double_comma_in_arguments()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_missing_comma_between_arguments()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_semicolon_in_arguments()) {
        return 1;
    }
    if (!test_translation_unit_rejects_call_incomplete_argument_expression()) {
        return 1;
    }
    if (!test_translation_unit_callable_entry_matrix()) {
        return 1;
    }

    /* Expression AST positive-shape coverage. */
    if (!test_expression_ast_parses_identifier_primary()) {
        return 1;
    }
    if (!test_expression_ast_parses_number_primary()) {
        return 1;
    }
    if (!test_expression_ast_parses_nested_parentheses()) {
        return 1;
    }
    if (!test_expression_ast_rejects_trailing_tokens()) {
        return 1;
    }
    if (!test_expression_ast_rejects_deep_parentheses_with_recursion_limit()) {
        return 1;
    }
    if (!test_expression_ast_precedence_add_then_mul()) {
        return 1;
    }
    if (!test_expression_ast_parentheses_override_precedence()) {
        return 1;
    }
    if (!test_expression_ast_subtraction_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_unary_minus_primary()) {
        return 1;
    }
    if (!test_expression_ast_unary_chain_right_associative()) {
        return 1;
    }
    if (!test_expression_ast_unary_binds_tighter_than_multiplicative()) {
        return 1;
    }
    if (!test_expression_ast_unary_logical_not_primary()) {
        return 1;
    }
    if (!test_expression_ast_unary_bitwise_not_primary()) {
        return 1;
    }
    if (!test_expression_ast_unary_prefix_increment_primary()) {
        return 1;
    }
    if (!test_expression_ast_unary_prefix_decrement_primary()) {
        return 1;
    }
    if (!test_expression_ast_postfix_increment_primary()) {
        return 1;
    }
    if (!test_expression_ast_postfix_decrement_primary()) {
        return 1;
    }
    if (!test_expression_ast_postfix_binds_tighter_than_multiplicative()) {
        return 1;
    }
    if (!test_expression_ast_call_no_args_primary()) {
        return 1;
    }
    if (!test_expression_ast_call_with_args_primary()) {
        return 1;
    }
    if (!test_expression_ast_call_binds_tighter_than_additive()) {
        return 1;
    }
    if (!test_expression_ast_accepts_call_chaining()) {
        return 1;
    }
    if (!test_expression_ast_accepts_parenthesized_callable_callee()) {
        return 1;
    }
    if (!test_expression_ast_accepts_parenthesized_non_identifier_callee()) {
        return 1;
    }
    if (!test_expression_ast_rejects_non_parenthesized_non_identifier_callee()) {
        return 1;
    }
    if (!test_expression_ast_accepts_nested_parenthesized_call_chaining()) {
        return 1;
    }
    if (!test_expression_ast_accepts_parenthesized_call_result_chaining()) {
        return 1;
    }
    if (!test_expression_ast_accepts_parenthesized_zero_arg_call_result_chaining()) {
        return 1;
    }
    if (!test_expression_ast_assignment_rhs_unary()) {
        return 1;
    }
    if (!test_expression_ast_assignment_rhs_logical_not()) {
        return 1;
    }
    if (!test_expression_ast_assignment_rhs_bitwise_not()) {
        return 1;
    }
    if (!test_expression_ast_unary_parenthesized_additive()) {
        return 1;
    }
    if (!test_expression_ast_multiplicative_right_unary()) {
        return 1;
    }

    /* Expression AST compatibility alias lock. */
    if (!test_expression_ast_equality_alias_still_forwards_assignment()) {
        return 1;
    }

    /* Expression AST negative-boundary coverage. */
    if (!test_expression_ast_rejects_assignment_rhs_incomplete_unary()) {
        return 1;
    }
    if (!test_expression_ast_rejects_assignment_rhs_incomplete_logical_not()) {
        return 1;
    }
    if (!test_expression_ast_rejects_assignment_rhs_incomplete_bitwise_not()) {
        return 1;
    }
    if (!test_expression_ast_rejects_assignment_rhs_incomplete_prefix_increment()) {
        return 1;
    }
    if (!test_expression_ast_rejects_prefix_increment_number_operand()) {
        return 1;
    }
    if (!test_expression_ast_rejects_postfix_increment_number_operand()) {
        return 1;
    }
    if (!test_expression_ast_rejects_prefix_increment_parenthesized_additive()) {
        return 1;
    }
    if (!test_expression_ast_rejects_postfix_increment_parenthesized_additive()) {
        return 1;
    }
    if (!test_expression_ast_rejects_prefix_increment_postfix_operand()) {
        return 1;
    }
    if (!test_expression_ast_accepts_prefix_increment_parenthesized_identifier()) {
        return 1;
    }
    if (!test_expression_ast_accepts_postfix_increment_parenthesized_identifier()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_postfix_followed_by_plus()) {
        return 1;
    }
    if (!test_expression_ast_rejects_call_missing_closing_paren()) {
        return 1;
    }
    if (!test_expression_ast_rejects_call_missing_argument_after_comma()) {
        return 1;
    }
    if (!test_expression_ast_rejects_non_callable_number_callee()) {
        return 1;
    }
    if (!test_expression_ast_callable_entry_matrix()) {
        return 1;
    }
    if (!test_expression_ast_rejects_multiplicative_rhs_incomplete_unary()) {
        return 1;
    }
    if (!test_expression_ast_rejects_parenthesized_incomplete_additive_operand()) {
        return 1;
    }
    if (!test_expression_ast_rejects_parenthesized_lhs_assignment()) {
        return 1;
    }
    if (!test_expression_ast_rejects_number_lhs_assignment()) {
        return 1;
    }
    if (!test_expression_ast_rejects_binary_lhs_assignment()) {
        return 1;
    }
    if (!test_expression_ast_rejects_parenthesized_assignment_lhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_double_parenthesized_identifier_lhs_assignment()) {
        return 1;
    }
    if (!test_expression_ast_rejects_assignment_to_parenthesized_equality()) {
        return 1;
    }
    if (!test_expression_ast_relational_lower_precedence_than_additive()) {
        return 1;
    }
    if (!test_expression_ast_relational_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_relational_with_parenthesized_right_additive()) {
        return 1;
    }
    if (!test_expression_ast_relational_gt_operator()) {
        return 1;
    }
    if (!test_expression_ast_relational_ge_operator()) {
        return 1;
    }
    if (!test_expression_ast_equality_lower_precedence_than_relational()) {
        return 1;
    }
    if (!test_expression_ast_equality_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_logical_and_lower_precedence_than_equality()) {
        return 1;
    }
    if (!test_expression_ast_logical_or_lower_precedence_than_logical_and()) {
        return 1;
    }
    if (!test_expression_ast_logical_or_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_bitwise_and_lower_precedence_than_equality()) {
        return 1;
    }
    if (!test_expression_ast_shift_lower_precedence_than_additive()) {
        return 1;
    }
    if (!test_expression_ast_relational_lower_precedence_than_shift()) {
        return 1;
    }
    if (!test_expression_ast_shift_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_bitwise_xor_lower_precedence_than_bitwise_and()) {
        return 1;
    }
    if (!test_expression_ast_bitwise_or_lower_precedence_than_bitwise_xor()) {
        return 1;
    }
    if (!test_expression_ast_logical_and_lower_precedence_than_bitwise_or()) {
        return 1;
    }
    if (!test_expression_ast_conditional_lower_precedence_than_logical_or()) {
        return 1;
    }
    if (!test_expression_ast_conditional_right_associative()) {
        return 1;
    }
    if (!test_expression_ast_assignment_parses_simple()) {
        return 1;
    }
    if (!test_expression_ast_assignment_is_right_associative()) {
        return 1;
    }
    if (!test_expression_ast_compound_assignment_parses_simple()) {
        return 1;
    }
    if (!test_expression_ast_compound_assignment_is_right_associative()) {
        return 1;
    }
    if (!test_expression_ast_assignment_lower_precedence_than_equality()) {
        return 1;
    }
    if (!test_expression_ast_equality_with_parenthesized_assignment_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_assignment_after_equality_chain()) {
        return 1;
    }
    if (!test_expression_ast_assignment_lower_precedence_than_logical_or()) {
        return 1;
    }
    if (!test_expression_ast_compound_assignment_lower_precedence_than_logical_or()) {
        return 1;
    }
    if (!test_expression_ast_assignment_lower_precedence_than_conditional()) {
        return 1;
    }
    if (!test_expression_ast_conditional_then_branch_allows_comma_expression()) {
        return 1;
    }
    if (!test_expression_ast_rejects_compound_assignment_rhs_incomplete()) {
        return 1;
    }
    if (!test_expression_ast_comma_still_lowest_around_conditional()) {
        return 1;
    }
    if (!test_expression_ast_conditional_else_parenthesized_comma()) {
        return 1;
    }
    if (!test_expression_ast_comma_lower_precedence_than_assignment()) {
        return 1;
    }
    if (!test_expression_ast_comma_left_associative()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_logical_and_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_logical_or_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_bitwise_and_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_bitwise_xor_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_bitwise_or_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_shift_left_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_shift_right_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_conditional_missing_colon()) {
        return 1;
    }
    if (!test_expression_ast_rejects_conditional_incomplete_else_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_incomplete_comma_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_conditional_then_comma_missing_middle_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_conditional_missing_then_rhs()) {
        return 1;
    }
    if (!test_expression_ast_rejects_conditional_else_trailing_comma()) {
        return 1;
    }
    if (!test_ast_collects_top_level_externals()) {
        return 1;
    }
    if (!test_ast_records_function_parameter_count()) {
        return 1;
    }
    if (!test_ast_parses_function_declaration_external()) {
        return 1;
    }
    if (!test_ast_statement_expression_slots_for_control_flow()) {
        return 1;
    }
    if (!test_ast_records_scope_declaration_and_parameter_names()) {
        return 1;
    }
    if (!test_ast_parses_unnamed_parameter_prototype()) {
        return 1;
    }
    if (!test_ast_rejects_unnamed_parameter_definition()) {
        return 1;
    }
    if (!test_ast_records_multiple_function_returns()) {
        return 1;
    }
    if (!test_ast_partial_return_not_all_paths()) {
        return 1;
    }
    if (!test_ast_accepts_break_continue_inside_loops()) {
        return 1;
    }
    if (!test_ast_rejects_break_outside_loop()) {
        return 1;
    }
    if (!test_ast_rejects_continue_outside_loop()) {
        return 1;
    }
    if (!test_ast_records_declaration_initializer_metadata()) {
        return 1;
    }
    if (!test_ast_aligns_initializer_slots_with_declarator_order()) {
        return 1;
    }
    if (!test_ast_collects_all_top_level_declarators()) {
        return 1;
    }
    if (!test_ast_failure_clears_previous_program()) {
        return 1;
    }
    if (!test_deep_assignment_hits_recursion_limit()) {
        return 1;
    }
    if (!test_deep_statement_nesting_hits_recursion_limit()) {
        return 1;
    }

    printf("[parser-reg] All parser regressions passed.\n");
    return 0;
}
