#define _POSIX_C_SOURCE 200809L

#include "compiler_backend.h"

#include "machine/ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int simple_backend_build_simple_mode_text(
    const ValueSsaProgram *program,
    char **out_text,
    CompilerError *error) {
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    int ok = 0;

    if (!program || !out_text) {
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    memset(&machine_error, 0, sizeof(machine_error));

    if (!machine_ir_build_all_spill_flat_program_only_report(
            program,
            8u,
            &report,
            &machine_error)) {
        if (error) {
            error->line = machine_error.line;
            error->column = machine_error.column;
            if (machine_error.message[0] != '\0') {
                snprintf(error->message, sizeof(error->message), "%s", machine_error.message);
            }
        }
        goto cleanup;
    }

    if (!compiler_emit_riscv_preview_text_from_report_simple(&report, COMPILER_MODE_RISCV, out_text, error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int compiler_simple_backend_emit_from_value_ssa(
    const ValueSsaProgram *program,
    char **out_text,
    CompilerError *error) {
    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "simple backend: invalid contract");
        }
        return 0;
    }
    return simple_backend_build_simple_mode_text(program, out_text, error);
}
