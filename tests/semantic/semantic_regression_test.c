#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_source_to_ast(const char *source,
                               TokenArray *tokens,
                               AstProgram *program,
                               ParserError *parse_err) {
    lexer_init_tokens(tokens);
    ast_program_init(program);

    if (!lexer_tokenize(source, tokens)) {
        fprintf(stderr, "[semantic-reg] FAIL: lexer failed for input\n");
        return 0;
    }

    if (!parser_parse_translation_unit_ast(tokens, program, parse_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: parser failed at %d:%d: %s\n",
                parse_err->line,
                parse_err->column,
                parse_err->message);
        lexer_free_tokens(tokens);
        ast_program_free(program);
        return 0;
    }

    return 1;
}

typedef struct {
    const char *case_id;
    const char *source;
    const char *expected_code;
    const char *expected_snippet;
    int expected_line;
    int expected_column;
} CallableDiagCase;

typedef struct {
    const char *case_id;
    const char *source;
} CallablePassCase;

static int run_callable_diag_case(const CallableDiagCase *c) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!c || !c->source || !c->expected_code) {
        return 0;
    }

    if (!parse_source_to_ast(c->source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: %s should fail semantic analysis\n",
                c->case_id);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, c->expected_code) == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: %s expected %s, got: %s\n",
                c->case_id,
                c->expected_code,
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (c->expected_snippet && strstr(sema_err.message, c->expected_snippet) == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: %s expected diagnostic snippet '%s', got: %s\n",
                c->case_id,
                c->expected_snippet,
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (c->expected_line > 0 && c->expected_column > 0 &&
        (sema_err.line != c->expected_line || sema_err.column != c->expected_column)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: %s expected location %d:%d, got %d:%d\n",
                c->case_id,
                c->expected_line,
                c->expected_column,
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

static int run_callable_pass_case(const CallablePassCase *c) {
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!c || !c->source) {
        return 0;
    }

    if (!parse_source_to_ast(c->source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (!semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: %s should pass semantic analysis, got: %s\n",
                c->case_id,
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_callable_diagnostic_matrix(void) {
    static const CallableDiagCase cases[] = {
        {"CALL-001",
         "int main(){return foo(1);}\n",
         "SEMA-CALL-001",
         "undeclared function 'foo'",
         1,
         22},
        {"CALL-002",
         "int main(){return foo(1);}\nint foo(int a){return a;}\n",
         "SEMA-CALL-002",
         "first declaration at 2:5",
         1,
         22},
        {"CALL-003",
         "int foo;\nint main(){return foo(1);}\n",
         "SEMA-CALL-003",
         "non-function symbol 'foo'",
         2,
         22},
        {"CALL-004",
         "int foo(int a);\nint main(){return foo(1,2);}\n",
         "SEMA-CALL-004",
         "expected 1, got 2",
         2,
         22},
        {"CALL-005-A",
         "int f(){return 1;}\nint main(){return f()(1);}\n",
         "SEMA-CALL-005",
         "call result is not callable",
         2,
         22},
        {"CALL-005-B",
         "int f(int a){return a;}\nint main(){return ((f)(1))(2);}\n",
         "SEMA-CALL-005",
         "call result is not callable",
         2,
         27},
        {"CALL-006-A",
         "int f(){return 1;}\nint main(){return (f+1)();}\n",
         "SEMA-CALL-006",
         "non-identifier callee",
         2,
         24},
        {"CALL-006-B",
         "int f(){return 1;}\nint main(){return (f?f:f)();}\n",
         "SEMA-CALL-006",
         "non-identifier callee",
         2,
         26},
        {"CALL-006-C",
         "int f(){return 1;}\nint main(){return ((f+1))();}\n",
         "SEMA-CALL-006",
         "non-identifier callee",
         2,
         26},
        {"CALL-006-D",
         "int f(){return 1;}\nint main(){return (f&&f)();}\n",
         "SEMA-CALL-006",
         "non-identifier callee",
         2,
         25},
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
    const char *source = "int main(){return 0;}\nint main(){return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr, "[semantic-reg] FAIL: duplicate function definitions should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "Duplicate function") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected duplicate-function diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
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
    const char *source = "int f(int a);\nint f(int a,int b);\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: mismatched declaration parameter counts should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "parameter count mismatch") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected parameter-count mismatch diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_declaration_definition_count_mismatch(void) {
    const char *source = "int f(int a);\nint f(){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: declaration/definition count mismatch should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "parameter count mismatch") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected parameter-count mismatch diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_function_variable_conflict(void) {
    const char *source = "int main;\nint main(){return 0;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr, "[semantic-reg] FAIL: function/variable conflict should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "conflict") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected conflict diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
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

static int test_semantic_rejects_function_definition_without_full_return(void) {
    const char *source = "int f(int a){if(a){return 1;}}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: function definition without full return should fail\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected all-paths-return diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
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

static int test_semantic_rejects_unreachable_return_after_while_true(void) {
    const char *source = "int f(){while(1){}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: while(1) with unreachable return should fail all-path rule\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected all-paths-return diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
}

static int test_semantic_rejects_unreachable_return_after_for_true(void) {
    const char *source = "int f(){for(;;){}return 1;}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
        return 0;
    }

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: for(;;) with unreachable return should fail all-path rule\n");
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected all-paths-return diagnostic, got: %s\n",
                sema_err.message);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    lexer_free_tokens(&tokens);
    ast_program_free(&program);
    return 1;
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

static int test_semantic_rejects_chained_call_as_unsupported_indirect_call(void) {
    const char *source = "int f(){return 1;}\nint main(){return f()(1);}\n";
    TokenArray tokens;
    AstProgram program;
    ParserError parse_err;
    SemanticError sema_err;

    if (!parse_source_to_ast(source, &tokens, &program, &parse_err)) {
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

static int test_semantic_rejects_non_identifier_callee_metadata_contract(void) {
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
    program.externals[0].returns_on_all_paths = 1;
    program.externals[0].called_function_count = 1;
    program.externals[0].called_function_names = (char **)calloc(1, sizeof(char *));
    program.externals[0].called_function_lines = (int *)calloc(1, sizeof(int));
    program.externals[0].called_function_columns = (int *)calloc(1, sizeof(int));
    program.externals[0].called_function_arg_counts = (size_t *)calloc(1, sizeof(size_t));
    program.externals[0].called_function_kinds = (int *)calloc(1, sizeof(int));
    if (!program.externals[0].called_function_names ||
        !program.externals[0].called_function_lines ||
        !program.externals[0].called_function_columns ||
        !program.externals[0].called_function_arg_counts ||
        !program.externals[0].called_function_kinds) {
        fprintf(stderr,
                "[semantic-reg] FAIL: out of memory while building metadata contract test\n");
        ast_program_free(&program);
        return 0;
    }

    program.externals[0].called_function_lines[0] = 1;
    program.externals[0].called_function_columns[0] = 12;
    program.externals[0].called_function_arg_counts[0] = 0;
    program.externals[0].called_function_kinds[0] = AST_CALL_CALLEE_NON_IDENTIFIER;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: non-identifier callee metadata should fail semantic analysis\n");
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-006") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-006 diagnostic, got: %s\n",
                sema_err.message);
        ast_program_free(&program);
        return 0;
    }

    ast_program_free(&program);
    return 1;
}

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

    if (strstr(sema_err.message, "first declaration at 2:5") == NULL) {
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
 *    - Current expectation is aligned with stricter all-path semantics.
 * 2) known_limitation_lock
 *    - Cases are intentionally accepted today.
 *    - They are guardrails against accidental behavior drift until Milestone D.
 *
 * Strict-semantics migration note (Milestone D):
 * - Move CF-02/CF-03/CF-06 from known_limitation_lock to reject expectations.
 * - Keep CF-01/04/05/07/08/09/10 as-is unless semantics policy changes again.
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
    {"CF-08", "int f(int a){while(a){if(a){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-09", "int f(){while(1){continue;}return 1;}\n", CF_EXPECT_REJECT},
    {"CF-10", "int f(){for(;;){int x=1;}return 0;}\n", CF_EXPECT_REJECT},
};

/* known_limitation_lock: currently accepted; strict all-path semantics would reject. */
static const CfCase kCfKnownLimitationLock[] = {
    {"CF-02", "int f(int a){while(1){if(a){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-03", "int f(int a){while(1){if(a){break;}else{continue;}}return 1;}\n", CF_EXPECT_ACCEPT},
    {"CF-06", "int f(int a){for(;;){if(a){break;}}return 1;}\n", CF_EXPECT_ACCEPT},
};

static int run_must_pass_or_fail_now_group(void) {
    return run_cf_case_group("must_pass_or_fail_now",
                             kCfMustPassOrFailNow,
                             sizeof(kCfMustPassOrFailNow) / sizeof(kCfMustPassOrFailNow[0]));
}

static int run_known_limitation_lock_group(void) {
    return run_cf_case_group("known_limitation_lock",
                             kCfKnownLimitationLock,
                             sizeof(kCfKnownLimitationLock) / sizeof(kCfKnownLimitationLock[0]));
}

int main(void) {
    if (!test_semantic_accepts_valid_program()) {
        return 1;
    }
    if (!test_semantic_rejects_duplicate_function()) {
        return 1;
    }
    if (!test_semantic_allows_function_declaration_then_definition()) {
        return 1;
    }
    if (!test_semantic_allows_duplicate_function_declarations()) {
        return 1;
    }
    if (!test_semantic_rejects_function_declaration_count_mismatch()) {
        return 1;
    }
    if (!test_semantic_rejects_declaration_definition_count_mismatch()) {
        return 1;
    }
    if (!test_semantic_rejects_function_variable_conflict()) {
        return 1;
    }
    if (!test_semantic_allows_repeated_declaration()) {
        return 1;
    }
    if (!test_semantic_allows_unnamed_external_contract()) {
        return 1;
    }
    if (!test_semantic_rejects_function_definition_without_full_return()) {
        return 1;
    }
    if (!test_semantic_accepts_function_definition_with_full_return()) {
        return 1;
    }
    if (!test_semantic_rejects_unreachable_return_after_while_true()) {
        return 1;
    }
    if (!test_semantic_rejects_unreachable_return_after_for_true()) {
        return 1;
    }
    if (!test_semantic_accepts_reachable_return_after_break_in_while_true()) {
        return 1;
    }
    if (!test_semantic_rejects_call_to_undeclared_function()) {
        return 1;
    }
    if (!test_semantic_rejects_call_to_non_function_symbol()) {
        return 1;
    }
    if (!test_semantic_reports_call_site_for_callable_diagnostic()) {
        return 1;
    }
    if (!test_semantic_rejects_call_argument_count_mismatch()) {
        return 1;
    }
    if (!test_semantic_rejects_chained_call_as_unsupported_indirect_call()) {
        return 1;
    }
    if (!test_semantic_rejects_parenthesized_call_result_forms()) {
        return 1;
    }
    if (!test_semantic_rejects_parenthesized_non_identifier_callee_form()) {
        return 1;
    }
    if (!test_semantic_rejects_non_identifier_callee_metadata_contract()) {
        return 1;
    }
    if (!test_semantic_callable_diagnostic_matrix()) {
        return 1;
    }
    if (!test_semantic_callable_accept_matrix()) {
        return 1;
    }
    if (!test_semantic_accepts_call_to_declared_function()) {
        return 1;
    }
    if (!test_semantic_rejects_call_before_visible_declaration()) {
        return 1;
    }
    if (!test_semantic_accepts_call_with_prior_prototype_then_later_definition()) {
        return 1;
    }
    if (!run_must_pass_or_fail_now_group()) {
        return 1;
    }

    /*
     * Run known-limitation locks as a dedicated group so they are not confused
     * with strict-semantic baseline cases.
     */
    if (!run_known_limitation_lock_group()) {
        return 1;
    }

    printf("[semantic-reg] All semantic regressions passed.\n");
    return 0;
}