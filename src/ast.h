#ifndef AST_H
#define AST_H

#include <stddef.h>

typedef enum {
    AST_EXPR_INT,
    AST_EXPR_IDENTIFIER,
    AST_EXPR_BINARY
} ASTExpressionType;

typedef enum {
    AST_BINARY_ADD,
    AST_BINARY_SUB,
    AST_BINARY_MUL,
    AST_BINARY_DIV
} ASTBinaryOp;

typedef struct ASTExpression ASTExpression;

struct ASTExpression {
    ASTExpressionType type;
    int line;
    int column;
    union {
        int int_value;
        char *identifier;
        struct {
            ASTBinaryOp op;
            ASTExpression *left;
            ASTExpression *right;
        } binary;
    };
};

typedef struct {
    char *name;
    int line;
    int column;
} ASTDeclaration;

typedef enum {
    AST_COMMAND_ASSIGNMENT,
    AST_COMMAND_WRITE,
    AST_COMMAND_WRITELN
} ASTCommandType;

typedef struct {
    char *name;
    int line;
    int column;
    ASTExpression *expression;
} ASTAssignmentCommand;

typedef struct {
    ASTExpression *expression;
} ASTWriteCommand;

typedef struct {
    ASTCommandType type;
    union {
        ASTAssignmentCommand assignment;
        ASTWriteCommand write;
    };
} ASTCommand;

typedef struct {
    char *name;
    ASTDeclaration *declarations;
    size_t declaration_count;
    ASTCommand *commands;
    size_t command_count;
} ASTProgram;

void ast_expression_free(ASTExpression *expression);
void ast_command_free(ASTCommand *command);
void ast_program_free(ASTProgram *program);

#endif
