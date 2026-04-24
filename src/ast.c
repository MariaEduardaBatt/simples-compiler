#include "ast.h"

#include <stdlib.h>

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
        case AST_COMMAND_WRITE:
        case AST_COMMAND_WRITELN:
            ast_expression_free(command->write.expression);
            break;
        case AST_COMMAND_IF: {
            size_t index;

            ast_expression_free(command->if_command.condition);
            for (index = 0; index < command->if_command.then_count; ++index) {
                ast_command_free(&command->if_command.then_commands[index]);
            }
            free(command->if_command.then_commands);
            for (index = 0; index < command->if_command.else_count; ++index) {
                ast_command_free(&command->if_command.else_commands[index]);
            }
            free(command->if_command.else_commands);
            break;
        }
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

    for (index = 0; index < program->command_count; ++index) {
        ast_command_free(&program->commands[index]);
    }
    free(program->commands);

    free(program);
}
