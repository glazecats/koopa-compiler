CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Iinclude

BUILD_DIR := build
LEXER_BUILD_DIR := $(BUILD_DIR)/lexer
PARSER_BUILD_DIR := $(BUILD_DIR)/parser
SEMANTIC_BUILD_DIR := $(BUILD_DIR)/semantic

LEXER_TEST_BIN := $(LEXER_BUILD_DIR)/lexer_test
PARSER_TEST_BIN := $(PARSER_BUILD_DIR)/parser_test
LEXER_REGRESSION_BIN := $(LEXER_BUILD_DIR)/lexer_regression_test
PARSER_REGRESSION_BIN := $(PARSER_BUILD_DIR)/parser_regression_test
PARSER_LEGACY_LINK_BIN := $(PARSER_BUILD_DIR)/parser_legacy_link_test
SEMANTIC_REGRESSION_BIN := $(SEMANTIC_BUILD_DIR)/semantic_regression_test

LEXER_TEST_INPUT := tests/lexer/test.c
PARSER_TEST_INPUT := tests/parser/test.c

.PHONY: all dirs lexer parser test test-lexer test-lexer-regression test-parser test-parser-regression test-parser-legacy-link test-semantic-regression clean

all: test

dirs:
	@mkdir -p $(LEXER_BUILD_DIR) $(PARSER_BUILD_DIR) $(SEMANTIC_BUILD_DIR)

lexer: $(LEXER_TEST_BIN)

parser: $(PARSER_TEST_BIN)

$(LEXER_TEST_BIN): src/lexer/lexer.c tests/lexer/lexer_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_test.c -o $@

$(LEXER_REGRESSION_BIN): src/lexer/lexer.c tests/lexer/lexer_regression_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_regression_test.c -o $@

$(PARSER_TEST_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/parser/parser_test.c include/lexer.h include/ast.h include/ast_internal.h include/parser.h include/semantic.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/parser/parser_test.c -o $@

$(PARSER_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c tests/parser/parser_regression_test.c include/lexer.h include/ast.h include/ast_internal.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c tests/parser/parser_regression_test.c -o $@

$(PARSER_LEGACY_LINK_BIN): src/lexer/lexer.c src/parser/parser.c tests/parser/parser_legacy_link_test.c include/lexer.h include/ast.h include/ast_internal.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/parser/parser.c tests/parser/parser_legacy_link_test.c -o $@

$(SEMANTIC_REGRESSION_BIN): src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/semantic/semantic_regression_test.c include/lexer.h include/ast.h include/ast_internal.h include/parser.h include/semantic.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/ast/ast.c src/parser/parser.c src/semantic/semantic.c tests/semantic/semantic_regression_test.c -o $@

test-lexer: $(LEXER_TEST_BIN)
	@echo "[lexer] running $(LEXER_TEST_INPUT)"
	@./$(LEXER_TEST_BIN) $(LEXER_TEST_INPUT)

test-lexer-regression: $(LEXER_REGRESSION_BIN)
	@echo "[lexer] running regression tests"
	@./$(LEXER_REGRESSION_BIN)

test-parser: $(PARSER_TEST_BIN)
	@echo "[parser] running $(PARSER_TEST_INPUT)"
	@./$(PARSER_TEST_BIN) $(PARSER_TEST_INPUT)

test-parser-regression: $(PARSER_REGRESSION_BIN)
	@echo "[parser] running regression tests"
	@./$(PARSER_REGRESSION_BIN)

test-parser-legacy-link: $(PARSER_LEGACY_LINK_BIN)
	@echo "[parser] running legacy-link test"
	@./$(PARSER_LEGACY_LINK_BIN)

test-semantic-regression: $(SEMANTIC_REGRESSION_BIN)
	@echo "[semantic] running regression tests"
	@./$(SEMANTIC_REGRESSION_BIN)

test:
	@$(MAKE) --no-print-directory test-lexer
	@$(MAKE) --no-print-directory test-lexer-regression
	@$(MAKE) --no-print-directory test-parser
	@$(MAKE) --no-print-directory test-parser-regression
	@$(MAKE) --no-print-directory test-parser-legacy-link
	@$(MAKE) --no-print-directory test-semantic-regression

clean:
	rm -rf $(BUILD_DIR)
