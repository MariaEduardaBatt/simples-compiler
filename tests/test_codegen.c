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

bool codegen_debug_resolve_procedure_for_loop_offsets(
    size_t proc_local_frame_bytes,
    const size_t *procedure_for_loop_ids,
    size_t procedure_for_loop_id_count,
    size_t label_id,
    size_t *end_offset,
    size_t *step_offset,
    CompilerError *error,
    int line,
    int column);

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
    SemanticInfo info = {0};
    char *assembly = NULL;

    token_list_init(&tokens);
    TEST_ASSERT_TRUE(lexer_scan(source, &tokens, &error));
    TEST_ASSERT_TRUE(parse_program(&tokens, &program, &error));
    TEST_ASSERT_NOT_NULL(program);
    TEST_ASSERT_TRUE(analyze_program(program, &info, &error));

    TEST_ASSERT_TRUE(codegen_generate_program(program, &info, &assembly, &error));
    TEST_ASSERT_NOT_NULL(assembly);

    semantic_info_free(&info);
    ast_program_free(program);
    token_list_free(&tokens);
    return assembly;
}

static char *generate_source_with_error(const char *source, CompilerError *out_error) {
    CompilerError error = {0};
    TokenList tokens;
    ASTProgram *program = NULL;
    SemanticInfo info = {0};
    char *assembly = NULL;

    token_list_init(&tokens);
    if (!lexer_scan(source, &tokens, &error)) {
        if (out_error != NULL) { *out_error = error; }
        token_list_free(&tokens);
        return NULL;
    }
    if (!parse_program(&tokens, &program, &error)) {
        if (out_error != NULL) { *out_error = error; }
        ast_program_free(program);
        token_list_free(&tokens);
        return NULL;
    }
    if (!analyze_program(program, &info, &error)) {
        if (out_error != NULL) { *out_error = error; }
        semantic_info_free(&info);
        ast_program_free(program);
        token_list_free(&tokens);
        return NULL;
    }

    if (!codegen_generate_program(program, &info, &assembly, &error)) {
        if (out_error != NULL) { *out_error = error; }
        assembly = NULL;
    }

    semantic_info_free(&info);
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
        assembly, "x dd 0\n", "newline db 10\nprint_buffer times 12 db 0\nread_buffer times 16 db 0\n\nsection .text\n_start:\n");
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
    SemanticInfo semantic = {.globals = NULL, .global_count = 1};
    char *assembly = NULL;
    CompilerError error = {0};

    TEST_ASSERT_TRUE(codegen_generate_program(&program, &semantic, &assembly, &error));
    TEST_ASSERT_NOT_NULL(assembly);
    assert_contains(assembly, "x dd 0");

    free(assembly);
}

void test_codegen_returns_null_when_symbol_fallback_has_no_declarations(void) {
    ASTProgram program = {.name = "demo", .declarations = NULL, .declaration_count = 1};
    SemanticInfo semantic = {.globals = NULL, .global_count = 1};
    char *assembly = NULL;
    CompilerError error = {0};

    TEST_ASSERT_FALSE(codegen_generate_program(&program, &semantic, &assembly, &error));
    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Internal error: code generation failed.", error.message);
}

void test_codegen_reports_internal_error_for_return_outside_procedure(void) {
    ASTCommand command = {0};
    ASTProgram program = {.name = "demo", .commands = &command, .command_count = 1};
    SemanticInfo semantic = {0};
    char *assembly = NULL;
    CompilerError error = {0};

    command.type = AST_COMMAND_RETURN;
    command.return_command.line = 12;
    command.return_command.column = 34;

    TEST_ASSERT_FALSE(codegen_generate_program(&program, &semantic, &assembly, &error));
    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Internal error: return command outside a procedure.", error.message);
    TEST_ASSERT_EQUAL(12, error.line);
    TEST_ASSERT_EQUAL(34, error.column);
}

