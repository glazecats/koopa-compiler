#include "parser_regression_intellisense_prelude.inc"

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

