#include "codegen.h"
#include "error.h"

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
    const SizeList *procedure_for_loop_ids;
    size_t next_label_id;
    const ASTProgram *program;
    const SemanticInfo *semantic;
    /* procedure context (NULL when generating main program) */
    const char *current_proc_name;
    const ASTParameter *parameters;
    size_t parameter_count;
    const ASTDeclaration *proc_locals;
    size_t proc_local_count;
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

/* Context-aware identifier access: checks procedure params/locals before globals. */

static bool generate_load_name(CodegenContext *context, const char *name) {
    size_t i;

    if (context->parameters != NULL) {
        for (i = 0; i < context->parameter_count; i++) {
            if (strcmp(context->parameters[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov eax, dword [ebp+%zu]\n", 8 + i * 4);
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov eax, dword [ebp-%zu]\n", 4 + i * 4);
            }
        }
    }

    return builder_append_load_identifier(context->builder, context->for_loop_ids, name);
}

static bool generate_store_eax_to_name(CodegenContext *context, const char *name) {
    size_t i;

    if (context->parameters != NULL) {
        for (i = 0; i < context->parameter_count; i++) {
            if (strcmp(context->parameters[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp+%zu], eax\n", 8 + i * 4);
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp-%zu], eax\n", 4 + i * 4);
            }
        }
    }

    return builder_append_store_eax_to_identifier(context->builder, context->for_loop_ids, name);
}

static bool generate_store_int_to_name(CodegenContext *context, const char *name, int value) {
    size_t i;

    if (context->parameters != NULL) {
        for (i = 0; i < context->parameter_count; i++) {
            if (strcmp(context->parameters[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp+%zu], %d\n", 8 + i * 4, value);
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp-%zu], %d\n", 4 + i * 4, value);
            }
        }
    }

    return builder_append_store_int_to_identifier(context->builder, context->for_loop_ids, name, value);
}

/* Push an expression result onto the stack; optimise integer literals to push <N>. */
static bool generate_push_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error);

static const char *symbol_name_at(const ASTProgram *program, const SymbolInfo *globals, size_t global_count, size_t index) {
    if (globals != NULL && index < global_count) {
        const char *name = globals[index].name;

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

static size_t symbol_count(const ASTProgram *program, const SymbolInfo *globals, size_t global_count) {
    if (globals != NULL) {
        return global_count;
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
static bool procedure_supports_integer_backend(const ASTProcedure *procedure, CompilerError *error);
static bool main_program_supports_integer_backend(
    const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error);
static bool collect_for_loop_ids_in_program(
    SizeList *for_loop_ids, size_t *next_label_id, const ASTProgram *program);

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

static bool collect_for_loop_ids_in_program(
    SizeList *for_loop_ids, size_t *next_label_id, const ASTProgram *program) {
    size_t index;

    if (program == NULL) {
        return false;
    }

    if (!collect_for_loop_ids_in_block(for_loop_ids, next_label_id, program->commands, program->command_count)) {
        return false;
    }

    for (index = 0; index < program->procedure_count; ++index) {
        if (!collect_for_loop_ids_in_block(
                for_loop_ids, next_label_id, program->procedures[index].commands, program->procedures[index].command_count)) {
            return false;
        }
    }

    return true;
}

static bool fail_float_procedure_codegen(CompilerError *error, int line, int column) {
    compiler_error_set(
        error,
        COMPILER_PHASE_CODEGEN,
        line,
        column,
        "Code generation for flutuante procedures is not supported yet.");
    return false;
}

static bool fail_float_expression_codegen(CompilerError *error, int line, int column) {
    compiler_error_set(
        error,
        COMPILER_PHASE_CODEGEN,
        line,
        column,
        "Code generation for flutuante expressions is not supported yet.");
    return false;
}

static bool fail_float_main_codegen(CompilerError *error, int line, int column) {
    compiler_error_set(
        error,
        COMPILER_PHASE_CODEGEN,
        line,
        column,
        "Code generation for flutuante values in the main program is not supported yet.");
    return false;
}

static bool fail_codegen_internal(CompilerError *error, int line, int column, const char *message) {
    if (error != NULL && error->message[0] == '\0') {
        compiler_error_set(
            error,
            COMPILER_PHASE_CODEGEN,
            line,
            column,
            message != NULL ? message : "Internal error: code generation failed.");
    }

    return false;
}

static bool resolve_procedure_for_loop_offsets(
    size_t proc_local_count,
    const SizeList *procedure_for_loop_ids,
    size_t label_id,
    size_t *end_offset,
    size_t *step_offset,
    CompilerError *error,
    int line,
    int column) {
    size_t index;

    if (procedure_for_loop_ids == NULL) {
        return fail_codegen_internal(
            error, line, column, "Internal error: missing procedure for-loop temporary slot metadata.");
    }

    for (index = 0; index < procedure_for_loop_ids->count; ++index) {
        if (procedure_for_loop_ids->items[index] == label_id) {
            if (end_offset != NULL) {
                *end_offset = (proc_local_count + index * 2 + 1) * 4;
            }
            if (step_offset != NULL) {
                *step_offset = (proc_local_count + index * 2 + 2) * 4;
            }

            if ((end_offset != NULL && *end_offset == 0) || (step_offset != NULL && *step_offset == 0)) {
                return fail_codegen_internal(
                    error, line, column, "Internal error: invalid procedure-for-loop temporary slot layout.");
            }

            return true;
        }
    }

    return fail_codegen_internal(
        error, line, column, "Internal error: missing procedure for-loop temporary slot mapping.");
}

static bool procedure_expression_supports_integer_backend(const ASTExpression *expression, CompilerError *error) {
    size_t index;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
        case AST_EXPR_IDENTIFIER:
            return true;
        case AST_EXPR_FLOAT:
            return fail_float_procedure_codegen(error, expression->line, expression->column);
        case AST_EXPR_CALL:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!procedure_expression_supports_integer_backend(expression->call.arguments[index], error)) {
                    return false;
                }
            }
            return true;
        case AST_EXPR_UNARY:
            return procedure_expression_supports_integer_backend(expression->unary.operand, error);
        case AST_EXPR_BINARY:
            return procedure_expression_supports_integer_backend(expression->binary.left, error) &&
                   procedure_expression_supports_integer_backend(expression->binary.right, error);
        default:
            compiler_error_set(
                error,
                COMPILER_PHASE_CODEGEN,
                expression->line,
                expression->column,
                "Internal error: unsupported expression type in procedure backend check.");
            return false;
    }
}

static bool find_global_declaration_type(const ASTProgram *program, const char *name, ASTType *out_type) {
    size_t index;

    if (program == NULL || name == NULL) {
        return false;
    }

    for (index = 0; index < program->declaration_count; ++index) {
        if (strcmp(program->declarations[index].name, name) == 0) {
            if (out_type != NULL) {
                *out_type = program->declarations[index].type;
            }
            return true;
        }
    }

    return false;
}

static bool find_procedure_signature_type(
    const SemanticInfo *semantic, const char *name, ASTType *out_return_type) {
    size_t index;

    if (semantic == NULL || name == NULL) {
        return false;
    }

    for (index = 0; index < semantic->procedure_count; ++index) {
        if (strcmp(semantic->procedures[index].name, name) == 0) {
            if (out_return_type != NULL) {
                *out_return_type = semantic->procedures[index].return_type;
            }
            return true;
        }
    }

    return false;
}

static bool main_expression_supports_integer_backend(
    const ASTExpression *expression, const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    ASTType type;
    size_t index;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return true;
        case AST_EXPR_FLOAT:
            return fail_float_expression_codegen(error, expression->line, expression->column);
        case AST_EXPR_IDENTIFIER:
            if (find_global_declaration_type(program, expression->identifier, &type) && type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(error, expression->line, expression->column);
            }
            return true;
        case AST_EXPR_CALL:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!main_expression_supports_integer_backend(expression->call.arguments[index], program, semantic, error)) {
                    return false;
                }
            }

            if (find_procedure_signature_type(semantic, expression->call.name, &type) && type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(error, expression->line, expression->column);
            }
            return true;
        case AST_EXPR_UNARY:
            return main_expression_supports_integer_backend(expression->unary.operand, program, semantic, error);
        case AST_EXPR_BINARY:
            return main_expression_supports_integer_backend(expression->binary.left, program, semantic, error) &&
                   main_expression_supports_integer_backend(expression->binary.right, program, semantic, error);
        default:
            compiler_error_set(
                error,
                COMPILER_PHASE_CODEGEN,
                expression->line,
                expression->column,
                "Internal error: unsupported expression type in main backend check.");
            return false;
    }
}

