#ifndef COMPILER_BACKEND_H
#define COMPILER_BACKEND_H

#include "compiler.h"
#include "machine/ir.h"

int compiler_emit_riscv_preview_text_from_report(
    const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);
int compiler_emit_riscv_preview_text_from_report_simple(
    const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);

int compiler_simple_backend_emit_from_value_ssa(
    const ValueSsaProgram *program,
    char **out_text,
    CompilerError *error);

#endif
