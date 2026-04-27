#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>

#include "ast.h"
#include "error.h"
#include "semantic.h"

bool codegen_generate_program(const ASTProgram *program, const SemanticInfo *semantic, char **out_assembly, CompilerError *error);

#endif
