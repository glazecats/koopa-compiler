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
    };
    static const CallablePassCase pass_cases[] = {
        {"TOPINIT-PASS-001", "int y = 1; int x = y; int main(){return x;}\n"},
        {"TOPINIT-PASS-002",
         "int foo(int a); int x = foo(1); int foo(int a){return a;} int main(){return x;}\n"},
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
    const char *source = "int main(){while(1){continue;}return 1;}\n";
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
                "[semantic-reg] FAIL: non-returning function should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected all-paths-return diagnostic from AST-primary flow, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
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
    const char *source = "int f(){return 1;}\nint main(){return f()(1);}\n";
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
 * - Unknown/value-dependent loop termination is accepted by default.
 * - Only reliably proven must-not-return loop paths are rejected.
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

/* must_pass_or_fail_now: current behavior matches strict all-path expectation. */
static const CfCase kCfMustPassOrFailNow[] = {
    {"CF-01", "int f(){while(1){break;}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-04", "int f(int a){while(1){if(a){break;}else{break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-05", "int f(){while(1){while(1){break;}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-07", "int f(int a){for(;;){if(a){break;}else{break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-08", "int f(int a){while(a){if(a){break;}}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-09", "int f(){while(1){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-10", "int f(){for(;;){int x=1;}return 0;}\n", CF_EXPECT_REJECT},
    {"CF-11", "int f(){while(1){while(1){break;}break;}return 1;}\n", CF_EXPECT_ACCEPT},
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
    {"CF-63", "int f(int a){for(;a;){if(a){break;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-64", "int f(int a){while(a){int a=0; if(a){break;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-65", "int f(int a){while(a){int a=0; a=1;} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-66", "int f(int a){for(;a;){int a=0; if(a){break;}} return 1;}\n", CF_EXPECT_REJECT},
    {"CF-67", "int f(int a,int b){while(a){int a=b; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-68", "int f(int a,int b){while(a){a=b; if(a) break; b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-69", "int f(int a,int b){while(a){int c=b; int a=c; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-70", "int f(int a,int b){for(;a;){int c=b; int a=c; if(a){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-71", "int f(int a,int b){while(a){int c=b; int d=c; if(d){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-72", "int f(int a,int b){while(a){int a=b; int c=a; if(c){break;} b--;} return 1;}\n", CF_EXPECT_ACCEPT},
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

