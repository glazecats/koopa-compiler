#ifndef VALUE_SSA_H
#define VALUE_SSA_H

#include <stddef.h>

#include "lower_ir.h"

typedef struct {
    int line;
    int column;
    char message[512];
} ValueSsaError;

typedef enum {
    VALUE_SSA_VALUE_IMMEDIATE = 0,
    VALUE_SSA_VALUE_ID,
} ValueSsaValueKind;

typedef struct {
    ValueSsaValueKind kind;
    long long immediate;
    size_t value_id;
} ValueSsaValueRef;

typedef enum {
    VALUE_SSA_SLOT_LOCAL = 0,
    VALUE_SSA_SLOT_GLOBAL,
} ValueSsaSlotKind;

typedef struct {
    ValueSsaSlotKind kind;
    size_t id;
} ValueSsaSlotRef;

typedef enum {
    VALUE_SSA_BINARY_ADD = 0,
    VALUE_SSA_BINARY_SUB,
    VALUE_SSA_BINARY_MUL,
    VALUE_SSA_BINARY_DIV,
    VALUE_SSA_BINARY_MOD,
    VALUE_SSA_BINARY_BIT_AND,
    VALUE_SSA_BINARY_BIT_XOR,
    VALUE_SSA_BINARY_BIT_OR,
    VALUE_SSA_BINARY_SHIFT_LEFT,
    VALUE_SSA_BINARY_SHIFT_RIGHT,
    VALUE_SSA_BINARY_EQ,
    VALUE_SSA_BINARY_NE,
    VALUE_SSA_BINARY_LT,
    VALUE_SSA_BINARY_LE,
    VALUE_SSA_BINARY_GT,
    VALUE_SSA_BINARY_GE,
} ValueSsaBinaryOp;

typedef enum {
    VALUE_SSA_INSTR_MOV = 0,
    VALUE_SSA_INSTR_BINARY,
    VALUE_SSA_INSTR_CALL,
    VALUE_SSA_INSTR_LOAD_LOCAL,
    VALUE_SSA_INSTR_STORE_LOCAL,
    VALUE_SSA_INSTR_LOAD_GLOBAL,
    VALUE_SSA_INSTR_STORE_GLOBAL,
} ValueSsaInstructionKind;

typedef struct {
    size_t id;
    char *source_name;
    int is_parameter;
} ValueSsaLocal;

typedef struct {
    size_t id;
    char *name;
    int has_initializer;
    long long initializer_value;
    int has_runtime_initializer;
} ValueSsaGlobal;

typedef struct {
    size_t predecessor_block_id;
    ValueSsaValueRef value;
} ValueSsaPhiInput;

typedef struct {
    size_t result_id;
    ValueSsaPhiInput *inputs;
    size_t input_count;
    size_t input_capacity;
} ValueSsaPhi;

typedef struct {
    ValueSsaInstructionKind kind;
    int has_result;
    ValueSsaValueRef result;
    union {
        ValueSsaValueRef mov_value;
        struct {
            ValueSsaBinaryOp op;
            ValueSsaValueRef lhs;
            ValueSsaValueRef rhs;
        } binary;
        struct {
            char *callee_name;
            ValueSsaValueRef *args;
            size_t arg_count;
        } call;
        ValueSsaSlotRef load_slot;
        struct {
            ValueSsaSlotRef slot;
            ValueSsaValueRef value;
        } store;
    } as;
} ValueSsaInstruction;

typedef enum {
    VALUE_SSA_TERM_RETURN = 0,
    VALUE_SSA_TERM_JUMP,
    VALUE_SSA_TERM_BRANCH,
} ValueSsaTerminatorKind;

typedef struct {
    ValueSsaTerminatorKind kind;
    union {
        ValueSsaValueRef return_value;
        size_t jump_target;
        struct {
            ValueSsaValueRef condition;
            size_t then_target;
            size_t else_target;
        } branch;
    } as;
} ValueSsaTerminator;

