#if defined(__INTELLISENSE__)
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int parse_source_to_ast(const char *source,
                               TokenArray *tokens,
                               AstProgram *program,
                               ParserError *parse_err);
static int run_callable_diag_case(const CallableDiagCase *c);
static int run_callable_pass_case(const CallablePassCase *c);
#endif
