#include "semantic_regression_intellisense_prelude.inc"

static int test_semantic_callable_diagnostic_matrix(void) {
    static const CallableDiagCase cases[] = {
        {"CALL-001",
        "int main(){return foo(1);}\n",
        "SEMA-CALL-001",
        "callee=foo",
        1,
        22},
        {"CALL-002",
        "int main(){return foo(1);}\nint foo(int a){return a;}\n",
        "SEMA-CALL-002",
        "callee=foo; decl_line=2; decl_col=5",
        1,
        22},
        {"CALL-003",
        "int foo;\nint main(){return foo(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        2,
        22},
        {"CALL-003-SHADOW-LOCAL",
        "int foo(int a){return a;}\nint main(){int foo=1; return foo(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-LOCAL-NO-TOPLEVEL",
        "int main(){int foo=1; return foo(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-LOCAL-WITH-LATER-DECL",
        "int main(){int foo=1; return foo(1);}\nint foo(int a){return a;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-PARAM",
        "int foo(int a){return a;}\nint main(int foo){return foo(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-PARAM-NO-TOPLEVEL",
        "int main(int foo){return foo(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-INNER-BLOCK",
        "int foo(int a){return a;}\nint main(){{int foo=1; return foo(1);} return 0;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-FOR-INIT",
        "int foo(int a){return a;}\nint main(){for(int foo=0;foo<1;foo=foo+1){return foo(1);}return 0;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-PAREN-CALLEE",
        "int foo(int a){return a;}\nint main(){int foo=1; return (foo)(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-SHADOW-CALL-RESULT-CHAIN",
        "int main(){int foo=1; return foo()(1);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-ORDER-BEFORE-ARG-UNDECL",
        "int main(){int foo=1; return foo(bar);}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-DECL-INIT-SHADOW-ORDER",
        "int foo(int a){return a;}\nint main(){int foo=1; int x=foo(bar); return x;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-003-FOR-COND-SHADOW-ORDER",
        "int main(){for(int foo=1;foo(bar);foo=foo+1){return 0;} return 0;}\n",
        "SEMA-CALL-003",
        "callee=foo; symbol_kind=non_function",
        0,
        0},
        {"CALL-004",
        "int foo(int a);\nint main(){return foo(1,2);}\n",
        "SEMA-CALL-004",
        "callee=foo; expected=1; got=2",
        2,
        22},
        {"CALL-004-DECL-INIT-ORDER",
        "int foo(int a);\nint main(){int x=foo(1,bar); return 0;}\n",
        "SEMA-CALL-004",
        "callee=foo; expected=1; got=2",
        0,
        0},
        {"CALL-004-FOR-STEP-ORDER",
        "int foo(int a);\nint main(){int i=0; for(;i<1;foo(1,bar)){i=i+1;} return 0;}\n",
        "SEMA-CALL-004",
        "callee=foo; expected=1; got=2",
        0,
        0},
        {"CALL-005-A",
        "int f(){return 1;}\nint main(){return f()(1);}\n",
        "SEMA-CALL-005",
        "callee_kind=call_result",
        2,
        22},
        {"CALL-005-B",
        "int f(int a){return a;}\nint main(){return ((f)(1))(2);}\n",
        "SEMA-CALL-005",
        "callee_kind=call_result",
        2,
        27},
        {"CALL-005-ORDER-BEFORE-ARG-UNDECL",
        "int f(){return 1;}\nint main(){return f()(bar);}\n",
        "SEMA-CALL-005",
        "callee_kind=call_result",
        0,
        0},
        {"CALL-005-DECL-INIT-ORDER",
        "int f(){return 1;}\nint main(){int x=f()(bar); return 0;}\n",
        "SEMA-CALL-005",
        "callee_kind=call_result",
        0,
        0},
        {"CALL-005-FOR-COND-ORDER",
        "int f(){return 1;}\nint main(){for(;f()(bar);){return 0;} return 0;}\n",
        "SEMA-CALL-005",
        "callee_kind=call_result",
        0,
        0},
        {"CALL-006-A",
        "int f(){return 1;}\nint main(){return (f+1)();}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        2,
        24},
        {"CALL-006-B",
        "int f(){return 1;}\nint main(){return (f?f:f)();}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        2,
        26},
        {"CALL-006-C",
        "int f(){return 1;}\nint main(){return ((f+1))();}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        2,
        26},
        {"CALL-006-D",
        "int f(){return 1;}\nint main(){return (f&&f)();}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        2,
        25},
        {"CALL-006-SHADOW-NONIDENTIFIER-STABLE",
        "int main(){int foo=1; return (foo+1)();}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        0,
        0},
        {"CALL-006-ORDER-BEFORE-ARG-UNDECL",
        "int main(){return (1+1)(bar);}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        0,
        0},
        {"CALL-006-DECL-INIT-ORDER",
        "int main(){int x=(1+1)(bar); return 0;}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        0,
        0},
        {"CALL-006-FOR-STEP-ORDER",
        "int main(){int i=0; for(;i<1;(1+1)(bar)){i=i+1;} return 0;}\n",
        "SEMA-CALL-006",
        "callee_kind=non_identifier",
        0,
        0},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_callable_diag_case(&cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_semantic_callable_accept_matrix(void) {
    static const CallablePassCase cases[] = {
        {"CALL-PASS-001", "int foo(int a){return a;}\nint main(){return foo(1);}\n"},
        {"CALL-PASS-002", "int foo(int a);\nint main(){return foo(1);}\n"},
        {"CALL-PASS-003", "int foo(int a);\nint main(){return (foo)(1);}\n"},
        {"CALL-PASS-004", "int foo(int a);\nint main(){return ((foo))(1);}\n"},
        {"CALL-PASS-005", "int foo();\nint main(){return foo();}\n"},
        {"CALL-PASS-006", "int foo(int a);\nint main(){return foo(1);}\nint foo(int a){return a;}\n"},
        {"CALL-PASS-007",
        "int foo(int a){return a;}\nint main(){{int foo=1;}return foo(1);}\n"},
        {"CALL-PASS-008",
        "int foo(int a){return a;}\nint main(){int x=foo(1); return x;}\n"},
        {"CALL-PASS-009",
        "int foo(int a){return a;}\nint main(){for(int i=foo(1);i<2;i=foo(i)){} return 0;}\n"},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_callable_pass_case(&cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_semantic_accepts_valid_program(void) {
    const char *source = "int x;\nint y;\nint main(){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: valid program rejected at %d:%d: %s\n",
            sema_err.line,
            sema_err.column,
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_duplicate_function(void) {
    static const CallableDiagCase c = {
        "TOP-003 duplicate function definition",
        "int main(){return 0;}\nint main(){return 1;}\n",
        "SEMA-TOP-003",
        "identifier=main; duplicate function definition",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_allows_function_declaration_then_definition(void) {
    const char *source = "int main(int a);\nint main(int a){return a;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: declaration+definition should be accepted, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_duplicate_function_declarations(void) {
    const char *source = "int main(int a);\nint main(int a);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: duplicate function declarations should be accepted, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_function_declaration_count_mismatch(void) {
    static const CallableDiagCase c = {
        "TOP-002 declaration count mismatch",
        "int f(int a);\nint f(int a,int b);\n",
        "SEMA-TOP-002",
        "identifier=f; prior_params=1; current_params=2; conflicting function declarations: parameter count mismatch",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_declaration_definition_count_mismatch(void) {
    static const CallableDiagCase c = {
        "TOP-002 declaration definition mismatch",
        "int f(int a);\nint f(){return 0;}\n",
        "SEMA-TOP-002",
        "identifier=f; prior_params=1; current_params=0; conflicting function declarations: parameter count mismatch",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_function_variable_conflict(void) {
    static const CallableDiagCase c = {
        "TOP-004 function variable conflict",
        "int main;\nint main(){return 0;}\n",
        "SEMA-TOP-004",
        "identifier=main; function/variable name conflict at top level",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_variable_function_conflict(void) {
    static const CallableDiagCase c = {
        "TOP-004 variable function conflict",
        "int main(){return 0;}\nint main;\n",
        "SEMA-TOP-004",
        "identifier=main; function/variable name conflict at top level",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_allows_repeated_declaration(void) {
    const char *source = "int x;\nint x;\nint main(){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: repeated declarations should currently pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_duplicate_initialized_top_level_definition(void) {
    const char *source = "int x = 1;\nint x = 2;\nint main(){return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: duplicate initialized top-level definition should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-TOP-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-TOP-001 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message,
        "identifier=x; duplicate top-level variable definition") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-TOP-001 identifier payload, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_initialized_definition_with_redeclaration(void) {
    const char *source = "int x = 1;\nint x;\nint main(){return x;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: initialized definition + redeclaration should pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_allows_unnamed_external_contract(void) {
    AstProgram program;
    SemanticError sema_err;

    ast_program_init(&program);

    if (!ast_program_add_external(&program, AST_EXTERNAL_DECLARATION, NULL)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: failed to build unnamed external via AST contract\n");
        ast_program_free(&program);
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: unnamed external should be accepted, got: %s\n",
            sema_err.message);
        ast_program_free(&program);
        return 0;
    }

    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_null_program_contract(void) {
    SemanticError sema_err;

    if (semantic_analyze_program(NULL, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: NULL program contract should fail semantic analysis\n");
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-INT-013") == NULL ||
        strstr(sema_err.message, "semantic input program is NULL") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-INT-013 NULL-program diagnostic, got: %s\n",
            sema_err.message);
        return 0;
    }

    if (sema_err.line != 0 || sema_err.column != 0) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected NULL-program diagnostic at 0:0, got %d:%d\n",
            sema_err.line,
            sema_err.column);
        return 0;
    }

    return 1;
}

static int test_semantic_rejects_function_definition_without_full_return(void) {
    static const CallableDiagCase c = {
        "CF-001 missing full return",
        "int f(int a){if(a){return 1;}}\n",
        "SEMA-CF-001",
        "Function definition must return on all control-flow paths",
        1,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_accepts_function_definition_with_full_return(void) {
    const char *source = "int f(int a){if(a){return 1;}else{return 0;}}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: full-return function definition should pass, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_prioritizes_scope_diagnostic_over_return_flow(void) {
    static const CallableDiagCase cases[] = {
        {"ORDER-SCOPE-BEFORE-FLOW-IF",
        "int f(){if(foo){return 1;}}\n",
        "SEMA-SCOPE-002",
        "identifier=foo; use of undeclared identifier",
        1,
        12},
        {"ORDER-SCOPE-BEFORE-FLOW-WHILE",
        "int f(){while(foo){return 1;}}\n",
        "SEMA-SCOPE-002",
        "identifier=foo; use of undeclared identifier",
        1,
        15},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (!run_callable_diag_case(&cases[i])) {
            return 0;
        }
    }

    return 1;
}

static int test_semantic_prioritizes_depth_diagnostic_over_return_flow(void) {
    const size_t repeat_count = 5000;
    const size_t max_source_size = 80 + (repeat_count * 2);
    char *source = (char *)malloc(max_source_size);
    size_t offset = 0;
    size_t i;
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!source) {
        fprintf(stderr,
            "[semantic-reg] FAIL: out of memory while building depth-before-flow source\n");
        return 0;
    }

    offset += (size_t)snprintf(source + offset,
        max_source_size - offset,
        "int f(int a){if(a");
    for (i = 0; i < repeat_count; ++i) {
        if (offset + 2 >= max_source_size) {
            fprintf(stderr,
                "[semantic-reg] FAIL: depth-before-flow source buffer overflow\n");
            free(source);
            return 0;
        }
        source[offset++] = '+';
        source[offset++] = 'a';
    }
    if (offset + 14 >= max_source_size) {
        fprintf(stderr,
            "[semantic-reg] FAIL: depth-before-flow source terminator overflow\n");
        free(source);
        return 0;
    }
    offset += (size_t)snprintf(source + offset,
        max_source_size - offset,
        "){return 1;}}\n");

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        free(source);
        return 0;
    }
    free(source);

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: depth-before-flow ordering case should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-INT-012") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-INT-012 before flow diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "expression depth exceeds semantic analysis limit") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected depth-limit diagnostic snippet, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (sema_err.line != 1 || sema_err.column != 1822) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected depth-before-flow location at 1:1822, got %d:%d\n",
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

static int test_semantic_rejects_unreachable_return_after_while_true(void) {
    static const CallableDiagCase c = {
        "CF-001 while true unreachable return",
        "int f(){while(1){}return 1;}\n",
        "SEMA-CF-001",
        "Function definition must return on all control-flow paths",
        1,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_unreachable_return_after_for_true(void) {
    static const CallableDiagCase c = {
        "CF-001 for true unreachable return",
        "int f(){for(;;){}return 1;}\n",
        "SEMA-CF-001",
        "Function definition must return on all control-flow paths",
        1,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_rejects_call_guard_return_only_without_tail_return(void) {
    static const CallableDiagCase c = {
        "CF-75 call guard return only",
        "int g();\nint f(){while(g()){return 1;}}\n",
        "SEMA-CF-001",
        "Function definition must return on all control-flow paths",
        2,
        5,
    };

    return run_callable_diag_case(&c);
}

static int test_semantic_accepts_reachable_return_after_break_in_while_true(void) {
    const char *source = "int f(){while(1){break;}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: break should make trailing return reachable, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_call_to_undeclared_function(void) {
    const char *source = "int main(){return foo(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call to undeclared function should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-001 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_call_to_non_function_symbol(void) {
    const char *source = "int foo;\nint main(){return foo(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call to non-function symbol should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-003") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-003 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_reports_call_site_for_callable_diagnostic(void) {
    const char *source = "int main(){\n"
    "  int x=0;\n"
    "  return foo(1);\n"
    "}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call-site diagnostic test should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-001") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-001 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (sema_err.line != 3 || sema_err.column != 13) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected call-site at 3:13, got %d:%d\n",
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

static int test_semantic_rejects_call_argument_count_mismatch(void) {
    const char *source = "int foo(int a);\nint main(){return foo(1,2);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: call argument-count mismatch should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-004") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-CALL-004 diagnostic, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_call_argument_mismatch_long_name_not_truncated(void) {
    char long_name[241];
    char source[768];
    size_t i;
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    for (i = 0; i < sizeof(long_name) - 1; ++i) {
        long_name[i] = (char)('a' + (i % 26));
    }
    long_name[sizeof(long_name) - 1] = '\0';

    snprintf(source,
        sizeof(source),
        "int %s(int a);\nint main(){return %s(1,2);}\n",
        long_name,
        long_name);

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: long-name argument-count mismatch should fail semantic analysis\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-004") == NULL ||
        strstr(sema_err.message, "expected=1; got=2") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected non-truncated SEMA-CALL-004 payload, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_missing_function_body_during_ast_primary_phase(void) {
    const char *source = "int f(){return 1;}\n";
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
            "[semantic-reg] FAIL: setup expected one function definition with function_body\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    ast_statement_free(program.externals[0].function_body);
    program.externals[0].function_body = NULL;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: missing function_body should fail during AST-primary semantic phase\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-INT-005") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-INT-005 for missing function_body, got: %s\n",
            sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_chained_call_as_unsupported_indirect_call(void) {
    const char *cases[] = {
        "int f(){return 1;}\nint main(){return f()(1);}\n",
        "int a(){return 1;}\nint main(){return a()();}\n"
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
                "[semantic-reg] FAIL: chained call should be rejected as unsupported indirect call\n");
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        if (strstr(sema_err.message, "SEMA-CALL-005") == NULL) {
            fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-005 diagnostic, got: %s\n",
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

static int test_semantic_rejects_parenthesized_call_result_forms(void) {
    const char *cases[] = {
        "int f(){return 1;}\nint main(){return (f())();}\n",
        "int f(int a){return a;}\nint main(){return (f(1))(2);}\n",
        "int f(int a){return a;}\nint main(){return ((f(1)))(2);}\n",
        "int f(int a){return a;}\nint main(){return ((f)(1))(2);}\n",
        "int f(){return 1;}\nint main(){return ((f)())();}\n"
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
                "[semantic-reg] FAIL: parenthesized call-result form should be rejected\n");
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        if (strstr(sema_err.message, "SEMA-CALL-005") == NULL) {
            fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-005 diagnostic, got: %s\n",
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

static int test_semantic_rejects_parenthesized_non_identifier_callee_form(void) {
    const char *cases[] = {
        "int f(){return 1;}\nint main(){return (f+1)();}\n",
        "int f(){return 1;}\nint main(){return (f?f:f)();}\n"
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
                "[semantic-reg] FAIL: parenthesized non-identifier callee form should fail semantic analysis\n");
            lexer_free_tokens(&tokens);
            ast_program_free(&program);
            return 0;
        }

        if (strstr(sema_err.message, "SEMA-CALL-006") == NULL) {
            fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-006 diagnostic, got: %s\n",
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

static int test_semantic_rejects_metadata_only_definition_during_ast_primary_phase(void) {
    AstProgram program;
    SemanticError sema_err;
    Token main_tok;

    ast_program_init(&program);

    main_tok.type = TOKEN_IDENTIFIER;
    main_tok.lexeme = "main";
    main_tok.length = 4;
    main_tok.number_value = 0;
    main_tok.line = 1;
    main_tok.column = 1;

    if (!ast_program_add_external(&program, AST_EXTERNAL_FUNCTION, &main_tok)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: failed to build function external for metadata contract test\n");
        ast_program_free(&program);
        return 0;
    }

    program.externals[0].is_function_definition = 1;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
            "[semantic-reg] FAIL: metadata-only function definition should fail semantic analysis\n");
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-INT-005") == NULL) {
        fprintf(stderr,
            "[semantic-reg] FAIL: expected SEMA-INT-005 diagnostic, got: %s\n",
            sema_err.message);
        ast_program_free(&program);
        return 0;
    }

    ast_program_free(&program);
    return 1;
}

