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

void test_semantic_accepts_numeric_matrix_element_assignment_and_read(void) {
    const char *source =
        "programa demo\n"
        "inteiro m[2][3], x;\n"
        "inicio\n"
        "  m[1][2] <- 7;\n"
        "  leia m[0][1];\n"
        "  x <- m[1][2];\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(2, info.global_count);
    TEST_ASSERT_EQUAL_size_t(2, info.globals[0].dimension_count);
    TEST_ASSERT_EQUAL_size_t(3, info.globals[0].row_capacity);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_matrix_access_with_single_index(void) {
    const char *source =
        "programa demo\n"
        "inteiro m[2][3], x;\n"
        "inicio\n"
        "  x <- m[1];\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Matriz 'm' requer dois indices.", error.message);

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
        "procedimento flutuante identidade(flutuante entrada)\n"
        "inicio\n"
        "  retorna entrada;\n"
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
    TEST_ASSERT_EQUAL(AST_TYPE_FLUTUANTE, info.procedures[0].parameters[0].type);
    TEST_ASSERT_EQUAL(AST_STORAGE_SCALAR, info.procedures[0].parameters[0].storage);
    TEST_ASSERT_EQUAL_size_t(0, info.procedures[0].parameters[0].capacity);

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

void test_semantic_rejects_string_literal_that_exceeds_capacity(void) {
    const char *source =
        "programa demo\n"
        "string nome[4];\n"
        "inicio\n"
        "  nome <- \"abcd\";\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Expressao string excede capacidade 4 de 'nome'.", error.message);

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

void test_semantic_rejects_vector_in_write_expression(void) {
    const char *source =
        "programa demo\n"
        "inteiro nums[3];\n"
        "inicio\n"
        "  escreval nums;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'nums' nao pode ser usado como escalar.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_vector_as_leia_target(void) {
    const char *source =
        "programa demo\n"
        "inteiro nums[3];\n"
        "inicio\n"
        "  leia nums;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'nums' nao pode ser alvo de 'leia'.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_vector_as_plain_assignment_target(void) {
    const char *source =
        "programa demo\n"
        "inteiro nums[3];\n"
        "inicio\n"
        "  nums <- 5;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING("Identificador 'nums' nao pode ser usado como escalar.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_plain_assignment(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "inicio\n"
        "  nome <- \"abc\";\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_leia_target(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "inicio\n"
        "  leia nome;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_in_write_expression(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "inicio\n"
        "  nome <- \"oi\";\n"
        "  escreval nome;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_to_string_variable_assignment(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "string outro[20];\n"
        "inicio\n"
        "  nome <- outro;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_element_integer_assignment(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "inicio\n"
        "  nome[0] <- 65;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_integer_read_from_string_element(void) {
    const char *source =
        "programa demo\n"
        "string nome[20];\n"
        "inteiro x;\n"
        "inicio\n"
        "  nome <- \"abc\";\n"
        "  x <- nome[0];\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_indexed_procedure_parameter(void) {
    const char *source =
        "procedimento vazio p(inteiro nums[2])\n"
        "inicio\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Parametro 'nums' em vetor requer passagem por valor.",
        error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_matrix_reference_parameter(void) {
    const char *source =
        "procedimento inteiro soma(inteiro nums[2][3])\n"
        "inicio\n"
        "  retorna nums[1][2];\n"
        "fim\n"
        "programa demo\n"
        "inteiro nums[2][3], x;\n"
        "inicio\n"
        "  x <- soma(nums);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(2, info.procedures[0].parameters[0].dimension_count);
    TEST_ASSERT_EQUAL(AST_PASS_BY_REFERENCE, info.procedures[0].parameters[0].pass_mode);
    TEST_ASSERT_EQUAL_size_t(3, info.procedures[0].parameters[0].row_capacity);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_collects_string_parameter_storage_and_capacity(void) {
    const char *source =
        "procedimento vazio saudacao(string nome[16])\n"
        "inicio\n"
        "  escreval \"oi\";\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.procedure_count);
    TEST_ASSERT_EQUAL_size_t(1, info.procedures[0].parameter_count);
    TEST_ASSERT_EQUAL(AST_TYPE_STRING, info.procedures[0].parameters[0].type);
    TEST_ASSERT_EQUAL(AST_STORAGE_INDEXED, info.procedures[0].parameters[0].storage);
    TEST_ASSERT_EQUAL_size_t(16, info.procedures[0].parameters[0].capacity);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_return_and_value_parameter(void) {
    const char *source =
        "procedimento string[24] copia(string nome[16] valor)\n"
        "inicio\n"
        "  retorna nome;\n"
        "fim\n"
        "programa demo\n"
        "string destino[24];\n"
        "inicio\n"
        "  destino <- copia(\"ana\");\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.procedure_count);
    TEST_ASSERT_EQUAL(AST_TYPE_STRING, info.procedures[0].return_type);
    TEST_ASSERT_EQUAL_size_t(24, info.procedures[0].return_capacity);
    TEST_ASSERT_EQUAL(AST_PASS_BY_VALUE, info.procedures[0].parameters[0].pass_mode);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_vector_value_parameter(void) {
    const char *source =
        "procedimento inteiro soma(inteiro nums[3] valor)\n"
        "inicio\n"
        "  retorna nums[0] + nums[1] + nums[2];\n"
        "fim\n"
        "programa demo\n"
        "inteiro nums[3];\n"
        "inteiro total;\n"
        "inicio\n"
        "  nums[0] <- 1;\n"
        "  nums[1] <- 2;\n"
        "  nums[2] <- 3;\n"
        "  total <- soma(nums);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_size_t(1, info.procedure_count);
    TEST_ASSERT_EQUAL(AST_PASS_BY_VALUE, info.procedures[0].parameters[0].pass_mode);
    TEST_ASSERT_EQUAL(AST_STORAGE_INDEXED, info.procedures[0].parameters[0].storage);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_string_variable_argument_for_string_parameter(void) {
    const char *source =
        "procedimento vazio saudacao(string nome[16])\n"
        "inicio\n"
        "  escreval nome;\n"
        "  nome[0] <- 65;\n"
        "  nome <- \"abc\";\n"
        "fim\n"
        "programa demo\n"
        "string pessoa[16];\n"
        "inicio\n"
        "  pessoa <- \"ana\";\n"
        "  saudacao(pessoa);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_string_literal_argument_for_string_parameter(void) {
    const char *source =
        "procedimento vazio saudacao(string nome[16])\n"
        "inicio\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  saudacao(\"ana\");\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Argumento 1 de 'saudacao' deve ser uma variavel string nomeada.",
        error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_string_argument_with_smaller_capacity_than_parameter(void) {
    const char *source =
        "procedimento vazio saudacao(string nome[16])\n"
        "inicio\n"
        "fim\n"
        "programa demo\n"
        "string pessoa[8];\n"
        "inicio\n"
        "  saudacao(pessoa);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_SEMANTIC, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Argumento 1 de 'saudacao' requer capacidade minima 16, mas recebeu 8.",
        error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_float_literal_assignment_in_main(void) {
    const char *source =
        "programa demo\n"
        "flutuante x;\n"
        "inicio\n"
        "  x <- 1.5;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_integer_to_float_promotion_in_assignment(void) {
    const char *source =
        "programa demo\n"
        "flutuante y;\n"
        "inicio\n"
        "  y <- 2;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_float_to_integer_assignment(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- 1.5;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Variavel 'x' espera tipo 'inteiro', mas recebeu 'flutuante'.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_explicit_float_to_integer_conversion(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- inteiro(1.5);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_mixed_integer_float_arithmetic_assignment(void) {
    const char *source =
        "programa demo\n"
        "inteiro x;\n"
        "flutuante y, z;\n"
        "inicio\n"
        "  x <- 2;\n"
        "  y <- 1.5;\n"
        "  z <- x + y;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_float_division(void) {
    const char *source =
        "programa demo\n"
        "flutuante x;\n"
        "inicio\n"
        "  x <- 7.5 div 2;\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_integer_argument_promotion_for_float_parameter(void) {
    const char *source =
        "procedimento flutuante dup(flutuante x)\n"
        "inicio\n"
        "  retorna x;\n"
        "fim\n"
        "programa demo\n"
        "flutuante y;\n"
        "inicio\n"
        "  y <- dup(1);\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_integer_return_promotion_for_float_procedure(void) {
    const char *source =
        "procedimento flutuante dois()\n"
        "inicio\n"
        "  retorna 2;\n"
        "fim\n"
        "programa demo\n"
        "flutuante y;\n"
        "inicio\n"
        "  y <- dois();\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_rejects_float_return_for_integer_procedure(void) {
    const char *source =
        "procedimento inteiro falha()\n"
        "inicio\n"
        "  retorna 1.5;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_FALSE(analyze_program(program, &info, &error));
    TEST_ASSERT_EQUAL_STRING("Procedimento 'falha' deve retornar tipo 'inteiro', mas recebeu 'flutuante'.", error.message);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
}

void test_semantic_accepts_float_comparisons_in_control_flow(void) {
    const char *source =
        "programa demo\n"
        "flutuante x, y;\n"
        "inteiro ok;\n"
        "inicio\n"
        "  x <- 1.5;\n"
        "  y <- 2;\n"
        "  se x < y entao\n"
        "    ok <- 1;\n"
        "  fimse\n"
        "  enquanto y > x faca\n"
        "    y <- x;\n"
        "  fimenquanto\n"
        "fim";
    TokenList tokens;
    ASTProgram *program = parse_source(source, &tokens);
    SemanticInfo info = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

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
    RUN_TEST(test_semantic_accepts_numeric_matrix_element_assignment_and_read);
    RUN_TEST(test_semantic_rejects_matrix_access_with_single_index);
    RUN_TEST(test_semantic_rejects_call_with_wrong_argument_count);
    RUN_TEST(test_semantic_rejects_access_to_global_from_procedure_scope);
    RUN_TEST(test_semantic_rejects_void_procedure_in_expression);
    RUN_TEST(test_semantic_rejects_argument_with_wrong_type);
    RUN_TEST(test_semantic_rejects_return_outside_procedure);
    RUN_TEST(test_semantic_rejects_void_procedure_returning_expression);
    RUN_TEST(test_semantic_rejects_non_void_procedure_without_return_on_all_paths);
    RUN_TEST(test_semantic_accepts_program_and_collects_procedure_signatures);
    RUN_TEST(test_semantic_rejects_string_without_capacity);
    RUN_TEST(test_semantic_rejects_string_literal_that_exceeds_capacity);
    RUN_TEST(test_semantic_rejects_scalar_identifier_indexing);
    RUN_TEST(test_semantic_rejects_scalar_identifier_indexing_in_assignment_target);
    RUN_TEST(test_semantic_rejects_vector_in_write_expression);
    RUN_TEST(test_semantic_rejects_vector_as_leia_target);
    RUN_TEST(test_semantic_rejects_vector_as_plain_assignment_target);
    RUN_TEST(test_semantic_accepts_string_plain_assignment);
    RUN_TEST(test_semantic_accepts_string_leia_target);
    RUN_TEST(test_semantic_accepts_string_in_write_expression);
    RUN_TEST(test_semantic_accepts_string_to_string_variable_assignment);
    RUN_TEST(test_semantic_accepts_string_element_integer_assignment);
    RUN_TEST(test_semantic_accepts_integer_read_from_string_element);
    RUN_TEST(test_semantic_rejects_indexed_procedure_parameter);
    RUN_TEST(test_semantic_accepts_matrix_reference_parameter);
    RUN_TEST(test_semantic_collects_string_parameter_storage_and_capacity);
    RUN_TEST(test_semantic_accepts_string_variable_argument_for_string_parameter);
    RUN_TEST(test_semantic_rejects_string_literal_argument_for_string_parameter);
    RUN_TEST(test_semantic_rejects_string_argument_with_smaller_capacity_than_parameter);
    RUN_TEST(test_semantic_accepts_string_return_and_value_parameter);
    RUN_TEST(test_semantic_accepts_vector_value_parameter);
    RUN_TEST(test_semantic_accepts_float_literal_assignment_in_main);
    RUN_TEST(test_semantic_accepts_integer_to_float_promotion_in_assignment);
    RUN_TEST(test_semantic_rejects_float_to_integer_assignment);
    RUN_TEST(test_semantic_accepts_explicit_float_to_integer_conversion);
    RUN_TEST(test_semantic_accepts_mixed_integer_float_arithmetic_assignment);
    RUN_TEST(test_semantic_accepts_float_division);
    RUN_TEST(test_semantic_accepts_integer_argument_promotion_for_float_parameter);
    RUN_TEST(test_semantic_accepts_integer_return_promotion_for_float_procedure);
    RUN_TEST(test_semantic_rejects_float_return_for_integer_procedure);
    RUN_TEST(test_semantic_accepts_float_comparisons_in_control_flow);
    return UNITY_END();
}