static bool main_command_supports_integer_backend(
    const ASTCommand *command, const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    ASTType type;
    size_t index;

    if (command == NULL) {
        return true;
    }

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            if (find_global_declaration_type(program, command->assignment.target.type == AST_TARGET_IDENTIFIER ? command->assignment.target.identifier : command->assignment.target.indexed.name, &type) && type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(error, command->assignment.target.line, command->assignment.target.column);
            }
            return main_expression_supports_integer_backend(command->assignment.expression, program, semantic, error);
        case AST_COMMAND_READ:
            if (find_global_declaration_type(program, command->read.name, &type) && type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(error, command->read.line, command->read.column);
            }
            return true;
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            return main_expression_supports_integer_backend(command->write.expression, program, semantic, error);
        case AST_COMMAND_CALL:
            for (index = 0; index < command->call_command.call.argument_count; ++index) {
                if (!main_expression_supports_integer_backend(
                        command->call_command.call.arguments[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_RETURN:
            return true;
        case AST_COMMAND_IF:
            if (!main_expression_supports_integer_backend(command->if_command.condition, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->if_command.then_count; ++index) {
                if (!main_command_supports_integer_backend(
                        &command->if_command.then_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            for (index = 0; index < command->if_command.else_count; ++index) {
                if (!main_command_supports_integer_backend(
                        &command->if_command.else_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_WHILE:
            if (!main_expression_supports_integer_backend(command->while_command.condition, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->while_command.body_count; ++index) {
                if (!main_command_supports_integer_backend(
                        &command->while_command.body_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_FOR:
            if (find_global_declaration_type(program, command->for_command.iterator_name, &type) &&
                type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(error, command->for_command.line, command->for_command.column);
            }
            if (!main_expression_supports_integer_backend(command->for_command.start_expression, program, semantic, error) ||
                !main_expression_supports_integer_backend(command->for_command.end_expression, program, semantic, error) ||
                !main_expression_supports_integer_backend(command->for_command.step_expression, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->for_command.body_count; ++index) {
                if (!main_command_supports_integer_backend(
                        &command->for_command.body_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        default:
            compiler_error_set(
                error,
                COMPILER_PHASE_CODEGEN,
                0,
                0,
                "Internal error: unsupported command type in main backend check.");
            return false;
    }
}

static bool main_program_supports_integer_backend(
    const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    size_t index;

    if (program == NULL) {
        return false;
    }

    for (index = 0; index < program->command_count; ++index) {
        if (!main_command_supports_integer_backend(&program->commands[index], program, semantic, error)) {
            return false;
        }
    }

    return true;
}

static bool procedure_command_supports_integer_backend(const ASTCommand *command, CompilerError *error) {
    size_t index;

    if (command == NULL) {
        return true;
    }

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            return procedure_expression_supports_integer_backend(command->assignment.expression, error);
        case AST_COMMAND_READ:
            return true;
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            return procedure_expression_supports_integer_backend(command->write.expression, error);
        case AST_COMMAND_CALL:
            for (index = 0; index < command->call_command.call.argument_count; ++index) {
                if (!procedure_expression_supports_integer_backend(command->call_command.call.arguments[index], error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_RETURN:
            return procedure_expression_supports_integer_backend(command->return_command.expression, error);
        case AST_COMMAND_IF:
            if (!procedure_expression_supports_integer_backend(command->if_command.condition, error)) {
                return false;
            }
            for (index = 0; index < command->if_command.then_count; ++index) {
                if (!procedure_command_supports_integer_backend(&command->if_command.then_commands[index], error)) {
                    return false;
                }
            }
            for (index = 0; index < command->if_command.else_count; ++index) {
                if (!procedure_command_supports_integer_backend(&command->if_command.else_commands[index], error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_WHILE:
            if (!procedure_expression_supports_integer_backend(command->while_command.condition, error)) {
                return false;
            }
            for (index = 0; index < command->while_command.body_count; ++index) {
                if (!procedure_command_supports_integer_backend(&command->while_command.body_commands[index], error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_FOR:
            if (!procedure_expression_supports_integer_backend(command->for_command.start_expression, error) ||
                !procedure_expression_supports_integer_backend(command->for_command.end_expression, error) ||
                !procedure_expression_supports_integer_backend(command->for_command.step_expression, error)) {
                return false;
            }
            for (index = 0; index < command->for_command.body_count; ++index) {
                if (!procedure_command_supports_integer_backend(&command->for_command.body_commands[index], error)) {
                    return false;
                }
            }
            return true;
        default:
            compiler_error_set(
                error,
                COMPILER_PHASE_CODEGEN,
                0,
                0,
                "Internal error: unsupported command type in procedure backend check.");
            return false;
    }
}

static bool procedure_supports_integer_backend(const ASTProcedure *procedure, CompilerError *error) {
    size_t index;

    if (procedure == NULL) {
        return false;
    }

    if (procedure->return_type == AST_TYPE_FLUTUANTE) {
        return fail_float_procedure_codegen(error, procedure->line, procedure->column);
    }

    for (index = 0; index < procedure->parameter_count; ++index) {
        if (procedure->parameters[index].type == AST_TYPE_FLUTUANTE) {
            return fail_float_procedure_codegen(error, procedure->parameters[index].line, procedure->parameters[index].column);
        }
    }

    for (index = 0; index < procedure->local_declaration_count; ++index) {
        if (procedure->local_declarations[index].type == AST_TYPE_FLUTUANTE) {
            return fail_float_procedure_codegen(
                error,
                procedure->local_declarations[index].line,
                procedure->local_declarations[index].column);
        }
    }

    for (index = 0; index < procedure->command_count; ++index) {
        if (!procedure_command_supports_integer_backend(&procedure->commands[index], error)) {
            return false;
        }
    }

    return true;
}

static bool generate_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    StringBuilder *builder = context->builder;

    if (expression == NULL) {
        return false;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return builder_appendf(builder, "    mov eax, %d\n", expression->int_value);
        case AST_EXPR_FLOAT:
            return fail_float_expression_codegen(error, expression->line, expression->column);
        case AST_EXPR_IDENTIFIER:
            return generate_load_name(context, expression->identifier);
        case AST_EXPR_CALL: {
            const ASTCall *call = &expression->call;
            size_t arg_i;

            for (arg_i = call->argument_count; arg_i > 0; arg_i--) {
                if (!generate_push_expression(context, call->arguments[arg_i - 1], error)) {
                    return false;
                }
            }

            if (!builder_appendf(builder, "    call proc_%s\n", call->name)) {
                return false;
            }

            if (call->argument_count > 0) {
                return builder_appendf(builder, "    add esp, %zu\n", call->argument_count * 4);
            }

            return true;
        }
        case AST_EXPR_UNARY:
            if (!generate_expression(context, expression->unary.operand, error)) {
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
            if (!generate_expression(context, expression->binary.left, error) ||
                !builder_append(builder, "    push eax\n") ||
                !generate_expression(context, expression->binary.right, error) ||
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

static bool generate_command_block(
    CodegenContext *context, const ASTCommand *commands, size_t command_count, CompilerError *error);

static bool generate_command(CodegenContext *context, const ASTCommand *command, CompilerError *error) {
    StringBuilder *builder = context->builder;

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT: {
            if (command->assignment.target.type == AST_TARGET_INDEXED) {
                compiler_error_set(
                    error,
                    COMPILER_PHASE_CODEGEN,
                    command->assignment.target.line,
                    command->assignment.target.column,
                    "Internal error: indexed assignment not yet supported by code generator.");
                return false;
            }

            if (command->assignment.expression != NULL && command->assignment.expression->type == AST_EXPR_INT) {
                return generate_store_int_to_name(context, command->assignment.target.identifier,
                                                  command->assignment.expression->int_value);
            }

            return generate_expression(context, command->assignment.expression, error) &&
                   generate_store_eax_to_name(context, command->assignment.target.identifier);
        }
        case AST_COMMAND_READ:
            return builder_append(builder, "    call read_int\n") &&
                   generate_store_eax_to_name(context, command->read.name);
        case AST_COMMAND_WRITE:
            return generate_expression(context, command->write.expression, error) &&
                   builder_append(builder, "    call print_int\n");
        case AST_COMMAND_WRITELN:
            return generate_expression(context, command->write.expression, error) &&
                   builder_append(builder, "    call print_int\n") &&
                   builder_append(builder, "    call print_newline\n");
        case AST_COMMAND_CALL: {
            const ASTCall *call = &command->call_command.call;
            size_t arg_i;

            for (arg_i = call->argument_count; arg_i > 0; arg_i--) {
                if (!generate_push_expression(context, call->arguments[arg_i - 1], error)) {
                    return false;
                }
            }

            if (!builder_appendf(builder, "    call proc_%s\n", call->name)) {
                return false;
            }

            if (call->argument_count > 0) {
                return builder_appendf(builder, "    add esp, %zu\n", call->argument_count * 4);
            }

            return true;
        }
        case AST_COMMAND_RETURN:
            if (context->current_proc_name == NULL) {
                return fail_codegen_internal(
                    error,
                    command->return_command.line,
                    command->return_command.column,
                    "Internal error: return command outside a procedure.");
            }

            if (command->return_command.expression != NULL &&
                !generate_expression(context, command->return_command.expression, error)) {
                return false;
            }

            return builder_appendf(builder, "    jmp .proc_%s_epilogue\n", context->current_proc_name);
        case AST_COMMAND_IF: {
            size_t label_id = context->next_label_id++;

            if (!generate_expression(context, command->if_command.condition, error) ||
                !builder_append(builder, "    cmp eax, 0\n")) {
                return false;
            }

            if (command->if_command.else_count > 0) {
                return builder_appendf(builder, "    je .Lelse%zu\n", label_id) &&
                       generate_command_block(
                           context, command->if_command.then_commands, command->if_command.then_count, error) &&
                       builder_appendf(builder, "    jmp .Lendif%zu\n", label_id) &&
                       builder_appendf(builder, ".Lelse%zu:\n", label_id) &&
                       generate_command_block(
                           context, command->if_command.else_commands, command->if_command.else_count, error) &&
                       builder_appendf(builder, ".Lendif%zu:\n", label_id);
            }

            return builder_appendf(builder, "    je .Lendif%zu\n", label_id) &&
                   generate_command_block(context, command->if_command.then_commands, command->if_command.then_count, error) &&
                   builder_appendf(builder, ".Lendif%zu:\n", label_id);
        }
        case AST_COMMAND_WHILE: {
            size_t label_id = context->next_label_id++;

            return builder_appendf(builder, ".Lwhile%zu:\n", label_id) &&
                   generate_expression(context, command->while_command.condition, error) &&
                   builder_append(builder, "    cmp eax, 0\n") &&
                   builder_appendf(builder, "    je .Lendwhile%zu\n", label_id) &&
                   generate_command_block(context, command->while_command.body_commands, command->while_command.body_count, error) &&
                   builder_appendf(builder, "    jmp .Lwhile%zu\n", label_id) &&
                   builder_appendf(builder, ".Lendwhile%zu:\n", label_id);
        }
        case AST_COMMAND_FOR: {
            size_t label_id = context->next_label_id++;
            size_t end_offset = 0;
            size_t step_offset = 0;

            if (context->current_proc_name != NULL) {
                if (!resolve_procedure_for_loop_offsets(
                        context->proc_local_count,
                        context->procedure_for_loop_ids,
                        label_id,
                        &end_offset,
                        &step_offset,
                        error,
                        command->for_command.line,
                        command->for_command.column)) {
                    return false;
                }
            }

            if (!generate_expression(context, command->for_command.start_expression, error) ||
                !generate_store_eax_to_name(context, command->for_command.iterator_name) ||
                !generate_expression(context, command->for_command.end_expression, error)) {
                return false;
            }

            if (context->current_proc_name != NULL) {
                if (!builder_appendf(builder, "    mov dword [ebp-%zu], eax\n", end_offset)) {
                    return false;
                }
            } else if (!builder_appendf(builder, "    mov dword [_for_end_%zu], eax\n", label_id)) {
                return false;
            }

            if (!generate_expression(context, command->for_command.step_expression, error)) {
                return false;
            }

            if (context->current_proc_name != NULL) {
                if (!builder_appendf(builder, "    mov dword [ebp-%zu], eax\n", step_offset)) {
                    return false;
                }
            } else if (!builder_appendf(builder, "    mov dword [_for_step_%zu], eax\n", label_id)) {
                return false;
            }

            return builder_appendf(builder, ".Lfor%zu:\n", label_id) &&
                   (context->current_proc_name != NULL
                        ? builder_appendf(builder, "    mov eax, dword [ebp-%zu]\n", step_offset)
                        : builder_appendf(builder, "    mov eax, dword [_for_step_%zu]\n", label_id)) &&
                   builder_append(builder, "    cmp eax, 0\n") &&
                   builder_appendf(builder, "    je .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, "    jg .Lforpos%zu\n", label_id) &&
                   generate_load_name(context, command->for_command.iterator_name) &&
                   (context->current_proc_name != NULL
                        ? builder_appendf(builder, "    mov ebx, dword [ebp-%zu]\n", end_offset)
                        : builder_appendf(builder, "    mov ebx, dword [_for_end_%zu]\n", label_id)) &&
                   builder_append(builder, "    cmp eax, ebx\n") &&
                   builder_appendf(builder, "    jl .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, "    jmp .Lforbody%zu\n", label_id) &&
                   builder_appendf(builder, ".Lforpos%zu:\n", label_id) &&
                   generate_load_name(context, command->for_command.iterator_name) &&
                   (context->current_proc_name != NULL
                        ? builder_appendf(builder, "    mov ebx, dword [ebp-%zu]\n", end_offset)
                        : builder_appendf(builder, "    mov ebx, dword [_for_end_%zu]\n", label_id)) &&
                   builder_append(builder, "    cmp eax, ebx\n") &&
                   builder_appendf(builder, "    jg .Lendfor%zu\n", label_id) &&
                   builder_appendf(builder, ".Lforbody%zu:\n", label_id) &&
                   generate_command_block(context, command->for_command.body_commands, command->for_command.body_count, error) &&
                   generate_load_name(context, command->for_command.iterator_name) &&
                   (context->current_proc_name != NULL
                        ? builder_appendf(builder, "    mov ebx, dword [ebp-%zu]\n", step_offset)
                        : builder_appendf(builder, "    mov ebx, dword [_for_step_%zu]\n", label_id)) &&
                   builder_append(builder, "    add eax, ebx\n") &&
                   generate_store_eax_to_name(context, command->for_command.iterator_name) &&
                   builder_appendf(builder, "    jmp .Lfor%zu\n", label_id) &&
                   builder_appendf(builder, ".Lendfor%zu:\n", label_id);
        }
        default:
            return false;
    }
}

static bool generate_command_block(
    CodegenContext *context, const ASTCommand *commands, size_t command_count, CompilerError *error) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        if (!generate_command(context, &commands[index], error)) {
            return false;
        }
    }

    return true;
}

static bool generate_push_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    if (expression->type == AST_EXPR_INT) {
        return builder_appendf(context->builder, "    push %d\n", expression->int_value);
    }

    return generate_expression(context, expression, error) &&
           builder_append(context->builder, "    push eax\n");
}

static bool generate_procedure(
    const ASTProcedure *proc, StringBuilder *builder, const SizeList *for_loop_ids, size_t *next_label_id,
    CompilerError *error) {
    CodegenContext context;
    SizeList procedure_for_loop_ids = {0};
    size_t preview_next_label = next_label_id != NULL ? *next_label_id : 0;
    size_t total_stack_slots = 0;
    size_t slot_index;

    if (!collect_for_loop_ids_in_block(
            &procedure_for_loop_ids, &preview_next_label, proc->commands, proc->command_count)) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    total_stack_slots = proc->local_declaration_count + procedure_for_loop_ids.count * 2;

    if (!builder_appendf(builder, "\nproc_%s:\n", proc->name) ||
        !builder_append(builder, "    push ebp\n    mov ebp, esp\n")) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    if (total_stack_slots > 0) {
        if (!builder_appendf(builder, "    sub esp, %zu\n", total_stack_slots * 4)) {
            free(procedure_for_loop_ids.items);
            return false;
        }

        for (slot_index = 0; slot_index < total_stack_slots; ++slot_index) {
            if (!builder_appendf(builder, "    mov dword [ebp-%zu], 0\n", (slot_index + 1) * 4)) {
                free(procedure_for_loop_ids.items);
                return false;
            }
        }
    }

    context.builder = builder;
    context.for_loop_ids = for_loop_ids;
    context.procedure_for_loop_ids = &procedure_for_loop_ids;
    context.next_label_id = next_label_id != NULL ? *next_label_id : 0;
    context.program = NULL;
    context.semantic = NULL;
    context.current_proc_name = proc->name;
    context.parameters = proc->parameters;
    context.parameter_count = proc->parameter_count;
    context.proc_locals = proc->local_declarations;
    context.proc_local_count = proc->local_declaration_count;

    if (!generate_command_block(&context, proc->commands, proc->command_count, error)) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    if (next_label_id != NULL) {
        *next_label_id = context.next_label_id;
    }

    if (!builder_appendf(builder, ".proc_%s_epilogue:\n", proc->name) ||
        !builder_append(builder, "    mov esp, ebp\n    pop ebp\n    ret\n")) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    free(procedure_for_loop_ids.items);
    return true;
}

#ifdef CODEGEN_TESTING
bool codegen_debug_resolve_procedure_for_loop_offsets(
    size_t proc_local_count,
    const size_t *procedure_for_loop_ids,
    size_t procedure_for_loop_id_count,
    size_t label_id,
    size_t *end_offset,
    size_t *step_offset,
    CompilerError *error,
    int line,
    int column) {
    SizeList list = {
        .items = (size_t *)procedure_for_loop_ids,
        .count = procedure_for_loop_id_count,
        .capacity = procedure_for_loop_id_count,
    };

    return resolve_procedure_for_loop_offsets(
        proc_local_count, procedure_for_loop_ids != NULL ? &list : NULL, label_id, end_offset, step_offset, error, line,
        column);
}
#endif

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

bool codegen_generate_program(const ASTProgram *program, const SemanticInfo *semantic, char **out_assembly, CompilerError *error) {
    StringBuilder builder = {0};
    SizeList for_loop_ids = {0};
    SizeList main_for_loop_ids = {0};
    CodegenContext context;
    const SymbolInfo *globals;
    size_t global_count;
    size_t index;
    size_t next_label_id = 0;

    if (out_assembly != NULL) {
        *out_assembly = NULL;
    }

    if (program == NULL) {
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    globals = (semantic != NULL) ? semantic->globals : NULL;
    global_count = (semantic != NULL) ? semantic->global_count : 0;

    /* Pre-check: reject any flutuante procedure before emitting anything. */
    for (index = 0; index < program->procedure_count; index++) {
        const ASTProcedure *proc = &program->procedures[index];

        if (!procedure_supports_integer_backend(proc, error)) {
            return false;
        }
    }

    if (!main_program_supports_integer_backend(program, semantic, error)) {
        return false;
    }

    if (!collect_for_loop_ids_in_program(&for_loop_ids, &next_label_id, program)) {
        free(for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    if (!collect_for_loop_ids_in_block(&main_for_loop_ids, &(size_t){0}, program->commands, program->command_count)) {
        free(main_for_loop_ids.items);
        free(for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    if (!builder_append(&builder, "global _start\n\nsection .data\n")) {
        goto fail;
    }

    for (index = 0; index < symbol_count(program, globals, global_count); ++index) {
        const char *name = symbol_name_at(program, globals, global_count, index);

        if (name == NULL || !builder_append_user_symbol_declaration(&builder, &for_loop_ids, name)) {
            goto fail;
        }
    }

    for (index = 0; index < main_for_loop_ids.count; ++index) {
        size_t label_id = main_for_loop_ids.items[index];

        if (!builder_appendf(&builder, "_for_end_%zu dd 0\n", label_id) ||
            !builder_appendf(&builder, "_for_step_%zu dd 0\n", label_id)) {
            goto fail;
        }
    }

    if (!builder_append(&builder, "newline db 10\nprint_buffer times 12 db 0\nread_buffer times 16 db 0\n\nsection .text\n_start:\n")) {
        goto fail;
    }

    context.builder = &builder;
    context.for_loop_ids = &for_loop_ids;
    context.procedure_for_loop_ids = NULL;
    context.next_label_id = 0;
    context.program = program;
    context.semantic = semantic;
    context.current_proc_name = NULL;
    context.parameters = NULL;
    context.parameter_count = 0;
    context.proc_locals = NULL;
    context.proc_local_count = 0;

    if (!generate_command_block(&context, program->commands, program->command_count, error)) {
        goto fail;
    }

    if (!builder_append(&builder, "    mov eax, 1\n    xor ebx, ebx\n    int 0x80\n")) {
        goto fail;
    }

    for (index = 0; index < program->procedure_count; index++) {
        if (!generate_procedure(&program->procedures[index], &builder, &for_loop_ids, &context.next_label_id, error)) {
            goto fail;
        }
    }

    if (!generate_helpers(&builder)) {
        goto fail;
    }

    free(main_for_loop_ids.items);
    free(for_loop_ids.items);
    if (out_assembly != NULL) {
        *out_assembly = builder.data;
    } else {
        free(builder.data);
    }
    return true;

fail:
    free(main_for_loop_ids.items);
    free(for_loop_ids.items);
    free(builder.data);
    return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
}
