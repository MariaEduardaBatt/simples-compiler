#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "semantic.h"

char *codegen_generate_program(const ASTProgram *program, const SymbolTable *symbols);

#endif
