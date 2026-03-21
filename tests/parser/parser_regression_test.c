#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    if (!parser_parse_expression_ast_additive(tokens, out_expression, err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: expression AST parse failed at %d:%d: %s\n",
                err->line,
                err->column,
                err->message);
        lexer_free_tokens(tokens);
        return 0;
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
    const char *source = "a b\n";
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for trailing-token expression input\n");
        return 0;
    }

    if (parser_parse_expression_ast_additive(&tokens, &expr, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: expression parser unexpectedly accepted trailing tokens\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    if (strstr(err.message, "EOF") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected EOF diagnostic for trailing tokens, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    return 1;
}

static int test_expression_ast_rejects_deep_parentheses_with_recursion_limit(void) {
    size_t depth = 3000;
    size_t len = depth * 2 + 2;
    char *source = (char *)malloc(len + 1);
    TokenArray tokens;
    AstExpression *expr = NULL;
    ParserError err;
    size_t i;

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

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
                "[parser-reg] FAIL: lexer failed for deep-parentheses expression input\n");
        free(source);
        return 0;
    }

    if (parser_parse_expression_ast_additive(&tokens, &expr, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: deep-parentheses expression should fail recursion limit\n");
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        free(source);
        return 0;
    }

    if (strstr(err.message, "recursion limit") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected recursion-limit diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        ast_expression_free(expr);
        free(source);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_expression_free(expr);
    free(source);
    return 1;
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
