#include "ir_pass.h"

#include <limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	unsigned char has_definition;
	IrInstructionKind kind;
	IrBinaryOp binary_op;
	IrValueRef mov_value;
	IrValueRef binary_lhs;
	IrValueRef binary_rhs;
} IrPassTempDef;

typedef struct {
	size_t block_id;
	size_t instruction_index;
} IrPassTempDefSite;

typedef enum {
	IR_PASS_TEMP_USE_MOV_VALUE = 0,
	IR_PASS_TEMP_USE_BINARY_LHS,
	IR_PASS_TEMP_USE_BINARY_RHS,
	IR_PASS_TEMP_USE_CALL_ARG,
	IR_PASS_TEMP_USE_TERM_RETURN,
	IR_PASS_TEMP_USE_TERM_BRANCH,
} IrPassTempUseKind;

typedef struct {
	size_t block_id;
	size_t instruction_index;
	size_t operand_index;
	IrPassTempUseKind kind;
} IrPassTempUseSite;

typedef struct {
	size_t temp_count;
	size_t *use_counts;
	size_t *definition_counts;
	IrPassTempDefSite *definition_sites;
	unsigned char *has_unique_definition_site;
	IrPassTempUseSite *use_sites;
	unsigned char *has_unique_use_site;
	IrValueRef *temp_copy_values;
	unsigned char *has_temp_copy_value;
	IrPassTempDef *temp_defs;
	long long *temp_constant_values;
	unsigned char *has_temp_constant_value;
} IrPassTempAnalysis;

typedef struct {
	size_t block_count;
	unsigned char *reachable;
	size_t *predecessor_counts;
} IrPassCfgAnalysis;

static void ir_pass_set_error(IrError *error, int line, int column, const char *fmt, ...);
static IrValueRef ir_pass_value_immediate(long long value);
static int ir_pass_value_ref_equals(IrValueRef lhs, IrValueRef rhs);
static int ir_pass_binary_op_requires_runtime_validation(IrBinaryOp op);
static int ir_pass_instruction_has_side_effects(const IrInstruction *instruction);
static void ir_pass_temp_analysis_free(IrPassTempAnalysis *analysis);
static int ir_pass_build_temp_analysis(const IrFunction *function,
	IrPassTempAnalysis *analysis,
	IrError *error);

static int ir_pass_note_temp_use(IrValueRef value,
	size_t *use_counts,
	IrPassTempUseSite *use_sites,
	unsigned char *has_unique_use_site,
	size_t use_count_len,
	size_t block_id,
	size_t instruction_index,
	IrPassTempUseKind kind,
	size_t operand_index,
	const char *function_name,
	IrError *error);
static int ir_pass_note_temp_remap_id(IrValueRef value,
	size_t *temp_remap,
	size_t temp_remap_len,
	size_t *next_temp_id,
	const char *function_name,
	IrError *error);
static int ir_pass_apply_temp_remap(IrValueRef *value,
	const size_t *temp_remap,
	size_t temp_remap_len,
	const char *function_name,
	IrError *error);
static int ir_pass_resolve_equivalent_value(IrValueRef value,
	const IrPassTempAnalysis *analysis,
	size_t temp_count,
	const char *function_name,
	IrError *error,
	IrValueRef *out_value);
static int ir_pass_is_removable_dead_temp_def(const IrInstruction *instruction,
	const size_t *use_counts,
	size_t use_count_len);
static int ir_pass_compact_function_temp_ids(IrFunction *function, IrError *error);

static int ir_pass_resolve_temp_copy_value(IrValueRef value,
	const IrPassTempAnalysis *analysis,
	size_t temp_count,
	const char *function_name,
	IrError *error,
	IrValueRef *out_value);
static IrValueRef *ir_pass_get_temp_use_site_value_mut(IrFunction *function,
	const IrPassTempUseSite *use_site);
static int ir_pass_apply_temp_copy_propagation(IrFunction *function,
	const IrPassTempAnalysis *analysis,
	IrError *error);
static int ir_pass_compact_function_temp_ids(IrFunction *function, IrError *error);

static void ir_pass_basic_block_free(IrBasicBlock *block);
static void ir_pass_cfg_analysis_free(IrPassCfgAnalysis *analysis);
static int ir_pass_build_cfg_analysis(const IrFunction *function,
	IrPassCfgAnalysis *analysis,
	IrError *error);
static int ir_pass_compact_function_blocks(IrFunction *function,
	const IrPassCfgAnalysis *analysis,
	IrError *error);

static int ir_pass_try_fold_binary_immediate(IrBinaryOp op,
	long long lhs,
	long long rhs,
	long long *out_value);

#define IR_PASS_SPLIT_AGGREGATOR 1

#include "ir_pass_core.inc"
#include "ir_pass_temp_analysis.inc"
#include "ir_pass_fold.inc"
#include "ir_pass_const.inc"
#include "ir_pass_copy.inc"
#include "ir_pass_cfg_analysis.inc"
#include "ir_pass_cfg.inc"
#include "ir_pass_dce.inc"
#include "ir_pass_pipeline.inc"

#undef IR_PASS_SPLIT_AGGREGATOR
