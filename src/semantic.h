#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "error.h"

typedef struct {
    char *name;
    ASTType type;
    ASTStorageKind storage;
    size_t capacity;
} SymbolInfo;

typedef struct {
    ASTType type;
    ASTStorageKind storage;
    size_t capacity;
    ASTPassMode pass_mode;
} ParameterInfo;

typedef struct {
    char *name;
    ASTType return_type;
    size_t return_capacity;
    ParameterInfo *parameters;
    size_t parameter_count;
} ProcedureSignature;

typedef struct {
    SymbolInfo *globals;
    size_t global_count;
    ProcedureSignature *procedures;
    size_t procedure_count;
} SemanticInfo;

bool analyze_program(const ASTProgram *program, SemanticInfo *out_info, CompilerError *error);
void semantic_info_free(SemanticInfo *info);

#endif
