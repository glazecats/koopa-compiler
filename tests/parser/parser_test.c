#include "lexer.h"
#include "parser.h"
#include "semantic.h"

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
    int dump_ast = 0;
    char *source_text;
    TokenArray tokens;
    lexer_init_tokens(&tokens);
    AstProgram program;
    ParserError error;
    SemanticError semantic_error;
    size_t i;

    ast_program_init(&program);

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <source-file> [--dump-tokens] [--dump-ast]\n", argv[0]);
        return 1;
    }

    input_path = argv[1];
    for (i = 2; i < (size_t)argc; ++i) {
        if (strcmp(argv[i], "--dump-tokens") == 0) {
            dump_tokens = 1;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            fprintf(stderr, "Usage: %s <source-file> [--dump-tokens] [--dump-ast]\n", argv[0]);
            return 1;
        }
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

    if (!parser_parse_translation_unit_ast(&tokens, &program, &error)) {
        fprintf(stderr, "[parser] Parse failed at %d:%d: %s\n", error.line, error.column, error.message);
        ast_program_free(&program);
        lexer_free_tokens(&tokens);
        free(source_text);
        return 1;
    }

    if (!semantic_analyze_program(&program, &semantic_error)) {
        fprintf(stderr,
                "[semantic] Analyze failed at %d:%d: %s\n",
                semantic_error.line,
                semantic_error.column,
                semantic_error.message);
        ast_program_free(&program);
        lexer_free_tokens(&tokens);
        free(source_text);
        return 1;
    }

    if (dump_ast) {
        printf("[ast] top-level externals: %zu\n", program.count);
        for (i = 0; i < program.count; ++i) {
            const AstExternal *ext = &program.externals[i];
            printf("[ast] %zu: %s", i, ast_external_kind_name(ext->kind));
            if (ext->name && ext->name_length > 0) {
                printf(" name=%s", ext->name);
            }
            if (ext->kind == AST_EXTERNAL_FUNCTION) {
                printf(" params=%zu", ext->parameter_count);
                printf(" def=%d", ext->is_function_definition);
                printf(" returns=%zu", ext->return_statement_count);
            } else if (ext->kind == AST_EXTERNAL_DECLARATION) {
                printf(" init=%d", ext->has_initializer);
            }
            if (ext->line > 0) {
                printf(" at %d:%d", ext->line, ext->column);
            }
            putchar('\n');
        }
    }

    printf("[parser] Parse succeeded.\n");
    printf("[semantic] Analyze succeeded.\n");

    ast_program_free(&program);
    lexer_free_tokens(&tokens);
    free(source_text);
    return 0;
}