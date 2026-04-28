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

typedef struct {
    char *name;
    ASTType return_type;
    ASTType *parameter_types;
    size_t parameter_count;
} ProcedureSignature;

typedef struct {
    SymbolTable globals;
    ProcedureSignature *procedures;
    size_t procedure_count;
} SemanticInfo;

bool analyze_program(const ASTProgram *program, SemanticInfo *out_info, CompilerError *error);
void symbol_table_free(SymbolTable *symbols);
void semantic_info_free(SemanticInfo *info);

#endif
