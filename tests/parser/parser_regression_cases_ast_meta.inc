#include "parser_regression_intellisense_prelude.inc"

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

    if (!program.externals[0].function_body ||
        program.externals[0].function_body->kind != AST_STMT_COMPOUND) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected add function_body compound statement AST\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].function_body->child_count != 1 ||
        !program.externals[0].function_body->children[0] ||
        program.externals[0].function_body->children[0]->kind != AST_STMT_RETURN ||
        program.externals[0].function_body->children[0]->expression_count != 1 ||
        !program.externals[0].function_body->children[0]->expressions[0]) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected add return statement with one expression subtree\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!program.externals[0].function_body->children[0]->has_primary_expression ||
        program.externals[0].function_body->children[0]->primary_expression_index != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected return statement primary expression slot metadata\n");
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

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_rejects_float_top_level_declaration_for_now(void) {
    const char *source = "float x;\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for float declaration AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: AST parse failed for float declaration input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1u ||
        program.externals[0].kind != AST_EXTERNAL_DECLARATION ||
        program.externals[0].declaration_value_kind != AST_DECLARATION_VALUE_FLOAT) {
        fprintf(stderr, "[parser-reg] FAIL: float declaration AST metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_float_function_signature_metadata(void) {
    const char *source = "float mix(float a, int b);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for float function signature AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for float function signature input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].name ||
        strcmp(program.externals[0].name, "mix") != 0 ||
        program.externals[0].function_return_type != AST_FUNCTION_RETURN_FLOAT ||
        !program.externals[0].parameter_value_kinds ||
        program.externals[0].parameter_value_kinds[0] != AST_PARAMETER_VALUE_FLOAT ||
        program.externals[0].parameter_value_kinds[1] != AST_PARAMETER_VALUE_INT) {
        fprintf(stderr, "[parser-reg] FAIL: float function signature metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_function_valued_parameter_metadata(void) {
    const char *source = "int apply(int f(int), int x);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for function-valued parameter AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for function-valued parameter input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].name ||
        strcmp(program.externals[0].name, "apply") != 0 ||
        program.externals[0].parameter_count != 2 ||
        !program.externals[0].parameter_value_kinds ||
        program.externals[0].parameter_value_kinds[0] != AST_PARAMETER_VALUE_FUNCTION ||
        program.externals[0].parameter_value_kinds[1] != AST_PARAMETER_VALUE_INT ||
        !program.externals[0].parameter_function_parameter_counts ||
        program.externals[0].parameter_function_parameter_counts[0] != 1u) {
        fprintf(stderr, "[parser-reg] FAIL: function-valued parameter metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_void_function_valued_parameter_metadata(void) {
    const char *source = "int apply(void f(int), int x);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for void function-valued parameter AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for void function-valued parameter input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].parameter_value_kinds ||
        program.externals[0].parameter_count != 2 ||
        program.externals[0].parameter_value_kinds[0] != AST_PARAMETER_VALUE_FUNCTION ||
        !program.externals[0].parameter_function_return_types ||
        program.externals[0].parameter_function_return_types[0] != AST_FUNCTION_RETURN_VOID ||
        !program.externals[0].parameter_function_parameter_counts ||
        program.externals[0].parameter_function_parameter_counts[0] != 1u) {
        fprintf(stderr, "[parser-reg] FAIL: void function-valued parameter metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_zero_arg_function_valued_parameter_metadata(void) {
    const char *source = "int apply0(int f());\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for zero-arg function-valued parameter AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for zero-arg function-valued parameter input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].parameter_value_kinds ||
        program.externals[0].parameter_count != 1 ||
        program.externals[0].parameter_value_kinds[0] != AST_PARAMETER_VALUE_FUNCTION ||
        !program.externals[0].parameter_function_parameter_counts ||
        program.externals[0].parameter_function_parameter_counts[0] != 0u ||
        !program.externals[0].parameter_function_return_types ||
        program.externals[0].parameter_function_return_types[0] != AST_FUNCTION_RETURN_INT) {
        fprintf(stderr, "[parser-reg] FAIL: zero-arg function-valued parameter metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_zero_arg_void_function_valued_parameter_metadata(void) {
    const char *source = "void apply0(void f());\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for zero-arg void function-valued parameter AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for zero-arg void function-valued parameter input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 ||
        program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        program.externals[0].function_return_type != AST_FUNCTION_RETURN_VOID ||
        !program.externals[0].parameter_value_kinds ||
        program.externals[0].parameter_count != 1 ||
        program.externals[0].parameter_value_kinds[0] != AST_PARAMETER_VALUE_FUNCTION ||
        !program.externals[0].parameter_function_parameter_counts ||
        program.externals[0].parameter_function_parameter_counts[0] != 0u ||
        !program.externals[0].parameter_function_return_types ||
        program.externals[0].parameter_function_return_types[0] != AST_FUNCTION_RETURN_VOID) {
        fprintf(stderr, "[parser-reg] FAIL: zero-arg void function-valued parameter metadata mismatch\n");
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
        program.externals[0].function_body != NULL ||
        program.externals[0].return_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected declaration metadata params=2 def=0 body=NULL returns=0, got params=%zu def=%d body=%p returns=%zu\n",
            program.externals[0].parameter_count,
            program.externals[0].is_function_definition,
            (void *)program.externals[0].function_body,
            program.externals[0].return_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_statement_expression_slots_for_control_flow(void) {
    const char *source =
        "int f(int n){while(n){n=n-1;}for(n=0;n;n=n-1){n=n-1;}if(n){return n;}defer return n;return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *while_stmt;
    AstStatement *for_stmt;
    AstStatement *if_stmt;
    AstStatement *defer_stmt;
    AstStatement *ret_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for statement-slot AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for statement-slot input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || !program.externals[0].function_body) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected one function with body for statement-slot test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.externals[0].function_body;
    if (body->kind != AST_STMT_COMPOUND || body->child_count != 5) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected compound body with 5 statements, got kind=%d count=%zu\n",
            (int)body->kind,
            body->child_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    while_stmt = body->children[0];
    for_stmt = body->children[1];
    if_stmt = body->children[2];
    defer_stmt = body->children[3];
    ret_stmt = body->children[4];

    if (!while_stmt || while_stmt->kind != AST_STMT_WHILE || !while_stmt->has_condition_expression ||
        while_stmt->condition_expression_index != 0 || while_stmt->expression_count != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: while statement condition slot metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!for_stmt || for_stmt->kind != AST_STMT_FOR || !for_stmt->has_for_init_expression ||
        !for_stmt->has_condition_expression || !for_stmt->has_for_step_expression ||
        for_stmt->for_init_expression_index != 0 || for_stmt->condition_expression_index != 1 ||
        for_stmt->for_step_expression_index != 2 || for_stmt->expression_count != 3) {
        fprintf(stderr,
            "[parser-reg] FAIL: for statement slot metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!if_stmt || if_stmt->kind != AST_STMT_IF || !if_stmt->has_condition_expression ||
        if_stmt->condition_expression_index != 0 || if_stmt->expression_count != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: if statement condition slot metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!defer_stmt || defer_stmt->kind != AST_STMT_DEFER ||
        defer_stmt->child_count != 1 || !defer_stmt->children[0] ||
        defer_stmt->children[0]->kind != AST_STMT_RETURN ||
        !defer_stmt->children[0]->has_primary_expression ||
        defer_stmt->children[0]->primary_expression_index != 0 ||
        defer_stmt->children[0]->expression_count != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: defer statement payload metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ret_stmt || ret_stmt->kind != AST_STMT_RETURN || !ret_stmt->has_primary_expression ||
        ret_stmt->primary_expression_index != 0 || ret_stmt->expression_count != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: return statement primary slot metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_unless_desugars_to_if_with_negated_condition(void) {
    const char *source = "int main(){ unless(x) return 1; return 0; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *unless_stmt;
    AstExpression *cond_expr;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for unless AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for unless input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.count == 1 ? program.externals[0].function_body : NULL;
    unless_stmt = body && body->child_count >= 1 ? body->children[0] : NULL;
    cond_expr = unless_stmt && unless_stmt->has_condition_expression &&
            unless_stmt->condition_expression_index < unless_stmt->expression_count
        ? unless_stmt->expressions[unless_stmt->condition_expression_index]
        : NULL;

    if (!body || body->kind != AST_STMT_COMPOUND || body->child_count != 2 ||
        !unless_stmt || unless_stmt->kind != AST_STMT_IF ||
        !cond_expr || cond_expr->kind != AST_EXPR_UNARY ||
        cond_expr->as.unary.op != TOKEN_BANG ||
        !cond_expr->as.unary.operand ||
        cond_expr->as.unary.operand->kind != AST_EXPR_IDENTIFIER ||
        strcmp(cond_expr->as.unary.operand->as.identifier.name, "x") != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: unless should desugar to if(!x) in AST\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_fndefer_records_payload_metadata(void) {
    const char *source = "int main(){ fndefer return 1; return 0; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *fndefer_stmt;
    AstStatement *ret_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for fndefer AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for fndefer input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.count == 1 ? program.externals[0].function_body : NULL;
    fndefer_stmt = body && body->child_count >= 1 ? body->children[0] : NULL;
    ret_stmt = body && body->child_count >= 2 ? body->children[1] : NULL;

    if (!fndefer_stmt || fndefer_stmt->kind != AST_STMT_FNDEFER ||
        fndefer_stmt->extension_origin != AST_EXTENSION_ORIGIN_FNDEFER ||
        fndefer_stmt->child_count != 1 || !fndefer_stmt->children || !fndefer_stmt->children[0] ||
        fndefer_stmt->children[0]->kind != AST_STMT_RETURN ||
        !fndefer_stmt->children[0]->has_primary_expression ||
        fndefer_stmt->children[0]->primary_expression_index != 0 ||
        fndefer_stmt->children[0]->expression_count != 1) {
        fprintf(stderr, "[parser-reg] FAIL: fndefer statement payload metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ret_stmt || ret_stmt->kind != AST_STMT_RETURN || !ret_stmt->has_primary_expression ||
        ret_stmt->primary_expression_index != 0 || ret_stmt->expression_count != 1) {
        fprintf(stderr, "[parser-reg] FAIL: trailing return metadata mismatch for fndefer AST test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_capdefer_records_capture_metadata(void) {
    const char *source = "int main(){ capdefer(x=1, y=2) return x+y; return 0; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *capdefer_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for capdefer AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for capdefer input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.count == 1 ? program.externals[0].function_body : NULL;
    capdefer_stmt = body && body->child_count >= 1 ? body->children[0] : NULL;

    if (!capdefer_stmt || capdefer_stmt->kind != AST_STMT_CAPDEFER ||
        capdefer_stmt->extension_origin != AST_EXTENSION_ORIGIN_CAPDEFER ||
        capdefer_stmt->capture_name_count != 2u ||
        capdefer_stmt->capture_expression_count != 2u ||
        !capdefer_stmt->capture_names ||
        !capdefer_stmt->capture_expressions ||
        strcmp(capdefer_stmt->capture_names[0], "x") != 0 ||
        strcmp(capdefer_stmt->capture_names[1], "y") != 0 ||
        !capdefer_stmt->children || capdefer_stmt->child_count != 1u ||
        !capdefer_stmt->children[0] ||
        capdefer_stmt->children[0]->kind != AST_STMT_RETURN) {
        fprintf(stderr, "[parser-reg] FAIL: capdefer statement capture metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_pair_declaration_and_member_metadata(void) {
    const char *source = "int main(){ pair a={1,2}; pair b=a; return b.first; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *pair_decl_a;
    AstStatement *pair_decl_b;
    AstStatement *ret_stmt;
    AstExpression *ret_expr;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for pair AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for pair input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.count == 1 ? program.externals[0].function_body : NULL;
    pair_decl_a = body && body->child_count >= 1 ? body->children[0] : NULL;
    pair_decl_b = body && body->child_count >= 2 ? body->children[1] : NULL;
    ret_stmt = body && body->child_count >= 3 ? body->children[2] : NULL;
    ret_expr = ret_stmt && ret_stmt->has_primary_expression &&
            ret_stmt->primary_expression_index < ret_stmt->expression_count
        ? ret_stmt->expressions[ret_stmt->primary_expression_index]
        : NULL;

    if (!pair_decl_a || pair_decl_a->kind != AST_STMT_DECLARATION ||
        pair_decl_a->extension_origin != AST_EXTENSION_ORIGIN_PAIR ||
        !pair_decl_a->declaration_value_kinds ||
        pair_decl_a->declaration_value_kinds[0] != AST_DECLARATION_VALUE_PAIR) {
        fprintf(stderr, "[parser-reg] FAIL: first pair declaration metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!pair_decl_b || pair_decl_b->kind != AST_STMT_DECLARATION ||
        !pair_decl_b->declaration_value_kinds ||
        pair_decl_b->declaration_value_kinds[0] != AST_DECLARATION_VALUE_PAIR ||
        !pair_decl_b->expressions || !pair_decl_b->expressions[0] ||
        pair_decl_b->expressions[0]->kind != AST_EXPR_IDENTIFIER ||
        strcmp(pair_decl_b->expressions[0]->as.identifier.name, "a") != 0) {
        fprintf(stderr, "[parser-reg] FAIL: pair copy declaration metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ret_expr || ret_expr->kind != AST_EXPR_MEMBER ||
        !ret_expr->as.member.field_name ||
        strcmp(ret_expr->as.member.field_name, "first") != 0) {
        fprintf(stderr, "[parser-reg] FAIL: pair member return expression metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_struct_definition_and_local_metadata(void) {
    const char *source =
        "struct Point { int x; int y; int z; };\n"
        "int main(){ struct Point p={1,2,3}; struct Point q=p; return q.z; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *decl_p;
    AstStatement *decl_q;
    AstStatement *ret_stmt;
    AstExpression *ret_expr;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for struct AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for struct input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.struct_type_count != 1u ||
        !program.struct_types ||
        !program.struct_types[0].name ||
        strcmp(program.struct_types[0].name, "Point") != 0 ||
        program.struct_types[0].field_count != 3u ||
        !program.struct_types[0].field_names ||
        strcmp(program.struct_types[0].field_names[0], "x") != 0 ||
        strcmp(program.struct_types[0].field_names[1], "y") != 0 ||
        strcmp(program.struct_types[0].field_names[2], "z") != 0) {
        fprintf(stderr, "[parser-reg] FAIL: struct type metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.count == 1 ? program.externals[0].function_body : NULL;
    decl_p = body && body->child_count >= 1 ? body->children[0] : NULL;
    decl_q = body && body->child_count >= 2 ? body->children[1] : NULL;
    ret_stmt = body && body->child_count >= 3 ? body->children[2] : NULL;
    ret_expr = ret_stmt && ret_stmt->has_primary_expression &&
            ret_stmt->primary_expression_index < ret_stmt->expression_count
        ? ret_stmt->expressions[ret_stmt->primary_expression_index]
        : NULL;

    if (!decl_p || !decl_q ||
        !decl_p->declaration_value_kinds || decl_p->declaration_value_kinds[0] != AST_DECLARATION_VALUE_STRUCT ||
        !decl_q->declaration_value_kinds || decl_q->declaration_value_kinds[0] != AST_DECLARATION_VALUE_STRUCT ||
        !decl_p->declaration_type_names || !decl_p->declaration_type_names[0] ||
        !decl_q->declaration_type_names || !decl_q->declaration_type_names[0] ||
        strcmp(decl_p->declaration_type_names[0], "Point") != 0 ||
        strcmp(decl_q->declaration_type_names[0], "Point") != 0) {
        fprintf(stderr, "[parser-reg] FAIL: struct declaration metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!ret_expr || ret_expr->kind != AST_EXPR_MEMBER ||
        !ret_expr->as.member.field_name ||
        strcmp(ret_expr->as.member.field_name, "z") != 0) {
        fprintf(stderr, "[parser-reg] FAIL: struct member return expression metadata mismatch\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_records_scope_declaration_and_parameter_names(void) {
    const char *source =
        "int f(int a,int b){int x=0,y=1;for(int i=0;i<2;i=i+1){x=x+i;}return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *decl_stmt;
    AstStatement *for_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for scope-name AST test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for scope-name input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].is_function_definition || !program.externals[0].function_body) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected one function definition with body for scope-name test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].parameter_count != 2 || !program.externals[0].parameter_names ||
        !program.externals[0].parameter_names[0] || !program.externals[0].parameter_names[1] ||
        strcmp(program.externals[0].parameter_names[0], "a") != 0 ||
        strcmp(program.externals[0].parameter_names[1], "b") != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected function parameter names [a,b] to be recorded\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.externals[0].function_body;
    if (body->kind != AST_STMT_COMPOUND || body->child_count != 3) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected compound body with 3 statements in scope-name test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    decl_stmt = body->children[0];
    for_stmt = body->children[1];

    if (!decl_stmt || decl_stmt->kind != AST_STMT_DECLARATION ||
        decl_stmt->declaration_name_count != 2 || !decl_stmt->declaration_names ||
        !decl_stmt->declaration_names[0] || !decl_stmt->declaration_names[1] ||
        strcmp(decl_stmt->declaration_names[0], "x") != 0 ||
        strcmp(decl_stmt->declaration_names[1], "y") != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected declaration statement names [x,y] to be recorded\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (decl_stmt->expression_count != 2 || !decl_stmt->expressions ||
        !decl_stmt->expressions[0] || !decl_stmt->expressions[1] ||
        decl_stmt->expressions[0]->kind != AST_EXPR_NUMBER ||
        decl_stmt->expressions[1]->kind != AST_EXPR_NUMBER ||
        decl_stmt->expressions[0]->as.number_value != 0 ||
        decl_stmt->expressions[1]->as.number_value != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected declaration initializer expressions [0,1] in statement AST\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!for_stmt || for_stmt->kind != AST_STMT_FOR || for_stmt->declaration_name_count != 1 ||
        !for_stmt->declaration_names || !for_stmt->declaration_names[0] ||
        strcmp(for_stmt->declaration_names[0], "i") != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected for-init declaration name [i] to be recorded\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (!for_stmt->has_for_init_expression || !for_stmt->has_condition_expression ||
        !for_stmt->has_for_step_expression || for_stmt->for_init_expression_index != 0 ||
        for_stmt->condition_expression_index != 1 || for_stmt->for_step_expression_index != 2 ||
        for_stmt->expression_count != 3) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected for-init declaration expression slots to be recorded\n");
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
        program.externals[0].return_statement_count != 0) {
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

    if (program.externals[0].return_statement_count != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected partial-return metadata returns=1, got returns=%zu\n",
            program.externals[0].return_statement_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_flow_is_not_semantic_authority(void) {
    const char *source =
        "int f(){if((1)){return 1;}}\n"
        "int g(){while((1)){} }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for parser-flow-authority AST input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: parser should not enforce semantic all-path return, got %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 2 || program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        program.externals[1].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected two function externals in parser-flow-authority test, got %zu\n",
            program.count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.externals[0].return_statement_count != 1 ||
        program.externals[1].return_statement_count != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected return counts [1,0] in parser-flow-authority test, got [%zu,%zu]\n",
            program.externals[0].return_statement_count,
            program.externals[1].return_statement_count);
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
    program.externals[0].kind != AST_EXTERNAL_FUNCTION) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected function external after break/continue loop\n");
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

    if (program.externals[0].declaration_initializer != NULL ||
        program.externals[1].declaration_initializer == NULL ||
        program.externals[2].declaration_initializer != NULL ||
        program.externals[1].declaration_initializer->kind != AST_EXPR_NUMBER ||
        program.externals[1].declaration_initializer->as.number_value != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected declaration initializer AST slots [NULL,number(1),NULL]\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_ast_aligns_initializer_slots_with_declarator_order(void) {
    const char *source =
        "int f(){int x=y,y=1,z;for(int i=j,j=0,k;i<1;i=i+1){return i;}return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *decl_stmt;
    AstStatement *for_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr,
            "[parser-reg] FAIL: lexer failed for initializer-slot alignment test input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for initializer-slot alignment input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].is_function_definition || !program.externals[0].function_body) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected one function definition for initializer-slot alignment test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.externals[0].function_body;
    if (body->kind != AST_STMT_COMPOUND || body->child_count != 3) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected compound body with 3 statements for initializer-slot alignment test\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    decl_stmt = body->children[0];
    if (!decl_stmt || decl_stmt->kind != AST_STMT_DECLARATION ||
        decl_stmt->declaration_name_count != 3 || decl_stmt->expression_count != 3 ||
        !decl_stmt->expressions || decl_stmt->expressions[0] == NULL ||
        decl_stmt->expressions[1] == NULL || decl_stmt->expressions[2] != NULL ||
        decl_stmt->expressions[0]->kind != AST_EXPR_IDENTIFIER ||
        !decl_stmt->expressions[0]->as.identifier.name ||
        strcmp(decl_stmt->expressions[0]->as.identifier.name, "y") != 0 ||
        decl_stmt->expressions[1]->kind != AST_EXPR_NUMBER ||
        decl_stmt->expressions[1]->as.number_value != 1) {
        fprintf(stderr,
            "[parser-reg] FAIL: declaration initializer slots did not align with declarator order [x,y,z]\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    for_stmt = body->children[1];
    if (!for_stmt || for_stmt->kind != AST_STMT_FOR || for_stmt->declaration_name_count != 3 ||
        for_stmt->expression_count != 5 || !for_stmt->expressions ||
        !for_stmt->has_for_init_expression || !for_stmt->has_condition_expression ||
        !for_stmt->has_for_step_expression || for_stmt->for_init_expression_index != 0 ||
        for_stmt->condition_expression_index != 3 || for_stmt->for_step_expression_index != 4 ||
        for_stmt->expressions[0] == NULL || for_stmt->expressions[1] == NULL ||
        for_stmt->expressions[2] != NULL ||
        for_stmt->expressions[0]->kind != AST_EXPR_IDENTIFIER ||
        !for_stmt->expressions[0]->as.identifier.name ||
        strcmp(for_stmt->expressions[0]->as.identifier.name, "j") != 0 ||
        for_stmt->expressions[1]->kind != AST_EXPR_NUMBER ||
        for_stmt->expressions[1]->as.number_value != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: for-init declaration initializer slots did not align with declarator order [i,j,k]\n");
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

static int test_ast_records_array_rank_metadata(void) {
    const char *source =
        "int g[3][4];\n"
        "int f(){int x[2], y[3][4]; return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError err;
    AstStatement *body;
    AstStatement *decl_stmt;

    lexer_init_tokens(&tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for array-rank metadata input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: AST parse failed for array-rank metadata input at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 2 ||
        program.externals[0].declaration_array_rank != 2 ||
        !program.externals[0].declaration_array_extent_exprs ||
        !program.externals[0].declaration_array_extent_exprs[0] ||
        !program.externals[0].declaration_array_extent_exprs[1] ||
        program.externals[0].declaration_array_extent_exprs[0]->kind != AST_EXPR_NUMBER ||
        program.externals[0].declaration_array_extent_exprs[0]->as.number_value != 3 ||
        program.externals[0].declaration_array_extent_exprs[1]->kind != AST_EXPR_NUMBER ||
        program.externals[0].declaration_array_extent_exprs[1]->as.number_value != 4) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected global array extents [3,4]\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    body = program.externals[1].function_body;
    decl_stmt = (body && body->kind == AST_STMT_COMPOUND && body->child_count > 0)
        ? body->children[0]
        : NULL;
    if (!decl_stmt || decl_stmt->kind != AST_STMT_DECLARATION ||
        decl_stmt->declaration_name_count != 2 ||
        !decl_stmt->declaration_array_ranks ||
        !decl_stmt->declaration_array_extent_exprs ||
        decl_stmt->declaration_array_ranks[0] != 1 ||
        decl_stmt->declaration_array_ranks[1] != 2 ||
        !decl_stmt->declaration_array_extent_exprs[0] ||
        !decl_stmt->declaration_array_extent_exprs[1] ||
        !decl_stmt->declaration_array_extent_exprs[0][0] ||
        decl_stmt->declaration_array_extent_exprs[0][0]->kind != AST_EXPR_NUMBER ||
        decl_stmt->declaration_array_extent_exprs[0][0]->as.number_value != 2 ||
        !decl_stmt->declaration_array_extent_exprs[1][0] ||
        !decl_stmt->declaration_array_extent_exprs[1][1] ||
        decl_stmt->declaration_array_extent_exprs[1][0]->kind != AST_EXPR_NUMBER ||
        decl_stmt->declaration_array_extent_exprs[1][0]->as.number_value != 3 ||
        decl_stmt->declaration_array_extent_exprs[1][1]->kind != AST_EXPR_NUMBER ||
        decl_stmt->declaration_array_extent_exprs[1][1]->as.number_value != 4) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected local declaration array extents [2] and [3,4]\n");
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

static int test_ast_failure_clears_complex_previous_program(void) {
    const char *valid_source =
        "int f(int a,int b){"
        "int x=0;"
        "x=(a?b:a)+(~a);"
        "x=f(a,b);"
        "x=(x++,--b);"
        "if(a){x=x+1;}else{x=x-1;}"
        "while(a){break;}"
        "for(x=0;a;x=x+1){continue;}"
        "return x;"
        "}\n";
    TokenArray ok_tokens;
    Token bad_tokens[1];
    TokenArray bad_stream;
    AstProgram program;
    ParserError err;

    lexer_init_tokens(&ok_tokens);
    ast_program_init(&program);

    if (!lexer_tokenize(valid_source, &ok_tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for complex stale-state setup input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(&ok_tokens, &program, &err)) {
        fprintf(stderr,
            "[parser-reg] FAIL: complex setup parse failed at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 1 || !program.externals[0].function_body) {
        fprintf(stderr,
            "[parser-reg] FAIL: expected complex setup AST with one function body, got count=%zu body=%p\n",
            program.count,
            (void *)(program.count > 0 ? program.externals[0].function_body : NULL));
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
        fprintf(stderr, "[parser-reg] FAIL: AST parser accepted complex-clear stream without EOF\n");
        lexer_free_tokens(&ok_tokens);
        ast_program_free(&program);
        return 0;
    }

    if (program.count != 0 || program.externals != NULL || program.capacity != 0) {
        fprintf(stderr,
            "[parser-reg] FAIL: failed parse should fully clear complex AST, got count=%zu externals=%p capacity=%zu\n",
            program.count,
            (void *)program.externals,
            program.capacity);
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
