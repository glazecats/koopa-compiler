CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Iinclude
FANALYZER_CFLAGS := -std=c11 -Wall -Wextra -fanalyzer -Iinclude
ASAN_CFLAGS := -std=c11 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -g -Iinclude
STRICT_WARN_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Iinclude

BUILD_DIR := build
COMMON_OBJ_DIR := $(BUILD_DIR)/obj
COMMON_LIB_DIR := $(BUILD_DIR)/lib
LEXER_BUILD_DIR := $(BUILD_DIR)/lexer
PARSER_BUILD_DIR := $(BUILD_DIR)/parser
SEMANTIC_BUILD_DIR := $(BUILD_DIR)/semantic
IR_BUILD_DIR := $(BUILD_DIR)/ir
LOWER_IR_BUILD_DIR := $(BUILD_DIR)/lower_ir
VALUE_SSA_BUILD_DIR := $(BUILD_DIR)/value_ssa
MEMORY_SSA_BUILD_DIR := $(BUILD_DIR)/memory_ssa
MACHINE_IR_BUILD_DIR := $(BUILD_DIR)/machine_ir
MACHINE_SELECT_BUILD_DIR := $(BUILD_DIR)/machine_select
MACHINE_LAYOUT_BUILD_DIR := $(BUILD_DIR)/machine_layout
MACHINE_EMIT_BUILD_DIR := $(BUILD_DIR)/machine_emit
MACHINE_ENCODE_BUILD_DIR := $(BUILD_DIR)/machine_encode
MACHINE_BYTES_BUILD_DIR := $(BUILD_DIR)/machine_bytes
MACHINE_OBJECT_BUILD_DIR := $(BUILD_DIR)/machine_object
MACHINE_RELOC_BUILD_DIR := $(BUILD_DIR)/machine_reloc
MACHINE_CONTAINER_BUILD_DIR := $(BUILD_DIR)/machine_container
MACHINE_ELF_BUILD_DIR := $(BUILD_DIR)/machine_elf
MACHINE_IMAGE_BUILD_DIR := $(BUILD_DIR)/machine_image
MACHINE_EXEC_BUILD_DIR := $(BUILD_DIR)/machine_exec
MACHINE_LOAD_BUILD_DIR := $(BUILD_DIR)/machine_load
MACHINE_RUNTIME_BUILD_DIR := $(BUILD_DIR)/machine_runtime
MACHINE_LAUNCH_BUILD_DIR := $(BUILD_DIR)/machine_launch
MACHINE_STEP_BUILD_DIR := $(BUILD_DIR)/machine_step
MACHINE_DECODE_BUILD_DIR := $(BUILD_DIR)/machine_decode
MACHINE_PAYLOAD_DECODE_BUILD_DIR := $(BUILD_DIR)/machine_payload_decode
MACHINE_INTERP_BUILD_DIR := $(BUILD_DIR)/machine_interp
MACHINE_TRANSITION_BUILD_DIR := $(BUILD_DIR)/machine_transition
MACHINE_STATE_BUILD_DIR := $(BUILD_DIR)/machine_state
MACHINE_MUTATION_BUILD_DIR := $(BUILD_DIR)/machine_mutation
MACHINE_WRITEBACK_BUILD_DIR := $(BUILD_DIR)/machine_writeback
MACHINE_COMMIT_BUILD_DIR := $(BUILD_DIR)/machine_commit
MACHINE_APPLY_BUILD_DIR := $(BUILD_DIR)/machine_apply
MACHINE_OBSERVE_BUILD_DIR := $(BUILD_DIR)/machine_observe
MACHINE_DELTA_BUILD_DIR := $(BUILD_DIR)/machine_delta
MACHINE_TRACE_BUILD_DIR := $(BUILD_DIR)/machine_trace
MACHINE_EVENT_BUILD_DIR := $(BUILD_DIR)/machine_event
MACHINE_OUTCOME_BUILD_DIR := $(BUILD_DIR)/machine_outcome
MACHINE_HISTORY_BUILD_DIR := $(BUILD_DIR)/machine_history
MACHINE_TIMELINE_BUILD_DIR := $(BUILD_DIR)/machine_timeline
MACHINE_LOG_BUILD_DIR := $(BUILD_DIR)/machine_log
MACHINE_JOURNAL_BUILD_DIR := $(BUILD_DIR)/machine_journal
COMPILER_TEST_BUILD_DIR := $(BUILD_DIR)/compiler_tests
TOOLS_BUILD_DIR := $(BUILD_DIR)/tools