void test_codegen_reports_explicit_error_when_procedure_for_loop_slots_are_missing(void) {
    CompilerError error = {0};
    size_t end_offset = 123;
    size_t step_offset = 456;

    TEST_ASSERT_FALSE(codegen_debug_resolve_procedure_for_loop_offsets(
        0, NULL, 0, 7, &end_offset, &step_offset, &error, 9, 11));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Internal error: missing procedure for-loop temporary slot metadata.", error.message);
    TEST_ASSERT_EQUAL(9, error.line);
    TEST_ASSERT_EQUAL(11, error.column);
    TEST_ASSERT_EQUAL_size_t(123, end_offset);
    TEST_ASSERT_EQUAL_size_t(456, step_offset);
}

void test_codegen_reports_explicit_error_when_procedure_for_loop_slot_layout_overflows(void) {
    CompilerError error = {0};
    size_t loop_ids[] = {3};
    size_t end_offset = 0;
    size_t step_offset = 0;

    /* With the byte-based formula end_offset = proc_local_bytes + (index*2+1)*4,
       pass SIZE_MAX-3 so that SIZE_MAX-3+4 wraps to 0 and triggers the overflow guard. */
    TEST_ASSERT_FALSE(codegen_debug_resolve_procedure_for_loop_offsets(
        (size_t)-4, loop_ids, 1, 3, &end_offset, &step_offset, &error, 4, 8));
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Internal error: invalid procedure-for-loop temporary slot layout.", error.message);
    TEST_ASSERT_EQUAL(4, error.line);
    TEST_ASSERT_EQUAL(8, error.column);
}