typedef struct {
    size_t id;
    ValueSsaPhi *phis;
    size_t phi_count;
    size_t phi_capacity;
    ValueSsaInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    int has_terminator;
    ValueSsaTerminator terminator;
} ValueSsaBasicBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    ValueSsaLocal *locals;
    size_t local_count;
    size_t local_capacity;
    ValueSsaBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t next_value_id;
} ValueSsaFunction;

typedef struct {
    ValueSsaGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    ValueSsaFunction *functions;
    size_t function_count;
    size_t function_capacity;
} ValueSsaProgram;

typedef struct {
    size_t block_count;
    unsigned char *predecessor_matrix;
    size_t *predecessor_counts;
    unsigned char *reachable;
    unsigned char *dominates;
    size_t *immediate_dominator;
    unsigned char *dominator_tree_children;
    size_t *dominator_tree_child_counts;
    unsigned char *dominance_frontier;
    size_t *dominance_frontier_counts;
} ValueSsaCfgAnalysis;

typedef enum {
    VALUE_SSA_USE_PHI = 0,
    VALUE_SSA_USE_INSTR,
    VALUE_SSA_USE_TERM,
} ValueSsaUseKind;

typedef enum {
    VALUE_SSA_USE_ROLE_PHI_INPUT = 0,
    VALUE_SSA_USE_ROLE_MOV_VALUE,
    VALUE_SSA_USE_ROLE_BINARY_LHS,
    VALUE_SSA_USE_ROLE_BINARY_RHS,
    VALUE_SSA_USE_ROLE_CALL_ARG,
    VALUE_SSA_USE_ROLE_STORE_VALUE,
    VALUE_SSA_USE_ROLE_RETURN_VALUE,
    VALUE_SSA_USE_ROLE_BRANCH_CONDITION,
} ValueSsaUseRole;

typedef struct {
    ValueSsaUseKind kind;
    ValueSsaUseRole role;
    size_t block_id;
    size_t phi_index;
    size_t instruction_index;
    size_t operand_index;
} ValueSsaUseSite;

typedef struct {
    size_t value_count;
    size_t *def_block_ids;
    size_t *def_phi_indices;
    size_t *def_instruction_indices;
    unsigned char *has_def;
    size_t *use_counts;
    size_t *use_offsets;
    ValueSsaUseSite *use_sites;
} ValueSsaDefUseAnalysis;

typedef struct {
    size_t block_count;
    size_t value_count;
    unsigned char *live_in;
    unsigned char *live_out;
} ValueSsaLivenessAnalysis;

typedef struct {
    size_t value_count;
    unsigned char *interferes;
} ValueSsaInterferenceGraph;

typedef struct {
    size_t value_count;
    size_t *weights;
} ValueSsaCopyAffinityGraph;

typedef struct {
    size_t lhs;
    size_t rhs;
    size_t weight;
} ValueSsaCopyAffinityCandidate;

typedef struct {
    size_t value_count;
    size_t *def_block_ids;
    size_t *use_counts;
    size_t *use_loop_depth_sums;
    size_t *use_call_density_sums;
    size_t *live_block_counts;
    size_t *loop_depth_sums;
    size_t *call_density_sums;
    size_t *call_crossing_counts;
    unsigned char *single_block_live_ranges;
    size_t *live_block_offsets;
    size_t *live_blocks;
    size_t live_blocks_count;
    size_t *interference_degrees;
    size_t *affinity_sums;
    unsigned char *move_related;
    unsigned char *rematerializable;
    unsigned char *split_child_values;
    size_t *split_child_depths;
    size_t *split_parent_value_ids;
    size_t *split_family_root_value_ids;
    ValueSsaCopyAffinityCandidate *candidates;
    size_t candidate_count;
} ValueSsaAllocationPrep;

typedef enum {
    VALUE_SSA_ALLOC_WORK_MOVE_HINT = 0,
    VALUE_SSA_ALLOC_WORK_CONSTRAINED,
    VALUE_SSA_ALLOC_WORK_SINGLE_BLOCK,
    VALUE_SSA_ALLOC_WORK_GLOBAL,
    VALUE_SSA_ALLOC_WORK_ISOLATED,
} ValueSsaAllocationWorkClass;

