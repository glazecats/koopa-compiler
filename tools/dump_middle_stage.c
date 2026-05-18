#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "memory_ssa_pass.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOOL_STAGE_IR = 0,
    TOOL_STAGE_LOWER_IR,
    TOOL_STAGE_VALUE_SSA_DIRECT,
    TOOL_STAGE_VALUE_SSA_CLASSIC,
    TOOL_STAGE_VALUE_SSA_MEMORY_VALUE,
    TOOL_STAGE_VALUE_SSA_MEMORY_FULL,
    TOOL_STAGE_VALUE_SSA_DEFAULT,
    TOOL_STAGE_VALUE_SSA_PERF,
} ToolStage;

static char *tool_read_file(const char *path) {
    FILE *fp = NULL;
    char *buffer = NULL;
    long size = 0;

    if (!path) {
        return NULL;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    if (size > 0 && fread(buffer, 1u, (size_t)size, fp) != (size_t)size) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    buffer[size] = '\0';
    return buffer;
}

static void tool_print_usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s <ir|lower-ir|value-ssa-direct|value-ssa-classic|value-ssa-memory-value|"
        "value-ssa-memory-full|value-ssa-default|value-ssa-perf> <input.sy>\n",
        argv0);
}

static int tool_parse_stage(const char *text, ToolStage *out_stage) {
    if (!text || !out_stage) {
        return 0;
    }
    if (strcmp(text, "ir") == 0) {
        *out_stage = TOOL_STAGE_IR;
        return 1;
    }
    if (strcmp(text, "lower-ir") == 0) {
        *out_stage = TOOL_STAGE_LOWER_IR;
        return 1;
    }
    if (strcmp(text, "value-ssa-direct") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_DIRECT;
        return 1;
    }
    if (strcmp(text, "value-ssa-classic") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_CLASSIC;
        return 1;
    }
    if (strcmp(text, "value-ssa-memory-value") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_MEMORY_VALUE;
        return 1;
    }
    if (strcmp(text, "value-ssa-memory-full") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_MEMORY_FULL;
        return 1;
    }
    if (strcmp(text, "value-ssa-default") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_DEFAULT;
        return 1;
    }
    if (strcmp(text, "value-ssa-perf") == 0) {
        *out_stage = TOOL_STAGE_VALUE_SSA_PERF;
        return 1;
    }
    return 0;
}

static void tool_print_ir_summary(const IrProgram *program) {
    size_t function_index;
    size_t block_total = 0u;
    size_t instruction_total = 0u;

    if (!program) {
        return;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const IrFunction *function = &program->functions[function_index];
        size_t block_index;

        block_total += function->block_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            instruction_total += function->blocks[block_index].instruction_count;
        }
    }

    printf("# summary ir functions=%zu blocks=%zu instructions=%zu\n",
        program->function_count,
        block_total,
        instruction_total);
}

static void tool_print_lower_ir_summary(const LowerIrProgram *program) {
    size_t function_index;
    size_t block_total = 0u;
    size_t instruction_total = 0u;
    size_t load_local_total = 0u;
    size_t store_local_total = 0u;

    if (!program) {
        return;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const LowerIrFunction *function = &program->functions[function_index];
        size_t block_index;

        block_total += function->block_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const LowerIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            instruction_total += block->instruction_count;
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case LOWER_IR_INSTR_LOAD_LOCAL:
                    ++load_local_total;
                    break;
                case LOWER_IR_INSTR_STORE_LOCAL:
                    ++store_local_total;
                    break;
                default:
                    break;
                }
            }
        }
    }

    printf("# summary lower-ir functions=%zu blocks=%zu instructions=%zu load_local=%zu store_local=%zu\n",
        program->function_count,
        block_total,
        instruction_total,
        load_local_total,
        store_local_total);
}

