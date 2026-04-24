#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "ast.h"
#include "error.h"
#include "token.h"

bool parse_program(const TokenList *tokens, ASTProgram **out_program, CompilerError *error);

#endif
