#include "semantic_regression_intellisense_prelude.inc"

static int test_semantic_accepts_call_to_declared_function(void) {
    const char *source = "int foo(int a);\nint main(){return foo(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call to declared function should pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_call_before_visible_declaration(void) {
    const char *source = "int main(){return foo(1);}\nint foo(int a){return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call before visible declaration should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-002 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "decl_line=2; decl_col=5") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected declaration location detail, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_top_level_initializer_semantic_matrix(void) {
    static const CallableDiagCase reject_cases[] = {
        {"TOPINIT-SCOPE-001",
        "int x = y; int y; int main(){return 0;}\n",
        "SEMA-SCOPE-002",
        "identifier=y",
        0,
        0},
        {"TOPINIT-CALL-001",
        "int foo; int x = foo(); int main(){return 0;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"TOPINIT-CALL-002",
        "int x = foo(); int foo(){return 1;} int main(){return 0;}\n",
        "SEMA-CALL-002",
        "callee=foo; decl_line=1",
        0,
        0},
        {"TOPINIT-SCOPE-002",
        "int x = y, y = 1; int main(){return 0;}\n",
        "SEMA-SCOPE-002",
        "identifier=y",
        0,
        0},
        {"TOPINIT-SCOPE-003",
        "int x = x; int main(){return x;}\n",
        "SEMA-SCOPE-002",
        "identifier=x",
        0,
        0},
        {"TOPINIT-CALL-003",
        "int x = x(); int main(){return 0;}\n",
        "SEMA-CALL-001",
        "callee=x",
        0,
        0},
        {"TOPINIT-FLOAT-TYPE-001",
        "float g = 1.25;\nint h = g;\nint main(){return 0;}\n",
        "SEMA-TYPE-004",
        "initializer scalar type mismatch",
        0,
        0},
        {"TOPINIT-FLOAT-TYPE-002",
        "float g = 1.25;\nfloat id(float x){return x;}\nint h = id(g);\nint main(){return 0;}\n",
        "SEMA-TYPE-004",
        "initializer scalar type mismatch",
        0,
        0},
    };
    static const CallablePassCase pass_cases[] = {
        {"TOPINIT-PASS-001", "int y = 1; int x = y; int main(){return x;}\n"},
        {"TOPINIT-PASS-002",
        "int foo(int a); int x = foo(1); int foo(int a){return a;} int main(){return x;}\n"},
        {"TOPINIT-FLOAT-PASS-001",
        "float g = 1.25;\nfloat h = g;\nint main(){return 0;}\n"},
        {"TOPINIT-FLOAT-PASS-002",
        "float g = 1.25;\nfloat id(float x){return x;}\nfloat h = id(g);\nint main(){return 0;}\n"},
    };
    size_t i;

    for (i = 0; i < sizeof(reject_cases) / sizeof(reject_cases[0]); ++i) {
        if (!run_callable_diag_case(&reject_cases[i])) {
            return 0;
        }
    }

    for (i = 0; i < sizeof(pass_cases) / sizeof(pass_cases[0]); ++i) {
        if (!run_callable_pass_case(&pass_cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_semantic_rejects_duplicate_local_declaration_same_scope(void) {
    const char *source = "int f(){int x; int x; return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: duplicate local declaration in same scope should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-001 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_undeclared_local_identifier_use(void) {
    const char *source = "int f(){int x; y=x; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: undeclared local identifier use should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_call_with_undeclared_argument_identifier(void) {
    const char *source = "int foo(int a){return a;}\nint main(){return foo(bar);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call with undeclared argument identifier should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for undeclared call argument, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "identifier=bar") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected undeclared call argument identifier detail, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_undeclared_call_argument_across_statement_slots(void) {
    static const char *cases[] = {
        "int foo(int a){return a;}\nint main(){int x=foo(bar); return x;}\n",
        "int foo(int a){return a;}\nint main(){for(;foo(bar);){return 0;} return 0;}\n",
        "int foo(int a){return a;}\nint main(){int i=0; for(;i<1;foo(bar)){i=i+1;} return 0;}\n",
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        TokenArray tokens;
        AstProgram program;
        ParserError parse_err;
        SemanticError sema_err;

        if (!parse_source_to_ast(cases[i], &tokens, &program, &parse_err)) {
            return 0;
        }

        if (semantic_analyze_program(&program, &sema_err)) {
            fprintf(stderr,
                "[semantic-reg] FAIL: undeclared call argument across statement slots should fail (case %zu)\n",
                i + 1);
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
            fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for undeclared call argument slot case %zu, got: %s\n",
                i + 1,
                sema_err.message);
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        if (strstr(sema_err.message, "identifier=bar") == NULL) {
            fprintf(stderr,
                "[semantic-reg] FAIL: expected identifier=bar detail for undeclared call argument slot case %zu, got: %s\n",
                i + 1,
                sema_err.message);
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        lexer_free_tokens(&tokens);
        ast_program_free(&program);
    }

    return 1;
}

static int test_semantic_accepts_subscript_identifier_visibility(void) {
    const char *source =
        "int main(){int a; int i; int x; x = a[i]; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: subscript identifier visibility should pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_undeclared_identifier_in_subscript_index(void) {
    const char *source =
        "int main(){int a; int x; x = a[i]; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: undeclared subscript index identifier should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL ||
        strstr(sema_err.message, "identifier=i") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected undeclared subscript-index diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_block_shadowing(void) {
    const char *source = "int f(){int x; {int x; x=1;} x=2; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: block shadowing should be accepted, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_for_init_declaration_visibility(void) {
    const char *source = "int f(){for(int i=0;i<3;i=i+1){int x=i;} return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-init declaration visibility should be accepted, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_parameter_redeclaration_in_function_body(void) {
    const char *source = "int f(int a){int a; return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: parameter redeclaration in function body should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-001 for parameter redeclaration, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_for_init_identifier_use_outside_loop(void) {
    const char *source = "int f(){for(int i=0;i<3;i=i+1){} return i;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-init identifier should be out of scope after loop\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for for-init lifetime, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_for_init_shadowing_outer_identifier(void) {
    const char *source = "int f(){int i=7;for(int i=0;i<2;i=i+1){i=i+1;}return i;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-init declaration should be allowed to shadow outer identifier, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_duplicate_parameter_names(void) {
    const char *source = "int f(int a,int a){return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: duplicate parameter names should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-001 for duplicate parameters, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (sema_err.line != 1 || sema_err.column != 17) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected duplicate parameter location at 1:17, got %d:%d\n",
            sema_err.line,
            sema_err.column);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_block_local_use_after_block_end(void) {
    const char *source = "int f(){ {int x=1;} return x; }\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: block-local identifier should be out of scope after block\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for post-block use, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_for_body_local_use_after_loop(void) {
    const char *source = "int f(){for(int i=0;i<1;i=i+1){int y=i;} return y;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-body local identifier should be out of scope after loop\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for post-loop body-local use, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_parameter_shadowing_in_inner_block(void) {
    const char *source = "int f(int a){{int a=1;} return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: inner block should be allowed to shadow parameter, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_undeclared_identifier_in_declaration_initializer(void) {
    const char *source = "int f(){int x=y; return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: undeclared identifier in declaration initializer should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for declaration initializer, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_undeclared_identifier_in_for_init_declaration_initializer(void) {
    const char *source = "int f(){for(int i=j;i<3;i=i+1){return i;} return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: undeclared identifier in for-init declaration initializer should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for for-init declaration initializer, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_same_declaration_forward_reference(void) {
    const char *source = "int f(){int x=y,y=1; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: same declaration forward reference should fail (x uses y before y is declared)\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for same declaration forward reference, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_same_declaration_reverse_reference(void) {
    const char *source = "int f(){int y=1,x=y; return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: reverse declaration reference should pass (x uses already-declared y), got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_for_init_same_declaration_forward_reference(void) {
    const char *source = "int f(){for(int i=j,j=0;i<1;i=i+1){return i;} return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-init same declaration forward reference should fail (i uses j before j is declared)\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-SCOPE-002") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-SCOPE-002 for for-init same declaration forward reference, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_for_init_same_declaration_reverse_reference(void) {
    const char *source = "int f(){for(int j=0,i=j;i<1;i=i+1){return i;} return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: for-init reverse declaration reference should pass (i uses already-declared j), got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_ast_primary_return_flow_ignores_tampered_metadata(void) {
    static const CallableDiagCase c = {
        "CF-001 ast primary return flow",
        "int main(){while(1){continue;}return 1;}\n",
        "SEMA-CF-001",
        "Function definition must return on all control-flow paths",
        1,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_ast_primary_callable_ignores_tampered_metadata(void) {
    const char *source = "int main(){return foo(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (program.count != 1 || program.externals[0].kind != AST_EXTERNAL_FUNCTION ||
        !program.externals[0].is_function_definition || !program.externals[0].function_body) {
        fprintf(stderr,
            "[semantic-reg] FAIL: setup expected one function definition with statement AST body\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: undeclared call should fail under AST-primary callable checks\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-001 from AST-primary callable path, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_ast_primary_chained_call_ignores_tampered_metadata(void) {
    const char *source = "int f(){return 1;}\nint id(int x){return x;}\nint main(){return id(1)(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    size_t main_index = (size_t)-1;
    size_t i;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    for (i = 0; i < program.count; ++i) {
        if (program.externals[i].kind == AST_EXTERNAL_FUNCTION && program.externals[i].name &&
            strcmp(program.externals[i].name, "main") == 0) {
            main_index = i;
            break;
        }
    }

    if (main_index == (size_t)-1 || !program.externals[main_index].is_function_definition ||
        !program.externals[main_index].function_body) {
        fprintf(stderr,
            "[semantic-reg] FAIL: setup expected function definition main with statement AST body\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: chained call should fail under AST-primary callable checks\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-005") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-005 from AST-primary chained-call path, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_accepts_call_with_prior_prototype_then_later_definition(void) {
    const char *source = "int foo(int a);\nint main(){return foo(1);}\nint foo(int a){return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call with prior prototype should pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_excessive_expression_depth(void) {
    const size_t repeat_count = 5000;
    const size_t max_source_size = 64 + (repeat_count * 2);
    char *source = (char *)malloc(max_source_size);
    size_t offset = 0;
    size_t i;
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!source) {
        fprintf(stderr,
            "[semantic-reg] FAIL: out of memory while building deep expression test source\n");
        return 0;
    }

    offset += (size_t)snprintf(source + offset,
        max_source_size - offset,
        "int f(int a){return a");
    for (i = 0; i < repeat_count; ++i) {
        if (offset + 2 >= max_source_size) {
            fprintf(stderr,
                "[semantic-reg] FAIL: deep expression test source buffer overflow\n");
            free(source);
            return 0;
        }
        source[offset++] = '+';
        source[offset++] = 'a';
    }
    if (offset + 4 >= max_source_size) {
        fprintf(stderr,
            "[semantic-reg] FAIL: deep expression test source terminator overflow\n");
        free(source);
        return 0;
    }
    source[offset++] = ';';
    source[offset++] = '}';
    source[offset++] = '\n';
    source[offset] = '\0';

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        free(source);
        return 0;
    }
    free(source);

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: deep expression should fail semantic analysis with depth guard\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-INT-012") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-INT-012 for deep expression, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int run_cf_matrix_expect_accepts(const char *case_id, const char *source) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should pass under current behavior, got: %s\n",
            case_id,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

/*
 * Important: these helpers encode CURRENT behavior only.
 * They are intentionally strict: every case must match its current expectation.
 * Do not relax these assertions to "accept either outcome".
 */
static int run_cf_matrix_expect_rejects(const char *case_id, const char *source) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should fail under current behavior\n",
            case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CF-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected SEMA-CF-001, got: %s\n",
            case_id,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected all-paths-return diagnostic, got: %s\n",
            case_id,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (sema_err.line <= 0 || sema_err.column <= 0) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected a concrete source location, got %d:%d\n",
            case_id,
            sema_err.line,
            sema_err.column);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

/*
 * Control-flow comparison matrix groups:
 * 1) must_pass_or_fail_now
 *    - Stable, policy-independent accept/reject cases.
 * 2) conservative_loop_acceptance
 *    - Value-dependent or mixed-exit loops that must stay accepted unless
 *      semantic flow can reliably prove non-return.
 * 3) deterministic_loop_rejects
 *    - Constant-condition loop shapes that are still reliably rejectable.
 *
 * Conservative CF policy note:
 * - `SEMA-CF-001` should avoid false positives.
 * - Value-dependent loops are accepted when exit remains plausibly mutation-driven
 *   or call-driven under the current heuristic.
 * - Value-dependent loops can still be rejected when the guard is treated as
 *   stable and loop entry does not by itself prove all-path return.
 * - Only loop shapes that remain conservatively non-returning under that policy
 *   are rejected.
 */
typedef enum {
    CF_EXPECT_ACCEPT,
    CF_EXPECT_REJECT
} CfExpectation;

typedef struct {
    const char *id;
    const char *source;
    CfExpectation expect_now;
} CfCase;

static int run_cf_case(const CfCase *c) {
    if (!c) {
        return 0;
    }
    if (c->expect_now == CF_EXPECT_ACCEPT) {
        return run_cf_matrix_expect_accepts(c->id, c->source);
    }
    return run_cf_matrix_expect_rejects(c->id, c->source);
}

static int run_cf_case_group(const char *group_name, const CfCase *cases, size_t count) {
    size_t i;

    for (i = 0; i < count; ++i) {
        if (!run_cf_case(&cases[i])) {
            fprintf(stderr,
                "[semantic-reg] FAIL: %s group failed at %s\n",
                group_name,
                cases[i].id);
            return 0;
        }
    }
    return 1;
}

static int run_extension_semantic_expect_accepts(const char *case_id, const char *source) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions options;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    memset(&options, 0, sizeof(options));
    options.allow_extension_features = 1;
    if (!semantic_analyze_program_with_options(&program, &options, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should pass under -extension semantics, got: %s\n",
            case_id,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int run_extension_semantic_expect_rejects(const char *case_id,
    const char *source,
    const char *expected_code) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;
    SemanticOptions options;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    memset(&options, 0, sizeof(options));
    options.allow_extension_features = 1;
    if (semantic_analyze_program_with_options(&program, &options, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s should fail under -extension semantics\n",
            case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (expected_code && strstr(sema_err.message, expected_code) == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: %s expected %s, got: %s\n",
            case_id,
            expected_code,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_accepts_simple_defer_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-ext-pass",
        "int main(){ defer putint(1); return 0; }\n");
}

static int test_semantic_rejects_defer_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "defer-non-extension",
        "int main(){ defer putint(1); return 0; }\n",
        "SEMA-EXT-001",
        "available only under -extension mode",
        1,
        13,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_defer_return_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-return-ext-reject",
        "int main(){ defer return 1; return 0; }\n",
        "SEMA-EXT-003");
}

static int test_semantic_rejects_defer_break_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-break-ext-reject",
        "int main(){ while(1){ defer break; break; } return 0; }\n",
        "SEMA-EXT-004");
}

static int test_semantic_rejects_defer_continue_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-continue-ext-reject",
        "int main(){ while(1){ defer continue; break; } return 0; }\n",
        "SEMA-EXT-005");
}

static int test_semantic_accepts_multiple_simple_defers_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-ext-multi-pass",
        "int main(){ defer putint(1); defer putint(2); return 0; }\n");
}

static int test_semantic_accepts_nested_defer_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-nested-ext-pass",
        "int main(){ defer { defer putint(1); } return 0; }\n");
}

static int test_semantic_accepts_nested_multi_defer_in_defer_body_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-nested-multi-ext-pass",
        "int main(){ defer { defer putint(1); { defer putint(2); } } return 0; }\n");
}

