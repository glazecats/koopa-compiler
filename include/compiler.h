#ifndef COMPILER_H
#define COMPILER_H

#include "ast.h"
#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"

typedef enum {
    COMPILER_MODE_RISCV = 0,
    COMPILER_MODE_PERF,
    COMPILER_MODE_EXTENSION,
} CompilerMode;

typedef struct {
    int line;
    int column;
    char message[512];
} CompilerError;

typedef struct {
    int skip_all_paths_return_check;
} CompilerOptions;

int compiler_mode_from_flag(const char *flag, CompilerMode *out_mode);
int compiler_frontend_lower_to_ir_for_testing(const char *source,
    CompilerMode mode,
    const CompilerOptions *options,
    TokenArray *tokens,
    AstProgram *ast_program,
    IrProgram *ir_program,
    CompilerError *error);
int compiler_frontend_lower_to_lower_ir_for_testing(const char *source,
    CompilerMode mode,
    const CompilerOptions *options,
    TokenArray *tokens,
    AstProgram *ast_program,
    IrProgram *ir_program,
    LowerIrProgram *lower_program,
    CompilerError *error);
int compiler_compile_source_text_with_options(const char *source,
    CompilerMode mode,
    const CompilerOptions *options,
    char **out_text,
    CompilerError *error);
int compiler_compile_source_text(const char *source,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);
int compiler_compile_file_with_options(const char *input_path,
    CompilerMode mode,
    const CompilerOptions *options,
    char **out_text,
    CompilerError *error);
int compiler_compile_file(const char *input_path,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);

#endif
