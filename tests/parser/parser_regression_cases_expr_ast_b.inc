#include "parser_regression_intellisense_prelude.inc"

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

