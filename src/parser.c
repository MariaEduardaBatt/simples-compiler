#include "parser.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const TokenList *tokens;
    size_t current;
} Parser;

static char *parser_strdup(const char *text) {
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

static const Token *parser_current(const Parser *parser) {
    static const Token fallback = {.type = TOK_EOF, .lexeme = "", .line = 1, .column = 1};

    if (parser == NULL || parser->tokens == NULL || parser->tokens->count == 0) {
        return &fallback;
    }

    if (parser->current >= parser->tokens->count) {
        return &parser->tokens->items[parser->tokens->count - 1];
    }

    return &parser->tokens->items[parser->current];
}

static const Token *parser_previous(const Parser *parser) {
    if (parser == NULL || parser->tokens == NULL || parser->current == 0 || parser->tokens->count == 0) {
        return parser_current(parser);
    }

    return &parser->tokens->items[parser->current - 1];
}

static bool parser_fail_at(const Token *token, CompilerError *error, const char *message) {
    int line = 1;
    int column = 1;

    if (token != NULL) {
        line = token->line;
        column = token->column;
    }

    compiler_error_set(error, COMPILER_PHASE_PARSER, line, column, message);
    return false;
}

static bool parser_fail_current(const Parser *parser, CompilerError *error, const char *message) {
    return parser_fail_at(parser_current(parser), error, message);
}

static bool parser_check(const Parser *parser, TokenType type) {
    return parser_current(parser)->type == type;
}

static bool parser_match(Parser *parser, TokenType type) {
    if (!parser_check(parser, type)) {
        return false;
    }

    parser->current++;
    return true;
}

static bool parser_expect(Parser *parser, TokenType type, CompilerError *error, const char *message) {
    if (parser_match(parser, type)) {
        return true;
    }

    return parser_fail_current(parser, error, message);
}

static ASTExpression *parser_make_int_expression(const Token *token, int value) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_INT;
    expression->line = token != NULL ? token->line : 1;
    expression->column = token != NULL ? token->column : 1;
    expression->int_value = value;
    return expression;
}

static ASTExpression *parser_make_identifier_expression(const Token *token) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_IDENTIFIER;
    expression->line = token != NULL ? token->line : 1;
    expression->column = token != NULL ? token->column : 1;
    expression->identifier = parser_strdup(token != NULL ? token->lexeme : NULL);
    if (expression->identifier == NULL) {
        free(expression);
        return NULL;
    }

    return expression;
}

static ASTExpression *parser_make_binary_expression(ASTBinaryOp op, ASTExpression *left, ASTExpression *right) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_BINARY;
    expression->line = left != NULL ? left->line : 1;
    expression->column = left != NULL ? left->column : 1;
    expression->binary.op = op;
    expression->binary.left = left;
    expression->binary.right = right;
    return expression;
}

static ASTExpression *parser_make_unary_expression(const Token *token, ASTUnaryOp op, ASTExpression *operand) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_UNARY;
    expression->line = token != NULL ? token->line : 1;
    expression->column = token != NULL ? token->column : 1;
    expression->unary.op = op;
    expression->unary.operand = operand;
    return expression;
}

static bool parser_append_declaration(ASTProgram *program, ASTDeclaration declaration) {
    ASTDeclaration *items;

    items = realloc(program->declarations, (program->declaration_count + 1) * sizeof(*program->declarations));
    if (items == NULL) {
        return false;
    }

    program->declarations = items;
    program->declarations[program->declaration_count++] = declaration;
    return true;
}

static bool parser_append_command(ASTProgram *program, ASTCommand command) {
    ASTCommand *items;

    items = realloc(program->commands, (program->command_count + 1) * sizeof(*program->commands));
    if (items == NULL) {
        return false;
    }

    program->commands = items;
    program->commands[program->command_count++] = command;
    return true;
}

static bool parser_oom(const Parser *parser, CompilerError *error) {
    return parser_fail_current(parser, error, "Memoria insuficiente.");
}

static ASTExpression *parse_expression(Parser *parser, CompilerError *error);

static ASTExpression *parse_factor(Parser *parser, CompilerError *error) {
    const Token *token;
    ASTExpression *expression;

    if (parser_match(parser, TOK_ID)) {
        token = parser_previous(parser);
        expression = parser_make_identifier_expression(token);
        if (expression == NULL) {
            parser_oom(parser, error);
        }
        return expression;
    }

    if (parser_match(parser, TOK_NUM_INT)) {
        long value;

        token = parser_previous(parser);
        errno = 0;
        value = strtol(token->lexeme, NULL, 10);
        if (errno == ERANGE || value < INT_MIN || value > INT_MAX) {
            parser_fail_at(token, error, "Literal inteiro fora do intervalo.");
            return NULL;
        }

        expression = parser_make_int_expression(token, (int)value);
        if (expression == NULL) {
            parser_oom(parser, error);
        }
        return expression;
    }

    if (parser_match(parser, TOK_ABRE_PAR)) {
        expression = parse_expression(parser, error);
        if (expression == NULL) {
            return NULL;
        }

        if (!parser_expect(parser, TOK_FECHA_PAR, error, "Esperado ')'.")) {
            ast_expression_free(expression);
            return NULL;
        }

        return expression;
    }

    parser_fail_current(parser, error, "Esperado fator.");
    return NULL;
}

