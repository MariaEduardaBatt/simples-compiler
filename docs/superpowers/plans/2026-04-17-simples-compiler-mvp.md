# SIMPLES Compiler MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an end-to-end C99 compiler for a canonical integer-only subset of SIMPLES that emits runnable NASM x86 32-bit assembly for Linux.

**Architecture:** Keep the pipeline strictly linear: lexer -> parser/AST -> semantic analysis -> NASM code generation -> CLI entry point. The first slice only supports `inteiro`, declarations, assignments, arithmetic expressions, and `escreva`/`escreval`, with fail-fast diagnostics and focused stage-by-stage tests.

**Tech Stack:** C99, GCC, Make, Unity Test Framework, NASM x86 32-bit, Linux `int 0x80`

---

## File Structure

Create this project structure as the baseline:

- `Makefile` — build, test, and end-to-end targets
- `third_party/unity/src/unity.h` — vendored Unity header
- `third_party/unity/src/unity.c` — vendored Unity implementation
- `src/error.h` / `src/error.c` — shared diagnostic type and formatting helpers
- `src/token.h` / `src/token.c` — token enum, token list, token helpers
- `src/lexer.h` / `src/lexer.c` — lexical scanner for canonical MVP tokens
- `src/ast.h` / `src/ast.c` — AST types and memory management
- `src/parser.h` / `src/parser.c` — recursive-descent parser for the MVP grammar
- `src/semantic.h` / `src/semantic.c` — global symbol table and semantic validation
- `src/codegen.h` / `src/codegen.c` — NASM emitter for assignments, arithmetic, and output
- `src/main.c` — CLI entry point that compiles one source file into one `.asm` file
- `tests/test_token.c` — token and token-list coverage
- `tests/test_lexer.c` — lexical coverage including source positions
- `tests/test_parser.c` — AST shape and expression-precedence coverage
- `tests/test_semantic.c` — duplicate declaration and undeclared-variable failures
- `tests/test_codegen.c` — assembly-fragment coverage for core constructs
- `tests/test_e2e.sh` — shell-level end-to-end verification
- `examples/assign.simples` — declaration + assignment fixture
- `examples/print.simples` — arithmetic + output fixture

Use these stable interfaces across tasks:

```c
typedef enum {
    COMPILER_PHASE_LEXER,
    COMPILER_PHASE_PARSER,
    COMPILER_PHASE_SEMANTIC
} CompilerPhase;

typedef struct {
    CompilerPhase phase;
    int line;
    int column;
    char message[256];
} CompilerError;

bool lexer_scan(const char *source, TokenList *out_tokens, CompilerError *error);
bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error);
bool analyze_program(const ASTProgram *program, SymbolTable *out_symbols, CompilerError *error);
char *codegen_generate_program(const ASTProgram *program, const SymbolTable *symbols);
int compile_file(const char *input_path, const char *output_path);
```

## Canonical MVP Grammar

All implementation tasks must follow this grammar exactly:

```ebnf
<programa>    ::= "programa" ID <declaracoes> "inicio" <comandos> "fim"
<declaracoes> ::= { "inteiro" ID { "," ID } ";" }
<comandos>    ::= { <atribuicao> ";" | <escrita> ";" }
<atribuicao>  ::= ID "<-" <expressao>
<escrita>     ::= ("escreva" | "escreval") <expressao>
<expressao>   ::= <termo> { ("+" | "-") <termo> }
<termo>       ::= <fator> { ("*" | "div") <fator> }
<fator>       ::= ID | NUM_INT | "(" <expressao> ")"
```

### Task 1: Bootstrap the build, Unity, and token core

**Files:**
- Create: `Makefile`
- Create: `third_party/unity/src/unity.h`
- Create: `third_party/unity/src/unity.c`
- Create: `src/error.h`
- Create: `src/error.c`
- Create: `src/token.h`
- Create: `src/token.c`
- Test: `tests/test_token.c`

- [ ] **Step 1: Vendor Unity into the repository**

```bash
mkdir -p third_party/unity/src
curl -fsSL https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.h -o third_party/unity/src/unity.h
curl -fsSL https://raw.githubusercontent.com/ThrowTheSwitch/Unity/master/src/unity.c -o third_party/unity/src/unity.c
```

