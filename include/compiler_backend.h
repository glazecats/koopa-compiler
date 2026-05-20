#ifndef COMPILER_BACKEND_H
#define COMPILER_BACKEND_H

#include "compiler.h"
#include "machine/ir.h"

int compiler_emit_riscv_preview_text_from_report(
    const MachineIrAllocateRewriteReport *report,
    CompilerMode mode,
    char **out_text,
    CompilerError *error);

#endif
