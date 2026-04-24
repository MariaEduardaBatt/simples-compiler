#include "unity.h"
#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

void setUp(void) {}

void tearDown(void) {}

static ASTProgram *parse_source(const char *source, TokenList *tokens) {
    CompilerError error = {0};
    ASTProgram *program = NULL;

    token_list_init(tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, tokens, &error));
    TEST_ASSERT_TRUE(parse_program(tokens, &program, &error));
    TEST_ASSERT_NOT_NULL(program);
    return program;
}

void test_semantic_rejects_duplicate_declarations(void) {
    const char *source =
        "programa demo\n"
        "inteiro x,\n"
        "        x;\n"
        "inicio\n"
        "escreva 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'x' ja declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(3, error.line);
    TEST_ASSERT_EQUAL_INT(9, error.column);
    TEST_ASSERT_EQUAL_size_t(0, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_variable_in_write_expression(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  escreva y;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(11, error.column);
    TEST_ASSERT_EQUAL_size_t(0, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_assignment_target(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  y <- 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(3, error.column);
    TEST_ASSERT_EQUAL_size_t(0, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_identifier_in_assignment_expression(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- y + 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(8, error.column);
    TEST_ASSERT_EQUAL_size_t(0, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_unary_integer_and_identifier_expressions(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- -1;\n"
        "  escreva nao x;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_size_t(1, symbols.count);
    TEST_ASSERT_EQUAL_STRING("x", symbols.names[0]);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_identifier_inside_unary_expression(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- -y;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(9, error.column);
    TEST_ASSERT_EQUAL_size_t(0, symbols.count);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_declared_variables_and_collects_symbols(void) {
    const char *source = "programa demo inteiro x, y; inicio x <- 1; escreva x + y; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_size_t(2, symbols.count);
    TEST_ASSERT_EQUAL_STRING("x", symbols.names[0]);
    TEST_ASSERT_EQUAL_STRING("y", symbols.names[1]);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_semantic_rejects_duplicate_declarations);
    RUN_TEST(test_semantic_rejects_undeclared_variable_in_write_expression);
    RUN_TEST(test_semantic_rejects_undeclared_assignment_target);
    RUN_TEST(test_semantic_rejects_undeclared_identifier_in_assignment_expression);
    RUN_TEST(test_semantic_accepts_unary_integer_and_identifier_expressions);
    RUN_TEST(test_semantic_rejects_undeclared_identifier_inside_unary_expression);
    RUN_TEST(test_semantic_accepts_declared_variables_and_collects_symbols);
    return UNITY_END();
}
