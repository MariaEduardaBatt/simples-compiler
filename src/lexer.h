#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

#include "error.h"
#include "token.h"

/*
 * Scans source into out_tokens.
 *
 * out_tokens must point to a TokenList that is either zero-initialized
 * (for example, TokenList list = {0};) or already initialized/returned by
 * the token utilities. lexer_scan resets any previous token storage in a
 * valid list before writing the new scan result, so repeated calls are
 * supported for callers that keep using the same initialized TokenList.
 */
bool lexer_scan(const char *source, TokenList *out_tokens, CompilerError *error);

#endif