typedef struct {
    size_t value_id;
    ValueSsaAllocationWorkClass work_class;
    size_t priority;
} ValueSsaAllocationWorkItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocationWorkItem *items;
} ValueSsaAllocationWorklist;

typedef int (*ValueSsaDominatorTreeWalkFn)(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error);

typedef struct {
    size_t binding_count;
    ValueSsaValueRef *current_values;
    unsigned char *has_current_value;
    size_t *history_binding_ids;
    ValueSsaValueRef *history_previous_values;
    unsigned char *history_previous_has_value;
    size_t history_count;
    size_t history_capacity;
    size_t *scope_history_starts;
    size_t scope_count;
    size_t scope_capacity;
} ValueSsaRenameState;

void value_ssa_program_init(ValueSsaProgram *program);
void value_ssa_program_free(ValueSsaProgram *program);
void value_ssa_cfg_analysis_init(ValueSsaCfgAnalysis *analysis);
void value_ssa_cfg_analysis_free(ValueSsaCfgAnalysis *analysis);
void value_ssa_def_use_analysis_init(ValueSsaDefUseAnalysis *analysis);
void value_ssa_def_use_analysis_free(ValueSsaDefUseAnalysis *analysis);
void value_ssa_liveness_analysis_init(ValueSsaLivenessAnalysis *analysis);
void value_ssa_liveness_analysis_free(ValueSsaLivenessAnalysis *analysis);
void value_ssa_interference_graph_init(ValueSsaInterferenceGraph *graph);
void value_ssa_interference_graph_free(ValueSsaInterferenceGraph *graph);
void value_ssa_copy_affinity_graph_init(ValueSsaCopyAffinityGraph *graph);
void value_ssa_copy_affinity_graph_free(ValueSsaCopyAffinityGraph *graph);
void value_ssa_allocation_prep_init(ValueSsaAllocationPrep *prep);
void value_ssa_allocation_prep_free(ValueSsaAllocationPrep *prep);
void value_ssa_allocation_worklist_init(ValueSsaAllocationWorklist *worklist);
void value_ssa_allocation_worklist_free(ValueSsaAllocationWorklist *worklist);
void value_ssa_rename_state_init(ValueSsaRenameState *state);
void value_ssa_rename_state_free(ValueSsaRenameState *state);

ValueSsaValueRef value_ssa_value_immediate(long long value);
ValueSsaValueRef value_ssa_value_id(size_t value_id);
ValueSsaSlotRef value_ssa_slot_local(size_t local_id);
ValueSsaSlotRef value_ssa_slot_global(size_t global_id);

int value_ssa_program_append_global(ValueSsaProgram *program,
    const char *name,
    ValueSsaGlobal **out_global,
    ValueSsaError *error);
int value_ssa_program_append_function(ValueSsaProgram *program,
    const char *name,
    int has_body,
    ValueSsaFunction **out_function,
    ValueSsaError *error);
int value_ssa_function_append_local(ValueSsaFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    ValueSsaError *error);
int value_ssa_function_append_block(ValueSsaFunction *function,
    size_t *out_block_id,
    ValueSsaBasicBlock **out_block,
    ValueSsaError *error);
size_t value_ssa_function_allocate_value(ValueSsaFunction *function);
int value_ssa_block_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error);
int value_ssa_block_append_instruction(ValueSsaBasicBlock *block,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error);
int value_ssa_block_set_return(ValueSsaBasicBlock *block,
    ValueSsaValueRef value,
    ValueSsaError *error);
int value_ssa_block_set_jump(ValueSsaBasicBlock *block,
    size_t target_block_id,
    ValueSsaError *error);
int value_ssa_block_set_branch(ValueSsaBasicBlock *block,
    ValueSsaValueRef condition,
    size_t then_target,
    size_t else_target,
    ValueSsaError *error);

int value_ssa_verify_program(const ValueSsaProgram *program, ValueSsaError *error);
int value_ssa_dump_program(const ValueSsaProgram *program, char **out_text);
const char *value_ssa_allocation_work_class_name(ValueSsaAllocationWorkClass work_class);
int value_ssa_allocation_prep_get_live_blocks(const ValueSsaAllocationPrep *allocation_prep,
    size_t value_id,
    const size_t **out_blocks,
    size_t *out_count);
