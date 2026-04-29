#include "unity.h"
#include "lexer.h"
#include "error.h"
#include "token.h"

#include <stdbool.h>

void setUp(void) {}

void tearDown(void) {}

static void assert_token(const TokenList *tokens, size_t index, TokenType type, const char *lexeme, int line, int column) {
    TEST_ASSERT_TRUE(index < tokens->count);
    TEST_ASSERT_EQUAL(type, tokens->items[index].type);
    TEST_ASSERT_EQUAL_STRING(lexeme, tokens->items[index].lexeme);
    TEST_ASSERT_EQUAL_INT(line, tokens->items[index].line);
    TEST_ASSERT_EQUAL_INT(column, tokens->items[index].column);
}

void test_lexer_scans_program_structure_into_expected_tokens(void) {
    const char *source = "programa demo inicio x <- 42; fim";
    TokenList tokens;
    CompilerError error = {0};
    bool ok;

    token_list_init(&tokens);
    ok = lexer_scan(source, &tokens, &error);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_size_t(9, tokens.count);
    assert_token(&tokens, 0, TOK_PROGRAMA, "programa", 1, 1);
    assert_token(&tokens, 1, TOK_ID, "demo", 1, 10);
    assert_token(&tokens, 2, TOK_INICIO, "inicio", 1, 15);
    assert_token(&tokens, 3, TOK_ID, "x", 1, 22);
    assert_token(&tokens, 4, TOK_ATRIB, "<-", 1, 24);
    assert_token(&tokens, 5, TOK_NUM_INT, "42", 1, 27);
    assert_token(&tokens, 6, TOK_PONTO_VIRGULA, ";", 1, 29);
    assert_token(&tokens, 7, TOK_FIM, "fim", 1, 31);
    assert_token(&tokens, 8, TOK_EOF, "", 1, 34);

    token_list_free(&tokens);
}

void test_lexer_scans_control_flow_keywords_and_relational_operators(void) {
    const char *source = "se x >= 1 entao escreva x; senao escreval 0; fimse";
    TokenList tokens;
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));

    assert_token(&tokens, 0, TOK_SE, "se", 1, 1);
    assert_token(&tokens, 2, TOK_MAIOR_IGUAL, ">=", 1, 6);
    assert_token(&tokens, 4, TOK_ENTAO, "entao", 1, 11);
    assert_token(&tokens, 8, TOK_SENAO, "senao", 1, 28);
    assert_token(&tokens, 12, TOK_FIMSE, "fimse", 1, 46);

    token_list_free(&tokens);
}

void test_lexer_tracks_line_and_column_across_multiple_lines(void) {
    const char *source = "programa demo\ninicio\nescreva 7;\nfim";
    TokenList tokens;
    CompilerError error = {0};
    bool ok;

    token_list_init(&tokens);
    ok = lexer_scan(source, &tokens, &error);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(TOK_ESCREVA, tokens.items[3].type);
    TEST_ASSERT_EQUAL_INT(3, tokens.items[3].line);
    TEST_ASSERT_EQUAL_INT(1, tokens.items[3].column);
    TEST_ASSERT_EQUAL(TOK_NUM_INT, tokens.items[4].type);
    TEST_ASSERT_EQUAL_INT(3, tokens.items[4].line);
    TEST_ASSERT_EQUAL_INT(9, tokens.items[4].column);
    TEST_ASSERT_EQUAL(TOK_PONTO_VIRGULA, tokens.items[5].type);
    TEST_ASSERT_EQUAL_INT(3, tokens.items[5].line);
    TEST_ASSERT_EQUAL_INT(10, tokens.items[5].column);

    token_list_free(&tokens);
}

void test_lexer_reports_invalid_character_with_line_information(void) {
    const char *source = "@";
    TokenList tokens;
    CompilerError error = {0};
    bool ok;

    token_list_init(&tokens);
    ok = lexer_scan(source, &tokens, &error);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_LEXER, error.phase);
    TEST_ASSERT_EQUAL_INT(1, error.line);
    TEST_ASSERT_EQUAL_INT(1, error.column);

    token_list_free(&tokens);
}

void test_lexer_accepts_zero_initialized_token_list(void) {
    TokenList tokens = {0};
    CompilerError error = {0};

    TEST_ASSERT_TRUE(lexer_scan("fim", &tokens, &error));
    TEST_ASSERT_EQUAL_size_t(2, tokens.count);
    assert_token(&tokens, 0, TOK_FIM, "fim", 1, 1);
    assert_token(&tokens, 1, TOK_EOF, "", 1, 4);

    token_list_free(&tokens);
}

void test_lexer_resets_initialized_output_tokens_before_each_scan(void) {
    TokenList tokens;
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan("programa demo inicio x <- 42; fim", &tokens, &error));
    TEST_ASSERT_TRUE(lexer_scan("fim", &tokens, &error));

    TEST_ASSERT_EQUAL_size_t(2, tokens.count);
    assert_token(&tokens, 0, TOK_FIM, "fim", 1, 1);
    assert_token(&tokens, 1, TOK_EOF, "", 1, 4);

    token_list_free(&tokens);
}

