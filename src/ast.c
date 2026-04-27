#include "ast.h"

#include <stdlib.h>

static void ast_expression_list_free(ASTExpression **expressions, size_t expression_count) {
    size_t index;

    for (index = 0; index < expression_count; ++index) {
        ast_expression_free(expressions[index]);
    }

    free(expressions);
}

static void ast_call_free(ASTCall *call) {
    if (call == NULL) {
        return;
    }

    free(call->name);
    ast_expression_list_free(call->arguments, call->argument_count);
}

static void ast_command_list_free(ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        ast_command_free(&commands[index]);
    }

    free(commands);
}

static void ast_declaration_list_free(ASTDeclaration *declarations, size_t declaration_count) {
    size_t index;

    for (index = 0; index < declaration_count; ++index) {
        free(declarations[index].name);
    }

    free(declarations);
}

static void ast_parameter_list_free(ASTParameter *parameters, size_t parameter_count) {
    size_t index;

    for (index = 0; index < parameter_count; ++index) {
        free(parameters[index].name);
    }

    free(parameters);
}

static void ast_procedure_list_free(ASTProcedure *procedures, size_t procedure_count) {
    size_t index;

    for (index = 0; index < procedure_count; ++index) {
        ASTProcedure *procedure = &procedures[index];

        free(procedure->name);
        ast_parameter_list_free(procedure->parameters, procedure->parameter_count);
        ast_declaration_list_free(procedure->local_declarations, procedure->local_declaration_count);
        ast_command_list_free(procedure->commands, procedure->command_count);
    }

    free(procedures);
}

void ast_expression_free(ASTExpression *expression) {
    if (expression == NULL) {
        return;
    }

    switch (expression->type) {
        case AST_EXPR_IDENTIFIER:
            free(expression->identifier);
            break;
        case AST_EXPR_CALL:
            ast_call_free(&expression->call);
            break;
        case AST_EXPR_UNARY:
            ast_expression_free(expression->unary.operand);
            break;
        case AST_EXPR_BINARY:
            ast_expression_free(expression->binary.left);
            ast_expression_free(expression->binary.right);
            break;
        case AST_EXPR_INT:
        case AST_EXPR_FLOAT:
        default:
            break;
    }

    free(expression);
}

void ast_command_free(ASTCommand *command) {
    if (command == NULL) {
        return;
    }

    switch (command->type) {
        case AST_COMMAND_ASSIGNMENT:
            free(command->assignment.name);
            ast_expression_free(command->assignment.expression);
            break;
        case AST_COMMAND_READ:
            free(command->read.name);
            break;
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            ast_expression_free(command->write.expression);
            break;
        case AST_COMMAND_CALL:
            ast_call_free(&command->call_command.call);
            break;
        case AST_COMMAND_RETURN:
            ast_expression_free(command->return_command.expression);
            break;
        case AST_COMMAND_IF:
            ast_expression_free(command->if_command.condition);
            ast_command_list_free(command->if_command.then_commands, command->if_command.then_count);
            ast_command_list_free(command->if_command.else_commands, command->if_command.else_count);
            break;
        case AST_COMMAND_WHILE:
            ast_expression_free(command->while_command.condition);
            ast_command_list_free(command->while_command.body_commands, command->while_command.body_count);
            break;
        case AST_COMMAND_FOR:
            free(command->for_command.iterator_name);
            ast_expression_free(command->for_command.start_expression);
            ast_expression_free(command->for_command.end_expression);
            ast_expression_free(command->for_command.step_expression);
            ast_command_list_free(command->for_command.body_commands, command->for_command.body_count);
            break;
        default:
            break;
    }
}

void ast_program_free(ASTProgram *program) {
    if (program == NULL) {
        return;
    }

    free(program->name);
    ast_procedure_list_free(program->procedures, program->procedure_count);
    ast_declaration_list_free(program->declarations, program->declaration_count);
    ast_command_list_free(program->commands, program->command_count);
    free(program);
}