LEXER_TEST_BIN := $(LEXER_BUILD_DIR)/lexer_test
PARSER_TEST_BIN := $(PARSER_BUILD_DIR)/parser_test
LEXER_REGRESSION_BIN := $(LEXER_BUILD_DIR)/lexer_regression_test
PARSER_REGRESSION_BIN := $(PARSER_BUILD_DIR)/parser_regression_test
PARSER_LEGACY_LINK_BIN := $(PARSER_BUILD_DIR)/parser_legacy_link_test
SEMANTIC_REGRESSION_BIN := $(SEMANTIC_BUILD_DIR)/semantic_regression_test
IR_REGRESSION_BIN := $(IR_BUILD_DIR)/ir_regression_test
IR_VERIFIER_BIN := $(IR_BUILD_DIR)/ir_verifier_test
IR_PASS_BIN := $(IR_BUILD_DIR)/ir_pass_test
LOWER_IR_REGRESSION_BIN := $(LOWER_IR_BUILD_DIR)/lower_ir_regression_test
LOWER_IR_VERIFIER_BIN := $(LOWER_IR_BUILD_DIR)/lower_ir_verifier_test
VALUE_SSA_REGRESSION_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_regression_test
VALUE_SSA_VERIFIER_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_verifier_test
VALUE_SSA_ANALYSIS_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_analysis_test
VALUE_SSA_INTERP_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_interp_test
VALUE_SSA_ORACLE_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_oracle_test
VALUE_SSA_ALLOC_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_alloc_test
VALUE_SSA_MACHINE_BIN := $(VALUE_SSA_BUILD_DIR)/value_ssa_machine_test
MACHINE_IR_BIN := $(MACHINE_IR_BUILD_DIR)/machine_ir_test
MACHINE_SELECT_BIN := $(MACHINE_SELECT_BUILD_DIR)/machine_select_test
MACHINE_LAYOUT_BIN := $(MACHINE_LAYOUT_BUILD_DIR)/machine_layout_test
MACHINE_EMIT_BIN := $(MACHINE_EMIT_BUILD_DIR)/machine_emit_test
MACHINE_ENCODE_BIN := $(MACHINE_ENCODE_BUILD_DIR)/machine_encode_test
MACHINE_BYTES_BIN := $(MACHINE_BYTES_BUILD_DIR)/machine_bytes_test
MACHINE_OBJECT_BIN := $(MACHINE_OBJECT_BUILD_DIR)/machine_object_test
MACHINE_RELOC_BIN := $(MACHINE_RELOC_BUILD_DIR)/machine_reloc_test
MACHINE_CONTAINER_BIN := $(MACHINE_CONTAINER_BUILD_DIR)/machine_container_test
MACHINE_ELF_BIN := $(MACHINE_ELF_BUILD_DIR)/machine_elf_test
MACHINE_IMAGE_BIN := $(MACHINE_IMAGE_BUILD_DIR)/machine_image_test
MACHINE_EXEC_BIN := $(MACHINE_EXEC_BUILD_DIR)/machine_exec_test
MACHINE_LOAD_BIN := $(MACHINE_LOAD_BUILD_DIR)/machine_load_test
MACHINE_RUNTIME_BIN := $(MACHINE_RUNTIME_BUILD_DIR)/machine_runtime_test
MACHINE_LAUNCH_BIN := $(MACHINE_LAUNCH_BUILD_DIR)/machine_launch_test
MACHINE_STEP_BIN := $(MACHINE_STEP_BUILD_DIR)/machine_step_test
MACHINE_DECODE_BIN := $(MACHINE_DECODE_BUILD_DIR)/machine_decode_test
MACHINE_PAYLOAD_DECODE_BIN := $(MACHINE_PAYLOAD_DECODE_BUILD_DIR)/machine_payload_decode_test
MACHINE_INTERP_BIN := $(MACHINE_INTERP_BUILD_DIR)/machine_interp_test
MACHINE_TRANSITION_BIN := $(MACHINE_TRANSITION_BUILD_DIR)/machine_transition_test
MACHINE_STATE_BIN := $(MACHINE_STATE_BUILD_DIR)/machine_state_test
MACHINE_MUTATION_BIN := $(MACHINE_MUTATION_BUILD_DIR)/machine_mutation_test
MACHINE_WRITEBACK_BIN := $(MACHINE_WRITEBACK_BUILD_DIR)/machine_writeback_test
MACHINE_COMMIT_BIN := $(MACHINE_COMMIT_BUILD_DIR)/machine_commit_test
MACHINE_APPLY_BIN := $(MACHINE_APPLY_BUILD_DIR)/machine_apply_test
MACHINE_OBSERVE_BIN := $(MACHINE_OBSERVE_BUILD_DIR)/machine_observe_test
MACHINE_DELTA_BIN := $(MACHINE_DELTA_BUILD_DIR)/machine_delta_test
MACHINE_TRACE_BIN := $(MACHINE_TRACE_BUILD_DIR)/machine_trace_test
MACHINE_EVENT_BIN := $(MACHINE_EVENT_BUILD_DIR)/machine_event_test
MACHINE_OUTCOME_BIN := $(MACHINE_OUTCOME_BUILD_DIR)/machine_outcome_test
MACHINE_HISTORY_BIN := $(MACHINE_HISTORY_BUILD_DIR)/machine_history_test
MACHINE_TIMELINE_BIN := $(MACHINE_TIMELINE_BUILD_DIR)/machine_timeline_test
MACHINE_LOG_BIN := $(MACHINE_LOG_BUILD_DIR)/machine_log_test
MACHINE_JOURNAL_BIN := $(MACHINE_JOURNAL_BUILD_DIR)/machine_journal_test
COMPILER_BIN := $(BUILD_DIR)/compiler
COMPILER_DRIVER_TEST_BIN := $(COMPILER_TEST_BUILD_DIR)/compiler_driver_test
MEMORY_SSA_REGRESSION_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_regression_test
MEMORY_SSA_VERIFIER_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_verifier_test
MEMORY_SSA_ANALYSIS_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_analysis_test
MEMORY_SSA_PASS_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_pass_test
DUMP_MACHINE_STAGE_BIN := $(TOOLS_BUILD_DIR)/dump_machine_stage
DUMP_MIDDLE_STAGE_BIN := $(TOOLS_BUILD_DIR)/dump_middle_stage
DUMP_ALLOC_STAGE_BIN := $(TOOLS_BUILD_DIR)/dump_alloc_stage
PROFILE_BACKEND_LAYERS_BIN := $(TOOLS_BUILD_DIR)/profile_backend_layers
PROFILE_COMPILER_STAGES_BIN := $(TOOLS_BUILD_DIR)/profile_compiler_stages
DIAG_ALLOCATOR_STAGES_BIN := $(TOOLS_BUILD_DIR)/diag_allocator_stages
COMPILER_COMMON_LIB := $(COMMON_LIB_DIR)/libcompiler_common.a

LEXER_TEST_INPUT := tests/lexer/test.c
PARSER_TEST_INPUT := tests/parser/test.c

PARSER_SPLIT_INCLUDES := \
	src/parser/parser_ast_compat.inc \
	src/parser/parser_core_expr.inc \
	src/parser/parser_stmt_decl_tu.inc

SEMANTIC_SPLIT_INCLUDES := \
	src/semantic/semantic_core_flow.inc \
	src/semantic/semantic_callable_rules.inc \
	src/semantic/semantic_scope_rules.inc \
	src/semantic/semantic_entry.inc

IR_SPLIT_INCLUDES := \
	src/ir/ir_core.inc \
	src/ir/ir_lower_scope.inc \
	src/ir/ir_lower_expr.inc \
	src/ir/ir_lower_stmt.inc \
	src/ir/ir_global_dep.inc \
	src/ir/ir_global_init.inc \
	src/ir/ir_verify.inc \
	src/ir/ir_dump.inc

IR_PASS_SPLIT_INCLUDES := \
	src/ir_pass/ir_pass_core.inc \
	src/ir_pass/ir_pass_temp_analysis.inc \
	src/ir_pass/ir_pass_fold.inc \
	src/ir_pass/ir_pass_const.inc \
	src/ir_pass/ir_pass_copy.inc \
	src/ir_pass/ir_pass_cfg_analysis.inc \
	src/ir_pass/ir_pass_cfg.inc \
	src/ir_pass/ir_pass_dce.inc \
	src/ir_pass/ir_pass_pipeline.inc

IR_PASS_TEST_INCLUDES := \
	tests/ir/ir_pass_test_intellisense_prelude.inc \
	tests/ir/ir_pass_test_direct.inc \
	tests/ir/ir_pass_test_pipeline.inc

LOWER_IR_SPLIT_INCLUDES := \
	src/lower_ir/lower_from_ir.inc \
	src/lower_ir/lower_ir_analysis.inc \
	src/lower_ir/lower_ir_verify.inc \
	src/lower_ir/lower_ir_dump.inc

VALUE_SSA_SPLIT_INCLUDES := \
	src/value_ssa/value_ssa_verify.inc \
	src/value_ssa/value_ssa_dump.inc \
	src/value_ssa/value_ssa_analysis.inc \
	src/value_ssa/value_ssa_alloc_prep.inc \
	src/value_ssa/value_ssa_alloc_worklist.inc \
	src/value_ssa/value_ssa_alloc_dump.inc \
	src/value_ssa/value_ssa_rename.inc \
	src/value_ssa/value_ssa_from_lower_ir.inc

MEMORY_SSA_SPLIT_INCLUDES := \
	src/memory_ssa/memory_ssa_verify.inc \
	src/memory_ssa/memory_ssa_dump.inc \
	src/memory_ssa/memory_ssa_analysis.inc \
	src/memory_ssa/memory_ssa_from_value_ssa.inc