static int test_semantic_rejects_nested_return_in_defer_body_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-nested-return-ext-reject",
        "int main(){ defer { if(1){ return 1; } } return 0; }\n",
        "SEMA-EXT-003");
}

static int test_semantic_rejects_nested_break_in_defer_body_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-nested-break-ext-reject",
        "int main(){ while(1){ defer { if(1){ break; } } break; } return 0; }\n",
        "SEMA-EXT-004");
}

static int test_semantic_rejects_nested_continue_in_defer_body_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "defer-nested-continue-ext-reject",
        "int main(){ while(1){ defer { if(1){ continue; } } break; } return 0; }\n",
        "SEMA-EXT-005");
}

static int test_semantic_accepts_defer_body_internal_break_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-internal-break-ext-pass",
        "int main(){ defer { while(1){ break; } } return 0; }\n");
}

static int test_semantic_accepts_defer_body_internal_continue_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "defer-internal-continue-ext-pass",
        "int main(){ defer { int i=0; while(i<1){ i=i+1; continue; } } return 0; }\n");
}

static int test_semantic_accepts_unless_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "unless-ext-pass",
        "int main(){ unless(0) return 1; return 0; }\n");
}

static int test_semantic_rejects_unless_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "unless-non-extension",
        "int main(){ unless(0) return 1; return 0; }\n",
        "SEMA-EXT-006",
        "available only under -extension mode",
        1,
        13,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_accepts_fndefer_with_stable_names_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "fndefer-stable-names-ext-pass",
        "int g; int main(int x){ int y=0; fndefer { putint(g); putint(x); putint(y); } y=3; return 0; }\n");
}

static int test_semantic_rejects_fndefer_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "fndefer-non-extension",
        "int main(){ fndefer putint(1); return 0; }\n",
        "SEMA-EXT-007",
        "available only under -extension mode",
        1,
        13,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_fndefer_nested_block_local_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "fndefer-nested-local-ext-reject",
        "int main(){ int x=0; { int y=1; fndefer putint(y); } return 0; }\n",
        "SEMA-EXT-013");
}

static int test_semantic_rejects_fndefer_loop_local_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "fndefer-loop-local-ext-reject",
        "int main(){ for(int i=0;i<3;i=i+1){ fndefer putint(i); } return 0; }\n",
        "SEMA-EXT-013");
}

static int test_semantic_rejects_fndefer_nested_defer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "fndefer-nested-defer-ext-reject",
        "int main(){ fndefer { defer putint(1); } return 0; }\n",
        "SEMA-EXT-011");
}

static int test_semantic_rejects_fndefer_loop_body_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "fndefer-loop-body-ext-reject",
        "int main(){ fndefer while(1){ } return 0; }\n",
        "SEMA-EXT-012");
}

static int test_semantic_rejects_fndefer_later_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "fndefer-later-local-ext-reject",
        "int main(){ fndefer putint(x); int x=1; return 0; }\n",
        NULL);
}

static int test_semantic_accepts_fndefer_conditional_registration_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "fndefer-conditional-registration-ext-pass",
        "int main(){ int x=1; if(x){ fndefer putint(x); } return 0; }\n");
}

static int test_semantic_accepts_capdefer_with_stable_names_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "capdefer-stable-ext-pass",
        "int main(){ int x=1; capdefer(y=x) putint(y); x=2; return 0; }\n");
}

static int test_semantic_accepts_float_declaration_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-decl-ext-pass",
        "float g; int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_function_signature_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-fn-sig-ext-pass",
        "float id(float x){ return x; }\nint main(){ return 0; }\n");
}

static int test_semantic_accepts_float_literal_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-literal-transport-ext-pass",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float mainf(){ float x = 1.25; return id(g); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_return_transport_from_global_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-return-global-ext-pass",
        "float g = 1.25;\n"
        "float get(){ return g; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_return_transport_from_global_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-return-global-call-ext-pass",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float get(){ return id(g); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_parameter_forward_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-param-forward-ext-pass",
        "float id(float x){ return x; }\n"
        "float forward(float x){ return id(x); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_parameter_local_forward_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-param-local-forward-ext-pass",
        "float id(float x){ return x; }\n"
        "float bounce(float x){ float y; y = x; return id(y); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_global_call_chain_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-global-call-chain-ext-pass",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float getg(){ return wrap(g); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_local_call_chain_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-local-call-chain-ext-pass",
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float bounce(float x){ float y; y = x; return wrap(y); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_addition_assignment_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-assign-float-ext-pass",
        "float f(float x, float y, float z){ float t; t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_float_mul_div_assignment_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-assign-float-ext-pass",
        "float f(float a, float b, float c){ float t; t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_addition_initializer_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-init-float-ext-pass",
        "float f(float x, float y, float z){ float t = (x + y) + z; return t; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_float_mul_div_initializer_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-init-float-ext-pass",
        "float f(float a, float b, float c){ float t = -a * (b / c); return t; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_global_float_operator_expression_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "global-float-op-ext-reject",
        "float g = 1.25;\n"
        "int main(){ return g + 1; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_global_float_operator_expression_in_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "global-float-op-init-ext-reject",
        "float g = 1.25;\n"
        "int h = g + 1;\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_if_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-if-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ if(g) return 1; return 0; }\n");
}

static int test_semantic_rejects_float_while_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-while-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ while(g) return 1; return 0; }\n");
}

static int test_semantic_rejects_float_for_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-for-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ for(;g;) return 1; return 0; }\n");
}

static int test_semantic_accepts_float_not_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-not-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ if(!g) return 0; return 1; }\n");
}

static int test_semantic_accepts_float_and_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-and-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ if(g && g) return 1; return 0; }\n");
}

static int test_semantic_accepts_float_or_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-or-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ if(g || 1.25) return 1; return 0; }\n");
}

static int test_semantic_accepts_float_ternary_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ternary-cond-ext-pass",
        "float g = 1.25;\n"
        "int main(){ return g ? 1 : 0; }\n");
}

static int test_semantic_accepts_explicit_float_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-convert-cond-ext-pass",
        "int main(){ if(float(3)) return 1; return 0; }\n");
}

static int test_semantic_accepts_recursive_explicit_float_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-convert-recursive-cond-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ if(float(add3(1, 2, 3))) return 1; return 0; }\n");
}

static int test_semantic_accepts_explicit_float_while_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-convert-while-cond-ext-pass",
        "int main(){ while(float(3)) return 1; return 0; }\n");
}

static int test_semantic_accepts_recursive_explicit_float_for_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-convert-recursive-for-cond-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ for(;float(add3(1, 2, 3));) return 1; return 0; }\n");
}

static int test_semantic_accepts_explicit_float_logical_condition_composition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-convert-logic-cond-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ if(!float(0) || (float(3) && float(add3(1, 2, 3)))) return 1; return 0; }\n");
}

static int test_semantic_accepts_recursive_float_if_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-recursive-if-cond-ext-pass",
        "int f(float x, float y, float z){ if((x + y) + z) return 1; return 0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_recursive_float_while_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-recursive-while-cond-ext-pass",
        "int f(float a, float b, float c){ while(-a * (b / c)) return 1; return 0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_recursive_float_for_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-recursive-for-cond-ext-pass",
        "int f(float x, float y, float z){ for(;(x + y) + z;) return 1; return 0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_same_type_float_ternary_value_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ternary-value-ext-pass",
        "float g = 1.25;\n"
        "float h = 1.25;\n"
        "float mainf(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_assignment_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-assign-transport-ext-pass",
        "float g = 1.25;\n"
        "float id(float x){ return x; }\n"
        "int main(){ float y; y = id(g); return 0; }\n");
}

static int test_semantic_rejects_float_assignment_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-assign-int-mismatch-ext-reject",
        "float g = 1.25;\n"
        "int main(){ int x = 0; x = g; return 0; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_float_ternary_value_return_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-return-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int bad(){ return g ? h : h; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_semantic_rejects_unary_call_ternary_value_return_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-return-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int bad(){ return -id(1.0) ? 1.0 : 2.0; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_semantic_accepts_float_equality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-eq-compare-ext-pass",
        "int eq(float x, float y){ return x == y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_inequality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ne-compare-ext-pass",
        "int ne(float x, float y){ return x != y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_relational_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-lt-compare-ext-pass",
        "int lt(float x, float y){ return x < y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_literal_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-literal-ext-pass",
        "float neg(){ return -1.25; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_minus_float_identifier_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-identifier-ext-pass",
        "float g = 1.25;\n"
        "float neg(){ return -g; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_minus_float_call_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-call-ext-pass",
        "float id(float x){ return x; }\n"
        "float g = 1.25;\n"
        "float neg(){ return -id(g); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_plus_float_identifier_transport_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-positive-identifier-ext-pass",
        "float g = 1.25;\n"
        "float pos(){ return +g; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_zero_float_condition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-zero-cond-ext-pass",
        "int main(){ if(-0.0) return 1; return 0; }\n");
}

static int test_semantic_accepts_negative_float_relational_compare_against_zero_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-lt-zero-ext-pass",
        "int lt(){ return -1.25 < 0.0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_relational_compare_against_negative_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-lt-negative-ext-pass",
        "int lt(){ return -1.25 < -0.5; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_zero_lt_zero_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-zero-lt-zero-ext-pass",
        "int lt(){ return -0.0 < 0.0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_zero_le_zero_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-negative-zero-le-zero-ext-pass",
        "int le(){ return -0.0 <= 0.0; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_addition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-add-ext-pass",
        "float add(float x, float y){ return x + y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_subtraction_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-sub-ext-pass",
        "float sub(float x, float y){ return x - y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_addition_combo_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-neg-add-combo-ext-pass",
        "float g = 1.25;\n"
        "float add(float y){ return -g + y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_subtraction_combo_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-neg-sub-combo-ext-pass",
        "float g = 1.25;\n"
        "float sub(float y){ return y - -g; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_multiplication_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-mul-ext-pass",
        "float mul(float x, float y){ return x * y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_division_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-div-ext-pass",
        "float divv(float x, float y){ return x / y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_multiplication_combo_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-neg-mul-combo-ext-pass",
        "float g = 1.25;\n"
        "float mul(float y){ return -g * y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_negative_float_division_combo_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-neg-div-combo-ext-pass",
        "float g = 1.25;\n"
        "float divv(float y){ return y / -g; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_addition_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-ext-pass",
        "float add3(float x, float y, float z){ return (x + y) + z; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_float_mul_div_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-ext-pass",
        "float f(float a, float b, float c){ return -a * (b / c); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_equality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-eq-compare-ext-pass",
        "int eq(float x, float y, float z){ return ((x + y) + z) == z; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_relational_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-lt-compare-ext-pass",
        "int lt(float x, float y, float z){ return ((x + y) + z) < z; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_muldiv_float_equality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-eq-compare-ext-pass",
        "int eq(float a, float b, float c){ return (-a * (b / c)) == c; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_muldiv_float_relational_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-lt-compare-ext-pass",
        "int lt(float a, float b, float c){ return (-a * (b / c)) < c; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_inequality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-ne-compare-ext-pass",
        "int ne(float x, float y, float z){ return ((x + y) + z) != z; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_le_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-le-compare-ext-pass",
        "int le(float x, float y, float z){ return ((x + y) + z) <= z; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_muldiv_float_inequality_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-ne-compare-ext-pass",
        "int ne(float a, float b, float c){ return (-a * (b / c)) != c; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_muldiv_float_ge_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-ge-compare-ext-pass",
        "int ge(float a, float b, float c){ return (-a * (b / c)) >= c; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_mixed_float_int_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-int-arith-mismatch-ext-reject",
        "float add(float x){ return x + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_float_literal_int_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-literal-int-arith-mismatch-ext-reject",
        "float add(){ return 1.25 + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_float_call_int_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-call-int-arith-mismatch-ext-reject",
        "float id(float x){ return x; }\n"
        "float add(float x){ return id(x) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_negative_float_call_int_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-neg-call-int-arith-mismatch-ext-reject",
        "float id(float x){ return x; }\n"
        "float add(float x){ return -id(x) * 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_global_float_int_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-global-cond-plus-int-ext-reject",
        "float g = 1.25;\n"
        "int main(){ if(g + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_call_int_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-call-cond-plus-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int main(){ if(id(1.0) + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_negative_float_call_int_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-neg-call-cond-plus-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int main(){ if(-id(1.0) + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_nested_float_tree_plus_int_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-nested-tree-cond-plus-int-ext-reject",
        "int main(float x, float y, float z){ if(((x + y) + z) + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_nested_float_muldiv_plus_int_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-nested-muldiv-cond-plus-int-ext-reject",
        "int main(float a, float b, float c){ if((-a * (b / c)) + 1) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_compare_against_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-compare-int-mismatch-ext-reject",
        "float g = 1.25;\n"
        "int main(){ return g == 1; }\n",
        "SEMA-TYPE-007");
}

