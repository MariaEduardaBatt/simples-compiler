#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    ASTType type;
    ASTStorageKind storage;
} TypedSymbol;

typedef struct {
    TypedSymbol *symbols;
    size_t count;
} TypedScope;

typedef struct {
    const SemanticInfo *info;
    const ASTProcedure *current_procedure;
    const TypedScope *scope;
    bool inside_procedure;
} SemanticContext;

static char *semantic_strdup(const char *text) {
    size_t length;
    char *copy;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

void semantic_info_free(SemanticInfo *info) {
    size_t index;

    if (info == NULL) {
        return;
    }

    for (index = 0; index < info->global_count; ++index) {
        free(info->globals[index].name);
    }
    free(info->globals);
    info->globals = NULL;
    info->global_count = 0;

    for (index = 0; index < info->procedure_count; ++index) {
        free(info->procedures[index].name);
        free(info->procedures[index].parameter_types);
    }

    free(info->procedures);
    info->procedures = NULL;
    info->procedure_count = 0;
}

static void typed_scope_free(TypedScope *scope) {
    size_t index;

    if (scope == NULL) {
        return;
    }

    for (index = 0; index < scope->count; ++index) {
        free(scope->symbols[index].name);
    }

    free(scope->symbols);
    scope->symbols = NULL;
    scope->count = 0;
}

static bool semantic_fail_at(CompilerError *error, int line, int column, const char *message) {
    compiler_error_set(error, COMPILER_PHASE_SEMANTIC, line, column, message);
    return false;
}

static bool semantic_fail(CompilerError *error, const char *message) {
    return semantic_fail_at(error, 1, 1, message);
}

static bool semantic_fail_expression(const ASTExpression *expression, CompilerError *error, const char *message) {
    if (expression == NULL) {
        return semantic_fail(error, message);
    }

    return semantic_fail_at(error, expression->line, expression->column, message);
}

static bool semantic_fail_declaration(const ASTDeclaration *declaration, CompilerError *error, const char *message) {
    if (declaration == NULL) {
        return semantic_fail(error, message);
    }

    return semantic_fail_at(error, declaration->line, declaration->column, message);
}

static const char *semantic_type_name(ASTType type) {
    switch (type) {
        case AST_TYPE_INTEIRO:
            return "inteiro";
        case AST_TYPE_FLUTUANTE:
            return "flutuante";
        case AST_TYPE_VAZIO:
            return "vazio";
        case AST_TYPE_STRING:
            return "string";
        default:
            return "desconhecido";
    }
}

static bool typed_scope_contains(const TypedScope *scope, const char *name) {
    size_t index;

    for (index = 0; index < scope->count; ++index) {
        if (strcmp(scope->symbols[index].name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool typed_scope_append(TypedScope *scope, const char *name, ASTType type, ASTStorageKind storage) {
    TypedSymbol *symbols;
    char *copy;

    copy = semantic_strdup(name);
    if (copy == NULL) {
        return false;
    }

    symbols = realloc(scope->symbols, (scope->count + 1) * sizeof(*scope->symbols));
    if (symbols == NULL) {
        free(copy);
        return false;
    }

    scope->symbols = symbols;
    scope->symbols[scope->count].name = copy;
    scope->symbols[scope->count].type = type;
    scope->symbols[scope->count].storage = storage;
    scope->count++;
    return true;
}

static bool typed_scope_lookup(const TypedScope *scope, const char *name, ASTType *out_type, ASTStorageKind *out_storage) {
    size_t index;

    for (index = 0; index < scope->count; ++index) {
        if (strcmp(scope->symbols[index].name, name) == 0) {
            if (out_type != NULL) {
                *out_type = scope->symbols[index].type;
            }
            if (out_storage != NULL) {
                *out_storage = scope->symbols[index].storage;
            }
            return true;
        }
    }

    return false;
}

static bool semantic_check_identifier(
    const TypedScope *scope, const char *name, int line, int column, ASTType *out_type, CompilerError *error) {
    char message[256];

    if (typed_scope_lookup(scope, name, out_type, NULL)) {
        return true;
    }

    snprintf(message, sizeof(message), "Identificador '%s' nao declarado.", name);
    return semantic_fail_at(error, line, column, message);
}

static bool semantic_info_contains_global(const SemanticInfo *info, const char *name) {
    size_t index;

    for (index = 0; index < info->global_count; ++index) {
        if (strcmp(info->globals[index].name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool semantic_info_append_global(SemanticInfo *info, const ASTDeclaration *decl) {
    SymbolInfo *globals;
    char *name_copy;

    name_copy = semantic_strdup(decl->name);
    if (name_copy == NULL) {
        return false;
    }

    globals = realloc(info->globals, (info->global_count + 1) * sizeof(*info->globals));
    if (globals == NULL) {
        free(name_copy);
        return false;
    }

    info->globals = globals;
    info->globals[info->global_count].name = name_copy;
    info->globals[info->global_count].type = decl->type;
    info->globals[info->global_count].storage = decl->storage;
    info->globals[info->global_count].capacity = decl->capacity;
    info->global_count++;
    return true;
}

static const ProcedureSignature *find_procedure(const SemanticInfo *info, const char *name) {
    size_t index;

    for (index = 0; index < info->procedure_count; ++index) {
        if (strcmp(info->procedures[index].name, name) == 0) {
            return &info->procedures[index];
        }
    }

    return NULL;
}

static bool analyze_expression(const ASTExpression *expression, const SemanticContext *ctx, ASTType *out_type, CompilerError *error);

static bool analyze_call(
    const ASTCall *call,
    const SemanticContext *ctx,
    bool used_as_expression,
    ASTType *out_type,
    CompilerError *error) {
    const ProcedureSignature *signature;
    char message[256];
    size_t index;

    signature = find_procedure(ctx->info, call->name);
    if (signature == NULL) {
        snprintf(message, sizeof(message), "Procedimento '%s' nao declarado.", call->name);
        return semantic_fail_at(error, call->line, call->column, message);
    }

    if (call->argument_count != signature->parameter_count) {
        snprintf(
            message,
            sizeof(message),
            "Procedimento '%s' espera %zu argumentos, mas recebeu %zu.",
            call->name,
            signature->parameter_count,
            call->argument_count);
        return semantic_fail_at(error, call->line, call->column, message);
    }

    for (index = 0; index < call->argument_count; ++index) {
        ASTType argument_type;

        if (!analyze_expression(call->arguments[index], ctx, &argument_type, error)) {
            return false;
        }

        if (argument_type != signature->parameter_types[index]) {
            snprintf(
                message,
                sizeof(message),
                "Argumento %zu de '%s' espera tipo '%s', mas recebeu '%s'.",
                index + 1,
                call->name,
                semantic_type_name(signature->parameter_types[index]),
                semantic_type_name(argument_type));
            return semantic_fail_expression(call->arguments[index], error, message);
        }
    }

    if (used_as_expression && signature->return_type == AST_TYPE_VAZIO) {
        snprintf(
            message,
            sizeof(message),
            "Procedimento '%s' com retorno vazio nao pode ser usado em expressao.",
            call->name);
        return semantic_fail_at(error, call->line, call->column, message);
    }

    if (out_type != NULL) {
        *out_type = signature->return_type;
    }

    return true;
}

static bool analyze_unary_expression(
    const ASTExpression *expression,
    const SemanticContext *ctx,
    ASTType *out_type,
    CompilerError *error) {
    ASTType operand_type;

    if (!analyze_expression(expression->unary.operand, ctx, &operand_type, error)) {
        return false;
    }

    switch (expression->unary.op) {
        case AST_UNARY_NEGATE:
            if (out_type != NULL) {
                *out_type = operand_type;
            }
            return true;
        case AST_UNARY_NOT:
            if (out_type != NULL) {
                *out_type = AST_TYPE_INTEIRO;
            }
            return true;
        default:
            return semantic_fail_expression(expression, error, "Expressao invalida.");
    }
}

static bool analyze_binary_expression(
    const ASTExpression *expression,
    const SemanticContext *ctx,
    ASTType *out_type,
    CompilerError *error) {
    ASTType left_type;
    ASTType right_type;
    char message[256];

    if (!analyze_expression(expression->binary.left, ctx, &left_type, error) ||
        !analyze_expression(expression->binary.right, ctx, &right_type, error)) {
        return false;
    }

    if (left_type != right_type) {
        snprintf(
            message,
            sizeof(message),
            "Operacao entre tipos '%s' e '%s' nao e permitida.",
            semantic_type_name(left_type),
            semantic_type_name(right_type));
        return semantic_fail_expression(expression, error, message);
    }

    switch (expression->binary.op) {
        case AST_BINARY_ADD:
        case AST_BINARY_SUB:
        case AST_BINARY_MUL:
        case AST_BINARY_DIV:
            if (out_type != NULL) {
                *out_type = left_type;
            }
            return true;
        case AST_BINARY_GT:
        case AST_BINARY_LT:
        case AST_BINARY_EQ:
        case AST_BINARY_NE:
        case AST_BINARY_GE:
        case AST_BINARY_LE:
        case AST_BINARY_AND:
        case AST_BINARY_OR:
            if (out_type != NULL) {
                *out_type = AST_TYPE_INTEIRO;
            }
            return true;
        default:
            return semantic_fail_expression(expression, error, "Expressao invalida.");
    }
}

static bool analyze_expression(const ASTExpression *expression, const SemanticContext *ctx, ASTType *out_type, CompilerError *error) {
    ASTType base_type;
    ASTStorageKind base_storage;
    ASTType index_type;
    char message[256];

    if (expression == NULL) {
        return semantic_fail(error, "Expressao invalida.");
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            if (out_type != NULL) {
                *out_type = AST_TYPE_INTEIRO;
            }
            return true;
        case AST_EXPR_FLOAT:
            if (out_type != NULL) {
                *out_type = AST_TYPE_FLUTUANTE;
            }
            return true;
        case AST_EXPR_STRING:
            if (out_type != NULL) {
                *out_type = AST_TYPE_STRING;
            }
            return true;
        case AST_EXPR_IDENTIFIER:
            return semantic_check_identifier(
                ctx->scope, expression->identifier, expression->line, expression->column, out_type, error);
        case AST_EXPR_INDEX:
            if (!typed_scope_lookup(ctx->scope, expression->index_access.name, &base_type, &base_storage)) {
                snprintf(message, sizeof(message), "Identificador '%s' nao declarado.", expression->index_access.name);
                return semantic_fail_at(error, expression->index_access.line, expression->index_access.column, message);
            }
            if (base_storage != AST_STORAGE_INDEXED) {
                snprintf(message, sizeof(message), "Identificador '%s' nao pode ser indexado.", expression->index_access.name);
                return semantic_fail_at(error, expression->index_access.line, expression->index_access.column, message);
            }
            if (!analyze_expression(expression->index_access.index, ctx, &index_type, error)) {
                return false;
            }
            if (index_type != AST_TYPE_INTEIRO) {
                return semantic_fail_expression(
                    expression->index_access.index, error, "Indice deve ser do tipo 'inteiro'.");
            }
            if (out_type != NULL) {
                *out_type = base_type;
            }
            return true;
        case AST_EXPR_CALL:
            return analyze_call(&expression->call, ctx, true, out_type, error);
        case AST_EXPR_UNARY:
            return analyze_unary_expression(expression, ctx, out_type, error);
        case AST_EXPR_BINARY:
            return analyze_binary_expression(expression, ctx, out_type, error);
        default:
            return semantic_fail_expression(expression, error, "Expressao invalida.");
    }
}

static bool analyze_command_list(
    const ASTCommand *commands,
    size_t command_count,
    const SemanticContext *ctx,
    bool *out_guarantees_return,
    CompilerError *error);

static bool analyze_assignment_command(const ASTAssignmentCommand *assignment, const SemanticContext *ctx, CompilerError *error) {
    ASTType variable_type;
    ASTStorageKind variable_storage;
    ASTType expression_type;
    ASTType index_type;
    char message[256];
    const char *target_name = assignment->target.type == AST_TARGET_IDENTIFIER
        ? assignment->target.identifier
        : assignment->target.indexed.name;
    int target_line = assignment->target.line;
    int target_column = assignment->target.column;

    if (!typed_scope_lookup(ctx->scope, target_name, &variable_type, &variable_storage)) {
        snprintf(message, sizeof(message), "Identificador '%s' nao declarado.", target_name);
        return semantic_fail_at(error, target_line, target_column, message);
    }

    if (assignment->target.type == AST_TARGET_INDEXED) {
        if (variable_storage != AST_STORAGE_INDEXED) {
            snprintf(message, sizeof(message), "Identificador '%s' nao pode ser indexado.", target_name);
            return semantic_fail_at(error, target_line, target_column, message);
        }
        if (!analyze_expression(assignment->target.indexed.index, ctx, &index_type, error)) {
            return false;
        }
        if (index_type != AST_TYPE_INTEIRO) {
            return semantic_fail_expression(
                assignment->target.indexed.index, error, "Indice deve ser do tipo 'inteiro'.");
        }
    }

    if (!analyze_expression(assignment->expression, ctx, &expression_type, error)) {
        return false;
    }

    if (variable_type != expression_type) {
        snprintf(
            message,
            sizeof(message),
            "Variavel '%s' espera tipo '%s', mas recebeu '%s'.",
            target_name,
            semantic_type_name(variable_type),
            semantic_type_name(expression_type));
        return semantic_fail_expression(assignment->expression, error, message);
    }

    return true;
}

static bool analyze_return_command(const ASTReturnCommand *command, const SemanticContext *ctx, CompilerError *error) {
    ASTType expression_type;
    char message[256];

    if (!ctx->inside_procedure) {
        return semantic_fail_at(error, command->line, command->column, "Comando 'retorna' fora de procedimento.");
    }

    if (ctx->current_procedure->return_type == AST_TYPE_VAZIO) {
        if (command->expression != NULL) {
            return semantic_fail_at(error, command->line, command->column, "Procedimento vazio nao pode retornar expressao.");
        }
        return true;
    }

    if (command->expression == NULL) {
        snprintf(
            message,
            sizeof(message),
            "Procedimento '%s' deve retornar expressao do tipo '%s'.",
            ctx->current_procedure->name,
            semantic_type_name(ctx->current_procedure->return_type));
        return semantic_fail_at(error, command->line, command->column, message);
    }

    if (!analyze_expression(command->expression, ctx, &expression_type, error)) {
        return false;
    }

    if (expression_type != ctx->current_procedure->return_type) {
        snprintf(
            message,
            sizeof(message),
            "Procedimento '%s' deve retornar tipo '%s', mas recebeu '%s'.",
            ctx->current_procedure->name,
            semantic_type_name(ctx->current_procedure->return_type),
            semantic_type_name(expression_type));
        return semantic_fail_expression(command->expression, error, message);
    }

    return true;
}

static bool analyze_for_command(const ASTForCommand *command, const SemanticContext *ctx, CompilerError *error) {
    ASTType iterator_type;
    ASTType start_type;
    ASTType end_type;
    ASTType step_type;
    char message[256];
    bool ignored_guarantee;

    if (!semantic_check_identifier(
            ctx->scope,
            command->iterator_name,
            command->line,
            command->column,
            &iterator_type,
            error) ||
        !analyze_expression(command->start_expression, ctx, &start_type, error) ||
        !analyze_expression(command->end_expression, ctx, &end_type, error) ||
        !analyze_expression(command->step_expression, ctx, &step_type, error)) {
        return false;
    }

    if (iterator_type != start_type || iterator_type != end_type || iterator_type != step_type) {
        snprintf(
            message,
            sizeof(message),
            "Comando 'para' requer iterador e limites do mesmo tipo '%s'.",
            semantic_type_name(iterator_type));
        return semantic_fail_at(error, command->line, command->column, message);
    }

    return analyze_command_list(command->body_commands, command->body_count, ctx, &ignored_guarantee, error);
}

static bool analyze_command(
    const ASTCommand *command,
    const SemanticContext *ctx,
    bool *out_guarantees_return,
    CompilerError *error) {
    bool then_guarantees_return = false;
    bool else_guarantees_return = false;
    bool body_guarantees_return = false;

    if (out_guarantees_return != NULL) {
        *out_guarantees_return = false;
    }

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            return analyze_assignment_command(&command->assignment, ctx, error);
        case AST_COMMAND_READ:
            return semantic_check_identifier(
                ctx->scope, command->read.name, command->read.line, command->read.column, NULL, error);
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            return analyze_expression(command->write.expression, ctx, NULL, error);
        case AST_COMMAND_CALL:
            return analyze_call(&command->call_command.call, ctx, false, NULL, error);
        case AST_COMMAND_RETURN:
            if (!analyze_return_command(&command->return_command, ctx, error)) {
                return false;
            }
            if (out_guarantees_return != NULL) {
                *out_guarantees_return = true;
            }
            return true;
        case AST_COMMAND_IF:
            if (!analyze_expression(command->if_command.condition, ctx, NULL, error) ||
                !analyze_command_list(
                    command->if_command.then_commands,
                    command->if_command.then_count,
                    ctx,
                    &then_guarantees_return,
                    error)) {
                return false;
            }

            if (command->if_command.else_count > 0 &&
                !analyze_command_list(
                    command->if_command.else_commands,
                    command->if_command.else_count,
                    ctx,
                    &else_guarantees_return,
                    error)) {
                return false;
            }

            if (out_guarantees_return != NULL) {
                *out_guarantees_return =
                    command->if_command.else_count > 0 && then_guarantees_return && else_guarantees_return;
            }
            return true;
        case AST_COMMAND_WHILE:
            if (!analyze_expression(command->while_command.condition, ctx, NULL, error) ||
                !analyze_command_list(
                    command->while_command.body_commands,
                    command->while_command.body_count,
                    ctx,
                    &body_guarantees_return,
                    error)) {
                return false;
            }
            return true;
        case AST_COMMAND_FOR:
            return analyze_for_command(&command->for_command, ctx, error);
        default:
            return semantic_fail(error, "Comando invalido.");
    }
}

static bool analyze_command_list(
    const ASTCommand *commands,
    size_t command_count,
    const SemanticContext *ctx,
    bool *out_guarantees_return,
    CompilerError *error) {
    size_t index;
    bool guarantees_return = false;

    for (index = 0; index < command_count; ++index) {
        bool command_guarantees_return = false;

        if (!analyze_command(&commands[index], ctx, &command_guarantees_return, error)) {
            return false;
        }

        if (command_guarantees_return) {
            guarantees_return = true;
        }
    }

    if (out_guarantees_return != NULL) {
        *out_guarantees_return = guarantees_return;
    }

    return true;
}

static bool analyze_procedure(const ASTProcedure *procedure, const SemanticInfo *info, CompilerError *error) {
    TypedScope scope = {0};
    SemanticContext ctx;
    char message[256];
    size_t index;
    bool guarantees_return = false;

    for (index = 0; index < procedure->parameter_count; ++index) {
        if (typed_scope_contains(&scope, procedure->parameters[index].name)) {
            snprintf(message, sizeof(message), "Parametro '%s' ja declarado.", procedure->parameters[index].name);
            typed_scope_free(&scope);
            return semantic_fail_at(
                error, procedure->parameters[index].line, procedure->parameters[index].column, message);
        }

        if (!typed_scope_append(&scope, procedure->parameters[index].name, procedure->parameters[index].type, AST_STORAGE_SCALAR)) {
            typed_scope_free(&scope);
            return semantic_fail(error, "Memoria insuficiente.");
        }
    }

    for (index = 0; index < procedure->local_declaration_count; ++index) {
        const ASTDeclaration *decl = &procedure->local_declarations[index];

        if (typed_scope_contains(&scope, decl->name)) {
            snprintf(message, sizeof(message), "Identificador '%s' ja declarado.", decl->name);
            typed_scope_free(&scope);
            return semantic_fail_declaration(decl, error, message);
        }

        if (decl->type == AST_TYPE_STRING && decl->storage != AST_STORAGE_INDEXED) {
            typed_scope_free(&scope);
            return semantic_fail_declaration(decl, error, "Declaracao de string requer capacidade fixa.");
        }

        if (!typed_scope_append(&scope, decl->name, decl->type, decl->storage)) {
            typed_scope_free(&scope);
            return semantic_fail(error, "Memoria insuficiente.");
        }
    }

    ctx.info = info;
    ctx.current_procedure = procedure;
    ctx.scope = &scope;
    ctx.inside_procedure = true;

    if (!analyze_command_list(procedure->commands, procedure->command_count, &ctx, &guarantees_return, error)) {
        typed_scope_free(&scope);
        return false;
    }

    typed_scope_free(&scope);

    if (procedure->return_type != AST_TYPE_VAZIO && !guarantees_return) {
        snprintf(
            message,
            sizeof(message),
            "Procedimento '%s' deve retornar valor em todos os caminhos.",
            procedure->name);
        return semantic_fail_at(error, procedure->line, procedure->column, message);
    }

    return true;
}

bool analyze_program(const ASTProgram *program, SemanticInfo *out_info, CompilerError *error) {
    size_t index;
    SemanticInfo info = {0};
    SemanticContext ctx;
    TypedScope global_scope = {0};
    char message[256];

    if (out_info == NULL) {
        return semantic_fail(error, "Saida invalida.");
    }

    out_info->globals = NULL;
    out_info->global_count = 0;
    out_info->procedures = NULL;
    out_info->procedure_count = 0;

    if (program == NULL) {
        return semantic_fail(error, "Programa invalido.");
    }

    for (index = 0; index < program->procedure_count; ++index) {
        ProcedureSignature *procedures;
        ASTType *parameter_types;
        size_t parameter_index;

        if (find_procedure(&info, program->procedures[index].name) != NULL) {
            snprintf(message, sizeof(message), "Procedimento '%s' ja declarado.", program->procedures[index].name);
            semantic_info_free(&info);
            return semantic_fail_at(error, program->procedures[index].line, program->procedures[index].column, message);
        }

        procedures = realloc(info.procedures, (info.procedure_count + 1) * sizeof(*info.procedures));
        if (procedures == NULL) {
            semantic_info_free(&info);
            return semantic_fail(error, "Memoria insuficiente.");
        }
        info.procedures = procedures;

        parameter_types = NULL;
        if (program->procedures[index].parameter_count > 0) {
            parameter_types = malloc(program->procedures[index].parameter_count * sizeof(*parameter_types));
            if (parameter_types == NULL) {
                semantic_info_free(&info);
                return semantic_fail(error, "Memoria insuficiente.");
            }
        }

        for (parameter_index = 0; parameter_index < program->procedures[index].parameter_count; ++parameter_index) {
            parameter_types[parameter_index] = program->procedures[index].parameters[parameter_index].type;
        }

        info.procedures[info.procedure_count].name = semantic_strdup(program->procedures[index].name);
        if (info.procedures[info.procedure_count].name == NULL) {
            free(parameter_types);
            semantic_info_free(&info);
            return semantic_fail(error, "Memoria insuficiente.");
        }

        info.procedures[info.procedure_count].return_type = program->procedures[index].return_type;
        info.procedures[info.procedure_count].parameter_types = parameter_types;
        info.procedures[info.procedure_count].parameter_count = program->procedures[index].parameter_count;
        info.procedure_count++;
    }

    for (index = 0; index < program->declaration_count; ++index) {
        const ASTDeclaration *decl = &program->declarations[index];

        if (semantic_info_contains_global(&info, decl->name)) {
            snprintf(message, sizeof(message), "Identificador '%s' ja declarado.", decl->name);
            semantic_info_free(&info);
            typed_scope_free(&global_scope);
            return semantic_fail_declaration(decl, error, message);
        }

        if (decl->type == AST_TYPE_STRING && decl->storage != AST_STORAGE_INDEXED) {
            semantic_info_free(&info);
            typed_scope_free(&global_scope);
            return semantic_fail_declaration(decl, error, "Declaracao de string requer capacidade fixa.");
        }

        if (!semantic_info_append_global(&info, decl) ||
            !typed_scope_append(&global_scope, decl->name, decl->type, decl->storage)) {
            semantic_info_free(&info);
            typed_scope_free(&global_scope);
            return semantic_fail(error, "Memoria insuficiente.");
        }
    }

    for (index = 0; index < program->procedure_count; ++index) {
        if (!analyze_procedure(&program->procedures[index], &info, error)) {
            semantic_info_free(&info);
            typed_scope_free(&global_scope);
            return false;
        }
    }

    ctx.info = &info;
    ctx.current_procedure = NULL;
    ctx.scope = &global_scope;
    ctx.inside_procedure = false;

    if (!analyze_command_list(program->commands, program->command_count, &ctx, NULL, error)) {
        semantic_info_free(&info);
        typed_scope_free(&global_scope);
        return false;
    }

    typed_scope_free(&global_scope);
    *out_info = info;
    return true;
}
