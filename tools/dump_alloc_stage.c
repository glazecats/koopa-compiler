#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_alloc.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOOL_COLOR_BUDGET 8u

typedef enum {
    TOOL_STAGE_INITIAL = 0,
    TOOL_STAGE_FINAL,
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
    fprintf(stderr, "usage: %s <initial|final> <input.sy-or-c>\n", argv0);
}

static int tool_parse_stage(const char *text, ToolStage *out_stage) {
    if (!text || !out_stage) {
        return 0;
    }
    if (strcmp(text, "initial") == 0) {
        *out_stage = TOOL_STAGE_INITIAL;
        return 1;
    }
    if (strcmp(text, "final") == 0) {
        *out_stage = TOOL_STAGE_FINAL;
        return 1;
    }
    return 0;
}

static int tool_build_value_ssa_program(const char *source_path, ValueSsaProgram *out_program) {
    char *source = NULL;
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
    ValueSsaError value_error;
    int ok = 0;

    lexer_init_tokens(&tokens);
    ast_program_init(&ast);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
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

    source = tool_read_file(source_path);
    if (!source) {
        fprintf(stderr, "failed to read %s\n", source_path ? source_path : "<null>");
        goto cleanup;
    }

    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast, &parser_error) ||
        !semantic_analyze_program_with_options(&ast, &semantic_options, &semantic_error) ||
        !ir_lower_program(&ast, &ir_options, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, out_program, &value_error) ||
        !value_ssa_optimize_perf_hotspots(out_program, &value_error)) {
        fprintf(stderr, "pipeline failed for %s\n", source_path ? source_path : "<null>");
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(source);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return ok;
}

int main(int argc, char **argv) {
    ToolStage stage;
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    ValueSsaAllocationPrep prep;
    ValueSsaError error;
    char *text = NULL;
    int ok = 0;

    if (argc != 3) {
        tool_print_usage(argv[0]);
        return 2;
    }
    if (!tool_parse_stage(argv[1], &stage)) {
        tool_print_usage(argv[0]);
        return 2;
    }

    value_ssa_program_init(&program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    value_ssa_allocation_prep_init(&prep);
    memset(&error, 0, sizeof(error));

    if (!tool_build_value_ssa_program(argv[2], &program)) {
        value_ssa_program_free(&program);
        return 1;
    }

    switch (stage) {
        case TOOL_STAGE_INITIAL:
            ok = value_ssa_allocate_program(&program, TOOL_COLOR_BUDGET, &result, &error) &&
                value_ssa_dump_program_allocation_result(&program, &result, &text, &error);
            break;
        case TOOL_STAGE_FINAL:
            ok = value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
                    &program, TOOL_COLOR_BUDGET, &result, &stats, &error) &&
                value_ssa_dump_program_allocation_result(&program, &result, &text, &error);
            break;
        default:
            ok = 0;
            break;
    }

    if (ok && text) {
        fputs(text, stdout);
    }

    for (size_t function_index = 0; function_index < program.function_count; ++function_index) {
        const ValueSsaFunction *function = &program.functions[function_index];
        char *prep_text = NULL;

        if (!function->has_body) {
            continue;
        }
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &prep, &error)) {
            continue;
        }
        if (value_ssa_dump_allocation_prep(function, &prep, &prep_text, &error)) {
            fprintf(stderr, "%s", prep_text);
        }
        free(prep_text);
        value_ssa_allocation_prep_free(&prep);
    }

    free(text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok ? 0 : 1;
}
