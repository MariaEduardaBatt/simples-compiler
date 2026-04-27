#include "unity.h"
#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"

#include <stdbool.h>

void setUp(void) {}

void tearDown(void) {}

static bool scan_source(const char *source, TokenList *tokens) {
    CompilerError error = {0};

    token_list_init(tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, tokens, &error));
    return true;
}

static ASTProgram *parse_source(const char *source, TokenList *tokens) {
    CompilerError error = {0};
    ASTProgram *program = NULL;

    scan_source(source, tokens);
    TEST_ASSERT_TRUE(parse_program(tokens, &program, &error));
    TEST_ASSERT_NOT_NULL(program);
    return program;
}

void test_parser_builds_assignment_ast_with_expected_counts_and_shape(void) {
    const char *source = "programa demo inteiro x; inicio x <- 1 + 2 * 3; fim";
    TokenList tokens;
    CompilerError error = {0};
    ASTProgram *program = NULL;

    scan_source(source, &tokens);

    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NOT_NULL(program);
    TEST_ASSERT_EQUAL_STRING("demo", program->name);
    TEST_ASSERT_EQUAL_size_t(1, program->declaration_count);
    TEST_ASSERT_EQUAL_size_t(1, program->command_count);
    TEST_ASSERT_EQUAL_STRING("x", program->declarations[0].name);
    TEST_ASSERT_EQUAL(AST_COMMAND_ASSIGNMENT, program->commands[0].type);
    TEST_ASSERT_EQUAL_STRING("x", program->commands[0].assignment.name);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, program->commands[0].assignment.expression->type);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_respects_multiplication_precedence_in_assignment_expression(void) {
    const char *source = "programa demo inteiro x; inicio x <- 1 + 2 * 3; fim";
    TokenList tokens;
    CompilerError error = {0};
    ASTProgram *program = NULL;
    ASTExpression *expression;

    scan_source(source, &tokens);

    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));

    expression = program->commands[0].assignment.expression;
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_ADD, expression->binary.op);
    TEST_ASSERT_NOT_NULL(expression->binary.right);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.right->type);
    TEST_ASSERT_EQUAL(AST_BINARY_MUL, expression->binary.right->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_reports_error_for_missing_command_semicolon(void) {
    const char *source = "programa demo inteiro x; inicio x <- 1 fim";
    TokenList tokens;
    CompilerError error = {0};
    ASTProgram *program = NULL;

    scan_source(source, &tokens);

    TEST_ASSERT_FALSE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NULL(program);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_PARSER, error.phase);

    token_list_free(&tokens);
}

void test_parser_reports_error_for_integer_literal_overflow(void) {
    const char *source = "programa demo inteiro x; inicio x <- 2147483648; fim";
    TokenList tokens;
    CompilerError error = {0};
    ASTProgram *program = NULL;

    scan_source(source, &tokens);

    TEST_ASSERT_FALSE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NULL(program);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_PARSER, error.phase);

    token_list_free(&tokens);
}

void test_parser_reports_error_for_leia_without_identifier(void) {
    const char *source = "programa demo inteiro x; inicio leia; fim";
    TokenList tokens;
    CompilerError error = {0};
    ASTProgram *program = NULL;

    scan_source(source, &tokens);

    TEST_ASSERT_FALSE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NULL(program);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_PARSER, error.phase);
    TEST_ASSERT_EQUAL_STRING("Esperado identificador apos 'leia'.", error.message);

    token_list_free(&tokens);
}

