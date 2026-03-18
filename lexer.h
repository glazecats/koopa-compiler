#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_INVALID,

    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,

    TOKEN_KW_INT,
    TOKEN_KW_RETURN,
    TOKEN_KW_IF,
    TOKEN_KW_ELSE,
    TOKEN_KW_WHILE,
    TOKEN_KW_FOR,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
} TokenType;

typedef struct {
    TokenType type;
    const char *lexeme;
    size_t length;
    long long number_value;
    int line;
    int column;
} Token;

typedef struct {
    Token *data;
    size_t size;
    size_t capacity;
} TokenArray;

int lexer_tokenize(const char *source, TokenArray *out_tokens);
void lexer_free_tokens(TokenArray *tokens);
const char *lexer_token_type_name(TokenType type);

#endif
