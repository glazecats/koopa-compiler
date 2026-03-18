#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_file_text(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "Failed to seek file: %s\n", path);
        fclose(fp);
        return NULL;
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        fprintf(stderr, "Failed to get file size: %s\n", path);
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to rewind file: %s\n", path);
        fclose(fp);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Out of memory while reading: %s\n", path);
        fclose(fp);
        return NULL;
    }

    size_t nread = fread(buffer, 1, (size_t)file_size, fp);
    if (nread != (size_t)file_size) {
        fprintf(stderr, "Failed to read complete file: %s\n", path);
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[file_size] = '\0';
    fclose(fp);
    return buffer;
}

static void print_token(const Token *t) {
    printf("%d:%d %-12s", t->line, t->column, lexer_token_type_name(t->type));

    if (t->length > 0) {
        printf(" lexeme=\"%.*s\"", (int)t->length, t->lexeme);
    }

    if (t->type == TOKEN_NUMBER) {
        printf(" value=%lld", t->number_value);
    }

    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source-file>\n", argv[0]);
        return 1;
    }

    char *source_text = read_file_text(argv[1]);
    if (!source_text) {
        return 1;
    }

    TokenArray tokens;
    lexer_init_tokens(&tokens);
    if (!lexer_tokenize(source_text, &tokens)) {
        free(source_text);
        return 1;
    }

    for (size_t i = 0; i < tokens.size; ++i) {
        print_token(&tokens.data[i]);
    }

    lexer_free_tokens(&tokens);
    free(source_text);
    return 0;
}