- [ ] **Step 2: Add the initial `Makefile` with per-test targets**

```makefile
CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -Isrc -Ithird_party/unity/src
BUILD_DIR := build
UNITY_SRC := third_party/unity/src/unity.c

all: $(BUILD_DIR)/simplesc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test-token: $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_token.c src/token.c src/error.c $(UNITY_SRC) -o $(BUILD_DIR)/test_token
	$(BUILD_DIR)/test_token

test-lexer: $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_lexer.c src/token.c src/error.c src/lexer.c $(UNITY_SRC) -o $(BUILD_DIR)/test_lexer
	$(BUILD_DIR)/test_lexer

test-parser: $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_parser.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c $(UNITY_SRC) -o $(BUILD_DIR)/test_parser
	$(BUILD_DIR)/test_parser

test-semantic: $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_semantic.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c $(UNITY_SRC) -o $(BUILD_DIR)/test_semantic
	$(BUILD_DIR)/test_semantic

test-codegen: $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_codegen.c src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/codegen.c $(UNITY_SRC) -o $(BUILD_DIR)/test_codegen
	$(BUILD_DIR)/test_codegen

$(BUILD_DIR)/simplesc: $(BUILD_DIR)
	$(CC) $(CFLAGS) src/token.c src/error.c src/lexer.c src/ast.c src/parser.c src/semantic.c src/codegen.c src/main.c -o $(BUILD_DIR)/simplesc

test: test-token test-lexer test-parser test-semantic test-codegen

e2e: all
	sh tests/test_e2e.sh

clean:
	rm -rf $(BUILD_DIR)
```

- [ ] **Step 3: Write the failing token test**

```c
#include "unity.h"
#include "token.h"

void setUp(void) {}
void tearDown(void) {}

void test_token_type_name_returns_keyword_name(void) {
    TEST_ASSERT_EQUAL_STRING("TOK_PROGRAMA", token_type_name(TOK_PROGRAMA));
}

void test_token_list_grows_and_preserves_lexeme(void) {
    TokenList list;
    token_list_init(&list);
    token_list_push(&list, (Token){ .type = TOK_ID, .lexeme = "demo", .line = 1, .column = 10 });

    TEST_ASSERT_EQUAL_size_t(1, list.count);
    TEST_ASSERT_EQUAL(TOK_ID, list.items[0].type);
    TEST_ASSERT_EQUAL_STRING("demo", list.items[0].lexeme);

    token_list_free(&list);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_token_type_name_returns_keyword_name);
    RUN_TEST(test_token_list_grows_and_preserves_lexeme);
    return UNITY_END();
}
```

- [ ] **Step 4: Run the token test and verify it fails**

Run: `make test-token`  
Expected: compile or link failure because `token_type_name`, `TokenList`, and token helpers do not exist yet.

- [ ] **Step 5: Implement `src/error.h`, `src/error.c`, `src/token.h`, and `src/token.c`**

```c
/* src/error.h */
#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

typedef enum {
    COMPILER_PHASE_LEXER,
    COMPILER_PHASE_PARSER,
    COMPILER_PHASE_SEMANTIC
} CompilerPhase;

typedef struct {
    CompilerPhase phase;
    int line;
    int column;
    char message[256];
} CompilerError;

void compiler_error_set(CompilerError *error, CompilerPhase phase, int line, int column, const char *message);

#endif
```

```c
/* src/error.c */
#include "error.h"
#include <stdio.h>
#include <string.h>

void compiler_error_set(CompilerError *error, CompilerPhase phase, int line, int column, const char *message) {
    if (error == NULL) return;
    error->phase = phase;
    error->line = line;
    error->column = column;
    snprintf(error->message, sizeof(error->message), "%s", message);
}
```

```c
/* src/token.h */
#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

typedef enum {
    TOK_PROGRAMA,
    TOK_INICIO,
    TOK_FIM,
    TOK_INTEIRO,
    TOK_ESCREVA,
    TOK_ESCREVAL,
    TOK_DIV,
    TOK_ID,
    TOK_NUM_INT,
    TOK_ATRIB,
    TOK_MAIS,
    TOK_MENOS,
    TOK_MULT,
    TOK_ABRE_PAR,
    TOK_FECHA_PAR,
    TOK_VIRGULA,
    TOK_PONTO_VIRGULA,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

const char *token_type_name(TokenType type);
void token_list_init(TokenList *list);
void token_list_push(TokenList *list, Token token);
void token_list_free(TokenList *list);

#endif
```