void test_parser_supports_comma_separated_integer_declarations(void) {
    const char *source = "programa demo inteiro x, y, total; inicio fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL_size_t(3, program->declaration_count);
    TEST_ASSERT_EQUAL_STRING("x", program->declarations[0].name);
    TEST_ASSERT_EQUAL_STRING("y", program->declarations[1].name);
    TEST_ASSERT_EQUAL_STRING("total", program->declarations[2].name);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_escreva_and_escreval_commands(void) {
    const char *source = "programa demo inteiro x; inicio escreva x; escreval 1 + 2; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL_size_t(2, program->command_count);
    TEST_ASSERT_EQUAL(AST_COMMAND_WRITE, program->commands[0].type);
    TEST_ASSERT_EQUAL(AST_EXPR_IDENTIFIER, program->commands[0].write.expression->type);
    TEST_ASSERT_EQUAL_STRING("x", program->commands[0].write.expression->identifier);
    TEST_ASSERT_EQUAL(AST_COMMAND_WRITELN, program->commands[1].type);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, program->commands[1].write.expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_ADD, program->commands[1].write.expression->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_preserves_parenthesized_expression_grouping(void) {
    const char *source = "programa demo inteiro x; inicio x <- (1 + 2) * 3; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_MUL, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_BINARY_ADD, expression->binary.left->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_builds_left_associative_subtraction(void) {
    const char *source = "programa demo inteiro x; inicio x <- 10 - 3 - 2; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_SUB, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_BINARY_SUB, expression->binary.left->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_INT, expression->binary.right->type);
    TEST_ASSERT_EQUAL(2, expression->binary.right->int_value);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_builds_left_associative_division(void) {
    const char *source = "programa demo inteiro x; inicio x <- 20 div 5 div 2; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_DIV, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_BINARY_DIV, expression->binary.left->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_INT, expression->binary.right->type);
    TEST_ASSERT_EQUAL(2, expression->binary.right->int_value);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_builds_relational_and_logical_expression_tree(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- nao (1 < 2) ou 3 = 4;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_OR, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_UNARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_UNARY_NOT, expression->binary.left->unary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_preserves_relational_precedence_below_addition(void) {
    const char *source = "programa demo inteiro x; inicio x <- 1 + 2 > 3; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_BINARY_GT, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_BINARY_ADD, expression->binary.left->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_builds_unary_negation_before_multiplication(void) {
    const char *source = "programa demo inteiro x; inicio x <- -1 * 2; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_MUL, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_UNARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_UNARY_NEGATE, expression->binary.left->unary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_applies_not_before_relational_operator_without_parentheses(void) {
    const char *source = "programa demo inteiro x; inicio x <- nao x > 5; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_GT, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_UNARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_UNARY_NOT, expression->binary.left->unary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_builds_left_associative_logical_expression_chain(void) {
    const char *source = "programa demo inteiro x; inicio x <- 1 = 1 ou 2 = 2 e 3 = 3; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    ASTExpression *expression = program->commands[0].assignment.expression;

    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->type);
    TEST_ASSERT_EQUAL(AST_BINARY_AND, expression->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->type);
    TEST_ASSERT_EQUAL(AST_BINARY_OR, expression->binary.left->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->binary.left->type);
    TEST_ASSERT_EQUAL(AST_BINARY_EQ, expression->binary.left->binary.left->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.left->binary.right->type);
    TEST_ASSERT_EQUAL(AST_BINARY_EQ, expression->binary.left->binary.right->binary.op);
    TEST_ASSERT_EQUAL(AST_EXPR_BINARY, expression->binary.right->type);
    TEST_ASSERT_EQUAL(AST_BINARY_EQ, expression->binary.right->binary.op);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_if_with_else_blocks(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  se x > 0 entao\n"
        "    escreva x;\n"
        "  senao\n"
        "    escreval 0;\n"
        "  fimse\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_COMMAND_IF, program->commands[0].type);
    TEST_ASSERT_EQUAL_size_t(1, program->commands[0].if_command.then_count);
    TEST_ASSERT_EQUAL_size_t(1, program->commands[0].if_command.else_count);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_while_loop_body(void) {
    const char *source =
        "programa demo inteiro x; inicio enquanto x < 3 faca x <- x + 1; fimenquanto fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_COMMAND_WHILE, program->commands[0].type);
    TEST_ASSERT_EQUAL_size_t(1, program->commands[0].while_command.body_count);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_for_loop_header_and_body(void) {
    const char *source =
        "programa demo inteiro i, total; inicio para i de 1 ate 3 passo 1 faca total <- total + i; fimpara fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_COMMAND_FOR, program->commands[0].type);
    TEST_ASSERT_EQUAL_STRING("i", program->commands[0].for_command.iterator_name);
    TEST_ASSERT_EQUAL_size_t(1, program->commands[0].for_command.body_count);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_read_command_target(void) {
    const char *source = "programa demo inteiro x; inicio leia x; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL_size_t(1, program->command_count);
    TEST_ASSERT_EQUAL(AST_COMMAND_READ, program->commands[0].type);
    TEST_ASSERT_EQUAL_STRING("x", program->commands[0].read.name);
    TEST_ASSERT_EQUAL_INT(1, program->commands[0].read.line);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_accepts_optional_top_level_procedures_before_program(void) {
    const char *source =
        "procedimento inteiro soma(inteiro a, inteiro b)\n"
        "inicio\n"
        "  retorna a + b;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- soma(1, 2);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL_size_t(1, program->procedure_count);
    TEST_ASSERT_EQUAL_STRING("soma", program->procedures[0].name);
    TEST_ASSERT_EQUAL(AST_TYPE_INTEIRO, program->procedures[0].return_type);
    TEST_ASSERT_EQUAL_size_t(2, program->procedures[0].parameter_count);
    TEST_ASSERT_EQUAL(AST_EXPR_CALL, program->commands[0].assignment.expression->type);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_accepts_void_call_command_and_return_without_expression(void) {
    const char *source =
        "procedimento vazio ping()\n"
        "inicio\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  ping();\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_COMMAND_RETURN, program->procedures[0].commands[0].type);
    TEST_ASSERT_EQUAL(AST_COMMAND_CALL, program->commands[0].type);

    ast_program_free(program);
    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parser_builds_assignment_ast_with_expected_counts_and_shape);
    RUN_TEST(test_parser_respects_multiplication_precedence_in_assignment_expression);
    RUN_TEST(test_parser_reports_error_for_missing_command_semicolon);
    RUN_TEST(test_parser_reports_error_for_integer_literal_overflow);
    RUN_TEST(test_parser_reports_error_for_leia_without_identifier);
    RUN_TEST(test_parser_supports_comma_separated_integer_declarations);
    RUN_TEST(test_parser_parses_escreva_and_escreval_commands);
    RUN_TEST(test_parser_preserves_parenthesized_expression_grouping);
    RUN_TEST(test_parser_builds_left_associative_subtraction);
    RUN_TEST(test_parser_builds_left_associative_division);
    RUN_TEST(test_parser_builds_relational_and_logical_expression_tree);
    RUN_TEST(test_parser_preserves_relational_precedence_below_addition);
    RUN_TEST(test_parser_builds_unary_negation_before_multiplication);
    RUN_TEST(test_parser_applies_not_before_relational_operator_without_parentheses);
    RUN_TEST(test_parser_builds_left_associative_logical_expression_chain);
    RUN_TEST(test_parser_parses_if_with_else_blocks);
    RUN_TEST(test_parser_parses_while_loop_body);
    RUN_TEST(test_parser_parses_for_loop_header_and_body);
    RUN_TEST(test_parser_parses_read_command_target);
    RUN_TEST(test_parser_accepts_optional_top_level_procedures_before_program);
    RUN_TEST(test_parser_accepts_void_call_command_and_return_without_expression);
    return UNITY_END();
}