static int test_semantic_rejects_nested_float_tree_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-nested-tree-plus-int-ext-reject",
        "float add(float x, float y){ return (x + y) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_nested_float_muldiv_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-nested-muldiv-plus-int-ext-reject",
        "float f(float a, float b, float c){ return (-a * (b / c)) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_ternary_value_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-plus-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return (g ? h : h) + 1; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_unary_call_ternary_value_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-plus-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float add(float x){ return (-id(x) ? x : x) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_ternary_value_plus_float_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-plus-float-callarg-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float wrap(float x){ return x; }\n"
        "float get(){ return wrap((g ? h : h) + h); }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_unary_call_ternary_value_plus_float_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-plus-float-callarg-ext-reject",
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return x; }\n"
        "float f(float x){ return wrap(((-id(x) ? x : x)) + x); }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_ternary_value_assignment_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-assign-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ int x=0; x = (g ? h : h); return x; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_unary_call_ternary_value_assignment_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-assign-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int main(){ int y=0; y = (-id(1.0) ? 1.0 : 2.0); return y; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_accepts_same_type_float_ternary_value_assignment_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ternary-value-assign-float-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ float y; y = (g ? h : h); return 0; }\n");
}

static int test_semantic_accepts_unary_call_same_type_float_ternary_value_assignment_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-unary-call-ternary-assign-float-ext-pass",
        "float id(float x){ return x; }\n"
        "float f(float x){ float y; y = (-id(x) ? x : x); return y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_same_type_float_ternary_value_initializer_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ternary-value-init-float-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float y = (g ? h : h);\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_float_ternary_value_compare_against_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-compare-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return (g ? h : h) == 0; }\n",
        "SEMA-TYPE-007");
}

static int test_semantic_rejects_unary_call_ternary_value_compare_against_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-compare-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int main(){ return (-id(1.0) ? 1.0 : 2.0) == 0; }\n",
        "SEMA-TYPE-007");
}

static int test_semantic_rejects_float_ternary_value_compare_against_float_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-compare-float-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int eq(){ return (g ? h : h) == h; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_unary_call_ternary_value_compare_against_float_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-compare-float-ext-reject",
        "float id(float x){ return x; }\n"
        "int eq(float x){ return (-id(x) ? x : x) == x; }\n"
        "int main(){ return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_rejects_float_ternary_value_call_argument_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-callarg-int-ext-reject",
        "int sink(int x){ return x; }\n"
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return sink((g ? h : h)); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_unary_call_ternary_value_call_argument_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-callarg-int-ext-reject",
        "int sink(int x){ return x; }\n"
        "float id(float x){ return x; }\n"
        "int main(){ return sink((-id(1.0) ? 1.0 : 2.0)); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_same_type_float_ternary_value_call_argument_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-ternary-value-callarg-float-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((g ? h : h)); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_call_same_type_float_ternary_value_call_argument_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-unary-call-ternary-callarg-float-ext-pass",
        "float id(float x){ return x; }\n"
        "float wrap(float x){ return id(x); }\n"
        "float get(){ return wrap((-id(1.0) ? 1.0 : 2.0)); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_chained_float_addition_call_argument_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-chain-add-callarg-float-ext-pass",
        "float wrap(float x){ return x; }\n"
        "float get(float x, float y, float z){ return wrap((x + y) + z); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_nested_float_mul_div_call_argument_to_float_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-nested-mul-div-callarg-float-ext-pass",
        "float wrap(float x){ return x; }\n"
        "float f(float a, float b, float c){ return wrap(-a * (b / c)); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_recursive_float_condition_with_ternary_neighbor_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-recursive-cond-ternary-neighbor-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ if((g ? h : h) + h) return 1; return 0; }\n",
        "SEMA-EXT-035");
}

static int test_semantic_accepts_float_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-helper-ternary-call-arith-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "float get(){ return pick() + h; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_call_helper_wrapped_ternary_call_arithmetic_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-unary-helper-ternary-call-arith-ext-pass",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "float f(float x){ return pick(x) + x; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_float_helper_wrapped_ternary_call_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-helper-ternary-call-compare-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int eq(){ return pick() == h; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_unary_call_helper_wrapped_ternary_call_compare_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-unary-helper-ternary-call-compare-ext-pass",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int eq(float x){ return pick(x) == x; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-plus-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "float add(){ return pick() + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-plus-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "float add(float x){ return pick(x) + 1; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_condition_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-cond-plus-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int main(){ if(pick() + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_condition_plus_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-cond-plus-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int main(float x){ if(pick(x) + 1) return 1; return 0; }\n",
        "SEMA-TYPE-008");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_compare_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-compare-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int eq(){ return pick() == 0; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-007");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_compare_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-compare-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int eq(float x){ return pick(x) == 0; }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-007");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_return_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-return-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int bad(){ return pick(); }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_return_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-return-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int bad(float x){ return pick(x); }\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-005");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_initializer_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-init-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int x = pick();\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_initializer_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-init-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int y = pick(1.0);\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_assignment_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-assign-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int main(){ int x = 0; x = pick(); return 0; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_assignment_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-assign-int-ext-reject",
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int main(){ int y = 0; y = pick(1.0); return 0; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_float_helper_wrapped_ternary_call_argument_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-helper-ternary-call-callarg-int-ext-reject",
        "int sink(int x){ return x; }\n"
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "float pick(){ return g ? h : h; }\n"
        "int main(){ return sink(pick()); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_unary_call_helper_wrapped_ternary_call_argument_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-helper-ternary-call-callarg-int-ext-reject",
        "int sink(int x){ return x; }\n"
        "float id(float x){ return x; }\n"
        "float pick(float x){ return -id(x) ? x : x; }\n"
        "int main(){ return sink(pick(1.0)); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_float_ternary_value_initializer_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-ternary-value-init-int-ext-reject",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int x = (g ? h : h);\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_unary_call_ternary_value_initializer_to_int_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-unary-call-ternary-init-int-ext-reject",
        "float id(float x){ return x; }\n"
        "int y = (-id(1.0) ? 1.0 : 2.0);\n"
        "int main(){ return 0; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_accepts_explicit_int_from_float_conversion_on_direct_root_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-float-direct-ext-pass",
        "float g = 1.25;\n"
        "int main(){ return int(g); }\n");
}

static int test_semantic_accepts_explicit_float_from_int_conversion_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-float-from-int-ext-pass",
        "float conv(int x, int y){ return float(x + y); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_redundant_explicit_same_type_conversion_for_now(void) {
    return run_extension_semantic_expect_rejects(
        "float-explicit-same-type-ext-reject",
        "float g = 1.25;\n"
        "int main(){ return float(g) == g; }\n",
        "SEMA-EXT-038");
}

static int test_semantic_accepts_explicit_int_from_float_conversion_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-float-ext-pass",
        "int conv(float x, float y){ return int(x + y); }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_explicit_int_from_float_ternary_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-float-ternary-bridge-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h); }\n");
}

static int test_semantic_accepts_explicit_int_from_recursive_float_call_argument_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-recursive-callarg-bridge-ext-pass",
        "int sink(int x){ return x; }\n"
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return sink(int(add3(1.0, 2.0, 3.0))); }\n");
}

static int test_semantic_accepts_explicit_int_from_recursive_float_initializer_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-recursive-init-bridge-ext-pass",
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int x = int(add3(1.0, 2.0, 3.0));\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_explicit_int_from_recursive_float_assignment_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-recursive-assign-bridge-ext-pass",
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ int x=0; x = int(add3(1.0, 2.0, 3.0)); return x; }\n");
}

static int test_semantic_accepts_explicit_int_from_float_compare_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-float-compare-bridge-ext-pass",
        "float g = 1.25;\n"
        "float h = 2.5;\n"
        "int main(){ return int(g ? h : h) == 2; }\n");
}

static int test_semantic_accepts_explicit_int_from_recursive_float_arithmetic_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-int-from-recursive-arith-bridge-ext-pass",
        "float add3(float a, float b, float c){ return (a + b) + c; }\n"
        "int main(){ return int(add3(1.0, 2.0, 3.0)) + 1; }\n");
}

static int test_semantic_accepts_explicit_float_from_recursive_int_initializer_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-float-from-recursive-init-bridge-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float z = float(add3(1, 2, 3));\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_explicit_float_from_recursive_int_assignment_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-float-from-recursive-assign-bridge-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ float y; y = float(add3(1, 2, 3)); return y; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_explicit_float_from_int_compare_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-float-from-int-compare-bridge-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "int main(){ return float(add3(1, 2, 3)) == float(6); }\n");
}

static int test_semantic_accepts_explicit_float_from_recursive_int_arithmetic_bridge_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-explicit-float-from-recursive-arith-bridge-ext-pass",
        "int add3(int a, int b, int c){ return (a + b) + c; }\n"
        "float mainf(){ return float(add3(1, 2, 3)) + 1.25; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_rejects_capdefer_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "capdefer-non-extension",
        "int main(){ int x=1; capdefer(y=x) putint(y); return 0; }\n",
        "SEMA-EXT-027",
        "available only under -extension mode",
        1,
        22,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_capdefer_nested_block_local_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "capdefer-nested-block-local-ext-reject",
        "int main(){ { int x=1; capdefer(y=x) return y; } return 0; }\n",
        "SEMA-EXT-022");
}

static int test_semantic_rejects_capdefer_loop_registration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "capdefer-loop-registration-ext-reject",
        "int main(){ int x=1; for(;x<2;x=x+1){ capdefer(y=x) putint(y); } return 0; }\n",
        "SEMA-EXT-034");
}

static int test_semantic_rejects_function_valued_parameter_extension_slice_for_now(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-ext-reject-for-now",
        "int apply(int f(int), int x){ return f; }\nint main(){ return 0; }\n",
        "SEMA-EXT-017");
}

static int test_semantic_rejects_function_valued_parameter_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "function-valued-param-non-extension",
        "int add1(int x){ return x+1; }\nint apply(int f(int), int x){ return f(x); }\nint main(){ return apply(add1, 41); }\n",
        "SEMA-EXT-015",
        "available only under -extension mode",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_accepts_function_valued_parameter_declaration_under_extension_when_unused(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-ext-unused-pass",
        "int apply(int f(int), int x){ return x; }\nint main(){ return 0; }\n");
}

static int test_semantic_accepts_function_valued_return_declaration_extension_slice_for_now(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-decl-pass-for-now",
        "int make(int x)(int);\nint main(){ return 0; }\n");
}

static int test_semantic_accepts_compatible_function_valued_return_redeclaration_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-redecl-pass",
        "int make(int x)(int);\nint make(int x)(int);\nint main(){ return 0; }\n");
}

static int test_semantic_rejects_mismatched_function_valued_return_redeclaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-redecl-mismatch",
        "int make(int x)(int);\nvoid make(int x)(int);\nint main(){ return 0; }\n",
        "SEMA-TOP-005");
}

static int test_semantic_accepts_function_valued_return_definition_with_closure_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-def-closure-pass",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\nint main(){ return 0; }\n");
}

static int test_semantic_rejects_function_valued_return_definition_with_mismatched_closure_signature_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-def-closure-mismatch",
        "int make(int x)(int) { return closure [x] void () { return; }; }\nint main(){ return 0; }\n",
        "SEMA-EXT-045");
}

static int test_semantic_rejects_function_valued_return_definition_with_multiple_closure_return_sites_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-def-multi-return-site-reject",
        "int make(int x)(int) { if (x) return closure [x] int (int y) { return x + y; }; return closure [x] int (int y) { return x - y; }; }\nint main(){ return 0; }\n",
        "SEMA-EXT-049");
}

static int test_semantic_accepts_local_function_value_initialization_from_function_valued_return_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-local-init-pass",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\nint main(){ int f(int)=make(3); return 0; }\n");
}

