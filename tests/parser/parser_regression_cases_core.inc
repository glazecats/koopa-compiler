#include "parser_regression_intellisense_prelude.inc"

static int test_reject_stream_without_eof(void) {
    Token bad_tokens[1];
    TokenArray stream;
    ParserError err;

    bad_tokens[0].type = TOKEN_KW_INT;
    bad_tokens[0].lexeme = "int";
    bad_tokens[0].length = 3;
    bad_tokens[0].number_value = 0;
    bad_tokens[0].line = 1;
    bad_tokens[0].column = 1;

    stream.data = bad_tokens;
    stream.magic = 0;
    stream.size = 1;
    stream.capacity = 1;

    if (parser_parse_translation_unit(&stream, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser accepted token stream without EOF\n");
        return 0;
    }

    if (strstr(err.message, "EOF") == NULL) {
        fprintf(stderr, "[parser-reg] FAIL: expected EOF-related error, got: %s\n", err.message);
        return 0;
    }

    return 1;
}

static int test_backtracking_error_not_polluted(void) {
    const char *source = "int x;\nint y\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for backtracking regression input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly accepted missing semicolon input\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (err.line <= 1) {
        fprintf(stderr, "[parser-reg] FAIL: expected error after first declaration, got line %d\n", err.line);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "';'") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected missing semicolon error, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "'('") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: stale function-parse error leaked into final message: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_farthest_failure_preferred(void) {
    const char *source = "int x(int y,)\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for farthest-failure input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly accepted invalid parameter list\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "'int'") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected deeper parameter-list error, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "';'") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: got earlier declaration-style error instead of farthest failure: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_success_clears_error_state(void) {
    const char *source = "int x;\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for success-state test input\n");
        return 0;
    }

    err.line = -1;
    err.column = -1;
    snprintf(err.message, sizeof(err.message), "stale");

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly failed valid declaration\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (err.line != 0 || err.column != 0 || err.message[0] != '\0') {
        fprintf(stderr,
                "[parser-reg] FAIL: success should clear error state, got line=%d col=%d msg=%s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_conditional_then_comma_in_return(void) {
    const char *source = "int main(){int a,b,c,d;return a?b,c:d;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for conditional-then-comma return input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept conditional-then-comma return input: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_ast_accepts_conditional_then_comma_initializer_declarator_list(void) {
    const char *source = "int a = x ? y,z : w, b;\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for conditional-then-comma initializer input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parser should accept conditional-then-comma initializer input: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 2 declarator externals for conditional initializer input, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].kind != AST_EXTERNAL_DECLARATION ||
        !program.externals[0].name || strcmp(program.externals[0].name, "a") != 0 ||
        program.externals[0].has_initializer != 1 ||
        program.externals[1].kind != AST_EXTERNAL_DECLARATION ||
        !program.externals[1].name || strcmp(program.externals[1].name, "b") != 0 ||
        program.externals[1].has_initializer != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: unexpected declarator metadata for conditional initializer input\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_translation_unit_rejects_prefix_increment_non_lvalue(void) {
    const char *source = "int main(){++1;return 0;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for prefix-increment non-lvalue input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted ++1 in statement context\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "lvalue") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected lvalue diagnostic for ++1, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_rejects_postfix_increment_non_lvalue(void) {
    const char *source = "int main(){1++;return 0;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for postfix-increment non-lvalue input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted 1++ in statement context\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "lvalue") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected lvalue diagnostic for 1++, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_function_call_expressions(void) {
    const char *source =
        "int id(int x){return x;}\n"
        "int main(){int y=0; y=id(1); y=id(y, y+1); return y;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for function-call translation-unit input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept function-call expressions: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_rejects_non_callable_number_callee(void) {
    return expect_translation_unit_parse_failure(
        "int main(){return 1(2);}\n",
        "non-callable number callee",
        "callable",
        NULL);
}

static int test_translation_unit_accepts_parenthesized_callable_callee(void) {
    const char *source =
        "int f(int x){return x;}\n"
        "int main(){return (f)(1);}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for parenthesized callable callee input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept parenthesized callable callee: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_parenthesized_non_identifier_callee(void) {
    const char *source =
        "int f(){return 1;}\n"
        "int main(){return (f+1)();}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for parenthesized non-identifier callee input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept parenthesized non-identifier callee: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_rejects_non_parenthesized_non_identifier_callee(void) {
    return expect_translation_unit_parse_failure(
        "int f(){return 1;}\n"
        "int main(){return f+1();}\n",
        "non-parenthesized non-identifier callee",
        "callable",
        NULL);
}

static int test_translation_unit_accepts_parenthesized_call_result_chaining(void) {
    const char *source =
        "int f(int x){return x;}\n"
        "int main(){return ((f)(1))(2);}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for parenthesized call-result chaining input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept ((f)(1))(2) in TU path: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_parenthesized_zero_arg_call_result_chaining(void) {
    const char *source =
        "int f(){return 1;}\n"
        "int main(){return ((f)())();}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for parenthesized zero-arg call-result chaining input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept ((f)())() in TU path: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_parenthesized_identifier_increment(void) {
    const char *source = "int main(){int a=1; ++(a); (a)++; return a;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for parenthesized identifier increment input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept parenthesized identifier ++ forms: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_accepts_compound_assignments(void) {
    const char *source =
        "int main(){int a=1,b=2,c=3; a+=b; b*=c; c<<=1; a|=c; return a+b+c;}\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for compound-assignment translation-unit input\n");
        return 0;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept compound assignments: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_rejects_compound_assignment_missing_rhs_plus(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int a=1; a+=; return a;}\n",
        "compound assignment missing RHS plus",
        "Expected expression",
        "SEMICOLON");
}

