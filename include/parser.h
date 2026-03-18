#ifndef PARSER_H
#define PARSER_H

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

#endif