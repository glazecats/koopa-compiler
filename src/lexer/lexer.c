#include "lexer.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *current;
    int line;
    int column;
    TokenArray *tokens;
} Lexer;

#define TOKEN_ARRAY_MAGIC 0x544f4b4eU

static bool token_array_reserve(TokenArray *arr, size_t new_cap) {
    Token *new_data = (Token *)realloc(arr->data, new_cap * sizeof(Token));
    if (!new_data) {
        return false;
    }
    arr->data = new_data;
    arr->capacity = new_cap;
    return true;
}

static bool token_array_push(TokenArray *arr, Token token) {
    if (arr->size == arr->capacity) {
        size_t next_cap = arr->capacity == 0 ? 32 : arr->capacity * 2;
        if (!token_array_reserve(arr, next_cap)) {
            return false;
        }
    }
    arr->data[arr->size++] = token;
    return true;
}

void lexer_init_tokens(TokenArray *tokens) {
    if (!tokens) {
        return;
    }
    tokens->magic = TOKEN_ARRAY_MAGIC;
    tokens->data = NULL;
    tokens->size = 0;
    tokens->capacity = 0;
}

void lexer_free_tokens(TokenArray *tokens) {
    if (!tokens) {
        return;
    }

    if (tokens->magic != TOKEN_ARRAY_MAGIC) {
        lexer_init_tokens(tokens);
        return;
    }

    free(tokens->data);
    lexer_init_tokens(tokens);
}

static bool token_array_state_is_valid(const TokenArray *tokens) {
    if (!tokens) {
        return false;
    }

    if (tokens->magic != TOKEN_ARRAY_MAGIC) {
        return false;
    }

    if (tokens->data == NULL) {
        return tokens->size == 0 && tokens->capacity == 0;
    }

    if (tokens->capacity == 0) {
        return false;
    }

    return tokens->size <= tokens->capacity;
}

static bool is_at_end(const Lexer *lx) {
    return *lx->current == '\0';
}

static char advance(Lexer *lx) {
    char c = *lx->current++;
    if (c == '\n') {
        lx->line++;
        lx->column = 1;
    } else {
        lx->column++;
    }
    return c;
}

static char peek(const Lexer *lx) {
    return *lx->current;
}

static char peek_next(const Lexer *lx) {
    if (is_at_end(lx)) {
        return '\0';
    }
    return lx->current[1];
}

static bool match(Lexer *lx, char expected) {
    if (is_at_end(lx)) {
        return false;
    }
    if (*lx->current != expected) {
        return false;
    }
    advance(lx);
    return true;
}

static bool add_token(Lexer *lx, TokenType type, const char *lexeme, size_t length,
                      long long number_value, int line, int column) {
    Token t;
    t.type = type;
    t.lexeme = lexeme;
    t.length = length;
    t.number_value = number_value;
    t.line = line;
    t.column = column;
    return token_array_push(lx->tokens, t);
}

static TokenType keyword_or_identifier(const char *start, size_t len) {
    if (len == 2 && strncmp(start, "if", 2) == 0) {
        return TOKEN_KW_IF;
    }
    if (len == 3 && strncmp(start, "int", 3) == 0) {
        return TOKEN_KW_INT;
    }
    if (len == 4 && strncmp(start, "else", 4) == 0) {
        return TOKEN_KW_ELSE;
    }
    if (len == 3 && strncmp(start, "for", 3) == 0) {
        return TOKEN_KW_FOR;
    }
    if (len == 5 && strncmp(start, "while", 5) == 0) {
        return TOKEN_KW_WHILE;
    }
    if (len == 6 && strncmp(start, "return", 6) == 0) {
        return TOKEN_KW_RETURN;
    }
    if (len == 5 && strncmp(start, "break", 5) == 0) {
        return TOKEN_KW_BREAK;
    }
    if (len == 8 && strncmp(start, "continue", 8) == 0) {
        return TOKEN_KW_CONTINUE;
    }
    return TOKEN_IDENTIFIER;
}

enum {
    SCAN_OK = 1,
    SCAN_OOM = 0,
    SCAN_ERROR = -1,
};

