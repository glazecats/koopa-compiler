#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "memory_ssa_pass.h"
#include "machine/bytes.h"
#include "machine/emit.h"
#include "machine/ir.h"
#include "machine/select.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOOL_COLOR_BUDGET 8u
#define TOOL_MACHINE_REGISTER_COUNT 8u

typedef enum {
    TOOL_STAGE_MACHINE_IR = 0,
    TOOL_STAGE_MACHINE_IR_RAW,
    TOOL_STAGE_MACHINE_IR_PHI,
    TOOL_STAGE_MACHINE_IR_CLEANED,
    TOOL_STAGE_SELECT,
    TOOL_STAGE_EMIT,
    TOOL_STAGE_BYTES,
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
        "usage: %s <machine-ir|machine-ir-raw|machine-ir-phi|machine-ir-cleaned|select|emit|bytes> <input.sy>\n",
        argv0);
}

static int tool_parse_stage(const char *text, ToolStage *out_stage) {
    if (!text || !out_stage) {
        return 0;
    }
    if (strcmp(text, "machine-ir") == 0) {
        *out_stage = TOOL_STAGE_MACHINE_IR;
        return 1;
    }
    if (strcmp(text, "machine-ir-raw") == 0) {
        *out_stage = TOOL_STAGE_MACHINE_IR_RAW;
        return 1;
    }
    if (strcmp(text, "machine-ir-phi") == 0) {
        *out_stage = TOOL_STAGE_MACHINE_IR_PHI;
        return 1;
    }
    if (strcmp(text, "machine-ir-cleaned") == 0) {
        *out_stage = TOOL_STAGE_MACHINE_IR_CLEANED;
        return 1;
    }
    if (strcmp(text, "select") == 0) {
        *out_stage = TOOL_STAGE_SELECT;
        return 1;
    }
    if (strcmp(text, "emit") == 0) {
        *out_stage = TOOL_STAGE_EMIT;
        return 1;
    }
    if (strcmp(text, "bytes") == 0) {
        *out_stage = TOOL_STAGE_BYTES;
        return 1;
    }
    return 0;
}

static int tool_build_machine_ir_report(const char *source_path,
    ToolStage stage,
    MachineIrAllocateRewriteReport *out_report) {
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
    ValueSsaProgram value_program;
    ValueSsaError value_error;
    MachineIrError machine_ir_error;
    int ok = 0;

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
    memset(&machine_ir_error, 0, sizeof(machine_ir_error));

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
        !value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error) ||
        !memory_ssa_pass_scalar_replace_local_slots(&value_program, &value_error) ||
        !memory_ssa_pass_scalar_replace_global_slots(&value_program, &value_error) ||
        !value_ssa_optimize_perf_hotspots(&value_program, &value_error)) {
        fprintf(stderr, "pipeline failed for %s\n", source_path ? source_path : "<null>");
        goto cleanup;
    }

    switch (stage) {
        case TOOL_STAGE_MACHINE_IR_RAW:
            ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
                &value_program,
                TOOL_COLOR_BUDGET,
                TOOL_MACHINE_REGISTER_COUNT,
                out_report,
                &machine_ir_error);
            break;
        case TOOL_STAGE_MACHINE_IR_PHI:
            ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
                &value_program,
                TOOL_COLOR_BUDGET,
                TOOL_MACHINE_REGISTER_COUNT,
                out_report,
                &machine_ir_error);
            break;
        case TOOL_STAGE_MACHINE_IR:
        case TOOL_STAGE_MACHINE_IR_CLEANED:
        case TOOL_STAGE_SELECT:
        case TOOL_STAGE_EMIT:
        case TOOL_STAGE_BYTES:
            ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
                &value_program,
                TOOL_COLOR_BUDGET,
                TOOL_MACHINE_REGISTER_COUNT,
                out_report,
                &machine_ir_error);
            break;
        default:
            ok = 0;
            break;
    }
    if (!ok) {
        fprintf(stderr, "pipeline failed for %s\n", source_path ? source_path : "<null>");
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(source);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return ok;
}

int main(int argc, char **argv) {
    ToolStage stage;
    MachineIrAllocateRewriteReport machine_report;
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

    machine_ir_allocate_rewrite_report_init(&machine_report);
    if (!tool_build_machine_ir_report(argv[2], stage, &machine_report)) {
        machine_ir_allocate_rewrite_report_free(&machine_report);
        return 1;
    }

    switch (stage) {
        case TOOL_STAGE_MACHINE_IR: {
            MachineIrError error;

            memset(&error, 0, sizeof(error));
            ok = machine_ir_dump_allocate_rewrite_report(&machine_report, &text, &error);
            break;
        }
        case TOOL_STAGE_MACHINE_IR_RAW: {
            MachineIrError error;

            memset(&error, 0, sizeof(error));
            ok = machine_ir_dump_allocate_rewrite_report(&machine_report, &text, &error);
            break;
        }
        case TOOL_STAGE_MACHINE_IR_PHI: {
            MachineIrError error;

            memset(&error, 0, sizeof(error));
            ok = machine_ir_dump_allocate_rewrite_report(&machine_report, &text, &error);
            break;
        }
        case TOOL_STAGE_MACHINE_IR_CLEANED: {
            MachineIrError error;

            memset(&error, 0, sizeof(error));
            ok = machine_ir_dump_allocate_rewrite_report(&machine_report, &text, &error);
            break;
        }
        case TOOL_STAGE_SELECT: {
            MachineSelectError error;

            memset(&error, 0, sizeof(error));
            ok = machine_select_dump_report_from_machine_ir_report(&machine_report, &text, &error);
            break;
        }
        case TOOL_STAGE_EMIT: {
            MachineEmitProgram emit_program;
            MachineEmitError error;

            machine_emit_program_init(&emit_program);
            memset(&error, 0, sizeof(error));
            ok = machine_emit_build_program_from_machine_ir_report(&machine_report, &emit_program, &error) &&
                machine_emit_dump_program(&emit_program, &text, &error);
            machine_emit_program_free(&emit_program);
            break;
        }
        case TOOL_STAGE_BYTES: {
            MachineBytesReport report;
            MachineBytesError error;

            machine_bytes_report_init(&report);
            memset(&error, 0, sizeof(error));
            ok = machine_bytes_build_report_from_machine_ir_report_with_profile(
                    &machine_report,
                    MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
                    &report,
                    &error) &&
                machine_bytes_dump_report(&report, &text, &error);
            machine_bytes_report_free(&report);
            break;
        }
        default:
            ok = 0;
            break;
    }

    if (!ok || !text) {
        free(text);
        machine_ir_allocate_rewrite_report_free(&machine_report);
        return 1;
    }

    fputs(text, stdout);
    free(text);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    return 0;
}
