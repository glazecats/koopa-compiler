#ifndef MEMORY_SSA_PASS_H
#define MEMORY_SSA_PASS_H

#include "value_ssa.h"

int memory_ssa_pass_forward_local_loads(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_forward_global_loads(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_redundant_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_dead_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_eliminate_dead_local_stores(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_canonicalize_memory_values(ValueSsaProgram *program, ValueSsaError *error);
int memory_ssa_pass_run_pipeline(ValueSsaProgram *program, ValueSsaError *error);

#endif
