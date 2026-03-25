#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect_translation_unit_parse_failure(const char *source,
                                                 const char *case_name,
                                                 const char *required_msg_a,
                                                 const char *required_msg_b);

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

typedef struct {
    const char *case_name;
    const char *source;
    int expect_success;
    const char *required_msg_a;
    const char *required_msg_b;
} ParserExprCallableCase;

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

static int test_expression_ast_callable_entry_matrix(void) {
    static const ParserExprCallableCase cases[] = {
        {"EXPR-CALL-ACCEPT-001 direct identifier", "f(1)\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-002 parenthesized identifier", "(f)(1)\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-003 nested parenthesized identifier", "((f))(1)\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-004 call-result chaining", "f()(1)\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-005 parenthesized call-result chaining", "(f())()\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-006 parenthesized non-identifier plus", "(f+1)()\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-007 nested parenthesized non-identifier plus", "((f+1))()\n", 1, NULL, NULL},
        {"EXPR-CALL-ACCEPT-008 parenthesized non-identifier ternary", "(f?f:f)()\n", 1, NULL, NULL},
        {"EXPR-CALL-REJECT-001 non-parenthesized non-identifier plus", "f+1()\n", 0, "callable", NULL},
        {"EXPR-CALL-REJECT-002 number callee", "1(2)\n", 0, "callable", NULL},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_expression_callable_case(&cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_expression_ast_parses_identifier_primary(void) {
    const char *source = "abc\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_IDENTIFIER ||
        !expr->as.identifier.name || strcmp(expr->as.identifier.name, "abc") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected identifier expression 'abc'\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_parses_number_primary(void) {
    const char *source = "42\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_NUMBER || expr->as.number_value != 42) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected number expression 42\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_parses_nested_parentheses(void) {
    const char *source = "((a))\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_PAREN || !expr->as.inner ||
        expr->as.inner->kind != AST_EXPR_PAREN || !expr->as.inner->as.inner ||
        expr->as.inner->as.inner->kind != AST_EXPR_IDENTIFIER ||
        !expr->as.inner->as.inner->as.identifier.name ||
        strcmp(expr->as.inner->as.inner->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected nested paren expression ((a)) structure\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_trailing_tokens(void) {
    return expect_expression_assignment_parse_failure("a b\n",
                                                     "trailing-token expression input",
                                                     "EOF",
                                                     NULL);
}

static int test_expression_ast_rejects_deep_parentheses_with_recursion_limit(void) {
    size_t depth = 3000;
    size_t len = depth * 2 + 2;
    char *source = (char *)malloc(len + 1);
    size_t i;
    int ok;

    if (!source) {
        fprintf(stderr,
                "[parser-reg] FAIL: out of memory preparing deep-parentheses input\n");
        return 0;
    }

    for (i = 0; i < depth; ++i) {
        source[i] = '(';
    }
    source[depth] = 'a';
    for (i = 0; i < depth; ++i) {
        source[depth + 1 + i] = ')';
    }
    source[len - 1] = '\n';
    source[len] = '\0';

    ok = expect_expression_assignment_parse_failure(source,
                                                   "deep-parentheses expression input",
                                                   "recursion limit",
                                                   NULL);
    free(source);
    return ok;
}

static int test_expression_ast_precedence_add_then_mul(void) {
    const char *source = "a+b*c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_STAR ||
        !expr->as.binary.right->as.binary.left ||
        expr->as.binary.right->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.binary.right ||
        expr->as.binary.right->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected a+(b*c) precedence tree for a+b*c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_parentheses_override_precedence(void) {
    const char *source = "(a+b)*c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_STAR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_PAREN ||
        !expr->as.binary.left->as.inner || expr->as.binary.left->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.inner->as.binary.op != TOKEN_PLUS ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a+b)*c tree with paren wrapping additive subexpression\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_subtraction_left_associative(void) {
    const char *source = "a-b-c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_MINUS ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_MINUS ||
        !expr->as.binary.left->as.binary.left ||
        expr->as.binary.left->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.left->as.binary.right ||
        expr->as.binary.left->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.binary.right->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a-b)-c left-associative tree for a-b-c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_minus_primary(void) {
    const char *source = "-a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_MINUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unary minus tree -(a) for -a\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_chain_right_associative(void) {
    const char *source = "-+a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_MINUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_UNARY ||
        expr->as.unary.operand->as.unary.op != TOKEN_PLUS ||
        !expr->as.unary.operand->as.unary.operand ||
        expr->as.unary.operand->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unary chain tree -(+a) for -+a\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_binds_tighter_than_multiplicative(void) {
    const char *source = "-a*b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_STAR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_UNARY ||
        expr->as.binary.left->as.unary.op != TOKEN_MINUS ||
        !expr->as.binary.left->as.unary.operand ||
        expr->as.binary.left->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.unary.operand->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (-a)*b tree for unary/multiplicative precedence\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_logical_not_primary(void) {
    const char *source = "!a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_BANG ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unary bang tree !(a) for !a\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_rhs_logical_not(void) {
    const char *source = "a=!b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_UNARY ||
        expr->as.binary.right->as.unary.op != TOKEN_BANG ||
        !expr->as.binary.right->as.unary.operand ||
        expr->as.binary.right->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.unary.operand->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(!b) for a=!b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_bitwise_not_primary(void) {
    const char *source = "~a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_TILDE ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unary tilde tree ~(a) for ~a\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_prefix_increment_primary(void) {
    const char *source = "++a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_PLUS_PLUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected prefix increment tree ++a for ++a\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_prefix_decrement_primary(void) {
    const char *source = "--a\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_MINUS_MINUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected prefix decrement tree --a for --a\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_postfix_increment_primary(void) {
    const char *source = "a++\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_POSTFIX || expr->as.postfix.op != TOKEN_PLUS_PLUS ||
        !expr->as.postfix.operand || expr->as.postfix.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.postfix.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected postfix increment tree a++ for a++\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_postfix_decrement_primary(void) {
    const char *source = "a--\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_POSTFIX || expr->as.postfix.op != TOKEN_MINUS_MINUS ||
        !expr->as.postfix.operand || expr->as.postfix.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.postfix.operand->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected postfix decrement tree a-- for a--\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_postfix_binds_tighter_than_multiplicative(void) {
    const char *source = "a++*b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_STAR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_POSTFIX ||
        expr->as.binary.left->as.postfix.op != TOKEN_PLUS_PLUS ||
        !expr->as.binary.left->as.postfix.operand ||
        expr->as.binary.left->as.postfix.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.postfix.operand->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a++)*b tree for a++*b\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_call_no_args_primary(void) {
    const char *source = "f()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.callee->as.identifier.name, "f") != 0 ||
        expr->as.call.arg_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected call tree f() with zero args for f()\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_call_with_args_primary(void) {
    const char *source = "f(a,b+1)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.callee->as.identifier.name, "f") != 0 ||
        expr->as.call.arg_count != 2 || !expr->as.call.args || !expr->as.call.args[0] ||
        !expr->as.call.args[1] || expr->as.call.args[0]->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.args[0]->as.identifier.name, "a") != 0 ||
        expr->as.call.args[1]->kind != AST_EXPR_BINARY ||
        expr->as.call.args[1]->as.binary.op != TOKEN_PLUS) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected call tree f(a, b+1) for f(a,b+1)\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_call_binds_tighter_than_additive(void) {
    const char *source = "f(a)+b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_CALL ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected additive tree (f(a))+b for f(a)+b\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_call_chaining(void) {
    const char *source = "f()()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_CALL || expr->as.call.arg_count != 0 ||
        !expr->as.call.callee->as.call.callee ||
        expr->as.call.callee->as.call.callee->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.callee->as.call.callee->as.identifier.name, "f") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected nested call tree for f()()\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_parenthesized_callable_callee(void) {
    const char *source = "(f)()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_PAREN || !expr->as.call.callee->as.inner ||
        expr->as.call.callee->as.inner->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.callee->as.inner->as.identifier.name, "f") != 0 ||
        expr->as.call.arg_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected parenthesized callable tree for (f)()\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_parenthesized_non_identifier_callee(void) {
    const char *source = "(f+1)()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_PAREN || !expr->as.call.callee->as.inner ||
        expr->as.call.callee->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.call.callee->as.inner->as.binary.op != TOKEN_PLUS) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected call tree for parenthesized non-identifier callee (f+1)()\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_non_parenthesized_non_identifier_callee(void) {
    return expect_expression_assignment_parse_failure("f+1()\n",
                                                     "non-parenthesized non-identifier callee input",
                                                     "callable",
                                                     NULL);
}

static int test_expression_ast_accepts_nested_parenthesized_call_chaining(void) {
    const char *source = "(f())()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_PAREN || !expr->as.call.callee->as.inner ||
        expr->as.call.callee->as.inner->kind != AST_EXPR_CALL ||
        !expr->as.call.callee->as.inner->as.call.callee ||
        expr->as.call.callee->as.inner->as.call.callee->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.call.callee->as.inner->as.call.callee->as.identifier.name, "f") != 0 ||
        expr->as.call.arg_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected nested parenthesized call chain for (f())()\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_parenthesized_call_result_chaining(void) {
    const char *source = "((f)(1))(2)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_PAREN || !expr->as.call.callee->as.inner ||
        expr->as.call.callee->as.inner->kind != AST_EXPR_CALL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected call tree for ((f)(1))(2)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_parenthesized_zero_arg_call_result_chaining(void) {
    const char *source = "((f)())()\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_CALL || !expr->as.call.callee ||
        expr->as.call.callee->kind != AST_EXPR_PAREN || !expr->as.call.callee->as.inner ||
        expr->as.call.callee->as.inner->kind != AST_EXPR_CALL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected call tree for ((f)())()\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_rhs_bitwise_not(void) {
    const char *source = "a=~b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_UNARY ||
        expr->as.binary.right->as.unary.op != TOKEN_TILDE ||
        !expr->as.binary.right->as.unary.operand ||
        expr->as.binary.right->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.unary.operand->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(~b) for a=~b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_rhs_unary(void) {
    const char *source = "a=-b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_UNARY ||
        expr->as.binary.right->as.unary.op != TOKEN_MINUS ||
        !expr->as.binary.right->as.unary.operand ||
        expr->as.binary.right->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.unary.operand->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(-b) for a=-b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_unary_parenthesized_additive(void) {
    const char *source = "-(a+b)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_MINUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_PAREN ||
        !expr->as.unary.operand->as.inner ||
        expr->as.unary.operand->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.unary.operand->as.inner->as.binary.op != TOKEN_PLUS) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unary paren tree -((a+b)) for -(a+b)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_multiplicative_right_unary(void) {
    const char *source = "a*-b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_STAR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_UNARY ||
        expr->as.binary.right->as.unary.op != TOKEN_MINUS ||
        !expr->as.binary.right->as.unary.operand ||
        expr->as.binary.right->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.unary.operand->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected multiplicative tree a*(-b) for a*-b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

/* Compatibility lock: legacy alias should keep forwarding to comma entrypoint. */
static int test_expression_ast_equality_alias_still_forwards_assignment(void) {
    const char *source = "a=b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for equality-alias compatibility input\n");
        return 0;
    }

    if (!parser_parse_expression_ast_equality(&tokens, &expr, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: equality alias should forward comma-level parser: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

/* Negative boundaries: malformed unary tails and non-lvalue assignment forms. */
static int test_expression_ast_rejects_assignment_rhs_incomplete_unary(void) {
    return expect_expression_assignment_parse_failure("a=+\n",
                                                     "incomplete unary assignment RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_assignment_rhs_incomplete_logical_not(void) {
    return expect_expression_assignment_parse_failure("a=!\n",
                                                     "incomplete logical-not assignment RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_assignment_rhs_incomplete_bitwise_not(void) {
    return expect_expression_assignment_parse_failure("a=~\n",
                                                     "incomplete bitwise-not assignment RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_assignment_rhs_incomplete_prefix_increment(void) {
    return expect_expression_assignment_parse_failure("a=++\n",
                                                     "incomplete prefix-increment assignment RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_prefix_increment_number_operand(void) {
    return expect_expression_assignment_parse_failure("++1\n",
                                                     "prefix increment number operand input",
                                                     "lvalue",
                                                     NULL);
}

static int test_expression_ast_rejects_postfix_increment_number_operand(void) {
    return expect_expression_assignment_parse_failure("1++\n",
                                                     "postfix increment number operand input",
                                                     "lvalue",
                                                     NULL);
}

static int test_expression_ast_rejects_prefix_increment_parenthesized_additive(void) {
    return expect_expression_assignment_parse_failure("++(a+b)\n",
                                                     "prefix increment parenthesized additive input",
                                                     "lvalue",
                                                     NULL);
}

static int test_expression_ast_rejects_postfix_increment_parenthesized_additive(void) {
    return expect_expression_assignment_parse_failure("(a+b)++\n",
                                                     "postfix increment parenthesized additive input",
                                                     "lvalue",
                                                     NULL);
}

static int test_expression_ast_rejects_prefix_increment_postfix_operand(void) {
    return expect_expression_assignment_parse_failure("++a++\n",
                                                     "prefix increment postfix operand input",
                                                     "lvalue",
                                                     NULL);
}

static int test_expression_ast_accepts_prefix_increment_parenthesized_identifier(void) {
    const char *source = "++(a)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_UNARY || expr->as.unary.op != TOKEN_PLUS_PLUS ||
        !expr->as.unary.operand || expr->as.unary.operand->kind != AST_EXPR_PAREN ||
        !expr->as.unary.operand->as.inner ||
        expr->as.unary.operand->as.inner->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.unary.operand->as.inner->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected prefix increment tree ++(a) for ++(a)\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_accepts_postfix_increment_parenthesized_identifier(void) {
    const char *source = "(a)++\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_POSTFIX || expr->as.postfix.op != TOKEN_PLUS_PLUS ||
        !expr->as.postfix.operand || expr->as.postfix.operand->kind != AST_EXPR_PAREN ||
        !expr->as.postfix.operand->as.inner ||
        expr->as.postfix.operand->as.inner->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.postfix.operand->as.inner->as.identifier.name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected postfix increment tree (a)++ for (a)++\\n\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_incomplete_postfix_followed_by_plus(void) {
    return expect_expression_assignment_parse_failure("a+++\n",
                                                     "incomplete postfix followed by plus input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_call_missing_closing_paren(void) {
    return expect_expression_assignment_parse_failure("f(a\n",
                                                     "call missing closing paren input",
                                                     "')'",
                                                     NULL);
}

static int test_expression_ast_rejects_call_missing_argument_after_comma(void) {
    return expect_expression_assignment_parse_failure("f(a,)\n",
                                                     "call missing argument after comma input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_non_callable_number_callee(void) {
    return expect_expression_assignment_parse_failure("1(2)\n",
                                                     "non-callable number callee input",
                                                     "callable",
                                                     NULL);
}

static int test_expression_ast_rejects_multiplicative_rhs_incomplete_unary(void) {
    return expect_expression_assignment_parse_failure("a*-\n",
                                                     "incomplete unary multiplicative RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_parenthesized_incomplete_additive_operand(void) {
    return expect_expression_assignment_parse_failure("-(a+)\n",
                                                     "incomplete parenthesized additive input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_parenthesized_lhs_assignment(void) {
    return expect_expression_assignment_parse_failure("(a)=b\n",
                                                     "parenthesized lhs assignment input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_rejects_number_lhs_assignment(void) {
    return expect_expression_assignment_parse_failure("1=b\n",
                                                     "number lhs assignment input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_rejects_binary_lhs_assignment(void) {
    return expect_expression_assignment_parse_failure("a+b=c\n",
                                                     "binary lhs assignment input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_rejects_parenthesized_assignment_lhs(void) {
    return expect_expression_assignment_parse_failure("(a=b)=c\n",
                                                     "parenthesized assignment lhs input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_rejects_double_parenthesized_identifier_lhs_assignment(void) {
    return expect_expression_assignment_parse_failure("((a))=b\n",
                                                     "double-parenthesized lhs assignment input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_rejects_assignment_to_parenthesized_equality(void) {
    return expect_expression_assignment_parse_failure("a=(b==c)=d\n",
                                                     "assignment-to-parenthesized-equality input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_relational_lower_precedence_than_additive(void) {
    const char *source = "a+b<c*d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_LT ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_PLUS ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_STAR) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a+b)<(c*d) tree for a+b<c*d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_relational_left_associative(void) {
    const char *source = "a<b<c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_LT ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_LT ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a<b)<c left-associative tree for a<b<c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_relational_with_parenthesized_right_additive(void) {
    const char *source = "a<=(b+c)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_LE ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_PAREN ||
        !expr->as.binary.right->as.inner || expr->as.binary.right->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.inner->as.binary.op != TOKEN_PLUS) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected a<=(b+c) relational tree with paren additive right side\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_relational_gt_operator(void) {
    const char *source = "a>b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_GT) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected binary '>' root for a>b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_relational_ge_operator(void) {
    const char *source = "a>=b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_GE) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected binary '>=' root for a>=b\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_equality_lower_precedence_than_relational(void) {
    const char *source = "a<b==c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_LT ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a<b)==c tree for a<b==c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_equality_left_associative(void) {
    const char *source = "a==b!=c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_NE ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a==b)!=c left-associative tree for a==b!=c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_logical_and_lower_precedence_than_equality(void) {
    const char *source = "a==b&&c!=d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_AND_AND ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_NE) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a==b)&&(c!=d) tree for a==b&&c!=d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_logical_or_lower_precedence_than_logical_and(void) {
    const char *source = "a&&b||c&&d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_OR_OR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_AND_AND ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_AND_AND) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a&&b)||(c&&d) tree for a&&b||c&&d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_logical_or_left_associative(void) {
    const char *source = "a||b||c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_OR_OR ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_OR_OR ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a||b)||c left-associative tree for a||b||c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_bitwise_and_lower_precedence_than_equality(void) {
    const char *source = "a==b&c==d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_AMP ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_EQ) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a==b)&(c==d) tree for a==b&c==d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_shift_lower_precedence_than_additive(void) {
    const char *source = "a+b<<c-d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_SHIFT_LEFT ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_PLUS ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_MINUS) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a+b)<<(c-d) tree for a+b<<c-d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_relational_lower_precedence_than_shift(void) {
    const char *source = "a<<b<c<<d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_LT ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_SHIFT_LEFT ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_SHIFT_LEFT) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a<<b)<(c<<d) tree for a<<b<c<<d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_shift_left_associative(void) {
    const char *source = "a<<b<<c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_SHIFT_LEFT ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_SHIFT_LEFT ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a<<b)<<c left-associative tree for a<<b<<c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_bitwise_xor_lower_precedence_than_bitwise_and(void) {
    const char *source = "a&b^c&d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_CARET ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_AMP ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_AMP) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a&b)^(c&d) tree for a&b^c&d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_bitwise_or_lower_precedence_than_bitwise_xor(void) {
    const char *source = "a^b|c^d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PIPE ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_CARET ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_CARET) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a^b)|(c^d) tree for a^b|c^d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_logical_and_lower_precedence_than_bitwise_or(void) {
    const char *source = "a|b&&c|d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_AND_AND ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_PIPE ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_PIPE) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a|b)&&(c|d) tree for a|b&&c|d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_conditional_lower_precedence_than_logical_or(void) {
    const char *source = "a||b?c:d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_TERNARY ||
        !expr->as.ternary.condition || expr->as.ternary.condition->kind != AST_EXPR_BINARY ||
        expr->as.ternary.condition->as.binary.op != TOKEN_OR_OR ||
        !expr->as.ternary.then_expr || expr->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.then_expr->as.identifier.name, "c") != 0 ||
        !expr->as.ternary.else_expr || expr->as.ternary.else_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.else_expr->as.identifier.name, "d") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a||b)?c:d tree for a||b?c:d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_conditional_right_associative(void) {
    const char *source = "a?b:c?d:e\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_TERNARY ||
        !expr->as.ternary.condition || expr->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.condition->as.identifier.name, "a") != 0 ||
        !expr->as.ternary.then_expr || expr->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.then_expr->as.identifier.name, "b") != 0 ||
        !expr->as.ternary.else_expr || expr->as.ternary.else_expr->kind != AST_EXPR_TERNARY ||
        !expr->as.ternary.else_expr->as.ternary.condition ||
        expr->as.ternary.else_expr->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.else_expr->as.ternary.condition->as.identifier.name, "c") != 0 ||
        !expr->as.ternary.else_expr->as.ternary.then_expr ||
        expr->as.ternary.else_expr->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.else_expr->as.ternary.then_expr->as.identifier.name, "d") != 0 ||
        !expr->as.ternary.else_expr->as.ternary.else_expr ||
        expr->as.ternary.else_expr->as.ternary.else_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.else_expr->as.ternary.else_expr->as.identifier.name, "e") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected right-associative tree a?b:(c?d:e) for a?b:c?d:e\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_parses_simple(void) {
    const char *source = "a=b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=b for assignment expression\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_is_right_associative(void) {
    const char *source = "a=b=c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.right->as.binary.left ||
        expr->as.binary.right->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.binary.right ||
        expr->as.binary.right->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected right-associative assignment tree a=(b=c)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_compound_assignment_parses_simple(void) {
    const char *source = "a+=b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected compound assignment tree a+=b for expression\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_compound_assignment_is_right_associative(void) {
    const char *source = "a+=b*=c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_STAR_ASSIGN ||
        !expr->as.binary.right->as.binary.left ||
        expr->as.binary.right->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.binary.right ||
        expr->as.binary.right->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected right-associative compound assignment tree a+=(b*=c)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_compound_assignment_lower_precedence_than_logical_or(void) {
    const char *source = "a+=b||c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_PLUS_ASSIGN ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_OR_OR) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected compound assignment tree a+=(b||c) for a+=b||c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_lower_precedence_than_equality(void) {
    const char *source = "a=b==c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.right->as.binary.left ||
        expr->as.binary.right->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.binary.right ||
        expr->as.binary.right->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(b==c) for a=b==c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_compound_assignment_rhs_incomplete(void) {
    return expect_expression_assignment_parse_failure("a+=\n",
                                                     "incomplete compound-assignment RHS input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_equality_with_parenthesized_assignment_rhs(void) {
    const char *source = "a==(b=c)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_EQ ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_PAREN ||
        !expr->as.binary.right->as.inner ||
        expr->as.binary.right->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.inner->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.right->as.inner->as.binary.left ||
        expr->as.binary.right->as.inner->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.inner->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.inner->as.binary.right ||
        expr->as.binary.right->as.inner->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.inner->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected equality tree with parenthesized assignment rhs for a==(b=c)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_assignment_after_equality_chain(void) {
    return expect_expression_assignment_parse_failure("a==b=c\n",
                                                     "assignment-after-equality input",
                                                     "EOF",
                                                     "ASSIGN");
}

static int test_expression_ast_assignment_lower_precedence_than_logical_or(void) {
    const char *source = "a=b||c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_OR_OR) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(b||c) for a=b||c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_assignment_lower_precedence_than_conditional(void) {
    const char *source = "a=b?c:d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.identifier.name, "a") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_TERNARY ||
        !expr->as.binary.right->as.ternary.condition ||
        expr->as.binary.right->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.ternary.condition->as.identifier.name, "b") != 0 ||
        !expr->as.binary.right->as.ternary.then_expr ||
        expr->as.binary.right->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.ternary.then_expr->as.identifier.name, "c") != 0 ||
        !expr->as.binary.right->as.ternary.else_expr ||
        expr->as.binary.right->as.ternary.else_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.ternary.else_expr->as.identifier.name, "d") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected assignment tree a=(b?c:d) for a=b?c:d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_conditional_then_branch_allows_comma_expression(void) {
    const char *source = "a?b,c:d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_TERNARY ||
        !expr->as.ternary.condition || expr->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.condition->as.identifier.name, "a") != 0 ||
        !expr->as.ternary.then_expr || expr->as.ternary.then_expr->kind != AST_EXPR_BINARY ||
        expr->as.ternary.then_expr->as.binary.op != TOKEN_COMMA ||
        !expr->as.ternary.then_expr->as.binary.left ||
        expr->as.ternary.then_expr->as.binary.left->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.then_expr->as.binary.left->as.identifier.name, "b") != 0 ||
        !expr->as.ternary.then_expr->as.binary.right ||
        expr->as.ternary.then_expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.then_expr->as.binary.right->as.identifier.name, "c") != 0 ||
        !expr->as.ternary.else_expr || expr->as.ternary.else_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.else_expr->as.identifier.name, "d") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected a?(b,c):d tree for a?b,c:d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_comma_still_lowest_around_conditional(void) {
    const char *source = "a?b:c,d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_COMMA ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_TERNARY ||
        !expr->as.binary.left->as.ternary.condition ||
        expr->as.binary.left->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.ternary.condition->as.identifier.name, "a") != 0 ||
        !expr->as.binary.left->as.ternary.then_expr ||
        expr->as.binary.left->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.ternary.then_expr->as.identifier.name, "b") != 0 ||
        !expr->as.binary.left->as.ternary.else_expr ||
        expr->as.binary.left->as.ternary.else_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.left->as.ternary.else_expr->as.identifier.name, "c") != 0 ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "d") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a?b:c),d tree for a?b:c,d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_conditional_else_parenthesized_comma(void) {
    const char *source = "a?b:(c,d)\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_TERNARY ||
        !expr->as.ternary.condition || expr->as.ternary.condition->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.condition->as.identifier.name, "a") != 0 ||
        !expr->as.ternary.then_expr || expr->as.ternary.then_expr->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.ternary.then_expr->as.identifier.name, "b") != 0 ||
        !expr->as.ternary.else_expr || expr->as.ternary.else_expr->kind != AST_EXPR_PAREN ||
        !expr->as.ternary.else_expr->as.inner ||
        expr->as.ternary.else_expr->as.inner->kind != AST_EXPR_BINARY ||
        expr->as.ternary.else_expr->as.inner->as.binary.op != TOKEN_COMMA) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected a?b:(c,d) tree for a?b:(c,d)\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_comma_lower_precedence_than_assignment(void) {
    const char *source = "a=b,c=d\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_COMMA ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_ASSIGN ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_BINARY ||
        expr->as.binary.right->as.binary.op != TOKEN_ASSIGN) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a=b),(c=d) tree for a=b,c=d\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_comma_left_associative(void) {
    const char *source = "a,b,c\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    if (!parse_expression_source_to_ast(source, &tokens, &expr, &err)) {
        return 0;
    }

    if (!expr || expr->kind != AST_EXPR_BINARY || expr->as.binary.op != TOKEN_COMMA ||
        !expr->as.binary.left || expr->as.binary.left->kind != AST_EXPR_BINARY ||
        expr->as.binary.left->as.binary.op != TOKEN_COMMA ||
        !expr->as.binary.right || expr->as.binary.right->kind != AST_EXPR_IDENTIFIER ||
        strcmp(expr->as.binary.right->as.identifier.name, "c") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected (a,b),c left-associative tree for a,b,c\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_incomplete_logical_and_rhs(void) {
    return expect_expression_assignment_parse_failure("a&&\n",
                                                     "incomplete logical-and rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_logical_or_rhs(void) {
    return expect_expression_assignment_parse_failure("a||\n",
                                                     "incomplete logical-or rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_bitwise_and_rhs(void) {
    return expect_expression_assignment_parse_failure("a&\n",
                                                     "incomplete bitwise-and rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_bitwise_xor_rhs(void) {
    return expect_expression_assignment_parse_failure("a^\n",
                                                     "incomplete bitwise-xor rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_bitwise_or_rhs(void) {
    return expect_expression_assignment_parse_failure("a|\n",
                                                     "incomplete bitwise-or rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_shift_left_rhs(void) {
    return expect_expression_assignment_parse_failure("a<<\n",
                                                     "incomplete shift-left rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_shift_right_rhs(void) {
    return expect_expression_assignment_parse_failure("a>>\n",
                                                     "incomplete shift-right rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_conditional_missing_colon(void) {
    return expect_expression_assignment_parse_failure("a?b\n",
                                                     "conditional missing-colon input",
                                                     "':'",
                                                     NULL);
}

static int test_expression_ast_rejects_conditional_incomplete_else_rhs(void) {
    return expect_expression_assignment_parse_failure("a?b:\n",
                                                     "conditional incomplete-else rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_incomplete_comma_rhs(void) {
    return expect_expression_assignment_parse_failure("a,\n",
                                                     "incomplete comma rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_conditional_then_comma_missing_middle_rhs(void) {
    return expect_expression_assignment_parse_failure("a?b,:d\n",
                                                     "conditional then-comma missing middle rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_conditional_missing_then_rhs(void) {
    return expect_expression_assignment_parse_failure("a?:c\n",
                                                     "conditional missing then rhs input",
                                                     "primary expression",
                                                     NULL);
}

static int test_expression_ast_rejects_conditional_else_trailing_comma(void) {
    return expect_expression_assignment_parse_failure("a?b:c,\n",
                                                     "conditional else trailing-comma input",
                                                     "primary expression",
                                                     NULL);
}

static int test_ast_collects_top_level_externals(void) {
    const char *source = "int x;\nint main(){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for AST top-level test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse unexpectedly failed at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 2 top-level externals, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].kind != AST_EXTERNAL_DECLARATION ||
        strcmp(program.externals[0].name, "x") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected first external to be declaration x, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[0].kind),
                program.externals[0].name ? program.externals[0].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[1].kind != AST_EXTERNAL_FUNCTION ||
        strcmp(program.externals[1].name, "main") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected second external to be function main, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[1].kind),
                program.externals[1].name ? program.externals[1].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_parameter_count(void) {
    const char *source = "int add(int a,int b){return a+b;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for function-parameter-count AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for function-parameter-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 1 top-level external, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].name || strcmp(program.externals[0].name, "add") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function add, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[0].kind),
                program.externals[0].name ? program.externals[0].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].parameter_count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add parameter_count=2, got %zu\n",
                program.externals[0].parameter_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].is_function_definition != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add to be a function definition (def=1), got def=%d\n",
                program.externals[0].is_function_definition);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].return_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add return_statement_count=1, got %zu\n",
                program.externals[0].return_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].returns_on_all_paths != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add returns_on_all_paths=1, got %d\n",
                program.externals[0].returns_on_all_paths);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].loop_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add loop_statement_count=0, got %zu\n",
                program.externals[0].loop_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected add if_statement_count=0, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected add break/continue/decl counts=0/0/0, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_parses_function_declaration_external(void) {
    const char *source = "int add(int a,int b);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for function-declaration AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for function declaration at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 1 function external for declaration, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].name || strcmp(program.externals[0].name, "add") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function declaration add, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[0].kind),
                program.externals[0].name ? program.externals[0].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].parameter_count != 2 ||
        program.externals[0].is_function_definition != 0 ||
        program.externals[0].return_statement_count != 0 ||
        program.externals[0].returns_on_all_paths != 0 ||
        program.externals[0].loop_statement_count != 0 ||
        program.externals[0].if_statement_count != 0 ||
        program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected declaration metadata params=2 def=0 returns=0 allpaths=0 loops=0 ifs=0 breaks=0 continues=0 decls=0, got params=%zu def=%d returns=%zu allpaths=%d loops=%zu ifs=%zu breaks=%zu continues=%zu decls=%zu\n",
                program.externals[0].parameter_count,
                program.externals[0].is_function_definition,
            program.externals[0].return_statement_count,
            program.externals[0].returns_on_all_paths,
            program.externals[0].loop_statement_count,
            program.externals[0].if_statement_count,
            program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_parses_unnamed_parameter_prototype(void) {
    /* Prototype accepts unnamed parameter: int f(int); */
    const char *source = "int f(int);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for unnamed-parameter prototype input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: unnamed-parameter prototype should parse, failed at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].name || strcmp(program.externals[0].name, "f") != 0 ||
        program.externals[0].parameter_count != 1 ||
        program.externals[0].is_function_definition != 0 ||
        program.externals[0].return_statement_count != 0 ||
        program.externals[0].returns_on_all_paths != 0 ||
        program.externals[0].loop_statement_count != 0 ||
        program.externals[0].if_statement_count != 0 ||
        program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: unnamed-parameter prototype metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_rejects_unnamed_parameter_definition(void) {
    /* Definition rejects unnamed parameter: int f(int){...} */
    const char *source = "int f(int){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for unnamed-parameter definition input\n");
        return 0;
    }

    if (parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: unnamed-parameter definition should fail but parsed successfully\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(err.message, "must have names") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected unnamed-definition diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_multiple_function_returns(void) {
    const char *source = "int f(int a){if(a){return a;}else{return 0;}}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for function-return-count AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for function-return-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected exactly one function external for return-count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].return_statement_count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function return_statement_count=2, got %zu\n",
                program.externals[0].return_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].returns_on_all_paths != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function returns_on_all_paths=1, got %d\n",
                program.externals[0].returns_on_all_paths);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].loop_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function loop_statement_count=0, got %zu\n",
                program.externals[0].loop_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function if_statement_count=1, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected break/continue/decl counts=0/0/0 for return-count test, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_partial_return_not_all_paths(void) {
    const char *source = "int f(int a){if(a){return 1;}a=a+1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for partial-return AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for partial-return input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected exactly one function external for partial-return test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].return_statement_count != 1 ||
        program.externals[0].returns_on_all_paths != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected partial-return metadata returns=1 allpaths=0, got returns=%zu allpaths=%d\n",
                program.externals[0].return_statement_count,
                program.externals[0].returns_on_all_paths);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].loop_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected partial-return loop_statement_count=0, got %zu\n",
                program.externals[0].loop_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected partial-return if_statement_count=1, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected partial-return break/continue/decl counts=0/0/0, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_accepts_break_continue_inside_loops(void) {
    const char *source =
        "int f(int a){while(1){if(a){break;}a=a-1;continue;}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for break/continue-in-loop input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser should accept break/continue inside loops, got %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        program.externals[0].returns_on_all_paths != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected function all-path return metadata after break/continue loop\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].loop_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected loop_statement_count=1 for break/continue loop case, got %zu\n",
                program.externals[0].loop_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected if_statement_count=1 for break/continue loop case, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 1 ||
        program.externals[0].continue_statement_count != 1 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected break/continue/decl counts=1/1/0 for break/continue loop case, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_rejects_break_outside_loop(void) {
    const char *source = "int f(){break;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for break-outside-loop input\n");
        return 0;
    }

    if (parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser accepted break outside loop\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(err.message, "inside loops") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected break-outside-loop diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_rejects_continue_outside_loop(void) {
    const char *source = "int f(){continue;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for continue-outside-loop input\n");
        return 0;
    }

    if (parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser accepted continue outside loop\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(err.message, "inside loops") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected continue-outside-loop diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_loop_statement_count(void) {
    const char *source = "int f(){while(1){break;}for(;;){break;}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for loop-count AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for loop-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected one function external for loop-count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].loop_statement_count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected loop_statement_count=2, got %zu\n",
                program.externals[0].loop_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected if_statement_count=0 for loop-count test, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 2 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected break/continue/decl counts=2/0/0 for loop-count test, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_if_statement_count(void) {
    const char *source =
        "int f(int a){if(a){if(a){return 1;}else{return 2;}}else{return 0;}}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for if-count AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for if-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected one function external for if-count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].if_statement_count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected if_statement_count=2, got %zu\n",
                program.externals[0].if_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 0 ||
        program.externals[0].continue_statement_count != 0 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected if-count test break/continue/decl counts=0/0/0, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_break_continue_statement_count(void) {
    const char *source =
        "int f(int a){for(;;){if(a){break;}continue;}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for break/continue-count AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for break/continue-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected one function external for break/continue-count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].break_statement_count != 1 ||
        program.externals[0].continue_statement_count != 1 ||
        program.externals[0].declaration_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected break/continue/decl counts=1/1/0, got %zu/%zu/%zu\n",
                program.externals[0].break_statement_count,
            program.externals[0].continue_statement_count,
            program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_declaration_statement_count(void) {
    const char *source =
        "int f(int a){int x=1;while(a){int y=2;break;}for(;;){int z=3;break;}return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for declaration-count AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for declaration-count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected one function external for declaration-count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].declaration_statement_count != 3) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected declaration_statement_count=3, got %zu\n",
                program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_does_not_count_for_init_declaration_statement(void) {
    const char *source =
        "int f(){for(int i=0;i<3;i=i+1){}int x=1;return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for for-init-declaration count input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for for-init-declaration count input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected one function external for for-init-declaration count test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].declaration_statement_count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected declaration_statement_count=1 (block decl only), got %zu\n",
                program.externals[0].declaration_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_declaration_initializer_metadata(void) {
    const char *source = "int a,b=1,c;\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for declaration-initializer AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for declaration-initializer input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 3) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 3 declaration externals, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].has_initializer != 0 ||
        program.externals[1].has_initializer != 1 ||
        program.externals[2].has_initializer != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected initializer flags [0,1,0], got [%d,%d,%d]\n",
                program.externals[0].has_initializer,
                program.externals[1].has_initializer,
                program.externals[2].has_initializer);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_collects_all_top_level_declarators(void) {
    const char *source = "int a,b;\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for multi-declarator AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST parse failed for multi-declarator input at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 2) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected 2 declarations for int a,b;, got %zu\n",
                program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].kind != AST_EXTERNAL_DECLARATION ||
        !program.externals[0].name || strcmp(program.externals[0].name, "a") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected first declarator name a, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[0].kind),
                program.externals[0].name ? program.externals[0].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[1].kind != AST_EXTERNAL_DECLARATION ||
        !program.externals[1].name || strcmp(program.externals[1].name, "b") != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected second declarator name b, got kind=%s name=%s\n",
                ast_external_kind_name(program.externals[1].kind),
                program.externals[1].name ? program.externals[1].name : "<null>");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_failure_clears_previous_program(void) {
    const char *valid_source = "int x;\n";
    TokenArray ok_tokens;
    Token bad_tokens[1];
    TokenArray bad_stream;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&ok_tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(valid_source, &ok_tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for AST stale-state setup input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&ok_tokens, &program, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: AST setup parse failed at %d:%d: %s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected setup AST count=1, got %zu\n",
                program.count);
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    bad_tokens[0].type = TOKEN_KW_INT;
    bad_tokens[0].lexeme = "int";
    bad_tokens[0].length = 3;
    bad_tokens[0].number_value = 0;
    bad_tokens[0].line = 1;
    bad_tokens[0].column = 1;

    bad_stream.data = bad_tokens;
    bad_stream.magic = 0;
    bad_stream.size = 1;
    bad_stream.capacity = 1;

    if (parser_parse_translation_unit_ast(&bad_stream, &program, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: AST parser accepted stream without EOF\n");
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 0) {
        fprintf(stderr,
                "[parser-reg] FAIL: failed parse should clear AST, got count=%zu\n",
                program.count);
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&ok_tokens);
    ast_program_free(&program);
    return 1;
}

static char *build_deep_assignment_source(size_t chain_len) {
    const char *prefix = "int main(){int a;a";
    const char *suffix = "=1;return a;}";
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t total_len = prefix_len + chain_len * 2 + suffix_len + 1;
    char *source = (char *)malloc(total_len);
    size_t pos = 0;
    size_t i;

    if (!source) {
        return NULL;
    }

    memcpy(source + pos, prefix, prefix_len);
    pos += prefix_len;

    for (i = 0; i < chain_len; ++i) {
        source[pos++] = '=';
        source[pos++] = 'a';
    }

    memcpy(source + pos, suffix, suffix_len);
    pos += suffix_len;
    source[pos] = '\0';
    return source;
}

static int test_deep_assignment_hits_recursion_limit(void) {
    TokenArray tokens;
    ParserError err;
    char *source = build_deep_assignment_source(3000);

    if (!source) {
        fprintf(stderr, "[parser-reg] FAIL: failed to allocate deep assignment source\n");
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for deep assignment input\n");
        free(source);
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted excessively deep assignment chain\n");
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "recursion limit") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected recursion-limit diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    lexer_free_tokens(&tokens);
    free(source);
    return 1;
}

static char *build_deep_block_source(size_t depth) {
    const char *prefix = "int main(){";
    const char *middle = "return 0;";
    const char *suffix = "}";
    size_t prefix_len = strlen(prefix);
    size_t middle_len = strlen(middle);
    size_t suffix_len = strlen(suffix);
    size_t total_len = prefix_len + depth + middle_len + depth + suffix_len + 1;
    char *source = (char *)malloc(total_len);
    size_t pos = 0;
    size_t i;

    if (!source) {
        return NULL;
    }

    memcpy(source + pos, prefix, prefix_len);
    pos += prefix_len;

    for (i = 0; i < depth; ++i) {
        source[pos++] = '{';
    }

    memcpy(source + pos, middle, middle_len);
    pos += middle_len;

    for (i = 0; i < depth; ++i) {
        source[pos++] = '}';
    }

    memcpy(source + pos, suffix, suffix_len);
    pos += suffix_len;
    source[pos] = '\0';
    return source;
}

static int test_deep_statement_nesting_hits_recursion_limit(void) {
    TokenArray tokens;
    ParserError err;
    char *source = build_deep_block_source(3000);

    if (!source) {
        fprintf(stderr, "[parser-reg] FAIL: failed to allocate deep block source\n");
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for deep block input\n");
        free(source);
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted excessively deep statement nesting\n");
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "recursion limit") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected recursion-limit diagnostic for statements, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "statement") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected statement recursion diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "unary-expression") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: statement nesting should not be reported as unary-expression: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    lexer_free_tokens(&tokens);
    free(source);
    return 1;
}

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
    if (!test_ast_records_function_loop_statement_count()) {
        return 1;
    }
    if (!test_ast_records_function_if_statement_count()) {
        return 1;
    }
    if (!test_ast_records_function_break_continue_statement_count()) {
        return 1;
    }
    if (!test_ast_records_function_declaration_statement_count()) {
        return 1;
    }
    if (!test_ast_does_not_count_for_init_declaration_statement()) {
        return 1;
    }
    if (!test_ast_records_declaration_initializer_metadata()) {
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
