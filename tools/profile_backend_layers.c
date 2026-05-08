#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "lower_ir.h"
#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "machine/ir.h"
#include "machine/select.h"
#include "machine/layout.h"
#include "machine/emit.h"
#include "machine/bytes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    char *buffer = NULL;
    long size;

    if (!fp) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }
    buffer = (char *)malloc((size_t)size + 1u);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    if (fread(buffer, 1u, (size_t)size, fp) != (size_t)size) {
        free(buffer);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    buffer[size] = '\0';
    return buffer;
}

int main(int argc, char **argv) {
    const char *path;
    char *source = NULL;
    TokenArray tokens;
    AstProgram ast;
    ParserError parser_error;
    SemanticError semantic_error;
    IrProgram ir_program;
    IrError ir_error;
    LowerIrProgram lower_program;
    LowerIrError lower_error;
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
    MachineSelectFunctionSummary select_summary;
    size_t select_function_count = 0;
    size_t function_index;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <input.sy>\n", argv[0]);
        return 2;
    }
    path = argv[1];

    memset(&parser_error, 0, sizeof(parser_error));
    memset(&semantic_error, 0, sizeof(semantic_error));
    memset(&ir_error, 0, sizeof(ir_error));
    memset(&lower_error, 0, sizeof(lower_error));
    memset(&value_error, 0, sizeof(value_error));
    memset(&machine_ir_error, 0, sizeof(machine_ir_error));
    memset(&select_error, 0, sizeof(select_error));
    memset(&layout_error, 0, sizeof(layout_error));
    memset(&emit_error, 0, sizeof(emit_error));
    memset(&bytes_error, 0, sizeof(bytes_error));

    lexer_init_tokens(&tokens);
    ast_program_init(&ast);
    ir_program_init(&ir_program);
    lower_ir_program_init(&lower_program);
    value_ssa_program_init(&value_program);
    machine_ir_allocate_rewrite_report_init(&machine_report);
    machine_select_program_init(&select_program);
    machine_layout_program_init(&layout_program);
    machine_emit_program_init(&emit_program);
    machine_bytes_report_init(&bytes_report);

    source = read_text_file(path);
    if (!source) {
        fprintf(stderr, "failed to read %s\n", path);
        return 1;
    }
    if (!lexer_tokenize(source, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast, &parser_error) ||
        !semantic_analyze_program(&ast, &semantic_error) ||
        !ir_lower_program(&ast, NULL, &ir_program, &ir_error) ||
        !lower_ir_lower_from_ir(&ir_program, NULL, &lower_program, &lower_error) ||
        !value_ssa_build_default_from_lower_ir(&lower_program, &value_program, &value_error) ||
        !machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
            &value_program, 8u, 8u, &machine_report, &machine_ir_error) ||
        !machine_select_build_program_from_machine_ir_report(&machine_report, &select_program, &select_error) ||
        !machine_layout_lower_program_from_machine_select(&select_program, &layout_program, &layout_error) ||
        !machine_emit_lower_program_from_machine_layout(&layout_program, &emit_program, &emit_error) ||
        !machine_bytes_build_report_from_machine_emit_program_with_profile(
            &emit_program, MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW, &bytes_report, &bytes_error)) {
        fprintf(stderr, "pipeline failed\n");
        free(source);
        return 1;
    }

    if (!machine_select_program_get_summary(&select_program, NULL, NULL, &select_function_count) ||
        select_function_count == 0) {
        fprintf(stderr, "missing machine_select summary count\n");
        free(source);
        return 1;
    }
    printf("machine_select function_count=%zu\n", select_function_count);
    for (function_index = 0; function_index < select_function_count; ++function_index) {
        const MachineSelectFunction *select_function = NULL;
        const MachineBytesFunctionSummary *bytes_summary = NULL;

        if (!machine_select_program_get_function(&select_program, function_index, &select_function) ||
            !select_function ||
            !machine_select_program_get_function_summary(&select_program, function_index, &select_summary)) {
            fprintf(stderr, "missing machine_select summary for function %zu\n", function_index);
            free(source);
            return 1;
        }
        if (!machine_bytes_report_get_function_summary_artifact(&bytes_report, function_index, &bytes_summary) ||
            !bytes_summary) {
            fprintf(stderr, "missing machine_bytes summary for function %zu\n", function_index);
            free(source);
            return 1;
        }

        printf("function[%zu] %s: select ops=%zu calls=%zu alu=%zu alui=%zu cmp=%zu cmpi=%zu load_global=%zu store_global=%zu spills=%d | bytes=%zu calls=%zu blocks=%zu spill-slots=%zu\n",
            function_index,
            select_function->name ? select_function->name : "<anon>",
            select_summary.op_count,
            select_summary.call_count + select_summary.call_imm_count + select_summary.call_spill_count +
                select_summary.call_imm_spill_count + select_summary.call_void_count + select_summary.call_void_imm_count,
            select_summary.alu_count,
            select_summary.alu_imm_count,
            select_summary.cmp_count,
            select_summary.cmp_imm_count,
            select_summary.load_global_count,
            select_summary.store_global_count + select_summary.store_global_imm_count,
            select_summary.has_spills,
            bytes_summary->byte_count,
            bytes_summary->call_count,
            bytes_summary->block_count,
            bytes_report.program.functions[function_index].spill_slot_count);
    }

    free(source);
    machine_bytes_report_free(&bytes_report);
    machine_emit_program_free(&emit_program);
    machine_layout_program_free(&layout_program);
    machine_select_program_free(&select_program);
    machine_ir_allocate_rewrite_report_free(&machine_report);
    value_ssa_program_free(&value_program);
    lower_ir_program_free(&lower_program);
    ir_program_free(&ir_program);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return 0;
}
