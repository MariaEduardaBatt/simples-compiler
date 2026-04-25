#include "unity.h"
#include "ast.h"
#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void assert_contains(const char *assembly, const char *fragment) {
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(assembly, fragment), fragment);
}

static void assert_contains_in_order(const char *assembly, const char *first, const char *second) {
    char *first_pos = strstr(assembly, first);
    char *second_pos;

    TEST_ASSERT_NOT_NULL_MESSAGE(first_pos, first);
    second_pos = strstr(first_pos, second);
    TEST_ASSERT_NOT_NULL_MESSAGE(second_pos, second);
}

static char *generate_source(const char *source) {
    CompilerError error = {0};
    TokenList tokens;
    ASTProgram *program = NULL;
    SymbolTable symbols = {0};
    char *assembly = NULL;

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NOT_NULL(program);
    TEST_ASSERT_TRUE(analyze_program(program, &symbols, &error));

    assembly = codegen_generate_program(program, &symbols);
    TEST_ASSERT_NOT_NULL(assembly);

    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
    return assembly;
}

void test_codegen_emits_direct_store_for_integer_assignment(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio x <- 10; fim");

    assert_contains(assembly, "mov dword [x], 10");

    free(assembly);
}

void test_codegen_emits_print_helper_calls_for_escreval(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio x <- 2 + 3; escreval x; fim");

    assert_contains(assembly, "call print_int");
    assert_contains(assembly, "call print_newline");

    free(assembly);
}

void test_codegen_emits_idiv_for_integer_division(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio x <- (20 div 5); fim");

    assert_contains(assembly, "pop eax\n    cdq\n    idiv ebx\n    mov dword [x], eax");

    free(assembly);
}

void test_codegen_emits_program_sections_and_helper_bodies_in_stable_order(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio escreval 1; fim");

    assert_contains_in_order(assembly, "global _start\n\nsection .data\n", "x dd 0\n");
    assert_contains_in_order(
        assembly, "x dd 0\n", "newline db 10\nprint_buffer times 12 db 0\n\nsection .text\n_start:\n");
    assert_contains_in_order(assembly, "section .text\n_start:\n", "    mov eax, 1\n    xor ebx, ebx\n    int 0x80\n");
    assert_contains_in_order(assembly, "    int 0x80\n", "\nprint_int:\n");
    assert_contains_in_order(assembly, "\nprint_int:\n", "\nprint_newline:\n");
    assert_contains(assembly, "mov edi, print_buffer + 12");
    assert_contains(assembly, "mov ecx, newline");

    free(assembly);
}

void test_codegen_emits_identifier_loads_and_plain_escreva_without_newline(void) {
    char *assembly = generate_source("programa demo inteiro x, y; inicio x <- 10; y <- x; escreva y; fim");

    assert_contains(assembly, "mov eax, dword [x]\n    mov dword [y], eax");
    assert_contains(assembly, "mov eax, dword [y]\n    call print_int");
    TEST_ASSERT_NULL(strstr(assembly, "call print_newline"));

    free(assembly);
}

void test_codegen_emits_add_sub_and_mul_instructions(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio x <- (1 + 2); x <- (x - 3); x <- (x * 4); fim");

    assert_contains(assembly, "add eax, ebx");
    assert_contains(assembly, "sub eax, ebx");
    assert_contains(assembly, "imul eax, ebx");

    free(assembly);
}

void test_codegen_emits_unary_negation_and_logical_not_sequences(void) {
    char *assembly = generate_source("programa demo inteiro x, y; inicio x <- -1; y <- nao x; fim");

    assert_contains(assembly, "mov eax, 1\n    neg eax\n    mov dword [x], eax");
    assert_contains(
        assembly,
        "mov eax, dword [x]\n    cmp eax, 0\n    sete al\n    movzx eax, al\n    mov dword [y], eax");

    free(assembly);
}

void test_codegen_uses_program_declarations_when_symbol_names_are_missing(void) {
    char name[] = "x";
    ASTDeclaration declaration = {.name = name, .line = 1, .column = 1};
    ASTProgram program = {.name = "demo", .declarations = &declaration, .declaration_count = 1};
    SymbolTable symbols = {.names = NULL, .count = 1};
    char *assembly = codegen_generate_program(&program, &symbols);

    TEST_ASSERT_NOT_NULL(assembly);
    assert_contains(assembly, "x dd 0");

    free(assembly);
}

void test_codegen_returns_null_when_symbol_fallback_has_no_declarations(void) {
    ASTProgram program = {.name = "demo", .declarations = NULL, .declaration_count = 1};
    SymbolTable symbols = {.names = NULL, .count = 1};

    TEST_ASSERT_NULL(codegen_generate_program(&program, &symbols));
}

