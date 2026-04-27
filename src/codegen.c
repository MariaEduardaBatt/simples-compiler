#include "codegen.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} SizeList;

typedef struct {
    StringBuilder *builder;
    const SizeList *for_loop_ids;
    size_t next_label_id;
} CodegenContext;

static bool builder_reserve(StringBuilder *builder, size_t extra_length) {
    size_t required_length;
    size_t new_capacity;
    char *new_data;

    required_length = builder->length + extra_length + 1;
    if (required_length <= builder->capacity) {
        return true;
    }

    new_capacity = builder->capacity == 0 ? 256 : builder->capacity;
    while (new_capacity < required_length) {
        new_capacity *= 2;
    }

    new_data = realloc(builder->data, new_capacity);
    if (new_data == NULL) {
        return false;
    }

    builder->data = new_data;
    builder->capacity = new_capacity;
    return true;
}

static bool builder_append(StringBuilder *builder, const char *text) {
    size_t text_length;

    text_length = strlen(text);
    if (!builder_reserve(builder, text_length)) {
        return false;
    }

    memcpy(builder->data + builder->length, text, text_length + 1);
    builder->length += text_length;
    return true;
}

static bool builder_appendf(StringBuilder *builder, const char *format, ...) {
    va_list args;
    va_list args_copy;
    int written;

    va_start(args, format);
    va_copy(args_copy, args);
    written = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    if (written < 0) {
        va_end(args);
        return false;
    }

    if (!builder_reserve(builder, (size_t)written)) {
        va_end(args);
        return false;
    }

    vsnprintf(builder->data + builder->length, builder->capacity - builder->length, format, args);
    va_end(args);
    builder->length += (size_t)written;
    return true;
}

static bool size_list_append(SizeList *list, size_t value) {
    size_t *items;
    size_t new_capacity;

    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        items = realloc(list->items, new_capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }

        list->items = items;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = value;
    return true;
}