void test_codegen_reports_float_expression_errors_in_main_body(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error("programa demo inicio escreval 1.5; fim", &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante expressions is not supported yet.", error.message);
}

void test_codegen_rejects_float_read_in_main_body_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error("programa demo flutuante x; inicio leia x; fim", &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante values in the main program is not supported yet.", error.message);
}

void test_codegen_rejects_float_assignment_via_identifier_in_main_body_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error("programa demo flutuante x, y; inicio x <- y; fim", &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante values in the main program is not supported yet.", error.message);
}

void test_codegen_rejects_float_write_via_identifier_in_main_body_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error("programa demo flutuante x; inicio escreval x; fim", &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante values in the main program is not supported yet.", error.message);
}

void test_codegen_sets_fallback_error_when_generation_fails_without_specific_diagnostic(void) {
    ASTCommand command = {.type = (ASTCommandType)999};
    ASTProgram program = {.name = "demo", .commands = &command, .command_count = 1};
    SemanticInfo semantic = {0};
    char *assembly = NULL;
    CompilerError error = {0};

    TEST_ASSERT_FALSE(codegen_generate_program(&program, &semantic, &assembly, &error));
    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Internal error: unsupported command type in main backend check.", error.message);
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

    assert_contains_in_order(assembly, "    cmp eax, 0\n    je .Lendfor0\n", ".Lforbody0:\n");

    free(assembly);
}

void test_codegen_uses_negative_step_bound_check_for_for_loop(void) {
    char *assembly =
        generate_source("programa demo inteiro i; inicio para i de 5 ate 1 passo -1 faca escreva i; fimpara fim");

    assert_contains_in_order(assembly, "    jg .Lforpos0\n", "    jl .Lendfor0\n");
    assert_contains_in_order(assembly, "    jl .Lendfor0\n", ".Lforbody0:\n");

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

void test_codegen_emits_read_call_and_store_for_leia(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio leia x; fim");

    assert_contains(assembly, "call read_int");
    assert_contains(assembly, "mov dword [x], eax");

    free(assembly);
}

void test_codegen_emits_read_buffer_and_helper_body(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio leia x; escreval x; fim");

    assert_contains(assembly, "read_buffer times 16 db 0");
    assert_contains(assembly, "\nread_int:\n");
    assert_contains(assembly, "mov eax, 3");
    assert_contains(assembly, "mov ebx, 0");

    free(assembly);
}

void test_codegen_read_int_validates_digit_characters(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio leia x; fim");

    assert_contains_in_order(assembly, "    cmp al, '0'\n    jb .read_int_done\n", "    cmp al, '9'\n    ja .read_int_done\n");
    assert_contains_in_order(assembly, "    cmp al, '9'\n    ja .read_int_done\n", "    sub al, '0'\n");

    free(assembly);
}

void test_codegen_read_int_bounds_loop_to_bytes_read(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio leia x; fim");

    assert_contains(assembly, "    lea edx, [read_buffer + eax]\n");
    assert_contains_in_order(assembly, ".read_int_digits:\n", "    cmp ecx, edx\n    jae .read_int_done\n");

    free(assembly);
}

void test_codegen_read_int_clamps_on_overflow(void) {
    char *assembly = generate_source("programa demo inteiro x; inicio leia x; fim");

    /* threshold check: accumulator vs 214748364 before multiply */
    assert_contains_in_order(assembly,
        "    sub al, '0'\n",
        "    cmp edi, 214748364\n"
        "    jg .read_int_overflow\n"
        "    jl .read_int_no_overflow\n");

    /* digit-level threshold: positive limit = 7, negative limit = 8 */
    assert_contains_in_order(assembly,
        "    jl .read_int_no_overflow\n",
        "    cmp esi, 0\n"
        "    je .read_int_check_pos_digit\n"
        "    cmp al, 8\n"
        "    jg .read_int_overflow\n"
        "    jmp .read_int_no_overflow\n"
        ".read_int_check_pos_digit:\n"
        "    cmp al, 7\n"
        "    jg .read_int_overflow\n");

    /* post-accumulation signed-overflow guard: catches 0x80000000 wrap */
    assert_contains(assembly,
        ".read_int_no_overflow:\n"
        "    imul edi, edi, 10\n"
        "    add edi, eax\n"
        "    js .read_int_limit_hit\n"
        "    inc ecx\n"
        "    jmp .read_int_digits\n"
        ".read_int_limit_hit:\n"
        "    inc ecx\n");

    /* overflow drain loop: consume remaining digits then clamp */
    assert_contains(assembly,
        ".read_int_limit_hit:\n"
        "    inc ecx\n"
        ".read_int_overflow:\n"
        "    cmp ecx, edx\n"
        "    jae .read_int_overflow_ret\n"
        "    movzx eax, byte [ecx]\n"
        "    cmp al, '0'\n"
        "    jb .read_int_overflow_ret\n"
        "    cmp al, '9'\n"
        "    ja .read_int_overflow_ret\n"
        "    inc ecx\n"
        "    jmp .read_int_overflow\n"
        ".read_int_overflow_ret:\n"
        "    cmp esi, 0\n"
        "    je .read_int_clamp_pos\n"
        "    mov eax, 0x80000000\n"
        "    ret\n"
        ".read_int_clamp_pos:\n"
        "    mov eax, 0x7fffffff\n"
        "    ret\n");

    free(assembly);
}

void test_codegen_emits_call_and_stack_cleanup_for_integer_procedure(void) {
    char *assembly = generate_source(
        "procedimento inteiro soma(inteiro a, inteiro b)\n"
        "inicio\n"
        "  retorna a + b;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- soma(1, 2);\n"
        "fim");

    assert_contains(assembly, "proc_soma:");
    assert_contains(assembly, "push ebp\n    mov ebp, esp");
    assert_contains(assembly, "mov eax, dword [ebp+8]");
    assert_contains(assembly, "mov eax, dword [ebp+12]");
    assert_contains(assembly, "push 2\n    push 1\n    call proc_soma\n    add esp, 8");
    assert_contains(assembly, "mov esp, ebp\n    pop ebp\n    ret");
    assert_contains(assembly, "mov dword [x], eax");
    free(assembly);
}

void test_codegen_rejects_float_procedure_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error(
        "procedimento flutuante soma(flutuante a, flutuante b)\n"
        "inicio\n"
        "  retorna a;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  escreval 1;\n"
        "fim",
        &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante procedures is not supported yet.", error.message);
}

void test_codegen_rejects_float_local_in_procedure_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error(
        "procedimento vazio usa()\n"
        "inicio\n"
        "flutuante x;\n"
        "  x <- 1.5;\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim",
        &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Code generation for flutuante procedures is not supported yet.", error.message);
}

void test_codegen_emits_void_procedure_return_without_expression(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  escreval 1;\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    assert_contains(assembly, "proc_usa:");
    assert_contains(assembly, "call proc_usa");
    assert_contains(assembly, "jmp .proc_usa_epilogue");
    TEST_ASSERT_NULL(strstr(assembly, "add esp, 0"));
    free(assembly);
}

void test_codegen_uses_frame_local_for_loop_temporaries_for_procedure_body(void) {
    char *assembly = generate_source(
        "procedimento inteiro soma(inteiro a, inteiro b)\n"
        "inicio\n"
        "  inteiro i;\n"
        "  para i de 0 ate 2 passo 1 faca escreva i; fimpara\n"
        "  retorna a + b;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- soma(1, 2);\n"
        "fim");

    assert_contains(assembly, "sub esp, 12");
    assert_contains(assembly, "mov dword [ebp-8], eax");
    assert_contains(assembly, "mov dword [ebp-12], eax");
    assert_contains(assembly, "mov eax, dword [ebp-12]");
    assert_contains(assembly, "mov ebx, dword [ebp-8]");
    TEST_ASSERT_NULL(strstr(assembly, "_for_end_0 dd 0"));
    TEST_ASSERT_NULL(strstr(assembly, "_for_step_0 dd 0"));
    assert_contains(assembly, ".Lfor0:");
    assert_contains(assembly, ".Lendfor0:");
    free(assembly);
}

void test_codegen_zero_initializes_procedure_locals_on_entry(void) {
    char *assembly = generate_source(
        "procedimento inteiro primeiro()\n"
        "inicio\n"
        "  inteiro x, y;\n"
        "  retorna x + y;\n"
        "fim\n"
        "programa demo\n"
        "inteiro z;\n"
        "inicio\n"
        "  z <- primeiro();\n"
        "fim");

    assert_contains_in_order(assembly, "push ebp\n    mov ebp, esp\n    sub esp, 8\n", "    mov dword [ebp-4], 0\n");
    assert_contains_in_order(assembly, "    mov dword [ebp-4], 0\n", "    mov dword [ebp-8], 0\n");
    free(assembly);
}

void test_codegen_assigns_distinct_frame_slots_for_multiple_procedure_for_loops(void) {
    char *assembly = generate_source(
        "procedimento inteiro soma(inteiro n)\n"
        "inicio\n"
        "  inteiro i, total;\n"
        "  total <- 0;\n"
        "  para i de 1 ate n passo 1 faca total <- total + i; fimpara\n"
        "  para i de n ate 1 passo -1 faca total <- total + i; fimpara\n"
        "  retorna total;\n"
        "fim\n"
        "programa demo\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- soma(3);\n"
        "fim");

    assert_contains(assembly, "sub esp, 24");
    assert_contains(assembly, "mov dword [ebp-12], eax");
    assert_contains(assembly, "mov dword [ebp-16], eax");
    assert_contains(assembly, "mov dword [ebp-20], eax");
    assert_contains(assembly, "mov dword [ebp-24], eax");
    assert_contains(assembly, "mov eax, dword [ebp-16]");
    assert_contains(assembly, "mov ebx, dword [ebp-12]");
    assert_contains(assembly, "mov eax, dword [ebp-24]");
    assert_contains(assembly, "mov ebx, dword [ebp-20]");
    free(assembly);
}

void test_codegen_rejects_indexed_assignment_target_with_internal_error(void) {
    char name[] = "nums";
    ASTExpression index_expr = {.type = AST_EXPR_INT, .line = 1, .column = 8, .int_value = 0};
    ASTCommand command = {0};
    ASTProgram program = {.name = "demo", .commands = &command, .command_count = 1};
    SemanticInfo semantic = {0};
    char *assembly = NULL;
    CompilerError error = {0};

    command.type = AST_COMMAND_ASSIGNMENT;
    command.assignment.target.type = AST_TARGET_INDEXED;
    command.assignment.target.line = 1;
    command.assignment.target.column = 3;
    command.assignment.target.indexed.name = name;
    command.assignment.target.indexed.index = &index_expr;
    command.assignment.expression = NULL; /* NULL expression forces generation failure */

    TEST_ASSERT_FALSE(codegen_generate_program(&program, &semantic, &assembly, &error));
    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING("Internal error: code generation failed.", error.message);
    TEST_ASSERT_EQUAL(0, error.line);
    TEST_ASSERT_EQUAL(0, error.column);
}

void test_codegen_emits_scaled_addressing_for_global_integer_vector_element(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "inteiro nums[4], x;\n"
        "inicio\n"
        "  nums[1] <- 42;\n"
        "  x <- nums[1];\n"
        "fim");

    assert_contains(assembly, "imul eax, 4");
    assert_contains(assembly, "lea edx, [nums + eax]");
    free(assembly);
}

void test_codegen_zero_initializes_local_integer_vector_storage(void) {
    char *assembly = generate_source(
        "procedimento vazio limpa()\n"
        "inicio\n"
        "  inteiro nums[3];\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  limpa();\n"
        "fim");

    assert_contains(assembly, "sub esp, 12");
    assert_contains(assembly, "mov dword [ebp-4], 0");
    assert_contains(assembly, "mov dword [ebp-8], 0");
    assert_contains(assembly, "mov dword [ebp-12], 0");
    free(assembly);
}

void test_codegen_emits_local_integer_vector_read_and_write(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  inteiro nums[3];\n"
        "  nums[0] <- 42;\n"
        "  escreva nums[1];\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    assert_contains(assembly, "neg eax");
    assert_contains(assembly, "lea edx, [ebp + eax - 4]");
    assert_contains(assembly, "mov dword [edx], eax");
    assert_contains(assembly, "mov eax, dword [edx]");
    free(assembly);
}

void test_codegen_procedure_local_vector_and_para_temporaries_do_not_overlap(void) {
    char *assembly = generate_source(
        "procedimento vazio preenche()\n"
        "inicio\n"
        "  inteiro nums[3];\n"
        "  inteiro i;\n"
        "  para i de 0 ate 2 passo 1 faca\n"
        "    nums[i] <- i;\n"
        "  fimpara\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  preenche();\n"
        "fim");

    /* 3 vector slots + 1 scalar + 2 para temporaries = 6 slots = 24 bytes */
    assert_contains(assembly, "sub esp, 24");
    /* vector base offset is 4 (first local, 3 capacity slots) */
    assert_contains(assembly, "lea edx, [ebp + eax - 4]");
    /* for-loop end and step occupy slots 5 and 6 (offsets 20 and 24), not inside vector slots */
    assert_contains(assembly, "mov dword [ebp-20], eax");
    assert_contains(assembly, "mov dword [ebp-24], eax");
    free(assembly);
}

void test_codegen_emits_byte_load_for_string_indexed_read_in_main(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[10];\n"
        "inteiro x;\n"
        "inicio\n"
        "  x <- nome[0];\n"
        "fim");

    assert_contains(assembly, "lea edx, [nome + eax]");
    assert_contains(assembly, "movzx eax, byte [edx]");
    assert_contains(assembly, "mov dword [x], eax");
    free(assembly);
}

