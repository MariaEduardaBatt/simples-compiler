#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    AST_TYPE_INTEIRO,
    AST_TYPE_FLUTUANTE,
    AST_TYPE_VAZIO
} ASTType;

typedef enum {
    AST_EXPR_INT,
    AST_EXPR_FLOAT,
    AST_EXPR_IDENTIFIER,
    AST_EXPR_CALL,
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

typedef struct {
    char *name;
    ASTType type;
    int line;
    int column;
} ASTDeclaration;

typedef struct {
    char *name;
    ASTType type;
    int line;
    int column;
} ASTParameter;

typedef struct {
    char *name;
    int line;
    int column;
    ASTExpression **arguments;
    size_t argument_count;
} ASTCall;

struct ASTExpression {
    ASTExpressionType type;
    int line;
    int column;
    union {
        int int_value;
        double float_value;
        char *identifier;
        ASTCall call;
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
} ASTReadCommand;

typedef enum {
    AST_COMMAND_ASSIGNMENT,
    AST_COMMAND_READ,
    AST_COMMAND_WRITE,
    AST_COMMAND_WRITELN,
    AST_COMMAND_CALL,
    AST_COMMAND_RETURN,
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
    ASTCall call;
} ASTCallCommand;

typedef struct {
    ASTExpression *expression;
    int line;
    int column;
} ASTReturnCommand;

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
        ASTCallCommand call_command;
        ASTReturnCommand return_command;
        ASTIfCommand if_command;
        ASTWhileCommand while_command;
        ASTForCommand for_command;
    };
};

typedef struct {
    char *name;
    ASTType return_type;
    ASTParameter *parameters;
    size_t parameter_count;
    ASTDeclaration *local_declarations;
    size_t local_declaration_count;
    ASTCommand *commands;
    size_t command_count;
    int line;
    int column;
} ASTProcedure;

typedef struct {
    char *name;
    ASTProcedure *procedures;
    size_t procedure_count;
    ASTDeclaration *declarations;
    size_t declaration_count;
    ASTCommand *commands;
    size_t command_count;
} ASTProgram;

void ast_expression_free(ASTExpression *expression);
void ast_expression_list_free(ASTExpression **expressions, size_t expression_count);
void ast_declaration_list_free(ASTDeclaration *declarations, size_t declaration_count);
void ast_parameter_list_free(ASTParameter *parameters, size_t parameter_count);
void ast_command_free(ASTCommand *command);
void ast_procedure_free(ASTProcedure *procedure);
void ast_program_free(ASTProgram *program);

#endif