```c
/* src/token.c */
#include "token.h"
#include <stdlib.h>
#include <string.h>

const char *token_type_name(TokenType type) {
    static const char *names[] = {
        "TOK_PROGRAMA", "TOK_INICIO", "TOK_FIM", "TOK_INTEIRO",
        "TOK_ESCREVA", "TOK_ESCREVAL", "TOK_DIV", "TOK_ID",
        "TOK_NUM_INT", "TOK_ATRIB", "TOK_MAIS", "TOK_MENOS",
        "TOK_MULT", "TOK_ABRE_PAR", "TOK_FECHA_PAR", "TOK_VIRGULA",
        "TOK_PONTO_VIRGULA", "TOK_EOF"
    };
    return names[type];
}

void token_list_init(TokenList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void token_list_push(TokenList *list, Token token) {
    if (list->count == list->capacity) {
        size_t next_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, next_capacity * sizeof(Token));
        list->capacity = next_capacity;
    }
    token.lexeme = strdup(token.lexeme);
    list->items[list->count++] = token;
}

void token_list_free(TokenList *list) {
    size_t i;
    for (i = 0; i < list->count; ++i) {
        free(list->items[i].lexeme);
    }
    free(list->items);
    token_list_init(list);
}
```

- [ ] **Step 6: Run the token test and verify it passes**

Run: `make test-token`  
Expected: `OK (2 tests)`

- [ ] **Step 7: Commit the bootstrap and token foundation**

```bash
git add Makefile third_party/unity/src/unity.h third_party/unity/src/unity.c src/error.h src/error.c src/token.h src/token.c tests/test_token.c
git commit -m "build: add test harness and token core"
```

### Task 2: Build the lexer with line/column tracking

**Files:**
- Create: `src/lexer.h`
- Create: `src/lexer.c`
- Test: `tests/test_lexer.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the failing lexer tests**

```c
#include "unity.h"
#include "error.h"
#include "lexer.h"

void setUp(void) {}
void tearDown(void) {}

void test_lexer_scans_program_header_and_assignment(void) {
    TokenList tokens;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inicio x <- 42; fim", &tokens, &error));
    TEST_ASSERT_EQUAL(TOK_PROGRAMA, tokens.items[0].type);
    TEST_ASSERT_EQUAL(TOK_ID, tokens.items[1].type);
    TEST_ASSERT_EQUAL(TOK_INICIO, tokens.items[2].type);
    TEST_ASSERT_EQUAL(TOK_ID, tokens.items[3].type);
    TEST_ASSERT_EQUAL(TOK_ATRIB, tokens.items[4].type);
    TEST_ASSERT_EQUAL(TOK_NUM_INT, tokens.items[5].type);
    TEST_ASSERT_EQUAL(TOK_PONTO_VIRGULA, tokens.items[6].type);
    TEST_ASSERT_EQUAL(TOK_FIM, tokens.items[7].type);
    TEST_ASSERT_EQUAL(TOK_EOF, tokens.items[8].type);

    token_list_free(&tokens);
}

void test_lexer_tracks_line_and_column(void) {
    TokenList tokens;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo\ninicio\nescreva 7;\nfim", &tokens, &error));
    TEST_ASSERT_EQUAL(3, tokens.items[3].line);
    TEST_ASSERT_EQUAL(1, tokens.items[3].column);

    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lexer_scans_program_header_and_assignment);
    RUN_TEST(test_lexer_tracks_line_and_column);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the lexer test and verify it fails**

Run: `make test-lexer`  
Expected: compile failure because `lexer.h` and `lexer_scan` do not exist.

- [ ] **Step 3: Implement the lexer interface and scanner**

```c
/* src/lexer.h */
#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "error.h"
#include "token.h"

bool lexer_scan(const char *source, TokenList *out_tokens, CompilerError *error);

#endif
```

