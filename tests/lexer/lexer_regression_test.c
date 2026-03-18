#include "lexer.h"

#include <string.h>
#include <stdio.h>

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

    tokens.data = NULL;
    tokens.size = 1;
    tokens.capacity = 0;

    if (lexer_tokenize("int c = 3;", &tokens)) {
        fprintf(stderr, "[lexer-reg] FAIL: invalid TokenArray state should be rejected\n");
        return 0;
    }

    return 1;
}

static int test_garbage_token_array_state_rejected(void) {
    TokenArray tokens;

    memset(&tokens, 0xAB, sizeof(tokens));
    if (lexer_tokenize("int d = 4;", &tokens)) {
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