static int test_semantic_accepts_immediate_call_of_function_valued_return_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-immediate-call-pass",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\nint main(){ return make(3)(4); }\n");
}

static int test_semantic_rejects_closure_function_valued_return_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-closure-immediate-call-scalar-type-mismatch-reject",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ return make(3)(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_closure_function_valued_return_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-closure-immediate-call-arity-mismatch-reject",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ return make(3)(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_local_closure_function_valued_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-local-closure-bind-call-pass",
        "int pick(int x)(int) { int f(int)=closure [x] int (int y) { return x + y; }; return f; }\n"
        "int main(){ int h(int)=pick(3); return h(4); }\n");
}

static int test_semantic_accepts_local_closure_function_valued_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-local-closure-immediate-call-pass",
        "int pick(int x)(int) { int f(int)=closure [x] int (int y) { return x + y; }; return f; }\n"
        "int main(){ return pick(3)(4); }\n");
}

static int test_semantic_accepts_dynamic_local_closure_function_valued_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-dynamic-local-closure-bind-call-pass",
        "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
        "int main(){ int h(int)=pick(3, 1); return h(4); }\n");
}

static int test_semantic_accepts_dynamic_local_closure_function_valued_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-dynamic-local-closure-immediate-call-pass",
        "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
        "int main(){ return pick(3, 1)(4); }\n");
}

static int test_semantic_accepts_dynamic_local_closure_function_valued_return_forward_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-dynamic-local-closure-forward-into-param-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int pick(int x, int c)(int) { int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=closure [x] int (int y) { return x - y; }; if(c) f=g; return f; }\n"
        "int main(){ return apply(pick(5, 1), 3); }\n");
}

static int test_semantic_accepts_noncapturing_function_valued_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-noncapturing-bind-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int pick()(int) { return add1; }\n"
        "int main(){ int f(int)=pick(); return f(41); }\n");
}

static int test_semantic_accepts_noncapturing_function_valued_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-noncapturing-immediate-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int pick()(int) { return add1; }\n"
        "int main(){ return pick()(41); }\n");
}

static int test_semantic_rejects_noncapturing_function_valued_return_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-noncapturing-immediate-call-scalar-type-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int pick()(float) { return addf; }\n"
        "int main(){ return pick()(0); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_noncapturing_function_valued_return_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-noncapturing-immediate-call-arity-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int pick()(int) { return add1; }\n"
        "int main(){ return pick()(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_rejects_noncapturing_function_valued_return_immediate_call_pair_argument_scalar_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-noncapturing-immediate-call-pair-arg-scalar-mismatch-reject",
        "int sum(pair p){ return p.first + p.second; }\n"
        "int pick()(pair) { return sum; }\n"
        "int main(){ return pick()(1); }\n",
        "SEMA-AGG-005");
}

static int test_semantic_rejects_noncapturing_function_valued_return_immediate_call_struct_argument_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-noncapturing-immediate-call-struct-arg-kind-mismatch-reject",
        "struct P { int x; int y; };\n"
        "struct Q { int x; int y; };\n"
        "int sum(struct P p){ return p.x + p.y; }\n"
        "int pick()(struct P) { return sum; }\n"
        "int main(){ struct Q q={1,2}; return pick()(q); }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_zero_arg_function_valued_return_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-ext-zero-arg-immediate-call-arity-mismatch-reject",
        "int next(){ return 9; }\n"
        "int pick()(){ return next; }\n"
        "int main(){ return pick()(1); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_local_noncapturing_function_value_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-local-noncapturing-immediate-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int pick()(int) { int f(int)=add1; return f; }\n"
        "int main(){ return pick()(41); }\n");
}

static int test_semantic_accepts_rebound_local_noncapturing_function_value_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-local-rebound-immediate-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick()(int) { int f(int)=add1; f=add2; return f; }\n"
        "int main(){ return pick()(40); }\n");
}

static int test_semantic_accepts_dynamic_noncapturing_function_value_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-dynamic-immediate-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
        "int main(){ return pick(1)(40); }\n");
}

static int test_semantic_accepts_dynamic_noncapturing_function_value_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-dynamic-bind-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
        "int main(){ int g(int)=pick(1); return g(40); }\n");
}

static int test_semantic_accepts_ternary_noncapturing_function_value_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-ternary-noncapturing-immediate-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
        "int main(){ return pick(1)(40); }\n");
}

static int test_semantic_accepts_ternary_noncapturing_function_value_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-ternary-noncapturing-bind-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int){ int f(int)=add1; int g(int)=add2; return ((putint(0), c) ? f : g); }\n"
        "int main(){ int h(int)=pick(1); return h(40); }\n");
}

static int test_semantic_accepts_ternary_closure_function_value_return_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-ternary-closure-immediate-call-pass",
        "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
        "int main(){ return pick(1)(4); }\n");
}

static int test_semantic_accepts_ternary_closure_function_value_return_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-return-ext-ternary-closure-bind-call-pass",
        "int pick(int c)(int){ int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; return ((putint(0), c) ? f : g); }\n"
        "int main(){ int h(int)=pick(1); return h(4); }\n");
}

static int test_semantic_accepts_dynamic_local_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-local-function-value-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int g(int)=add1; if(c) g=add2; return apply(g, 40); }\n");
}

static int test_semantic_accepts_dynamic_returned_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-function-value-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int pick(int c)(int) { int f(int)=add1; if(c) f=add2; return f; }\n"
        "int main(){ int g(int)=pick(1); return apply(g, 40); }\n");
}

static int test_semantic_accepts_dynamic_local_zero_arg_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-local-zero-arg-function-value-forward-into-param-pass",
        "int next1(){ return 1; }\n"
        "int next2(){ return 2; }\n"
        "int apply0(int f()){ return f(); }\n"
        "int main(){ int c=1; int g()=next1; if(c) g=next2; return apply0(g); }\n");
}

static int test_semantic_accepts_dynamic_local_zero_arg_void_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-local-zero-arg-void-function-value-forward-into-param-pass",
        "void ping1(){ putint(1); }\n"
        "void ping2(){ putint(2); }\n"
        "void apply0(void f()){ f(); }\n"
        "int main(){ int c=1; void g()=ping1; if(c) g=ping2; apply0(g); return 0; }\n");
}

static int test_semantic_accepts_dynamic_returned_zero_arg_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-zero-arg-function-value-forward-into-param-pass",
        "int next1(){ return 1; }\n"
        "int next2(){ return 2; }\n"
        "int apply0(int f()){ return f(); }\n"
        "int pick(int c)(){ int g()=next1; if(c) g=next2; return g; }\n"
        "int main(){ int g()=pick(1); return apply0(g); }\n");
}

static int test_semantic_accepts_direct_returned_noncapturing_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-noncapturing-function-value-arg-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int add1(int x){ return x+1; }\n"
        "int pick()(int){ return add1; }\n"
        "int main(){ return apply(pick(), 4); }\n");
}

static int test_semantic_accepts_returned_function_value_parameter_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-function-value-parameter-immediate-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return id(add1)(41); }\n");
}

static int test_semantic_rejects_returned_function_value_parameter_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "returned-function-value-parameter-immediate-call-scalar-type-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return id(add1)(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_returned_function_value_parameter_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "returned-function-value-parameter-immediate-call-arity-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return id(add1)(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_returned_function_value_parameter_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-function-value-parameter-bind-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ int g(int)=id(add1); return g(41); }\n");
}

static int test_semantic_accepts_dynamic_returned_function_value_parameter_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-function-value-parameter-immediate-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; if(c) f=add2; return id(f)(40); }\n");
}

static int test_semantic_rejects_dynamic_returned_function_value_parameter_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-returned-function-value-parameter-immediate-call-scalar-type-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; if(c) f=add2; return id(f)(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_dynamic_returned_function_value_parameter_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-returned-function-value-parameter-immediate-call-arity-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; if(c) f=add2; return id(f)(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_dynamic_returned_function_value_parameter_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-function-value-parameter-bind-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; if(c) f=add2; int g(int)=id(f); return g(40); }\n");
}

static int test_semantic_accepts_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-backed-function-value-parameter-immediate-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(4); }\n");
}

static int test_semantic_rejects_returned_closure_backed_function_value_parameter_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "returned-closure-backed-function-value-parameter-immediate-call-scalar-type-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_returned_closure_backed_function_value_parameter_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "returned-closure-backed-function-value-parameter-immediate-call-arity-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return id(f)(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-backed-function-value-parameter-bind-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; int g(int)=id(f); return g(4); }\n");
}

static int test_semantic_accepts_dynamic_returned_closure_backed_function_value_parameter_immediate_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-closure-backed-function-value-parameter-immediate-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(4); }\n");
}

static int test_semantic_rejects_dynamic_returned_closure_backed_function_value_parameter_immediate_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-returned-closure-backed-function-value-parameter-immediate-call-scalar-type-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_dynamic_returned_closure_backed_function_value_parameter_immediate_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-returned-closure-backed-function-value-parameter-immediate-call-arity-mismatch-reject",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return id(f)(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_dynamic_returned_closure_backed_function_value_parameter_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-closure-backed-function-value-parameter-bind-call-pass",
        "int id(int f(int))(int) { return f; }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; int h(int)=id(f); return h(4); }\n");
}

static int test_semantic_accepts_direct_returned_noncapturing_zero_arg_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-noncapturing-zero-arg-function-value-arg-pass",
        "int apply0(int f()){ return f(); }\n"
        "int next(){ return 9; }\n"
        "int pick()(){ return next; }\n"
        "int main(){ return apply0(pick()); }\n");
}

static int test_semantic_accepts_direct_returned_noncapturing_zero_arg_void_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-noncapturing-zero-arg-void-function-value-arg-pass",
        "void apply0(void f()){ f(); }\n"
        "void ping(){ putint(7); }\n"
        "void pick()(){ return ping; }\n"
        "int main(){ apply0(pick()); return 0; }\n");
}

static int test_semantic_accepts_direct_returned_closure_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-closure-function-value-arg-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ return apply(make(3), 4); }\n");
}

static int test_semantic_accepts_direct_returned_closure_zero_arg_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-closure-zero-arg-function-value-arg-pass",
        "int apply0(int f()){ return f(); }\n"
        "int make(int x)() { return closure [x] int () { return x; }; }\n"
        "int main(){ return apply0(make(3)); }\n");
}

static int test_semantic_rejects_direct_returned_closure_zero_arg_function_value_argument_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "direct-returned-closure-zero-arg-function-value-arg-arity-mismatch-reject",
        "int apply0(int f()){ return f(1); }\n"
        "int make(int x)() { return closure [x] int () { return x; }; }\n"
        "int main(){ return apply0(make(3)); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_direct_returned_closure_zero_arg_void_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-returned-closure-zero-arg-void-function-value-arg-pass",
        "void apply0(void f()){ f(); }\n"
        "void make(int x)() { return closure [x] void () { putint(x); return; }; }\n"
        "int main(){ apply0(make(7)); return 0; }\n");
}

static int test_semantic_rejects_direct_returned_closure_zero_arg_void_function_value_argument_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "direct-returned-closure-zero-arg-void-function-value-arg-arity-mismatch-reject",
        "void apply0(void f()){ f(1); }\n"
        "void make(int x)() { return closure [x] void () { putint(x); return; }; }\n"
        "int main(){ apply0(make(7)); return 0; }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_direct_dynamic_returned_noncapturing_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-dynamic-returned-noncapturing-function-value-arg-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int){ int f(int)=add1; if(c) f=add2; return f; }\n"
        "int main(){ return apply(pick(1), 40); }\n");
}

static int test_semantic_accepts_direct_dynamic_returned_noncapturing_zero_arg_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-dynamic-returned-noncapturing-zero-arg-function-value-arg-pass",
        "int apply0(int f()){ return f(); }\n"
        "int next1(){ return 1; }\n"
        "int next2(){ return 2; }\n"
        "int pick(int c)(){ int g()=next1; if(c) g=next2; return g; }\n"
        "int main(){ return apply0(pick(1)); }\n");
}

static int test_semantic_accepts_direct_dynamic_returned_noncapturing_zero_arg_void_function_value_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "direct-dynamic-returned-noncapturing-zero-arg-void-function-value-arg-pass",
        "void apply0(void f()){ f(); }\n"
        "void ping1(){ putint(1); }\n"
        "void ping2(){ putint(2); }\n"
        "void pick(int c)(){ void g()=ping1; if(c) g=ping2; return g; }\n"
        "int main(){ apply0(pick(1)); return 0; }\n");
}

static int test_semantic_accepts_dynamic_function_value_assignment_then_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-function-value-assignment-then-direct-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if(c) f=add2; h=f; return h(40); }\n");
}

static int test_semantic_accepts_ternary_function_value_assignment_then_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-function-value-assignment-then-direct-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=add1; h=((putint(0), c) ? f : g); return h(40); }\n");
}

static int test_semantic_accepts_ternary_closure_assignment_then_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-closure-assignment-then-direct-call-pass",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z){ return x+z; }; int g(int)=closure [y] int (int z){ return y+z; }; int h(int)=f; h=((putint(0), c) ? f : g); return h(4); }\n");
}

static int test_semantic_accepts_dynamic_function_value_assignment_then_forward_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-function-value-assignment-then-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if(c) f=add2; h=f; return apply(h, 40); }\n");
}

static int test_semantic_accepts_dynamic_function_value_assignment_then_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-function-value-assignment-then-return-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; h=f; return h; }\n"
        "int main(){ return pick(1)(40); }\n");
}