```c
/* src/lexer.c */
#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *slice_dup(const char *start, size_t length) {
    char *copy = malloc(length + 1);
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static TokenType keyword_type(const char *lexeme) {
    if (strcmp(lexeme, "programa") == 0) return TOK_PROGRAMA;
    if (strcmp(lexeme, "inicio") == 0) return TOK_INICIO;
    if (strcmp(lexeme, "fim") == 0) return TOK_FIM;
    if (strcmp(lexeme, "inteiro") == 0) return TOK_INTEIRO;
    if (strcmp(lexeme, "escreva") == 0) return TOK_ESCREVA;
    if (strcmp(lexeme, "escreval") == 0) return TOK_ESCREVAL;
    if (strcmp(lexeme, "div") == 0) return TOK_DIV;
    return TOK_ID;
}

bool lexer_scan(const char *source, TokenList *out_tokens, CompilerError *error) {
    int line = 1;
    int column = 1;
    token_list_init(out_tokens);

    while (*source != '\0') {
        if (*source == ' ' || *source == '\t' || *source == '\r') {
            ++source;
            ++column;
            continue;
        }
        if (*source == '\n') {
            ++source;
            ++line;
            column = 1;
            continue;
        }
        if (isalpha((unsigned char)*source) || *source == '_') {
            const char *start = source;
            int start_column = column;
            while (isalnum((unsigned char)*source) || *source == '_') {
                ++source;
                ++column;
            }
            char *lexeme = slice_dup(start, (size_t)(source - start));
            token_list_push(out_tokens, (Token){ keyword_type(lexeme), lexeme, line, start_column });
            continue;
        }
        if (isdigit((unsigned char)*source)) {
            const char *start = source;
            int start_column = column;
            while (isdigit((unsigned char)*source)) {
                ++source;
                ++column;
            }
            token_list_push(out_tokens, (Token){ TOK_NUM_INT, slice_dup(start, (size_t)(source - start)), line, start_column });
            continue;
        }
        if (*source == '<' && source[1] == '-') {
            token_list_push(out_tokens, (Token){ TOK_ATRIB, slice_dup(source, 2), line, column });
            source += 2;
            column += 2;
            continue;
        }
        if (*source == '+') { token_list_push(out_tokens, (Token){ TOK_MAIS, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == '-') { token_list_push(out_tokens, (Token){ TOK_MENOS, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == '*') { token_list_push(out_tokens, (Token){ TOK_MULT, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == '(') { token_list_push(out_tokens, (Token){ TOK_ABRE_PAR, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == ')') { token_list_push(out_tokens, (Token){ TOK_FECHA_PAR, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == ',') { token_list_push(out_tokens, (Token){ TOK_VIRGULA, slice_dup(source, 1), line, column++ }); ++source; continue; }
        if (*source == ';') { token_list_push(out_tokens, (Token){ TOK_PONTO_VIRGULA, slice_dup(source, 1), line, column++ }); ++source; continue; }

        compiler_error_set(error, COMPILER_PHASE_LEXER, line, column, "invalid character");
        token_list_free(out_tokens);
        return false;
    }

    token_list_push(out_tokens, (Token){ TOK_EOF, slice_dup("", 0), line, column });
    return true;
}
```

- [ ] **Step 4: Add one explicit invalid-character test before moving on**

```c
void test_lexer_rejects_invalid_character(void) {
    TokenList tokens;
    CompilerError error;

    TEST_ASSERT_FALSE(lexer_scan("programa demo inicio @ fim", &tokens, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_LEXER, error.phase);
    TEST_ASSERT_EQUAL(1, error.line);
}
```

- [ ] **Step 5: Run the lexer test suite and verify it passes**

Run: `make test-lexer`  
Expected: `OK (3 tests)`

- [ ] **Step 6: Commit the lexer**

```bash
git add Makefile src/lexer.h src/lexer.c tests/test_lexer.c
git commit -m "feat: add lexer for canonical MVP syntax"
```

### Task 3: Implement the AST and recursive-descent parser

**Files:**
- Create: `src/ast.h`
- Create: `src/ast.c`
- Create: `src/parser.h`
- Create: `src/parser.c`
- Test: `tests/test_parser.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the failing parser tests for program shape and precedence**

```c
#include "unity.h"
#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"

