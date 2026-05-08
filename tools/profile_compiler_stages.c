#include "compiler.h"
#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "machine/bytes.h"
#include "machine/emit.h"
#include "machine/ir.h"
#include "machine/layout.h"
#include "machine/select.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define TOOL_COLOR_BUDGET 8u
#define TOOL_MACHINE_REGISTER_COUNT 8u

static double tool_now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static char *tool_read_text_file(const char *path) {
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

int main(int argc, char **argv) {
    const char *input_path = NULL;
    char *source = NULL;
    char *riscv_text = NULL;
    char *perf_text = NULL;
    TokenArray tokens;
    AstProgram ast_program;
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
    MachineIrAllocateRewriteReport machine_report;
    MachineIrError machine_ir_error;
    MachineSelectProgram select_program;
    MachineSelectError select_error;
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    MachineBytesReport bytes_report;
    MachineBytesError bytes_error;
    CompilerError compiler_error;
    double t0 = 0.0;
    double t1 = 0.0;
    double measured_sum = 0.0;
    double total_riscv = 0.0;
    double total_perf = 0.0;
    int ok = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <input.sy>\n", argv[0]);
        return 2;
    }
    input_path = argv[1];

    lexer_init_tokens(&tokens);
    ast_program_init(&ast_program);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);
    machine_bytes_report_init(&bytes_report);
    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&semantic_options, 0, sizeof(semantic_options));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&ir_options, 0, sizeof(ir_options));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&lower_options, 0, sizeof(lower_options));
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_ir_error, 0, sizeof(machine_ir_error));
    memset(&select_error, 0, sizeof(select_error));
    memset(&layout_error, 0, sizeof(layout_error));
    memset(&emit_error, 0, sizeof(emit_error));
    memset(&bytes_error, 0, sizeof(bytes_error));
    memset(&compiler_error, 0, sizeof(compiler_error));

    semantic_options.skip_all_paths_return_check = 1;
    ir_options.allow_implicit_fallthrough_return = 1;
    lower_options.allow_implicit_fallthrough_return = 1;

    source = tool_read_text_file(input_path);
    if (!source) {
        fprintf(stderr, "failed to read %s\n", input_path);
        ok = 1;
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = lexer_tokenize(source, &tokens);
    t1 = tool_now_s();
    printf("lexer %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = parser_parse_translation_unit_ast(&tokens, &ast_program, &parser_error);
    t1 = tool_now_s();
    printf("parser %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = semantic_analyze_program_with_options(&ast_program, &semantic_options, &semantic_error);
    t1 = tool_now_s();
    printf("semantic %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = ir_lower_program(&ast_program, &ir_options, &ir_program, &ir_error);
    t1 = tool_now_s();
    printf("ir_lower %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = lower_ir_lower_from_ir(&ir_program, &lower_options, &lower_program, &lower_error);
    t1 = tool_now_s();
    printf("lower_ir %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error);
    t1 = tool_now_s();
    printf("value_ssa %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
        &value_program,
        TOOL_COLOR_BUDGET,
        TOOL_MACHINE_REGISTER_COUNT,
        &machine_report,
        &machine_ir_error);
    t1 = tool_now_s();
    printf("machine_ir_report %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = machine_select_build_program_from_machine_ir_report(&machine_report, &select_program, &select_error);
    t1 = tool_now_s();
    printf("machine_select %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error);
    t1 = tool_now_s();
    printf("machine_layout %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error);
    t1 = tool_now_s();
    printf("machine_emit %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = machine_bytes_build_report_from_machine_emit_program_with_profile(
        &emit_program,
        MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
        &bytes_report,
        &bytes_error);
    t1 = tool_now_s();
    printf("machine_bytes %.3f\n", t1 - t0);
    measured_sum += t1 - t0;
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = compiler_compile_source_text(source, COMPILER_MODE_RISCV, &riscv_text, &compiler_error);
    t1 = tool_now_s();
    total_riscv = t1 - t0;
    printf("full_riscv %.3f\n", total_riscv);
    if (!ok) {
        goto cleanup;
    }

    t0 = tool_now_s();
    ok = compiler_compile_source_text(source, COMPILER_MODE_PERF, &perf_text, &compiler_error);
    t1 = tool_now_s();
    total_perf = t1 - t0;
    printf("full_perf %.3f\n", total_perf);
    if (!ok) {
        goto cleanup;
    }

    printf("measured_stage_sum %.3f\n", measured_sum);
    printf("estimated_post_machine_bytes_riscv %.3f\n", total_riscv > measured_sum ? total_riscv - measured_sum : 0.0);
    printf("estimated_post_machine_bytes_perf %.3f\n", total_perf > measured_sum ? total_perf - measured_sum : 0.0);
    ok = 0;

cleanup:
    if (ok != 0) {
        if (compiler_error.message[0] != '\0') {
            fprintf(stderr, "compiler error: %s\n", compiler_error.message);
        }
    }
    free(source);
    free(riscv_text);
    free(perf_text);
    machine_bytes_report_free(&bytes_report);
    machine_emit_program_free(&emit_program);
    machine_layout_program_free(&layout_program);
    machine_select_program_free(&select_program);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast_program);
    lexer_free_tokens(&tokens);
    return ok;
}
