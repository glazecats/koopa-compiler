#ifndef AST_H
#define AST_H

#include <stddef.h>

#include "lexer.h"

typedef enum {
    AST_EXTERNAL_FUNCTION = 0,
    AST_EXTERNAL_DECLARATION,
} AstExternalKind;

typedef enum {
    AST_FUNCTION_RETURN_INT = 0,
    AST_FUNCTION_RETURN_FLOAT,
    AST_FUNCTION_RETURN_PAIR,
    AST_FUNCTION_RETURN_STRUCT,
    AST_FUNCTION_RETURN_VOID,
} AstFunctionReturnType;

typedef enum {
    AST_PARAMETER_VALUE_INT = 0,
    AST_PARAMETER_VALUE_FLOAT,
    AST_PARAMETER_VALUE_PAIR,
    AST_PARAMETER_VALUE_STRUCT,
    AST_PARAMETER_VALUE_FUNCTION,
} AstParameterValueKind;

typedef enum {
    AST_DECLARATION_VALUE_INT = 0,
    AST_DECLARATION_VALUE_FLOAT,
    AST_DECLARATION_VALUE_PAIR,
    AST_DECLARATION_VALUE_STRUCT,
    AST_DECLARATION_VALUE_FUNCTION,
} AstDeclarationValueKind;

typedef enum {
    AST_EXPR_IDENTIFIER = 0,
    AST_EXPR_NUMBER,
    AST_EXPR_FLOAT_LITERAL,
    AST_EXPR_CONVERSION,
    AST_EXPR_INIT_LIST,
    AST_EXPR_PAREN,
    AST_EXPR_UNARY,
    AST_EXPR_POSTFIX,
    AST_EXPR_SUBSCRIPT,
    AST_EXPR_MEMBER,
    AST_EXPR_CALL,
    AST_EXPR_BINARY,
    AST_EXPR_TERNARY,
    AST_EXPR_CLOSURE,
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
    AST_STMT_DEFER,
    AST_STMT_FNDEFER,
    AST_STMT_CAPDEFER,
} AstStatementKind;

typedef enum {
    AST_EXTENSION_ORIGIN_NONE = 0,
    AST_EXTENSION_ORIGIN_DEFER,
    AST_EXTENSION_ORIGIN_FNDEFER,
    AST_EXTENSION_ORIGIN_CAPDEFER,
    AST_EXTENSION_ORIGIN_UNLESS,
    AST_EXTENSION_ORIGIN_PAIR,
    AST_EXTENSION_ORIGIN_STRUCT,
} AstExtensionOrigin;

typedef struct {
    char *name;
    size_t name_length;
    char **field_names;
    int *field_value_kinds;
    char **field_type_names;
    size_t field_count;
} AstStructType;

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
        double float_number_value;
        struct {
            AstDeclarationValueKind target_type_kind;
            AstExpression *operand;
        } conversion;
        struct {
            AstExpression **items;
            size_t item_count;
        } init_list;
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
            AstExpression *base;
            AstExpression *index;
        } subscript;
        struct {
            AstExpression *base;
            char *field_name;
            size_t field_name_length;
        } member;
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
        struct {
            char **capture_names;
            size_t capture_name_count;
            AstFunctionReturnType return_type;
            char *return_type_name;
            size_t parameter_count;
            char **parameter_names;
            int *parameter_value_kinds;
            char **parameter_type_names;
            AstFunctionReturnType *parameter_function_return_types;
            size_t *parameter_function_parameter_counts;
            int **parameter_function_parameter_value_kinds;
            AstFunctionReturnType **parameter_function_parameter_return_types;
            size_t **parameter_function_parameter_parameter_counts;
            AstStatement *body;
        } closure;
    } as;
};

struct AstStatement {
    AstStatementKind kind;
    AstExtensionOrigin extension_origin;
    int line;
    int column;
    int declaration_is_const;
    char **declaration_names;
    size_t declaration_name_count;
    int *declaration_value_kinds;
    size_t *declaration_array_ranks;
    AstExpression ***declaration_array_extent_exprs;
    AstFunctionReturnType *declaration_function_return_types;
    size_t *declaration_function_parameter_counts;
    int **declaration_function_parameter_value_kinds;
    AstFunctionReturnType **declaration_function_parameter_return_types;
    size_t **declaration_function_parameter_parameter_counts;
    int ***declaration_function_parameter_parameter_value_kinds;
    AstFunctionReturnType ***declaration_function_parameter_parameter_return_types;
    size_t ***declaration_function_parameter_parameter_parameter_counts;
    char **declaration_type_names;
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
    char **capture_names;
    size_t capture_name_count;
    AstExpression **capture_expressions;
    size_t capture_expression_count;
    AstStatement **children;
    size_t child_count;
};

typedef struct {
    AstExternalKind kind;
    AstFunctionReturnType function_return_type;
    char *function_return_type_name;
    int has_function_return_signature;
    AstFunctionReturnType function_return_function_return_type;
    char *function_return_function_return_type_name;
    size_t function_return_function_parameter_count;
    int *function_return_function_parameter_value_kinds;
    AstFunctionReturnType *function_return_function_parameter_return_types;
    size_t *function_return_function_parameter_parameter_counts;
    int **function_return_function_parameter_parameter_value_kinds;
    AstFunctionReturnType **function_return_function_parameter_parameter_return_types;
    size_t **function_return_function_parameter_parameter_parameter_counts;
    char **function_return_function_parameter_type_names;
    int declaration_value_kind;
    char *declaration_type_name;
    char *name;
    size_t name_length;
    size_t declaration_array_rank;
    AstExpression **declaration_array_extent_exprs;
    int is_const_qualified;
    int has_initializer;
    AstExpression *declaration_initializer;
    size_t parameter_count;
    char **parameter_names;
    int *parameter_value_kinds;
    char **parameter_type_names;
    size_t *parameter_array_ranks;
    AstExpression ***parameter_array_extent_exprs;
    AstFunctionReturnType *parameter_function_return_types;
    size_t *parameter_function_parameter_counts;
    int **parameter_function_parameter_value_kinds;
    AstFunctionReturnType **parameter_function_parameter_return_types;
    size_t **parameter_function_parameter_parameter_counts;
    int ***parameter_function_parameter_parameter_value_kinds;
    AstFunctionReturnType ***parameter_function_parameter_parameter_return_types;
    size_t ***parameter_function_parameter_parameter_parameter_counts;
    int *parameter_is_const;
    int *parameter_name_lines;
    int *parameter_name_columns;
    int is_function_definition;
    AstStatement *function_body;
    size_t return_statement_count;
    int line;
    int column;
} AstExternal;

static inline int ast_external_function_returns_value(const AstExternal *external) {
    return external && external->kind == AST_EXTERNAL_FUNCTION &&
        external->function_return_type != AST_FUNCTION_RETURN_VOID;
}

typedef struct {
    AstStructType *struct_types;
    size_t struct_type_count;
    size_t struct_type_capacity;
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