MEMORY_SSA_PASS_SPLIT_INCLUDES := \
	src/memory_ssa_pass/memory_ssa_pass_core.inc \
	src/memory_ssa_pass/memory_ssa_pass_slot_promotion.inc \
	src/memory_ssa_pass/memory_ssa_pass_load_forward.inc \
	src/memory_ssa_pass/memory_ssa_pass_store_cleanup.inc \
	src/memory_ssa_pass/memory_ssa_pass_local_scalar_replace.inc \
	src/memory_ssa_pass/memory_ssa_pass_global_scalar_replace.inc \
	src/memory_ssa_pass/memory_ssa_pass_scalar_replace.inc \
	src/memory_ssa_pass/memory_ssa_pass_memory_cse.inc \
	src/memory_ssa_pass/memory_ssa_pass_memory_value.inc \
	src/memory_ssa_pass/memory_ssa_pass_bridge.inc \
	src/memory_ssa_pass/memory_ssa_pass_pipeline.inc

VALUE_SSA_PASS_SPLIT_INCLUDES := \
	src/value_ssa_pass/value_ssa_simplify.inc \
	src/value_ssa_pass/value_ssa_load_forward.inc \
	src/value_ssa_pass/value_ssa_store_dce.inc \
	src/value_ssa_pass/value_ssa_normalize.inc \
	src/value_ssa_pass/value_ssa_identity.inc \
	src/value_ssa_pass/value_ssa_fold.inc \
	src/value_ssa_pass/value_ssa_cse.inc \
	src/value_ssa_pass/value_ssa_simplify_cfg.inc \
	src/value_ssa_pass/value_ssa_dce.inc \
	src/value_ssa_pass/value_ssa_pass_bridge.inc \
	src/value_ssa_pass/value_ssa_pass_pipeline.inc

VALUE_SSA_INTERP_SPLIT_INCLUDES := \
	src/value_ssa_interp/value_ssa_interp_state.inc \
	src/value_ssa_interp/value_ssa_interp_exec.inc

VALUE_SSA_ALLOC_SPLIT_INCLUDES := \
	src/value_ssa_alloc/value_ssa_alloc_core.inc \
	src/value_ssa_alloc/value_ssa_alloc_spill.inc \
	src/value_ssa_alloc/value_ssa_alloc_spill_cost.inc \
	src/value_ssa_alloc/value_ssa_alloc_coalesce.inc \
	src/value_ssa_alloc/value_ssa_alloc_move.inc \
	src/value_ssa_alloc/value_ssa_alloc_move_worklist.inc \
	src/value_ssa_alloc/value_ssa_alloc_plan.inc \
	src/value_ssa_alloc/value_ssa_alloc_move_engine.inc \
	src/value_ssa_alloc/value_ssa_alloc_move_transition.inc \
	src/value_ssa_alloc/value_ssa_alloc_select.inc \
	src/value_ssa_alloc/value_ssa_alloc_color.inc \
	src/value_ssa_alloc/value_ssa_alloc_rewrite.inc \
	src/value_ssa_alloc/value_ssa_alloc_layout.inc \
	src/value_ssa_alloc/value_ssa_alloc_layout_dump.inc \
	src/value_ssa_alloc/value_ssa_alloc_dump.inc

VALUE_SSA_MACHINE_SPLIT_INCLUDES := \
	src/value_ssa_machine/value_ssa_machine_core.inc \
	src/value_ssa_machine/value_ssa_machine_query.inc \
	src/value_ssa_machine/value_ssa_machine_dump.inc \
	src/value_ssa_machine/value_ssa_machine_call_clobber.inc \
	src/value_ssa_machine/value_ssa_machine_protection.inc \
	src/value_ssa_machine/value_ssa_machine_report.inc

MACHINE_IR_SPLIT_INCLUDES := \
	src/machine/lowering/machine_ir/machine_ir_core.inc \
	src/machine/lowering/machine_ir/machine_ir_query.inc \
	src/machine/lowering/machine_ir/machine_ir_verify.inc \
	src/machine/lowering/machine_ir/machine_ir_dump.inc \
	src/machine/lowering/machine_ir/machine_ir_lower.inc \
	src/machine/lowering/machine_ir/machine_ir_report.inc \
	src/machine/lowering/machine_ir/machine_ir_phi_elim.inc \
	src/machine/lowering/machine_ir/machine_ir_constraints.inc \
	src/machine/lowering/machine_ir/machine_ir_cleanup.inc \
	src/machine/lowering/machine_ir/machine_ir_call_effects.inc \
	src/machine/lowering/machine_ir/machine_ir_slot_cleanup.inc \
	src/machine/lowering/machine_ir/machine_ir_copy_cleanup.inc \
	src/machine/lowering/machine_ir/machine_ir_value_cleanup.inc

MACHINE_SELECT_SPLIT_INCLUDES := \
	src/machine/lowering/machine_select/machine_select_core.inc \
	src/machine/lowering/machine_select/machine_select_query.inc \
	src/machine/lowering/machine_select/machine_select_verify.inc \
	src/machine/lowering/machine_select/machine_select_dump.inc \
	src/machine/lowering/machine_select/machine_select_lower_value.inc \
	src/machine/lowering/machine_select/machine_select_lower_memory.inc \
	src/machine/lowering/machine_select/machine_select_lower_call.inc \
	src/machine/lowering/machine_select/machine_select_lower_control.inc \
	src/machine/lowering/machine_select/machine_select_lower.inc \
	src/machine/lowering/machine_select/machine_select_cleanup.inc \
	src/machine/lowering/machine_select/machine_select_report.inc

MACHINE_LAYOUT_SPLIT_INCLUDES := \
	src/machine/lowering/machine_layout/machine_layout_core.inc \
	src/machine/lowering/machine_layout/machine_layout_verify.inc \
	src/machine/lowering/machine_layout/machine_layout_dump.inc \
	src/machine/lowering/machine_layout/machine_layout_lower.inc

MACHINE_EMIT_SPLIT_INCLUDES := \
	src/machine/lowering/machine_emit/machine_emit_core.inc \
	src/machine/lowering/machine_emit/machine_emit_query.inc \
	src/machine/lowering/machine_emit/machine_emit_verify.inc \
	src/machine/lowering/machine_emit/machine_emit_dump.inc \
	src/machine/lowering/machine_emit/machine_emit_lower.inc \
	src/machine/lowering/machine_emit/machine_emit_report.inc

MACHINE_ENCODE_SPLIT_INCLUDES := \
	src/machine/lowering/machine_encode/machine_encode_core.inc \
	src/machine/lowering/machine_encode/machine_encode_query.inc \
	src/machine/lowering/machine_encode/machine_encode_verify.inc \
	src/machine/lowering/machine_encode/machine_encode_dump.inc \
	src/machine/lowering/machine_encode/machine_encode_lower.inc \
	src/machine/lowering/machine_encode/machine_encode_report.inc

