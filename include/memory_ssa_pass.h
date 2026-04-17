#ifndef MEMORY_SSA_PASS_H
#define MEMORY_SSA_PASS_H

#include "value_ssa.h"

int memory_ssa_pass_forward_local_loads(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_forward_global_loads(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_promote_local_slots(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_promote_global_slots(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_redundant_local_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_redundant_global_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_redundant_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_dead_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_dead_local_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_dead_global_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_scalar_replace_local_slots(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_scalar_replace_global_slots(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_scalar_replace_slots(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_redundant_memory_binaries(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_canonicalize_memory_values(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_build_memory_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);
int memory_ssa_pass_run_pipeline(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_build_canonicalized_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);

#endif