static ASTExpression *parse_unary(Parser *parser, CompilerError *error) {
    if (parser_match(parser, TOK_NAO) || parser_match(parser, TOK_MENOS)) {
        const Token *operator_token = parser_previous(parser);
        ASTUnaryOp op = operator_token->type == TOK_NAO ? AST_UNARY_NOT : AST_UNARY_NEGATE;
        ASTExpression *operand = parse_unary(parser, error);
        ASTExpression *expression;

        if (operand == NULL) {
            return NULL;
        }

        expression = parser_make_unary_expression(operator_token, op, operand);
        if (expression == NULL) {
            ast_expression_free(operand);
            parser_oom(parser, error);
            return NULL;
        }

        return expression;
    }

    return parse_factor(parser, error);
}

static ASTExpression *parse_multiplicative(Parser *parser, CompilerError *error) {
    ASTExpression *left = parse_unary(parser, error);

    if (left == NULL) {
        return NULL;
    }

    while (parser_check(parser, TOK_MULT) || parser_check(parser, TOK_DIV)) {
        ASTBinaryOp op;
        ASTExpression *right;
        ASTExpression *combined;

        parser_match(parser, parser_current(parser)->type);
        op = parser_previous(parser)->type == TOK_MULT ? AST_BINARY_MUL : AST_BINARY_DIV;
        right = parse_unary(parser, error);
        if (right == NULL) {
            ast_expression_free(left);
            return NULL;
        }

        combined = parser_make_binary_expression(op, left, right);
        if (combined == NULL) {
            ast_expression_free(left);
            ast_expression_free(right);
            parser_oom(parser, error);
            return NULL;
        }

        left = combined;
    }

    return left;
}

static ASTExpression *parse_additive(Parser *parser, CompilerError *error) {
    ASTExpression *left = parse_multiplicative(parser, error);

    if (left == NULL) {
        return NULL;
    }

    while (parser_check(parser, TOK_MAIS) || parser_check(parser, TOK_MENOS)) {
        ASTBinaryOp op;
        ASTExpression *right;
        ASTExpression *combined;

        parser_match(parser, parser_current(parser)->type);
        op = parser_previous(parser)->type == TOK_MAIS ? AST_BINARY_ADD : AST_BINARY_SUB;
        right = parse_multiplicative(parser, error);
        if (right == NULL) {
            ast_expression_free(left);
            return NULL;
        }

        combined = parser_make_binary_expression(op, left, right);
        if (combined == NULL) {
            ast_expression_free(left);
            ast_expression_free(right);
            parser_oom(parser, error);
            return NULL;
        }

        left = combined;
    }

    return left;
}

static ASTBinaryOp parser_relational_op(TokenType type) {
    switch (type) {
        case TOK_MAIOR:
            return AST_BINARY_GT;
        case TOK_MENOR:
            return AST_BINARY_LT;
        case TOK_IGUAL:
            return AST_BINARY_EQ;
        case TOK_DIFERENTE:
            return AST_BINARY_NE;
        case TOK_MAIOR_IGUAL:
            return AST_BINARY_GE;
        case TOK_MENOR_IGUAL:
            return AST_BINARY_LE;
        default:
            abort();
    }
}

static ASTExpression *parse_relational(Parser *parser, CompilerError *error) {
    ASTExpression *left = parse_additive(parser, error);

    if (left == NULL) {
        return NULL;
    }

    if (parser_check(parser, TOK_MAIOR) || parser_check(parser, TOK_MENOR) ||
        parser_check(parser, TOK_IGUAL) || parser_check(parser, TOK_DIFERENTE) ||
        parser_check(parser, TOK_MAIOR_IGUAL) || parser_check(parser, TOK_MENOR_IGUAL)) {
        ASTBinaryOp op;
        ASTExpression *right;
        ASTExpression *combined;

        parser_match(parser, parser_current(parser)->type);
        op = parser_relational_op(parser_previous(parser)->type);
        right = parse_additive(parser, error);
        if (right == NULL) {
            ast_expression_free(left);
            return NULL;
        }

        combined = parser_make_binary_expression(op, left, right);
        if (combined == NULL) {
            ast_expression_free(left);
            ast_expression_free(right);
            parser_oom(parser, error);
            return NULL;
        }

        left = combined;
    }

    return left;
}

static ASTExpression *parse_logical(Parser *parser, CompilerError *error) {
    ASTExpression *left = parse_relational(parser, error);

    if (left == NULL) {
        return NULL;
    }

    while (parser_check(parser, TOK_E) || parser_check(parser, TOK_OU)) {
        ASTBinaryOp op;
        ASTExpression *right;
        ASTExpression *combined;

        parser_match(parser, parser_current(parser)->type);
        op = parser_previous(parser)->type == TOK_E ? AST_BINARY_AND : AST_BINARY_OR;
        right = parse_relational(parser, error);
        if (right == NULL) {
            ast_expression_free(left);
            return NULL;
        }

        combined = parser_make_binary_expression(op, left, right);
        if (combined == NULL) {
            ast_expression_free(left);
            ast_expression_free(right);
            parser_oom(parser, error);
            return NULL;
        }

        left = combined;
    }

    return left;
}