static int test_semantic_accepts_comma_wrapped_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "comma-wrapped-function-value-direct-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int main(){ int g(int)=add1; return (0, g)(41); }\n");
}

static int test_semantic_rejects_comma_wrapped_function_value_direct_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "comma-wrapped-function-value-direct-call-scalar-type-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int main(){ int f(float)=addf; return (0, f)(0); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_assignment_result_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "assignment-result-function-value-direct-call-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int f(int)=add1; int h(int)=add1; return (h=f)(41); }\n");
}

static int test_semantic_rejects_assignment_result_function_value_direct_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "assignment-result-function-value-direct-call-scalar-type-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int main(){ int f(float)=addf; int h(float)=addf; return (h=f)(0); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_wrapped_function_value_local_initializer_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "wrapped-function-value-local-initializer-pass",
        "int add1(int x){ return x+1; }\n"
        "int main(){ int f(int)=add1; int h(int)=(0, f); return h(41); }\n");
}

static int test_semantic_accepts_ternary_function_value_local_initializer_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-function-value-local-initializer-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(int)=c ? f : g; return h(40); }\n");
}

static int test_semantic_rejects_ternary_function_value_local_initializer_parameter_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-function-value-local-initializer-param-kind-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; int h(float)=c ? f : g; return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_function_valued_return_call_local_initializer_parameter_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-return-call-local-initializer-param-kind-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int pick()(int){ return add1; }\n"
        "int main(){ int h(float)=pick(); return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_accepts_wrapped_function_value_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "wrapped-function-value-return-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick()(int){ int f(int)=add1; int h(int)=add1; return (h=f); }\n"
        "int main(){ return pick()(41); }\n");
}

static int test_semantic_accepts_dynamic_wrapped_function_value_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-wrapped-function-value-return-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int pick(int c)(int){ int f(int)=add1; int h(int)=add1; if(c) f=add2; return (h=f); }\n"
        "int main(){ return pick(1)(40); }\n");
}

static int test_semantic_accepts_dynamic_comma_wrapped_function_value_forwarding_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-comma-wrapped-function-value-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int g(int)=add1; if(c) g=add2; return apply((0, g), 40); }\n");
}

static int test_semantic_rejects_dynamic_comma_wrapped_function_value_forwarding_into_parameter_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-comma-wrapped-function-value-forward-into-param-scalar-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int c=1; int g(int)=add1; if(c) g=add2; return apply((0, g), 40); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_dynamic_assignment_result_function_value_forwarding_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-assignment-result-function-value-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if(c) f=add2; return apply((h=f), 40); }\n");
}

static int test_semantic_rejects_dynamic_assignment_result_function_value_forwarding_into_parameter_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "dynamic-assignment-result-function-value-forward-into-param-scalar-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int c=1; int f(int)=add1; int h(int)=add1; if(c) f=add2; return apply((h=f), 40); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_ternary_function_value_actual_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-function-value-actual-argument-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(((putint(0), c) ? f : g), 40); }\n");
}

static int test_semantic_rejects_ternary_function_value_actual_argument_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-function-value-actual-argument-scalar-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(((putint(0), c) ? f : g), 40); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_ternary_function_value_actual_argument_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-function-value-actual-argument-arity-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int apply(int f(int), int x){ return f(1, 2); }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return apply(((putint(0), c) ? f : g), 40); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_ternary_closure_actual_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-closure-actual-argument-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }\n");
}

static int test_semantic_rejects_ternary_closure_actual_argument_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-closure-actual-argument-scalar-mismatch-reject",
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_ternary_closure_actual_argument_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-closure-actual-argument-arity-mismatch-reject",
        "int apply(int f(int), int x){ return f(1, 2); }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return apply(((putint(0), c) ? f : g), 4); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_ternary_function_value_callee_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-function-value-callee-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return (((putint(0), c) ? f : g))(40); }\n");
}

static int test_semantic_rejects_ternary_function_value_callee_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-function-value-callee-scalar-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return (((putint(0), c) ? f : g))(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_ternary_function_value_callee_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-function-value-callee-arity-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=1; int f(int)=add1; int g(int)=add2; return (((putint(0), c) ? f : g))(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_ternary_closure_callee_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-closure-callee-pass",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return (((putint(0), c) ? f : g))(4); }\n");
}

static int test_semantic_rejects_ternary_closure_callee_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-closure-callee-scalar-mismatch-reject",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return (((putint(0), c) ? f : g))(1.25); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_ternary_closure_callee_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "ternary-closure-callee-arity-mismatch-reject",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; return (((putint(0), c) ? f : g))(1, 2); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_rejects_calling_function_with_function_valued_parameter_under_extension_for_now(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-ext-call-pass",
        "int add1(int x){ return x+1; }\nint apply(int f(int), int x){ return x; }\nint main(){ return apply(add1, 41); }\n");
}

static int test_semantic_rejects_non_function_arg_to_function_valued_parameter_call_under_extension_for_now(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-ext-nonfunction-call-reject",
        "int apply(int f(int), int x){ return x; }\nint main(){ return apply(0, 41); }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_top_level_function_value_in_plain_value_position_under_extension_for_now(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-top-level-plain-value-reject",
        "int add1(int x){ return x+1; }\nint main(){ return add1; }\n",
        "SEMA-EXT-019");
}

static int test_semantic_accepts_direct_call_of_function_valued_parameter_under_extension_for_now(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-direct-call-semantic-pass",
        "int add1(int x){ return x+1; }\nint apply(int f(int), int x){ return f(x); }\nint main(){ return apply(add1, 41); }\n");
}

static int test_semantic_rejects_direct_call_of_function_valued_parameter_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-direct-call-scalar-type-mismatch-reject",
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return apply(add1, 0); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_direct_call_of_function_valued_parameter_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-direct-call-arity-mismatch-reject",
        "int apply(int f(int), int x){ return f(1, 2); }\n"
        "int add1(int x){ return x+1; }\n"
        "int main(){ return apply(add1, 0); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_local_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-function-value-direct-call-pass",
        "int add1(int x){ return x+1; }\nint main(){ int f(int)=add1; return f(41); }\n");
}

static int test_semantic_rejects_local_function_value_direct_call_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-direct-call-scalar-type-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int main(){ int f(float)=addf; return f(0); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_local_function_value_direct_call_pair_argument_scalar_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-direct-call-pair-arg-scalar-mismatch-reject",
        "int sum(pair p){ return p.first + p.second; }\n"
        "int main(){ int f(pair)=sum; return f(1); }\n",
        "SEMA-AGG-005");
}

static int test_semantic_rejects_local_function_value_direct_call_struct_argument_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-direct-call-struct-arg-kind-mismatch-reject",
        "struct P { int x; int y; };\n"
        "struct Q { int x; int y; };\n"
        "int sum(struct P p){ return p.x + p.y; }\n"
        "int main(){ struct Q q={1,2}; int f(struct P)=sum; return f(q); }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_local_function_value_plain_value_position_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-plain-value-reject",
        "int add1(int x){ return x+1; }\nint main(){ int f(int)=add1; return f; }\n",
        "SEMA-EXT-019");
}

static int test_semantic_accepts_local_zero_arg_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-zero-arg-function-value-direct-call-pass",
        "int next(){ return 9; }\nint main(){ int f()=next; return f(); }\n");
}

static int test_semantic_rejects_local_zero_arg_function_value_direct_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-zero-arg-function-value-direct-call-arity-mismatch-reject",
        "int next(){ return 9; }\nint main(){ int f()=next; return f(1); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_local_zero_arg_void_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-zero-arg-void-function-value-direct-call-pass",
        "void ping(){ putint(1); return; }\nint main(){ void f()=ping; f(); return 0; }\n");
}

static int test_semantic_accepts_local_builtin_function_value_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-builtin-function-value-direct-call-pass",
        "int main(){ void f(int)=putint; f(7); return 0; }\n");
}

static int test_semantic_accepts_local_function_value_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-function-value-forward-into-param-pass",
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int g(int)=add1; return apply(g, 41); }\n");
}

static int test_semantic_accepts_returned_closure_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-forward-into-param-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ int f(int)=make(3); return apply(f, 4); }\n");
}

static int test_semantic_accepts_zero_arg_returned_closure_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-zero-arg-forward-into-param-pass",
        "int apply0(int f()){ return f(); }\n"
        "int make(int x)() { return closure [x] int () { return x; }; }\n"
        "int main(){ int f()=make(3); return apply0(f); }\n");
}

static int test_semantic_accepts_void_returned_closure_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-void-forward-into-param-pass",
        "void apply0(void f()){ f(); }\n"
        "void make(int x)() { return closure [x] void () { putint(x); return; }; }\n"
        "int main(){ void f()=make(7); apply0(f); return 0; }\n");
}

static int test_semantic_accepts_returned_closure_alias_forwarding_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-alias-forward-into-param-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ int f(int)=make(3); int g(int)=f; return apply(g, 4); }\n");
}

static int test_semantic_accepts_closure_assignment_then_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-assignment-then-direct-call-pass",
        "int main(){ int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; g=f; return g(4); }\n");
}

static int test_semantic_accepts_closure_assignment_then_forward_into_function_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-assignment-then-forward-into-param-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int y=5; int x=3; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; g=f; return apply(g, 4); }\n");
}

static int test_semantic_accepts_returned_closure_assignment_then_direct_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-assignment-then-direct-call-pass",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int main(){ int f(int)=make(3); int g(int)=make(5); g=f; return g(4); }\n");
}

static int test_semantic_accepts_returned_closure_assignment_then_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-closure-assignment-then-return-pass",
        "int make(int x)(int) { return closure [x] int (int y) { return x + y; }; }\n"
        "int pick()(int){ int f(int)=make(3); int g(int)=make(5); g=f; return g; }\n"
        "int main(){ return pick()(4); }\n");
}

static int test_semantic_accepts_dynamic_closure_local_dispatch_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-closure-local-dispatch-pass",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return f(4); }\n");
}

static int test_semantic_accepts_ternary_closure_local_initializer_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "ternary-closure-local-initializer-pass",
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; int h(int)=c ? f : g; return h(4); }\n");
}

static int test_semantic_accepts_dynamic_closure_local_forward_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-closure-local-forward-into-param-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int c=1; int x=3; int y=5; int f(int)=closure [x] int (int z) { return x + z; }; int g(int)=closure [y] int (int z) { return y + z; }; if(c) f=g; return apply(f, 4); }\n");
}

static int test_semantic_accepts_multi_capture_closure_forward_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "multi-capture-closure-forward-into-param-pass",
        "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
        "int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return apply(f, 5, 6); }\n");
}

static int test_semantic_accepts_returned_multi_capture_local_closure_bind_and_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "returned-multi-capture-local-closure-bind-call-pass",
        "int pick(int x,int y)(int,int){ int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f; }\n"
        "int main(){ int h(int,int)=pick(3,4); return h(5,6); }\n");
}

static int test_semantic_accepts_dynamic_returned_multi_capture_local_closure_forward_into_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "dynamic-returned-multi-capture-local-closure-forward-pass",
        "int apply(int f(int,int), int a, int b){ return f(a,b); }\n"
        "int pick(int x, int y, int c)(int,int){ int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; int g(int,int)=closure [x,y] int (int a, int b) { return x + y - a - b; }; if(c) f=g; return f; }\n"
        "int main(){ return apply(pick(5, 7, 1), 3, 2); }\n");
}

static int test_semantic_accepts_local_function_value_alias_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-function-value-alias-chain-pass",
        "int add1(int x){ return x+1; }\n"
        "int main(){ int f(int)=add1; int g(int)=f; return g(41); }\n");
}

static int test_semantic_rejects_local_function_value_nonfunction_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-nonfunction-init-reject",
        "int main(){ int f(int)=0; return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_local_function_value_signature_mismatch_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-signature-mismatch-init-reject",
        "int next(){ return 9; }\nint main(){ int f(int)=next; return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_local_function_value_alias_chain_parameter_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-alias-chain-param-kind-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int main(){ int f(float)=addf; int g(int)=f; return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_local_function_value_alias_chain_expected_parameter_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-alias-chain-expected-param-kind-mismatch-reject",
        "int addi(int x){ return x+1; }\n"
        "int main(){ int f(int)=addi; int g(float)=f; return 0; }\n",
        "SEMA-EXT-018");
}

static int test_semantic_rejects_local_function_value_assignment_parameter_kind_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "local-function-value-assignment-param-kind-mismatch-reject",
        "int addf(float x){ return 1; }\n"
        "int addi(int x){ return x+1; }\n"
        "int main(){ int f(float)=addf; int g(int)=addi; g=f; return 0; }\n",
        "SEMA-EXT-019");
}

static int test_semantic_accepts_local_function_value_reassignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-function-value-reassignment-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int f(int)=add1; f=add2; return f(40); }\n");
}

static int test_semantic_accepts_dynamic_local_function_value_dispatch_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "local-function-value-dynamic-dispatch-pass",
        "int add1(int x){ return x+1; }\n"
        "int add2(int x){ return x+2; }\n"
        "int main(){ int c=0; int f(int)=add1; if((putint(0), c)) f=add2; return f(40); }\n");
}

static int test_semantic_rejects_closure_literal_outside_extension_mode(void) {
    const CallableDiagCase c = {
        "closure-literal-non-extension",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return 0; }\n",
        "SEMA-EXT-039",
        "closures are available only under -extension mode",
        1,
        33,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_accepts_closure_literal_function_local_initializer_shape_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-literal-ext-phase1-shape-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; return f(4); }\n");
}