void setUp(void) {}
void tearDown(void) {}

void test_parser_builds_assignment_ast(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 1 + 2 * 3; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));

    TEST_ASSERT_EQUAL_size_t(1, program->declaration_count);
    TEST_ASSERT_EQUAL_size_t(1, program->command_count);
    TEST_ASSERT_EQUAL(AST_COMMAND_ASSIGNMENT, program->commands[0].type);
    TEST_ASSERT_EQUAL_STRING("x", program->commands[0].assignment.name);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, program->commands[0].assignment.expression->type);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_honors_multiplication_precedence(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 1 + 2 * 3; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));

    TEST_ASSERT_EQUAL(AST_BINARY_ADD, program->commands[0].assignment.expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, program->commands[0].assignment.expression->binary.right->type);
    TEST_ASSERT_EQUAL(AST_BINARY_MUL, program->commands[0].assignment.expression->binary.right->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_builds_assignment_ast);
    RUN_TEST(test_parser_honors_multiplication_precedence);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the parser test and verify it fails**

Run: `make test-parser`  
Expected: compile failure because AST and parser types are missing.

- [ ] **Step 3: Implement the AST types and parser surface**

```c
/* src/ast.h */
#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum { AST_EXPR_INT, AST_EXPR_IDENTIFIER, AST_EXPR_BINARY } ASTExpressionType;
typedef enum { AST_BINARY_ADD, AST_BINARY_SUB, AST_BINARY_MUL, AST_BINARY_DIV } ASTBinaryOp;
typedef enum { AST_COMMAND_ASSIGNMENT, AST_COMMAND_WRITE, AST_COMMAND_WRITELN } ASTCommandType;

typedef struct ASTExpression ASTExpression;

struct ASTExpression {
    ASTExpressionType type;
    union {
        int int_value;
        char *identifier;
        struct {
            ASTBinaryOp op;
            ASTExpression *left;
            ASTExpression *right;
        } binary;
    };
};

typedef struct {
    char *name;
} ASTDeclaration;

typedef struct {
    ASTCommandType type;
    union {
        struct {
            char *name;
            ASTExpression *expression;
        } assignment;
        ASTExpression *write_expression;
    };
} ASTCommand;

typedef struct {
    char *name;
    ASTDeclaration *declarations;
    size_t declaration_count;
    ASTCommand *commands;
    size_t command_count;
} ASTProgram;

void ast_program_free(ASTProgram *program);

#endif
```

```c
/* src/parser.h */
#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "ast.h"
#include "error.h"
#include "token.h"

bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error);

#endif
```

```c
/* src/parser.c */
#include "parser.h"
#include <stdlib.h>

typedef struct {
    const TokenList *tokens;
    size_t current;
} Parser;

static const Token *peek(Parser *parser) { return &parser->tokens->items[parser->current]; }
static const Token *advance(Parser *parser) { return &parser->tokens->items[parser->current++]; }
static bool match(Parser *parser, TokenType type) {
    if (peek(parser)->type != type) return false;
    advance(parser);
    return true;
}

static bool expect(Parser *parser, TokenType type, CompilerError *error) {
    if (match(parser, type)) return true;
    compiler_error_set(error, COMPILER_PHASE_PARSER, peek(parser)->line, peek(parser)->column, token_type_name(type));
    return false;
}

static bool parse_command(Parser *parser, ASTCommand *out_command, CompilerError *error);
static ASTExpression *parse_factor(Parser *parser, CompilerError *error);
static ASTExpression *parse_term(Parser *parser, CompilerError *error);
static ASTExpression *parse_expression(Parser *parser, CompilerError *error);

bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error) {
    Parser parser = { tokens, 0 };
    ASTProgram *program = calloc(1, sizeof(*program));

    if (!expect(&parser, TOK_PROGRAMA, error)) return false;
    program->name = strdup(advance(&parser)->lexeme);
    while (peek(&parser)->type == TOK_INTEIRO) {
        advance(&parser);
        do {
            program->declarations = realloc(program->declarations, sizeof(*program->declarations) * (program->declaration_count + 1));
            program->declarations[program->declaration_count++].name = strdup(advance(&parser)->lexeme);
        } while (match(&parser, TOK_VIRGULA));
        if (!expect(&parser, TOK_PONTO_VIRGULA, error)) return false;
    }
    if (!expect(&parser, TOK_INICIO, error)) return false;
    while (peek(&parser)->type != TOK_FIM) {
        program->commands = realloc(program->commands, sizeof(*program->commands) * (program->command_count + 1));
        if (!parse_command(&parser, &program->commands[program->command_count], error)) return false;
        ++program->command_count;
        if (!expect(&parser, TOK_PONTO_VIRGULA, error)) return false;
    }
    if (!expect(&parser, TOK_FIM, error)) return false;
    if (!expect(&parser, TOK_EOF, error)) return false;
    *out_program = program;
    return true;
}
```

