#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const TokenArray *tokens;
    size_t current;
    ParserError *error;
    int has_error;
    size_t error_index;
    size_t expression_recursion_depth;
    size_t expression_recursion_limit;
    size_t statement_recursion_depth;
    size_t statement_recursion_limit;
    size_t loop_depth;
} Parser;

typedef struct {
    int may_fallthrough;
    int may_break;
} ParserLocalControl;

typedef struct {
    const Token *name_tok;
    size_t parameter_count;
    char **parameter_names;
    int *parameter_name_lines;
    int *parameter_name_columns;
    int has_unnamed_parameter;
} ParsedFunctionSignature;

static const Token *peek(const Parser *p);
static const Token *previous(const Parser *p);
static int is_at_end(const Parser *p);
static const Token *advance(Parser *p);
static int check(const Parser *p, TokenType type);
static int match(Parser *p, TokenType type);
static void set_error(Parser *p, const Token *at, const char *fmt, ...);
static int enter_expression_recursion(Parser *p, const char *rule_name);
static void leave_expression_recursion(Parser *p);
static int enter_statement_recursion(Parser *p, const char *rule_name);
static void leave_statement_recursion(Parser *p);
static int consume(Parser *p, TokenType type, const char *what);
static int consume_token(Parser *p, TokenType type, const char *what, const Token **out_token);

static AstStatement *alloc_ast_statement(Parser *p,
    AstStatementKind kind,
    const Token *anchor_tok);
static void free_name_array(char **names, size_t count);
static int build_expression_ast_from_slice(Parser *p,
    size_t start,
    size_t end,
    AstExpression **out_expr,
    const Token *anchor_tok);
static int append_ast_statement_expression(Parser *p,
    AstStatement *stmt,
    AstExpression *expr,
    const Token *anchor_tok);

static int parse_expression(Parser *p);
static int parse_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement);
static int parse_compound_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement);
static int parse_declaration(Parser *p,
    AstProgram *top_level_program,
    char ***out_decl_names,
    size_t *out_decl_name_count,
    AstExpression ***out_decl_initializer_exprs,
    size_t *out_decl_initializer_expr_count);
static int parse_parameter_list(Parser *p,
    size_t *out_param_count,
    int *out_has_unnamed_parameter,
    char ***out_parameter_names,
    int **out_parameter_name_lines,
    int **out_parameter_name_columns);
static void parsed_function_signature_init(ParsedFunctionSignature *signature);
static void parsed_function_signature_release(ParsedFunctionSignature *signature);
static int parse_function_external_signature(Parser *p,
    int capture_parameter_names,
    ParsedFunctionSignature *out_signature);
static int parse_function_external_definition_body(Parser *p,
    const ParsedFunctionSignature *signature,
    AstStatement **out_function_body,
    size_t *out_return_statement_count);
static int parse_function_external(Parser *p,
    const Token **out_name_token,
    size_t *out_parameter_count,
    char ***out_parameter_names,
    int **out_parameter_name_lines,
    int **out_parameter_name_columns,
    int *out_is_definition,
    AstStatement **out_function_body,
    size_t *out_return_statement_count);
static int parser_prepare(Parser *p, const TokenArray *tokens, ParserError *error);
static int parse_translation_unit_external_or_declaration(Parser *p, AstProgram *out_program);

/*
 * Protect recursive-descent hotspots from stack overflow.
 * If the nesting depth is unreasonable, fail gracefully with a parser error.
 */
#define PARSER_EXPRESSION_RECURSION_LIMIT 2048U
#define PARSER_STATEMENT_RECURSION_LIMIT 2048U

#define PARSER_SPLIT_AGGREGATOR 1
#include "parser_ast_compat.inc"
#include "parser_core_expr.inc"
#include "parser_stmt_decl_tu.inc"
#undef PARSER_SPLIT_AGGREGATOR