static int test_semantic_rejects_closure_literal_non_int_capture_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-literal-non-int-capture-ext-reject",
        "int main(){ float x=1.25; int f(int)=closure [x] int (int y) { return y; }; return 0; }\n",
        "SEMA-EXT-043");
}

static int test_semantic_rejects_closure_literal_nested_local_capture_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-literal-nested-local-capture-ext-reject",
        "int main(){ if(1){ int x=3; int f(int)=closure [x] int (int y) { return x + y; }; } return 0; }\n",
        "SEMA-EXT-041");
}

static int test_semantic_rejects_closure_literal_signature_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-literal-signature-mismatch-ext-reject",
        "int main(){ int x=3; int f()=closure [x] int (int y) { return x + y; }; return 0; }\n",
        "SEMA-EXT-045");
}

static int test_semantic_rejects_closure_literal_body_shape_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-literal-body-shape-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z=y; if(z) putint(z); return z; }; return 0; }\n",
        "SEMA-EXT-041");
}

static int test_semantic_rejects_closure_literal_uncaptured_identifier_in_body_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-literal-uncaptured-body-name-ext-reject",
        "int g=7; int main(){ int x=3; int f(int)=closure [x] int (int y) { return x + y + g; }; return 0; }\n",
        "SEMA-EXT-044");
}

static int test_semantic_accepts_closure_literal_alias_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-literal-alias-chain-ext-pass",
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; return g(4); }\n");
}

static int test_semantic_accepts_closure_literal_alias_chain_two_hops_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-literal-alias-chain-two-hop-ext-pass",
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; int h(int)=g; return h(4); }\n");
}

static int test_semantic_accepts_zero_arg_closure_alias_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-zero-alias-chain-ext-pass",
        "int main(){ int x=3; int f()=closure [x] int () { return x; }; int g()=f; return g(); }\n");
}

static int test_semantic_accepts_void_closure_alias_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-void-alias-chain-ext-pass",
        "int main(){ int x=7; void f()=closure [x] void () { putint(x); return; }; void g()=f; g(); return 0; }\n");
}

static int test_semantic_accepts_closure_literal_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-forward-into-function-param-ext-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; return apply(g, 4); }\n");
}

static int test_semantic_accepts_zero_arg_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-zero-forward-into-function-param-ext-pass",
        "int apply0(int f()){ return f(); }\n"
        "int main(){ int y=3; int f()=closure [y] int () { return y; }; return apply0(f); }\n");
}

static int test_semantic_rejects_zero_arg_closure_forward_into_function_value_parameter_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-zero-forward-into-function-param-arity-mismatch-reject",
        "int apply0(int f()){ return f(1); }\n"
        "int main(){ int y=3; int f()=closure [y] int () { return y; }; return apply0(f); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_void_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-void-forward-into-function-param-ext-pass",
        "void apply0(void f()){ f(); }\n"
        "int main(){ int y=7; void f()=closure [y] void () { putint(y); return; }; apply0(f); return 0; }\n");
}

static int test_semantic_rejects_void_closure_forward_into_function_value_parameter_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-void-forward-into-function-param-arity-mismatch-reject",
        "void apply0(void f()){ f(1); }\n"
        "int main(){ int y=7; void f()=closure [y] void () { putint(y); return; }; apply0(f); return 0; }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_two_hop_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-two-hop-forward-into-function-param-ext-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; int g(int)=f; int h(int)=g; return apply(h, 4); }\n");
}

static int test_semantic_accepts_zero_arg_alias_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-zero-alias-forward-into-function-param-ext-pass",
        "int apply0(int f()){ return f(); }\n"
        "int main(){ int y=3; int f()=closure [y] int () { return y; }; int g()=f; return apply0(g); }\n");
}

static int test_semantic_accepts_void_alias_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-void-alias-forward-into-function-param-ext-pass",
        "void apply0(void f()){ f(); }\n"
        "int main(){ int y=7; void f()=closure [y] void () { putint(y); return; }; void g()=f; apply0(g); return 0; }\n");
}

static int test_semantic_accepts_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-direct-literal-arg-ext-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int y=3; return apply(closure [y] int (int z) { return y + z; }, 4); }\n");
}

static int test_semantic_rejects_direct_closure_literal_argument_to_function_value_parameter_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-direct-literal-arg-scalar-mismatch-reject",
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int y=3; return apply(closure [y] int (int z) { return y + z; }, 4); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_accepts_zero_arg_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-zero-direct-literal-arg-ext-pass",
        "int apply0(int f()){ return f(); }\n"
        "int main(){ int y=3; return apply0(closure [y] int () { return y; }); }\n");
}

static int test_semantic_rejects_zero_arg_direct_closure_literal_argument_to_function_value_parameter_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-zero-direct-literal-arg-arity-mismatch-reject",
        "int apply0(int f()){ return f(1); }\n"
        "int main(){ int y=3; return apply0(closure [y] int () { return y; }); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_void_direct_closure_literal_argument_to_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-void-direct-literal-arg-ext-pass",
        "void apply0(void f()){ f(); }\n"
        "int main(){ int y=7; apply0(closure [y] void () { putint(y); return; }); return 0; }\n");
}

static int test_semantic_rejects_void_direct_closure_literal_argument_to_function_value_parameter_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-void-direct-literal-arg-arity-mismatch-reject",
        "void apply0(void f()){ f(1); }\n"
        "int main(){ int y=7; apply0(closure [y] void () { putint(y); return; }); return 0; }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_comma_wrapped_closure_forward_into_function_value_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-comma-forward-into-function-param-ext-pass",
        "int apply(int f(int), int x){ return f(x); }\n"
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f, f), 4); }\n");
}

static int test_semantic_rejects_comma_wrapped_closure_forward_into_function_value_parameter_scalar_type_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-comma-forward-into-function-param-scalar-mismatch-reject",
        "int apply(int f(int), int x){ return f(1.25); }\n"
        "int main(){ int y=3; int f(int)=closure [y] int (int z) { return y + z; }; return apply((f, f), 4); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_assignment_of_closure_literal_after_function_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-assignment-after-local-decl-ext-reject",
        "int main(){ int y=3; int f(int); f = closure [y] int (int z) { return y + z; }; return 0; }\n",
        "SEMA-EXT-040");
}

static int test_semantic_accepts_zero_arg_closure_direct_local_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-zero-arg-direct-call-ext-pass",
        "int main(){ int x=3; int f()=closure [x] int () { return x; }; return f(); }\n");
}

static int test_semantic_rejects_zero_arg_closure_direct_local_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-zero-arg-direct-call-arity-mismatch-reject",
        "int main(){ int x=3; int f()=closure [x] int () { return x; }; return f(1); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_multi_capture_closure_direct_local_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-multi-capture-direct-call-ext-pass",
        "int main(){ int x=3; int y=4; int f()=closure [x,y] int () { return x + y; }; return f(); }\n");
}

static int test_semantic_accepts_multi_capture_two_arg_closure_direct_local_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-multi-capture-two-arg-direct-call-ext-pass",
        "int main(){ int x=3; int y=4; int f(int,int)=closure [x,y] int (int a, int b) { return x + y + a + b; }; return f(5,6); }\n");
}

static int test_semantic_accepts_void_closure_direct_local_call_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-void-direct-call-ext-pass",
        "int main(){ int x=7; void f()=closure [x] void () { putint(x); return; }; f(); return 0; }\n");
}

static int test_semantic_accepts_int_closure_expression_statement_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-prestmt-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { putint(x); return x + y; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_control_flow_prefix_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-control-prefix-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { if(y) putint(x); return x + y; }; return f(4); }\n",
        "SEMA-EXT-041");
}

static int test_semantic_accepts_int_closure_local_decl_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-local-decl-prefix-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = x + y; return z; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_local_decl_shadow_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-local-decl-shadow-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int x = y; return x; }; return f(4); }\n",
        "SEMA-EXT-046");
}

static int test_semantic_rejects_int_closure_non_int_local_decl_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-local-decl-float-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { float z = 1.25; return y; }; return f(4); }\n",
        "SEMA-EXT-041");
}

static int test_semantic_accepts_int_closure_parameter_assignment_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-param-assign-prefix-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { y = y + 1; return x + y; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_local_assignment_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-local-assign-prefix-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z = z + 1; return x + z; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_capture_assignment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-capture-assign-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { x = y; return x; }; return f(4); }\n",
        "SEMA-EXT-047");
}

static int test_semantic_accepts_int_closure_uninitialized_local_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-uninit-local-prefix-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z; z = y + 1; return x + z; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_multi_declarator_local_prefix_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-multi-decl-prefix-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int a, b; a = y; b = a + 1; return x + b; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_local_decl_shadow_parameter_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-local-decl-shadow-param-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int y; y = 1; return x + y; }; return f(4); }\n",
        "SEMA-EXT-046");
}

static int test_semantic_accepts_int_closure_parameter_compound_assignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-param-plus-assign-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { y += 1; return x + y; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_local_compound_assignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-local-plus-assign-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z += 1; return x + z; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_parameter_prefix_increment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-param-preinc-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { ++y; return x + y; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_local_postfix_increment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-local-postinc-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { int z = y; z++; return x + z; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_capture_prefix_increment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-capture-preinc-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { ++x; return x + y; }; return f(4); }\n",
        "SEMA-EXT-047");
}

static int test_semantic_accepts_int_closure_bitwise_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-bitwise-expr-ext-pass",
        "int main(){ int x=6; int f(int)=closure [x] int (int y) { return (x & y) ^ 1; }; return f(3); }\n");
}

static int test_semantic_accepts_int_closure_logical_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-logical-expr-ext-pass",
        "int main(){ int x=0; int f(int)=closure [x] int (int y) { return (x || y) && 1; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_shift_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-shift-expr-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return (x << 1) + (y >> 1); }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_bitwise_compound_assignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-bitwise-compound-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { y &= 7; return x + y; }; return f(12); }\n");
}

static int test_semantic_accepts_int_closure_bitwise_not_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-bitwise-not-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return ~x + y; }; return f(4); }\n");
}

static int test_semantic_rejects_int_closure_comma_expression_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "closure-int-comma-ext-reject",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return (x = y, x + y); }; return f(4); }\n",
        "SEMA-EXT-047");
}

static int test_semantic_accepts_int_closure_return_assignment_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-return-assign-expr-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return (y = y + 1); }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_return_prefix_increment_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-return-preinc-expr-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return ++y; }; return f(4); }\n");
}

static int test_semantic_accepts_int_closure_return_comma_expression_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "closure-int-return-comma-expr-ext-pass",
        "int main(){ int x=3; int f(int)=closure [x] int (int y) { return (y = y + 1, x + y); }; return f(4); }\n");
}

static int test_semantic_accepts_pair_copy_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-copy-ext-pass",
        "int main(){ pair a={1,2}; pair b=a; b=a; return b.first; }\n");
}

static int test_semantic_rejects_plain_pair_value_in_int_context_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-plain-value-int-context-ext-reject",
        "int main(){ pair a={1,2}; return a; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_rejects_pair_assignment_from_scalar_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-assign-scalar-ext-reject",
        "int main(){ pair a={1,2}; a = 1; return a.first; }\n",
        "SEMA-AGG-005");
}

static int test_semantic_rejects_pair_init_from_scalar_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-init-scalar-ext-reject",
        "int main(){ pair a=1; return 0; }\n",
        "SEMA-AGG-005");
}

static int test_semantic_rejects_pair_init_list_too_many_items_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-init-too-many-ext-reject",
        "int main(){ pair a={1,2,3}; return 0; }\n",
        "SEMA-AGG-002");
}

static int test_semantic_accepts_struct_copy_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-copy-ext-pass",
        "struct Point { int x; int y; int z; };\n"
        "int main(){ struct Point a={1,2,3}; struct Point b=a; b=a; return b.z; }\n");
}

static int test_semantic_rejects_unknown_struct_type_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-unknown-type-ext-reject",
        "int main(){ struct Point p={1,2}; return 0; }\n",
        "SEMA-STRUCT-004");
}

static int test_semantic_rejects_mismatched_struct_assignment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-mismatched-copy-ext-reject",
        "struct A { int x; int y; int z; };\n"
        "struct B { int x; int y; int z; };\n"
        "int main(){ struct A a={1,2,3}; struct B b={4,5,6}; a=b; return a.x; }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_struct_init_from_scalar_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-init-scalar-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p=1; return 0; }\n",
        "SEMA-AGG-005");
}

static int test_semantic_rejects_mismatched_struct_copy_init_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-mismatched-copy-init-ext-reject",
        "struct A { int x; int y; };\n"
        "struct B { int x; int y; };\n"
        "int main(){ struct B b={1,2}; struct A a=b; return a.x; }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_pair_struct_assignment_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-struct-assignment-mismatch-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ pair a={1,2}; struct Point b={3,4}; a=b; return a.first; }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_plain_struct_value_in_int_context_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-plain-value-int-context-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; return p; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_rejects_local_pair_as_scalar_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-local-scalar-callarg-ext-reject",
        "int sink(int x){ return x; }\n"
        "int main(){ pair a={1,2}; return sink(a); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_local_struct_as_scalar_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-local-scalar-callarg-ext-reject",
        "struct Point { int x; int y; };\n"
        "int sink(int x){ return x; }\n"
        "int main(){ struct Point p={1,2}; return sink(p); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_local_pair_scalar_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-local-scalar-init-ext-reject",
        "int main(){ pair a={1,2}; int x = a; return x; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_local_struct_scalar_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-local-scalar-init-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; int x = p; return x; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_local_pair_scalar_assignment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-local-scalar-assign-ext-reject",
        "int main(){ pair a={1,2}; int x = 0; x = a; return x; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_local_struct_scalar_assignment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-local-scalar-assign-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; int x = 0; x = p; return x; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_local_pair_control_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-local-control-ext-reject",
        "int main(){ pair a={1,2}; if(a) return 1; return 0; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_rejects_local_struct_control_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-local-control-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; if(p) return 1; return 0; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_accepts_pair_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-return-ext-pass",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_struct_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-return-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int main(){ return 0; }\n");
}