static bool for_loop_temp_name_equals(const SizeList *for_loop_ids, const char *prefix, const char *name) {
    char expected[64];
    size_t index;
    int written;

    if (for_loop_ids == NULL || name == NULL) {
        return false;
    }

    for (index = 0; index < for_loop_ids->count; ++index) {
        written = snprintf(expected, sizeof(expected), "%s%zu", prefix, for_loop_ids->items[index]);
        if (written < 0 || (size_t)written >= sizeof(expected)) {
            return false;
        }

        if (strcmp(expected, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool user_symbol_needs_escape(const SizeList *for_loop_ids, const char *name) {
    return for_loop_temp_name_equals(for_loop_ids, "_for_end_", name) ||
           for_loop_temp_name_equals(for_loop_ids, "_for_step_", name);
}

static bool builder_append_user_symbol_name(StringBuilder *builder, const SizeList *for_loop_ids, const char *name) {
    if (user_symbol_needs_escape(for_loop_ids, name)) {
        return builder_appendf(builder, "$%s", name);
    }

    return builder_append(builder, name);
}

static bool builder_append_user_symbol_declaration(StringBuilder *builder, const SizeList *for_loop_ids, const char *name) {
    return builder_append_user_symbol_name(builder, for_loop_ids, name) &&
           builder_append(builder, " dd 0\n");
}

static bool builder_append_load_identifier(StringBuilder *builder, const SizeList *for_loop_ids, const char *name) {
    return builder_append(builder, "    mov eax, dword [") &&
           builder_append_user_symbol_name(builder, for_loop_ids, name) &&
           builder_append(builder, "]\n");
}

static bool builder_append_store_eax_to_identifier(StringBuilder *builder, const SizeList *for_loop_ids, const char *name) {
    return builder_append(builder, "    mov dword [") &&
           builder_append_user_symbol_name(builder, for_loop_ids, name) &&
           builder_append(builder, "], eax\n");
}

static bool builder_append_store_int_to_identifier(
    StringBuilder *builder, const SizeList *for_loop_ids, const char *name, int value) {
    return builder_append(builder, "    mov dword [") &&
           builder_append_user_symbol_name(builder, for_loop_ids, name) &&
           builder_appendf(builder, "], %d\n", value);
}

static const char *symbol_name_at(const ASTProgram *program, const SymbolTable *symbols, size_t index) {
    if (symbols != NULL && symbols->names != NULL && index < symbols->count) {
        const char *name = symbols->names[index];

        if (name != NULL) {
            return name;
        }
    }

    if (program != NULL && program->declarations != NULL && index < program->declaration_count) {
        const char *name = program->declarations[index].name;

        if (name != NULL) {
            return name;
        }
    }

    return NULL;
}

static size_t symbol_count(const ASTProgram *program, const SymbolTable *symbols) {
    if (symbols != NULL && symbols->names != NULL) {
        return symbols->count;
    }

    if (program != NULL) {
        return program->declaration_count;
    }

    return 0;
}

static bool builder_append_booleanize(StringBuilder *builder, const char *register_name, const char *byte_register_name) {
    return builder_appendf(
        builder,
        "    cmp %s, 0\n"
        "    setne %s\n"
        "    movzx %s, %s\n",
        register_name,
        byte_register_name,
        register_name,
        byte_register_name);
}

static bool collect_for_loop_ids_in_block(SizeList *for_loop_ids, size_t *next_label_id, const ASTCommand *commands, size_t command_count);

static bool collect_for_loop_ids_in_command(SizeList *for_loop_ids, size_t *next_label_id, const ASTCommand *command) {
    size_t label_id;

    switch (command->type) {
        case AST_COMMAND_IF:
            (*next_label_id)++;
            return collect_for_loop_ids_in_block(
                       for_loop_ids, next_label_id, command->if_command.then_commands, command->if_command.then_count) &&
                   collect_for_loop_ids_in_block(
                       for_loop_ids, next_label_id, command->if_command.else_commands, command->if_command.else_count);
        case AST_COMMAND_WHILE:
            (*next_label_id)++;
            return collect_for_loop_ids_in_block(
                for_loop_ids, next_label_id, command->while_command.body_commands, command->while_command.body_count);
        case AST_COMMAND_FOR:
            label_id = (*next_label_id)++;
            return size_list_append(for_loop_ids, label_id) &&
                   collect_for_loop_ids_in_block(
                       for_loop_ids, next_label_id, command->for_command.body_commands, command->for_command.body_count);
        case AST_COMMAND_ASSIGNMENT:
        case AST_COMMAND_READ:
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
        case AST_COMMAND_CALL:
        case AST_COMMAND_RETURN:
            return true;
        default:
            return false;
    }
}

static bool collect_for_loop_ids_in_block(SizeList *for_loop_ids, size_t *next_label_id, const ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        if (!collect_for_loop_ids_in_command(for_loop_ids, next_label_id, &commands[index])) {
            return false;
        }
    }

    return true;
}

static bool generate_expression(CodegenContext *context, const ASTExpression *expression) {
    StringBuilder *builder = context->builder;

    if (expression == NULL) {
        return false;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return builder_appendf(builder, "    mov eax, %d\n", expression->int_value);
        case AST_EXPR_IDENTIFIER:
            return builder_append_load_identifier(builder, context->for_loop_ids, expression->identifier);
        case AST_EXPR_UNARY:
            if (!generate_expression(context, expression->unary.operand)) {
                return false;
            }

            switch (expression->unary.op) {
                case AST_UNARY_NEGATE:
                    return builder_append(builder, "    neg eax\n");
                case AST_UNARY_NOT:
                    return builder_append(builder, "    cmp eax, 0\n    sete al\n    movzx eax, al\n");
                default:
                    return false;
            }
        case AST_EXPR_BINARY:
            if (!generate_expression(context, expression->binary.left) ||
                !builder_append(builder, "    push eax\n") ||
                !generate_expression(context, expression->binary.right) ||
                !builder_append(builder, "    mov ebx, eax\n") ||
                !builder_append(builder, "    pop eax\n")) {
                return false;
            }

            switch (expression->binary.op) {
                case AST_BINARY_ADD:
                    return builder_append(builder, "    add eax, ebx\n");
                case AST_BINARY_SUB:
                    return builder_append(builder, "    sub eax, ebx\n");
                case AST_BINARY_MUL:
                    return builder_append(builder, "    imul eax, ebx\n");
                case AST_BINARY_DIV:
                    return builder_append(builder, "    cdq\n    idiv ebx\n");
                case AST_BINARY_GT:
                    return builder_append(builder, "    cmp eax, ebx\n    setg al\n    movzx eax, al\n");
                case AST_BINARY_LT:
                    return builder_append(builder, "    cmp eax, ebx\n    setl al\n    movzx eax, al\n");
                case AST_BINARY_EQ:
                    return builder_append(builder, "    cmp eax, ebx\n    sete al\n    movzx eax, al\n");
                case AST_BINARY_NE:
                    return builder_append(builder, "    cmp eax, ebx\n    setne al\n    movzx eax, al\n");
                case AST_BINARY_GE:
                    return builder_append(builder, "    cmp eax, ebx\n    setge al\n    movzx eax, al\n");
                case AST_BINARY_LE:
                    return builder_append(builder, "    cmp eax, ebx\n    setle al\n    movzx eax, al\n");
                case AST_BINARY_AND:
                    return builder_append_booleanize(builder, "eax", "al") &&
                           builder_append_booleanize(builder, "ebx", "bl") &&
                           builder_append(builder, "    and eax, ebx\n");
                case AST_BINARY_OR:
                    return builder_append_booleanize(builder, "eax", "al") &&
                           builder_append_booleanize(builder, "ebx", "bl") &&
                           builder_append(builder, "    or eax, ebx\n");
                default:
                    return false;
            }
        default:
            return false;
    }
}

static bool generate_command_block(CodegenContext *context, const ASTCommand *commands, size_t command_count);

static bool generate_command(CodegenContext *context, const ASTCommand *command) {
    StringBuilder *builder = context->builder;

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            if (command->assignment.expression != NULL && command->assignment.expression->type == AST_EXPR_INT) {
                return builder_append_store_int_to_identifier(
                    builder,
                    context->for_loop_ids,
                    command->assignment.name,
                    command->assignment.expression->int_value);
            }

            return generate_expression(context, command->assignment.expression) &&
                   builder_append_store_eax_to_identifier(builder, context->for_loop_ids, command->assignment.name);
        case AST_COMMAND_READ:
            return builder_append(builder, "    call read_int\n") &&
                   builder_append_store_eax_to_identifier(builder, context->for_loop_ids, command->read.name);
        case AST_COMMAND_WRITE:
            return generate_expression(context, command->write.expression) &&
                   builder_append(builder, "    call print_int\n");
        case AST_COMMAND_WRITELN:
            return generate_expression(context, command->write.expression) &&
                   builder_append(builder, "    call print_int\n") &&
                   builder_append(builder, "    call print_newline\n");
        case AST_COMMAND_CALL:
        case AST_COMMAND_RETURN:
            return false;
        case AST_COMMAND_IF: {
            size_t label_id = context->next_label_id++;

            if (!generate_expression(context, command->if_command.condition) ||
                !builder_append(builder, "    cmp eax, 0\n")) {
                return false;
            }

            if (command->if_command.else_count > 0) {
                return builder_appendf(builder, "    je .Lelse%zu\n", label_id) &&
                       generate_command_block(context, command->if_command.then_commands, command->if_command.then_count) &&
                       builder_appendf(builder, "    jmp .Lendif%zu\n", label_id) &&
                       builder_appendf(builder, ".Lelse%zu:\n", label_id) &&
                       generate_command_block(context, command->if_command.else_commands, command->if_command.else_count) &&
                       builder_appendf(builder, ".Lendif%zu:\n", label_id);
            }

            return builder_appendf(builder, "    je .Lendif%zu\n", label_id) &&
                   generate_command_block(context, command->if_command.then_commands, command->if_command.then_count) &&
                   builder_appendf(builder, ".Lendif%zu:\n", label_id);
        }
        case AST_COMMAND_WHILE: {
            size_t label_id = context->next_label_id++;

            return builder_appendf(builder, ".Lwhile%zu:\n", label_id) &&
                   generate_expression(context, command->while_command.condition) &&
                   builder_append(builder, "    cmp eax, 0\n") &&
                   builder_appendf(builder, "    je .Lendwhile%zu\n", label_id) &&
                   generate_command_block(context, command->while_command.body_commands, command->while_command.body_count) &&
                   builder_appendf(builder, "    jmp .Lwhile%zu\n", label_id) &&
                   builder_appendf(builder, ".Lendwhile%zu:\n", label_id);
        }
        case AST_COMMAND_FOR: {
            size_t label_id = context->next_label_id++;

            return generate_expression(context, command->for_command.start_expression) &&
                   builder_append_store_eax_to_identifier(builder, context->for_loop_ids, command->for_command.iterator_name) &&
                   generate_expression(context, command->for_command.end_expression) &&
                   builder_appendf(builder, "    mov dword [_for_end_%zu], eax\n", label_id) &&
                   generate_expression(context, command->for_command.step_expression) &&
                   builder_appendf(builder, "    mov dword [_for_step_%zu], eax\n", label_id) &&
                   builder_appendf(builder, ".Lfor%zu:\n", label_id) &&
                   builder_appendf(builder, "    mov eax, dword [_for_step_%zu]\n", label_id) &&
                   builder_append(builder, "    cmp eax, 0\n") &&
                   builder_appendf(builder, "    je .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, "    jg .Lforpos%zu\n", label_id) &&
                   builder_append_load_identifier(builder, context->for_loop_ids, command->for_command.iterator_name) &&
                   builder_appendf(builder, "    mov ebx, dword [_for_end_%zu]\n", label_id) &&
                   builder_append(builder, "    cmp eax, ebx\n") &&
                   builder_appendf(builder, "    jl .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, "    jmp .Lforbody%zu\n", label_id) &&
                   builder_appendf(builder, ".Lforpos%zu:\n", label_id) &&
                   builder_append_load_identifier(builder, context->for_loop_ids, command->for_command.iterator_name) &&
                   builder_appendf(builder, "    mov ebx, dword [_for_end_%zu]\n", label_id) &&
                   builder_append(builder, "    cmp eax, ebx\n") &&
                   builder_appendf(builder, "    jg .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, ".Lforbody%zu:\n", label_id) &&
                   generate_command_block(context, command->for_command.body_commands, command->for_command.body_count) &&
                   builder_append_load_identifier(builder, context->for_loop_ids, command->for_command.iterator_name) &&
                   builder_appendf(builder, "    mov ebx, dword [_for_step_%zu]\n", label_id) &&
                   builder_append(builder, "    add eax, ebx\n") &&
                   builder_append_store_eax_to_identifier(builder, context->for_loop_ids, command->for_command.iterator_name) &&
                   builder_appendf(builder, "    jmp .Lfor%zu\n", label_id) &&
                   builder_appendf(builder, ".Lendfor%zu:\n", label_id);
        }
        default:
            return false;
    }
}

static bool generate_command_block(CodegenContext *context, const ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        if (!generate_command(context, &commands[index])) {
            return false;
        }
    }

    return true;
}

static bool generate_helpers(StringBuilder *builder) {
    return builder_append(
               builder,
               "\n"
               "read_int:\n"
               "    mov eax, 3\n"
               "    mov ebx, 0\n"
               "    mov ecx, read_buffer\n"
               "    mov edx, 16\n"
               "    int 0x80\n"
               "    cmp eax, 0\n"
               "    jle .read_int_eof\n"
               "    lea edx, [read_buffer + eax]\n"
               "    xor esi, esi\n"
               "    xor edi, edi\n"
               "    mov ecx, read_buffer\n"
               "    cmp byte [ecx], '-'\n"
               "    jne .read_int_digits\n"
               "    mov esi, 1\n"
               "    inc ecx\n"
               ".read_int_digits:\n"
               "    cmp ecx, edx\n"
               "    jae .read_int_done\n"
               "    movzx eax, byte [ecx]\n"
               "    cmp al, 0\n"
               "    je .read_int_done\n"
               "    cmp al, 10\n"
               "    je .read_int_done\n"
               "    cmp al, 13\n"
               "    je .read_int_done\n"
               "    cmp al, '0'\n"
               "    jb .read_int_done\n"
               "    cmp al, '9'\n"
               "    ja .read_int_done\n"
               "    sub al, '0'\n"
               "    cmp edi, 214748364\n"
               "    jg .read_int_overflow\n"
               "    jl .read_int_no_overflow\n"
               "    cmp esi, 0\n"
               "    je .read_int_check_pos_digit\n"
               "    cmp al, 8\n"
               "    jg .read_int_overflow\n"
               "    jmp .read_int_no_overflow\n"
               ".read_int_check_pos_digit:\n"
               "    cmp al, 7\n"
               "    jg .read_int_overflow\n"
               ".read_int_no_overflow:\n"
               "    imul edi, edi, 10\n"
               "    add edi, eax\n"
               "    js .read_int_limit_hit\n"
               "    inc ecx\n"
               "    jmp .read_int_digits\n"
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
               "    ret\n"
               ".read_int_done:\n"
               "    mov eax, edi\n"
               "    cmp esi, 0\n"
               "    je .read_int_ret\n"
               "    neg eax\n"
               ".read_int_ret:\n"
               "    ret\n"
               ".read_int_eof:\n"
               "    xor eax, eax\n"
               "    ret\n"
               "\n"
               "print_int:\n"
               "    mov esi, eax\n"
               "    xor ecx, ecx\n"
               "    mov edi, print_buffer + 12\n"
               "    mov ebx, 10\n"
               ".print_int_loop:\n"
               "    cdq\n"
               "    idiv ebx\n"
               "    test edx, edx\n"
               "    jge .print_int_digit_ready\n"
               "    neg edx\n"
               ".print_int_digit_ready:\n"
               "    add dl, '0'\n"
               "    dec edi\n"
               "    mov [edi], dl\n"
               "    inc ecx\n"
               "    test eax, eax\n"
               "    jnz .print_int_loop\n"
               "    test esi, esi\n"
               "    jge .print_int_write\n"
               "    dec edi\n"
               "    mov byte [edi], '-'\n"
               "    inc ecx\n"
               ".print_int_write:\n"
               "    mov eax, 4\n"
               "    mov ebx, 1\n"
               "    mov edx, ecx\n"
               "    mov ecx, edi\n"
               "    int 0x80\n"
               "    ret\n"
               "\n"
               "print_newline:\n"
               "    mov eax, 4\n"
               "    mov ebx, 1\n"
               "    mov ecx, newline\n"
               "    mov edx, 1\n"
               "    int 0x80\n"
               "    ret\n");
}

char *codegen_generate_program(const ASTProgram *program, const SymbolTable *symbols) {
    StringBuilder builder = {0};
    SizeList for_loop_ids = {0};
    CodegenContext context = {.builder = &builder, .for_loop_ids = &for_loop_ids, .next_label_id = 0};
    size_t index;
    size_t next_label_id = 0;

    if (program == NULL) {
        return NULL;
    }

    if (!collect_for_loop_ids_in_block(&for_loop_ids, &next_label_id, program->commands, program->command_count)) {
        free(for_loop_ids.items);
        return NULL;
    }

    if (!builder_append(&builder, "global _start\n\nsection .data\n")) {
        free(for_loop_ids.items);
        free(builder.data);
        return NULL;
    }

    for (index = 0; index < symbol_count(program, symbols); ++index) {
        const char *name = symbol_name_at(program, symbols, index);

        if (name == NULL || !builder_append_user_symbol_declaration(&builder, &for_loop_ids, name)) {
            free(for_loop_ids.items);
            free(builder.data);
            return NULL;
        }
    }

    for (index = 0; index < for_loop_ids.count; ++index) {
        size_t label_id = for_loop_ids.items[index];

        if (!builder_appendf(&builder, "_for_end_%zu dd 0\n", label_id) ||
            !builder_appendf(&builder, "_for_step_%zu dd 0\n", label_id)) {
            free(for_loop_ids.items);
            free(builder.data);
            return NULL;
        }
    }

    if (!builder_append(&builder, "newline db 10\nprint_buffer times 12 db 0\nread_buffer times 16 db 0\n\nsection .text\n_start:\n")) {
        free(for_loop_ids.items);
        free(builder.data);
        return NULL;
    }

    if (!generate_command_block(&context, program->commands, program->command_count)) {
        free(for_loop_ids.items);
        free(builder.data);
        return NULL;
    }

    if (!builder_append(&builder, "    mov eax, 1\n    xor ebx, ebx\n    int 0x80\n") ||
        !generate_helpers(&builder)) {
        free(for_loop_ids.items);
        free(builder.data);
        return NULL;
    }

    free(for_loop_ids.items);
    return builder.data;
}
