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

void test_semantic_rejects_undeclared_identifier_inside_if_condition(void) {
    const char *source = "programa demo inteiro x; inicio se y > 0 entao escreva x; fimse fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_uses_else_count_as_source_of_truth_for_if_else(void) {
    ASTDeclaration declaration = {.name = "x", .line = 1, .column = 1};
    ASTExpression condition = {.type = AST_EXPR_INT, .line = 1, .column = 1, .int_value = 1};
    ASTExpression else_expression = {.type = AST_EXPR_IDENTIFIER, .line = 1, .column = 1, .identifier = "y"};
    ASTCommand else_command = {.type = AST_COMMAND_WRITE, .write = {.expression = &else_expression}};
    ASTCommand if_command = {.type = AST_COMMAND_IF,
                             .if_command = {.condition = &condition,
                                            .then_commands = NULL,
                                            .then_count = 0,
                                            .else_commands = &else_command,
                                            .else_count = 1}};
    ASTProgram program = {.name = "demo", .declarations = &declaration, .declaration_count = 1, .commands = &if_command, .command_count = 1};
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(&program, &symbols, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    symbol_table_free(&symbols);
}

void test_semantic_rejects_undeclared_identifier_inside_while_body(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  enquanto x < 3 faca\n"
        "    y <- x + 1;\n"
        "  fimenquanto\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_identifier_in_while_condition(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  enquanto z < 3 faca\n"
        "    x <- x + 1;\n"
        "  fimenquanto\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'z' nao declarado.", error.message);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_for_iterator(void) {
    const char *source = "programa demo inteiro total; inicio para i de 1 ate 3 passo 1 faca total <- total + 1; fimpara fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'i' nao declarado.", error.message);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_for_codegen_like_identifier_names(void) {
    const char *source =
        "programa demo\n"
        "inteiro _for_end_0,\n"
        "        _for_step_0;\n"
        "inicio\n"
        "  _for_end_0 <- 1;\n"
        "  escreva _for_step_0;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SymbolTable symbols = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));
    TEST_ASSERT_EQUAL_size_t(2, symbols.count);
    TEST_ASSERT_EQUAL_STRING("_for_end_0", symbols.names[0]);
    TEST_ASSERT_EQUAL_STRING("_for_step_0", symbols.names[1]);

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
    RUN_TEST(test_semantic_rejects_undeclared_identifier_inside_if_condition);
    RUN_TEST(test_semantic_uses_else_count_as_source_of_truth_for_if_else);
    RUN_TEST(test_semantic_rejects_undeclared_identifier_inside_while_body);
    RUN_TEST(test_semantic_rejects_undeclared_identifier_in_while_condition);
    RUN_TEST(test_semantic_rejects_undeclared_for_iterator);
    RUN_TEST(test_semantic_accepts_for_codegen_like_identifier_names);
    return UNITY_END();
}
