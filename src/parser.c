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

static bool parser_oom(const Parser *parser, CompilerError *error) {
    return parser_fail_current(parser, error, "Memoria insuficiente.");
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

static ASTExpression *parser_make_float_expression(const Token *token, double value) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_FLOAT;
    expression->line = token != NULL ? token->line : 1;
    expression->column = token != NULL ? token->column : 1;
    expression->float_value = value;
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

static ASTExpression *parser_make_call_expression(const Token *token, ASTExpression **arguments, size_t argument_count) {
    ASTExpression *expression = calloc(1, sizeof(*expression));

    if (expression == NULL) {
        return NULL;
    }

    expression->type = AST_EXPR_CALL;
    expression->line = token != NULL ? token->line : 1;
    expression->column = token != NULL ? token->column : 1;
    expression->call.name = parser_strdup(token != NULL ? token->lexeme : NULL);
    if (expression->call.name == NULL) {
        free(expression);
        return NULL;
    }
    expression->call.line = token != NULL ? token->line : 1;
    expression->call.column = token != NULL ? token->column : 1;
    expression->call.arguments = arguments;
    expression->call.argument_count = argument_count;
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

static bool parser_append_declaration(ASTDeclaration **declarations, size_t *declaration_count, ASTDeclaration declaration) {
    ASTDeclaration *items = realloc(*declarations, (*declaration_count + 1) * sizeof(**declarations));

    if (items == NULL) {
        return false;
    }

    *declarations = items;
    (*declarations)[(*declaration_count)++] = declaration;
    return true;
}

static bool parser_append_parameter(ASTParameter **parameters, size_t *parameter_count, ASTParameter parameter) {
    ASTParameter *items = realloc(*parameters, (*parameter_count + 1) * sizeof(**parameters));

    if (items == NULL) {
        return false;
    }

    *parameters = items;
    (*parameters)[(*parameter_count)++] = parameter;
    return true;
}

static bool parser_append_command(ASTCommand **commands, size_t *count, ASTCommand command) {
    ASTCommand *items = realloc(*commands, (*count + 1) * sizeof(**commands));

    if (items == NULL) {
        return false;
    }

    *commands = items;
    (*commands)[(*count)++] = command;
    return true;
}

static bool parser_append_procedure(ASTProgram *program, ASTProcedure procedure) {
    ASTProcedure *items = realloc(program->procedures, (program->procedure_count + 1) * sizeof(*program->procedures));

    if (items == NULL) {
        return false;
    }

    program->procedures = items;
    program->procedures[program->procedure_count++] = procedure;
    return true;
}

static bool parser_append_argument(ASTExpression ***arguments, size_t *argument_count, ASTExpression *argument) {
    ASTExpression **items = realloc(*arguments, (*argument_count + 1) * sizeof(**arguments));

    if (items == NULL) {
        return false;
    }

    *arguments = items;
    (*arguments)[(*argument_count)++] = argument;
    return true;
}

static bool parse_type(Parser *parser, bool allow_void, ASTType *out_type, CompilerError *error) {
    if (parser_match(parser, TOK_INTEIRO)) {
        *out_type = AST_TYPE_INTEIRO;
        return true;
    }

    if (parser_match(parser, TOK_FLUTUANTE)) {
        *out_type = AST_TYPE_FLUTUANTE;
        return true;
    }

    if (allow_void && parser_match(parser, TOK_VAZIO)) {
        *out_type = AST_TYPE_VAZIO;
        return true;
    }

    return parser_fail_current(parser, error, allow_void ? "Esperado tipo de retorno." : "Esperado tipo.");
}

static bool parser_is_declaration_type(const Parser *parser) {
    return parser_check(parser, TOK_INTEIRO) || parser_check(parser, TOK_FLUTUANTE);
}

static ASTExpression *parse_expression(Parser *parser, CompilerError *error);
static bool parse_command(Parser *parser, ASTCommand *command, CompilerError *error);

static bool parser_command_requires_semicolon(ASTCommandType type) {
    return type == AST_COMMAND_ASSIGNMENT ||
           type == AST_COMMAND_READ ||
           type == AST_COMMAND_WRITE ||
           type == AST_COMMAND_WRITELN ||
           type == AST_COMMAND_CALL ||
           type == AST_COMMAND_RETURN;
}

static bool parser_is_terminator(const Parser *parser, const TokenType *terminators, size_t terminator_count) {
    size_t index;

    for (index = 0; index < terminator_count; ++index) {
        if (parser_check(parser, terminators[index])) {
            return true;
        }
    }

    return false;
}

static bool parse_command_list(
    Parser *parser,
    ASTCommand **commands,
    size_t *command_count,
    const TokenType *terminators,
    size_t terminator_count,
    CompilerError *error) {
    while (!parser_is_terminator(parser, terminators, terminator_count) && !parser_check(parser, TOK_EOF)) {
        ASTCommand command;

        if (!parse_command(parser, &command, error)) {
            return false;
        }

        if (parser_command_requires_semicolon(command.type) &&
            !parser_expect(parser, TOK_PONTO_VIRGULA, error, "Esperado ';' apos comando.")) {
            ast_command_free(&command);
            return false;
        }

        if (!parser_append_command(commands, command_count, command)) {
            ast_command_free(&command);
            return parser_oom(parser, error);
        }
    }

    return true;
}

static bool parse_argument_list(Parser *parser, ASTExpression ***arguments, size_t *argument_count, CompilerError *error) {
    *arguments = NULL;
    *argument_count = 0;

    if (parser_match(parser, TOK_FECHA_PAR)) {
        return true;
    }

    do {
        ASTExpression *argument = parse_expression(parser, error);

        if (argument == NULL) {
            ast_expression_list_free(*arguments, *argument_count);
            *arguments = NULL;
            *argument_count = 0;
            return false;
        }

        if (!parser_append_argument(arguments, argument_count, argument)) {
            ast_expression_free(argument);
            ast_expression_list_free(*arguments, *argument_count);
            *arguments = NULL;
            *argument_count = 0;
            return parser_oom(parser, error);
        }
    } while (parser_match(parser, TOK_VIRGULA));

    if (!parser_expect(parser, TOK_FECHA_PAR, error, "Esperado ')'.")) {
        ast_expression_list_free(*arguments, *argument_count);
        *arguments = NULL;
        *argument_count = 0;
        return false;
    }

    return true;
}

static ASTExpression *parse_factor(Parser *parser, CompilerError *error) {
    const Token *token;
    ASTExpression *expression;

    if (parser_match(parser, TOK_ID)) {
        ASTExpression **arguments = NULL;
        size_t argument_count = 0;

        token = parser_previous(parser);
        if (!parser_match(parser, TOK_ABRE_PAR)) {
            expression = parser_make_identifier_expression(token);
            if (expression == NULL) {
                parser_oom(parser, error);
            }
            return expression;
        }

        if (!parse_argument_list(parser, &arguments, &argument_count, error)) {
            return NULL;
        }

        expression = parser_make_call_expression(token, arguments, argument_count);
        if (expression == NULL) {
            ast_expression_list_free(arguments, argument_count);
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

    if (parser_match(parser, TOK_NUM_FLOAT)) {
        double value;

        token = parser_previous(parser);
        errno = 0;
        value = strtod(token->lexeme, NULL);
        if (errno == ERANGE) {
            parser_fail_at(token, error, "Literal flutuante fora do intervalo.");
            return NULL;
        }
        expression = parser_make_float_expression(token, value);
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

static bool parse_declaration_list(Parser *parser, ASTDeclaration **declarations, size_t *declaration_count, CompilerError *error) {
    while (parser_is_declaration_type(parser)) {
        ASTType declaration_type;

        if (!parse_type(parser, false, &declaration_type, error)) {
            return false;
        }

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
            declaration.type = declaration_type;
            declaration.line = name_token->line;
            declaration.column = name_token->column;

            if (!parser_append_declaration(declarations, declaration_count, declaration)) {
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

static bool parse_parameter_list(Parser *parser, ASTParameter **parameters, size_t *parameter_count, CompilerError *error) {
    *parameters = NULL;
    *parameter_count = 0;

    if (parser_match(parser, TOK_FECHA_PAR)) {
        return true;
    }

    do {
        ASTType parameter_type;
        ASTParameter parameter = {0};
        const Token *name_token;

        if (!parse_type(parser, false, &parameter_type, error)) {
            ast_parameter_list_free(*parameters, *parameter_count);
            *parameters = NULL;
            *parameter_count = 0;
            return false;
        }

        if (!parser_expect(parser, TOK_ID, error, "Esperado identificador no parametro.")) {
            ast_parameter_list_free(*parameters, *parameter_count);
            *parameters = NULL;
            *parameter_count = 0;
            return false;
        }

        name_token = parser_previous(parser);
        parameter.name = parser_strdup(name_token->lexeme);
        if (parameter.name == NULL) {
            ast_parameter_list_free(*parameters, *parameter_count);
            *parameters = NULL;
            *parameter_count = 0;
            return parser_oom(parser, error);
        }
        parameter.type = parameter_type;
        parameter.line = name_token->line;
        parameter.column = name_token->column;

        if (!parser_append_parameter(parameters, parameter_count, parameter)) {
            free(parameter.name);
            ast_parameter_list_free(*parameters, *parameter_count);
            *parameters = NULL;
            *parameter_count = 0;
            return parser_oom(parser, error);
        }
    } while (parser_match(parser, TOK_VIRGULA));

    if (!parser_expect(parser, TOK_FECHA_PAR, error, "Esperado ')'.")) {
        ast_parameter_list_free(*parameters, *parameter_count);
        *parameters = NULL;
        *parameter_count = 0;
        return false;
    }

    return true;
}

static bool parse_procedure(Parser *parser, ASTProcedure *procedure, CompilerError *error) {
    static const TokenType command_terminators[] = {TOK_FIM};
    const Token *name_token;

    memset(procedure, 0, sizeof(*procedure));

    if (!parser_expect(parser, TOK_PROCEDIMENTO, error, "Esperado 'procedimento'.") ||
        !parse_type(parser, true, &procedure->return_type, error) ||
        !parser_expect(parser, TOK_ID, error, "Esperado identificador do procedimento.")) {
        return false;
    }

    name_token = parser_previous(parser);
    procedure->name = parser_strdup(name_token->lexeme);
    if (procedure->name == NULL) {
        return parser_oom(parser, error);
    }
    procedure->line = name_token->line;
    procedure->column = name_token->column;

    if (!parser_expect(parser, TOK_ABRE_PAR, error, "Esperado '('.") ||
        !parse_parameter_list(parser, &procedure->parameters, &procedure->parameter_count, error) ||
        !parser_expect(parser, TOK_INICIO, error, "Esperado 'inicio'.") ||
        !parse_declaration_list(parser, &procedure->local_declarations, &procedure->local_declaration_count, error) ||
        !parse_command_list(
            parser,
            &procedure->commands,
            &procedure->command_count,
            command_terminators,
            sizeof(command_terminators) / sizeof(command_terminators[0]),
            error) ||
        !parser_expect(parser, TOK_FIM, error, "Esperado 'fim'.")) {
        ast_procedure_free(procedure);
        return false;
    }

    return true;
}

static bool parse_command(Parser *parser, ASTCommand *command, CompilerError *error) {
    memset(command, 0, sizeof(*command));

    if (parser_match(parser, TOK_ID)) {
        const Token *name_token = parser_previous(parser);

        if (parser_match(parser, TOK_ATRIB)) {
            command->type = AST_COMMAND_ASSIGNMENT;
            command->assignment.name = parser_strdup(name_token->lexeme);
            if (command->assignment.name == NULL) {
                return parser_oom(parser, error);
            }
            command->assignment.line = name_token->line;
            command->assignment.column = name_token->column;
            command->assignment.expression = parse_expression(parser, error);
            if (command->assignment.expression == NULL) {
                ast_command_free(command);
                return false;
            }
            return true;
        }

        if (parser_match(parser, TOK_ABRE_PAR)) {
            command->type = AST_COMMAND_CALL;
            command->call_command.call.name = parser_strdup(name_token->lexeme);
            if (command->call_command.call.name == NULL) {
                return parser_oom(parser, error);
            }
            command->call_command.call.line = name_token->line;
            command->call_command.call.column = name_token->column;
            if (!parse_argument_list(
                    parser,
                    &command->call_command.call.arguments,
                    &command->call_command.call.argument_count,
                    error)) {
                ast_command_free(command);
                return false;
            }
            return true;
        }

        return parser_fail_at(name_token, error, "Esperado '<-' ou '(' apos identificador.");
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

    if (parser_match(parser, TOK_LEIA)) {
        const Token *name_token;

        command->type = AST_COMMAND_READ;
        if (!parser_expect(parser, TOK_ID, error, "Esperado identificador apos 'leia'.")) {
            return false;
        }

        name_token = parser_previous(parser);
        command->read.name = parser_strdup(name_token->lexeme);
        if (command->read.name == NULL) {
            return parser_oom(parser, error);
        }

        command->read.line = name_token->line;
        command->read.column = name_token->column;
        return true;
    }

    if (parser_match(parser, TOK_RETORNA)) {
        const Token *return_token = parser_previous(parser);

        command->type = AST_COMMAND_RETURN;
        command->return_command.line = return_token->line;
        command->return_command.column = return_token->column;
        if (!parser_check(parser, TOK_PONTO_VIRGULA)) {
            command->return_command.expression = parse_expression(parser, error);
            if (command->return_command.expression == NULL) {
                ast_command_free(command);
                return false;
            }
        }
        return true;
    }

    if (parser_match(parser, TOK_SE)) {
        static const TokenType then_terminators[] = {TOK_SENAO, TOK_FIMSE};
        static const TokenType else_terminators[] = {TOK_FIMSE};

        command->type = AST_COMMAND_IF;
        command->if_command.condition = parse_expression(parser, error);
        if (command->if_command.condition == NULL) {
            ast_command_free(command);
            return false;
        }

        if (!parser_expect(parser, TOK_ENTAO, error, "Esperado 'entao'.")) {
            ast_command_free(command);
            return false;
        }

        if (!parse_command_list(
                parser,
                &command->if_command.then_commands,
                &command->if_command.then_count,
                then_terminators,
                sizeof(then_terminators) / sizeof(then_terminators[0]),
                error)) {
            ast_command_free(command);
            return false;
        }

        if (parser_match(parser, TOK_SENAO)) {
            if (!parse_command_list(
                    parser,
                    &command->if_command.else_commands,
                    &command->if_command.else_count,
                    else_terminators,
                    sizeof(else_terminators) / sizeof(else_terminators[0]),
                    error)) {
                ast_command_free(command);
                return false;
            }
        }

        if (!parser_expect(parser, TOK_FIMSE, error, "Esperado 'fimse'.")) {
            ast_command_free(command);
            return false;
        }

        return true;
    }

    if (parser_match(parser, TOK_ENQUANTO)) {
        static const TokenType body_terminators[] = {TOK_FIMENQUANTO};

        command->type = AST_COMMAND_WHILE;
        command->while_command.condition = parse_expression(parser, error);
        if (command->while_command.condition == NULL) {
            ast_command_free(command);
            return false;
        }

        if (!parser_expect(parser, TOK_FACA, error, "Esperado 'faca'.")) {
            ast_command_free(command);
            return false;
        }

        if (!parse_command_list(
                parser,
                &command->while_command.body_commands,
                &command->while_command.body_count,
                body_terminators,
                sizeof(body_terminators) / sizeof(body_terminators[0]),
                error)) {
            ast_command_free(command);
            return false;
        }

        if (!parser_expect(parser, TOK_FIMENQUANTO, error, "Esperado 'fimenquanto'.")) {
            ast_command_free(command);
            return false;
        }

        return true;
    }

    if (parser_match(parser, TOK_PARA)) {
        static const TokenType body_terminators[] = {TOK_FIMPARA};
        const Token *iterator_token;

        command->type = AST_COMMAND_FOR;

        if (!parser_expect(parser, TOK_ID, error, "Esperado identificador apos 'para'.")) {
            ast_command_free(command);
            return false;
        }

        iterator_token = parser_previous(parser);
        command->for_command.iterator_name = parser_strdup(iterator_token->lexeme);
        if (command->for_command.iterator_name == NULL) {
            ast_command_free(command);
            return parser_oom(parser, error);
        }
        command->for_command.line = iterator_token->line;
        command->for_command.column = iterator_token->column;

        if (!parser_expect(parser, TOK_DE, error, "Esperado 'de'.") ||
            (command->for_command.start_expression = parse_expression(parser, error)) == NULL ||
            !parser_expect(parser, TOK_ATE, error, "Esperado 'ate'.") ||
            (command->for_command.end_expression = parse_expression(parser, error)) == NULL ||
            !parser_expect(parser, TOK_PASSO, error, "Esperado 'passo'.") ||
            (command->for_command.step_expression = parse_expression(parser, error)) == NULL ||
            !parser_expect(parser, TOK_FACA, error, "Esperado 'faca'.")) {
            ast_command_free(command);
            return false;
        }

        if (!parse_command_list(
                parser,
                &command->for_command.body_commands,
                &command->for_command.body_count,
                body_terminators,
                sizeof(body_terminators) / sizeof(body_terminators[0]),
                error)) {
            ast_command_free(command);
            return false;
        }

        if (!parser_expect(parser, TOK_FIMPARA, error, "Esperado 'fimpara'.")) {
            ast_command_free(command);
            return false;
        }

        return true;
    }

    return parser_fail_current(parser, error, "Esperado comando.");
}

bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error) {
    Parser parser = {.tokens = tokens, .current = 0};
    ASTProgram *program;
    static const TokenType command_terminators[] = {TOK_FIM};

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

    while (parser_check(&parser, TOK_PROCEDIMENTO)) {
        ASTProcedure procedure;

        if (!parse_procedure(&parser, &procedure, error)) {
            ast_program_free(program);
            return false;
        }

        if (!parser_append_procedure(program, procedure)) {
            ast_procedure_free(&procedure);
            ast_program_free(program);
            compiler_error_set(error, COMPILER_PHASE_PARSER, parser_current(&parser)->line, parser_current(&parser)->column, "Memoria insuficiente.");
            return false;
        }
    }

    if (!parser_expect(&parser, TOK_PROGRAMA, error, "Esperado 'programa'.") ||
        !parser_expect(&parser, TOK_ID, error, "Esperado identificador do programa.")) {
        ast_program_free(program);
        return false;
    }

    program->name = parser_strdup(parser_previous(&parser)->lexeme);
    if (program->name == NULL) {
        ast_program_free(program);
        compiler_error_set(
            error,
            COMPILER_PHASE_PARSER,
            parser_previous(&parser)->line,
            parser_previous(&parser)->column,
            "Memoria insuficiente.");
        return false;
    }

    if (!parse_declaration_list(&parser, &program->declarations, &program->declaration_count, error) ||
        !parser_expect(&parser, TOK_INICIO, error, "Esperado 'inicio'.") ||
        !parse_command_list(
            &parser,
            &program->commands,
            &program->command_count,
            command_terminators,
            sizeof(command_terminators) / sizeof(command_terminators[0]),
            error) ||
        !parser_expect(&parser, TOK_FIM, error, "Esperado 'fim'.") ||
        !parser_expect(&parser, TOK_EOF, error, "Esperado fim do arquivo.")) {
        ast_program_free(program);
        return false;
    }

    *out_program = program;
    return true;
}
