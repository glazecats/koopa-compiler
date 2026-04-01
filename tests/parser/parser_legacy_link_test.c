#include "lexer.h"
#include "parser.h"

#include <stdio.h>

int main(void) {
    const char *source = "int x;\n";
    TokenArray tokens;
    ParserError err;

    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source, &tokens)) {
        fprintf(stderr, "[parser-legacy] lexer failed\n");
        return 1;
    }

    if (!parser_parse_translation_unit(&tokens, &err)) {
        fprintf(stderr,
            "[parser-legacy] parser failed at %d:%d: %s\n",
            err.line,
            err.column,
            err.message);
        lexer_free_tokens(&tokens);
        return 1;
    }

    lexer_free_tokens(&tokens);
    printf("[parser-legacy] old parser API link smoke passed.\n");
    return 0;
}
