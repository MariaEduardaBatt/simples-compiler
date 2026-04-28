#include "lexer.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    const char *source;
    size_t index;
    int line;
    int column;
} LexerState;

static char lexer_peek(const LexerState *state) {
    return state->source[state->index];
}

static void lexer_advance(LexerState *state) {
    char current = lexer_peek(state);

    if (current == '\0') {
        return;
    }

    state->index++;
    if (current == '\n') {
        state->line++;
        state->column = 1;
    } else {
        state->column++;
    }
}

static bool lexer_is_identifier_start(char current) {
    return isalpha((unsigned char)current) || current == '_';
}

static bool lexer_is_identifier_part(char current) {
    return isalnum((unsigned char)current) || current == '_';
}

static bool lexer_span_is(const char *start, size_t length, const char *text) {
    return strlen(text) == length && strncmp(start, text, length) == 0;
}

static TokenType lexer_keyword_type(const char *start, size_t length) {
    if (lexer_span_is(start, length, "string")) {
        return TOK_STRING;
    }
    if (lexer_span_is(start, length, "programa")) {
        return TOK_PROGRAMA;
    }
    if (lexer_span_is(start, length, "inicio")) {
        return TOK_INICIO;
    }
    if (lexer_span_is(start, length, "fim")) {
        return TOK_FIM;
    }
    if (lexer_span_is(start, length, "inteiro")) {
        return TOK_INTEIRO;
    }
    if (lexer_span_is(start, length, "flutuante")) {
        return TOK_FLUTUANTE;
    }
    if (lexer_span_is(start, length, "vazio")) {
        return TOK_VAZIO;
    }
    if (lexer_span_is(start, length, "procedimento")) {
        return TOK_PROCEDIMENTO;
    }
    if (lexer_span_is(start, length, "retorna")) {
        return TOK_RETORNA;
    }
    if (lexer_span_is(start, length, "escreva")) {
        return TOK_ESCREVA;
    }
    if (lexer_span_is(start, length, "escreval")) {
        return TOK_ESCREVAL;
    }
    if (lexer_span_is(start, length, "leia")) {
        return TOK_LEIA;
    }
    if (lexer_span_is(start, length, "div")) {
        return TOK_DIV;
    }
    if (lexer_span_is(start, length, "se")) {
        return TOK_SE;
    }
    if (lexer_span_is(start, length, "entao")) {
        return TOK_ENTAO;
    }
    if (lexer_span_is(start, length, "senao")) {
        return TOK_SENAO;
    }
    if (lexer_span_is(start, length, "fimse")) {
        return TOK_FIMSE;
    }
    if (lexer_span_is(start, length, "enquanto")) {
        return TOK_ENQUANTO;
    }
    if (lexer_span_is(start, length, "fimenquanto")) {
        return TOK_FIMENQUANTO;
    }
    if (lexer_span_is(start, length, "para")) {
        return TOK_PARA;
    }
    if (lexer_span_is(start, length, "de")) {
        return TOK_DE;
    }
    if (lexer_span_is(start, length, "ate")) {
        return TOK_ATE;
    }
    if (lexer_span_is(start, length, "passo")) {
        return TOK_PASSO;
    }
    if (lexer_span_is(start, length, "faca")) {
        return TOK_FACA;
    }
    if (lexer_span_is(start, length, "fimpara")) {
        return TOK_FIMPARA;
    }
    if (lexer_span_is(start, length, "e")) {
        return TOK_E;
    }
    if (lexer_span_is(start, length, "ou")) {
        return TOK_OU;
    }
    if (lexer_span_is(start, length, "nao")) {
        return TOK_NAO;
    }

    return TOK_ID;
}

static bool lexer_push_span(TokenList *tokens, TokenType type, const char *start, size_t length, int line, int column) {
    Token token;
    char lexeme[256];

    if (length >= sizeof(lexeme)) {
        return false;
    }

    memcpy(lexeme, start, length);
    lexeme[length] = '\0';

    token.type = type;
    token.lexeme = lexeme;
    token.line = line;
    token.column = column;
    return token_list_push(tokens, token);
}

static void lexer_fail(TokenList *tokens, CompilerError *error, int line, int column, const char *message) {
    token_list_free(tokens);
    compiler_error_set(error, COMPILER_PHASE_LEXER, line, column, message);
}

