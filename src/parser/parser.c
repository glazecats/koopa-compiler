#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const TokenArray *tokens;
    size_t current;
    ParserError *error;
    int has_error;
    size_t error_index;
    size_t expression_recursion_depth;
    size_t expression_recursion_limit;
    size_t statement_recursion_depth;
    size_t statement_recursion_limit;
    size_t loop_depth;
    int track_function_return_count;
    size_t function_return_count;
    int track_function_calls;
    char **function_called_names;
    int *function_called_lines;
    int *function_called_columns;
    size_t *function_called_arg_counts;
    int *function_called_kinds;
    size_t function_called_count;
    size_t function_called_capacity;
} Parser;

/*
 * Protect recursive-descent hotspots from stack overflow.
 * If the nesting depth is unreasonable, fail gracefully with a parser error.
 */
#define PARSER_EXPRESSION_RECURSION_LIMIT 2048U
#define PARSER_STATEMENT_RECURSION_LIMIT 2048U

#define PARSER_SPLIT_AGGREGATOR 1
#include "parser_ast_compat.inc"
#include "parser_core_expr.inc"
#include "parser_stmt_decl_tu.inc"
#undef PARSER_SPLIT_AGGREGATOR
