#include "memory_ssa_pass.h"

#include "memory_ssa.h"
#include "value_ssa_pass.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void memory_ssa_pass_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static void memory_ssa_pass_copy_memory_error(ValueSsaError *error, const MemorySsaError *memory_error);
static int memory_ssa_pass_compact_removed_instructions(ValueSsaBasicBlock *block,
    const unsigned char *remove_flags);
static int memory_ssa_pass_validate_alignment(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    ValueSsaError *error);
static int memory_ssa_pass_instruction_site_dominates(const ValueSsaCfgAnalysis *cfg_analysis,
    size_t candidate_block_id,
    size_t candidate_instruction_index,
    size_t use_block_id,
    size_t use_instruction_index,
    ValueSsaError *error);
static int memory_ssa_pass_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error);
static int memory_ssa_pass_value_matches_slot_version(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    ValueSsaValueRef value,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    unsigned char *visit_state,
    int *out_matches,
    ValueSsaError *error);
static int memory_ssa_pass_find_block_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);
static int memory_ssa_pass_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs);
static int memory_ssa_pass_slot_refs_equal(ValueSsaSlotRef lhs, ValueSsaSlotRef rhs);
static int memory_ssa_pass_dump_program_copy(const ValueSsaProgram *program,
    char **out_text,
    ValueSsaError *error);
static int memory_ssa_pass_run_one_step(const char *step_name,
    int (*pass_fn)(ValueSsaProgram *, ValueSsaError *),
    ValueSsaProgram *program,
    ValueSsaError *error);
static int memory_ssa_pass_forward_loads(ValueSsaProgram *program,
    ValueSsaInstructionKind load_kind,
    const char *invalid_error_code,
    ValueSsaError *error);
static int memory_ssa_pass_eliminate_dead_stores_impl(ValueSsaProgram *program,
    int include_globals,
    ValueSsaError *error);

#include "memory_ssa_pass_core.inc"
#include "memory_ssa_pass_load_forward.inc"
#include "memory_ssa_pass_store_cleanup.inc"
#include "memory_ssa_pass_memory_value.inc"
#include "memory_ssa_pass_pipeline.inc"
