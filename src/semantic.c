#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void symbol_table_free(SymbolTable *symbols) {
    size_t index;

    if (symbols == NULL) {
        return;
    }

    for (index = 0; index < symbols->count; ++index) {
        free(symbols->names[index]);
    }

    free(symbols->names);
    symbols->names = NULL;
    symbols->count = 0;
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

static bool symbol_table_contains(const SymbolTable *symbols, const char *name) {
    size_t index;

    for (index = 0; index < symbols->count; ++index) {
        if (strcmp(symbols->names[index], name) == 0) {
            return true;
        }
    }

    return false;
}

static bool symbol_table_append(SymbolTable *symbols, const char *name) {
    char **names;
    char *copy;

    copy = semantic_strdup(name);
    if (copy == NULL) {
        return false;
    }

    names = realloc(symbols->names, (symbols->count + 1) * sizeof(*symbols->names));
    if (names == NULL) {
        free(copy);
        return false;
    }

    symbols->names = names;
    symbols->names[symbols->count++] = copy;
    return true;
}

static bool semantic_check_identifier(const SymbolTable *symbols, const char *name, int line, int column, CompilerError *error) {
    char message[256];

    if (symbol_table_contains(symbols, name)) {
        return true;
    }

    snprintf(message, sizeof(message), "Identificador '%s' nao declarado.", name);
    return semantic_fail_at(error, line, column, message);
}

static bool analyze_expression(const ASTExpression *expression, const SymbolTable *symbols, CompilerError *error) {
    if (expression == NULL) {
        return semantic_fail(error, "Expressao invalida.");
    }

    switch (expression->type) {
        case AST_EXPR_INT:
            return true;
        case AST_EXPR_IDENTIFIER:
            return semantic_check_identifier(symbols, expression->identifier, expression->line, expression->column, error);
        case AST_EXPR_UNARY:
            return analyze_expression(expression->unary.operand, symbols, error);
        case AST_EXPR_BINARY:
            return analyze_expression(expression->binary.left, symbols, error) &&
                   analyze_expression(expression->binary.right, symbols, error);
        default:
            return semantic_fail_expression(expression, error, "Expressao invalida.");
    }
}

static bool analyze_command_list(
    const ASTCommand *commands, size_t command_count, const SymbolTable *symbols, CompilerError *error);

static bool analyze_command(const ASTCommand *command, const SymbolTable *symbols, CompilerError *error) {
    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            return semantic_check_identifier(
                       symbols, command->assignment.name, command->assignment.line, command->assignment.column, error) &&
                   analyze_expression(command->assignment.expression, symbols, error);
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            return analyze_expression(command->write.expression, symbols, error);
        case AST_COMMAND_IF:
            return analyze_expression(command->if_command.condition, symbols, error) &&
                   analyze_command_list(command->if_command.then_commands, command->if_command.then_count, symbols, error) &&
                   (command->if_command.else_count == 0 ||
                    analyze_command_list(command->if_command.else_commands, command->if_command.else_count, symbols, error));
        case AST_COMMAND_WHILE:
            return analyze_expression(command->while_command.condition, symbols, error) &&
                   analyze_command_list(command->while_command.body_commands, command->while_command.body_count, symbols, error);
        case AST_COMMAND_FOR:
            return semantic_check_identifier(
                       symbols,
                       command->for_command.iterator_name,
                       command->for_command.line,
                       command->for_command.column,
                       error) &&
                   analyze_expression(command->for_command.start_expression, symbols, error) &&
                   analyze_expression(command->for_command.end_expression, symbols, error) &&
                   analyze_expression(command->for_command.step_expression, symbols, error) &&
                   analyze_command_list(command->for_command.body_commands, command->for_command.body_count, symbols, error);
        default:
            return semantic_fail(error, "Comando invalido.");
    }
}

static bool analyze_command_list(
    const ASTCommand *commands, size_t command_count, const SymbolTable *symbols, CompilerError *error) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        if (!analyze_command(&commands[index], symbols, error)) {
            return false;
        }
    }

    return true;
}

bool analyze_program(const ASTProgram *program, SymbolTable *out_symbols, CompilerError *error) {
    size_t index;
    SymbolTable symbols = {0};

    if (out_symbols == NULL) {
        return semantic_fail(error, "Saida invalida.");
    }

    out_symbols->names = NULL;
    out_symbols->count = 0;

    if (program == NULL) {
        return semantic_fail(error, "Programa invalido.");
    }

    for (index = 0; index < program->declaration_count; ++index) {
        char message[256];
        const char *name = program->declarations[index].name;

        if (symbol_table_contains(&symbols, name)) {
            snprintf(message, sizeof(message), "Identificador '%s' ja declarado.", name);
            symbol_table_free(&symbols);
            return semantic_fail_declaration(&program->declarations[index], error, message);
        }

        if (!symbol_table_append(&symbols, name)) {
            symbol_table_free(&symbols);
            return semantic_fail(error, "Memoria insuficiente.");
        }
    }

    if (!analyze_command_list(program->commands, program->command_count, &symbols, error)) {
        symbol_table_free(&symbols);
        return false;
    }

    *out_symbols = symbols;
    return true;
}
