#ifndef AST_H
#define AST_H

#include <stddef.h>

#include "lexer.h"

typedef enum {
    AST_EXTERNAL_FUNCTION = 0,
    AST_EXTERNAL_DECLARATION,
} AstExternalKind;

typedef enum {
    AST_EXPR_IDENTIFIER = 0,
    AST_EXPR_NUMBER,
    AST_EXPR_PAREN,
    AST_EXPR_BINARY,
} AstExpressionKind;

typedef struct AstExpression AstExpression;

struct AstExpression {
    AstExpressionKind kind;
    int line;
    int column;
    union {
        struct {
            char *name;
            size_t name_length;
        } identifier;
        long long number_value;
        AstExpression *inner;
        struct {
            TokenType op;
            AstExpression *left;
            AstExpression *right;
        } binary;
    } as;
};

typedef struct {
    AstExternalKind kind;
    char *name;
    size_t name_length;
    int has_initializer;
    size_t parameter_count;
    int is_function_definition;
    size_t return_statement_count;
    int returns_on_all_paths;
    size_t loop_statement_count;
    size_t if_statement_count;
    size_t break_statement_count;
    size_t continue_statement_count;
    size_t declaration_statement_count;
    int line;
    int column;
} AstExternal;

typedef struct {
    AstExternal *externals;
    size_t count;
    size_t capacity;
} AstProgram;

void ast_program_init(AstProgram *program);
void ast_program_free(AstProgram *program);

/*
 * Adds a top-level external declaration/function to program.
 * If name_token is NULL, the external is recorded without a name.
 * Returns 1 on success, 0 on allocation failure.
 */
int ast_program_add_external(AstProgram *program, AstExternalKind kind, const Token *name_token);

const char *ast_external_kind_name(AstExternalKind kind);

void ast_expression_free(AstExpression *expr);

const char *ast_expression_kind_name(AstExpressionKind kind);

#endif