static int test_translation_unit_rejects_compound_assignment_missing_rhs_shift_left(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int a=1; a<<=; return a;}\n",
        "compound assignment missing RHS shift-left",
        "Expected expression",
        "SEMICOLON");
}

static int test_translation_unit_rejects_compound_assignment_number_lhs(void) {
    return expect_translation_unit_parse_failure(
        "int main(){1+=2; return 0;}\n",
        "compound assignment number lhs",
        "';'",
        "PLUS_ASSIGN");
}

static int test_translation_unit_rejects_compound_assignment_parenthesized_identifier_lhs(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int a=1; (a)+=2; return a;}\n",
        "compound assignment parenthesized identifier lhs",
        "';'",
        "PLUS_ASSIGN");
}

static int expect_translation_unit_parse_failure(const char *source,
                                                 const char *case_name,
                                                 const char *required_msg_a,
                                                 const char *required_msg_b) {
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for %s\n",
                case_name);
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted %s\n",
                case_name);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (required_msg_a && strstr(err.message, required_msg_a) == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                required_msg_a,
                case_name,
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (required_msg_b && strstr(err.message, required_msg_b) == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                required_msg_b,
                case_name,
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

typedef struct {
    const char *case_name;
    const char *source;
    int expect_success;
    const char *required_msg_a;
    const char *required_msg_b;
} ParserTuCallableCase;

static int run_translation_unit_callable_case(const ParserTuCallableCase *c) {
    TokenArray tokens;
    ParserError err;

    if (!c || !c->source) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(c->source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for %s\n",
                c->case_name);
        return 0;
    }

    if (c->expect_success) {
        if (!parser_parse_translation_unit(&tokens, &err)) {
            fprintf(stderr,
                    "[parser-reg] FAIL: parser should accept %s: %s\n",
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            return 0;
        }
    } else {
        if (parser_parse_translation_unit(&tokens, &err)) {
            fprintf(stderr,
                    "[parser-reg] FAIL: parser unexpectedly accepted %s\n",
                    c->case_name);
            lexer_free_tokens(&tokens);
            return 0;
        }

        if (c->required_msg_a && strstr(err.message, c->required_msg_a) == NULL) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                    c->required_msg_a,
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            return 0;
        }

        if (c->required_msg_b && strstr(err.message, c->required_msg_b) == NULL) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                    c->required_msg_b,
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_translation_unit_callable_entry_matrix(void) {
    static const ParserTuCallableCase cases[] = {
        {"TU-CALL-ACCEPT-001 direct identifier",
         "int f(int a){return a;}\nint main(){return f(1);}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-002 parenthesized identifier",
         "int f(int a){return a;}\nint main(){return (f)(1);}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-003 nested parenthesized identifier",
         "int f(int a){return a;}\nint main(){return ((f))(1);}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-004 call-result chaining",
         "int f(){return 1;}\nint main(){return f()(1);}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-005 parenthesized call-result chaining",
         "int f(){return 1;}\nint main(){return (f())();}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-006 parenthesized non-identifier plus",
         "int f(){return 1;}\nint main(){return (f+1)();}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-007 nested parenthesized non-identifier plus",
         "int f(){return 1;}\nint main(){return ((f+1))();}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-ACCEPT-008 parenthesized non-identifier ternary",
         "int f(){return 1;}\nint main(){return (f?f:f)();}\n",
         1,
         NULL,
         NULL},
        {"TU-CALL-REJECT-001 non-parenthesized non-identifier plus",
         "int f(){return 1;}\nint main(){return f+1();}\n",
         0,
         "callable",
         NULL},
        {"TU-CALL-REJECT-002 number callee",
         "int main(){return 1(2);}\n",
         0,
         "callable",
         NULL},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_translation_unit_callable_case(&cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_translation_unit_rejects_call_missing_first_argument(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(,1); return y;}\n",
        "call missing first argument",
        "Expected expression",
        "COMMA");
}

static int test_translation_unit_rejects_call_missing_argument_after_comma(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1,); return y;}\n",
        "call missing argument after comma",
        "Expected expression",
        "RPAREN");
}

static int test_translation_unit_rejects_call_missing_closing_paren(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1; return y;}\n",
        "call missing closing paren",
        "')'",
        "SEMICOLON");
}

