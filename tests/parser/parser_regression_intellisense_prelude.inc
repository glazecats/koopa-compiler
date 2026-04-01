#if defined(__INTELLISENSE__)
#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PARSER_EXPR_CALLABLE_CASE_TYPEDEF
#define PARSER_EXPR_CALLABLE_CASE_TYPEDEF
typedef struct {
    const char *case_name;
    const char *source;
    int expect_success;
    const char *required_msg_a;
    const char *required_msg_b;
} ParserExprCallableCase;
#endif

static int run_expression_callable_case(const ParserExprCallableCase *c);
static int parse_expression_source_to_ast(const char *source,
    TokenArray *tokens,
    AstExpression **out_expression,
    ParserError *err);
static int expect_expression_assignment_parse_failure(const char *source,
    const char *case_name,
    const char *required_msg_a,
    const char *required_msg_b);
#endif