bool lexer_scan(const char *source, TokenList *out_tokens, CompilerError *error) {
    LexerState state;

    if (out_tokens == NULL) {
        compiler_error_set(error, COMPILER_PHASE_LEXER, 1, 1, "Fonte invalida.");
        return false;
    }

    /*
     * Public contract: out_tokens is zero-initialized or was initialized by
     * the token utilities, so token_list_free is the supported reset path here.
     */
    token_list_free(out_tokens);

    if (source == NULL) {
        compiler_error_set(error, COMPILER_PHASE_LEXER, 1, 1, "Fonte invalida.");
        return false;
    }

    state.source = source;
    state.index = 0;
    state.line = 1;
    state.column = 1;

    while (lexer_peek(&state) != '\0') {
        char current = lexer_peek(&state);
        int token_line = state.line;
        int token_column = state.column;

        if (current == ' ' || current == '\t' || current == '\r' || current == '\n') {
            lexer_advance(&state);
            continue;
        }

        if (lexer_is_identifier_start(current)) {
            const char *start = state.source + state.index;
            size_t length = 0;

            while (lexer_is_identifier_part(lexer_peek(&state))) {
                lexer_advance(&state);
                length++;
            }

            if (!lexer_push_span(out_tokens, lexer_keyword_type(start, length), start, length, token_line, token_column)) {
                lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                return false;
            }
            continue;
        }

        if (isdigit((unsigned char)current)) {
            const char *start = state.source + state.index;
            size_t length = 0;
            TokenType type = TOK_NUM_INT;

            while (isdigit((unsigned char)lexer_peek(&state))) {
                lexer_advance(&state);
                length++;
            }

            if (lexer_peek(&state) == '.' && isdigit((unsigned char)state.source[state.index + 1])) {
                type = TOK_NUM_FLOAT;
                lexer_advance(&state);
                length++;

                while (isdigit((unsigned char)lexer_peek(&state))) {
                    lexer_advance(&state);
                    length++;
                }
            }

            if (!lexer_push_span(out_tokens, type, start, length, token_line, token_column)) {
                lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                return false;
            }
            continue;
        }

        if (current == '"') {
            const char *lit_start = state.source + state.index;
            size_t length = 0;
            lexer_advance(&state);
            length++;
            while (lexer_peek(&state) != '"' && lexer_peek(&state) != '\0' && lexer_peek(&state) != '\n') {
                lexer_advance(&state);
                length++;
            }
            if (lexer_peek(&state) != '"') {
                lexer_fail(out_tokens, error, token_line, token_column, "Literal de string nao terminado.");
                return false;
            }
            lexer_advance(&state);
            length++;
            if (!lexer_push_span(out_tokens, TOK_STRING_LITERAL, lit_start, length, token_line, token_column)) {
                lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                return false;
            }
            continue;
        }

        if (current == '<' && state.source[state.index + 1] == '-') {
            if (!lexer_push_span(out_tokens, TOK_ATRIB, state.source + state.index, 2, token_line, token_column)) {
                lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                return false;
            }
            lexer_advance(&state);
            lexer_advance(&state);
            continue;
        }

        switch (current) {
            case '>':
                if (state.source[state.index + 1] == '=') {
                    if (!lexer_push_span(out_tokens, TOK_MAIOR_IGUAL, state.source + state.index, 2, token_line, token_column)) {
                        lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                        return false;
                    }
                    lexer_advance(&state);
                    lexer_advance(&state);
                } else {
                    if (!lexer_push_span(out_tokens, TOK_MAIOR, state.source + state.index, 1, token_line, token_column)) {
                        lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                        return false;
                    }
                    lexer_advance(&state);
                }
                break;
            case '<':
                if (state.source[state.index + 1] == '>') {
                    if (!lexer_push_span(out_tokens, TOK_DIFERENTE, state.source + state.index, 2, token_line, token_column)) {
                        lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                        return false;
                    }
                    lexer_advance(&state);
                    lexer_advance(&state);
                } else if (state.source[state.index + 1] == '=') {
                    if (!lexer_push_span(out_tokens, TOK_MENOR_IGUAL, state.source + state.index, 2, token_line, token_column)) {
                        lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                        return false;
                    }
                    lexer_advance(&state);
                    lexer_advance(&state);
                } else {
                    if (!lexer_push_span(out_tokens, TOK_MENOR, state.source + state.index, 1, token_line, token_column)) {
                        lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                        return false;
                    }
                    lexer_advance(&state);
                }
                break;
            case '=':
                if (!lexer_push_span(out_tokens, TOK_IGUAL, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case '+':
                if (!lexer_push_span(out_tokens, TOK_MAIS, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case '-':
                if (!lexer_push_span(out_tokens, TOK_MENOS, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case '*':
                if (!lexer_push_span(out_tokens, TOK_MULT, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case '(':
                if (!lexer_push_span(out_tokens, TOK_ABRE_PAR, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case ')':
                if (!lexer_push_span(out_tokens, TOK_FECHA_PAR, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case ',':
                if (!lexer_push_span(out_tokens, TOK_VIRGULA, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case '[':
                if (!lexer_push_span(out_tokens, TOK_ABRE_COL, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case ']':
                if (!lexer_push_span(out_tokens, TOK_FECHA_COL, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            case ';':
                if (!lexer_push_span(out_tokens, TOK_PONTO_VIRGULA, state.source + state.index, 1, token_line, token_column)) {
                    lexer_fail(out_tokens, error, token_line, token_column, "Falha ao registrar token.");
                    return false;
                }
                lexer_advance(&state);
                break;
            default:
                lexer_fail(out_tokens, error, token_line, token_column, "Caractere invalido.");
                return false;
        }
    }

    if (!lexer_push_span(out_tokens, TOK_EOF, state.source + state.index, 0, state.line, state.column)) {
        lexer_fail(out_tokens, error, state.line, state.column, "Falha ao registrar token.");
        return false;
    }

    return true;
}