COMPILER_COMMON_SRCS := $(filter-out src/compiler/main.c,$(shell find src -name '*.c' | sort))
COMPILER_COMMON_BUILD_DEPS := $(shell find include -name '*.h' | sort) $(shell find src -name '*.inc' | sort)
COMPILER_COMMON_OBJS := $(patsubst src/%.c,$(COMMON_OBJ_DIR)/%.o,$(COMPILER_COMMON_SRCS))
TEST_JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
TEST_LINK_CMD = @mkdir -p $(dir $@) && $(CC) $(CFLAGS) $(filter tests/%.c,$^) $(COMPILER_COMMON_LIB) -o $@
ALL_TEST_BINS := \
	$(LEXER_TEST_BIN) \
	$(LEXER_REGRESSION_BIN) \
	$(PARSER_TEST_BIN) \
	$(PARSER_REGRESSION_BIN) \
	$(PARSER_LEGACY_LINK_BIN) \
	$(SEMANTIC_REGRESSION_BIN) \
	$(IR_REGRESSION_BIN) \
	$(IR_VERIFIER_BIN) \
	$(IR_PASS_BIN) \
	$(LOWER_IR_REGRESSION_BIN) \
	$(LOWER_IR_VERIFIER_BIN) \
	$(VALUE_SSA_REGRESSION_BIN) \
	$(VALUE_SSA_VERIFIER_BIN) \
	$(VALUE_SSA_ANALYSIS_BIN) \
	$(VALUE_SSA_INTERP_BIN) \
	$(VALUE_SSA_ORACLE_BIN) \
	$(VALUE_SSA_ALLOC_BIN) \
	$(VALUE_SSA_MACHINE_BIN) \
	$(MACHINE_IR_BIN) \
	$(MACHINE_SELECT_BIN) \
	$(MACHINE_LAYOUT_BIN) \
	$(MACHINE_EMIT_BIN) \
	$(MACHINE_ENCODE_BIN) \
	$(MACHINE_BYTES_BIN) \
	$(MACHINE_OBJECT_BIN) \
	$(MACHINE_RELOC_BIN) \
	$(MACHINE_CONTAINER_BIN) \
	$(MACHINE_ELF_BIN) \
	$(MACHINE_IMAGE_BIN) \
	$(MACHINE_EXEC_BIN) \
	$(MACHINE_LOAD_BIN) \
	$(MACHINE_RUNTIME_BIN) \
	$(MACHINE_LAUNCH_BIN) \
	$(MACHINE_STEP_BIN) \
	$(MACHINE_DECODE_BIN) \
	$(MACHINE_PAYLOAD_DECODE_BIN) \
	$(MACHINE_INTERP_BIN) \
	$(MACHINE_TRANSITION_BIN) \
	$(MACHINE_STATE_BIN) \
	$(MACHINE_MUTATION_BIN) \
	$(MACHINE_WRITEBACK_BIN) \
	$(MACHINE_COMMIT_BIN) \
	$(MACHINE_APPLY_BIN) \
	$(MACHINE_OBSERVE_BIN) \
	$(MACHINE_DELTA_BIN) \
	$(MACHINE_TRACE_BIN) \
	$(MACHINE_EVENT_BIN) \
	$(MACHINE_OUTCOME_BIN) \
	$(MACHINE_HISTORY_BIN) \
	$(MACHINE_TIMELINE_BIN) \
	$(MACHINE_LOG_BIN) \
	$(MACHINE_JOURNAL_BIN) \
	$(COMPILER_DRIVER_TEST_BIN) \
	$(MEMORY_SSA_REGRESSION_BIN) \
	$(MEMORY_SSA_VERIFIER_BIN) \
	$(MEMORY_SSA_ANALYSIS_BIN) \
	$(MEMORY_SSA_PASS_BIN)
TEST_TARGETS := \
	test-lexer \
	test-lexer-regression \
	test-parser \
	test-parser-regression \
	test-parser-legacy-link \
	test-semantic-regression \
	test-ir-regression \
	test-ir-verifier \
	test-ir-pass \
	test-lower-ir-regression \
	test-lower-ir-verifier \
	test-value-ssa-regression \
	test-value-ssa-verifier \
	test-value-ssa-analysis \
	test-value-ssa-interp \
	test-value-ssa-oracle \
	test-value-ssa-alloc \
	test-value-ssa-machine \
	test-machine-ir \
	test-machine-select \
	test-machine-layout \
	test-machine-emit \
	test-machine-encode \
	test-machine-bytes \
	test-machine-object \
	test-machine-reloc \
	test-machine-container \
	test-machine-elf \
	test-machine-image \
	test-machine-exec \
	test-machine-load \
	test-machine-runtime \
	test-machine-launch \
	test-machine-step \
	test-machine-decode \
	test-machine-payload-decode \
	test-machine-interp \
	test-machine-transition \
	test-machine-state \
	test-machine-mutation \
	test-machine-writeback \
	test-machine-commit \
	test-machine-apply \
	test-machine-observe \
	test-machine-delta \
	test-machine-trace \
	test-machine-event \
	test-machine-outcome \
	test-machine-history \
	test-machine-timeline \
	test-machine-log \
	test-machine-journal \
	test-compiler-driver \
	test-compiler-cli \
	test-compiler-asm \
	test-memory-ssa-regression \
	test-memory-ssa-verifier \
	test-memory-ssa-analysis \
	test-memory-ssa-pass

.PHONY: all check dirs force lexer parser compiler tools dump-machine-stage dump-middle-stage dump-alloc-stage profile-backend-layers profile-compiler-stages diag-allocator-stages test test-lexer test-lexer-regression test-parser test-parser-regression test-parser-legacy-link test-semantic-regression test-ir-regression test-ir-verifier test-ir-pass test-lower-ir-regression test-lower-ir-verifier test-value-ssa-regression test-value-ssa-verifier test-value-ssa-analysis test-value-ssa-interp test-value-ssa-oracle test-value-ssa-alloc test-value-ssa-machine test-machine-ir test-machine-select test-machine-layout test-machine-emit test-machine-encode test-machine-bytes test-machine-object test-machine-reloc test-machine-container test-machine-elf test-machine-image test-machine-exec test-machine-load test-machine-runtime test-machine-launch test-machine-step test-machine-decode test-machine-payload-decode test-machine-interp test-machine-transition test-machine-state test-machine-mutation test-machine-writeback test-machine-commit test-machine-apply test-machine-observe test-machine-delta test-machine-trace test-machine-event test-machine-outcome test-machine-history test-machine-timeline test-machine-log test-machine-journal test-compiler-driver test-compiler-cli test-compiler-asm test-memory-ssa-regression test-memory-ssa-verifier test-memory-ssa-analysis test-memory-ssa-pass test-fanalyzer test-asan test-strict-warnings clean
PARSER_REGRESSION_INCLUDES := \
	tests/parser/parser_regression_intellisense_prelude.inc \
	tests/parser/parser_regression_cases_core.inc \
	tests/parser/parser_regression_cases_expr_ast_a.inc \
	tests/parser/parser_regression_cases_expr_ast_b.inc \
	tests/parser/parser_regression_cases_ast_meta.inc

SEMANTIC_REGRESSION_INCLUDES := \
	tests/semantic/semantic_regression_intellisense_prelude.inc \
	tests/semantic/semantic_regression_callable_flow.inc \
	tests/semantic/semantic_regression_scope_cf.inc

