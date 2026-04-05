#ifndef IR_PASS_H
#define IR_PASS_H

#include <stddef.h>

#include "ir.h"

typedef int (*IrPassRunFn)(IrProgram *program, IrError *error);

typedef struct {
    const char *name;
    IrPassRunFn run;
} IrPassSpec;

/* Runs passes in order and stops at the first failure. */
int ir_pass_run_pipeline(IrProgram *program,
    const IrPassSpec *passes,
    size_t pass_count,
    IrError *error);

/* Runs the current default optimization pipeline. */
int ir_pass_run_default_pipeline(IrProgram *program, IrError *error);

/* Built-in optimization pass: fold and simplify safe binary instructions with immediate or redundant operands. */
int ir_pass_fold_immediate_binary(IrProgram *program, IrError *error);

/* Built-in optimization pass: propagate temp values provably equal to immediates. */
int ir_pass_propagate_temp_constants(IrProgram *program, IrError *error);

/* Built-in optimization pass: propagate temp copies created by mov defs. */
int ir_pass_propagate_temp_copies(IrProgram *program, IrError *error);

/* Built-in optimization pass: simplify constant CFG edges and remove dead blocks. */
int ir_pass_simplify_cfg(IrProgram *program, IrError *error);

/* Built-in optimization pass: remove dead temp defs for pure mov/binary ops. */
int ir_pass_eliminate_dead_temp_defs(IrProgram *program, IrError *error);

#endif