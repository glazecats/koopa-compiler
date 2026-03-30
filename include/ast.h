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
    AST_EXPR_UNARY,
    AST_EXPR_POSTFIX,
    AST_EXPR_CALL,
    AST_EXPR_BINARY,
    AST_EXPR_TERNARY,
} AstExpressionKind;

typedef enum {
    AST_STMT_COMPOUND = 0,
    AST_STMT_DECLARATION,
    AST_STMT_EXPRESSION,
    AST_STMT_IF,
    AST_STMT_WHILE,
    AST_STMT_FOR,
    AST_STMT_RETURN,
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
} AstStatementKind;

typedef enum {
    AST_CALL_CALLEE_DIRECT_IDENTIFIER = 0,
    AST_CALL_CALLEE_CALL_RESULT,
    AST_CALL_CALLEE_NON_IDENTIFIER,
} AstCallCalleeKind;

typedef struct AstExpression AstExpression;
typedef struct AstStatement AstStatement;

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
            AstExpression *operand;
        } unary;
        struct {
            TokenType op;
            AstExpression *operand;
        } postfix;
        struct {
            AstExpression *callee;
            AstExpression **args;
            size_t arg_count;
        } call;
        struct {
            TokenType op;
            AstExpression *left;
            AstExpression *right;
        } binary;
        struct {
            AstExpression *condition;
            AstExpression *then_expr;
            AstExpression *else_expr;
        } ternary;
    } as;
};

struct AstStatement {
    AstStatementKind kind;
    int line;
    int column;
    char **declaration_names;
    size_t declaration_name_count;
    AstExpression **expressions;
    size_t expression_count;
    int has_primary_expression;
    size_t primary_expression_index;
    int has_condition_expression;
    size_t condition_expression_index;
    int has_for_init_expression;
    size_t for_init_expression_index;
    int has_for_step_expression;
    size_t for_step_expression_index;
    AstStatement **children;
    size_t child_count;
};

typedef struct {
    AstExternalKind kind;
    char *name;
    size_t name_length;
    int has_initializer;
    size_t parameter_count;
    char **parameter_names;
    int is_function_definition;
    AstStatement *function_body;
    size_t return_statement_count;
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

void ast_statement_free(AstStatement *stmt);

const char *ast_statement_kind_name(AstStatementKind kind);

#endif