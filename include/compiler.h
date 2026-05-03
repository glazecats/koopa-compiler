#ifndef COMPILER_H
#define COMPILER_H

typedef enum {
    COMPILER_MODE_RISCV = 0,
    COMPILER_MODE_PERF,
} CompilerMode;

typedef struct {
    int line;
    int column;
    char message[512];
} CompilerError;

int compiler_mode_from_flag(const char *flag, CompilerMode *out_mode);
int compiler_compile_source_text(const char *source,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);
int compiler_compile_file(const char *input_path,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);

#endif
