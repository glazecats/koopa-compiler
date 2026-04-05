#if defined(__INTELLISENSE__)
#include "ast.h"
#include "ir.h"
#include "ir_pass.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*IrPassProgramSetupFn)(IrProgram *program);
typedef int (*IrPassProgramStepCheckFn)(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index);

typedef struct {
    const char *case_id;
    const char *source;
    IrPassProgramSetupFn setup;
} IrPassFixedPointCase;

typedef struct {
    const char *case_id;
    const char *source;
    IrPassProgramSetupFn setup;
    IrPassProgramStepCheckFn step_check;
} IrPassStepwiseCase;

static int lower_source_to_ir_program(const char *source, IrProgram *out_program);
static int expect_error_fragment(const char *case_id,
    const IrError *error,
    const char *expected_fragment);
static int expect_program_dump(const char *case_id,
    const IrProgram *program,
    const char *expected_text);
static IrBasicBlock *find_first_branch_block_mut(IrProgram *program);
static IrValueRef make_immediate_value(long long value);
static IrValueRef make_temp_value(size_t temp_id);
static IrValueRef make_local_value(size_t local_id);
static int rewrite_as_multi_def_join_value_program(IrProgram *program,
    IrValueRef then_value,
    IrValueRef else_value,
    size_t *out_join_block_id);
static int rewrite_as_multi_def_join_temp_program(IrProgram *program, size_t *out_join_block_id);
static int rewrite_branch_successors_to_same_temp_return(IrProgram *program,
    size_t *out_branch_block_id,
    size_t *out_temp_id);
static int rewrite_branch_successors_to_equivalent_temp_returns(IrProgram *program,
    IrValueRef first_source,
    IrValueRef second_source,
    IrValueRef *out_expected_value);
static int rewrite_as_single_use_call_arg_copy_program(IrProgram *program);
static int rewrite_branch_successors_to_local_value_returns(IrProgram *program,
    IrValueRef then_value,
    IrValueRef else_value);
static int rewrite_branch_successors_to_local_constant_returns(IrProgram *program,
    long long then_value,
    long long else_value);
static int setup_post_dce_same_return_fixed_point_case(IrProgram *program);
static int setup_branch_condition_true_fixed_point_case(IrProgram *program);
static int setup_multi_def_constant_path_case(IrProgram *program);
static int setup_multi_def_copy_path_case(IrProgram *program);
static int expect_default_pipeline_fixed_point_case(const IrPassFixedPointCase *test_case);
static int expect_default_pipeline_stepwise_case(const IrPassStepwiseCase *test_case);
static int check_stepwise_call_count_stays_one(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index);
static int check_stepwise_branch_distinction_remains(const char *case_id,
    const IrProgram *program,
    const char *pass_name,
    size_t pass_index);
static int resolve_simple_block_return_value_for_test(const IrBasicBlock *block, IrValueRef *out_value);
static int is_foldable_binary_op(IrBinaryOp op);
static int is_safely_foldable_immediate_binary(IrBinaryOp op, long long lhs, long long rhs);
static int binary_op_requires_runtime_validation_for_test(IrBinaryOp op);
static int instruction_has_side_effects_for_test(const IrInstruction *instruction);
static int note_temp_use(IrValueRef value, size_t *use_counts, size_t use_count_len);
static size_t count_dead_temp_defs(const IrProgram *program);
static size_t count_total_instructions(const IrProgram *program);
static size_t count_call_instructions(const IrProgram *program);
static size_t count_binary_instructions_with_op(const IrProgram *program, IrBinaryOp op);
static size_t count_total_blocks(const IrProgram *program);
static size_t count_immediate_branch_terminators(const IrProgram *program);
static size_t count_branch_terminators(const IrProgram *program);
static size_t count_jump_to_empty_return_blocks(const IrProgram *program);
static size_t count_temp_copy_propagatable_uses(const IrProgram *program);
static size_t count_immediate_binary_with_safety(const IrProgram *program, int count_only_safe);
static int is_identity_simplifiable_binary(const IrInstruction *instruction);
static int ir_value_ref_equals_for_test(IrValueRef lhs, IrValueRef rhs);
static int is_same_operand_simplifiable_binary(const IrInstruction *instruction);
static size_t count_identity_binary_simplification_opportunities(const IrProgram *program);
static size_t count_same_operand_binary_simplification_opportunities(const IrProgram *program);
#endif
