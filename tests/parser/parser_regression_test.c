#include "lexer.h"
#include "parser.h"

#include <stdio.h>
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

int main(void) {
    if (!test_reject_stream_without_eof()) {
        return 1;
    }

    printf("[parser-reg] All parser regressions passed.\n");
    return 0;
}
