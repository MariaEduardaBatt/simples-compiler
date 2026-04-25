#include "token.h"

#include <stdlib.h>
#include <string.h>

static char *token_clone_lexeme(const char *lexeme) {
    size_t length;
    char *copy;

    if (lexeme == NULL) {
        return NULL;
    }

    length = strlen(lexeme);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, lexeme, length + 1);
    return copy;
}

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_PROGRAMA:
            return "TOK_PROGRAMA";
        case TOK_INICIO:
            return "TOK_INICIO";
        case TOK_FIM:
            return "TOK_FIM";
        case TOK_INTEIRO:
            return "TOK_INTEIRO";
        case TOK_ESCREVA:
            return "TOK_ESCREVA";
        case TOK_ESCREVAL:
            return "TOK_ESCREVAL";
        case TOK_DIV:
            return "TOK_DIV";
        case TOK_SE:
            return "TOK_SE";
        case TOK_ENTAO:
            return "TOK_ENTAO";
        case TOK_SENAO:
            return "TOK_SENAO";
        case TOK_FIMSE:
            return "TOK_FIMSE";
        case TOK_ENQUANTO:
            return "TOK_ENQUANTO";
        case TOK_FIMENQUANTO:
            return "TOK_FIMENQUANTO";
        case TOK_PARA:
            return "TOK_PARA";
        case TOK_DE:
            return "TOK_DE";
        case TOK_ATE:
            return "TOK_ATE";
        case TOK_PASSO:
            return "TOK_PASSO";
        case TOK_FACA:
            return "TOK_FACA";
        case TOK_FIMPARA:
            return "TOK_FIMPARA";
        case TOK_E:
            return "TOK_E";
        case TOK_OU:
            return "TOK_OU";
        case TOK_NAO:
            return "TOK_NAO";
        case TOK_ID:
            return "TOK_ID";
        case TOK_NUM_INT:
            return "TOK_NUM_INT";
        case TOK_ATRIB:
            return "TOK_ATRIB";
        case TOK_MAIOR:
            return "TOK_MAIOR";
        case TOK_MENOR:
            return "TOK_MENOR";
        case TOK_IGUAL:
            return "TOK_IGUAL";
        case TOK_DIFERENTE:
            return "TOK_DIFERENTE";
        case TOK_MAIOR_IGUAL:
            return "TOK_MAIOR_IGUAL";
        case TOK_MENOR_IGUAL:
            return "TOK_MENOR_IGUAL";
        case TOK_MAIS:
            return "TOK_MAIS";
        case TOK_MENOS:
            return "TOK_MENOS";
        case TOK_MULT:
            return "TOK_MULT";
        case TOK_ABRE_PAR:
            return "TOK_ABRE_PAR";
        case TOK_FECHA_PAR:
            return "TOK_FECHA_PAR";
        case TOK_VIRGULA:
            return "TOK_VIRGULA";
        case TOK_PONTO_VIRGULA:
            return "TOK_PONTO_VIRGULA";
        case TOK_EOF:
            return "TOK_EOF";
        default:
            return "TOK_UNKNOWN";
    }
}

void token_list_init(TokenList *list) {
    if (list == NULL) {
        return;
    }

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

bool token_list_push(TokenList *list, Token token) {
    const char *original_lexeme;
    char *lexeme_copy;
    Token *new_items;
    size_t new_capacity;

    if (list == NULL) {
        return false;
    }

    if (list->count == list->capacity) {
        new_capacity = list->capacity == 0 ? 4 : list->capacity * 2;
        new_items = realloc(list->items, new_capacity * sizeof(*list->items));
        if (new_items == NULL) {
            return false;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    original_lexeme = token.lexeme;
    lexeme_copy = token_clone_lexeme(original_lexeme);
    if (original_lexeme != NULL && lexeme_copy == NULL) {
        return false;
    }

    token.lexeme = lexeme_copy;
    list->items[list->count++] = token;
    return true;
}

void token_list_free(TokenList *list) {
    size_t index;

    if (list == NULL) {
        return;
    }

    for (index = 0; index < list->count; ++index) {
        free(list->items[index].lexeme);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}
