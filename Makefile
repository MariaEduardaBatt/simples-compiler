CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -Isrc -Ithird_party/unity/src
BUILD_DIR := build
UNITY_SRC := third_party/unity/src/unity.c
TOKEN_TEST_BIN := $(BUILD_DIR)/test_token
LEXER_TEST_BIN := $(BUILD_DIR)/test_lexer
PARSER_TEST_BIN := $(BUILD_DIR)/test_parser
SEMANTIC_TEST_BIN := $(BUILD_DIR)/test_semantic
CODEGEN_TEST_BIN := $(BUILD_DIR)/test_codegen

TOKEN_TEST_SRCS := tests/test_token.c src/token.c src/error.c $(UNITY_SRC)
LEXER_TEST_SRCS := tests/test_lexer.c src/token.c src/error.c src/lexer.c $(UNITY_SRC)
PARSER_TEST_SRCS := tests/test_parser.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c $(UNITY_SRC)
SEMANTIC_TEST_SRCS := tests/test_semantic.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c $(UNITY_SRC)
CODEGEN_TEST_SRCS := tests/test_codegen.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/codegen.c $(UNITY_SRC)
CODEGEN_TEST_HEADERS := $(wildcard $(CODEGEN_TEST_SRCS:%.c=%.h) $(dir $(UNITY_SRC))unity.h)
CODEGEN_TEST_FILES := $(CODEGEN_TEST_SRCS) $(CODEGEN_TEST_HEADERS)
SIMPLESC_SRCS := src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/codegen.c src/main.c

define have_all_files
$(if $(strip $(foreach file,$1,$(if $(wildcard $(file)),,$(file)))),,yes)
endef

.PHONY: all test-token test-lexer test-parser test-semantic test-codegen test e2e clean

all: $(TOKEN_TEST_BIN) $(BUILD_DIR)/simplesc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TOKEN_TEST_BIN): $(TOKEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TOKEN_TEST_SRCS) -o $@

test-token: $(TOKEN_TEST_BIN)
	$(TOKEN_TEST_BIN)

$(LEXER_TEST_BIN): $(LEXER_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LEXER_TEST_SRCS) -o $@

test-lexer: $(LEXER_TEST_BIN)
	$(LEXER_TEST_BIN)

$(PARSER_TEST_BIN): $(PARSER_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PARSER_TEST_SRCS) -o $@

test-parser: $(PARSER_TEST_BIN)
	$(PARSER_TEST_BIN)

$(SEMANTIC_TEST_BIN): $(SEMANTIC_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SEMANTIC_TEST_SRCS) -o $@

test-semantic: $(SEMANTIC_TEST_BIN)
	$(SEMANTIC_TEST_BIN)

ifneq ($(wildcard tests/test_codegen.c),)
$(CODEGEN_TEST_BIN): $(CODEGEN_TEST_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CODEGEN_TEST_SRCS) -o $@

test-codegen: $(CODEGEN_TEST_BIN)
	$(CODEGEN_TEST_BIN)
else
test-codegen:
	@echo "Skipping test-codegen: code generation tests not implemented yet"
endif

$(BUILD_DIR)/simplesc: $(SIMPLESC_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SIMPLESC_SRCS) -o $(BUILD_DIR)/simplesc


test: test-token $(if $(call have_all_files,$(LEXER_TEST_SRCS)),test-lexer) $(if $(call have_all_files,$(PARSER_TEST_SRCS)),test-parser) $(if $(call have_all_files,$(SEMANTIC_TEST_SRCS)),test-semantic) $(if $(wildcard tests/test_codegen.c),test-codegen)

e2e: $(BUILD_DIR)/simplesc tests/test_e2e.sh
	sh tests/test_e2e.sh

clean:
	rm -rf $(BUILD_DIR)
