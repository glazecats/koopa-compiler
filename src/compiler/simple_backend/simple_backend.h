#ifndef SIMPLE_BACKEND_H
#define SIMPLE_BACKEND_H

#include "compiler.h"
#include "value_ssa.h"

int compiler_simple_backend_emit_from_value_ssa(
    const ValueSsaProgram *program,
    char **out_text,
    CompilerError *error);

#endif
