#include "lexer.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    const char *source;
    TokenArray *tokens;
} TokenizeArgs;

static int run_with_stderr_suppressed(int (*fn)(void *), void *ctx) {
    int saved_stderr = dup(STDERR_FILENO);
    int devnull_fd = open("/dev/null", O_WRONLY);
    int result;

    if (saved_stderr < 0 || devnull_fd < 0) {
        if (saved_stderr >= 0) {
            close(saved_stderr);
        }
        if (devnull_fd >= 0) {
            close(devnull_fd);
        }
        return fn(ctx);
    }

    fflush(stderr);
    if (dup2(devnull_fd, STDERR_FILENO) < 0) {
        close(devnull_fd);
        close(saved_stderr);
        return fn(ctx);
    }

    result = fn(ctx);

    fflush(stderr);
    (void)dup2(saved_stderr, STDERR_FILENO);
    close(devnull_fd);
    close(saved_stderr);
    return result;
}

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

int main(void) {
    if (!test_block_comment_ending_at_eof()) {
        return 1;
    }
    if (!test_reuse_token_array_without_manual_free()) {
        return 1;
    }
    if (!test_invalid_token_array_state_rejected()) {
        return 1;
    }
    if (!test_garbage_token_array_state_rejected()) {
        return 1;
    }

    printf("[lexer-reg] All lexer regressions passed.\n");
    return 0;
}