- [ ] **Step 4: Add one explicit parser failure test for a missing semicolon**

```c
void test_parser_reports_missing_semicolon(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 1 fim", &tokens, &error));
    TEST_ASSERT_FALSE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_PARSER, error.phase);

    token_list_free(&tokens);
}
```

- [ ] **Step 5: Run the parser test suite and verify it passes**

Run: `make test-parser`  
Expected: `OK (3 tests)`

- [ ] **Step 6: Commit the parser and AST**

```bash
git add Makefile src/ast.h src/ast.c src/parser.h src/parser.c tests/test_parser.c
git commit -m "feat: add parser and AST for MVP programs"
```

### Task 4: Add semantic validation with a flat symbol table

**Files:**
- Create: `src/semantic.h`
- Create: `src/semantic.c`
- Test: `tests/test_semantic.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the failing semantic tests**

```c
#include "unity.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

void setUp(void) {}
void tearDown(void) {}

void test_semantic_rejects_duplicate_declaration(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x, x; inicio escreva 1; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_variable_usage(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio escreva y; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);

    ast_program_free(program);
    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_rejects_duplicate_declaration);
    RUN_TEST(test_semantic_rejects_undeclared_variable_usage);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the semantic test and verify it fails**

Run: `make test-semantic`  
Expected: compile failure because `semantic.h`, `SymbolTable`, and `analyze_program` do not exist.

- [ ] **Step 3: Implement the semantic analyzer**

```c
/* src/semantic.h */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>
#include "ast.h"
#include "error.h"

typedef struct {
    char **names;
    size_t count;
} SymbolTable;

bool analyze_program(const ASTProgram *program, SymbolTable *out_symbols, CompilerError *error);
void symbol_table_free(SymbolTable *symbols);

#endif
```

```c
/* src/semantic.c */
#include "semantic.h"
#include <stdlib.h>
#include <string.h>

static bool symbol_table_contains(const SymbolTable *symbols, const char *name) {
    size_t i;
    for (i = 0; i < symbols->count; ++i) {
        if (strcmp(symbols->names[i], name) == 0) return true;
    }
    return false;
}

static bool expression_identifiers_exist(const ASTExpression *expression, const SymbolTable *symbols) {
    if (expression->type == AST_EXPR_INT) return true;
    if (expression->type == AST_EXPR_IDENTIFIER) return symbol_table_contains(symbols, expression->identifier);
    return expression_identifiers_exist(expression->binary.left, symbols)
        && expression_identifiers_exist(expression->binary.right, symbols);
}

bool analyze_program(const ASTProgram *program, SymbolTable *out_symbols, CompilerError *error) {
    size_t i;
    out_symbols->names = calloc(program->declaration_count, sizeof(char *));
    out_symbols->count = 0;

    for (i = 0; i < program->declaration_count; ++i) {
        const char *name = program->declarations[i].name;
        if (symbol_table_contains(out_symbols, name)) {
            compiler_error_set(error, COMPILER_PHASE_SEMANTIC, 1, 1, "duplicate declaration");
            return false;
        }
        out_symbols->names[out_symbols->count++] = strdup(name);
    }

    for (i = 0; i < program->command_count; ++i) {
        const ASTCommand *command = &program->commands[i];
        if (command->type == AST_COMMAND_ASSIGNMENT && !symbol_table_contains(out_symbols, command->assignment.name)) {
            compiler_error_set(error, COMPILER_PHASE_SEMANTIC, 1, 1, "undeclared assignment target");
            return false;
        }
        if (command->type == AST_COMMAND_ASSIGNMENT &&
            !expression_identifiers_exist(command->assignment.expression, out_symbols)) {
            compiler_error_set(error, COMPILER_PHASE_SEMANTIC, 1, 1, "undeclared identifier in expression");
            return false;
        }
        if ((command->type == AST_COMMAND_WRITE || command->type == AST_COMMAND_WRITELN) &&
            !expression_identifiers_exist(command->write_expression, out_symbols)) {
            compiler_error_set(error, COMPILER_PHASE_SEMANTIC, 1, 1, "undeclared identifier in output");
            return false;
        }
    }
    return true;
}
```