static int test_semantic_accepts_pair_call_result_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-call-copy-init-ext-pass",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ pair q = mk(); return q.second; }\n");
}

static int test_semantic_accepts_struct_call_result_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-call-copy-init-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int main(){ struct Point q = mk(); return q.y; }\n");
}

static int test_semantic_accepts_pair_call_result_assignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-call-assignment-ext-pass",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ pair q={7,8}; q = mk(); return q.second; }\n");
}

static int test_semantic_accepts_struct_call_result_assignment_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-call-assignment-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int main(){ struct Point q={7,8}; q = mk(); return q.y; }\n");
}

static int test_semantic_accepts_pair_parameter_to_return_forwarding_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-param-return-forward-ext-pass",
        "pair id(pair p){ return p; }\n"
        "int main(){ pair q={1,2}; pair r=id(q); return r.second; }\n");
}

static int test_semantic_accepts_struct_parameter_to_return_forwarding_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-param-return-forward-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Point id(struct Point p){ return p; }\n"
        "int main(){ struct Point q={1,2}; struct Point r=id(q); return r.y; }\n");
}

static int test_semantic_accepts_nested_struct_return_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-return-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "int main(){ struct Mid q=mk(); return q.p.second + q.z; }\n");
}

static int test_semantic_accepts_pair_call_result_as_aggregate_call_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-call-result-callarg-ext-pass",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int sum(pair p){ return p.first + p.second; }\n"
        "int main(){ return sum(mk()); }\n");
}

static int test_semantic_accepts_struct_call_result_as_aggregate_call_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-call-result-callarg-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int sum(struct Point p){ return p.x + p.y; }\n"
        "int main(){ return sum(mk()); }\n");
}

static int test_semantic_accepts_nested_struct_call_result_as_aggregate_call_argument_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-call-result-callarg-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "int sum(struct Mid m){ return m.p.first + m.p.second + m.z; }\n"
        "int main(){ return sum(mk()); }\n");
}

static int test_semantic_accepts_pair_multi_hop_return_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-multi-hop-return-chain-ext-pass",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair id(pair p){ return p; }\n"
        "pair wrap(){ return id(mk()); }\n"
        "int main(){ pair q=wrap(); return q.second; }\n");
}

static int test_semantic_accepts_nested_struct_multi_hop_return_chain_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-multi-hop-return-chain-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "struct Mid id(struct Mid m){ return m; }\n"
        "struct Mid wrap(){ return id(mk()); }\n"
        "int main(){ struct Mid q=wrap(); return q.p.second + q.z; }\n");
}

static int test_semantic_rejects_pair_call_result_as_scalar_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-call-result-scalar-callarg-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int sink(int x){ return x; }\n"
        "int main(){ return sink(mk()); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_pair_multi_hop_return_chain_as_scalar_call_argument_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-multi-hop-scalar-callarg-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair wrap(){ return mk(); }\n"
        "int sink(int x){ return x; }\n"
        "int main(){ return sink(wrap()); }\n",
        "SEMA-TYPE-003");
}

static int test_semantic_rejects_pair_call_result_scalar_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-call-result-scalar-init-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ int x = mk(); return x; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_pair_call_result_scalar_assignment_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-call-result-scalar-assign-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ int x = 0; x = mk(); return x; }\n",
        "SEMA-TYPE-006");
}

static int test_semantic_rejects_pair_multi_hop_return_chain_scalar_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-multi-hop-scalar-init-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair wrap(){ return mk(); }\n"
        "int main(){ int x = wrap(); return x; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_nested_struct_multi_hop_return_chain_scalar_initializer_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "nested-struct-multi-hop-scalar-init-ext-reject",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "struct Mid id(struct Mid m){ return m; }\n"
        "struct Mid wrap(){ return id(mk()); }\n"
        "int main(){ int x = wrap(); return x; }\n",
        "SEMA-TYPE-004");
}

static int test_semantic_rejects_pair_multi_hop_return_chain_control_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-multi-hop-control-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "pair wrap(){ return mk(); }\n"
        "int main(){ if(wrap()) return 1; return 0; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_rejects_nested_struct_multi_hop_return_chain_control_condition_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "nested-struct-multi-hop-control-ext-reject",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "struct Mid id(struct Mid m){ return m; }\n"
        "struct Mid wrap(){ return id(mk()); }\n"
        "int main(){ if(wrap()) return 1; return 0; }\n",
        "SEMA-AGG-004");
}

static int test_semantic_rejects_pair_call_result_direct_member_access_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-call-result-member-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ return mk().second; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_rejects_struct_call_result_direct_member_access_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-call-result-member-ext-reject",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int main(){ return mk().y; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_rejects_parenthesized_pair_call_result_direct_member_access_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-call-result-member-paren-ext-reject",
        "pair mk(){ pair p={1,2}; return p; }\n"
        "int main(){ return (mk()).second; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_rejects_parenthesized_struct_call_result_direct_member_access_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-call-result-member-paren-ext-reject",
        "struct Point { int x; int y; };\n"
        "struct Point mk(){ struct Point p={1,2}; return p; }\n"
        "int main(){ return ((mk())).y; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_rejects_nested_struct_call_result_direct_member_chain_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "nested-struct-call-result-member-chain-ext-reject",
        "struct Mid { pair p; int z; };\n"
        "struct Mid mk(){ struct Mid m={{1,2},3}; return m; }\n"
        "int main(){ return mk().p.second; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_rejects_unknown_struct_field_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-unknown-field-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p={1,2}; return p.z; }\n",
        "SEMA-AGG-003");
}

static int test_semantic_accepts_pair_and_struct_member_lookup_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-struct-member-lookup-ext-pass",
        "struct Point { int x; int y; };\n"
        "int main(){ pair a={1,2}; struct Point p={3,4}; return a.first + p.y; }\n");
}

static int test_semantic_accepts_nested_pair_field_lookup_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-pair-field-lookup-ext-pass",
        "struct Outer { pair p; int z; };\n"
        "int main(){ struct Outer o={{1,2},3}; return o.p.first + o.p.second + o.z; }\n");
}

static int test_semantic_accepts_nested_struct_field_lookup_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-field-lookup-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Outer { struct Point p; int z; };\n"
        "int main(){ struct Outer o={{1,2},3}; return o.p.x + o.p.y + o.z; }\n");
}

static int test_semantic_accepts_nested_pair_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-pair-copy-init-ext-pass",
        "struct Outer { pair p; int z; };\n"
        "int main(){ struct Outer o={{1,2},3}; pair q=o.p; return q.first + q.second; }\n");
}

static int test_semantic_accepts_nested_struct_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-copy-init-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Outer { struct Point p; int z; };\n"
        "int main(){ struct Outer o={{1,2},3}; struct Point q=o.p; return q.x + q.y; }\n");
}

static int test_semantic_accepts_nested_pair_assignment_from_member_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-pair-assign-member-ext-pass",
        "struct Outer { pair p; int z; };\n"
        "int main(){ pair q={4,5}; struct Outer o={{1,2},3}; q=o.p; return q.first + q.second; }\n");
}

static int test_semantic_accepts_nested_struct_assignment_from_member_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-assign-member-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Outer { struct Point p; int z; };\n"
        "int main(){ struct Point q={4,5}; struct Outer o={{1,2},3}; q=o.p; return q.x + q.y; }\n");
}

static int test_semantic_accepts_deep_nested_struct_field_lookup_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-struct-field-lookup-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Mid { struct Point p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2},3},4}; return o.m.p.x + o.m.p.y + o.m.z + o.w; }\n");
}

static int test_semantic_accepts_deep_nested_struct_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-struct-copy-init-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Mid { struct Point p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2},3},4}; struct Mid m=o.m; return m.p.x + m.p.y + m.z; }\n");
}

static int test_semantic_accepts_deep_nested_struct_assignment_from_member_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-struct-assign-member-ext-pass",
        "struct Point { int x; int y; };\n"
        "struct Mid { struct Point p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Mid m={{7,8},9}; struct Outer o={{{1,2},3},4}; m=o.m; return m.p.x + m.p.y + m.z; }\n");
}

static int test_semantic_accepts_deep_nested_pair_field_lookup_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-pair-field-lookup-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2},3},4}; return o.m.p.first + o.m.p.second + o.m.z + o.w; }\n");
}

static int test_semantic_accepts_deep_nested_pair_copy_init_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-pair-copy-init-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2},3},4}; struct Mid m=o.m; return m.p.first + m.p.second + m.z; }\n");
}

static int test_semantic_accepts_deep_nested_pair_assignment_from_member_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "deep-nested-pair-assign-member-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Mid m={{7,8},9}; struct Outer o={{{1,2},3},4}; m=o.m; return m.p.first + m.p.second + m.z; }\n");
}

static int test_semantic_accepts_pair_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "pair-param-ext-pass",
        "int sum(pair p){ return p.first + p.second; }\n"
        "int main(){ pair p={1,2}; return sum(p); }\n");
}

static int test_semantic_accepts_struct_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "struct-param-ext-pass",
        "struct Point { int x; int y; };\n"
        "int sum(struct Point p){ return p.x + p.y; }\n"
        "int main(){ struct Point p={3,4}; return sum(p); }\n");
}

static int test_semantic_accepts_nested_struct_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "nested-struct-param-ext-pass",
        "struct Mid { pair p; int z; };\n"
        "int sum(struct Mid m){ return m.p.first + m.p.second + m.z; }\n"
        "int main(){ struct Mid m={{1,2},3}; return sum(m); }\n");
}

static int test_semantic_rejects_deep_nested_struct_assignment_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "deep-nested-struct-assign-mismatch-ext-reject",
        "struct P { int x; int y; };\n"
        "struct Q { int x; int y; };\n"
        "struct Outer { struct P p; int z; };\n"
        "int main(){ struct Q q={4,5}; struct Outer o={{{1,2},3}}; q=o.p; return q.x; }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_deep_nested_pair_struct_assignment_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "deep-nested-pair-struct-assign-mismatch-ext-reject",
        "struct A { pair p; int z; };\n"
        "struct B { pair p; int z; };\n"
        "struct Outer { struct A a; int w; };\n"
        "int main(){ struct B b={{7,8},9}; struct Outer o={{{1,2},3},4}; b=o.a; return b.z; }\n",
        "SEMA-AGG-006");
}

static int test_semantic_rejects_deep_nested_pair_initializer_too_many_items_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "deep-nested-pair-init-too-many-ext-reject",
        "struct Mid { pair p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2,3},4},5}; return 0; }\n",
        "SEMA-AGG-002");
}

static int test_semantic_rejects_deep_nested_struct_initializer_too_many_items_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "deep-nested-struct-init-too-many-ext-reject",
        "struct Point { int x; int y; };\n"
        "struct Mid { struct Point p; int z; };\n"
        "struct Outer { struct Mid m; int w; };\n"
        "int main(){ struct Outer o={{{1,2,3},4},5}; return 0; }\n",
        "SEMA-AGG-002");
}

static int test_semantic_accepts_float_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "float-local-decl-ext-pass",
        "int main(){ float x; return 0; }\n");
}

static int test_semantic_rejects_pair_array_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "pair-array-local-decl-ext-reject",
        "int main(){ pair p[2]; return 0; }\n",
        "SEMA-AGG-001");
}

static int test_semantic_rejects_struct_array_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "struct-array-local-decl-ext-reject",
        "struct Point { int x; int y; };\n"
        "int main(){ struct Point p[2]; return 0; }\n",
        "SEMA-STRUCT-005");
}

static int test_semantic_rejects_float_array_local_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-array-local-decl-ext-reject",
        "int main(){ float a[2]; return 0; }\n",
        "SEMA-EXT-037");
}

static int test_semantic_rejects_float_array_global_declaration_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-array-global-decl-ext-reject",
        "float a[2];\nint main(){ return 0; }\n",
        "SEMA-EXT-037");
}

static int test_semantic_rejects_float_array_parameter_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "float-array-param-ext-reject",
        "float f(float a[]){ return 0; }\nint main(){ return 0; }\n",
        "SEMA-EXT-037");
}

static int test_semantic_accepts_forwarding_function_valued_parameter_under_extension_for_now(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-forward-semantic-pass",
        "int add1(int x){ return x+1; }\n"
        "int apply(int f(int), int x){ return f(x); }\n"
        "int wrapper(int f(int), int x){ return apply(f, x); }\n"
        "int main(){ return wrapper(add1, 41); }\n");
}

static int test_semantic_accepts_multiple_function_valued_parameters_under_extension_for_now(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-compose-semantic-pass",
        "int add1(int x){ return x+1; }\n"
        "int double1(int x){ return x*2; }\n"
        "int compose(int f(int), int g(int), int x){ return f(g(x)); }\n"
        "int main(){ return compose(add1, double1, 20); }\n");
}

