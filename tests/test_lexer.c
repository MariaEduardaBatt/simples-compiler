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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lexer_scans_program_structure_into_expected_tokens);
    RUN_TEST(test_lexer_tracks_line_and_column_across_multiple_lines);
    RUN_TEST(test_lexer_reports_invalid_character_with_line_information);
    RUN_TEST(test_lexer_accepts_zero_initialized_token_list);
    RUN_TEST(test_lexer_resets_initialized_output_tokens_before_each_scan);
    return UNITY_END();
}