- [ ] **Step 4: Add one success-path semantic test before moving on**

```c
void test_semantic_accepts_declared_variables(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x, y; inicio x <- 1; escreva x + y; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_size_t(2, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}
```

- [ ] **Step 5: Run the semantic test suite and verify it passes**

Run: `make test-semantic`  
Expected: `OK (3 tests)`

- [ ] **Step 6: Commit semantic analysis**

```bash
git add Makefile src/semantic.h src/semantic.c tests/test_semantic.c
git commit -m "feat: add semantic checks for MVP subset"
```

### Task 5: Generate NASM for assignments, arithmetic, and output

**Files:**
- Create: `src/codegen.h`
- Create: `src/codegen.c`
- Test: `tests/test_codegen.c`
- Modify: `Makefile`

- [ ] **Step 1: Write the failing codegen tests**

```c
#include "unity.h"
#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

void setUp(void) {}
void tearDown(void) {}

void test_codegen_emits_assignment_store(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;
    char *assembly;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 10; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));

    assembly = codegen_generate_program(program, &symbols);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "mov dword [x], 10"));

    free(assembly);
    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_codegen_emits_print_helper_calls(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;
    char *assembly;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 2 + 3; escreval x; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));

    assembly = codegen_generate_program(program, &symbols);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "call print_int"));
    TEST_ASSERT_NOT_NULL(strstr(assembly, "call print_newline"));

    free(assembly);
    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_codegen_emits_assignment_store);
    RUN_TEST(test_codegen_emits_print_helper_calls);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the codegen test and verify it fails**

Run: `make test-codegen`  
Expected: compile failure because `codegen.h` and `codegen_generate_program` do not exist.

- [ ] **Step 3: Implement the code generator**

```c
/* src/codegen.h */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "semantic.h"

char *codegen_generate_program(const ASTProgram *program, const SymbolTable *symbols);

#endif
```

```c
/* src/codegen.c */
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_header(FILE *stream, const SymbolTable *symbols) {
    size_t i;
    fputs("section .data\n", stream);
    for (i = 0; i < symbols->count; ++i) {
        fprintf(stream, "%s dd 0\n", symbols->names[i]);
    }
    fputs("newline db 10\n\nsection .text\nglobal _start\n_start:\n", stream);
}

static void emit_exit(FILE *stream) {
    fputs("mov eax, 1\nxor ebx, ebx\nint 0x80\n", stream);
}

static void emit_command(FILE *stream, const ASTCommand *command) {
    if (command->type == AST_COMMAND_ASSIGNMENT) {
        fprintf(stream, "mov dword [%s], eax\n", command->assignment.name);
        return;
    }
    fputs("call print_int\n", stream);
    if (command->type == AST_COMMAND_WRITELN) {
        fputs("call print_newline\n", stream);
    }
}

char *codegen_generate_program(const ASTProgram *program, const SymbolTable *symbols) {
    char *buffer = NULL;
    size_t size = 0;
    size_t i;
    FILE *stream = open_memstream(&buffer, &size);

    emit_header(stream, symbols);
    for (i = 0; i < program->command_count; ++i) {
        emit_command(stream, &program->commands[i]);
    }
    emit_exit(stream);
    fclose(stream);
    return buffer;
}
```

- [ ] **Step 4: Add one test for integer division**

```c
void test_codegen_emits_idiv_for_div_operator(void) {
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;
    char *assembly;

    TEST_ASSERT_TRUE(lexer_scan("programa demo inteiro x; inicio x <- 8 div 2; fim", &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));

    assembly = codegen_generate_program(program, &symbols);
    TEST_ASSERT_NOT_NULL(strstr(assembly, "idiv"));

    free(assembly);
    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}
