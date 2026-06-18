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
    const ASTExpression *expression;
    size_t label_id;
} StringLiteralLabel;

typedef struct {
    const ASTExpression *expression;
    size_t label_id;
} FloatLiteralLabel;

typedef struct {
    const ASTCall *call;
    size_t label_id;
    size_t capacity;
} StringReturnTempLabel;

typedef struct {
    StringLiteralLabel *items;
    size_t count;
    size_t capacity;
} StringLiteralList;

typedef struct {
    FloatLiteralLabel *items;
    size_t count;
    size_t capacity;
} FloatLiteralList;

typedef struct {
    StringReturnTempLabel *items;
    size_t count;
    size_t capacity;
} StringReturnTempList;

typedef struct {
    StringBuilder *builder;
    const SizeList *for_loop_ids;
    const SizeList *procedure_for_loop_ids;
    size_t next_label_id;
    const ASTProgram *program;
    const SemanticInfo *semantic;
    /* procedure context (NULL when generating main program) */
    const char *current_proc_name;
    const ASTProcedure *current_procedure;
    const ASTParameter *parameters;
    size_t parameter_count;
    const ASTDeclaration *proc_locals;
    size_t proc_local_count;        /* number of local declarations (for iteration) */
    size_t proc_local_frame_bytes;  /* aligned byte count of local frame (used for for-loop temp offsets) */
    const StringReturnTempList *string_return_temps;
    const StringLiteralList *string_literals;
    const FloatLiteralList *float_literals;
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

static bool string_literal_list_append(StringLiteralList *list, const ASTExpression *expression, size_t label_id) {
    StringLiteralLabel *items;
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

    list->items[list->count].expression = expression;
    list->items[list->count].label_id = label_id;
    list->count++;
    return true;
}

static bool float_literal_list_append(FloatLiteralList *list, const ASTExpression *expression, size_t label_id) {
    FloatLiteralLabel *items;
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

    list->items[list->count].expression = expression;
    list->items[list->count].label_id = label_id;
    list->count++;
    return true;
}

static bool string_return_temp_list_append(
    StringReturnTempList *list, const ASTCall *call, size_t label_id, size_t capacity) {
    StringReturnTempLabel *items;
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

    list->items[list->count].call = call;
    list->items[list->count].label_id = label_id;
    list->items[list->count].capacity = capacity;
    list->count++;
    return true;
}

static const StringReturnTempLabel *find_string_return_temp_label(
    const StringReturnTempList *list, const ASTCall *call) {
    size_t index;

    if (list == NULL || call == NULL) {
        return NULL;
    }

    for (index = 0; index < list->count; ++index) {
        if (list->items[index].call == call) {
            return &list->items[index];
        }
    }

    return NULL;
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

static const ProcedureSignature *find_procedure_signature(const SemanticInfo *semantic, const char *name);

static size_t storage_size_bytes(ASTType type, ASTStorageKind storage, size_t capacity) {
    if (storage == AST_STORAGE_INDEXED) {
        if (type == AST_TYPE_STRING) {
            return capacity;
        }
        if (type == AST_TYPE_FLUTUANTE) {
            return capacity * 8;
        }
        return capacity * 4;
    }

    return type == AST_TYPE_FLUTUANTE ? 8 : 4;
}

static size_t parameter_slot_size_bytes(ASTType type, ASTStorageKind storage) {
    if (storage == AST_STORAGE_INDEXED) {
        return 4;
    }

    return type == AST_TYPE_FLUTUANTE ? 8 : 4;
}

/* Returns the total frame bytes occupied by a list of local declarations.
   String vectors reserve capacity bytes; integer vectors reserve capacity*4 bytes;
   float vectors reserve capacity*8 bytes; float scalars reserve 8 bytes. */
static size_t compute_local_frame_bytes(const ASTDeclaration *locals, size_t count) {
    size_t i;
    size_t bytes = 0;

    for (i = 0; i < count; i++) {
        bytes += storage_size_bytes(locals[i].type, locals[i].storage, locals[i].capacity);
    }
    return bytes;
}

static bool procedure_parameter_is_by_value_aggregate(const ASTParameter *parameter) {
    return parameter != NULL && parameter->storage == AST_STORAGE_INDEXED && parameter->pass_mode == AST_PASS_BY_VALUE;
}

static bool build_codegen_parameter_views(
    const ASTProcedure *proc,
    ASTParameter **out_parameters,
    size_t *out_parameter_count,
    ASTDeclaration **out_locals,
    size_t *out_local_count) {
    ASTParameter *parameters = NULL;
    ASTDeclaration *locals = NULL;
    size_t parameter_total = 0;
    size_t local_total = 0;
    size_t parameter_index = 0;
    size_t local_index = 0;
    size_t index;
    size_t by_value_count = 0;

    if (out_parameters == NULL || out_parameter_count == NULL || out_locals == NULL || out_local_count == NULL) {
        return false;
    }

    for (index = 0; index < proc->parameter_count; ++index) {
        if (procedure_parameter_is_by_value_aggregate(&proc->parameters[index])) {
            by_value_count++;
        } else {
            parameter_total++;
        }
    }

    if (parameter_total > 0) {
        parameters = malloc(parameter_total * sizeof(*parameters));
        if (parameters == NULL) {
            return false;
        }
    }

    local_total = by_value_count + proc->local_declaration_count;
    if (local_total > 0) {
        locals = calloc(local_total, sizeof(*locals));
        if (locals == NULL) {
            free(parameters);
            return false;
        }
    }

    for (index = 0; index < proc->parameter_count; ++index) {
        const ASTParameter *parameter = &proc->parameters[index];

        if (procedure_parameter_is_by_value_aggregate(parameter)) {
            locals[local_index].name = malloc(strlen(parameter->name) + 1);
            if (locals[local_index].name == NULL) {
                goto fail;
            }
            strcpy(locals[local_index].name, parameter->name);
            locals[local_index].type = parameter->type;
            locals[local_index].storage = parameter->storage;
            locals[local_index].capacity = parameter->capacity;
            locals[local_index].line = parameter->line;
            locals[local_index].column = parameter->column;
            local_index++;
            continue;
        }

        parameters[parameter_index].name = malloc(strlen(parameter->name) + 1);
        if (parameters[parameter_index].name == NULL) {
            goto fail;
        }
        strcpy(parameters[parameter_index].name, parameter->name);
        parameters[parameter_index].type = parameter->type;
        parameters[parameter_index].storage = parameter->storage;
        parameters[parameter_index].capacity = parameter->capacity;
        parameter_index++;
    }

    for (index = 0; index < proc->local_declaration_count; ++index) {
        const ASTDeclaration *decl = &proc->local_declarations[index];

        locals[local_index].name = malloc(strlen(decl->name) + 1);
        if (locals[local_index].name == NULL) {
            goto fail;
        }
        strcpy(locals[local_index].name, decl->name);
        locals[local_index].type = decl->type;
        locals[local_index].storage = decl->storage;
        locals[local_index].capacity = decl->capacity;
        locals[local_index].line = decl->line;
        locals[local_index].column = decl->column;
        local_index++;
    }

    *out_parameters = parameters;
    *out_parameter_count = parameter_index;
    *out_locals = locals;
    *out_local_count = local_index;
    return true;

fail:
    if (locals != NULL) {
        for (index = 0; index < local_total; ++index) {
            free(locals[index].name);
        }
    }
    free(locals);
    if (parameters != NULL) {
        for (index = 0; index < parameter_index; ++index) {
            free(parameters[index].name);
        }
    }
    free(parameters);
    return false;
}

/* Returns the ebp-relative base offset (positive) for local declaration[target_index].
   For scalars and integer vectors [ebp-offset] addresses the first/only dword element.
   For strings [ebp-offset] addresses byte 0, with subsequent bytes at increasing addresses
   ([ebp-offset+1], [ebp-offset+2], …) so the layout is compatible with print_string/read_string. */
static size_t local_declaration_frame_offset(const ASTDeclaration *proc_locals, size_t target_index) {
    size_t i;
    size_t bytes = 0;

    for (i = 0; i < target_index; i++) {
        bytes += storage_size_bytes(proc_locals[i].type, proc_locals[i].storage, proc_locals[i].capacity);
    }
    /* Add the target variable's own contribution to find its base offset. */
    if (proc_locals[target_index].storage == AST_STORAGE_INDEXED) {
        if (proc_locals[target_index].type == AST_TYPE_STRING) {
            bytes += proc_locals[target_index].capacity;
        } else if (proc_locals[target_index].type == AST_TYPE_FLUTUANTE) {
            bytes += 8;
        } else {
            bytes += 4;
        }
    } else {
        bytes += storage_size_bytes(
            proc_locals[target_index].type,
            proc_locals[target_index].storage,
            proc_locals[target_index].capacity);
    }
    return bytes;
}

/* Returns the ebp-relative base offset of the named local, or 0 if not found. */
static size_t local_declaration_base_offset_by_name(
    const ASTDeclaration *proc_locals, size_t proc_local_count, const char *name) {
    size_t i;

    for (i = 0; i < proc_local_count; i++) {
        if (strcmp(proc_locals[i].name, name) == 0) {
            return local_declaration_frame_offset(proc_locals, i);
        }
    }
    return 0;
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

static const ASTParameter *find_context_parameter(
    const ASTParameter *parameters, size_t parameter_count, const char *name, size_t *out_index) {
    size_t index;

    if (parameters == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0; index < parameter_count; ++index) {
        if (strcmp(parameters[index].name, name) == 0) {
            if (out_index != NULL) {
                *out_index = index;
            }
            return &parameters[index];
        }
    }

    return NULL;
}

static size_t procedure_parameter_base_offset(const ASTProcedure *procedure) {
    if (procedure != NULL && procedure->return_type == AST_TYPE_STRING) {
        return 12;
    }

    return 8;
}

static size_t parameter_stack_offset(
    const ASTProcedure *procedure, const ASTParameter *parameters, size_t parameter_count, size_t parameter_index) {
    size_t index;
    size_t offset = procedure_parameter_base_offset(procedure);

    if (parameters == NULL || parameter_index >= parameter_count) {
        return 0;
    }

    for (index = 0; index < parameter_index; ++index) {
        offset += parameter_slot_size_bytes(parameters[index].type, parameters[index].storage);
    }

    return offset;
}

static size_t procedure_string_return_buffer_stack_offset(const ASTProcedure *procedure) {
    if (procedure == NULL || procedure->return_type != AST_TYPE_STRING) {
        return 0;
    }

    return 8;
}

static size_t procedure_signature_argument_bytes(const ProcedureSignature *signature) {
    size_t index;
    size_t bytes = signature != NULL && signature->return_type == AST_TYPE_STRING ? 4 : 0;

    if (signature == NULL) {
        return 0;
    }

    for (index = 0; index < signature->parameter_count; ++index) {
        bytes += parameter_slot_size_bytes(signature->parameters[index].type, signature->parameters[index].storage);
    }

    return bytes;
}

/* Context-aware identifier access: checks procedure params/locals before globals. */

static bool generate_load_name(CodegenContext *context, const char *name) {
    size_t i;

    if (context->parameters != NULL) {
        for (i = 0; i < context->parameter_count; i++) {
            if (strcmp(context->parameters[i].name, name) == 0) {
                return builder_appendf(
                    context->builder,
                    "    mov eax, dword [ebp+%zu]\n",
                    parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, i));
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov eax, dword [ebp-%zu]\n",
                                       local_declaration_frame_offset(context->proc_locals, i));
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
                return builder_appendf(
                    context->builder,
                    "    mov dword [ebp+%zu], eax\n",
                    parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, i));
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp-%zu], eax\n",
                                       local_declaration_frame_offset(context->proc_locals, i));
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
                return builder_appendf(
                    context->builder,
                    "    mov dword [ebp+%zu], %d\n",
                    parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, i),
                    value);
            }
        }
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                return builder_appendf(context->builder, "    mov dword [ebp-%zu], %d\n",
                                       local_declaration_frame_offset(context->proc_locals, i), value);
            }
        }
    }

    return builder_append_store_int_to_identifier(context->builder, context->for_loop_ids, name, value);
}

/* Push an expression result onto the stack; optimise integer literals to push <N>. */
static bool generate_push_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error);
static bool generate_float_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error);
static bool generate_call(CodegenContext *context, const ASTCall *call, CompilerError *error);
static bool generate_push_call_argument(
    CodegenContext *context,
    const char *procedure_name,
    size_t argument_index,
    const ASTExpression *expression,
    CompilerError *error);

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

static ASTStorageKind symbol_storage_at(
    const ASTProgram *program, const SymbolInfo *globals, size_t global_count, size_t index) {
    if (globals != NULL && index < global_count) {
        return globals[index].storage;
    }

    if (program != NULL && program->declarations != NULL && index < program->declaration_count) {
        return program->declarations[index].storage;
    }

    return AST_STORAGE_SCALAR;
}

static size_t symbol_capacity_at(
    const ASTProgram *program, const SymbolInfo *globals, size_t global_count, size_t index) {
    if (globals != NULL && index < global_count) {
        return globals[index].capacity;
    }

    if (program != NULL && program->declarations != NULL && index < program->declaration_count) {
        return program->declarations[index].capacity;
    }

    return 0;
}

