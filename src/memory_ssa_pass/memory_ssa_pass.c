#include "memory_ssa_pass.h"

#include "memory_ssa.h"
#include "value_ssa_pass.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int memory_ssa_pass_trace_enabled(void) {
    const char *flag = getenv("MEMORY_SSA_PASS_TRACE");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static void memory_ssa_pass_trace(const char *fmt, ...) {
    va_list args;

    if (!fmt || !memory_ssa_pass_trace_enabled()) {
        return;
    }

    fprintf(stderr, "[memory-ssa-pass-trace] ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

static long long memory_ssa_pass_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

static void memory_ssa_pass_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static void memory_ssa_pass_copy_memory_error(ValueSsaError *error, const MemorySsaError *memory_error);
static int memory_ssa_pass_compact_removed_instructions(ValueSsaBasicBlock *block,
    const unsigned char *remove_flags);
static int memory_ssa_pass_append_instruction_ignoring_terminator(ValueSsaBasicBlock *block,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error);
static int memory_ssa_pass_validate_alignment(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    ValueSsaError *error);
static int memory_ssa_pass_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error);
static int memory_ssa_pass_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs);
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
    int include_locals,
    int include_globals,
    ValueSsaError *error);
static int memory_ssa_pass_block_has_backedge_predecessor(const ValueSsaCfgAnalysis *cfg_analysis, size_t block_id);
static int memory_ssa_pass_find_prior_equivalent_value_in_block(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    size_t before_instruction_index,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);
static int memory_ssa_pass_try_promote_dominating_memory_phi(ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    const ValueSsaCfgAnalysis *cfg_analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_promoted,
    ValueSsaError *error);
static int memory_ssa_pass_find_dominating_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    const ValueSsaCfgAnalysis *cfg_analysis,
    size_t block_id,
    size_t instruction_index,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);

#include "memory_ssa_pass_core.inc"
#include "memory_ssa_pass_slot_promotion.inc"
#include "memory_ssa_pass_load_forward.inc"
#include "memory_ssa_pass_store_cleanup.inc"
#include "memory_ssa_pass_local_scalar_replace.inc"
#include "memory_ssa_pass_global_scalar_replace.inc"
#include "memory_ssa_pass_scalar_replace.inc"
#include "memory_ssa_pass_memory_cse.inc"
#include "memory_ssa_pass_memory_value.inc"
#include "memory_ssa_pass_bridge.inc"
#include "memory_ssa_pass_pipeline.inc"
