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
	src/lower_ir/lower_ir_verify.inc \
	src/lower_ir/lower_ir_dump.inc

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

.PHONY: all dirs lexer parser test test-lexer test-lexer-regression test-parser test-parser-regression test-parser-legacy-link test-semantic-regression test-ir-regression test-ir-verifier test-ir-pass test-lower-ir-regression test-lower-ir-verifier test-fanalyzer test-asan test-strict-warnings clean

all: test

dirs:
	@mkdir -p $(LEXER_BUILD_DIR) $(PARSER_BUILD_DIR) $(SEMANTIC_BUILD_DIR) $(IR_BUILD_DIR) $(LOWER_IR_BUILD_DIR)

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