static int test_translation_unit_rejects_call_double_comma_in_arguments(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1,,2); return y;}\n",
        "call double comma in arguments",
        "Expected expression",
        "COMMA");
}

static int test_translation_unit_rejects_call_missing_comma_between_arguments(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1 2); return y;}\n",
        "call missing comma between arguments",
        "')'",
        "NUMBER");
}

static int test_translation_unit_rejects_call_semicolon_in_arguments(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1;2); return y;}\n",
        "call semicolon in arguments",
        "')'",
        "SEMICOLON");
}

static int test_translation_unit_rejects_call_incomplete_argument_expression(void) {
    return expect_translation_unit_parse_failure(
        "int main(){int y=0; y=id(1,2+); return y;}\n",
        "call incomplete argument expression",
        "Expected expression",
        "RPAREN");
}

static int parse_expression_source_to_ast(const char *source,
                                         TokenArray *tokens,
                                         AstExpression **out_expression,
                                         ParserError *err) {
    lexer_init_tokens(tokens);
    if (out_expression) {
        *out_expression = NULL;
    }

    if (!lexer_tokenize(source, tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for expression AST input\n");
        return 0;
    }

    if (!parser_parse_expression_ast_assignment(tokens, out_expression, err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: expression AST parse failed at %d:%d: %s\n",
                err->line,
                err->column,
                err->message);
        lexer_free_tokens(tokens);
        return 0;
    }

    /* Translation-unit call-argument negative boundaries. */

    return 1;
}

static int expect_expression_assignment_parse_failure(const char *source,
                                                     const char *case_name,
                                                     const char *required_msg_a,
                                                     const char *required_msg_b) {
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for %s\n",
                case_name);
        return 0;
    }

    if (parser_parse_expression_ast_assignment(&tokens, &expr, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: expression parser unexpectedly accepted %s\n",
                source);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    if (required_msg_a && strstr(err.message, required_msg_a) == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                required_msg_a,
                source,
                err.message);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    if (required_msg_b && strstr(err.message, required_msg_b) == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                required_msg_b,
                source,
                err.message);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

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

static int run_expression_callable_case(const ParserExprCallableCase *c) {
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!c || !c->source) {
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(c->source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for %s\n",
                c->case_name);
        return 0;
    }

    if (c->expect_success) {
        if (!parser_parse_expression_ast_assignment(&tokens, &expr, &err)) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expression parser should accept %s: %s\n",
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            ast_expression_free(expr);
            return 0;
        }
    } else {
        if (parser_parse_expression_ast_assignment(&tokens, &expr, &err)) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expression parser unexpectedly accepted %s\n",
                    c->case_name);
            lexer_free_tokens(&tokens);
            ast_expression_free(expr);
            return 0;
        }

        if (c->required_msg_a && strstr(err.message, c->required_msg_a) == NULL) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                    c->required_msg_a,
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            ast_expression_free(expr);
            return 0;
        }

        if (c->required_msg_b && strstr(err.message, c->required_msg_b) == NULL) {
            fprintf(stderr,
                    "[parser-reg] FAIL: expected diagnostic containing '%s' for %s, got: %s\n",
                    c->required_msg_b,
                    c->case_name,
                    err.message);
            lexer_free_tokens(&tokens);
            ast_expression_free(expr);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