static void tool_print_value_ssa_summary(const ValueSsaProgram *program) {
    size_t function_index;
    size_t block_total = 0u;
    size_t phi_total = 0u;
    size_t instruction_total = 0u;
    size_t load_local_total = 0u;
    size_t store_local_total = 0u;
    size_t mov_total = 0u;
    size_t binary_total = 0u;

    if (!program) {
        return;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        block_total += function->block_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            phi_total += block->phi_count;
            instruction_total += block->instruction_count;
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                case VALUE_SSA_INSTR_MOV:
                    ++mov_total;
                    break;
                case VALUE_SSA_INSTR_BINARY:
                    ++binary_total;
                    break;
                case VALUE_SSA_INSTR_LOAD_LOCAL:
                    ++load_local_total;
                    break;
                case VALUE_SSA_INSTR_STORE_LOCAL:
                    ++store_local_total;
                    break;
                default:
                    break;
                }
            }
        }
    }

    printf("# summary value-ssa functions=%zu blocks=%zu phis=%zu instructions=%zu mov=%zu binary=%zu load_local=%zu store_local=%zu\n",
        program->function_count,
        block_total,
        phi_total,
        instruction_total,
        mov_total,
        binary_total,
        load_local_total,
        store_local_total);
}

int main(int argc, char **argv) {
    ToolStage stage;
    char *source = NULL;
    char *dump_text = NULL;
    TokenArray tokens;
    AstProgram ast;
    ParserError parser_error;
    SemanticError semantic_error;
    SemanticOptions semantic_options;
    IrProgram ir_program;
    IrError ir_error;
    IrLowerOptions ir_options;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
    LowerIrOptions lower_options;
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    int ok = 0;

    if (argc != 3) {
        tool_print_usage(argv[0]);
        return 2;
    }
    if (!tool_parse_stage(argv[1], &stage)) {
        tool_print_usage(argv[0]);
        return 2;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    memset(&value_error, 0, sizeof(value_error));

    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    source = tool_read_file(argv[2]);
    if (!source) {
        fprintf(stderr, "failed to read %s\n", argv[2]);
        goto cleanup;
    }

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast, &parser_error) ||
        !semantic_analyze_program_with_options(&ast, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast, &ir_options, &ir_program, &ir_error)) {
        fprintf(stderr, "failed before ir dump stage\n");
        goto cleanup;
    }

    if (stage == TOOL_STAGE_IR) {
        tool_print_ir_summary(&ir_program);
        ok = ir_dump_program(&ir_program, &dump_text);
        goto emit;
    }

    if (!lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error)) {
        fprintf(stderr, "failed before lower-ir dump stage\n");
        goto cleanup;
    }

    if (stage == TOOL_STAGE_LOWER_IR) {
        tool_print_lower_ir_summary(&lower_program);
        ok = lower_ir_dump_program(&lower_program, &dump_text);
        goto emit;
    }

    switch (stage) {
    case TOOL_STAGE_VALUE_SSA_DIRECT:
        ok = value_ssa_build_from_lower_ir(&lower_program, &value_program, &value_error);
        break;
    case TOOL_STAGE_VALUE_SSA_CLASSIC:
        ok = value_ssa_build_canonicalized_from_lower_ir(&lower_program, &value_program, &value_error);
        break;
    case TOOL_STAGE_VALUE_SSA_MEMORY_VALUE:
        ok = value_ssa_build_memory_value_canonicalized_from_lower_ir(&lower_program, &value_program, &value_error);
        break;
    case TOOL_STAGE_VALUE_SSA_MEMORY_FULL:
        ok = value_ssa_build_memory_canonicalized_from_lower_ir(&lower_program, &value_program, &value_error);
        break;
    case TOOL_STAGE_VALUE_SSA_DEFAULT:
        ok = value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error);
        break;
    case TOOL_STAGE_VALUE_SSA_PERF:
        ok = value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error) &&
            memory_ssa_pass_scalar_replace_local_slots(&value_program, &value_error) &&
            memory_ssa_pass_scalar_replace_global_slots(&value_program, &value_error) &&
            value_ssa_optimize_perf_hotspots(&value_program, &value_error);
        break;
    default:
        ok = 0;
        break;
    }
    if (!ok) {
        fprintf(stderr, "failed before value-ssa dump stage");
        if (value_error.message[0] != '\0') {
            fprintf(stderr, ": %s", value_error.message);
        }
        fputc('\n', stderr);
        goto cleanup;
    }

    tool_print_value_ssa_summary(&value_program);
    ok = value_ssa_dump_program(&value_program, &dump_text);

emit:
    if (!ok || !dump_text) {
        goto cleanup;
    }
    fputs(dump_text, stdout);
    ok = 1;

cleanup:
    free(dump_text);
    free(source);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return ok ? 0 : 1;
}