int value_ssa_allocation_prep_find_affinity_candidate(const ValueSsaAllocationPrep *allocation_prep,
    size_t lhs,
    size_t rhs,
    size_t *out_index,
    const ValueSsaCopyAffinityCandidate **out_candidate);
size_t value_ssa_allocation_prep_get_affinity_weight(const ValueSsaAllocationPrep *allocation_prep,
    size_t lhs,
    size_t rhs);
int value_ssa_allocation_prep_get_top_affinity_neighbors(const ValueSsaAllocationPrep *allocation_prep,
    size_t value_id,
    size_t max_count,
    size_t *out_neighbor_value_ids,
    size_t *out_weights,
    size_t *out_count);
int value_ssa_allocation_worklist_find_value(const ValueSsaAllocationWorklist *worklist,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocationWorkItem **out_item);
int value_ssa_allocation_worklist_get_class_range(const ValueSsaAllocationWorklist *worklist,
    ValueSsaAllocationWorkClass work_class,
    size_t *out_begin,
    size_t *out_end);
int value_ssa_dump_allocation_prep(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocation_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaAllocationWorklist *worklist,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocation_worklist_for_class(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaAllocationWorklist *worklist,
    ValueSsaAllocationWorkClass work_class,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_cfg_analysis(const ValueSsaFunction *function,
    ValueSsaCfgAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_compute_def_use_analysis(const ValueSsaFunction *function,
    ValueSsaDefUseAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_compute_liveness_analysis(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    ValueSsaLivenessAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_compute_interference_graph(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    const ValueSsaLivenessAnalysis *liveness_analysis,
    ValueSsaInterferenceGraph *graph,
    ValueSsaError *error);
int value_ssa_compute_copy_affinity_graph(const ValueSsaFunction *function,
    const ValueSsaInterferenceGraph *interference_graph,
    ValueSsaCopyAffinityGraph *graph,
    ValueSsaError *error);
int value_ssa_compute_allocation_prep(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use_analysis,
    const ValueSsaLivenessAnalysis *liveness_analysis,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *copy_affinity_graph,
    ValueSsaAllocationPrep *prep,
    ValueSsaError *error);
int value_ssa_compute_allocation_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaAllocationWorklist *worklist,
    ValueSsaError *error);
int value_ssa_compute_phi_placement(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    const unsigned char *definition_blocks,
    unsigned char *out_phi_blocks,
    ValueSsaError *error);
int value_ssa_compute_dominator_tree_preorder(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t *out_order,
    size_t *out_count,
    ValueSsaError *error);
int value_ssa_walk_dominator_tree(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    ValueSsaDominatorTreeWalkFn enter_block,
    ValueSsaDominatorTreeWalkFn leave_block,
    void *user_data,
    ValueSsaError *error);
int value_ssa_rename_state_prepare(ValueSsaRenameState *state, size_t binding_count, ValueSsaError *error);
int value_ssa_rename_state_begin_scope(ValueSsaRenameState *state, ValueSsaError *error);
int value_ssa_rename_state_end_scope(ValueSsaRenameState *state, ValueSsaError *error);
int value_ssa_rename_state_bind(ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaValueRef value,
    ValueSsaError *error);
int value_ssa_rename_state_clear(ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaError *error);
int value_ssa_rename_state_lookup(const ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaValueRef *out_value,
    ValueSsaError *error);
int value_ssa_rename_rewrite_value_ref(ValueSsaValueRef *value,
    const ValueSsaRenameState *state,
    ValueSsaError *error);
int value_ssa_rename_rewrite_block_uses(ValueSsaBasicBlock *block,
    const ValueSsaRenameState *state,
    ValueSsaError *error);
int value_ssa_rename_rewrite_phi_inputs_for_predecessor(ValueSsaBasicBlock *block,
    size_t predecessor_block_id,
    const ValueSsaRenameState *state,
    ValueSsaError *error);
int value_ssa_rename_function_values(ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_build_from_lower_ir(const LowerIrProgram *program,
    ValueSsaProgram *out_program,
    ValueSsaError *error);

#endif
