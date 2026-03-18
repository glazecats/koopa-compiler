#include "lexer.h"
#include "parser.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char *read_file_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    char *buffer;
    long file_size;
    size_t nread;

    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    nread = fread(buffer, 1, (size_t)file_size, fp);
    fclose(fp);

    if (nread != (size_t)file_size) {
        free(buffer);
        return NULL;
    }

    buffer[file_size] = '\0';
    return buffer;
}

int main(int argc, char **argv) {
    const char *input_path;
    int dump_tokens = 0;
    char *source_text;
    TokenArray tokens;
    lexer_init_tokens(&tokens);
    ParserError error;
    size_t i;

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <source-file> [--dump-tokens]\n", argv[0]);
        return 1;
    }

    input_path = argv[1];
    if (argc == 3) {
        if (strcmp(argv[2], "--dump-tokens") != 0) {
            fprintf(stderr, "Unknown option: %s\n", argv[2]);
            fprintf(stderr, "Usage: %s <source-file> [--dump-tokens]\n", argv[0]);
            return 1;
        }
        dump_tokens = 1;
    }

    source_text = read_file_text(input_path);
    if (!source_text) {
        fprintf(stderr, "Failed to read file: %s\n", input_path);
        return 1;
    }

    if (!lexer_tokenize(source_text, &tokens)) {
        free(source_text);
        return 1;
    }

    printf("[parser] input: %s\n", input_path);
    printf("[parser] token count: %zu\n", tokens.size);

    if (dump_tokens) {
        for (i = 0; i < tokens.size; ++i) {
            const Token *t = &tokens.data[i];
            printf("%d:%d %-12s", t->line, t->column, lexer_token_type_name(t->type));
            if (t->length > 0) {
                printf(" lexeme=\"%.*s\"", (int)t->length, t->lexeme);
            }
            if (t->type == TOKEN_NUMBER) {
                printf(" value=%lld", t->number_value);
            }
            putchar('\n');
        }
    }

    if (!parser_parse_translation_unit(&tokens, &error)) {
        fprintf(stderr, "[parser] Parse failed at %d:%d: %s\n", error.line, error.column, error.message);
        lexer_free_tokens(&tokens);
        free(source_text);
        return 1;
    }

    printf("[parser] Parse succeeded.\n");

    lexer_free_tokens(&tokens);
    free(source_text);
    return 0;
}