void test_codegen_emits_byte_store_for_string_indexed_write_in_main(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[10];\n"
        "inicio\n"
        "  nome[0] <- 65;\n"
        "fim");

    assert_contains(assembly, "lea edx, [nome + eax]");
    assert_contains(assembly, "mov byte [edx], al");
    free(assembly);
}

void test_codegen_copies_string_literal_into_fixed_buffer(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  nome <- \"ana\";\n"
        "fim");

    assert_contains(assembly, "mov byte [nome], 'a'");
    assert_contains(assembly, "mov byte [nome+1], 'n'");
    assert_contains(assembly, "mov byte [nome+2], 'a'");
    assert_contains(assembly, "mov byte [nome+3], 0");
    free(assembly);
}

void test_codegen_emits_string_write_loop_for_escreval(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  nome <- \"oi\";\n"
        "  escreval nome;\n"
        "fim");

    assert_contains(assembly, "call print_string");
    assert_contains(assembly, "call print_newline");
    free(assembly);
}

void test_codegen_emits_local_string_indexed_read_in_procedure(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[10];\n"
        "  inteiro x;\n"
        "  x <- nome[0];\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    assert_contains(assembly, "lea edx, [ebp + eax - 10]");
    assert_contains(assembly, "movzx eax, byte [edx]");
    free(assembly);
}

