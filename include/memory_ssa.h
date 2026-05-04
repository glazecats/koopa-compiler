#ifndef MEMORY_SSA_H
#define MEMORY_SSA_H

#include <stddef.h>

#include "value_ssa.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MemorySsaError;

typedef struct {
    size_t id;
    ValueSsaSlotRef slot;
    char *debug_name;
    int has_entry_version;
    size_t entry_version_id;
} MemorySsaTrackedSlot;

typedef struct {
    size_t predecessor_block_id;
    size_t version_id;
} MemorySsaPhiInput;

typedef struct {
    size_t slot_id;
    size_t result_version_id;
    MemorySsaPhiInput *inputs;
    size_t input_count;
    size_t input_capacity;
} MemorySsaPhi;

typedef struct {
    size_t slot_id;
    int has_use_version;
    size_t use_version_id;
    int has_def_version;
    size_t def_version_id;
} MemorySsaAccess;

typedef struct {
    ValueSsaInstruction instruction;
    MemorySsaAccess *accesses;
    size_t access_count;
    size_t access_capacity;
} MemorySsaInstruction;

typedef enum {
    MEMORY_SSA_VERSION_DEF_ENTRY = 0,
    MEMORY_SSA_VERSION_DEF_PHI,
    MEMORY_SSA_VERSION_DEF_INSTR,
} MemorySsaVersionDefKind;

typedef enum {
    MEMORY_SSA_VERSION_USE_PHI = 0,
    MEMORY_SSA_VERSION_USE_INSTR,
} MemorySsaVersionUseKind;

typedef enum {
    MEMORY_SSA_VERSION_USE_ROLE_PHI_INPUT = 0,
    MEMORY_SSA_VERSION_USE_ROLE_LOAD,
    MEMORY_SSA_VERSION_USE_ROLE_STORE_PREVIOUS,
    MEMORY_SSA_VERSION_USE_ROLE_CALL,
} MemorySsaVersionUseRole;

typedef struct {
    MemorySsaVersionUseKind kind;
    MemorySsaVersionUseRole role;
    size_t block_id;
    size_t phi_index;
    size_t instruction_index;
    size_t access_index;
    size_t predecessor_block_id;
} MemorySsaVersionUseSite;

typedef struct {
    size_t id;
    MemorySsaPhi *phis;
    size_t phi_count;
    size_t phi_capacity;
    MemorySsaInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    int has_terminator;
    ValueSsaTerminator terminator;
} MemorySsaBasicBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    MemorySsaTrackedSlot *tracked_slots;
    size_t tracked_slot_count;
    size_t tracked_slot_capacity;
    MemorySsaBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t next_version_id;
} MemorySsaFunction;

typedef struct {
    size_t version_count;
    size_t *def_slot_ids;
    MemorySsaVersionDefKind *def_kinds;
    size_t *def_block_ids;
    size_t *def_phi_indices;
    size_t *def_instruction_indices;
    unsigned char *has_def;
    size_t *use_counts;
    size_t *use_offsets;
    MemorySsaVersionUseSite *use_sites;
} MemorySsaDefUseAnalysis;

typedef struct {
    MemorySsaFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MemorySsaProgram;

void memory_ssa_program_init(MemorySsaProgram *program);
void memory_ssa_program_free(MemorySsaProgram *program);
void memory_ssa_def_use_analysis_init(MemorySsaDefUseAnalysis *analysis);
void memory_ssa_def_use_analysis_free(MemorySsaDefUseAnalysis *analysis);

int memory_ssa_program_append_function(MemorySsaProgram *program,
    const char *name,
    int has_body,
    MemorySsaFunction **out_function,
    MemorySsaError *error);
int memory_ssa_function_append_tracked_slot(MemorySsaFunction *function,
    ValueSsaSlotRef slot,
    const char *debug_name,
    size_t *out_slot_id,
    MemorySsaError *error);
int memory_ssa_function_set_entry_version(MemorySsaFunction *function,
    size_t slot_id,
    size_t version_id,
    MemorySsaError *error);
int memory_ssa_function_append_block(MemorySsaFunction *function,
    size_t *out_block_id,
    MemorySsaBasicBlock **out_block,
    MemorySsaError *error);
size_t memory_ssa_function_allocate_version(MemorySsaFunction *function);
int memory_ssa_block_append_phi(MemorySsaBasicBlock *block,
    size_t slot_id,
    size_t result_version_id,
    const MemorySsaPhiInput *inputs,
    size_t input_count,
    MemorySsaError *error);
int memory_ssa_instruction_append_access(MemorySsaInstruction *instruction,
    const MemorySsaAccess *access,
    MemorySsaError *error);
int memory_ssa_block_append_instruction(MemorySsaBasicBlock *block,
    const MemorySsaInstruction *instruction,
    MemorySsaError *error);
int memory_ssa_block_set_return(MemorySsaBasicBlock *block,
    ValueSsaValueRef value,
    MemorySsaError *error);
int memory_ssa_block_set_void_return(MemorySsaBasicBlock *block, MemorySsaError *error);
int memory_ssa_block_set_jump(MemorySsaBasicBlock *block,
    size_t target_block_id,
    MemorySsaError *error);
int memory_ssa_block_set_branch(MemorySsaBasicBlock *block,
    ValueSsaValueRef condition,
    size_t then_target,
    size_t else_target,
    MemorySsaError *error);

int memory_ssa_verify_program(const MemorySsaProgram *program, MemorySsaError *error);
int memory_ssa_dump_program(const MemorySsaProgram *program, char **out_text);
int memory_ssa_build_from_value_ssa(const ValueSsaProgram *program,
    MemorySsaProgram *out_program,
    MemorySsaError *error);
int memory_ssa_compute_def_use_analysis(const MemorySsaFunction *function,
    MemorySsaDefUseAnalysis *analysis,
    MemorySsaError *error);
int memory_ssa_find_version_use_sites(const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    const MemorySsaVersionUseSite **out_sites,
    size_t *out_count);
int memory_ssa_resolve_version_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error);
int memory_ssa_resolve_load_store_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error);
int memory_ssa_resolve_store_previous_value(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error);
int memory_ssa_is_trivially_dead_store(const MemorySsaFunction *function,
    const MemorySsaDefUseAnalysis *analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_is_dead,
    MemorySsaError *error);
int memory_ssa_value_matches_slot_version(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    ValueSsaValueRef value,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_matches,
    MemorySsaError *error);
int memory_ssa_find_block_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    MemorySsaError *error);

#endif
