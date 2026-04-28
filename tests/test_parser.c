#include "unity.h"
#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

static char *copy_text(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    TEST_ASSERT_NOT_NULL(copy);
    memcpy(copy, text, length);
    return copy;
}

static ASTExpression *make_int_expression(int value) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    TEST_ASSERT_NOT_NULL(expression);
    expression->type = AST_EXPR_INT;
    expression->int_value = value;
    return expression;
}

static ASTExpression *make_identifier_expression(const char *name) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    TEST_ASSERT_NOT_NULL(expression);
    expression->type = AST_EXPR_IDENTIFIER;
    expression->identifier = copy_text(name);
    return expression;
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
    TEST_ASSERT_EQUAL(AST_TARGET_IDENTIFIER, program->commands[0].assignment.target.type);
    TEST_ASSERT_EQUAL_STRING("x", program->commands[0].assignment.target.identifier);
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

void test_ast_procedure_free_releases_owned_members_and_resets_fields(void) {
    ASTProcedure procedure = {0};

    procedure.name = copy_text("ping");
    procedure.parameters = calloc(1, sizeof(*procedure.parameters));
    procedure.local_declarations = calloc(1, sizeof(*procedure.local_declarations));
    procedure.commands = calloc(2, sizeof(*procedure.commands));

    TEST_ASSERT_NOT_NULL(procedure.parameters);
    TEST_ASSERT_NOT_NULL(procedure.local_declarations);
    TEST_ASSERT_NOT_NULL(procedure.commands);

    procedure.parameter_count = 1;
    procedure.parameters[0].name = copy_text("value");

    procedure.local_declaration_count = 1;
    procedure.local_declarations[0].name = copy_text("local");

    procedure.command_count = 2;
    procedure.commands[0].type = AST_COMMAND_CALL;
    procedure.commands[0].call_command.call.name = copy_text("print");
    procedure.commands[0].call_command.call.arguments = calloc(2, sizeof(*procedure.commands[0].call_command.call.arguments));
    TEST_ASSERT_NOT_NULL(procedure.commands[0].call_command.call.arguments);
    procedure.commands[0].call_command.call.argument_count = 2;
    procedure.commands[0].call_command.call.arguments[0] = make_int_expression(1);
    procedure.commands[0].call_command.call.arguments[1] = make_identifier_expression("value");

    procedure.commands[1].type = AST_COMMAND_RETURN;
    procedure.commands[1].return_command.expression = make_int_expression(0);

    ast_procedure_free(&procedure);

    TEST_ASSERT_NULL(procedure.name);
    TEST_ASSERT_NULL(procedure.parameters);
    TEST_ASSERT_EQUAL_size_t(0, procedure.parameter_count);
    TEST_ASSERT_NULL(procedure.local_declarations);
    TEST_ASSERT_EQUAL_size_t(0, procedure.local_declaration_count);
    TEST_ASSERT_NULL(procedure.commands);
    TEST_ASSERT_EQUAL_size_t(0, procedure.command_count);
}

void test_parser_parses_indexed_declarations_and_indexed_assignment(void) {
    const char *source =
        "programa demo\n"
        "inteiro nums[10];\n"
        "inicio\n"
        "  nums[1] <- 42;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_STORAGE_INDEXED, program->declarations[0].storage);
    TEST_ASSERT_EQUAL_size_t(10, program->declarations[0].capacity);
    TEST_ASSERT_EQUAL(AST_TARGET_INDEXED, program->commands[0].assignment.target.type);

    ast_program_free(program);
    token_list_free(&tokens);
}

void test_parser_parses_string_literal_expression(void) {
    const char *source =
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  nome <- \"ana\";\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);

    TEST_ASSERT_EQUAL(AST_EXPR_STRING, program->commands[0].assignment.expression->type);
    TEST_ASSERT_EQUAL_STRING("ana", program->commands[0].assignment.expression->string_value);

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
    RUN_TEST(test_ast_procedure_free_releases_owned_members_and_resets_fields);
    RUN_TEST(test_parser_parses_indexed_declarations_and_indexed_assignment);
    RUN_TEST(test_parser_parses_string_literal_expression);
    return UNITY_END();
}