void test_codegen_local_string_frame_reserves_n_bytes_not_dwords(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[8];\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    /* string[8] must reserve 8 bytes, not 8*4=32 bytes */
    assert_contains(assembly, "sub esp, 8");
    TEST_ASSERT_NULL(strstr(assembly, "sub esp, 32"));
    free(assembly);
}

void test_codegen_local_string_literal_assignment_emits_frame_byte_stores(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[8];\n"
        "  nome <- \"hi\";\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    /* string[8] at base_offset=8: byte 0 at [ebp-8], byte 1 at [ebp-7], null at [ebp-6] */
    assert_contains(assembly, "mov byte [ebp-8], 'h'");
    assert_contains(assembly, "mov byte [ebp-7], 'i'");
    assert_contains(assembly, "mov byte [ebp-6], 0");
    free(assembly);
}

void test_codegen_string_literal_assignment_escapes_apostrophes_in_byte_stores(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  nome <- \"d'agua\";\n"
        "fim");

    assert_contains(assembly, "mov byte [nome], 'd'");
    assert_contains(assembly, "mov byte [nome+1], 39");
    assert_contains(assembly, "mov byte [nome+2], 'a'");
    free(assembly);
}

void test_codegen_local_string_read_uses_frame_address(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[8];\n"
        "  leia nome;\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    /* string[8] at base_offset=8: lea eax, [ebp-8] */
    assert_contains(assembly, "lea eax, [ebp-8]");
    assert_contains(assembly, "call read_string");
    free(assembly);
}

