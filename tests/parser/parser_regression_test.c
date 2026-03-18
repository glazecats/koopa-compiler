#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_reject_stream_without_eof(void) {
    Token bad_tokens[1];
    TokenArray stream;
    ParserError err;

    bad_tokens[0].type = TOKEN_KW_INT;
    bad_tokens[0].lexeme = "int";
    bad_tokens[0].length = 3;
    bad_tokens[0].number_value = 0;
    bad_tokens[0].line = 1;
    bad_tokens[0].column = 1;

    stream.data = bad_tokens;
    stream.magic = 0;
    stream.size = 1;
    stream.capacity = 1;

    if (parser_parse_translation_unit(&stream, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser accepted token stream without EOF\n");
        return 0;
    }

    if (strstr(err.message, "EOF") == NULL) {
        fprintf(stderr, "[parser-reg] FAIL: expected EOF-related error, got: %s\n", err.message);
        return 0;
    }

    return 1;
}

static int test_backtracking_error_not_polluted(void) {
    const char *source = "int x;\nint y\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for backtracking regression input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly accepted missing semicolon input\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (err.line <= 1) {
        fprintf(stderr, "[parser-reg] FAIL: expected error after first declaration, got line %d\n", err.line);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "';'") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected missing semicolon error, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "'('") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: stale function-parse error leaked into final message: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_farthest_failure_preferred(void) {
    const char *source = "int x(int y,int)\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for farthest-failure input\n");
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly accepted invalid parameter list\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "identifier") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected deeper parameter-list error, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (strstr(err.message, "';'") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: got earlier declaration-style error instead of farthest failure: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static int test_success_clears_error_state(void) {
    const char *source = "int x;\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for success-state test input\n");
        return 0;
    }

    err.line = -1;
    err.column = -1;
    snprintf(err.message, sizeof(err.message), "stale");

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr, "[parser-reg] FAIL: parser unexpectedly failed valid declaration\n");
        lexer_free_tokens(&tokens);
        return 0;
    }

    if (err.line != 0 || err.column != 0 || err.message[0] != '\0') {
        fprintf(stderr,
                "[parser-reg] FAIL: success should clear error state, got line=%d col=%d msg=%s\n",
                err.line,
                err.column,
                err.message);
        lexer_free_tokens(&tokens);
        return 0;
    }

    lexer_free_tokens(&tokens);
    return 1;
}

static char *build_deep_assignment_source(size_t chain_len) {
    const char *prefix = "int main(){int a;a";
    const char *suffix = "=1;return a;}";
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);
    size_t total_len = prefix_len + chain_len * 2 + suffix_len + 1;
    char *source = (char *)malloc(total_len);
    size_t pos = 0;
    size_t i;

    if (!source) {
        return NULL;
    }

    memcpy(source + pos, prefix, prefix_len);
    pos += prefix_len;

    for (i = 0; i < chain_len; ++i) {
        source[pos++] = '=';
        source[pos++] = 'a';
    }

    memcpy(source + pos, suffix, suffix_len);
    pos += suffix_len;
    source[pos] = '\0';
    return source;
}

static int test_deep_assignment_hits_recursion_limit(void) {
    TokenArray tokens;
    ParserError err;
    char *source = build_deep_assignment_source(3000);

    if (!source) {
        fprintf(stderr, "[parser-reg] FAIL: failed to allocate deep assignment source\n");
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for deep assignment input\n");
        free(source);
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted excessively deep assignment chain\n");
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "recursion limit") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected recursion-limit diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    lexer_free_tokens(&tokens);
    free(source);
    return 1;
}

static char *build_deep_block_source(size_t depth) {
    const char *prefix = "int main(){";
    const char *middle = "return 0;";
    const char *suffix = "}";
    size_t prefix_len = strlen(prefix);
    size_t middle_len = strlen(middle);
    size_t suffix_len = strlen(suffix);
    size_t total_len = prefix_len + depth + middle_len + depth + suffix_len + 1;
    char *source = (char *)malloc(total_len);
    size_t pos = 0;
    size_t i;

    if (!source) {
        return NULL;
    }

    memcpy(source + pos, prefix, prefix_len);
    pos += prefix_len;

    for (i = 0; i < depth; ++i) {
        source[pos++] = '{';
    }

    memcpy(source + pos, middle, middle_len);
    pos += middle_len;

    for (i = 0; i < depth; ++i) {
        source[pos++] = '}';
    }

    memcpy(source + pos, suffix, suffix_len);
    pos += suffix_len;
    source[pos] = '\0';
    return source;
}

static int test_deep_statement_nesting_hits_recursion_limit(void) {
    TokenArray tokens;
    ParserError err;
    char *source = build_deep_block_source(3000);

    if (!source) {
        fprintf(stderr, "[parser-reg] FAIL: failed to allocate deep block source\n");
        return 0;
    }

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-reg] FAIL: lexer failed for deep block input\n");
        free(source);
        return 0;
    }

    if (parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
                "[parser-reg] FAIL: parser unexpectedly accepted excessively deep statement nesting\n");
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "recursion limit") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected recursion-limit diagnostic for statements, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "statement") == NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: expected statement recursion diagnostic, got: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    if (strstr(err.message, "unary-expression") != NULL) {
        fprintf(stderr,
                "[parser-reg] FAIL: statement nesting should not be reported as unary-expression: %s\n",
                err.message);
        lexer_free_tokens(&tokens);
        free(source);
        return 0;
    }

    lexer_free_tokens(&tokens);
    free(source);
    return 1;
}

int main(void) {
    if (!test_reject_stream_without_eof()) {
        return 1;
    }
    if (!test_backtracking_error_not_polluted()) {
        return 1;
    }
    if (!test_farthest_failure_preferred()) {
        return 1;
    }
    if (!test_success_clears_error_state()) {
        return 1;
    }
    if (!test_deep_assignment_hits_recursion_limit()) {
        return 1;
    }
    if (!test_deep_statement_nesting_hits_recursion_limit()) {
        return 1;
    }

    printf("[parser-reg] All parser regressions passed.\n");
    return 0;
}
