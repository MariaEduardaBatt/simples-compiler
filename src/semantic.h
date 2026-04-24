#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "error.h"

typedef struct {
    char **names;
    size_t count;
} SymbolTable;

bool analyze_program(const ASTProgram *program, SymbolTable *out_symbols, CompilerError *error);
void symbol_table_free(SymbolTable *symbols);

#endif
