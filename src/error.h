#ifndef ERROR_H
#define ERROR_H

typedef enum {
    COMPILER_PHASE_LEXER,
    COMPILER_PHASE_PARSER,
    COMPILER_PHASE_SEMANTIC,
    COMPILER_PHASE_CODEGEN
} CompilerPhase;

typedef struct {
    CompilerPhase phase;
    int line;
    int column;
    char message[256];
} CompilerError;

void compiler_error_set(CompilerError *error, CompilerPhase phase, int line, int column, const char *message);

#endif