void test_codegen_local_string_write_uses_frame_address(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[8];\n"
        "  escreval nome;\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    /* string[8] at base_offset=8: lea eax, [ebp-8] */
    assert_contains(assembly, "lea eax, [ebp-8]");
    assert_contains(assembly, "call print_string");
    free(assembly);
}

void test_codegen_emits_string_literal_escreva_via_literal_label(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  escreva \"abc\";\n"
        "fim");

    assert_contains(assembly, "_strlit_0 db 'a', 'b', 'c', 0");
    assert_contains(assembly, "mov eax, _strlit_0");
    assert_contains(assembly, "call print_string");
    TEST_ASSERT_NULL(strstr(assembly, "call print_newline"));
    free(assembly);
}

void test_codegen_emits_string_literal_escreval_via_literal_label(void) {
    char *assembly = generate_source(
        "programa demo\n"
        "string nome[8];\n"
        "inicio\n"
        "  escreval \"abc\";\n"
        "fim");

    assert_contains(assembly, "_strlit_0 db 'a', 'b', 'c', 0");
    assert_contains(assembly, "mov eax, _strlit_0");
    assert_contains(assembly, "call print_string");
    assert_contains(assembly, "call print_newline");
    free(assembly);
}

void test_codegen_rejects_string_return_procedure_with_explicit_error(void) {
    CompilerError error = {0};
    char *assembly = generate_source_with_error(
        "procedimento string saudacao()\n"
        "inicio\n"
        "  retorna \"oi\";\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  escreval 1;\n"
        "fim",
        &error);

    TEST_ASSERT_NULL(assembly);
    TEST_ASSERT_EQUAL(COMPILER_PHASE_CODEGEN, error.phase);
    TEST_ASSERT_EQUAL_STRING(
        "Code generation for string procedure signatures is not supported yet.", error.message);
}