static int test_semantic_accepts_void_function_valued_parameter_with_builtin_binding_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-void-builtin-pass",
        "void apply(void f(int), int x){ f(x); }\n"
        "int main(){ apply(putint, 7); return 0; }\n");
}

static int test_semantic_accepts_zero_arg_function_valued_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-zero-arg-pass",
        "int next(){ return 9; }\n"
        "int apply0(int f()){ return f(); }\n"
        "int main(){ return apply0(next); }\n");
}

static int test_semantic_rejects_zero_arg_function_valued_parameter_direct_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-zero-arg-direct-call-arity-mismatch-reject",
        "int apply0(int f()){ return f(1); }\n"
        "int next(){ return 9; }\n"
        "int main(){ return apply0(next); }\n",
        "SEMA-CALL-004");
}

static int test_semantic_accepts_zero_arg_void_function_valued_parameter_under_extension(void) {
    return run_extension_semantic_expect_accepts(
        "function-valued-param-zero-arg-void-pass",
        "void ping(){ putint(1); }\n"
        "void apply0(void f()){ f(); }\n"
        "int main(){ apply0(ping); return 0; }\n");
}

static int test_semantic_rejects_zero_arg_void_function_valued_parameter_direct_call_arity_mismatch_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-zero-arg-void-direct-call-arity-mismatch-reject",
        "void apply0(void f()){ f(1); }\n"
        "void ping(){ return; }\n"
        "int main(){ apply0(ping); return 0; }\n",
        "SEMA-CALL-004");
}

static int test_semantic_rejects_nonvoid_function_for_void_function_valued_parameter_under_extension(void) {
    return run_extension_semantic_expect_rejects(
        "function-valued-param-void-mismatch-reject",
        "int add1(int x){ return x+1; }\n"
        "void apply(void f(int), int x){ f(x); }\n"
        "int main(){ apply(add1, 7); return 0; }\n",
        "SEMA-EXT-018");
}

/* must_pass_or_fail_now: current behavior matches strict all-path expectation. */
static const CfCase kCfMustPassOrFailNow[] = {
    {"CF-01", "int f(){while(1){break;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-04", "int f(int a){while(1){if(a){break;}else{break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-05", "int f(){while(1){while(1){break;}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-07", "int f(int a){for(;;){if(a){break;}else{break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-08", "int f(int a){while(a){if(a){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-09", "int f(){while(1){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-10", "int f(){for(;;){int x=1;}return 0;}\n", CF_EXPECT_REJECT},
    {"CF-11", "int f(){while(1){while(1){break;}break;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-73", "int f(int a){while(a){return 1;}}\n", CF_EXPECT_REJECT},
    {"CF-74", "int f(int a){for(;a;){return 1;}}\n", CF_EXPECT_REJECT},
    {"CF-76", "int f(int a){for(;a;){if(1){return 1;} else {return 2;}}}\n", CF_EXPECT_REJECT},
    {"CF-77", "int f(int a){while(a){{return 1;}}}\n", CF_EXPECT_REJECT},
    {"CF-78", "int f(int a){while(a){return 1;} return 0;}\n", CF_EXPECT_ACCEPT},
    {"CF-79", "int f(int a){for(;a;){return 1;} return 0;}\n", CF_EXPECT_ACCEPT},
    {"CF-80", "int f(){while(1){break;return 1;}}\n", CF_EXPECT_REJECT},
    {"CF-81", "int f(){for(;;){break;return 1;}}\n", CF_EXPECT_REJECT},
    {"CF-82",
    "int f(int a){while(1){if(a){return 1;}for(;a<2;a=a+1){if(a){return 2;}}a=1;}}\n",
    CF_EXPECT_ACCEPT},
    {"CF-83",
    "int f(int a){for(;1;a=1){if(a){return 1;}while(a<2){if(a){return 2;}}}}\n",
    CF_EXPECT_ACCEPT},
    {"CF-84",
    "int f(int a){for(;1;a=1){if(a){return 1;}for(;a<2;a=a+1){if(a){return 2;}}}}\n",
    CF_EXPECT_ACCEPT},
    {"CF-85",
    "int f(int a){while(1){if(a){return 1;}a=1;for(;a<2;a=a+1){if(a){return 2;}}}}\n",
    CF_EXPECT_ACCEPT},
};

/* conservative_loop_acceptance: body/step updates keep loop exits plausible. */
static const CfCase kCfConservativeLoopAcceptance[] = {
    {"CF-02", "int f(int a){while(1){a=a-1; if(a==0){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-03", "int g; int tick(){g=g-1; return g;} int f(){while(1){tick(); if(g==0){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-06", "int f(int a){for(;;a=a-1){if(a==0){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
};

/* deterministic_loop_rejects: only reliably dead loop shapes stay rejected. */
static const CfCase kCfDeterministicLoopRejects[] = {
    {"CF-12", "int f(int a){while(1){while(1){if(a){break;}else{continue;}}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-13", "int f(int a){for(;;){if(a){break;}else{while(1){break;}break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-14", "int f(int a){for(;;){if(a){break;}else{continue;}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-15", "int f(int a){while(1){if(a){break;}else{while(1){}}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-16", "int f(int a){while(1){if(a){break;}else{return 2;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-17", "int f(int a){for(;;){if(a){break;}else{for(;;){}}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-18", "int f(int a){for(;;){if(a){break;}else{return 2;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-19", "int f(){while((1)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-20", "int f(){while(+1){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-21", "int f(){while((0,1)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-22", "int f(){for(;(1);){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-23", "int f(){for(;(0,1);){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-24", "int f(){while(+0){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-25", "int f(){for(;(0);){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-26", "int f(){while((1||0)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-27", "int f(){for(;(1<2);){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-28", "int f(){while((1?1:0)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-29", "int f(){while((0||0)){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-30", "int f(){for(;(1<0);){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-31", "int f(){while((0?1:0)){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-32", "int f(){while((1+2)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-33", "int f(){for(;(4-4);){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-34", "int f(){while((2*3)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-35", "int f(){for(;(8/4);){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-36", "int f(){while((9%3)){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-37", "int f(){while((1<<3)){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-38", "int f(){for(;(8>>3);){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-39", "int f(){while((1/0)){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-40", "int f(){for(;(1<<63);){continue;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-41",
    "int f(){while((1?1:0)){for(;(1||0);){continue;}break;}return 1;}\n",
    CF_EXPECT_REJECT},
    {"CF-42",
    "int f(){for(;(1?1:0);){if((1?1:0)){break;}else{break;}}return 1;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-43",
    "int f(){for(;(1?1:0);){if((1?1:0)){continue;}else{break;}}return 1;}\n",
    CF_EXPECT_REJECT},
    {"CF-44",
    "int f(){while((1?1:0)){for(;(0?1:0);){continue;}break;}return 1;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-45",
    "int f(){for(;(1?1:0);){while((1||0)){if(1){continue;}else{break;}}break;}return 1;}\n",
    CF_EXPECT_REJECT},
    {"CF-46",
    "int f(){for(;(1?1:0);){while((1&&0)){continue;}break;}return 1;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-47",
    "int f(){while((1?1:0)){for(;(1<<2);){if(1){continue;}else{break;}}}return 1;}\n",
    CF_EXPECT_REJECT},
    {"CF-48",
    "int f(){while((1?1:0)){for(;(1<<2);){break;}break;}return 1;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-49",
    "int f(){while(-(~9223372036854775807)){continue;}return 1;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-50",
    "int f(){while((~9223372036854775807)%(-1)){return 1;}return 0;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-51", "int f(){if(1) return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-52", "int f(){if((1)) return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-53", "int f(){if(+1) return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-54", "int f(){if((0,1)) return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-55", "int f(){if(0) ; else return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-56",
    "int f(){while(1){if((0,1)) break;}return 2;}\n",
    CF_EXPECT_ACCEPT},
    {"CF-57", "int f(int a){while(a){} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-58", "int f(int a){while(a){continue;} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-59", "int f(int a){for(;a;){continue;} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-60", "int f(int a){while(a){break;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-61", "int f(int a){while(a){while(1){continue;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-62", "int f(int a){for(;a;){for(;;){continue;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-84", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int x=1; while(a[0]){ a[1]--; } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-88", "int f(){ int x=1; int y=1; while(x){ y--; } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-89", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int t=7; while(a[0]){ t--; } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-90", "int f(){ int x=1; while(x){ int t=7; t--; } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-91", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ continue; } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-92", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ for(;a[0];a[1]--){ } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-93", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int i=0; for(;a[0];a[i]--){ i=0; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-94", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int i=1; for(;a[0];a[i]--){ } return 1; }\n", CF_EXPECT_REJECT},
    {"CF-63", "int f(int a){for(;a;){if(a){break;}} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-64", "int f(int a){while(a){int a=0; if(a){break;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-65", "int f(int a){while(a){int a=0; a=1;} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-66", "int f(int a){for(;a;){int a=0; if(a){break;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-67", "int f(int a,int b){while(a){int a=b; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-68", "int f(int a,int b){while(a){a=b; if(a) break; b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-69", "int f(int a,int b){while(a){int c=b; int a=c; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-70", "int f(int a,int b){for(;a;){int c=b; int a=c; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-71", "int f(int a,int b){while(a){int c=b; int d=c; if(d){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-72", "int f(int a,int b){while(a){int a=b; int c=a; if(c){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-73", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ a[0]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-74", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ for(;a[0];){ a[0]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-75", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ --a[0]; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-76", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ a[0] = a[0] - 1; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-77", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ if(a[0]) break; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-78", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ putint(a[0]); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-79", "int g(int x); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ a[0] = g(a[0]); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-80", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ int i = 0; a[i]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-81", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ for(int i=0;i<8;i++){ putint(a[i]); } while(a[0]){ a[0]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-82", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int x=1; int t=7; while(a[0]){ a[t--]--; t=(t+8)%8; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-83", "int g(); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ g(); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-85", "int g(); int f(){ int x=1; while(x){ g(); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-86", "int g(int x); int f(){ int x=1; while(x){ x = g(x); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-87", "int f(){ int x=1; while(x){ int y=0; x=y; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-95", "int a[8]={1,2,3,4,5,6,7,8}; int f(){ int i=0; while(a[0]){ a[i]--; i=0; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-96", "int touch(int b[]); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ touch(a); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-97", "int touch(int x); int f(){ int x=1; while(x){ touch(x); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-98", "int next(int x); int f(){ int x=1; while(x){ x = next(x); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-99", "int next(); int f(){ int x=1; while(x){ next(); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-100", "int next(int x); int f(){ int x=1; while(x){ int y = next(x); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-101", "int touch(int b[]); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ int z = touch(a); } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-102", "int getidx(); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ a[getidx()]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-103", "int getidx(); int a[8]={1,2,3,4,5,6,7,8}; int f(){ while(a[0]){ int i = getidx(); a[i]--; } return 1; }\n", CF_EXPECT_ACCEPT},
    {"CF-104", "int getv(); int f(){ int x=1; while(x){ int y = getv(); } return 1; }\n", CF_EXPECT_ACCEPT},
};

static int run_must_pass_or_fail_now_group(void) {
    return run_cf_case_group("must_pass_or_fail_now",
        kCfMustPassOrFailNow,
        sizeof(kCfMustPassOrFailNow) / sizeof(kCfMustPassOrFailNow[0]));
}

static int run_conservative_loop_acceptance_group(void) {
    return run_cf_case_group("conservative_loop_acceptance",
        kCfConservativeLoopAcceptance,
        sizeof(kCfConservativeLoopAcceptance) /
            sizeof(kCfConservativeLoopAcceptance[0]));
}

static int run_deterministic_loop_rejects_group(void) {
    return run_cf_case_group("deterministic_loop_rejects",
        kCfDeterministicLoopRejects,
        sizeof(kCfDeterministicLoopRejects) /
            sizeof(kCfDeterministicLoopRejects[0]));
}

static int test_semantic_rejects_const_local_without_initializer(void) {
    const CallableDiagCase c = {
        "const-local-no-init",
        "int f(){ const int x; return 0; }\n",
        "SEMA-CONST-002",
        "const declaration requires initializer",
        1,
        10,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_const_top_level_without_initializer(void) {
    const CallableDiagCase c = {
        "const-top-no-init",
        "const int g;\nint main(){ return 0; }\n",
        "SEMA-CONST-002",
        "const declaration requires initializer",
        1,
        11,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_assignment_to_const(void) {
    const CallableDiagCase c = {
        "const-assign",
        "int f(){ const int x = 1; x = 2; return x; }\n",
        "SEMA-CONST-001",
        "assignment to const object is not allowed",
        1,
        29,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_assignment_to_const_parameter(void) {
    const CallableDiagCase c = {
        "const-param-assign",
        "int f(const int x){ x = 1; return x; }\n",
        "SEMA-CONST-001",
        "assignment to const object is not allowed",
        1,
        23,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_assignment_to_const_array_element(void) {
    const CallableDiagCase c = {
        "const-array-assign",
        "int f(){ const int a[2] = {1, 2}; a[0] = 3; return a[0]; }\n",
        "SEMA-CONST-001",
        "assignment to const object is not allowed",
        1,
        40,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_increment_of_const_array_element(void) {
    const CallableDiagCase c = {
        "const-array-post-inc",
        "int f(){ const int a[2] = {1, 2}; a[0]++; return a[0]; }\n",
        "SEMA-CONST-001",
        "assignment to const object is not allowed",
        1,
        39,
    };

    return run_callable_diag_case(&c);
}