void test_codegen_emits_labels_and_jump_for_if_else(void) {
    char *assembly =
        generate_source("programa demo inteiro x; inicio x <- 1; se x > 0 entao escreva x; senao escreval 0; fimse fim");

    assert_contains(assembly, "cmp eax, 0");
    assert_contains(assembly, "je .Lelse0");
    assert_contains(assembly, "jmp .Lendif0");

    free(assembly);
}

void test_codegen_emits_label_and_jump_for_if_without_else(void) {
    char *assembly =
        generate_source("programa demo inteiro x; inicio x <- 1; se x > 0 entao escreva x; fimse fim");

    assert_contains(assembly, "cmp eax, 0");
    assert_contains(assembly, "je .Lendif0");
    TEST_ASSERT_NULL(strstr(assembly, ".Lelse0"));

    free(assembly);
}

void test_codegen_emits_loop_labels_for_enquanto(void) {
    char *assembly =
        generate_source("programa demo inteiro x; inicio enquanto x < 3 faca x <- x + 1; fimenquanto fim");

    assert_contains(assembly, ".Lwhile0:");
    assert_contains(assembly, "je .Lendwhile0");
    assert_contains(assembly, ".Lendwhile0:");
    assert_contains(assembly, "jmp .Lwhile0");

    free(assembly);
}

void test_codegen_materializes_for_bounds_and_step_once(void) {
    char *assembly =
        generate_source("programa demo inteiro i, total; inicio para i de 1 ate 3 passo 1 faca total <- total + i; fimpara fim");

    assert_contains(assembly, "_for_end_0 dd 0");
    assert_contains(assembly, "_for_step_0 dd 0");
    assert_contains(assembly, ".Lfor0:");
    assert_contains(assembly, ".Lendfor0:");

    free(assembly);
}

void test_codegen_skips_for_body_when_step_is_zero(void) {
    char *assembly =
        generate_source("programa demo inteiro i; inicio para i de 1 ate 5 passo 0 faca escreva i; fimpara fim");

    assert_contains(assembly, "    cmp eax, 0\n    je .Lendfor0");

    free(assembly);
}

void test_codegen_uses_negative_step_bound_check_for_for_loop(void) {
    char *assembly =
        generate_source("programa demo inteiro i; inicio para i de 5 ate 1 passo -1 faca escreva i; fimpara fim");

    assert_contains(assembly, "    jl .Lendfor0");

    free(assembly);
}

void test_codegen_escapes_user_identifiers_that_collide_with_for_temporaries(void) {
    char *assembly =
        generate_source("programa demo inteiro i, _for_end_0, _for_step_0; inicio _for_end_0 <- 3; _for_step_0 <- 1; para i de 1 ate _for_end_0 passo _for_step_0 faca escreva _for_end_0; fimpara fim");

    assert_contains(assembly, "$_for_end_0 dd 0");
    assert_contains(assembly, "$_for_step_0 dd 0");
    assert_contains(assembly, "_for_end_0 dd 0");
    assert_contains(assembly, "_for_step_0 dd 0");
    assert_contains(assembly, "mov dword [$_for_end_0], 3");
    assert_contains(assembly, "mov dword [$_for_step_0], 1");
    assert_contains(assembly, "mov eax, dword [$_for_end_0]\n    mov dword [_for_end_0], eax");
    assert_contains(assembly, "mov eax, dword [$_for_step_0]\n    mov dword [_for_step_0], eax");
    assert_contains(assembly, "mov eax, dword [$_for_end_0]\n    call print_int");

    free(assembly);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_codegen_emits_direct_store_for_integer_assignment);
    RUN_TEST(test_codegen_emits_print_helper_calls_for_escreval);
    RUN_TEST(test_codegen_emits_idiv_for_integer_division);
    RUN_TEST(test_codegen_emits_program_sections_and_helper_bodies_in_stable_order);
    RUN_TEST(test_codegen_emits_identifier_loads_and_plain_escreva_without_newline);
    RUN_TEST(test_codegen_emits_add_sub_and_mul_instructions);
    RUN_TEST(test_codegen_emits_unary_negation_and_logical_not_sequences);
    RUN_TEST(test_codegen_uses_program_declarations_when_symbol_names_are_missing);
    RUN_TEST(test_codegen_returns_null_when_symbol_fallback_has_no_declarations);
    RUN_TEST(test_codegen_emits_labels_and_jump_for_if_else);
    RUN_TEST(test_codegen_emits_label_and_jump_for_if_without_else);
    RUN_TEST(test_codegen_emits_loop_labels_for_enquanto);
    RUN_TEST(test_codegen_materializes_for_bounds_and_step_once);
    RUN_TEST(test_codegen_skips_for_body_when_step_is_zero);
    RUN_TEST(test_codegen_uses_negative_step_bound_check_for_for_loop);
    RUN_TEST(test_codegen_escapes_user_identifiers_that_collide_with_for_temporaries);
    return UNITY_END();
}
