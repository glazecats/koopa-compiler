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

int main(void) {
    if (!test_semantic_accepts_valid_program()) {
        return 1;
    }
    if (!test_semantic_rejects_duplicate_function()) {
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

    printf("[semantic-reg] All semantic regressions passed.\n");
    return 0;
}