```

- [ ] **Step 5: Run the codegen test suite and verify it passes**

Run: `make test-codegen`  
Expected: `OK (3 tests)`

- [ ] **Step 6: Commit code generation**

```bash
git add Makefile src/codegen.h src/codegen.c tests/test_codegen.c
git commit -m "feat: add NASM generation for MVP subset"
```

### Task 6: Add the CLI, fixtures, and end-to-end verification

**Files:**
- Create: `src/main.c`
- Create: `tests/test_e2e.sh`
- Create: `examples/assign.simples`
- Create: `examples/print.simples`
- Modify: `Makefile`

- [ ] **Step 1: Write the end-to-end shell test first**

```sh
#!/bin/sh
set -eu

mkdir -p build/e2e
build/simplesc examples/print.simples build/e2e/print.asm
nasm -f elf32 build/e2e/print.asm -o build/e2e/print.o
ld -m elf_i386 build/e2e/print.o -o build/e2e/print

output="$(build/e2e/print)"
[ "$output" = "5" ]
echo "e2e ok"
```

- [ ] **Step 2: Run the end-to-end test and verify it fails**

Run: `make e2e`  
Expected: failure because `build/simplesc` and the example fixtures do not exist yet.

- [ ] **Step 3: Implement the CLI and the example inputs**

```c
/* src/main.c */
#include <stdio.h>
#include <stdlib.h>
#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

static char *read_entire_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    long length;
    char *buffer;
    fseek(fp, 0, SEEK_END);
    length = ftell(fp);
    rewind(fp);
    buffer = malloc((size_t)length + 1);
    fread(buffer, 1, (size_t)length, fp);
    buffer[length] = '\0';
    fclose(fp);
    return buffer;
}

static int write_text_file(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    fputs(text, fp);
    fclose(fp);
    return 0;
}

static int report_error(const CompilerError *error) {
    fprintf(stderr, "%d:%d: %s\n", error->line, error->column, error->message);
    return 1;
}

int compile_file(const char *input_path, const char *output_path) {
    char *source = read_entire_file(input_path);
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols;
    CompilerError error;
    char *assembly;

    if (!lexer_scan(source, &tokens, &error)) return report_error(&error);
    if (!parse_program(&tokens, &program, &error)) return report_error(&error);
    if (!analyze_program(program, &symbols, &error)) return report_error(&error);

    assembly = codegen_generate_program(program, &symbols);
    write_text_file(output_path, assembly);
    free(assembly);
    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
    free(source);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.simples> <output.asm>\n", argv[0]);
        return 1;
    }
    return compile_file(argv[1], argv[2]);
}
```

```text
// examples/assign.simples
programa demo
inteiro x;
inicio
x <- 10;
fim
```

```text
// examples/print.simples
programa demo
inteiro x;
inicio
x <- 2 + 3;
escreval x;
fim
```

- [ ] **Step 4: Add one explicit CLI failure-path check**

```bash
./build/simplesc
```

Expected: exit status `1` and usage text on stderr.

- [ ] **Step 5: Run the full test and end-to-end suite**

Run: `make test && make e2e`  
Expected:

```text
OK (2 tests)
OK (3 tests)
OK (3 tests)
OK (3 tests)
OK (3 tests)
e2e ok
```

- [ ] **Step 6: Commit the CLI and fixtures**

```bash
git add Makefile src/main.c tests/test_e2e.sh examples/assign.simples examples/print.simples
git commit -m "feat: add CLI and end-to-end coverage"
```

## Final Verification

After Task 6, run this manual acceptance sequence before merging:

```bash
make clean
make test
make e2e
build/simplesc examples/assign.simples build/assign.asm
head -40 build/assign.asm
```

Expected outcomes:

- All unit suites pass
- End-to-end script prints `e2e ok`
- `build/assign.asm` exists
- The assembly contains `_start` and the declared variable symbol
