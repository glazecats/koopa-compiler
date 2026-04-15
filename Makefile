CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Iinclude
FANALYZER_CFLAGS := -std=c11 -Wall -Wextra -fanalyzer -Iinclude
ASAN_CFLAGS := -std=c11 -Wall -Wextra -fsanitize=address -fno-omit-frame-pointer -g -Iinclude
STRICT_WARN_CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wformat=2 -Iinclude

BUILD_DIR := build
LEXER_BUILD_DIR := $(BUILD_DIR)/lexer
PARSER_BUILD_DIR := $(BUILD_DIR)/parser
SEMANTIC_BUILD_DIR := $(BUILD_DIR)/semantic
IR_BUILD_DIR := $(BUILD_DIR)/ir
LOWER_IR_BUILD_DIR := $(BUILD_DIR)/lower_ir
VALUE_SSA_BUILD_DIR := $(BUILD_DIR)/value_ssa
MEMORY_SSA_BUILD_DIR := $(BUILD_DIR)/memory_ssa

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
MEMORY_SSA_REGRESSION_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_regression_test
MEMORY_SSA_VERIFIER_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_verifier_test
MEMORY_SSA_ANALYSIS_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_analysis_test
MEMORY_SSA_PASS_BIN := $(MEMORY_SSA_BUILD_DIR)/memory_ssa_pass_test

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
	src/memory_ssa_pass/memory_ssa_pass_load_forward.inc \
	src/memory_ssa_pass/memory_ssa_pass_store_cleanup.inc \
	src/memory_ssa_pass/memory_ssa_pass_memory_value.inc \
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
	src/value_ssa_pass/value_ssa_pass_pipeline.inc

VALUE_SSA_INTERP_SPLIT_INCLUDES := \
	src/value_ssa_interp/value_ssa_interp_state.inc \
	src/value_ssa_interp/value_ssa_interp_exec.inc

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

.PHONY: all dirs lexer parser test test-lexer test-lexer-regression test-parser test-parser-regression test-parser-legacy-link test-semantic-regression test-ir-regression test-ir-verifier test-ir-pass test-lower-ir-regression test-lower-ir-verifier test-value-ssa-regression test-value-ssa-verifier test-value-ssa-analysis test-value-ssa-interp test-value-ssa-oracle test-memory-ssa-regression test-memory-ssa-verifier test-memory-ssa-analysis test-memory-ssa-pass test-fanalyzer test-asan test-strict-warnings clean

all: test

dirs:
	@mkdir -p $(LEXER_BUILD_DIR) $(PARSER_BUILD_DIR) $(SEMANTIC_BUILD_DIR) $(IR_BUILD_DIR) $(LOWER_IR_BUILD_DIR) $(VALUE_SSA_BUILD_DIR) $(MEMORY_SSA_BUILD_DIR)

lexer: $(LEXER_TEST_BIN)

parser: $(PARSER_TEST_BIN)

$(LEXER_TEST_BIN): src/lexer/lexer.c tests/lexer/lexer_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_test.c -o $@

$(LEXER_REGRESSION_BIN): src/lexer/lexer.c tests/lexer/lexer_regression_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_regression_test.c -o $@