void test_lexer_scans_leia_statement(void) {
    const char *source = "leia valor;";
    TokenList tokens;
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));
    TEST_ASSERT_EQUAL_size_t(4, tokens.count);
    assert_token(&tokens, 0, TOK_LEIA, "leia", 1, 1);
    assert_token(&tokens, 1, TOK_ID, "valor", 1, 6);
    assert_token(&tokens, 2, TOK_PONTO_VIRGULA, ";", 1, 11);
    assert_token(&tokens, 3, TOK_EOF, "", 1, 12);

    token_list_free(&tokens);
}

void test_lexer_scans_procedure_keywords_and_float_literals(void) {
    const char *source = "procedimento flutuante soma(flutuante x) inicio retorna 3.14; fim";
    TokenList tokens;
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));

    assert_token(&tokens, 0, TOK_PROCEDIMENTO, "procedimento", 1, 1);
    assert_token(&tokens, 1, TOK_FLUTUANTE, "flutuante", 1, 14);
    assert_token(&tokens, 2, TOK_ID, "soma", 1, 24);
    assert_token(&tokens, 3, TOK_ABRE_PAR, "(", 1, 28);
    assert_token(&tokens, 4, TOK_FLUTUANTE, "flutuante", 1, 29);
    assert_token(&tokens, 5, TOK_ID, "x", 1, 39);
    assert_token(&tokens, 6, TOK_FECHA_PAR, ")", 1, 40);
    assert_token(&tokens, 7, TOK_INICIO, "inicio", 1, 42);
    assert_token(&tokens, 8, TOK_RETORNA, "retorna", 1, 49);
    assert_token(&tokens, 9, TOK_NUM_FLOAT, "3.14", 1, 57);
    assert_token(&tokens, 10, TOK_PONTO_VIRGULA, ";", 1, 61);
    assert_token(&tokens, 11, TOK_FIM, "fim", 1, 63);
    assert_token(&tokens, 12, TOK_EOF, "", 1, 66);

    token_list_free(&tokens);
}

void test_lexer_scans_vazio_keyword(void) {
    const char *source = "vazio";
    TokenList tokens;
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));
    TEST_ASSERT_EQUAL_size_t(2, tokens.count);
    assert_token(&tokens, 0, TOK_VAZIO, "vazio", 1, 1);
    assert_token(&tokens, 1, TOK_EOF, "", 1, 6);

    token_list_free(&tokens);
}

void test_lexer_scans_string_keyword_brackets_and_literal(void) {
    const char *source = "string nome[32]; escreval \"oi\";";
    TokenList tokens = {0};
    CompilerError error = {0};

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));
    TEST_ASSERT_EQUAL_size_t(10, tokens.count);
    assert_token(&tokens, 0, TOK_STRING, "string", 1, 1);
    assert_token(&tokens, 2, TOK_ABRE_COL, "[", 1, 12);
    assert_token(&tokens, 3, TOK_NUM_INT, "32", 1, 13);
    assert_token(&tokens, 4, TOK_FECHA_COL, "]", 1, 15);
    assert_token(&tokens, 6, TOK_ESCREVAL, "escreval", 1, 18);
    assert_token(&tokens, 7, TOK_STRING_LITERAL, "\"oi\"", 1, 27);
    assert_token(&tokens, 9, TOK_EOF, "", 1, 32);
    token_list_free(&tokens);
}

void test_lexer_reports_unterminated_string_literal_error(void) {
    const char *source = "\"hello";
    TokenList tokens;
    CompilerError error = {0};
    bool ok;

    token_list_init(&tokens);
    ok = lexer_scan(source, &tokens, &error);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_LEXER, error.phase);
    TEST_ASSERT_EQUAL_INT(1, error.line);
    TEST_ASSERT_EQUAL_INT(1, error.column);
    TEST_ASSERT_EQUAL_STRING("Literal de string nao terminado.", error.message);

    token_list_free(&tokens);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lexer_scans_program_structure_into_expected_tokens);
    RUN_TEST(test_lexer_scans_control_flow_keywords_and_relational_operators);
    RUN_TEST(test_lexer_tracks_line_and_column_across_multiple_lines);
    RUN_TEST(test_lexer_reports_invalid_character_with_line_information);
    RUN_TEST(test_lexer_accepts_zero_initialized_token_list);
    RUN_TEST(test_lexer_resets_initialized_output_tokens_before_each_scan);
    RUN_TEST(test_lexer_scans_leia_statement);
    RUN_TEST(test_lexer_scans_procedure_keywords_and_float_literals);
    RUN_TEST(test_lexer_scans_vazio_keyword);
    RUN_TEST(test_lexer_scans_string_keyword_brackets_and_literal);
    RUN_TEST(test_lexer_reports_unterminated_string_literal_error);
    return UNITY_END();
}
