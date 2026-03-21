#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct {
    int line;
    int column;
    char message[128];
} ParserError;

/*
 * The token stream must be non-empty and end with TOKEN_EOF.
 * Returns 1 on success, 0 on syntax/contract error.
 */
int parser_parse_translation_unit(const TokenArray *tokens, ParserError *error);

/*
 * Parses tokens and builds a top-level AST program.
 * out_program must be initialized (via ast_program_init or {0}).
 * Returns 1 on success, 0 on syntax/contract/allocation error.
 */
int parser_parse_translation_unit_ast(const TokenArray *tokens,
                                      AstProgram *out_program,
                                      ParserError *error);

/*
 * Parses a standalone expression token stream into an expression AST supporting
 * primary, parenthesized, multiplicative, and additive expressions.
 * tokens must be non-empty and end with TOKEN_EOF.
 * On success, out_expression receives ownership of the allocated AST root.
 */
int parser_parse_expression_ast_additive(const TokenArray *tokens,
                                         AstExpression **out_expression,
                                         ParserError *error);

/* Backward-compatible alias for previous API name. */
int parser_parse_expression_ast_primary(const TokenArray *tokens,
                                        AstExpression **out_expression,
                                        ParserError *error);

#endif