$(PARSER_TEST_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/parser/parser_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/parser/parser_test.c -o $@

$(PARSER_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c tests/parser/parser_regression_test.c $(PARSER_SPLIT_INCLUDES) $(PARSER_REGRESSION_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c tests/parser/parser_regression_test.c -o $@

$(PARSER_LEGACY_LINK_BIN): src/lexer/lexer.c src/parser/parser.c tests/parser/parser_legacy_link_test.c $(PARSER_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/parser/parser.c tests/parser/parser_legacy_link_test.c -o $@

$(SEMANTIC_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/semantic/semantic_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(SEMANTIC_REGRESSION_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/semantic/semantic_regression_test.c -o $@

$(IR_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_regression_test.c -o $@

$(IR_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_verifier_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c tests/ir/ir_verifier_test.c -o $@

$(IR_PASS_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/ir_pass/ir_pass.c tests/ir/ir_pass_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(IR_PASS_SPLIT_INCLUDES) $(IR_PASS_TEST_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/ir_pass.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/ir_pass/ir_pass.c tests/ir/ir_pass_test.c -o $@

$(LOWER_IR_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_regression_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/lower_ir.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_regression_test.c -o $@

$(LOWER_IR_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_verifier_test.c $(PARSER_SPLIT_INCLUDES) $(SEMANTIC_SPLIT_INCLUDES) $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/parser.h include/semantic.h include/ir.h include/lower_ir.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c src/ir/ir.c src/lower_ir/lower_ir.c tests/lower_ir/lower_ir_verifier_test.c -o $@

$(VALUE_SSA_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_regression_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_interp.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_regression_test.c -o $@

$(VALUE_SSA_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_verifier_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_verifier_test.c -o $@

$(VALUE_SSA_ANALYSIS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_analysis_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_analysis_test.c -o $@

$(VALUE_SSA_INTERP_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_interp_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_interp.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_interp_test.c -o $@

$(VALUE_SSA_ORACLE_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_oracle_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(VALUE_SSA_INTERP_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/value_ssa_interp.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/value_ssa_interp/value_ssa_interp.c src/lower_ir/lower_ir.c tests/value_ssa/value_ssa_oracle_test.c -o $@

$(MEMORY_SSA_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_regression_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_regression_test.c -o $@

$(MEMORY_SSA_VERIFIER_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_verifier_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_verifier_test.c -o $@

$(MEMORY_SSA_ANALYSIS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_analysis_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/memory_ssa.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/memory_ssa/memory_ssa.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_analysis_test.c -o $@

$(MEMORY_SSA_PASS_BIN): src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_pass_test.c $(IR_SPLIT_INCLUDES) $(LOWER_IR_SPLIT_INCLUDES) $(VALUE_SSA_SPLIT_INCLUDES) $(VALUE_SSA_PASS_SPLIT_INCLUDES) $(MEMORY_SSA_SPLIT_INCLUDES) $(MEMORY_SSA_PASS_SPLIT_INCLUDES) include/lexer.h include/ast.h include/ast_internal.h include/ast_lifecycle_template.h include/ir.h include/lower_ir.h include/value_ssa.h include/value_ssa_pass.h include/memory_ssa.h include/memory_ssa_pass.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/ir/ir.c src/value_ssa/value_ssa.c src/value_ssa_pass/value_ssa_pass.c src/memory_ssa/memory_ssa.c src/memory_ssa_pass/memory_ssa_pass.c src/lower_ir/lower_ir.c tests/memory_ssa/memory_ssa_pass_test.c -o $@

test-lexer: $(LEXER_TEST_BIN)
	@echo "[lexer] running $(LEXER_TEST_INPUT)"
	@tmp="./$(LEXER_TEST_BIN).run.$$$$"; \
	cp "./$(LEXER_TEST_BIN)" "$$tmp" && "$$tmp" $(LEXER_TEST_INPUT); \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lexer-regression: $(LEXER_REGRESSION_BIN)
	@echo "[lexer] running regression tests"
	@tmp="./$(LEXER_REGRESSION_BIN).run.$$$$"; \
	cp "./$(LEXER_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser: $(PARSER_TEST_BIN)
	@echo "[parser] running $(PARSER_TEST_INPUT)"
	@tmp="./$(PARSER_TEST_BIN).run.$$$$"; \
	cp "./$(PARSER_TEST_BIN)" "$$tmp" && "$$tmp" $(PARSER_TEST_INPUT); \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser-regression: $(PARSER_REGRESSION_BIN)
	@echo "[parser] running regression tests"
	@tmp="./$(PARSER_REGRESSION_BIN).run.$$$$"; \
	cp "./$(PARSER_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-parser-legacy-link: $(PARSER_LEGACY_LINK_BIN)
	@echo "[parser] running legacy-link test"
	@tmp="./$(PARSER_LEGACY_LINK_BIN).run.$$$$"; \
	cp "./$(PARSER_LEGACY_LINK_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-semantic-regression: $(SEMANTIC_REGRESSION_BIN)
	@echo "[semantic] running regression tests"
	@tmp="./$(SEMANTIC_REGRESSION_BIN).run.$$$$"; \
	cp "./$(SEMANTIC_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-regression: $(IR_REGRESSION_BIN)
	@echo "[ir] running regression tests"
	@tmp="./$(IR_REGRESSION_BIN).run.$$$$"; \
	cp "./$(IR_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-verifier: $(IR_VERIFIER_BIN)
	@echo "[ir] running verifier tests"
	@tmp="./$(IR_VERIFIER_BIN).run.$$$$"; \
	cp "./$(IR_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-ir-pass: $(IR_PASS_BIN)
	@echo "[ir] running pass tests"
	@tmp="./$(IR_PASS_BIN).run.$$$$"; \
	cp "./$(IR_PASS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lower-ir-regression: $(LOWER_IR_REGRESSION_BIN)
	@echo "[lower-ir] running regression tests"
	@tmp="./$(LOWER_IR_REGRESSION_BIN).run.$$$$"; \
	cp "./$(LOWER_IR_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-lower-ir-verifier: $(LOWER_IR_VERIFIER_BIN)
	@echo "[lower-ir] running verifier tests"
	@tmp="./$(LOWER_IR_VERIFIER_BIN).run.$$$$"; \
	cp "./$(LOWER_IR_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-regression: $(VALUE_SSA_REGRESSION_BIN)
	@echo "[value-ssa] running regression tests"
	@tmp="./$(VALUE_SSA_REGRESSION_BIN).run.$$$$"; \
	cp "./$(VALUE_SSA_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-verifier: $(VALUE_SSA_VERIFIER_BIN)
	@echo "[value-ssa] running verifier tests"
	@tmp="./$(VALUE_SSA_VERIFIER_BIN).run.$$$$"; \
	cp "./$(VALUE_SSA_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-analysis: $(VALUE_SSA_ANALYSIS_BIN)
	@echo "[value-ssa] running analysis tests"
	@tmp="./$(VALUE_SSA_ANALYSIS_BIN).run.$$$$"; \
	cp "./$(VALUE_SSA_ANALYSIS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-interp: $(VALUE_SSA_INTERP_BIN)
	@echo "[value-ssa] running interpreter tests"
	@tmp="./$(VALUE_SSA_INTERP_BIN).run.$$$$"; \
	cp "./$(VALUE_SSA_INTERP_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-value-ssa-oracle: $(VALUE_SSA_ORACLE_BIN)
	@echo "[value-ssa] running oracle tests"
	@tmp="./$(VALUE_SSA_ORACLE_BIN).run.$$$$"; \
	cp "./$(VALUE_SSA_ORACLE_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-regression: $(MEMORY_SSA_REGRESSION_BIN)
	@echo "[memory-ssa] running regression tests"
	@tmp="./$(MEMORY_SSA_REGRESSION_BIN).run.$$$$"; \
	cp "./$(MEMORY_SSA_REGRESSION_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-verifier: $(MEMORY_SSA_VERIFIER_BIN)
	@echo "[memory-ssa] running verifier tests"
	@tmp="./$(MEMORY_SSA_VERIFIER_BIN).run.$$$$"; \
	cp "./$(MEMORY_SSA_VERIFIER_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-analysis: $(MEMORY_SSA_ANALYSIS_BIN)
	@echo "[memory-ssa] running analysis tests"
	@tmp="./$(MEMORY_SSA_ANALYSIS_BIN).run.$$$$"; \
	cp "./$(MEMORY_SSA_ANALYSIS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test-memory-ssa-pass: $(MEMORY_SSA_PASS_BIN)
	@echo "[memory-ssa] running pass tests"
	@tmp="./$(MEMORY_SSA_PASS_BIN).run.$$$$"; \
	cp "./$(MEMORY_SSA_PASS_BIN)" "$$tmp" && "$$tmp"; \
	status=$$?; \
	rm -f "$$tmp"; \
	exit $$status

test:
	@$(MAKE) --no-print-directory test-lexer
	@$(MAKE) --no-print-directory test-lexer-regression
	@$(MAKE) --no-print-directory test-parser
	@$(MAKE) --no-print-directory test-parser-regression
	@$(MAKE) --no-print-directory test-parser-legacy-link
	@$(MAKE) --no-print-directory test-semantic-regression
	@$(MAKE) --no-print-directory test-ir-regression
	@$(MAKE) --no-print-directory test-ir-verifier
	@$(MAKE) --no-print-directory test-ir-pass
	@$(MAKE) --no-print-directory test-lower-ir-regression
	@$(MAKE) --no-print-directory test-lower-ir-verifier
	@$(MAKE) --no-print-directory test-value-ssa-regression
	@$(MAKE) --no-print-directory test-value-ssa-verifier
	@$(MAKE) --no-print-directory test-value-ssa-analysis
	@$(MAKE) --no-print-directory test-value-ssa-interp
	@$(MAKE) --no-print-directory test-value-ssa-oracle
	@$(MAKE) --no-print-directory test-memory-ssa-regression
	@$(MAKE) --no-print-directory test-memory-ssa-verifier
	@$(MAKE) --no-print-directory test-memory-ssa-analysis
	@$(MAKE) --no-print-directory test-memory-ssa-pass

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
