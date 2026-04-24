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

static bool generate_expression(StringBuilder *builder, const ASTExpression *expression) {
    if (expression == NULL) {
        return false;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return builder_appendf(builder, "    mov eax, %d\n", expression->int_value);
        case AST_EXPR_IDENTIFIER:
            return builder_appendf(builder, "    mov eax, dword [%s]\n", expression->identifier);
        case AST_EXPR_UNARY:
            if (!generate_expression(builder, expression->unary.operand)) {
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
            if (!generate_expression(builder, expression->binary.left) ||
                !builder_append(builder, "    push eax\n") ||
                !generate_expression(builder, expression->binary.right) ||
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
                default:
                    return false;
            }
        default:
            return false;
    }
}

static bool generate_command(StringBuilder *builder, const ASTCommand *command) {
    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            if (command->assignment.expression != NULL && command->assignment.expression->type == AST_EXPR_INT) {
                return builder_appendf(
                    builder, "    mov dword [%s], %d\n", command->assignment.name, command->assignment.expression->int_value);
            }

            return generate_expression(builder, command->assignment.expression) &&
                   builder_appendf(builder, "    mov dword [%s], eax\n", command->assignment.name);
        case AST_COMMAND_WRITE:
            return generate_expression(builder, command->write.expression) &&
                   builder_append(builder, "    call print_int\n");
        case AST_COMMAND_WRITELN:
            return generate_expression(builder, command->write.expression) &&
                   builder_append(builder, "    call print_int\n") &&
                   builder_append(builder, "    call print_newline\n");
        default:
            return false;
    }
}

static bool generate_helpers(StringBuilder *builder) {
    return builder_append(
               builder,
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
    size_t index;

    if (program == NULL) {
        return NULL;
    }

    if (!builder_append(&builder, "global _start\n\nsection .data\n")) {
        free(builder.data);
        return NULL;
    }

    for (index = 0; index < symbol_count(program, symbols); ++index) {
        const char *name = symbol_name_at(program, symbols, index);

        if (name == NULL || !builder_appendf(&builder, "%s dd 0\n", name)) {
            free(builder.data);
            return NULL;
        }
    }

    if (!builder_append(&builder, "newline db 10\nprint_buffer times 12 db 0\n\nsection .text\n_start:\n")) {
        free(builder.data);
        return NULL;
    }

    for (index = 0; index < program->command_count; ++index) {
        if (!generate_command(&builder, &program->commands[index])) {
            free(builder.data);
            return NULL;
        }
    }

    if (!builder_append(&builder, "    mov eax, 1\n    xor ebx, ebx\n    int 0x80\n") ||
        !generate_helpers(&builder)) {
        free(builder.data);
        return NULL;
    }

    return builder.data;
}
