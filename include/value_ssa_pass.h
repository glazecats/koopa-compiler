#ifndef VALUE_SSA_PASS_H
#define VALUE_SSA_PASS_H

#include "value_ssa.h"

typedef enum {
    VALUE_SSA_LOWER_IR_CANONICALIZE_CLASSIC = 0,
    VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_VALUE,
    VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
    VALUE_SSA_LOWER_IR_CANONICALIZE_DEFAULT = VALUE_SSA_LOWER_IR_CANONICALIZE_MEMORY_FULL,
} ValueSsaLowerIrCanonicalizeMode;

int value_ssa_simplify_trivial_values(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_forward_local_loads(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_forward_global_loads(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_eliminate_redundant_stores(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_eliminate_dead_stores(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_normalize_binary_operands(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_simplify_algebraic_identities(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_fold_constants(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_eliminate_redundant_binaries(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_simplify_cfg(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_eliminate_dead_value_defs(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_canonicalize_program(ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_build_from_lower_ir_with_canonicalization(const LowerIrProgram *program,
    ValueSsaLowerIrCanonicalizeMode mode,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_default_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_memory_value_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int value_ssa_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);

#endif