static ASTExpression *parse_expression(Parser *parser, CompilerError *error) {
    return parse_logical(parser, error);
}

static bool parse_declaration_list(Parser *parser, ASTProgram *program, CompilerError *error) {
    while (parser_match(parser, TOK_INTEIRO)) {
        do {
            const Token *name_token;
            ASTDeclaration declaration = {0};

            if (!parser_expect(parser, TOK_ID, error, "Esperado identificador na declaracao.")) {
                return false;
            }

            name_token = parser_previous(parser);
            declaration.name = parser_strdup(name_token->lexeme);
            if (declaration.name == NULL) {
                return parser_oom(parser, error);
            }
            declaration.line = name_token->line;
            declaration.column = name_token->column;

            if (!parser_append_declaration(program, declaration)) {
                free(declaration.name);
                return parser_oom(parser, error);
            }
        } while (parser_match(parser, TOK_VIRGULA));

        if (!parser_expect(parser, TOK_PONTO_VIRGULA, error, "Esperado ';' apos declaracao.")) {
            return false;
        }
    }

    return true;
}

static bool parse_command(Parser *parser, ASTCommand *command, CompilerError *error) {
    memset(command, 0, sizeof(*command));

    if (parser_match(parser, TOK_ID)) {
        const Token *name_token = parser_previous(parser);

        command->type = AST_COMMAND_ASSIGNMENT;
        command->assignment.name = parser_strdup(name_token->lexeme);
        if (command->assignment.name == NULL) {
            return parser_oom(parser, error);
        }
        command->assignment.line = name_token->line;
        command->assignment.column = name_token->column;

        if (!parser_expect(parser, TOK_ATRIB, error, "Esperado '<-' na atribuicao.")) {
            ast_command_free(command);
            return false;
        }

        command->assignment.expression = parse_expression(parser, error);
        if (command->assignment.expression == NULL) {
            ast_command_free(command);
            return false;
        }

        return true;
    }

    if (parser_match(parser, TOK_ESCREVA) || parser_match(parser, TOK_ESCREVAL)) {
        command->type = parser_previous(parser)->type == TOK_ESCREVA ? AST_COMMAND_WRITE : AST_COMMAND_WRITELN;
        command->write.expression = parse_expression(parser, error);
        if (command->write.expression == NULL) {
            ast_command_free(command);
            return false;
        }

        return true;
    }

    return parser_fail_current(parser, error, "Esperado comando.");
}

static bool parse_command_list(Parser *parser, ASTProgram *program, CompilerError *error) {
    while (!parser_check(parser, TOK_FIM) && !parser_check(parser, TOK_EOF)) {
        ASTCommand command;

        if (!parse_command(parser, &command, error)) {
            return false;
        }

        if (!parser_expect(parser, TOK_PONTO_VIRGULA, error, "Esperado ';' apos comando.")) {
            ast_command_free(&command);
            return false;
        }

        if (!parser_append_command(program, command)) {
            ast_command_free(&command);
            return parser_oom(parser, error);
        }
    }

    return true;
}

bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error) {
    Parser parser = {.tokens = tokens, .current = 0};
    ASTProgram *program;

    if (out_program == NULL) {
        compiler_error_set(error, COMPILER_PHASE_PARSER, 1, 1, "Saida invalida.");
        return false;
    }

    *out_program = NULL;

    if (tokens == NULL || tokens->count == 0) {
        compiler_error_set(error, COMPILER_PHASE_PARSER, 1, 1, "Lista de tokens invalida.");
        return false;
    }

    program = calloc(1, sizeof(*program));
    if (program == NULL) {
        compiler_error_set(error, COMPILER_PHASE_PARSER, 1, 1, "Memoria insuficiente.");
        return false;
    }

    if (!parser_expect(&parser, TOK_PROGRAMA, error, "Esperado 'programa'.") ||
        !parser_expect(&parser, TOK_ID, error, "Esperado identificador do programa.")) {
        ast_program_free(program);
        return false;
    }

    program->name = parser_strdup(parser_previous(&parser)->lexeme);
    if (program->name == NULL) {
        ast_program_free(program);
        compiler_error_set(error, COMPILER_PHASE_PARSER, parser_previous(&parser)->line, parser_previous(&parser)->column, "Memoria insuficiente.");
        return false;
    }

    if (!parse_declaration_list(&parser, program, error) ||
        !parser_expect(&parser, TOK_INICIO, error, "Esperado 'inicio'.") ||
        !parse_command_list(&parser, program, error) ||
        !parser_expect(&parser, TOK_FIM, error, "Esperado 'fim'.") ||
        !parser_expect(&parser, TOK_EOF, error, "Esperado fim do arquivo.")) {
        ast_program_free(program);
        return false;
    }

    *out_program = program;
    return true;
}
