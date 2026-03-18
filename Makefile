CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Iinclude

BUILD_DIR := build
LEXER_BUILD_DIR := $(BUILD_DIR)/lexer
PARSER_BUILD_DIR := $(BUILD_DIR)/parser

LEXER_TEST_BIN := $(LEXER_BUILD_DIR)/lexer_test
PARSER_TEST_BIN := $(PARSER_BUILD_DIR)/parser_test
LEXER_REGRESSION_BIN := $(LEXER_BUILD_DIR)/lexer_regression_test
PARSER_REGRESSION_BIN := $(PARSER_BUILD_DIR)/parser_regression_test

LEXER_TEST_INPUT := tests/lexer/test.cpp
PARSER_TEST_INPUT := tests/parser/test.cpp

.PHONY: all dirs lexer parser test test-lexer test-lexer-regression test-parser test-parser-regression clean

all: test

dirs:
	@mkdir -p $(LEXER_BUILD_DIR) $(PARSER_BUILD_DIR)

lexer: $(LEXER_TEST_BIN)

parser: $(PARSER_TEST_BIN)

$(LEXER_TEST_BIN): src/lexer/lexer.c tests/lexer/lexer_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_test.c -o $@

$(LEXER_REGRESSION_BIN): src/lexer/lexer.c tests/lexer/lexer_regression_test.c include/lexer.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c tests/lexer/lexer_regression_test.c -o $@

$(PARSER_TEST_BIN): src/lexer/lexer.c src/parser/parser.c tests/parser/parser_test.c include/lexer.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/parser/parser.c tests/parser/parser_test.c -o $@

$(PARSER_REGRESSION_BIN): src/lexer/lexer.c src/parser/parser.c tests/parser/parser_regression_test.c include/lexer.h include/parser.h | dirs
	$(CC) $(CFLAGS) src/lexer/lexer.c src/parser/parser.c tests/parser/parser_regression_test.c -o $@

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

test:
	@$(MAKE) --no-print-directory test-lexer
	@$(MAKE) --no-print-directory test-lexer-regression
	@$(MAKE) --no-print-directory test-parser
	@$(MAKE) --no-print-directory test-parser-regression

clean:
	rm -rf $(BUILD_DIR)
