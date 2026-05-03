#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compiler_write_text_file(const char *path, const char *text) {
    FILE *fp;
    size_t length;

    if (!path || !text) {
        return 0;
    }
    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    length = strlen(text);
    if (length > 0u && fwrite(text, 1u, length, fp) != length) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static void compiler_print_usage(const char *argv0) {
    fprintf(stderr, "Usage: %s -riscv <input-file> -o <output-file>\n", argv0);
    fprintf(stderr, "       %s -perf <input-file> -o <output-file>\n", argv0);
}

int main(int argc, char **argv) {
    CompilerMode mode;
    CompilerError error;
    const char *input_path;
    const char *output_path;
    char *output_text = NULL;

    memset(&error, 0, sizeof(error));
    if (argc != 5) {
        compiler_print_usage(argv[0]);
        return 1;
    }
    if (!compiler_mode_from_flag(argv[1], &mode)) {
        fprintf(stderr, "Unsupported mode: %s\n", argv[1]);
        compiler_print_usage(argv[0]);
        return 1;
    }
    input_path = argv[2];
    if (strcmp(argv[3], "-o") != 0) {
        fprintf(stderr, "Expected -o before output path, got: %s\n", argv[3]);
        compiler_print_usage(argv[0]);
        return 1;
    }
    output_path = argv[4];

    if (!compiler_compile_file(input_path, mode, &output_text, &error)) {
        fprintf(stderr,
            "[compiler] FAIL");
        if (error.line > 0 || error.column > 0) {
            fprintf(stderr, " at %d:%d", error.line, error.column);
        }
        if (error.message[0] != '\0') {
            fprintf(stderr, ": %s", error.message);
        }
        fputc('\n', stderr);
        free(output_text);
        return 1;
    }
    if (!compiler_write_text_file(output_path, output_text ? output_text : "")) {
        fprintf(stderr, "[compiler] FAIL: could not write output file: %s\n", output_path);
        free(output_text);
        return 1;
    }

    free(output_text);
    return 0;
}
