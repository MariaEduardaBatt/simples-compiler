#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    AST_EXPR_INT,
    AST_EXPR_IDENTIFIER,
    AST_EXPR_UNARY,
    AST_EXPR_BINARY
} ASTExpressionType;

typedef enum {
    AST_UNARY_NEGATE,
    AST_UNARY_NOT
} ASTUnaryOp;

typedef enum {
    AST_BINARY_ADD,
    AST_BINARY_SUB,
    AST_BINARY_MUL,
    AST_BINARY_DIV,
    AST_BINARY_GT,
    AST_BINARY_LT,
    AST_BINARY_EQ,
    AST_BINARY_NE,
    AST_BINARY_GE,
    AST_BINARY_LE,
    AST_BINARY_AND,
    AST_BINARY_OR
} ASTBinaryOp;

typedef struct ASTExpression ASTExpression;
typedef struct ASTCommand ASTCommand;

struct ASTExpression {
    ASTExpressionType type;
    int line;
    int column;
    union {
        int int_value;
        char *identifier;
        struct {
            ASTUnaryOp op;
            ASTExpression *operand;
        } unary;
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

typedef struct {
    char *name;
    int line;
    int column;
} ASTReadCommand;

typedef enum {
    AST_COMMAND_ASSIGNMENT,
    AST_COMMAND_READ,
    AST_COMMAND_WRITE,
    AST_COMMAND_WRITELN,
    AST_COMMAND_IF,
    AST_COMMAND_WHILE,
    AST_COMMAND_FOR
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
    ASTExpression *condition;
    ASTCommand *then_commands;
    size_t then_count;
    ASTCommand *else_commands;
    size_t else_count;
} ASTIfCommand;

typedef struct {
    ASTExpression *condition;
    ASTCommand *body_commands;
    size_t body_count;
} ASTWhileCommand;

typedef struct {
    char *iterator_name;
    int line;
    int column;
    ASTExpression *start_expression;
    ASTExpression *end_expression;
    ASTExpression *step_expression;
    ASTCommand *body_commands;
    size_t body_count;
} ASTForCommand;

struct ASTCommand {
    ASTCommandType type;
    union {
        ASTAssignmentCommand assignment;
        ASTReadCommand read;
        ASTWriteCommand write;
        ASTIfCommand if_command;
        ASTWhileCommand while_command;
        ASTForCommand for_command;
    };
};

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
