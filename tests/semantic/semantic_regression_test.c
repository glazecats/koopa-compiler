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
    int saved_returns_on_all_paths;

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

    /* Tamper parser return metadata; AST-primary flow should still reject this function. */
    saved_returns_on_all_paths = program.externals[0].returns_on_all_paths;
    program.externals[0].returns_on_all_paths = 1;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: non-returning function should fail even if parser return metadata is tampered\n");
        program.externals[0].returns_on_all_paths = saved_returns_on_all_paths;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "all control-flow paths") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected all-paths-return diagnostic from AST-primary flow, got: %s\n",
                sema_err.message);
        program.externals[0].returns_on_all_paths = saved_returns_on_all_paths;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    program.externals[0].returns_on_all_paths = saved_returns_on_all_paths;

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
    size_t saved_count;

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

    /* Tamper parser-side callable metadata; AST-primary path should still drive semantic result. */
    saved_count = program.externals[0].called_function_count;
    program.externals[0].called_function_count = 0;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: undeclared call should fail even if parser callable metadata is tampered\n");
        program.externals[0].called_function_count = saved_count;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-001") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-001 from AST-primary callable path, got: %s\n",
                sema_err.message);
        program.externals[0].called_function_count = saved_count;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    program.externals[0].called_function_count = saved_count;

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
    size_t saved_count;

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

    if (program.externals[main_index].called_function_count < 2) {
        fprintf(stderr,
                "[semantic-reg] FAIL: setup expected chained-call metadata count >= 2, got %zu\n",
                program.externals[main_index].called_function_count);
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    /* Hide outer call in parser metadata; AST-primary traversal should still reject chained call. */
    saved_count = program.externals[main_index].called_function_count;
    program.externals[main_index].called_function_count = 1;

    if (semantic_analyze_program(&program, &sema_err)) {
        fprintf(stderr,
                "[semantic-reg] FAIL: chained call should fail even if parser callable metadata is tampered\n");
        program.externals[main_index].called_function_count = saved_count;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    if (strstr(sema_err.message, "SEMA-CALL-005") == NULL) {
        fprintf(stderr,
                "[semantic-reg] FAIL: expected SEMA-CALL-005 from AST-primary chained-call path, got: %s\n",
                sema_err.message);
        program.externals[main_index].called_function_count = saved_count;
        lexer_free_tokens(&tokens);
        ast_program_free(&program);
        return 0;
    }

    program.externals[main_index].called_function_count = saved_count;

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
    if (!test_semantic_call_argument_mismatch_long_name_not_truncated()) {
        return 1;
    }
    if (!test_semantic_rejects_missing_function_body_during_ast_primary_phase()) {
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
    if (!test_semantic_rejects_metadata_only_definition_during_ast_primary_phase()) {
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
    if (!test_semantic_rejects_duplicate_local_declaration_same_scope()) {
        return 1;
    }
    if (!test_semantic_rejects_undeclared_local_identifier_use()) {
        return 1;
    }
    if (!test_semantic_rejects_call_with_undeclared_argument_identifier()) {
        return 1;
    }
    if (!test_semantic_rejects_undeclared_call_argument_across_statement_slots()) {
        return 1;
    }
    if (!test_semantic_allows_block_shadowing()) {
        return 1;
    }
    if (!test_semantic_allows_for_init_declaration_visibility()) {
        return 1;
    }
    if (!test_semantic_rejects_parameter_redeclaration_in_function_body()) {
        return 1;
    }
    if (!test_semantic_rejects_for_init_identifier_use_outside_loop()) {
        return 1;
    }
    if (!test_semantic_allows_for_init_shadowing_outer_identifier()) {
        return 1;
    }
    if (!test_semantic_rejects_duplicate_parameter_names()) {
        return 1;
    }
    if (!test_semantic_rejects_block_local_use_after_block_end()) {
        return 1;
    }
    if (!test_semantic_rejects_for_body_local_use_after_loop()) {
        return 1;
    }
    if (!test_semantic_allows_parameter_shadowing_in_inner_block()) {
        return 1;
    }
    if (!test_semantic_rejects_undeclared_identifier_in_declaration_initializer()) {
        return 1;
    }
    if (!test_semantic_rejects_undeclared_identifier_in_for_init_declaration_initializer()) {
        return 1;
    }
    if (!test_semantic_rejects_same_declaration_forward_reference()) {
        return 1;
    }
    if (!test_semantic_allows_same_declaration_reverse_reference()) {
        return 1;
    }
    if (!test_semantic_rejects_for_init_same_declaration_forward_reference()) {
        return 1;
    }
    if (!test_semantic_allows_for_init_same_declaration_reverse_reference()) {
        return 1;
    }
    if (!test_semantic_ast_primary_return_flow_ignores_tampered_metadata()) {
        return 1;
    }
    if (!test_semantic_ast_primary_callable_ignores_tampered_metadata()) {
        return 1;
    }
    if (!test_semantic_ast_primary_chained_call_ignores_tampered_metadata()) {
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