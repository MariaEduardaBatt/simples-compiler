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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'x' ja declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(3, error.line);
    TEST_ASSERT_EQUAL_INT(9, error.column);
    TEST_ASSERT_EQUAL_size_t(0, info.global_count);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(11, error.column);
    TEST_ASSERT_EQUAL_size_t(0, info.global_count);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(3, error.column);
    TEST_ASSERT_EQUAL_size_t(0, info.global_count);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(8, error.column);
    TEST_ASSERT_EQUAL_size_t(0, info.global_count);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.global_count);
    TEST_ASSERT_EQUAL_STRING("x", info.globals[0].name);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);
    TEST_ASSERT_EQUAL_INT(4, error.line);
    TEST_ASSERT_EQUAL_INT(9, error.column);
    TEST_ASSERT_EQUAL_size_t(0, info.global_count);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_declared_variables_and_collects_symbols(void) {
    const char *source = "programa demo inteiro x, y; inicio x <- 1; escreva x + y; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(2, info.global_count);
    TEST_ASSERT_EQUAL_STRING("x", info.globals[0].name);
    TEST_ASSERT_EQUAL_STRING("y", info.globals[1].name);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_identifier_inside_if_condition(void) {
    const char *source = "programa demo inteiro x; inicio se y > 0 entao escreva x; fimse fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    semantic_info_free(&info);
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
    ASTProgram program = {.name = "demo", .procedures = NULL, .procedure_count = 0, .declarations = &declaration, .declaration_count = 1, .commands = &if_command, .command_count = 1};
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(&program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'y' nao declarado.", error.message);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'z' nao declarado.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_for_iterator(void) {
    const char *source = "programa demo inteiro total; inicio para i de 1 ate 3 passo 1 faca total <- total + 1; fimpara fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'i' nao declarado.", error.message);

    semantic_info_free(&info);
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
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(2, info.global_count);
    TEST_ASSERT_EQUAL_STRING("_for_end_0", info.globals[0].name);
    TEST_ASSERT_EQUAL_STRING("_for_step_0", info.globals[1].name);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_undeclared_read_target(void) {
    const char *source = "programa demo inicio leia x; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'x' nao declarado.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_declared_read_target(void) {
    const char *source = "programa demo inteiro x; inicio leia x; fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.global_count);
    TEST_ASSERT_EQUAL_STRING("x", info.globals[0].name);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_call_with_wrong_argument_count(void) {
    const char *source =
        "procedimento inteiro soma(inteiro a, inteiro b)\n"
        "inicio\n"
        "  retorna a + b;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- soma(1);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Procedimento 'soma' espera 2 argumentos, mas recebeu 1.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_access_to_global_from_procedure_scope(void) {
    const char *source =
        "procedimento inteiro soma(inteiro a)\n"
        "inicio\n"
        "  retorna a + global;\n"
        "fim\n"
        "programa demo\n"
        "inteiro global;\n"
        "inicio\n"
        "  global <- 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'global' nao declarado.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_void_procedure_in_expression(void) {
    const char *source =
        "procedimento vazio ping()\n"
        "inicio\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- ping();\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Procedimento 'ping' com retorno vazio nao pode ser usado em expressao.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_argument_with_wrong_type(void) {
    const char *source =
        "procedimento inteiro duplica(inteiro a)\n"
        "inicio\n"
        "  retorna a + a;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- duplica(1.5);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Argumento 1 de 'duplica' espera tipo 'inteiro', mas recebeu 'flutuante'.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_return_outside_procedure(void) {
    const char *source =
        "programa demo\n"
        "inicio\n"
        "  retorna 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Comando 'retorna' fora de procedimento.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_void_procedure_returning_expression(void) {
    const char *source =
        "procedimento vazio ping()\n"
        "inicio\n"
        "  retorna 1;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  ping();\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Procedimento vazio nao pode retornar expressao.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_non_void_procedure_without_return_on_all_paths(void) {
    const char *source =
        "procedimento inteiro escolhe(inteiro a)\n"
        "inicio\n"
        "  se a > 0 entao\n"
        "    retorna a;\n"
        "  fimse\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Procedimento 'escolhe' deve retornar valor em todos os caminhos.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_program_and_collects_procedure_signatures(void) {
    const char *source =
        "procedimento flutuante identidade(flutuante valor)\n"
        "inicio\n"
        "  retorna valor;\n"
        "fim\n"
        "programa demo\n"
        "flutuante total;\n"
        "inicio\n"
        "  total <- identidade(1.5);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.global_count);
    TEST_ASSERT_EQUAL_size_t(1, info.procedure_count);
    TEST_ASSERT_EQUAL_STRING("identidade", info.procedures[0].name);
    TEST_ASSERT_EQUAL(AST_TYPE_FLUTUANTE, info.procedures[0].return_type);
    TEST_ASSERT_EQUAL_size_t(1, info.procedures[0].parameter_count);
    TEST_ASSERT_EQUAL(AST_TYPE_FLUTUANTE, info.procedures[0].parameter_types[0]);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}


void test_semantic_rejects_string_without_capacity(void) {
    const char *source =
        "programa demo\n"
        "string nome;\n"
        "inicio\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Declaracao de string requer capacidade fixa.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_scalar_identifier_indexing(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  escreval x[0];\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'x' nao pode ser indexado.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_scalar_identifier_indexing_in_assignment_target(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x[0] <- 1;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Identificador 'x' nao pode ser indexado.", error.message);

    semantic_info_free(&info);
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
    RUN_TEST(test_semantic_rejects_undeclared_read_target);
    RUN_TEST(test_semantic_accepts_declared_read_target);
    RUN_TEST(test_semantic_rejects_call_with_wrong_argument_count);
    RUN_TEST(test_semantic_rejects_access_to_global_from_procedure_scope);
    RUN_TEST(test_semantic_rejects_void_procedure_in_expression);
    RUN_TEST(test_semantic_rejects_argument_with_wrong_type);
    RUN_TEST(test_semantic_rejects_return_outside_procedure);
    RUN_TEST(test_semantic_rejects_void_procedure_returning_expression);
    RUN_TEST(test_semantic_rejects_non_void_procedure_without_return_on_all_paths);
    RUN_TEST(test_semantic_accepts_program_and_collects_procedure_signatures);
    RUN_TEST(test_semantic_rejects_string_without_capacity);
    RUN_TEST(test_semantic_rejects_scalar_identifier_indexing);
    RUN_TEST(test_semantic_rejects_scalar_identifier_indexing_in_assignment_target);
    return UNITY_END();
}