all: compiler

check: test

force:

dirs:
	@mkdir -p $(COMMON_LIB_DIR) $(LEXER_BUILD_DIR) $(PARSER_BUILD_DIR) $(SEMANTIC_BUILD_DIR) $(IR_BUILD_DIR) $(LOWER_IR_BUILD_DIR) $(VALUE_SSA_BUILD_DIR) $(MEMORY_SSA_BUILD_DIR) $(MACHINE_IR_BUILD_DIR) $(MACHINE_SELECT_BUILD_DIR) $(MACHINE_LAYOUT_BUILD_DIR) $(MACHINE_EMIT_BUILD_DIR) $(MACHINE_ENCODE_BUILD_DIR) $(MACHINE_BYTES_BUILD_DIR) $(MACHINE_OBJECT_BUILD_DIR) $(MACHINE_RELOC_BUILD_DIR) $(MACHINE_CONTAINER_BUILD_DIR) $(MACHINE_ELF_BUILD_DIR) $(MACHINE_IMAGE_BUILD_DIR) $(MACHINE_EXEC_BUILD_DIR) $(MACHINE_LOAD_BUILD_DIR) $(MACHINE_RUNTIME_BUILD_DIR) $(MACHINE_LAUNCH_BUILD_DIR) $(MACHINE_STEP_BUILD_DIR) $(MACHINE_DECODE_BUILD_DIR) $(MACHINE_PAYLOAD_DECODE_BUILD_DIR) $(MACHINE_INTERP_BUILD_DIR) $(MACHINE_TRANSITION_BUILD_DIR) $(MACHINE_STATE_BUILD_DIR) $(MACHINE_MUTATION_BUILD_DIR) $(MACHINE_WRITEBACK_BUILD_DIR) $(MACHINE_COMMIT_BUILD_DIR) $(MACHINE_APPLY_BUILD_DIR) $(MACHINE_OBSERVE_BUILD_DIR) $(MACHINE_DELTA_BUILD_DIR) $(MACHINE_TRACE_BUILD_DIR) $(MACHINE_EVENT_BUILD_DIR) $(MACHINE_OUTCOME_BUILD_DIR) $(MACHINE_HISTORY_BUILD_DIR) $(MACHINE_TIMELINE_BUILD_DIR) $(MACHINE_LOG_BUILD_DIR) $(MACHINE_JOURNAL_BUILD_DIR) $(COMPILER_TEST_BUILD_DIR) $(TOOLS_BUILD_DIR)

$(COMMON_OBJ_DIR)/%.o: src/%.c $(COMPILER_COMMON_BUILD_DEPS) | dirs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(COMPILER_COMMON_LIB): $(COMPILER_COMMON_OBJS) | dirs
	$(AR) rcs $@ $^

$(ALL_TEST_BINS): $(COMPILER_COMMON_LIB)

lexer: $(LEXER_TEST_BIN)

parser: $(PARSER_TEST_BIN)

compiler: $(COMPILER_BIN) tools

$(LEXER_TEST_BIN): src/lexer/lexer.c tests/lexer/lexer_test.c include/lexer.h | dirs
	$(TEST_LINK_CMD)

$(LEXER_REGRESSION_BIN): src/lexer/lexer.c tests/lexer/lexer_regression_test.c include/lexer.h | dirs
	$(TEST_LINK_CMD)

