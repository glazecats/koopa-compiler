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
    TOKEN_KW_BREAK,
    TOKEN_KW_CONTINUE,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_BANG,
    TOKEN_TILDE,
    TOKEN_AMP,
    TOKEN_CARET,
    TOKEN_PIPE,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_RIGHT,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_QUESTION,
    TOKEN_COLON,
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
    unsigned int magic;
    Token *data;
    size_t size;
    size_t capacity;
} TokenArray;

/*
 * Initializes TokenArray to a safe empty state.
 * Must be called before first use unless the struct is zero-initialized.
 */
void lexer_init_tokens(TokenArray *tokens);

/*
 * Tokenizes source into out_tokens.
 * out_tokens must be in a valid initialized state (via lexer_init_tokens or {0}).
 * Returns 1 on success, 0 on lexical/contract error.
 */
int lexer_tokenize(const char *source, TokenArray *out_tokens);

/*
 * Frees token storage and resets TokenArray to empty state.
 */
void lexer_free_tokens(TokenArray *tokens);
const char *lexer_token_type_name(TokenType type);

#endif
