#include "ast.h"

#include <stdlib.h>

static void ast_command_list_free(ASTCommand *commands, size_t command_count) {
    size_t index;

    for (index = 0; index < command_count; ++index) {
        ast_command_free(&commands[index]);
    }

    free(commands);
}

void ast_expression_free(ASTExpression *expression) {
    if (expression == NULL) {
        return;
    }

    switch (expression->type) {
        case AST_EXPR_IDENTIFIER:
            free(expression->identifier);
            break;
        case AST_EXPR_UNARY:
            ast_expression_free(expression->unary.operand);
            break;
        case AST_EXPR_BINARY:
            ast_expression_free(expression->binary.left);
            ast_expression_free(expression->binary.right);
            break;
        case AST_EXPR_INT:
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
        case AST_COMMAND_IF: {
            ast_expression_free(command->if_command.condition);
            ast_command_list_free(command->if_command.then_commands, command->if_command.then_count);
            ast_command_list_free(command->if_command.else_commands, command->if_command.else_count);
            break;
        }
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
    size_t index;

    if (program == NULL) {
        return;
    }

    free(program->name);

    for (index = 0; index < program->declaration_count; ++index) {
        free(program->declarations[index].name);
    }
    free(program->declarations);

    ast_command_list_free(program->commands, program->command_count);

    free(program);
}