static int scan_number(Lexer *lx, const char *start, int line, int column) {
    long long value = (long long)(*start - '0');
    while (isdigit((unsigned char)peek(lx))) {
        int digit = advance(lx) - '0';
        if (value > (LLONG_MAX - digit) / 10) {
            fprintf(stderr, "Integer literal out of range at %d:%d\n", line, column);
            return SCAN_ERROR;
        }
        value = value * 10 + digit;
    }

    size_t len = (size_t)(lx->current - start);
    if (!add_token(lx, TOKEN_NUMBER, start, len, value, line, column)) {
        return SCAN_OOM;
    }
    return SCAN_OK;
}

static bool scan_identifier(Lexer *lx, const char *start, int line, int column) {
    while (isalnum((unsigned char)peek(lx)) || peek(lx) == '_') {
        advance(lx);
    }
    size_t len = (size_t)(lx->current - start);
    TokenType type = keyword_or_identifier(start, len);
    return add_token(lx, type, start, len, 0, line, column);
}

int lexer_tokenize(const char *source, TokenArray *out_tokens) {
    if (!source || !out_tokens) {
        return 0;
    }

    if (out_tokens->magic == 0 && out_tokens->data == NULL && out_tokens->size == 0 &&
        out_tokens->capacity == 0) {
        lexer_init_tokens(out_tokens);
    }

    if (!token_array_state_is_valid(out_tokens)) {
        fprintf(stderr, "TokenArray is not initialized; call lexer_init_tokens first\n");
        return 0;
    }

    if (out_tokens->data) {
        free(out_tokens->data);
    }
    lexer_init_tokens(out_tokens);

    Lexer lx;
    lx.current = source;
    lx.line = 1;
    lx.column = 1;
    lx.tokens = out_tokens;

    while (!is_at_end(&lx)) {
        int tok_line = lx.line;
        int tok_col = lx.column;
        const char *tok_start = lx.current;
        char c = advance(&lx);

        if (isspace((unsigned char)c)) {
            continue;
        }

        if (isalpha((unsigned char)c) || c == '_') {
            if (!scan_identifier(&lx, tok_start, tok_line, tok_col)) {
                goto oom;
            }
            continue;
        }

        if (isdigit((unsigned char)c)) {
            int scan_status = scan_number(&lx, tok_start, tok_line, tok_col);
            if (scan_status == SCAN_ERROR) {
                lexer_free_tokens(out_tokens);
                return 0;
            }
            if (scan_status == SCAN_OOM) {
                goto oom;
            }
            continue;
        }

        switch (c)
        {
        case '+':
            if (match(&lx, '+')) {
                if (!add_token(&lx, TOKEN_PLUS_PLUS, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_PLUS, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '-':
            if (match(&lx, '-')) {
                if (!add_token(&lx, TOKEN_MINUS_MINUS, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_MINUS, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '*':
            if (!add_token(&lx, TOKEN_STAR, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '/':
            if (peek(&lx) == '/') {
                while (!is_at_end(&lx) && peek(&lx) != '\n') {
                    advance(&lx);
                }
                break;
            }
            if (peek(&lx) == '*') {
                bool closed = false;
                advance(&lx);
                while (!is_at_end(&lx)) {
                    if (peek(&lx) == '*' && peek_next(&lx) == '/') {
                        advance(&lx);
                        advance(&lx);
                        closed = true;
                        break;
                    }
                    advance(&lx);
                }
                if (!closed) {
                    fprintf(stderr, "Unterminated block comment at %d:%d\n", tok_line, tok_col);
                    lexer_free_tokens(out_tokens);
                    return 0;
                }
                break;
            }
            if (!add_token(&lx, TOKEN_SLASH, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '%':
            if (!add_token(&lx, TOKEN_PERCENT, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '(':
            if (!add_token(&lx, TOKEN_LPAREN, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case ')':
            if (!add_token(&lx, TOKEN_RPAREN, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '{':
            if (!add_token(&lx, TOKEN_LBRACE, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '}':
            if (!add_token(&lx, TOKEN_RBRACE, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case ';':
            if (!add_token(&lx, TOKEN_SEMICOLON, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case ',':
            if (!add_token(&lx, TOKEN_COMMA, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '?':
            if (!add_token(&lx, TOKEN_QUESTION, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case ':':
            if (!add_token(&lx, TOKEN_COLON, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '=':
            if (match(&lx, '=')) {
                if (!add_token(&lx, TOKEN_EQ, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_ASSIGN, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '!':
            if (match(&lx, '=')) {
                if (!add_token(&lx, TOKEN_NE, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_BANG, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '~':
            if (!add_token(&lx, TOKEN_TILDE, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '&':
            if (match(&lx, '&')) {
                if (!add_token(&lx, TOKEN_AND_AND, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_AMP, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '^':
            if (!add_token(&lx, TOKEN_CARET, tok_start, 1, 0, tok_line, tok_col))
                goto oom;
            break;
        case '|':
            if (match(&lx, '|')) {
                if (!add_token(&lx, TOKEN_OR_OR, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_PIPE, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '<':
            if (match(&lx, '<')) {
                if (!add_token(&lx, TOKEN_SHIFT_LEFT, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else if (match(&lx, '=')) {
                if (!add_token(&lx, TOKEN_LE, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_LT, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        case '>':
            if (match(&lx, '>')) {
                if (!add_token(&lx, TOKEN_SHIFT_RIGHT, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else if (match(&lx, '=')) {
                if (!add_token(&lx, TOKEN_GE, tok_start, 2, 0, tok_line, tok_col))
                    goto oom;
            } else {
                if (!add_token(&lx, TOKEN_GT, tok_start, 1, 0, tok_line, tok_col))
                    goto oom;
            }
            break;
        default:
            fprintf(stderr, "Unexpected character '%c' at %d:%d\n", c, tok_line, tok_col);
            lexer_free_tokens(out_tokens);
            return 0;
        }
    }

    if (!add_token(&lx, TOKEN_EOF, lx.current, 0, 0, lx.line, lx.column)) {
        goto oom;
    }

    return 1;

oom:
    fprintf(stderr, "Out of memory while tokenizing\n");
    lexer_free_tokens(out_tokens);
    return 0;
}

const char *lexer_token_type_name(TokenType type) {
    switch (type)
    {
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_INVALID:
        return "INVALID";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_NUMBER:
        return "NUMBER";
    case TOKEN_KW_INT:
        return "KW_INT";
    case TOKEN_KW_RETURN:
        return "KW_RETURN";
    case TOKEN_KW_IF:
        return "KW_IF";
    case TOKEN_KW_ELSE:
        return "KW_ELSE";
    case TOKEN_KW_WHILE:
        return "KW_WHILE";
    case TOKEN_KW_FOR:
        return "KW_FOR";
    case TOKEN_KW_BREAK:
        return "KW_BREAK";
    case TOKEN_KW_CONTINUE:
        return "KW_CONTINUE";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_PLUS_PLUS:
        return "PLUS_PLUS";
    case TOKEN_MINUS_MINUS:
        return "MINUS_MINUS";
    case TOKEN_STAR:
        return "STAR";
    case TOKEN_SLASH:
        return "SLASH";
    case TOKEN_PERCENT:
        return "PERCENT";
    case TOKEN_BANG:
        return "BANG";
    case TOKEN_TILDE:
        return "TILDE";
    case TOKEN_AMP:
        return "AMP";
    case TOKEN_CARET:
        return "CARET";
    case TOKEN_PIPE:
        return "PIPE";
    case TOKEN_AND_AND:
        return "AND_AND";
    case TOKEN_OR_OR:
        return "OR_OR";
    case TOKEN_ASSIGN:
        return "ASSIGN";
    case TOKEN_EQ:
        return "EQ";
    case TOKEN_NE:
        return "NE";
    case TOKEN_LT:
        return "LT";
    case TOKEN_LE:
        return "LE";
    case TOKEN_GT:
        return "GT";
    case TOKEN_GE:
        return "GE";
    case TOKEN_SHIFT_LEFT:
        return "SHIFT_LEFT";
    case TOKEN_SHIFT_RIGHT:
        return "SHIFT_RIGHT";
    case TOKEN_LPAREN:
        return "LPAREN";
    case TOKEN_RPAREN:
        return "RPAREN";
    case TOKEN_LBRACE:
        return "LBRACE";
    case TOKEN_RBRACE:
        return "RBRACE";
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    case TOKEN_COMMA:
        return "COMMA";
    case TOKEN_QUESTION:
        return "QUESTION";
    case TOKEN_COLON:
        return "COLON";
    default:
        return "UNKNOWN";
    }
}