$(PARSER_TEST_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/parser/parser_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h | dirs
	$(TEST_LINK_CMD)

$(PARSER_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c tests/parser/parser_regression_test.c $(PARSER_SPLIT_INCLUDES) $(PARSER_REGRESSION_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h | dirs
	$(TEST_LINK_CMD)

$(PARSER_LEGACY_LINK_BIN): src/lexer/lexer.c src/parser/parser.c tests/parser/parser_legacy_link_test.c $(PARSER_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h | dirs
	$(TEST_LINK_CMD)

$(SEMANTIC_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/semantic/semantic_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(SEMANTIC_REGRESSION_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h | dirs
	$(TEST_LINK_CMD)

$(IR_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h | dirs
	$(TEST_LINK_CMD)

$(IR_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_verifier_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h | dirs
	$(TEST_LINK_CMD)

$(IR_PASS_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/ir_pass/ir_pass.c tests/ir/ir_pass_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(IR_PASS_SPLIT_INCLUDES) $(IR_PASS_TEST_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/ir_pass.h | dirs
	$(TEST_LINK_CMD)

$(LOWER_IR_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/lower_ir.h | dirs
	$(TEST_LINK_CMD)

$(LOWER_IR_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_verifier_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/lower_ir.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_regression_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_interp.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_verifier_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_ANALYSIS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_analysis_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_INTERP_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_interp_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_interp.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_ORACLE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_oracle_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_interp.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_ALLOC_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_alloc_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(VALUE_SSA_MACHINE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_machine_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_IR_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/lowering/machine_ir/machine_ir_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_SELECT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/lowering/machine_select/machine_select_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_LAYOUT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/lowering/machine_layout/machine_layout_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_EMIT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/lowering/machine_emit/machine_emit_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_ENCODE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/lowering/machine_encode/machine_encode_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_BYTES_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/object/machine_bytes/machine_bytes_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_OBJECT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/object/machine_object/machine_object_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_RELOC_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/object/machine_reloc/machine_reloc_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_CONTAINER_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/object/machine_container/machine_container_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_ELF_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/object/machine_elf/machine_elf_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_IMAGE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_image/machine_image_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_EXEC_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_exec/machine_exec_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_LOAD_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_load/machine_load_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_RUNTIME_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_runtime/machine_runtime_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_LAUNCH_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_launch/machine_launch_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_STEP_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_step/machine_step_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_DECODE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_decode/machine_decode_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_PAYLOAD_DECODE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_payload_decode/machine_payload_decode_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_INTERP_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_interp/machine_interp_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_TRANSITION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_transition/machine_transition_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_STATE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_state/machine_state_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_MUTATION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_mutation/machine_mutation_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_WRITEBACK_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_writeback/machine_writeback_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_COMMIT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_commit/machine_commit_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_APPLY_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/runtime/machine_apply/machine_apply_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_OBSERVE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_observe/machine_observe_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_DELTA_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_delta/machine_delta_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_TRACE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_trace/machine_trace_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_EVENT_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_event/machine_event_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_OUTCOME_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/machine/observe/machine_outcome/machine_outcome.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_outcome/machine_outcome_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/machine/outcome.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_HISTORY_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/machine/observe/machine_outcome/machine_outcome.c src/machine/observe/machine_history/machine_history.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_history/machine_history_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/machine/outcome.h include/machine/history.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_TIMELINE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/machine/observe/machine_outcome/machine_outcome.c src/machine/observe/machine_history/machine_history.c src/machine/observe/machine_timeline/machine_timeline.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_timeline/machine_timeline_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/machine/outcome.h include/machine/history.h include/machine/timeline.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_LOG_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/machine/observe/machine_outcome/machine_outcome.c src/machine/observe/machine_history/machine_history.c src/machine/observe/machine_timeline/machine_timeline.c src/machine/observe/machine_log/machine_log.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_log/machine_log_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/machine/outcome.h include/machine/history.h include/machine/timeline.h include/machine/log.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MACHINE_JOURNAL_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_alloc/value_ssa_alloc.c src/value_ssa_machine/value_ssa_machine.c src/machine/lowering/machine_ir/machine_ir.c src/machine/lowering/machine_select/machine_select.c src/machine/lowering/machine_layout/machine_layout.c src/machine/lowering/machine_emit/machine_emit.c src/machine/lowering/machine_encode/machine_encode.c src/machine/object/machine_bytes/machine_bytes.c src/machine/object/machine_object/machine_object.c src/machine/object/machine_reloc/machine_reloc.c src/machine/object/machine_container/machine_container.c src/machine/object/machine_elf/machine_elf.c src/machine/runtime/machine_image/machine_image.c src/machine/runtime/machine_exec/machine_exec.c src/machine/runtime/machine_load/machine_load.c src/machine/runtime/machine_runtime/machine_runtime.c src/machine/runtime/machine_launch/machine_launch.c src/machine/runtime/machine_step/machine_step.c src/machine/runtime/machine_decode/machine_decode.c src/machine/runtime/machine_payload_decode/machine_payload_decode.c src/machine/runtime/machine_interp/machine_interp.c src/machine/runtime/machine_transition/machine_transition.c src/machine/runtime/machine_state/machine_state.c src/machine/runtime/machine_mutation/machine_mutation.c src/machine/runtime/machine_writeback/machine_writeback.c src/machine/runtime/machine_commit/machine_commit.c src/machine/runtime/machine_apply/machine_apply.c src/machine/observe/machine_observe/machine_observe.c src/machine/observe/machine_delta/machine_delta.c src/machine/observe/machine_trace/machine_trace.c src/machine/observe/machine_event/machine_event.c src/machine/observe/machine_outcome/machine_outcome.c src/machine/observe/machine_history/machine_history.c src/machine/observe/machine_timeline/machine_timeline.c src/machine/observe/machine_log/machine_log.c src/machine/observe/machine_journal/machine_journal.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/machine/observe/machine_journal/machine_journal_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_ALLOC_SPLIT_INCLUDES) $(VALUE_SSA_MACHINE_SPLIT_INCLUDES) $(MACHINE_IR_SPLIT_INCLUDES) $(MACHINE_SELECT_SPLIT_INCLUDES) $(MACHINE_LAYOUT_SPLIT_INCLUDES) $(MACHINE_EMIT_SPLIT_INCLUDES) $(MACHINE_ENCODE_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_alloc.h include/value_ssa_machine.h include/machine/ir.h include/machine/select.h include/machine/layout.h include/machine/emit.h include/machine/encode.h include/machine/bytes.h include/machine/object.h include/machine/reloc.h include/machine/container.h include/machine/elf.h include/machine/image.h include/machine/exec.h include/machine/load.h include/machine/runtime.h include/machine/launch.h include/machine/step.h include/machine/decode.h include/machine/payload_decode.h include/machine/interp.h include/machine/transition.h include/machine/state.h include/machine/mutation.h include/machine/writeback.h include/machine/commit.h include/machine/apply.h include/machine/observe.h include/machine/delta.h include/machine/trace.h include/machine/event.h include/machine/outcome.h include/machine/history.h include/machine/timeline.h include/machine/log.h include/machine/journal.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(MEMORY_SSA_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_regression_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(TEST_LINK_CMD)

$(MEMORY_SSA_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_verifier_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(TEST_LINK_CMD)

$(MEMORY_SSA_ANALYSIS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_analysis_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(TEST_LINK_CMD)

$(MEMORY_SSA_PASS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_pass_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(TEST_LINK_CMD)

$(COMPILER_BIN): force src/compiler/main.c $(COMPILER_COMMON_LIB) include/compiler.h | dirs
	@if [ -d "$@" ]; then rm -rf "$@"; fi
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) src/compiler/main.c $(COMPILER_COMMON_LIB) -o $@

$(COMPILER_DRIVER_TEST_BIN): tests/compiler/compiler_driver_test.c $(COMPILER_COMMON_LIB) include/compiler.h | dirs
	$(TEST_LINK_CMD)

$(DUMP_MACHINE_STAGE_BIN): tools/dump_machine_stage.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/dump_machine_stage.c $(COMPILER_COMMON_LIB) -o $@

$(DUMP_MIDDLE_STAGE_BIN): tools/dump_middle_stage.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/dump_middle_stage.c $(COMPILER_COMMON_LIB) -o $@

$(DUMP_ALLOC_STAGE_BIN): tools/dump_alloc_stage.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/dump_alloc_stage.c $(COMPILER_COMMON_LIB) -o $@

$(PROFILE_BACKEND_LAYERS_BIN): tools/profile_backend_layers.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/profile_backend_layers.c $(COMPILER_COMMON_LIB) -o $@

$(PROFILE_COMPILER_STAGES_BIN): tools/profile_compiler_stages.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/profile_compiler_stages.c $(COMPILER_COMMON_LIB) -o $@

$(DIAG_ALLOCATOR_STAGES_BIN): tools/diag_allocator_stages.c $(COMPILER_COMMON_LIB) $(COMPILER_COMMON_BUILD_DEPS) | dirs
	$(CC) $(CFLAGS) tools/diag_allocator_stages.c $(COMPILER_COMMON_LIB) -o $@

tools: $(DUMP_MACHINE_STAGE_BIN) $(DUMP_MIDDLE_STAGE_BIN) $(DUMP_ALLOC_STAGE_BIN) $(PROFILE_BACKEND_LAYERS_BIN) $(PROFILE_COMPILER_STAGES_BIN) $(DIAG_ALLOCATOR_STAGES_BIN)

dump-machine-stage: $(DUMP_MACHINE_STAGE_BIN)

dump-middle-stage: $(DUMP_MIDDLE_STAGE_BIN)

dump-alloc-stage: $(DUMP_ALLOC_STAGE_BIN)

profile-backend-layers: $(PROFILE_BACKEND_LAYERS_BIN)

profile-compiler-stages: $(PROFILE_COMPILER_STAGES_BIN)

diag-allocator-stages: $(DIAG_ALLOCATOR_STAGES_BIN)

test-lexer: $(LEXER_TEST_BIN)
	@echo "[lexer] running $(LEXER_TEST_INPUT)"
	@tmp=$$(mktemp "./$(LEXER_TEST_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(LEXER_TEST_BIN)" "$$tmp" && "$$tmp" $(LEXER_TEST_INPUT); \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lexer-regression: $(LEXER_REGRESSION_BIN)
	@echo "[lexer] running regression tests"
	@tmp=$$(mktemp "./$(LEXER_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(LEXER_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser: $(PARSER_TEST_BIN)
	@echo "[parser] running $(PARSER_TEST_INPUT)"
	@tmp=$$(mktemp "./$(PARSER_TEST_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(PARSER_TEST_BIN)" "$$tmp" && "$$tmp" $(PARSER_TEST_INPUT); \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser-regression: $(PARSER_REGRESSION_BIN)
	@echo "[parser] running regression tests"
	@tmp=$$(mktemp "./$(PARSER_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(PARSER_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser-legacy-link: $(PARSER_LEGACY_LINK_BIN)
	@echo "[parser] running legacy-link test"
	@tmp=$$(mktemp "./$(PARSER_LEGACY_LINK_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(PARSER_LEGACY_LINK_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-semantic-regression: $(SEMANTIC_REGRESSION_BIN)
	@echo "[semantic] running regression tests"
	@tmp=$$(mktemp "./$(SEMANTIC_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(SEMANTIC_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-regression: $(IR_REGRESSION_BIN)
	@echo "[ir] running regression tests"
	@tmp=$$(mktemp "./$(IR_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(IR_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-verifier: $(IR_VERIFIER_BIN)
	@echo "[ir] running verifier tests"
	@tmp=$$(mktemp "./$(IR_VERIFIER_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(IR_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-pass: $(IR_PASS_BIN)
	@echo "[ir] running pass tests"
	@tmp=$$(mktemp "./$(IR_PASS_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(IR_PASS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lower-ir-regression: $(LOWER_IR_REGRESSION_BIN)
	@echo "[lower-ir] running regression tests"
	@tmp=$$(mktemp "./$(LOWER_IR_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(LOWER_IR_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lower-ir-verifier: $(LOWER_IR_VERIFIER_BIN)
	@echo "[lower-ir] running verifier tests"
	@tmp=$$(mktemp "./$(LOWER_IR_VERIFIER_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(LOWER_IR_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-regression: $(VALUE_SSA_REGRESSION_BIN)
	@echo "[value-ssa] running regression tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-verifier: $(VALUE_SSA_VERIFIER_BIN)
	@echo "[value-ssa] running verifier tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_VERIFIER_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-analysis: $(VALUE_SSA_ANALYSIS_BIN)
	@echo "[value-ssa] running analysis tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_ANALYSIS_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_ANALYSIS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-interp: $(VALUE_SSA_INTERP_BIN)
	@echo "[value-ssa] running interpreter tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_INTERP_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_INTERP_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-oracle: $(VALUE_SSA_ORACLE_BIN)
	@echo "[value-ssa] running oracle tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_ORACLE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_ORACLE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-alloc: $(VALUE_SSA_ALLOC_BIN)
	@echo "[value-ssa] running allocator tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_ALLOC_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_ALLOC_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-machine: $(VALUE_SSA_MACHINE_BIN)
	@echo "[value-ssa] running machine-model tests"
	@tmp=$$(mktemp "./$(VALUE_SSA_MACHINE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(VALUE_SSA_MACHINE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-ir: $(MACHINE_IR_BIN)
	@echo "[machine-ir] running tests"
	@tmp=$$(mktemp "./$(MACHINE_IR_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_IR_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-select: $(MACHINE_SELECT_BIN)
	@echo "[machine-select] running tests"
	@tmp=$$(mktemp "./$(MACHINE_SELECT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_SELECT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-layout: $(MACHINE_LAYOUT_BIN)
	@echo "[machine-layout] running tests"
	@tmp=$$(mktemp "./$(MACHINE_LAYOUT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_LAYOUT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-emit: $(MACHINE_EMIT_BIN)
	@echo "[machine-emit] running tests"
	@tmp=$$(mktemp "./$(MACHINE_EMIT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_EMIT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-encode: $(MACHINE_ENCODE_BIN)
	@echo "[machine-encode] running tests"
	@tmp=$$(mktemp "./$(MACHINE_ENCODE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_ENCODE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-bytes: $(MACHINE_BYTES_BIN)
	@echo "[machine-bytes] running tests"
	@tmp=$$(mktemp "./$(MACHINE_BYTES_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_BYTES_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-object: $(MACHINE_OBJECT_BIN)
	@echo "[machine-object] running tests"
	@tmp=$$(mktemp "./$(MACHINE_OBJECT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_OBJECT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-reloc: $(MACHINE_RELOC_BIN)
	@echo "[machine-reloc] running tests"
	@tmp=$$(mktemp "./$(MACHINE_RELOC_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_RELOC_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-container: $(MACHINE_CONTAINER_BIN)
	@echo "[machine-container] running tests"
	@tmp=$$(mktemp "./$(MACHINE_CONTAINER_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_CONTAINER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-elf: $(MACHINE_ELF_BIN)
	@echo "[machine-elf] running tests"
	@tmp=$$(mktemp "./$(MACHINE_ELF_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_ELF_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-image: $(MACHINE_IMAGE_BIN)
	@echo "[machine-image] running tests"
	@tmp=$$(mktemp "./$(MACHINE_IMAGE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_IMAGE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-exec: $(MACHINE_EXEC_BIN)
	@echo "[machine-exec] running tests"
	@tmp=$$(mktemp "./$(MACHINE_EXEC_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_EXEC_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-load: $(MACHINE_LOAD_BIN)
	@echo "[machine-load] running tests"
	@tmp=$$(mktemp "./$(MACHINE_LOAD_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_LOAD_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-runtime: $(MACHINE_RUNTIME_BIN)
	@echo "[machine-runtime] running tests"
	@tmp=$$(mktemp "./$(MACHINE_RUNTIME_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_RUNTIME_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-launch: $(MACHINE_LAUNCH_BIN)
	@echo "[machine-launch] running tests"
	@tmp=$$(mktemp "./$(MACHINE_LAUNCH_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_LAUNCH_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-step: $(MACHINE_STEP_BIN)
	@echo "[machine-step] running tests"
	@tmp=$$(mktemp "./$(MACHINE_STEP_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_STEP_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-decode: $(MACHINE_DECODE_BIN)
	@echo "[machine-decode] running tests"
	@tmp=$$(mktemp "./$(MACHINE_DECODE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_DECODE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-payload-decode: $(MACHINE_PAYLOAD_DECODE_BIN)
	@echo "[machine-payload-decode] running tests"
	@tmp=$$(mktemp "./$(MACHINE_PAYLOAD_DECODE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_PAYLOAD_DECODE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-interp: $(MACHINE_INTERP_BIN)
	@echo "[machine-interp] running tests"
	@tmp=$$(mktemp "./$(MACHINE_INTERP_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_INTERP_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-transition: $(MACHINE_TRANSITION_BIN)
	@echo "[machine-transition] running tests"
	@tmp=$$(mktemp "./$(MACHINE_TRANSITION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_TRANSITION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-state: $(MACHINE_STATE_BIN)
	@echo "[machine-state] running tests"
	@tmp=$$(mktemp "./$(MACHINE_STATE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_STATE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-mutation: $(MACHINE_MUTATION_BIN)
	@echo "[machine-mutation] running tests"
	@tmp=$$(mktemp "./$(MACHINE_MUTATION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_MUTATION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-writeback: $(MACHINE_WRITEBACK_BIN)
	@echo "[machine-writeback] running tests"
	@tmp=$$(mktemp "./$(MACHINE_WRITEBACK_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_WRITEBACK_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-commit: $(MACHINE_COMMIT_BIN)
	@echo "[machine-commit] running tests"
	@tmp=$$(mktemp "./$(MACHINE_COMMIT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_COMMIT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-apply: $(MACHINE_APPLY_BIN)
	@echo "[machine-apply] running tests"
	@tmp=$$(mktemp "./$(MACHINE_APPLY_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_APPLY_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-observe: $(MACHINE_OBSERVE_BIN)
	@echo "[machine-observe] running tests"
	@tmp=$$(mktemp "./$(MACHINE_OBSERVE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_OBSERVE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-delta: $(MACHINE_DELTA_BIN)
	@echo "[machine-delta] running tests"
	@tmp=$$(mktemp "./$(MACHINE_DELTA_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_DELTA_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-trace: $(MACHINE_TRACE_BIN)
	@echo "[machine-trace] running tests"
	@tmp=$$(mktemp "./$(MACHINE_TRACE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_TRACE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-event: $(MACHINE_EVENT_BIN)
	@echo "[machine-event] running tests"
	@tmp=$$(mktemp "./$(MACHINE_EVENT_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_EVENT_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-outcome: $(MACHINE_OUTCOME_BIN)
	@echo "[machine-outcome] running tests"
	@tmp=$$(mktemp "./$(MACHINE_OUTCOME_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_OUTCOME_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-history: $(MACHINE_HISTORY_BIN)
	@echo "[machine-history] running tests"
	@tmp=$$(mktemp "./$(MACHINE_HISTORY_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_HISTORY_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-timeline: $(MACHINE_TIMELINE_BIN)
	@echo "[machine-timeline] running tests"
	@tmp=$$(mktemp "./$(MACHINE_TIMELINE_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_TIMELINE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-log: $(MACHINE_LOG_BIN)
	@echo "[machine-log] running tests"
	@tmp=$$(mktemp "./$(MACHINE_LOG_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_LOG_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-machine-journal: $(MACHINE_JOURNAL_BIN)
	@echo "[machine-journal] running tests"
	@tmp=$$(mktemp "./$(MACHINE_JOURNAL_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MACHINE_JOURNAL_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-compiler-driver: $(COMPILER_DRIVER_TEST_BIN)
	@echo "[compiler] running driver regression tests"
	@tmp=$$(mktemp "./$(COMPILER_DRIVER_TEST_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(COMPILER_DRIVER_TEST_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-compiler-cli: $(COMPILER_BIN)
	@echo "[compiler] running cli smoke test"
	@tmpbin="./$(COMPILER_BIN).cli.run.$$$$"; \
	tmpdir="./$(COMPILER_TEST_BUILD_DIR)/cli-smoke.$$$$"; \
	mkdir -p "$$tmpdir" && \
	cp "./$(COMPILER_BIN)" "$$tmpbin" && \
	printf '%s\n' 'int main(){return 0;}' > "$$tmpdir/input.sy" && \
	"$$tmpbin" -riscv "$$tmpdir/input.sy" -o "$$tmpdir/output.s" && \
	test "$$(sed -n '1p' "$$tmpdir/output.s")" = ".attribute arch, \"rv32im\"" && \
	test "$$(sed -n '2p' "$$tmpdir/output.s")" = ".text" && \
	grep -Fxq "main:" "$$tmpdir/output.s" && \
	grep -Fxq "  li a0, 0" "$$tmpdir/output.s"; \
	status=$$?; \
	rm -f "$$tmpbin"; \
	rm -rf "$$tmpdir"; \
	exit $$status

test-compiler-asm: $(COMPILER_BIN)
	@echo "[compiler] running asm acceptance test"
	@tmpbin="./$(COMPILER_BIN).asm.run.$$$$"; \
	tmpdir="./$(COMPILER_TEST_BUILD_DIR)/asm-smoke.$$$$"; \
	mkdir -p "$$tmpdir" && \
	cp "./$(COMPILER_BIN)" "$$tmpbin" && \
	printf '%s\n' 'int choose(int x){ if (x) return 2; return 3; } int ext(int a); int main(){ return ext(choose(1)); }' > "$$tmpdir/control.sy" && \
	printf '%s\n' 'int g; int main(){ return g; }' > "$$tmpdir/global_bss.sy" && \
	printf '%s\n' 'int g = 7; int main(){ return g; }' > "$$tmpdir/global_data.sy" && \
	printf '%s\n' 'int g; int set(){ g = 5; return 0; } int main(){ set(); return g; }' > "$$tmpdir/global_store.sy" && \
	"$$tmpbin" -riscv "$$tmpdir/control.sy" -o "$$tmpdir/control.s" && \
	"$$tmpbin" -riscv "$$tmpdir/global_bss.sy" -o "$$tmpdir/global_bss.s" && \
	"$$tmpbin" -riscv "$$tmpdir/global_data.sy" -o "$$tmpdir/global_data.s" && \
	"$$tmpbin" -riscv "$$tmpdir/global_store.sy" -o "$$tmpdir/global_store.s" && \
	llvm-mc -triple=riscv32 -filetype=obj "$$tmpdir/control.s" -o "$$tmpdir/control.o" && \
	llvm-mc -triple=riscv32 -filetype=obj "$$tmpdir/global_bss.s" -o "$$tmpdir/global_bss.o" && \
	llvm-mc -triple=riscv32 -filetype=obj "$$tmpdir/global_data.s" -o "$$tmpdir/global_data.o" && \
	llvm-mc -triple=riscv32 -filetype=obj "$$tmpdir/global_store.s" -o "$$tmpdir/global_store.o"; \
	status=$$?; \
	rm -f "$$tmpbin"; \
	rm -rf "$$tmpdir"; \
	exit $$status

test-memory-ssa-regression: $(MEMORY_SSA_REGRESSION_BIN)
	@echo "[memory-ssa] running regression tests"
	@tmp=$$(mktemp "./$(MEMORY_SSA_REGRESSION_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MEMORY_SSA_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-verifier: $(MEMORY_SSA_VERIFIER_BIN)
	@echo "[memory-ssa] running verifier tests"
	@tmp=$$(mktemp "./$(MEMORY_SSA_VERIFIER_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MEMORY_SSA_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-analysis: $(MEMORY_SSA_ANALYSIS_BIN)
	@echo "[memory-ssa] running analysis tests"
	@tmp=$$(mktemp "./$(MEMORY_SSA_ANALYSIS_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MEMORY_SSA_ANALYSIS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-pass: $(MEMORY_SSA_PASS_BIN)
	@echo "[memory-ssa] running pass tests"
	@tmp=$$(mktemp "./$(MEMORY_SSA_PASS_BIN).run.XXXXXX"); \
	rm -f "$$tmp" && \
	cp "./$(MEMORY_SSA_PASS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test:
	@$(MAKE) --no-print-directory -j$(TEST_JOBS) $(TEST_TARGETS)

test-fanalyzer:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory test CFLAGS='$(FANALYZER_CFLAGS)'

test-asan:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory test CFLAGS='$(ASAN_CFLAGS)'

test-strict-warnings:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory test CFLAGS='$(STRICT_WARN_CFLAGS)'

clean:
	rm -rf $(BUILD_DIR)
