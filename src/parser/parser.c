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
} Parser;

typedef struct {
    int may_fallthrough;
    int may_break;
} ParserLocalControl;

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
