#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TOK_PROGRAMA,
    TOK_INICIO,
    TOK_FIM,
    TOK_INTEIRO,
    TOK_ESCREVA,
    TOK_ESCREVAL,
    TOK_DIV,
    TOK_ID,
    TOK_NUM_INT,
    TOK_ATRIB,
    TOK_MAIS,
    TOK_MENOS,
    TOK_MULT,
    TOK_ABRE_PAR,
    TOK_FECHA_PAR,
    TOK_VIRGULA,
    TOK_PONTO_VIRGULA,
    TOK_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

const char *token_type_name(TokenType type);
/* Initializes a TokenList for use with token_list_push, token_list_free, or lexer_scan. */
void token_list_init(TokenList *list);
bool token_list_push(TokenList *list, Token token);
/*
 * Releases storage owned by the list and leaves it empty.
 * The list must have been zero-initialized or previously initialized by the token utilities.
 */
void token_list_free(TokenList *list);

#endif
