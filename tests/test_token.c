#include "unity.h"
#include "token.h"

void setUp(void) {}

void tearDown(void) {}

void test_token_type_name_returns_keyword_name(void) {
    TEST_ASSERT_EQUAL_STRING("TOK_PROGRAMA", token_type_name(TOK_PROGRAMA));
}

void test_token_type_name_returns_control_flow_name(void) {
    TEST_ASSERT_EQUAL_STRING("TOK_SE", token_type_name(TOK_SE));
    TEST_ASSERT_EQUAL_STRING("TOK_FIMPARA", token_type_name(TOK_FIMPARA));
}

void test_token_type_name_returns_leia_name(void) {
    TEST_ASSERT_EQUAL_STRING("TOK_LEIA", token_type_name(TOK_LEIA));
}

void test_token_list_grows_and_preserves_all_entries_after_growth(void) {
    TokenList list;
    char lexemes[][16] = {"alpha", "beta", "gamma", "delta", "epsilon"};
    const char *expected_lexemes[] = {"alpha", "beta", "gamma", "delta", "epsilon"};
    TokenType expected_types[] = {TOK_ID, TOK_NUM_INT, TOK_MAIS, TOK_MENOS, TOK_EOF};
    size_t initial_capacity;
    size_t index;

    token_list_init(&list);

    for (index = 0; index < 4; ++index) {
        token_list_push(&list, (Token){.type = expected_types[index], .lexeme = lexemes[index], .line = (int)index + 1, .column = (int)index + 10});
        lexemes[index][0] = 'X';
    }

    initial_capacity = list.capacity;
    token_list_push(&list, (Token){.type = expected_types[4], .lexeme = lexemes[4], .line = 5, .column = 14});
    lexemes[4][0] = 'X';

    TEST_ASSERT_EQUAL_size_t(5, list.count);
    TEST_ASSERT_TRUE(list.capacity > initial_capacity);

    for (index = 0; index < list.count; ++index) {
        TEST_ASSERT_EQUAL(expected_types[index], list.items[index].type);
        TEST_ASSERT_EQUAL_STRING(expected_lexemes[index], list.items[index].lexeme);
        TEST_ASSERT_EQUAL_INT((int)index + 1, list.items[index].line);
        TEST_ASSERT_EQUAL_INT((int)index + 10, list.items[index].column);
    }

    token_list_free(&list);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_token_type_name_returns_keyword_name);
    RUN_TEST(test_token_type_name_returns_control_flow_name);
    RUN_TEST(test_token_type_name_returns_leia_name);
    RUN_TEST(test_token_list_grows_and_preserves_all_entries_after_growth);
    return UNITY_END();
}