static ASTType symbol_type_at(
    const ASTProgram *program, const SymbolInfo *globals, size_t global_count, size_t index) {
    if (globals != NULL && index < global_count) {
        return globals[index].type;
    }

    if (program != NULL && program->declarations != NULL && index < program->declaration_count) {
        return program->declarations[index].type;
    }

    return AST_TYPE_INTEIRO;
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
static bool main_program_supports_backend(
    const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error);
static bool collect_for_loop_ids_in_program(
    SizeList *for_loop_ids, size_t *next_label_id, const ASTProgram *program);
static bool find_procedure_signature_type(
    const SemanticInfo *semantic, const char *name, ASTType *out_return_type);
static bool generate_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error);

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
    size_t proc_local_frame_bytes,
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
                *end_offset = proc_local_frame_bytes + (index * 2 + 1) * 4;
            }
            if (step_offset != NULL) {
                *step_offset = proc_local_frame_bytes + (index * 2 + 2) * 4;
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

static size_t find_global_declaration_capacity(const ASTProgram *program, const char *name) {
    size_t index;

    if (program == NULL || name == NULL) {
        return 0;
    }

    for (index = 0; index < program->declaration_count; ++index) {
        if (strcmp(program->declarations[index].name, name) == 0) {
            return program->declarations[index].capacity;
        }
    }

    return 0;
}

static bool find_context_variable_info(
    const ASTParameter *parameters,
    size_t parameter_count,
    const ASTDeclaration *proc_locals,
    size_t proc_local_count,
    const ASTProgram *program,
    const char *name,
    ASTType *out_type,
    ASTStorageKind *out_storage,
    size_t *out_capacity,
    size_t *out_dimension_count,
    size_t *out_row_capacity) {
    size_t i;
    ASTType type;

    if (name == NULL) {
        return false;
    }

    if (parameters != NULL) {
        for (i = 0; i < parameter_count; ++i) {
            if (strcmp(parameters[i].name, name) == 0) {
                if (out_type != NULL) {
                    *out_type = parameters[i].type;
                }
                if (out_storage != NULL) {
                    *out_storage = parameters[i].storage;
                }
                if (out_capacity != NULL) {
                    *out_capacity = parameters[i].capacity;
                }
                if (out_dimension_count != NULL) {
                    *out_dimension_count = parameters[i].dimension_count;
                }
                if (out_row_capacity != NULL) {
                    *out_row_capacity = parameters[i].row_capacity;
                }
                return true;
            }
        }
    }

    if (proc_locals != NULL) {
        for (i = 0; i < proc_local_count; i++) {
            if (strcmp(proc_locals[i].name, name) == 0) {
                if (out_type != NULL) {
                    *out_type = proc_locals[i].type;
                }
                if (out_storage != NULL) {
                    *out_storage = proc_locals[i].storage;
                }
                if (out_capacity != NULL) {
                    *out_capacity = proc_locals[i].capacity;
                }
                if (out_dimension_count != NULL) {
                    *out_dimension_count = proc_locals[i].dimension_count;
                }
                if (out_row_capacity != NULL) {
                    *out_row_capacity = proc_locals[i].row_capacity;
                }
                return true;
            }
        }
    }

    if (find_global_declaration_type(program, name, &type)) {
        if (out_type != NULL) {
            *out_type = type;
        }
        if (out_storage != NULL) {
            size_t index;

            *out_storage = AST_STORAGE_SCALAR;
            for (index = 0; index < program->declaration_count; ++index) {
                if (strcmp(program->declarations[index].name, name) == 0) {
                    *out_storage = program->declarations[index].storage;
                    break;
                }
            }
        }
        if (out_capacity != NULL) {
            *out_capacity = find_global_declaration_capacity(program, name);
        }
        if (out_dimension_count != NULL || out_row_capacity != NULL) {
            size_t index;

            for (index = 0; index < program->declaration_count; ++index) {
                if (strcmp(program->declarations[index].name, name) == 0) {
                    if (out_dimension_count != NULL) {
                        *out_dimension_count = program->declarations[index].dimension_count;
                    }
                    if (out_row_capacity != NULL) {
                        *out_row_capacity = program->declarations[index].row_capacity;
                    }
                    break;
                }
            }
        }
        return true;
    }

    return false;
}

static ASTType find_context_variable_type(
    const ASTParameter *parameters,
    size_t parameter_count,
    const ASTDeclaration *proc_locals,
    size_t proc_local_count,
    const ASTProgram *program,
    const char *name) {
    ASTType type = AST_TYPE_INTEIRO;

    find_context_variable_info(
        parameters, parameter_count, proc_locals, proc_local_count, program, name, &type, NULL, NULL, NULL, NULL);
    return type;
}

static size_t find_context_variable_capacity(
    const ASTParameter *parameters,
    size_t parameter_count,
    const ASTDeclaration *proc_locals,
    size_t proc_local_count,
    const ASTProgram *program,
    const char *name) {
    size_t capacity = 0;

    find_context_variable_info(
        parameters, parameter_count, proc_locals, proc_local_count, program, name, NULL, NULL, &capacity, NULL, NULL);
    return capacity;
}

static size_t find_context_variable_dimension_count(
    const ASTParameter *parameters,
    size_t parameter_count,
    const ASTDeclaration *proc_locals,
    size_t proc_local_count,
    const ASTProgram *program,
    const char *name) {
    size_t dimension_count = 0;

    find_context_variable_info(
        parameters, parameter_count, proc_locals, proc_local_count, program, name, NULL, NULL, NULL, &dimension_count, NULL);
    return dimension_count;
}

static size_t find_context_variable_row_capacity(
    const ASTParameter *parameters,
    size_t parameter_count,
    const ASTDeclaration *proc_locals,
    size_t proc_local_count,
    const ASTProgram *program,
    const char *name) {
    size_t row_capacity = 0;

    find_context_variable_info(
        parameters, parameter_count, proc_locals, proc_local_count, program, name, NULL, NULL, NULL, NULL, &row_capacity);
    return row_capacity;
}

static bool generate_linearized_index(
    CodegenContext *context, const ASTIndexedAccess *access, CompilerError *error) {
    size_t dimension_count = find_context_variable_dimension_count(
        context->parameters,
        context->parameter_count,
        context->proc_locals,
        context->proc_local_count,
        context->program,
        access->name);

    if (dimension_count >= 2 && access->index2 != NULL) {
        size_t row_capacity = find_context_variable_row_capacity(
            context->parameters,
            context->parameter_count,
            context->proc_locals,
            context->proc_local_count,
            context->program,
            access->name);

        return generate_expression(context, access->index, error) &&
               builder_append(context->builder, "    push eax\n") &&
               generate_expression(context, access->index2, error) &&
               builder_append(context->builder, "    mov ebx, eax\n") &&
               builder_append(context->builder, "    pop eax\n") &&
               builder_appendf(context->builder, "    imul eax, %zu\n", row_capacity) &&
               builder_append(context->builder, "    add eax, ebx\n");
    }

    return generate_expression(context, access->index, error);
}

static bool infer_expression_type(
    const CodegenContext *context,
    const ASTExpression *expression,
    ASTType *out_type) {
    ASTType left_type;
    ASTType right_type;

    if (context == NULL || expression == NULL || out_type == NULL) {
        return false;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            *out_type = AST_TYPE_INTEIRO;
            return true;
        case AST_EXPR_FLOAT:
            *out_type = AST_TYPE_FLUTUANTE;
            return true;
        case AST_EXPR_STRING:
            *out_type = AST_TYPE_STRING;
            return true;
        case AST_EXPR_IDENTIFIER:
            return find_context_variable_info(
                context->parameters,
                context->parameter_count,
                context->proc_locals,
                context->proc_local_count,
                context->program,
                expression->identifier,
                out_type,
                NULL,
                NULL,
                NULL,
                NULL);
        case AST_EXPR_INDEX:
            if (!find_context_variable_info(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    expression->index_access.name,
                    &left_type,
                    NULL,
                    NULL,
                    NULL,
                    NULL)) {
                return false;
            }
            *out_type = left_type == AST_TYPE_STRING ? AST_TYPE_INTEIRO : left_type;
            return true;
        case AST_EXPR_CALL:
            return find_procedure_signature_type(context->semantic, expression->call.name, out_type);
        case AST_EXPR_CAST:
            if (out_type != NULL) {
                *out_type = expression->cast.target_type;
            }
            return true;
        case AST_EXPR_UNARY:
            if (!infer_expression_type(context, expression->unary.operand, &left_type)) {
                return false;
            }
            *out_type = expression->unary.op == AST_UNARY_NOT ? AST_TYPE_INTEIRO : left_type;
            return true;
        case AST_EXPR_BINARY:
            if (!infer_expression_type(context, expression->binary.left, &left_type) ||
                !infer_expression_type(context, expression->binary.right, &right_type)) {
                return false;
            }

            switch (expression->binary.op) {
                case AST_BINARY_ADD:
                case AST_BINARY_SUB:
                case AST_BINARY_MUL:
                    *out_type = (left_type == AST_TYPE_FLUTUANTE || right_type == AST_TYPE_FLUTUANTE)
                        ? AST_TYPE_FLUTUANTE
                        : AST_TYPE_INTEIRO;
                    return true;
                case AST_BINARY_DIV:
                case AST_BINARY_GT:
                case AST_BINARY_LT:
                case AST_BINARY_EQ:
                case AST_BINARY_NE:
                case AST_BINARY_GE:
                case AST_BINARY_LE:
                case AST_BINARY_AND:
                case AST_BINARY_OR:
                    *out_type = AST_TYPE_INTEIRO;
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}

static bool infer_main_expression_type(
    const ASTExpression *expression,
    const ASTProgram *program,
    const SemanticInfo *semantic,
    ASTType *out_type) {
    CodegenContext context = {0};

    context.program = program;
    context.semantic = semantic;
    return infer_expression_type(&context, expression, out_type);
}

static bool expression_needs_float_codegen(const CodegenContext *context, const ASTExpression *expression) {
    ASTType expression_type;
    ASTType left_type;
    ASTType right_type;

    if (context == NULL || expression == NULL) {
        return false;
    }

    if (expression->type == AST_EXPR_BINARY) {
        if (!infer_expression_type(context, expression->binary.left, &left_type) ||
            !infer_expression_type(context, expression->binary.right, &right_type)) {
            return false;
        }

        if ((expression->binary.op == AST_BINARY_GT ||
             expression->binary.op == AST_BINARY_LT ||
             expression->binary.op == AST_BINARY_EQ ||
             expression->binary.op == AST_BINARY_NE ||
             expression->binary.op == AST_BINARY_GE ||
             expression->binary.op == AST_BINARY_LE) &&
            (left_type == AST_TYPE_FLUTUANTE || right_type == AST_TYPE_FLUTUANTE)) {
            return true;
        }
    }

    return infer_expression_type(context, expression, &expression_type) &&
           expression_type == AST_TYPE_FLUTUANTE;
}

static const ProcedureSignature *find_procedure_signature(const SemanticInfo *semantic, const char *name) {
    size_t index;

    if (semantic == NULL || name == NULL) {
        return NULL;
    }

    for (index = 0; index < semantic->procedure_count; ++index) {
        if (strcmp(semantic->procedures[index].name, name) == 0) {
            return &semantic->procedures[index];
        }
    }

    return NULL;
}

static bool find_procedure_signature_type(
    const SemanticInfo *semantic, const char *name, ASTType *out_return_type) {
    const ProcedureSignature *signature = find_procedure_signature(semantic, name);

    if (signature == NULL) {
        return false;
    }

    if (out_return_type != NULL) {
        *out_return_type = signature->return_type;
    }

    return true;
}

static bool main_expression_supports_backend(
    const ASTExpression *expression, const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    ASTType left_type;
    ASTType right_type;
    size_t index;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
        case AST_EXPR_STRING:
        case AST_EXPR_FLOAT:
            return true;
        case AST_EXPR_IDENTIFIER:
            return true;
        case AST_EXPR_CALL:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!main_expression_supports_backend(expression->call.arguments[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_EXPR_CAST:
            return main_expression_supports_backend(expression->cast.operand, program, semantic, error);
        case AST_EXPR_UNARY:
            return main_expression_supports_backend(expression->unary.operand, program, semantic, error);
        case AST_EXPR_BINARY:
            if (!main_expression_supports_backend(expression->binary.left, program, semantic, error) ||
                !main_expression_supports_backend(expression->binary.right, program, semantic, error)) {
                return false;
            }

            if ((expression->binary.op == AST_BINARY_GT ||
                 expression->binary.op == AST_BINARY_LT ||
                 expression->binary.op == AST_BINARY_EQ ||
                 expression->binary.op == AST_BINARY_NE ||
                 expression->binary.op == AST_BINARY_GE ||
                 expression->binary.op == AST_BINARY_LE ||
                 expression->binary.op == AST_BINARY_ADD ||
                 expression->binary.op == AST_BINARY_SUB ||
                 expression->binary.op == AST_BINARY_MUL) &&
                infer_main_expression_type(expression->binary.left, program, semantic, &left_type) &&
                infer_main_expression_type(expression->binary.right, program, semantic, &right_type) &&
                (left_type == AST_TYPE_FLUTUANTE || right_type == AST_TYPE_FLUTUANTE)) {
                return true;
            }

            return true;
        case AST_EXPR_INDEX:
            return main_expression_supports_backend(expression->index_access.index, program, semantic, error) &&
                   main_expression_supports_backend(expression->index_access.index2, program, semantic, error);
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

static bool main_command_supports_backend(
    const ASTCommand *command, const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    ASTType type;
    size_t index;

    if (command == NULL) {
        return true;
    }

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            return main_expression_supports_backend(command->assignment.expression, program, semantic, error);
        case AST_COMMAND_READ:
            return true;
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            return main_expression_supports_backend(command->write.expression, program, semantic, error);
        case AST_COMMAND_CALL:
            for (index = 0; index < command->call_command.call.argument_count; ++index) {
                if (!main_expression_supports_backend(
                        command->call_command.call.arguments[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_RETURN:
            return true;
        case AST_COMMAND_IF:
            if (infer_main_expression_type(command->if_command.condition, program, semantic, &type) &&
                type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(
                    error,
                    command->if_command.condition != NULL ? command->if_command.condition->line : 0,
                    command->if_command.condition != NULL ? command->if_command.condition->column : 0);
            }
            if (!main_expression_supports_backend(command->if_command.condition, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->if_command.then_count; ++index) {
                if (!main_command_supports_backend(
                        &command->if_command.then_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            for (index = 0; index < command->if_command.else_count; ++index) {
                if (!main_command_supports_backend(
                        &command->if_command.else_commands[index], program, semantic, error)) {
                    return false;
                }
            }
            return true;
        case AST_COMMAND_WHILE:
            if (infer_main_expression_type(command->while_command.condition, program, semantic, &type) &&
                type == AST_TYPE_FLUTUANTE) {
                return fail_float_main_codegen(
                    error,
                    command->while_command.condition != NULL ? command->while_command.condition->line : 0,
                    command->while_command.condition != NULL ? command->while_command.condition->column : 0);
            }
            if (!main_expression_supports_backend(command->while_command.condition, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->while_command.body_count; ++index) {
                if (!main_command_supports_backend(
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
            if (!main_expression_supports_backend(command->for_command.start_expression, program, semantic, error) ||
                !main_expression_supports_backend(command->for_command.end_expression, program, semantic, error) ||
                !main_expression_supports_backend(command->for_command.step_expression, program, semantic, error)) {
                return false;
            }
            for (index = 0; index < command->for_command.body_count; ++index) {
                if (!main_command_supports_backend(
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

static bool main_program_supports_backend(
    const ASTProgram *program, const SemanticInfo *semantic, CompilerError *error) {
    size_t index;

    if (program == NULL) {
        return false;
    }

    for (index = 0; index < program->command_count; ++index) {
        if (!main_command_supports_backend(&program->commands[index], program, semantic, error)) {
            return false;
        }
    }

    return true;
}

static bool collect_string_literals_in_expression(StringLiteralList *list, size_t *next_label_id, const ASTExpression *expression) {
    size_t index;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_STRING:
            return string_literal_list_append(list, expression, (*next_label_id)++);
        case AST_EXPR_INDEX:
            return collect_string_literals_in_expression(list, next_label_id, expression->index_access.index) &&
                   collect_string_literals_in_expression(list, next_label_id, expression->index_access.index2);
        case AST_EXPR_CAST:
            return collect_string_literals_in_expression(list, next_label_id, expression->cast.operand);
        case AST_EXPR_CALL:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!collect_string_literals_in_expression(list, next_label_id, expression->call.arguments[index])) {
                    return false;
                }
            }
            return true;
        case AST_EXPR_UNARY:
            return collect_string_literals_in_expression(list, next_label_id, expression->unary.operand);
        case AST_EXPR_BINARY:
            return collect_string_literals_in_expression(list, next_label_id, expression->binary.left) &&
                   collect_string_literals_in_expression(list, next_label_id, expression->binary.right);
        default:
            return true;
    }
}

static bool collect_string_literals_in_commands(
    StringLiteralList *list, size_t *next_label_id, const ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        const ASTCommand *command = &commands[index];
        switch (command->type) {
            case AST_COMMAND_ASSIGNMENT:
                if (!collect_string_literals_in_expression(list, next_label_id, command->assignment.expression)) {
                    return false;
                }
                if (command->assignment.target.type == AST_TARGET_INDEXED &&
                    (!collect_string_literals_in_expression(list, next_label_id, command->assignment.target.indexed.index) ||
                     !collect_string_literals_in_expression(list, next_label_id, command->assignment.target.indexed.index2))) {
                    return false;
                }
                break;
            case AST_COMMAND_WRITE:
            case AST_COMMAND_WRITELN:
                if (!collect_string_literals_in_expression(list, next_label_id, command->write.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_CALL:
                if (!collect_string_literals_in_expression(list, next_label_id, (const ASTExpression *)&(ASTExpression){
                        .type = AST_EXPR_CALL, .call = command->call_command.call})) {
                    return false;
                }
                break;
            case AST_COMMAND_RETURN:
                if (!collect_string_literals_in_expression(list, next_label_id, command->return_command.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_IF:
                if (!collect_string_literals_in_expression(list, next_label_id, command->if_command.condition) ||
                    !collect_string_literals_in_commands(list, next_label_id, command->if_command.then_commands, command->if_command.then_count) ||
                    !collect_string_literals_in_commands(list, next_label_id, command->if_command.else_commands, command->if_command.else_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_WHILE:
                if (!collect_string_literals_in_expression(list, next_label_id, command->while_command.condition) ||
                    !collect_string_literals_in_commands(list, next_label_id, command->while_command.body_commands, command->while_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_FOR:
                if (!collect_string_literals_in_expression(list, next_label_id, command->for_command.start_expression) ||
                    !collect_string_literals_in_expression(list, next_label_id, command->for_command.end_expression) ||
                    !collect_string_literals_in_expression(list, next_label_id, command->for_command.step_expression) ||
                    !collect_string_literals_in_commands(list, next_label_id, command->for_command.body_commands, command->for_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_READ:
                if (command->read.target_type == AST_TARGET_INDEXED &&
                    (!collect_string_literals_in_expression(list, next_label_id, command->read.index) ||
                     !collect_string_literals_in_expression(list, next_label_id, command->read.index2))) {
                    return false;
                }
                break;
        }
    }

    return true;
}

static bool collect_string_literals_in_program(StringLiteralList *list, const ASTProgram *program) {
    size_t index;
    size_t next_label_id = 0;

    if (!collect_string_literals_in_commands(list, &next_label_id, program->commands, program->command_count)) {
        return false;
    }

    for (index = 0; index < program->procedure_count; ++index) {
        if (!collect_string_literals_in_commands(
                list, &next_label_id, program->procedures[index].commands, program->procedures[index].command_count)) {
            return false;
        }
    }

    return true;
}

static bool collect_string_return_temps_in_expression(
    StringReturnTempList *list,
    size_t *next_label_id,
    const SemanticInfo *semantic,
    const ASTExpression *expression) {
    size_t index;
    const ProcedureSignature *signature;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_INDEX:
            return collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->index_access.index) &&
                   collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->index_access.index2);
        case AST_EXPR_CAST:
            return collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->cast.operand);
        case AST_EXPR_CALL:
            signature = find_procedure_signature(semantic, expression->call.name);
            if (signature != NULL && signature->return_type == AST_TYPE_STRING) {
                size_t i;

                for (i = 0; i < list->count; ++i) {
                    if (list->items[i].call == &expression->call) {
                        goto recurse_args;
                    }
                }

                if (!string_return_temp_list_append(list, &expression->call, (*next_label_id)++, signature->return_capacity)) {
                    return false;
                }
            }
recurse_args:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!collect_string_return_temps_in_expression(
                        list, next_label_id, semantic, expression->call.arguments[index])) {
                    return false;
                }
            }
            return true;
        case AST_EXPR_UNARY:
            return collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->unary.operand);
        case AST_EXPR_BINARY:
            return collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->binary.left) &&
                   collect_string_return_temps_in_expression(list, next_label_id, semantic, expression->binary.right);
        default:
            return true;
    }
}

static bool collect_string_return_temps_in_commands(
    StringReturnTempList *list, size_t *next_label_id, const SemanticInfo *semantic,
    const ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        const ASTCommand *command = &commands[index];

        switch (command->type) {
            case AST_COMMAND_ASSIGNMENT:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->assignment.expression)) {
                    return false;
                }
                if (command->assignment.target.type == AST_TARGET_INDEXED &&
                    (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->assignment.target.indexed.index) ||
                     !collect_string_return_temps_in_expression(list, next_label_id, semantic, command->assignment.target.indexed.index2))) {
                    return false;
                }
                break;
            case AST_COMMAND_WRITE:
            case AST_COMMAND_WRITELN:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->write.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_CALL:
                {
                    const ProcedureSignature *signature =
                        find_procedure_signature(semantic, command->call_command.call.name);
                    size_t exists_index;
                    size_t arg_index;

                    if (signature != NULL && signature->return_type == AST_TYPE_STRING) {
                        bool exists = false;
                        for (exists_index = 0; exists_index < list->count; ++exists_index) {
                            if (list->items[exists_index].call == &command->call_command.call) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists &&
                            !string_return_temp_list_append(
                                list, &command->call_command.call, (*next_label_id)++, signature->return_capacity)) {
                            return false;
                        }
                    }

                    for (arg_index = 0; arg_index < command->call_command.call.argument_count; ++arg_index) {
                        if (!collect_string_return_temps_in_expression(
                                list, next_label_id, semantic, command->call_command.call.arguments[arg_index])) {
                            return false;
                        }
                    }
                }
                break;
            case AST_COMMAND_RETURN:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->return_command.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_IF:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->if_command.condition) ||
                    !collect_string_return_temps_in_commands(
                        list, next_label_id, semantic, command->if_command.then_commands, command->if_command.then_count) ||
                    !collect_string_return_temps_in_commands(
                        list, next_label_id, semantic, command->if_command.else_commands, command->if_command.else_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_WHILE:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->while_command.condition) ||
                    !collect_string_return_temps_in_commands(
                        list, next_label_id, semantic, command->while_command.body_commands, command->while_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_FOR:
                if (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->for_command.start_expression) ||
                    !collect_string_return_temps_in_expression(list, next_label_id, semantic, command->for_command.end_expression) ||
                    !collect_string_return_temps_in_expression(list, next_label_id, semantic, command->for_command.step_expression) ||
                    !collect_string_return_temps_in_commands(
                        list, next_label_id, semantic, command->for_command.body_commands, command->for_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_READ:
                if (command->read.target_type == AST_TARGET_INDEXED &&
                    (!collect_string_return_temps_in_expression(list, next_label_id, semantic, command->read.index) ||
                     !collect_string_return_temps_in_expression(list, next_label_id, semantic, command->read.index2))) {
                    return false;
                }
                break;
        }
    }

    return true;
}

static bool collect_string_return_temps_in_program(
    StringReturnTempList *list, const SemanticInfo *semantic, const ASTProgram *program) {
    size_t index;
    size_t next_label_id = 0;

    if (!collect_string_return_temps_in_commands(
            list, &next_label_id, semantic, program->commands, program->command_count)) {
        return false;
    }

    for (index = 0; index < program->procedure_count; ++index) {
        if (!collect_string_return_temps_in_commands(
                list,
                &next_label_id,
                semantic,
                program->procedures[index].commands,
                program->procedures[index].command_count)) {
            return false;
        }
    }

    return true;
}


static bool collect_float_literals_in_expression(FloatLiteralList *list, size_t *next_label_id, const ASTExpression *expression) {
    size_t index;

    if (expression == NULL) {
        return true;
    }

    switch (expression->type) {
        case AST_EXPR_FLOAT:
            return float_literal_list_append(list, expression, (*next_label_id)++);
        case AST_EXPR_CALL:
            for (index = 0; index < expression->call.argument_count; ++index) {
                if (!collect_float_literals_in_expression(list, next_label_id, expression->call.arguments[index])) {
                    return false;
                }
            }
            return true;
        case AST_EXPR_CAST:
            return collect_float_literals_in_expression(list, next_label_id, expression->cast.operand);
        case AST_EXPR_UNARY:
            return collect_float_literals_in_expression(list, next_label_id, expression->unary.operand);
        case AST_EXPR_BINARY:
            return collect_float_literals_in_expression(list, next_label_id, expression->binary.left) &&
                   collect_float_literals_in_expression(list, next_label_id, expression->binary.right);
        case AST_EXPR_INDEX:
            return collect_float_literals_in_expression(list, next_label_id, expression->index_access.index) &&
                   collect_float_literals_in_expression(list, next_label_id, expression->index_access.index2);
        case AST_EXPR_INT:
        case AST_EXPR_STRING:
        case AST_EXPR_IDENTIFIER:
            return true;
        default:
            return false;
    }
}

static bool collect_float_literals_in_commands(
    FloatLiteralList *list,
    size_t *next_label_id,
    const ASTCommand *commands,
    size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        const ASTCommand *command = &commands[index];

        switch (command->type) {
            case AST_COMMAND_ASSIGNMENT:
                if (!collect_float_literals_in_expression(list, next_label_id, command->assignment.expression) ||
                    (command->assignment.target.type == AST_TARGET_INDEXED &&
                     (!collect_float_literals_in_expression(
                          list, next_label_id, command->assignment.target.indexed.index) ||
                      !collect_float_literals_in_expression(
                          list, next_label_id, command->assignment.target.indexed.index2)))) {
                    return false;
                }
                break;
            case AST_COMMAND_WRITE:
            case AST_COMMAND_WRITELN:
                if (!collect_float_literals_in_expression(list, next_label_id, command->write.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_CALL:
                for (index = 0; index < command->call_command.call.argument_count; ++index) {
                    if (!collect_float_literals_in_expression(
                            list, next_label_id, command->call_command.call.arguments[index])) {
                        return false;
                    }
                }
                break;
            case AST_COMMAND_RETURN:
                if (!collect_float_literals_in_expression(list, next_label_id, command->return_command.expression)) {
                    return false;
                }
                break;
            case AST_COMMAND_IF:
                if (!collect_float_literals_in_expression(list, next_label_id, command->if_command.condition) ||
                    !collect_float_literals_in_commands(
                        list, next_label_id, command->if_command.then_commands, command->if_command.then_count) ||
                    !collect_float_literals_in_commands(
                        list, next_label_id, command->if_command.else_commands, command->if_command.else_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_WHILE:
                if (!collect_float_literals_in_expression(list, next_label_id, command->while_command.condition) ||
                    !collect_float_literals_in_commands(
                        list, next_label_id, command->while_command.body_commands, command->while_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_FOR:
                if (!collect_float_literals_in_expression(list, next_label_id, command->for_command.start_expression) ||
                    !collect_float_literals_in_expression(list, next_label_id, command->for_command.end_expression) ||
                    !collect_float_literals_in_expression(list, next_label_id, command->for_command.step_expression) ||
                    !collect_float_literals_in_commands(
                        list, next_label_id, command->for_command.body_commands, command->for_command.body_count)) {
                    return false;
                }
                break;
            case AST_COMMAND_READ:
                if (command->read.target_type == AST_TARGET_INDEXED &&
                    (!collect_float_literals_in_expression(list, next_label_id, command->read.index) ||
                     !collect_float_literals_in_expression(list, next_label_id, command->read.index2))) {
                    return false;
                }
                break;
            default:
                return false;
        }
    }

    return true;
}

static bool collect_float_literals_in_program(FloatLiteralList *list, const ASTProgram *program) {
    size_t index;
    size_t next_label_id = 0;

    if (!collect_float_literals_in_commands(list, &next_label_id, program->commands, program->command_count)) {
        return false;
    }

    for (index = 0; index < program->procedure_count; ++index) {
        if (!collect_float_literals_in_commands(
                list, &next_label_id, program->procedures[index].commands, program->procedures[index].command_count)) {
            return false;
        }
    }

    return true;
}

static bool builder_append_string_literal_bytes(StringBuilder *builder, const char *literal) {
    size_t index;

    for (index = 0; literal[index] != '\0'; ++index) {
        unsigned char ch = (unsigned char)literal[index];
        if (index > 0 && !builder_append(builder, ", ")) {
            return false;
        }
        if (ch == '\'' || ch == '\\' || ch < 32 || ch > 126) {
            if (!builder_appendf(builder, "%u", (unsigned int)ch)) {
                return false;
            }
        } else if (!builder_appendf(builder, "'%c'", (char)ch)) {
            return false;
        }
    }

    if (literal[0] != '\0' && !builder_append(builder, ", ")) {
        return false;
    }

    return builder_append(builder, "0");
}

static bool builder_append_nasm_byte_literal(StringBuilder *builder, unsigned char ch) {
    if (ch == '\'' || ch == '\\' || ch < 32 || ch > 126) {
        return builder_appendf(builder, "%u", (unsigned int)ch);
    }

    return builder_appendf(builder, "'%c'", (char)ch);
}

static bool emit_string_literal_declarations(StringBuilder *builder, const StringLiteralList *list) {
    size_t index;

    for (index = 0; index < list->count; ++index) {
        if (!builder_appendf(builder, "_strlit_%zu db ", list->items[index].label_id) ||
            !builder_append_string_literal_bytes(builder, list->items[index].expression->string_value) ||
            !builder_append(builder, "\n")) {
            return false;
        }
    }

    return true;
}

static bool emit_string_return_temp_declarations(StringBuilder *builder, const StringReturnTempList *list) {
    size_t index;

    for (index = 0; index < list->count; ++index) {
        if (!builder_appendf(builder, "_retstr_%zu times %zu db 0\n", list->items[index].label_id, list->items[index].capacity)) {
            return false;
        }
    }

    return true;
}

static bool emit_float_literal_declarations(StringBuilder *builder, const FloatLiteralList *list) {
    size_t index;
    char literal[64];
    int length;

    for (index = 0; index < list->count; ++index) {
        length = snprintf(literal, sizeof(literal), "%.17g", list->items[index].expression->float_value);
        if (length < 0 || (size_t)length >= sizeof(literal)) {
            return false;
        }

        if (strchr(literal, '.') == NULL && strchr(literal, 'e') == NULL && strchr(literal, 'E') == NULL) {
            if ((size_t)length + 2 >= sizeof(literal)) {
                return false;
            }
            literal[length] = '.';
            literal[length + 1] = '0';
            literal[length + 2] = '\0';
        }

        if (!builder_appendf(builder, "_flt_%zu dq %s\n", list->items[index].label_id, literal)) {
            return false;
        }
    }

    return true;
}

static bool generate_string_literal_address(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    size_t index;

    if (context->string_literals != NULL) {
        for (index = 0; index < context->string_literals->count; ++index) {
            if (context->string_literals->items[index].expression == expression) {
                return builder_appendf(context->builder, "    mov eax, _strlit_%zu\n", context->string_literals->items[index].label_id);
            }
        }
    }

    return fail_codegen_internal(
        error,
        expression != NULL ? expression->line : 0,
        expression != NULL ? expression->column : 0,
        "Internal error: missing string literal label mapping.");
}

static bool generate_float_literal_load(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    size_t index;

    if (context->float_literals != NULL) {
        for (index = 0; index < context->float_literals->count; ++index) {
            if (context->float_literals->items[index].expression == expression) {
                return builder_appendf(context->builder, "    fld qword [_flt_%zu]\n", context->float_literals->items[index].label_id);
            }
        }
    }

    return fail_codegen_internal(
        error,
        expression != NULL ? expression->line : 0,
        expression != NULL ? expression->column : 0,
        "Internal error: missing float literal label mapping.");
}

static bool append_promote_eax_to_st0(StringBuilder *builder) {
    return builder_append(builder, "    mov dword [tmp_int], eax\n") &&
           builder_append(builder, "    fild dword [tmp_int]\n");
}

static bool append_truncate_st0_to_eax(StringBuilder *builder) {
    return builder_append(builder, "    fisttp dword [tmp_int]\n") &&
           builder_append(builder, "    mov eax, dword [tmp_int]\n");
}

static bool generate_float_load_from_name(CodegenContext *context, const char *name) {
    size_t parameter_index;
    const ASTParameter *parameter;
    size_t local_offset;

    parameter = find_context_parameter(context->parameters, context->parameter_count, name, &parameter_index);
    if (parameter != NULL && parameter->type == AST_TYPE_FLUTUANTE && parameter->storage == AST_STORAGE_SCALAR) {
        return builder_appendf(
            context->builder,
            "    fld qword [ebp+%zu]\n",
            parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    local_offset = local_declaration_base_offset_by_name(context->proc_locals, context->proc_local_count, name);
    if (local_offset > 0 &&
        find_context_variable_type(
            NULL, 0, context->proc_locals, context->proc_local_count, context->program, name) == AST_TYPE_FLUTUANTE) {
        return builder_appendf(context->builder, "    fld qword [ebp-%zu]\n", local_offset);
    }

    return builder_append(context->builder, "    fld qword [") &&
           builder_append_user_symbol_name(context->builder, context->for_loop_ids, name) &&
           builder_append(context->builder, "]\n");
}

static bool generate_float_store_to_name(CodegenContext *context, const char *name) {
    size_t parameter_index;
    const ASTParameter *parameter;
    size_t local_offset;

    parameter = find_context_parameter(context->parameters, context->parameter_count, name, &parameter_index);
    if (parameter != NULL && parameter->type == AST_TYPE_FLUTUANTE && parameter->storage == AST_STORAGE_SCALAR) {
        return builder_appendf(
            context->builder,
            "    fstp qword [ebp+%zu]\n",
            parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    local_offset = local_declaration_base_offset_by_name(context->proc_locals, context->proc_local_count, name);
    if (local_offset > 0 &&
        find_context_variable_type(
            NULL, 0, context->proc_locals, context->proc_local_count, context->program, name) == AST_TYPE_FLUTUANTE) {
        return builder_appendf(context->builder, "    fstp qword [ebp-%zu]\n", local_offset);
    }

    return builder_append(context->builder, "    fstp qword [") &&
           builder_append_user_symbol_name(context->builder, context->for_loop_ids, name) &&
           builder_append(context->builder, "]\n");
}

static bool generate_float_index_load(CodegenContext *context, const ASTIndexedAccess *access, CompilerError *error) {
    size_t base_offset;
    ASTType base_type;
    size_t parameter_index;
    const ASTParameter *parameter;

    base_type = find_context_variable_type(
        context->parameters,
        context->parameter_count,
        context->proc_locals,
        context->proc_local_count,
        context->program,
        access->name);

    if (base_type != AST_TYPE_FLUTUANTE) {
        return fail_codegen_internal(error, access->line, access->column, "Internal error: float index load on non-float vector.");
    }

    parameter = find_context_parameter(context->parameters, context->parameter_count, access->name, &parameter_index);
    if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
        return generate_linearized_index(context, access, error) &&
               builder_append(context->builder, "    imul eax, 8\n") &&
               builder_append(context->builder, "    lea edx, [ebp + eax]\n") &&
               builder_appendf(
                   context->builder,
                   "    fld qword [edx + %zu]\n",
                   parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    if (context->proc_locals != NULL) {
        base_offset = local_declaration_base_offset_by_name(
            context->proc_locals, context->proc_local_count, access->name);
        if (base_offset > 0) {
            return generate_linearized_index(context, access, error) &&
                   builder_append(context->builder, "    imul eax, 8\n") &&
                   builder_append(context->builder, "    neg eax\n") &&
                   builder_appendf(context->builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                   builder_append(context->builder, "    fld qword [edx]\n");
        }
    }

    return generate_linearized_index(context, access, error) &&
           builder_append(context->builder, "    imul eax, 8\n") &&
           builder_append(context->builder, "    lea edx, [") &&
           builder_append_user_symbol_name(context->builder, context->for_loop_ids, access->name) &&
           builder_append(context->builder, " + eax]\n") &&
           builder_append(context->builder, "    fld qword [edx]\n");
}

static bool generate_float_index_store(CodegenContext *context, const ASTIndexedAccess *access, CompilerError *error) {
    size_t base_offset;
    ASTType base_type;
    size_t parameter_index;
    const ASTParameter *parameter;

    base_type = find_context_variable_type(
        context->parameters,
        context->parameter_count,
        context->proc_locals,
        context->proc_local_count,
        context->program,
        access->name);

    if (base_type != AST_TYPE_FLUTUANTE) {
        return fail_codegen_internal(error, access->line, access->column, "Internal error: float index store on non-float vector.");
    }

    parameter = find_context_parameter(context->parameters, context->parameter_count, access->name, &parameter_index);
    if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
        return generate_linearized_index(context, access, error) &&
               builder_append(context->builder, "    imul eax, 8\n") &&
               builder_append(context->builder, "    lea edx, [ebp + eax]\n") &&
               builder_appendf(
                   context->builder,
                   "    fstp qword [edx + %zu]\n",
                   parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    if (context->proc_locals != NULL) {
        base_offset = local_declaration_base_offset_by_name(
            context->proc_locals, context->proc_local_count, access->name);
        if (base_offset > 0) {
            return generate_linearized_index(context, access, error) &&
                   builder_append(context->builder, "    imul eax, 8\n") &&
                   builder_append(context->builder, "    neg eax\n") &&
                   builder_appendf(context->builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                   builder_append(context->builder, "    fstp qword [edx]\n");
        }
    }

    return generate_linearized_index(context, access, error) &&
           builder_append(context->builder, "    imul eax, 8\n") &&
           builder_append(context->builder, "    lea edx, [") &&
           builder_append_user_symbol_name(context->builder, context->for_loop_ids, access->name) &&
           builder_append(context->builder, " + eax]\n") &&
           builder_append(context->builder, "    fstp qword [edx]\n");
}

static bool generate_string_literal_copy(
    CodegenContext *context, const char *target_name, const char *literal) {
    StringBuilder *builder = context->builder;
    size_t i, j, len;
    size_t parameter_index;
    const ASTParameter *parameter;

    parameter = find_context_parameter(context->parameters, context->parameter_count, target_name, &parameter_index);
    if (parameter != NULL && parameter->type == AST_TYPE_STRING && parameter->storage == AST_STORAGE_INDEXED) {
        len = (literal != NULL) ? strlen(literal) : 0;

        if (!builder_appendf(
                builder,
                "    mov edx, dword [ebp+%zu]\n",
                parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index))) {
            return false;
        }

        if (len == 0) {
            return builder_append(builder, "    mov byte [edx], 0\n");
        }

        if (!builder_append(builder, "    mov byte [edx], ") ||
            !builder_append_nasm_byte_literal(builder, (unsigned char)literal[0]) ||
            !builder_append(builder, "\n")) {
            return false;
        }

        for (i = 1; i < len; i++) {
            if (!builder_appendf(builder, "    mov byte [edx+%zu], ", i) ||
                !builder_append_nasm_byte_literal(builder, (unsigned char)literal[i]) ||
                !builder_append(builder, "\n")) {
                return false;
            }
        }

        return builder_appendf(builder, "    mov byte [edx+%zu], 0\n", len);
    }

    /* Local string targets: emit frame-based byte stores. */
    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, target_name) == 0) {
                size_t base_offset = local_declaration_frame_offset(context->proc_locals, i);
                len = (literal != NULL) ? strlen(literal) : 0;

                if (len == 0) {
                    return builder_appendf(builder, "    mov byte [ebp-%zu], 0\n", base_offset);
                }

                /* byte 0 at [ebp-base_offset], byte j at [ebp-(base_offset-j)] */
                if (!builder_appendf(builder, "    mov byte [ebp-%zu], ", base_offset) ||
                    !builder_append_nasm_byte_literal(builder, (unsigned char)literal[0]) ||
                    !builder_append(builder, "\n")) {
                    return false;
                }
                for (j = 1; j < len; j++) {
                    if (!builder_appendf(builder, "    mov byte [ebp-%zu], ", base_offset - j) ||
                        !builder_append_nasm_byte_literal(builder, (unsigned char)literal[j]) ||
                        !builder_append(builder, "\n")) {
                        return false;
                    }
                }
                return builder_appendf(builder, "    mov byte [ebp-%zu], 0\n", base_offset - len);
            }
        }
    }

    len = (literal != NULL) ? strlen(literal) : 0;

    if (len == 0) {
        return builder_appendf(builder, "    mov byte [%s], 0\n", target_name);
    }

    if (!builder_appendf(builder, "    mov byte [%s], ", target_name) ||
        !builder_append_nasm_byte_literal(builder, (unsigned char)literal[0]) ||
        !builder_append(builder, "\n")) {
        return false;
    }

    for (i = 1; i < len; i++) {
        if (!builder_appendf(builder, "    mov byte [%s+%zu], ", target_name, i) ||
            !builder_append_nasm_byte_literal(builder, (unsigned char)literal[i]) ||
            !builder_append(builder, "\n")) {
            return false;
        }
    }

    return builder_appendf(builder, "    mov byte [%s+%zu], 0\n", target_name, len);
}

static bool generate_string_address(CodegenContext *context, const char *var_name) {
    size_t i;
    size_t parameter_index;
    const ASTParameter *parameter;

    parameter = find_context_parameter(context->parameters, context->parameter_count, var_name, &parameter_index);
    if (parameter != NULL && parameter->type == AST_TYPE_STRING && parameter->storage == AST_STORAGE_INDEXED) {
        return builder_appendf(
            context->builder,
            "    mov eax, dword [ebp+%zu]\n",
            parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, var_name) == 0) {
                size_t base_offset = local_declaration_frame_offset(context->proc_locals, i);
                return builder_appendf(context->builder, "    lea eax, [ebp-%zu]\n", base_offset);
            }
        }
    }

    return builder_appendf(context->builder, "    mov eax, %s\n", var_name);
}

static bool generate_aggregate_address(CodegenContext *context, const char *name) {
    size_t i;
    size_t parameter_index;
    const ASTParameter *parameter;

    if (context->proc_locals != NULL) {
        for (i = 0; i < context->proc_local_count; i++) {
            if (strcmp(context->proc_locals[i].name, name) == 0) {
                size_t base_offset = local_declaration_frame_offset(context->proc_locals, i);
                return builder_appendf(context->builder, "    lea eax, [ebp-%zu]\n", base_offset);
            }
        }
    }

    parameter = find_context_parameter(context->parameters, context->parameter_count, name, &parameter_index);
    if (parameter != NULL && parameter->type == AST_TYPE_STRING && parameter->storage == AST_STORAGE_INDEXED) {
        return builder_appendf(
            context->builder,
            "    mov eax, dword [ebp+%zu]\n",
            parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index));
    }

    return builder_appendf(context->builder, "    mov eax, %s\n", name);
}

static bool generate_string_expression_address(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    const ProcedureSignature *signature;

    if (expression == NULL) {
        return false;
    }

    switch (expression->type) {
        case AST_EXPR_STRING:
            return generate_string_literal_address(context, expression, error);
        case AST_EXPR_IDENTIFIER:
            return generate_string_address(context, expression->identifier);
        case AST_EXPR_CALL:
            signature = find_procedure_signature(context->semantic, expression->call.name);
            if (signature == NULL || signature->return_type != AST_TYPE_STRING) {
                return false;
            }
            return generate_call(context, &expression->call, error);
        default:
            return false;
    }
}

static bool append_copy_string_from_eax_to_edx(StringBuilder *builder, size_t label_id) {
    return builder_append(builder, "    mov esi, eax\n") &&
           builder_append(builder, "    mov edi, edx\n") &&
           builder_appendf(builder, ".Lcopystr%zu:\n", label_id) &&
           builder_append(builder, "    mov al, byte [esi]\n") &&
           builder_append(builder, "    mov byte [edi], al\n") &&
           builder_append(builder, "    inc esi\n") &&
           builder_append(builder, "    inc edi\n") &&
           builder_append(builder, "    test al, al\n") &&
           builder_appendf(builder, "    jne .Lcopystr%zu\n", label_id);
}

static bool generate_string_assignment(
    CodegenContext *context, const char *target_name, const ASTExpression *expression, CompilerError *error) {
    if (expression != NULL && expression->type == AST_EXPR_STRING) {
        return generate_string_literal_copy(context, target_name, expression->string_value);
    }

    size_t label_id = context->next_label_id++;

    if (!generate_string_address(context, target_name) ||
        !builder_append(context->builder, "    push eax\n") ||
        !generate_string_expression_address(context, expression, error)) {
        return false;
    }

    if (!builder_append(context->builder, "    pop edx\n")) {
        return false;
    }

    return append_copy_string_from_eax_to_edx(context->builder, label_id);
}

static bool builder_append_print_string_helper(StringBuilder *builder) {
    return builder_append(
        builder,
        "\nprint_string:\n"
        "    mov esi, eax\n"
        "    xor edx, edx\n"
        ".print_string_len:\n"
        "    cmp byte [esi + edx], 0\n"
        "    je .print_string_write\n"
        "    inc edx\n"
        "    jmp .print_string_len\n"
        ".print_string_write:\n"
        "    test edx, edx\n"
        "    jz .print_string_done\n"
        "    mov ecx, esi\n"
        "    mov eax, 4\n"
        "    mov ebx, 1\n"
        "    int 0x80\n"
        ".print_string_done:\n"
        "    ret\n");
}

static bool builder_append_read_string_helper(StringBuilder *builder) {
    return builder_append(
        builder,
        "\nread_string:\n"
        "    push esi\n"
        "    mov esi, eax\n"
        "    mov ecx, eax\n"
        "    mov eax, 3\n"
        "    mov ebx, 0\n"
        "    int 0x80\n"
        "    test eax, eax\n"
        "    jle .read_string_empty\n"
        "    mov edx, eax\n"
        "    xor eax, eax\n"
        ".read_string_scan:\n"
        "    cmp eax, edx\n"
        "    jae .read_string_null\n"
        "    cmp byte [esi + eax], 10\n"
        "    je .read_string_null\n"
        "    cmp byte [esi + eax], 13\n"
        "    je .read_string_null\n"
        "    inc eax\n"
        "    jmp .read_string_scan\n"
        ".read_string_null:\n"
        "    mov byte [esi + eax], 0\n"
        "    pop esi\n"
        "    ret\n"
        ".read_string_empty:\n"
        "    mov byte [esi], 0\n"
        "    pop esi\n"
        "    ret\n");
}

static bool generate_float_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error);

static bool generate_float_comparison(
    CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    ASTType left_type;
    ASTType right_type;
    size_t label_id;
    const char *jump = NULL;

    if (expression == NULL || expression->type != AST_EXPR_BINARY) {
        return fail_codegen_internal(error, 0, 0, "Internal error: invalid float comparison expression.");
    }

    if (!infer_expression_type(context, expression->binary.left, &left_type) ||
        !infer_expression_type(context, expression->binary.right, &right_type)) {
        return fail_codegen_internal(error, expression->line, expression->column, "Internal error: missing float comparison type.");
    }

    if (left_type != AST_TYPE_FLUTUANTE && right_type != AST_TYPE_FLUTUANTE) {
        return fail_codegen_internal(error, expression->line, expression->column, "Internal error: float comparison without float operand.");
    }

    if (!generate_float_expression(context, expression->binary.left, error) ||
        !generate_float_expression(context, expression->binary.right, error) ||
        !builder_append(context->builder, "    fcomip st0, st1\n") ||
        !builder_append(context->builder, "    fstp st0\n") ||
        !builder_append(context->builder, "    mov eax, 0\n")) {
        return false;
    }

    switch (expression->binary.op) {
        case AST_BINARY_GT:
            jump = "jb";
            break;
        case AST_BINARY_LT:
            jump = "ja";
            break;
        case AST_BINARY_EQ:
            jump = "je";
            break;
        case AST_BINARY_NE:
            jump = "jne";
            break;
        case AST_BINARY_GE:
            jump = "jbe";
            break;
        case AST_BINARY_LE:
            jump = "jae";
            break;
        default:
            return fail_codegen_internal(error, expression->line, expression->column, "Internal error: unsupported float comparison operator.");
    }

    label_id = context->next_label_id++;
    return builder_appendf(context->builder, "    %s .Lfloat_true%zu\n", jump, label_id) &&
           builder_appendf(context->builder, "    jmp .Lfloat_end%zu\n", label_id) &&
           builder_appendf(context->builder, ".Lfloat_true%zu:\n", label_id) &&
           builder_append(context->builder, "    mov eax, 1\n") &&
           builder_appendf(context->builder, ".Lfloat_end%zu:\n", label_id);
}

static bool generate_float_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    ASTType expression_type;
    const ProcedureSignature *signature;

    if (expression == NULL || !infer_expression_type(context, expression, &expression_type)) {
        return fail_codegen_internal(error, 0, 0, "Internal error: missing float expression type.");
    }

    switch (expression->type) {
        case AST_EXPR_FLOAT:
            return generate_float_literal_load(context, expression, error);
        case AST_EXPR_INT:
            return builder_appendf(context->builder, "    mov dword [tmp_int], %d\n", expression->int_value) &&
                   builder_append(context->builder, "    fild dword [tmp_int]\n");
        case AST_EXPR_IDENTIFIER:
            if (expression_type == AST_TYPE_FLUTUANTE) {
                return generate_float_load_from_name(context, expression->identifier);
            }
            if (!generate_load_name(context, expression->identifier)) {
                return false;
            }
            return append_promote_eax_to_st0(context->builder);
        case AST_EXPR_INDEX:
            return generate_float_index_load(context, &expression->index_access, error);
        case AST_EXPR_CALL:
            signature = find_procedure_signature(context->semantic, expression->call.name);
            if (!generate_call(context, &expression->call, error)) {
                return false;
            }
            if (signature != NULL && signature->return_type == AST_TYPE_FLUTUANTE) {
                return true;
            }
            return append_promote_eax_to_st0(context->builder);
        case AST_EXPR_CAST:
            if (!infer_expression_type(context, expression->cast.operand, &expression_type)) {
                return false;
            }
            if (expression->cast.target_type == AST_TYPE_FLUTUANTE) {
                if (expression_type == AST_TYPE_FLUTUANTE) {
                    return generate_float_expression(context, expression->cast.operand, error);
                }

                return generate_expression(context, expression->cast.operand, error) &&
                       append_promote_eax_to_st0(context->builder);
            }

            if (expression_type == AST_TYPE_FLUTUANTE) {
                return generate_float_expression(context, expression->cast.operand, error) &&
                       append_truncate_st0_to_eax(context->builder) &&
                       append_promote_eax_to_st0(context->builder);
            }

            return generate_expression(context, expression->cast.operand, error) &&
                   append_promote_eax_to_st0(context->builder);
        case AST_EXPR_UNARY:
            if (expression->unary.op != AST_UNARY_NEGATE) {
                return fail_float_expression_codegen(error, expression->line, expression->column);
            }
            if (!generate_float_expression(context, expression->unary.operand, error)) {
                return false;
            }
            return builder_append(context->builder, "    fchs\n");
        case AST_EXPR_BINARY:
            if ((expression->binary.op == AST_BINARY_GT ||
                 expression->binary.op == AST_BINARY_LT ||
                expression->binary.op == AST_BINARY_EQ ||
                 expression->binary.op == AST_BINARY_NE ||
                 expression->binary.op == AST_BINARY_GE ||
                 expression->binary.op == AST_BINARY_LE) &&
                expression_type == AST_TYPE_INTEIRO) {
                return generate_float_comparison(context, expression, error);
            }

            if (expression->binary.op == AST_BINARY_DIV) {
                return generate_float_expression(context, expression->binary.left, error) &&
                       generate_float_expression(context, expression->binary.right, error) &&
                       builder_append(context->builder, "    fdivp st1, st0\n");
            }

            if (!generate_float_expression(context, expression->binary.left, error) ||
                !generate_float_expression(context, expression->binary.right, error)) {
                return false;
            }

            switch (expression->binary.op) {
                case AST_BINARY_ADD:
                    return builder_append(context->builder, "    faddp st1, st0\n");
                case AST_BINARY_SUB:
                    return builder_append(context->builder, "    fsubp st1, st0\n");
                case AST_BINARY_MUL:
                    return builder_append(context->builder, "    fmulp st1, st0\n");
                default:
                    return fail_float_expression_codegen(error, expression->line, expression->column);
            }
        default:
            return fail_float_expression_codegen(error, expression->line, expression->column);
    }
}

static bool generate_expression(CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    StringBuilder *builder = context->builder;
    ASTType expression_type;

    if (expression == NULL) {
        return false;
    }

    if (expression_needs_float_codegen(context, expression)) {
        ASTType inferred_type;

        if (infer_expression_type(context, expression, &inferred_type) &&
            inferred_type == AST_TYPE_INTEIRO &&
            expression->type == AST_EXPR_BINARY &&
            (expression->binary.op == AST_BINARY_GT ||
             expression->binary.op == AST_BINARY_LT ||
             expression->binary.op == AST_BINARY_EQ ||
             expression->binary.op == AST_BINARY_NE ||
             expression->binary.op == AST_BINARY_GE ||
             expression->binary.op == AST_BINARY_LE)) {
            return generate_float_comparison(context, expression, error);
        }

        if (!generate_float_expression(context, expression, error)) {
            return false;
        }

        if (infer_expression_type(context, expression, &inferred_type) &&
            inferred_type == AST_TYPE_INTEIRO) {
            return true;
        }

        return true;
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return builder_appendf(builder, "    mov eax, %d\n", expression->int_value);
        case AST_EXPR_STRING:
            return generate_string_literal_address(context, expression, error);
        case AST_EXPR_FLOAT:
            return fail_float_expression_codegen(error, expression->line, expression->column);
        case AST_EXPR_IDENTIFIER:
            return generate_load_name(context, expression->identifier);
        case AST_EXPR_CALL: {
            return generate_call(context, &expression->call, error);
        }
        case AST_EXPR_CAST:
            if (expression->cast.target_type == AST_TYPE_FLUTUANTE) {
                return fail_codegen_internal(
                    error,
                    expression->line,
                    expression->column,
                    "Internal error: float cast reached integer codegen path.");
            }

            if (infer_expression_type(context, expression->cast.operand, &expression_type) &&
                expression_type == AST_TYPE_FLUTUANTE) {
                return generate_float_expression(context, expression->cast.operand, error) &&
                       append_truncate_st0_to_eax(builder);
            }

            return generate_expression(context, expression->cast.operand, error);
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
        case AST_EXPR_INDEX: {
            const ASTIndexedAccess *access = &expression->index_access;
            size_t base_offset;
            ASTType indexed_base_type;
            size_t parameter_index;
            const ASTParameter *parameter;

            indexed_base_type = find_context_variable_type(
                context->parameters,
                context->parameter_count,
                context->proc_locals,
                context->proc_local_count,
                context->program,
                access->name);

            if (indexed_base_type == AST_TYPE_STRING) {
                parameter = find_context_parameter(
                    context->parameters, context->parameter_count, access->name, &parameter_index);
                if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
                    return generate_linearized_index(context, access, error) &&
                           builder_appendf(
                               builder,
                               "    mov edx, dword [ebp+%zu]\n",
                               parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index)) &&
                           builder_append(builder, "    add edx, eax\n") &&
                           builder_append(builder, "    movzx eax, byte [edx]\n");
                }

                if (context->proc_locals != NULL) {
                    base_offset = local_declaration_base_offset_by_name(
                        context->proc_locals, context->proc_local_count, access->name);
                    if (base_offset > 0) {
                        return generate_linearized_index(context, access, error) &&
                               builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                               builder_append(builder, "    movzx eax, byte [edx]\n");
                    }
                }

                return generate_linearized_index(context, access, error) &&
                       builder_append(builder, "    lea edx, [") &&
                       builder_append_user_symbol_name(builder, context->for_loop_ids, access->name) &&
                       builder_append(builder, " + eax]\n") &&
                       builder_append(builder, "    movzx eax, byte [edx]\n");
            }

            parameter = find_context_parameter(
                context->parameters, context->parameter_count, access->name, &parameter_index);
            if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
                return generate_linearized_index(context, access, error) &&
                       builder_append(builder, "    imul eax, 4\n") &&
                       builder_appendf(
                           builder,
                           "    mov edx, dword [ebp+%zu]\n",
                           parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index)) &&
                       builder_append(builder, "    lea edx, [edx + eax]\n") &&
                       builder_append(builder, "    mov eax, dword [edx]\n");
            }

            /* Local vector: elements stored descending from ebp (nums[0]=ebp-N, nums[1]=ebp-N-4...) */
            if (context->proc_locals != NULL) {
                base_offset = local_declaration_base_offset_by_name(
                    context->proc_locals, context->proc_local_count, access->name);
                if (base_offset > 0) {
                    return generate_linearized_index(context, access, error) &&
                           builder_append(builder, "    imul eax, 4\n") &&
                           builder_append(builder, "    neg eax\n") &&
                           builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                           builder_append(builder, "    mov eax, dword [edx]\n");
                }
            }

            /* Global vector: elements stored ascending from label address */
            return generate_linearized_index(context, access, error) &&
                   builder_append(builder, "    imul eax, 4\n") &&
                   builder_append(builder, "    lea edx, [") &&
                   builder_append_user_symbol_name(builder, context->for_loop_ids, access->name) &&
                   builder_append(builder, " + eax]\n") &&
                   builder_append(builder, "    mov eax, dword [edx]\n");
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
            ASTType target_type;
            ASTType expression_type;

            if (command->assignment.target.type == AST_TARGET_INDEXED) {
                const ASTIndexedAccess *access = &command->assignment.target.indexed;
                size_t base_offset;
                ASTType indexed_base_type;
                size_t parameter_index;
                const ASTParameter *parameter;

                indexed_base_type = find_context_variable_type(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    access->name);

                if (indexed_base_type == AST_TYPE_FLUTUANTE) {
                    return generate_float_expression(context, command->assignment.expression, error) &&
                           generate_float_index_store(context, access, error);
                }

                /* Evaluate value first so address in edx is not clobbered by the expression. */
                if (!generate_expression(context, command->assignment.expression, error)) {
                    return false;
                }
                if (!builder_append(builder, "    push eax\n")) {
                    return false;
                }

                if (indexed_base_type == AST_TYPE_STRING) {
                    parameter = find_context_parameter(
                        context->parameters, context->parameter_count, access->name, &parameter_index);
                    if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
                        return generate_linearized_index(context, access, error) &&
                               builder_appendf(
                                   builder,
                                   "    mov edx, dword [ebp+%zu]\n",
                                   parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index)) &&
                               builder_append(builder, "    add edx, eax\n") &&
                               builder_append(builder, "    pop eax\n") &&
                               builder_append(builder, "    mov byte [edx], al\n");
                    }

                    if (context->proc_locals != NULL) {
                        base_offset = local_declaration_base_offset_by_name(
                            context->proc_locals, context->proc_local_count, access->name);
                        if (base_offset > 0) {
                            if (!generate_linearized_index(context, access, error) ||
                                !builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset)) {
                                return false;
                            }
                            return builder_append(builder, "    pop eax\n") &&
                                   builder_append(builder, "    mov byte [edx], al\n");
                        }
                    }

                    return generate_linearized_index(context, access, error) &&
                           builder_append(builder, "    lea edx, [") &&
                           builder_append_user_symbol_name(builder, context->for_loop_ids, access->name) &&
                           builder_append(builder, " + eax]\n") &&
                           builder_append(builder, "    pop eax\n") &&
                           builder_append(builder, "    mov byte [edx], al\n");
                }

                parameter = find_context_parameter(
                    context->parameters, context->parameter_count, access->name, &parameter_index);
                if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
                    return generate_linearized_index(context, access, error) &&
                           builder_append(builder, "    imul eax, 4\n") &&
                           builder_appendf(
                               builder,
                               "    mov edx, dword [ebp+%zu]\n",
                               parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index)) &&
                           builder_append(builder, "    lea edx, [edx + eax]\n") &&
                           builder_append(builder, "    pop eax\n") &&
                           builder_append(builder, "    mov dword [edx], eax\n");
                }

                if (context->proc_locals != NULL) {
                    base_offset = local_declaration_base_offset_by_name(
                        context->proc_locals, context->proc_local_count, access->name);
                    if (base_offset > 0) {
                        if (!generate_linearized_index(context, access, error) ||
                            !builder_append(builder, "    imul eax, 4\n") ||
                            !builder_append(builder, "    neg eax\n") ||
                            !builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset)) {
                            return false;
                        }
                        return builder_append(builder, "    pop eax\n") &&
                               builder_append(builder, "    mov dword [edx], eax\n");
                    }
                }

                /* Global vector */
                return generate_linearized_index(context, access, error) &&
                       builder_append(builder, "    imul eax, 4\n") &&
                       builder_append(builder, "    lea edx, [") &&
                       builder_append_user_symbol_name(builder, context->for_loop_ids, access->name) &&
                       builder_append(builder, " + eax]\n") &&
                       builder_append(builder, "    pop eax\n") &&
                       builder_append(builder, "    mov dword [edx], eax\n");
            }

            if (command->assignment.target.type == AST_TARGET_IDENTIFIER &&
                find_context_variable_type(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    command->assignment.target.identifier) == AST_TYPE_STRING) {
                return generate_string_assignment(
                    context, command->assignment.target.identifier, command->assignment.expression, error);
            }

            if (command->assignment.target.type == AST_TARGET_IDENTIFIER &&
                find_context_variable_info(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    command->assignment.target.identifier,
                    &target_type,
                    NULL,
                    NULL,
                    NULL,
                    NULL) &&
                target_type == AST_TYPE_FLUTUANTE &&
                infer_expression_type(context, command->assignment.expression, &expression_type) &&
                (expression_type == AST_TYPE_FLUTUANTE || expression_type == AST_TYPE_INTEIRO)) {
                if (!generate_float_expression(context, command->assignment.expression, error)) {
                    return false;
                }

                return generate_float_store_to_name(context, command->assignment.target.identifier);
            }

            if (command->assignment.target.type == AST_TARGET_INDEXED &&
                find_context_variable_type(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    command->assignment.target.indexed.name) == AST_TYPE_FLUTUANTE &&
                infer_expression_type(context, command->assignment.expression, &expression_type) &&
                (expression_type == AST_TYPE_FLUTUANTE || expression_type == AST_TYPE_INTEIRO)) {
                if (!generate_float_expression(context, command->assignment.expression, error)) {
                    return false;
                }

                return generate_float_index_store(context, &command->assignment.target.indexed, error);
            }

            if (command->assignment.expression != NULL && command->assignment.expression->type == AST_EXPR_INT) {
                return generate_store_int_to_name(context, command->assignment.target.identifier,
                                                  command->assignment.expression->int_value);
            }

            return generate_expression(context, command->assignment.expression, error) &&
                   generate_store_eax_to_name(context, command->assignment.target.identifier);
        }
        case AST_COMMAND_READ: {
            const char *rname = command->read.name;
            ASTType rtype = find_context_variable_type(
                context->parameters,
                context->parameter_count,
                context->proc_locals,
                context->proc_local_count,
                context->program,
                rname);

            if (command->read.target_type == AST_TARGET_INDEXED) {
                ASTIndexedAccess access = {
                    .name = command->read.name,
                    .index = command->read.index,
                    .index2 = command->read.index2,
                    .line = command->read.line,
                    .column = command->read.column,
                };

                if (rtype == AST_TYPE_FLUTUANTE) {
                    return builder_append(builder, "    call print_flush\n") &&
                           builder_append(builder, "    call read_float\n") &&
                           generate_float_index_store(context, &access, error);
                }

                if (!builder_append(builder, "    call print_flush\n") ||
                    !builder_append(builder, "    call read_int\n") ||
                    !builder_append(builder, "    push eax\n")) {
                    return false;
                }

                if (rtype == AST_TYPE_STRING) {
                    size_t base_offset;
                    size_t parameter_index;
                    const ASTParameter *parameter =
                        find_context_parameter(context->parameters, context->parameter_count, rname, &parameter_index);

                    if (parameter != NULL && parameter->storage == AST_STORAGE_INDEXED) {
                        return generate_linearized_index(context, &access, error) &&
                               builder_appendf(
                                   builder,
                                   "    mov edx, dword [ebp+%zu]\n",
                                   parameter_stack_offset(context->current_procedure, context->parameters, context->parameter_count, parameter_index)) &&
                               builder_append(builder, "    add edx, eax\n") &&
                               builder_append(builder, "    pop eax\n") &&
                               builder_append(builder, "    mov byte [edx], al\n");
                    }

                    if (context->proc_locals != NULL) {
                        base_offset = local_declaration_base_offset_by_name(
                            context->proc_locals, context->proc_local_count, rname);
                        if (base_offset > 0) {
                            return generate_linearized_index(context, &access, error) &&
                                   builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                                   builder_append(builder, "    pop eax\n") &&
                                   builder_append(builder, "    mov byte [edx], al\n");
                        }
                    }

                    return generate_linearized_index(context, &access, error) &&
                           builder_append(builder, "    lea edx, [") &&
                           builder_append_user_symbol_name(builder, context->for_loop_ids, rname) &&
                           builder_append(builder, " + eax]\n") &&
                           builder_append(builder, "    pop eax\n") &&
                           builder_append(builder, "    mov byte [edx], al\n");
                }

                {
                    size_t base_offset;

                    if (context->proc_locals != NULL) {
                        base_offset = local_declaration_base_offset_by_name(
                            context->proc_locals, context->proc_local_count, rname);
                        if (base_offset > 0) {
                            return generate_linearized_index(context, &access, error) &&
                                   builder_append(builder, "    imul eax, 4\n") &&
                                   builder_append(builder, "    neg eax\n") &&
                                   builder_appendf(builder, "    lea edx, [ebp + eax - %zu]\n", base_offset) &&
                                   builder_append(builder, "    pop eax\n") &&
                                   builder_append(builder, "    mov dword [edx], eax\n");
                        }
                    }
                }

                return generate_linearized_index(context, &access, error) &&
                       builder_append(builder, "    imul eax, 4\n") &&
                       builder_append(builder, "    lea edx, [") &&
                       builder_append_user_symbol_name(builder, context->for_loop_ids, rname) &&
                       builder_append(builder, " + eax]\n") &&
                       builder_append(builder, "    pop eax\n") &&
                       builder_append(builder, "    mov dword [edx], eax\n");
            }

            if (rtype == AST_TYPE_STRING) {
                size_t cap = find_context_variable_capacity(
                    context->parameters,
                    context->parameter_count,
                    context->proc_locals,
                    context->proc_local_count,
                    context->program,
                    rname);
                size_t max_read = cap > 1 ? cap - 1 : 0;

                return generate_string_address(context, rname) &&
                       builder_append(builder, "    call print_flush\n") &&
                       builder_appendf(builder, "    mov edx, %zu\n", max_read) &&
                       builder_append(builder, "    call read_string\n");
            }

            if (rtype == AST_TYPE_FLUTUANTE) {
                return builder_append(builder, "    call print_flush\n") &&
                       builder_append(builder, "    call read_float\n") &&
                       generate_float_store_to_name(context, rname);
            }

            return builder_append(builder, "    call print_flush\n") &&
                   builder_append(builder, "    call read_int\n") &&
                   generate_store_eax_to_name(context, rname);
        }
        case AST_COMMAND_WRITE: {
            const ASTExpression *wexpr = command->write.expression;
            ASTType wtype;

            if (wexpr != NULL && infer_expression_type(context, wexpr, &wtype) && wtype == AST_TYPE_STRING) {
                return generate_string_expression_address(context, wexpr, error) &&
                       builder_append(builder, "    call print_string\n");
            }

            if (wexpr != NULL && infer_expression_type(context, wexpr, &wtype) && wtype == AST_TYPE_FLUTUANTE) {
                return generate_float_expression(context, wexpr, error) &&
                       builder_append(builder, "    sub esp, 8\n") &&
                       builder_append(builder, "    fstp qword [esp]\n") &&
                       builder_append(builder, "    call print_float\n") &&
                       builder_append(builder, "    add esp, 8\n");
            }

            return generate_expression(context, wexpr, error) &&
                   builder_append(builder, "    call print_int\n");
        }
        case AST_COMMAND_WRITELN: {
            const ASTExpression *wexpr = command->write.expression;
            ASTType wtype;

            if (wexpr != NULL && infer_expression_type(context, wexpr, &wtype) && wtype == AST_TYPE_STRING) {
                return generate_string_expression_address(context, wexpr, error) &&
                       builder_append(builder, "    call print_string\n") &&
                       builder_append(builder, "    call print_newline\n");
            }

            if (wexpr != NULL && infer_expression_type(context, wexpr, &wtype) && wtype == AST_TYPE_FLUTUANTE) {
                return generate_float_expression(context, wexpr, error) &&
                       builder_append(builder, "    sub esp, 8\n") &&
                       builder_append(builder, "    fstp qword [esp]\n") &&
                       builder_append(builder, "    call print_float\n") &&
                       builder_append(builder, "    add esp, 8\n") &&
                       builder_append(builder, "    call print_newline\n");
            }

            return generate_expression(context, wexpr, error) &&
                   builder_append(builder, "    call print_int\n") &&
                   builder_append(builder, "    call print_newline\n");
        }
        case AST_COMMAND_CALL: {
            return generate_call(context, &command->call_command.call, error);
        }
        case AST_COMMAND_RETURN:
            if (context->current_proc_name == NULL) {
                return fail_codegen_internal(
                    error,
                    command->return_command.line,
                    command->return_command.column,
                    "Internal error: return command outside a procedure.");
            }

            if (command->return_command.expression != NULL) {
                const ProcedureSignature *signature =
                    find_procedure_signature(context->semantic, context->current_proc_name);

                if (signature != NULL && signature->return_type == AST_TYPE_STRING) {
                    size_t return_offset = procedure_string_return_buffer_stack_offset(context->current_procedure);
                    size_t label_id = context->next_label_id++;

                    if (!generate_string_expression_address(context, command->return_command.expression, error) ||
                        !builder_appendf(builder, "    mov edx, dword [ebp+%zu]\n", return_offset) ||
                        !append_copy_string_from_eax_to_edx(builder, label_id)) {
                        return false;
                    }
                } else if (signature != NULL && signature->return_type == AST_TYPE_FLUTUANTE) {
                    if (!generate_float_expression(context, command->return_command.expression, error)) {
                        return false;
                    }
                } else if (!generate_expression(context, command->return_command.expression, error)) {
                    return false;
                }
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
            ASTType iterator_type = find_context_variable_type(
                context->parameters,
                context->parameter_count,
                context->proc_locals,
                context->proc_local_count,
                context->program,
                command->for_command.iterator_name);

            if (iterator_type == AST_TYPE_FLUTUANTE) {
                return fail_float_expression_codegen(
                    error, command->for_command.line, command->for_command.column);
            }

            if (context->current_proc_name != NULL) {
                if (!resolve_procedure_for_loop_offsets(
                        context->proc_local_frame_bytes,
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

static bool generate_push_float_argument(
    CodegenContext *context, const ASTExpression *expression, CompilerError *error) {
    if (!generate_float_expression(context, expression, error)) {
        return false;
    }

    return builder_append(context->builder, "    sub esp, 8\n") &&
           builder_append(context->builder, "    fstp qword [esp]\n");
}

static bool generate_push_call_argument(
    CodegenContext *context,
    const char *procedure_name,
    size_t argument_index,
    const ASTExpression *expression,
    CompilerError *error) {
    const ProcedureSignature *signature;

    signature = find_procedure_signature(context->semantic, procedure_name);
    if (signature != NULL &&
        argument_index < signature->parameter_count &&
        signature->parameters[argument_index].storage == AST_STORAGE_INDEXED) {
        if (signature->parameters[argument_index].pass_mode == AST_PASS_BY_REFERENCE) {
            if (expression == NULL || expression->type != AST_EXPR_IDENTIFIER) {
                compiler_error_set(
                    error,
                    COMPILER_PHASE_CODEGEN,
                    expression != NULL ? expression->line : 0,
                    expression != NULL ? expression->column : 0,
                    "Code generation for string literal procedure arguments is not supported yet.");
                return false;
            }

            return generate_aggregate_address(context, expression->identifier) &&
                   builder_append(context->builder, "    push eax\n");
        }

        if (signature->parameters[argument_index].type == AST_TYPE_STRING) {
            return generate_string_expression_address(context, expression, error) &&
                   builder_append(context->builder, "    push eax\n");
        }

        if (expression == NULL || expression->type != AST_EXPR_IDENTIFIER) {
            compiler_error_set(
                error,
                COMPILER_PHASE_CODEGEN,
                expression != NULL ? expression->line : 0,
                expression != NULL ? expression->column : 0,
                "Code generation for by-value vector arguments requires a named variable.");
            return false;
        }

        return generate_aggregate_address(context, expression->identifier) &&
               builder_append(context->builder, "    push eax\n");
    }

    if (signature != NULL &&
        argument_index < signature->parameter_count &&
        signature->parameters[argument_index].type == AST_TYPE_FLUTUANTE) {
        return generate_push_float_argument(context, expression, error);
    }

    return generate_push_expression(context, expression, error);
}

static bool generate_call(CodegenContext *context, const ASTCall *call, CompilerError *error) {
    const ProcedureSignature *signature = find_procedure_signature(context->semantic, call->name);
    const StringReturnTempLabel *return_temp = NULL;
    size_t arg_i;
    size_t argument_bytes = procedure_signature_argument_bytes(signature);

    for (arg_i = call->argument_count; arg_i > 0; arg_i--) {
        if (!generate_push_call_argument(context, call->name, arg_i - 1, call->arguments[arg_i - 1], error)) {
            return false;
        }
    }

    if (signature != NULL && signature->return_type == AST_TYPE_STRING) {
        return_temp = find_string_return_temp_label(context->string_return_temps, call);
        if (return_temp == NULL) {
            return fail_codegen_internal(error, 0, 0, "Internal error: missing string return temp buffer.");
        }
        if (!builder_appendf(context->builder, "    push _retstr_%zu\n", return_temp->label_id)) {
            return false;
        }
    }

    if (!builder_appendf(context->builder, "    call proc_%s\n", call->name)) {
        return false;
    }

    if (argument_bytes > 0) {
        if (!builder_appendf(context->builder, "    add esp, %zu\n", argument_bytes)) {
            return false;
        }
    }

    if (signature != NULL && signature->return_type == AST_TYPE_STRING) {
        return builder_appendf(context->builder, "    mov eax, _retstr_%zu\n", return_temp->label_id);
    }

    return true;
}

static bool generate_procedure(
    const ASTProcedure *proc,
    const ASTProgram *program,
    const SemanticInfo *semantic,
    const StringReturnTempList *string_return_temps,
    const StringLiteralList *string_literals,
    const FloatLiteralList *float_literals,
    StringBuilder *builder,
    const SizeList *for_loop_ids,
    size_t *next_label_id, CompilerError *error) {
    CodegenContext context;
    SizeList procedure_for_loop_ids = {0};
    ASTParameter *codegen_parameters = NULL;
    ASTDeclaration *codegen_locals = NULL;
    size_t preview_next_label = next_label_id != NULL ? *next_label_id : 0;
    size_t total_local_bytes;
    size_t total_local_bytes_aligned;
    size_t total_frame_bytes = 0;
    size_t slot_index;
    size_t copy_index = 0;
    size_t param_index;
    size_t codegen_parameter_count = 0;
    size_t codegen_local_count = 0;

    if (!collect_for_loop_ids_in_block(
            &procedure_for_loop_ids, &preview_next_label, proc->commands, proc->command_count)) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    if (!build_codegen_parameter_views(proc, &codegen_parameters, &codegen_parameter_count, &codegen_locals, &codegen_local_count)) {
        free(procedure_for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    total_local_bytes = compute_local_frame_bytes(codegen_locals, codegen_local_count);
    total_local_bytes_aligned = (total_local_bytes + 3) & ~(size_t)3;
    total_frame_bytes = total_local_bytes_aligned + procedure_for_loop_ids.count * 2 * 4;

    if (!builder_appendf(builder, "\nproc_%s:\n", proc->name) ||
        !builder_append(builder, "    push ebp\n    mov ebp, esp\n")) {
        free(procedure_for_loop_ids.items);
        return false;
    }

    if (total_frame_bytes > 0) {
        if (!builder_appendf(builder, "    sub esp, %zu\n", total_frame_bytes)) {
            free(procedure_for_loop_ids.items);
            return false;
        }

        for (slot_index = 0; slot_index < total_frame_bytes / 4; ++slot_index) {
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
    context.program = program;
    context.semantic = semantic;
    context.current_proc_name = proc->name;
    context.current_procedure = proc;
    context.parameters = codegen_parameters;
    context.parameter_count = codegen_parameter_count;
    context.proc_locals = codegen_locals;
    context.proc_local_count = codegen_local_count;
    context.proc_local_frame_bytes = total_local_bytes_aligned;
    context.string_return_temps = string_return_temps;
    context.string_literals = string_literals;
    context.float_literals = float_literals;

    for (param_index = 0; param_index < proc->parameter_count; ++param_index) {
        const ASTParameter *parameter = &proc->parameters[param_index];
        size_t parameter_size = storage_size_bytes(parameter->type, parameter->storage, parameter->capacity);

        if (!procedure_parameter_is_by_value_aggregate(parameter)) {
            continue;
        }

        if (parameter->type == AST_TYPE_STRING) {
            if (!builder_appendf(
                    builder,
                    "    mov esi, dword [ebp+%zu]\n",
                    parameter_stack_offset(proc, proc->parameters, proc->parameter_count, param_index)) ||
                !builder_appendf(builder, "    lea edi, [ebp-%zu]\n", local_declaration_frame_offset(codegen_locals, copy_index)) ||
                !builder_appendf(builder, "    mov ecx, %zu\n", parameter_size) ||
                !builder_append(builder, "    cld\n    rep movsb\n")) {
                free(procedure_for_loop_ids.items);
                for (slot_index = 0; slot_index < codegen_local_count; ++slot_index) {
                    free(codegen_locals[slot_index].name);
                }
                free(codegen_locals);
                for (slot_index = 0; slot_index < codegen_parameter_count; ++slot_index) {
                    free(codegen_parameters[slot_index].name);
                }
                free(codegen_parameters);
                return false;
            }
        } else {
            size_t source_offset = parameter_stack_offset(proc, proc->parameters, proc->parameter_count, param_index);
            size_t dest_offset = local_declaration_frame_offset(codegen_locals, copy_index);
            size_t element_size = parameter->type == AST_TYPE_FLUTUANTE ? 8 : 4;
            size_t element_count = parameter_size / element_size;

            if (!builder_appendf(builder, "    mov esi, dword [ebp+%zu]\n", source_offset) ||
                !builder_appendf(builder, "    lea edi, [ebp-%zu]\n", dest_offset) ||
                !builder_appendf(builder, "    mov ecx, %zu\n", element_count) ||
                !builder_append(builder, "    cld\n") ||
                !builder_appendf(builder, ".Lparamcopy%zu:\n", copy_index) ||
                !builder_append(builder, "    mov eax, dword [esi]\n") ||
                !builder_append(builder, "    mov dword [edi], eax\n") ||
                !builder_append(builder, "    add esi, 4\n") ||
                !builder_append(builder, "    sub edi, 4\n") ||
                !builder_appendf(builder, "    loop .Lparamcopy%zu\n", copy_index)) {
                free(procedure_for_loop_ids.items);
                for (slot_index = 0; slot_index < codegen_local_count; ++slot_index) {
                    free(codegen_locals[slot_index].name);
                }
                free(codegen_locals);
                for (slot_index = 0; slot_index < codegen_parameter_count; ++slot_index) {
                    free(codegen_parameters[slot_index].name);
                }
                free(codegen_parameters);
                return false;
            }
        }

        copy_index++;
    }

    if (!generate_command_block(&context, proc->commands, proc->command_count, error)) {
        free(procedure_for_loop_ids.items);
        for (slot_index = 0; slot_index < codegen_local_count; ++slot_index) {
            free(codegen_locals[slot_index].name);
        }
        free(codegen_locals);
        for (slot_index = 0; slot_index < codegen_parameter_count; ++slot_index) {
            free(codegen_parameters[slot_index].name);
        }
        free(codegen_parameters);
        return false;
    }

    if (next_label_id != NULL) {
        *next_label_id = context.next_label_id;
    }

    if (!builder_appendf(builder, ".proc_%s_epilogue:\n", proc->name) ||
        !builder_append(builder, "    mov esp, ebp\n    pop ebp\n    ret\n")) {
        free(procedure_for_loop_ids.items);
        for (slot_index = 0; slot_index < codegen_local_count; ++slot_index) {
            free(codegen_locals[slot_index].name);
        }
        free(codegen_locals);
        for (slot_index = 0; slot_index < codegen_parameter_count; ++slot_index) {
            free(codegen_parameters[slot_index].name);
        }
        free(codegen_parameters);
        return false;
    }

    free(procedure_for_loop_ids.items);
    for (slot_index = 0; slot_index < codegen_local_count; ++slot_index) {
        free(codegen_locals[slot_index].name);
    }
    free(codegen_locals);
    for (slot_index = 0; slot_index < codegen_parameter_count; ++slot_index) {
        free(codegen_parameters[slot_index].name);
    }
    free(codegen_parameters);
    return true;
}

#ifdef CODEGEN_TESTING
bool codegen_debug_resolve_procedure_for_loop_offsets(
    size_t proc_local_frame_bytes,
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
        proc_local_frame_bytes, procedure_for_loop_ids != NULL ? &list : NULL, label_id, end_offset, step_offset, error,
        line, column);
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
                "    ret\n"
                "\n"
                "print_flush:\n"
                "    mov eax, 118\n"
                "    mov ebx, 1\n"
                "    int 0x80\n"
                "    ret\n") &&
            builder_append(
                builder,
                "\n"
                "read_float:\n"
                "    mov eax, 3\n"
                "    mov ebx, 0\n"
                "    mov ecx, read_buffer\n"
               "    mov edx, 16\n"
               "    int 0x80\n"
               "    cmp eax, 0\n"
               "    jle .read_float_zero\n"
               "    lea edx, [read_buffer + eax]\n"
               "    xor esi, esi\n"
               "    xor edi, edi\n"
               "    xor ebx, ebx\n"
               "    mov ecx, 1\n"
               "    mov eax, read_buffer\n"
               "    cmp byte [eax], '-'\n"
               "    jne .read_float_int_digits\n"
               "    mov esi, 1\n"
               "    inc eax\n"
               ".read_float_int_digits:\n"
               "    cmp eax, edx\n"
               "    jae .read_float_build\n"
               "    movzx ebp, byte [eax]\n"
               "    cmp ebp, '.'\n"
               "    je .read_float_frac_start\n"
               "    cmp ebp, 10\n"
               "    je .read_float_build\n"
               "    cmp ebp, 13\n"
               "    je .read_float_build\n"
               "    cmp ebp, '0'\n"
               "    jb .read_float_build\n"
               "    cmp ebp, '9'\n"
               "    ja .read_float_build\n"
               "    sub ebp, '0'\n"
               "    imul edi, edi, 10\n"
               "    add edi, ebp\n"
               "    inc eax\n"
               "    jmp .read_float_int_digits\n"
               ".read_float_frac_start:\n"
               "    inc eax\n"
               ".read_float_frac_digits:\n"
               "    cmp eax, edx\n"
               "    jae .read_float_build\n"
               "    movzx ebp, byte [eax]\n"
               "    cmp ebp, 10\n"
               "    je .read_float_build\n"
               "    cmp ebp, 13\n"
               "    je .read_float_build\n"
               "    cmp ebp, '0'\n"
               "    jb .read_float_build\n"
               "    cmp ebp, '9'\n"
               "    ja .read_float_build\n"
               "    sub ebp, '0'\n"
               "    imul ebx, ebx, 10\n"
               "    add ebx, ebp\n"
               "    imul ecx, ecx, 10\n"
               "    inc eax\n"
               "    jmp .read_float_frac_digits\n"
               ".read_float_build:\n"
               "    mov dword [tmp_int], edi\n"
               "    fild dword [tmp_int]\n"
               "    cmp ecx, 1\n"
               "    je .read_float_apply_sign\n"
               "    mov dword [tmp_int], ebx\n"
               "    fild dword [tmp_int]\n"
               "    mov dword [tmp_int], ecx\n"
               "    fild dword [tmp_int]\n"
               "    fdivp st1, st0\n"
               "    faddp st1, st0\n"
               ".read_float_apply_sign:\n"
               "    cmp esi, 0\n"
               "    je .read_float_ret\n"
               "    fchs\n"
               ".read_float_ret:\n"
               "    ret\n"
               ".read_float_zero:\n"
               "    fldz\n"
               "    ret\n"
               "\n"
               "print_float:\n"
               "    push ebp\n"
               "    mov ebp, esp\n"
               "    fld qword [ebp + 8]\n"
               "    fstp qword [tmp_float]\n"
               "    mov eax, dword [tmp_float + 4]\n"
               "    test eax, 0x80000000\n"
               "    jz .print_float_abs\n"
               "    mov byte [print_buffer], '-'\n"
               "    mov eax, 4\n"
               "    mov ebx, 1\n"
               "    mov ecx, print_buffer\n"
               "    mov edx, 1\n"
               "    int 0x80\n"
               "    and dword [tmp_float + 4], 0x7fffffff\n"
               ".print_float_abs:\n"
               "    fld qword [tmp_float]\n"
               "    fld st0\n"
               "    fisttp dword [tmp_int]\n"
               "    fild dword [tmp_int]\n"
               "    fsubp st1, st0\n"
               "    fstp qword [tmp_float]\n"
               "    mov eax, dword [tmp_int]\n"
               "    call print_int\n"
               "    fld qword [tmp_float]\n"
               "    fld qword [flt_scale]\n"
               "    fmulp st1, st0\n"
               "    fistp dword [tmp_int]\n"
               "    mov eax, dword [tmp_int]\n"
               "    mov byte [print_buffer], '.'\n"
               "    mov ecx, 6\n"
               "    mov esi, print_buffer + 6\n"
               ".print_float_frac_loop:\n"
               "    xor edx, edx\n"
               "    mov ebx, 10\n"
               "    div ebx\n"
               "    add dl, '0'\n"
               "    mov [esi], dl\n"
               "    dec esi\n"
               "    dec ecx\n"
               "    jnz .print_float_frac_loop\n"
               "    mov ecx, 6\n"
               ".print_float_trim:\n"
               "    cmp ecx, 1\n"
               "    jbe .print_float_write\n"
               "    cmp byte [print_buffer + ecx], '0'\n"
               "    jne .print_float_write\n"
               "    dec ecx\n"
               "    jmp .print_float_trim\n"
               ".print_float_write:\n"
               "    mov eax, 4\n"
               "    mov ebx, 1\n"
               "    mov esi, ecx\n"
               "    inc esi\n"
               "    mov ecx, print_buffer\n"
               "    mov edx, esi\n"
               "    int 0x80\n"
               "    mov esp, ebp\n"
               "    pop ebp\n"
               "    ret\n") &&
           builder_append_print_string_helper(builder) &&
           builder_append_read_string_helper(builder);
}

bool codegen_generate_program(const ASTProgram *program, const SemanticInfo *semantic, char **out_assembly, CompilerError *error) {
    StringBuilder builder = {0};
    SizeList for_loop_ids = {0};
    SizeList main_for_loop_ids = {0};
    StringReturnTempList string_return_temps = {0};
    StringLiteralList string_literals = {0};
    FloatLiteralList float_literals = {0};
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

    if (!main_program_supports_backend(program, semantic, error)) {
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

    if (!collect_string_literals_in_program(&string_literals, program)) {
        free(main_for_loop_ids.items);
        free(for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    if (!collect_string_return_temps_in_program(&string_return_temps, semantic, program)) {
        free(string_literals.items);
        free(main_for_loop_ids.items);
        free(for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    if (!collect_float_literals_in_program(&float_literals, program)) {
        free(string_return_temps.items);
        free(string_literals.items);
        free(main_for_loop_ids.items);
        free(for_loop_ids.items);
        return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
    }

    if (!builder_append(&builder, "global _start\n\nsection .data\n")) {
        goto fail;
    }

    for (index = 0; index < symbol_count(program, globals, global_count); ++index) {
        const char *name = symbol_name_at(program, globals, global_count, index);
        ASTStorageKind storage = symbol_storage_at(program, globals, global_count, index);
        size_t capacity = symbol_capacity_at(program, globals, global_count, index);

        if (name == NULL) {
            goto fail;
        }

        if (storage == AST_STORAGE_INDEXED && capacity > 0) {
            ASTType var_type = symbol_type_at(program, globals, global_count, index);
            const char *unit = (var_type == AST_TYPE_STRING) ? "db" : (var_type == AST_TYPE_FLUTUANTE ? "dq" : "dd");
            const char *zero = (var_type == AST_TYPE_FLUTUANTE) ? "0.0" : "0";

            if (!builder_append_user_symbol_name(&builder, &for_loop_ids, name) ||
                !builder_appendf(&builder, " times %zu %s %s\n", capacity, unit, zero)) {
                goto fail;
            }
        } else {
            ASTType var_type = symbol_type_at(program, globals, global_count, index);

            if (var_type == AST_TYPE_FLUTUANTE) {
                if (!builder_append_user_symbol_name(&builder, &for_loop_ids, name) ||
                    !builder_append(&builder, " dq 0.0\n")) {
                    goto fail;
                }
            } else if (!builder_append_user_symbol_declaration(&builder, &for_loop_ids, name)) {
                goto fail;
            }
        }
    }

    for (index = 0; index < main_for_loop_ids.count; ++index) {
        size_t label_id = main_for_loop_ids.items[index];

        if (!builder_appendf(&builder, "_for_end_%zu dd 0\n", label_id) ||
            !builder_appendf(&builder, "_for_step_%zu dd 0\n", label_id)) {
            goto fail;
        }
    }

    if (!emit_string_literal_declarations(&builder, &string_literals)) {
        goto fail;
    }

    if (!emit_string_return_temp_declarations(&builder, &string_return_temps)) {
        goto fail;
    }

    if (!emit_float_literal_declarations(&builder, &float_literals)) {
        goto fail;
    }

    if (!builder_append(
            &builder,
            "newline db 10\nprint_buffer times 12 db 0\nread_buffer times 16 db 0\ntmp_int dd 0\ntmp_float dq 0.0\nflt_scale dq 1000000.0\n\nsection .text\n_start:\n")) {
        goto fail;
    }

    context.builder = &builder;
    context.for_loop_ids = &for_loop_ids;
    context.procedure_for_loop_ids = NULL;
    context.next_label_id = 0;
    context.program = program;
    context.semantic = semantic;
    context.current_proc_name = NULL;
    context.current_procedure = NULL;
    context.parameters = NULL;
    context.parameter_count = 0;
    context.proc_locals = NULL;
    context.proc_local_count = 0;
    context.proc_local_frame_bytes = 0;
    context.string_return_temps = &string_return_temps;
    context.string_literals = &string_literals;
    context.float_literals = &float_literals;

    if (!generate_command_block(&context, program->commands, program->command_count, error)) {
        goto fail;
    }

    if (!builder_append(&builder, "    mov eax, 1\n    xor ebx, ebx\n    int 0x80\n")) {
        goto fail;
    }

    for (index = 0; index < program->procedure_count; index++) {
        if (!generate_procedure(
                &program->procedures[index],
                program,
                semantic,
                &string_return_temps,
                &string_literals,
                &float_literals,
                &builder,
                &for_loop_ids,
                &context.next_label_id,
                error)) {
            goto fail;
        }
    }

    if (!generate_helpers(&builder)) {
        goto fail;
    }

    free(main_for_loop_ids.items);
    free(for_loop_ids.items);
    free(string_return_temps.items);
    free(string_literals.items);
    free(float_literals.items);
    if (out_assembly != NULL) {
        *out_assembly = builder.data;
    } else {
        free(builder.data);
    }
    return true;

fail:
    free(string_return_temps.items);
    free(float_literals.items);
    free(string_literals.items);
    free(main_for_loop_ids.items);
    free(for_loop_ids.items);
    free(builder.data);
    return fail_codegen_internal(error, 0, 0, "Internal error: code generation failed.");
}