void test_codegen_local_string_and_scalar_have_non_overlapping_frame_offsets(void) {
    char *assembly = generate_source(
        "procedimento vazio usa()\n"
        "inicio\n"
        "  string nome[8];\n"
        "  inteiro x;\n"
        "  x <- 42;\n"
        "  retorna;\n"
        "fim\n"
        "programa demo\n"
        "inicio\n"
        "  usa();\n"
        "fim");

    /* string[8]: 8 bytes + scalar: 4 bytes = 12 total */
    assert_contains(assembly, "sub esp, 12");
    /* scalar x at base_offset=12: dword at [ebp-12] */
    assert_contains(assembly, "mov dword [ebp-12], 42");
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
    RUN_TEST(test_codegen_reports_internal_error_for_return_outside_procedure);
    RUN_TEST(test_codegen_reports_explicit_error_when_procedure_for_loop_slots_are_missing);
    RUN_TEST(test_codegen_reports_explicit_error_when_procedure_for_loop_slot_layout_overflows);
    RUN_TEST(test_codegen_reports_float_expression_errors_in_main_body);
    RUN_TEST(test_codegen_rejects_float_read_in_main_body_with_explicit_error);
    RUN_TEST(test_codegen_rejects_float_assignment_via_identifier_in_main_body_with_explicit_error);
    RUN_TEST(test_codegen_rejects_float_write_via_identifier_in_main_body_with_explicit_error);
    RUN_TEST(test_codegen_sets_fallback_error_when_generation_fails_without_specific_diagnostic);
    RUN_TEST(test_codegen_emits_labels_and_jump_for_if_else);
    RUN_TEST(test_codegen_emits_label_and_jump_for_if_without_else);
    RUN_TEST(test_codegen_emits_loop_labels_for_enquanto);
    RUN_TEST(test_codegen_materializes_for_bounds_and_step_once);
    RUN_TEST(test_codegen_skips_for_body_when_step_is_zero);
    RUN_TEST(test_codegen_uses_negative_step_bound_check_for_for_loop);
    RUN_TEST(test_codegen_escapes_user_identifiers_that_collide_with_for_temporaries);
    RUN_TEST(test_codegen_emits_read_call_and_store_for_leia);
    RUN_TEST(test_codegen_emits_read_buffer_and_helper_body);
    RUN_TEST(test_codegen_read_int_validates_digit_characters);
    RUN_TEST(test_codegen_read_int_bounds_loop_to_bytes_read);
    RUN_TEST(test_codegen_read_int_clamps_on_overflow);
    RUN_TEST(test_codegen_emits_call_and_stack_cleanup_for_integer_procedure);
    RUN_TEST(test_codegen_rejects_float_procedure_with_explicit_error);
    RUN_TEST(test_codegen_rejects_float_local_in_procedure_with_explicit_error);
    RUN_TEST(test_codegen_emits_void_procedure_return_without_expression);
    RUN_TEST(test_codegen_uses_frame_local_for_loop_temporaries_for_procedure_body);
    RUN_TEST(test_codegen_zero_initializes_procedure_locals_on_entry);
    RUN_TEST(test_codegen_assigns_distinct_frame_slots_for_multiple_procedure_for_loops);
    RUN_TEST(test_codegen_rejects_indexed_assignment_target_with_internal_error);
    RUN_TEST(test_codegen_emits_scaled_addressing_for_global_integer_vector_element);
    RUN_TEST(test_codegen_zero_initializes_local_integer_vector_storage);
    RUN_TEST(test_codegen_emits_local_integer_vector_read_and_write);
    RUN_TEST(test_codegen_procedure_local_vector_and_para_temporaries_do_not_overlap);
    RUN_TEST(test_codegen_emits_byte_load_for_string_indexed_read_in_main);
    RUN_TEST(test_codegen_emits_byte_store_for_string_indexed_write_in_main);
    RUN_TEST(test_codegen_emits_local_string_indexed_read_in_procedure);
    RUN_TEST(test_codegen_copies_string_literal_into_fixed_buffer);
    RUN_TEST(test_codegen_emits_string_write_loop_for_escreval);
    RUN_TEST(test_codegen_local_string_frame_reserves_n_bytes_not_dwords);
    RUN_TEST(test_codegen_local_string_literal_assignment_emits_frame_byte_stores);
    RUN_TEST(test_codegen_string_literal_assignment_escapes_apostrophes_in_byte_stores);
    RUN_TEST(test_codegen_local_string_read_uses_frame_address);
    RUN_TEST(test_codegen_local_string_write_uses_frame_address);
    RUN_TEST(test_codegen_local_string_and_scalar_have_non_overlapping_frame_offsets);
    RUN_TEST(test_codegen_emits_string_literal_escreva_via_literal_label);
    RUN_TEST(test_codegen_emits_string_literal_escreval_via_literal_label);
    RUN_TEST(test_codegen_rejects_string_return_procedure_with_explicit_error);
    return UNITY_END();
}
