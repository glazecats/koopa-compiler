#include "lexer.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    const char *source;
    TokenArray *tokens;
} TokenizeArgs;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
#endif

static int run_with_stderr_suppressed(int (*fn)(void *), void *ctx) {
    int saved_stderr_fd;
    int devnull_fd;
    int result;
    int stderr_redirected = 0;

    fflush(stderr);
    saved_stderr_fd = dup(STDERR_FILENO);
    if (saved_stderr_fd < 0) {
        return fn(ctx);
    }

    devnull_fd = open("/dev/null", O_WRONLY);
    if (devnull_fd >= 0) {
        if (dup2(devnull_fd, STDERR_FILENO) >= 0) {
            stderr_redirected = 1;
        }
        close(devnull_fd);
    }

    result = fn(ctx);

    if (stderr_redirected) {
        fflush(stderr);
        dup2(saved_stderr_fd, STDERR_FILENO);
    }
    close(saved_stderr_fd);

    return result;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int run_lexer_tokenize_suppressed(void *ctx) {
    TokenizeArgs *args = (TokenizeArgs *)ctx;

    return lexer_tokenize(args->source, args->tokens);
}

static int test_block_comment_ending_at_eof(void) {
    TokenArray tokens;
    lexer_init_tokens(&tokens);

    if (!lexer_tokenize("/* ok */", &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: block comment ending at EOF was rejected\n");
        return 0;
    }

    if (tokens.size != 1 || tokens.data[0].type != TOKEN_EOF) {
        fprintf(stderr, "[lexer-reg] FAIL: expected only EOF token after full-comment file\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_reuse_token_array_without_manual_free(void) {
    TokenArray tokens;
    lexer_init_tokens(&tokens);

    if (!lexer_tokenize("int a = 1;", &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: first tokenize failed\n");
        return 0;
    }

    if (!lexer_tokenize("int b = 2;", &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: second tokenize failed\n");
        return 0;
    }

    if (tokens.size == 0 || tokens.data[0].type != TOKEN_KW_INT) {
        fprintf(stderr, "[lexer-reg] FAIL: unexpected token stream after reuse\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_invalid_token_array_state_rejected(void) {
    TokenArray tokens;
    TokenizeArgs args;

    memset(&tokens, 0, sizeof(tokens));

    tokens.data = NULL;
    tokens.size = 1;
    tokens.capacity = 0;

    args.source = "int c = 3;";
    args.tokens = &tokens;

    if (run_with_stderr_suppressed(run_lexer_tokenize_suppressed, &args)) {
        fprintf(stderr, "[lexer-reg] FAIL: invalid TokenArray state should be rejected\n");
        return 0;
    }

    return 1;
}

static int test_garbage_token_array_state_rejected(void) {
    TokenArray tokens;
    TokenizeArgs args;

    memset(&tokens, 0xAB, sizeof(tokens));
    args.source = "int d = 4;";
    args.tokens = &tokens;

    if (run_with_stderr_suppressed(run_lexer_tokenize_suppressed, &args)) {
        fprintf(stderr, "[lexer-reg] FAIL: garbage TokenArray state should be rejected\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    return 1;
}

static int test_break_continue_keywords(void) {
    const char *source = "while(1){continue;break;}";
    TokenArray tokens;
    size_t i;
    int saw_break = 0;
    int saw_continue = 0;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on break/continue keyword input\n");
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type == TOKEN_KW_BREAK) {
            saw_break = 1;
        }
        if (tokens.data[i].type == TOKEN_KW_CONTINUE) {
            saw_continue = 1;
        }
    }

    if (!saw_break || !saw_continue) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected KW_BREAK and KW_CONTINUE tokens (break=%d continue=%d)\n",
                saw_break,
                saw_continue);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_bang_and_not_equal_tokens(void) {
    const char *source = "!a!=b;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_BANG,
        TOKEN_IDENTIFIER,
        TOKEN_NE,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on bang/not-equal input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for !a!=b;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for !a!=b; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_tilde_bang_and_not_equal_tokens(void) {
    const char *source = "~a!=!b;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_TILDE,
        TOKEN_IDENTIFIER,
        TOKEN_NE,
        TOKEN_BANG,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on tilde/bang/not-equal input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for ~a!=!b;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for ~a!=!b; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_and_and_or_or_token_sequence(void) {
    const char *source = "a&&b||c;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_IDENTIFIER,
        TOKEN_AND_AND,
        TOKEN_IDENTIFIER,
        TOKEN_OR_OR,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on &&/|| input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for a&&b||c;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for a&&b||c; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_bitwise_operator_token_sequence(void) {
    const char *source = "a&b^c|d;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_IDENTIFIER,
        TOKEN_AMP,
        TOKEN_IDENTIFIER,
        TOKEN_CARET,
        TOKEN_IDENTIFIER,
        TOKEN_PIPE,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on bitwise operator input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for a&b^c|d;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for a&b^c|d; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);

    return 1;
}

static int test_shift_operator_token_sequence(void) {
    const char *source = "a<<b>>c;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_IDENTIFIER,
        TOKEN_SHIFT_LEFT,
        TOKEN_IDENTIFIER,
        TOKEN_SHIFT_RIGHT,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on shift operator input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for a<<b>>c;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for a<<b>>c; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_ternary_operator_token_sequence(void) {
    const char *source = "a?b:c;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_IDENTIFIER,
        TOKEN_QUESTION,
        TOKEN_IDENTIFIER,
        TOKEN_COLON,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on ternary operator input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for a?b:c;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for a?b:c; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_prefix_increment_decrement_token_sequence(void) {
    const char *source = "++a;--b;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_PLUS_PLUS,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_MINUS_MINUS,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on prefix ++/-- input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for ++a;--b;, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: token %zu mismatch for ++a;--b; expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_compound_assignment_boundary_token_sequence(void) {
    const char *source = "a+=b; c-=d; e*=f; g/=h; i%=j; k<<=l; m>>=n; o&=p; q|=r; s^=t;";
    TokenArray tokens;
    const TokenType expected[] = {
        TOKEN_IDENTIFIER,
        TOKEN_PLUS_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_MINUS_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_STAR_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_SLASH_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_PERCENT_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_SHIFT_LEFT_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_SHIFT_RIGHT_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_AMP_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_PIPE_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER,
        TOKEN_CARET_ASSIGN,
        TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
        TOKEN_EOF,
    };
    size_t i;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: lexer failed on compound-assignment boundary input\n");
        return 0;
    }

    if (tokens.size != sizeof(expected) / sizeof(expected[0])) {
        fprintf(stderr,
                "[lexer-reg] FAIL: expected %zu tokens for compound-assignment boundary input, got %zu\n",
                sizeof(expected) / sizeof(expected[0]),
                tokens.size);
        lexer_free_tokens(&tokens);
        return 0;
    }

    for (i = 0; i < tokens.size; ++i) {
        if (tokens.data[i].type != expected[i]) {
            fprintf(stderr,
                    "[lexer-reg] FAIL: compound-boundary token %zu mismatch: expected %s got %s\n",
                    i,
                    lexer_token_type_name(expected[i]),
                    lexer_token_type_name(tokens.data[i].type));
            lexer_free_tokens(&tokens);
            return 0;
        }
    }

    lexer_free_tokens(&tokens);
    return 1;
}

int main(void) {
    int ok = 1;

    ok &= test_block_comment_ending_at_eof();
    ok &= test_reuse_token_array_without_manual_free();
    ok &= test_invalid_token_array_state_rejected();
    ok &= test_garbage_token_array_state_rejected();
    ok &= test_break_continue_keywords();
    ok &= test_bang_and_not_equal_tokens();
    ok &= test_tilde_bang_and_not_equal_tokens();
    ok &= test_and_and_or_or_token_sequence();
    ok &= test_bitwise_operator_token_sequence();
    ok &= test_shift_operator_token_sequence();
    ok &= test_ternary_operator_token_sequence();
    ok &= test_prefix_increment_decrement_token_sequence();
    ok &= test_compound_assignment_boundary_token_sequence();

    if (!ok) {
        return 1;
    }

    printf("[lexer-reg] All lexer regressions passed.\n");
    return 0;
}
