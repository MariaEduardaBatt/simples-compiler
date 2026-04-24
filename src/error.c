#include "error.h"

#include <stdio.h>

void compiler_error_set(CompilerError *error, CompilerPhase phase, int line, int column, const char *message) {
    if (error == NULL) {
        return;
    }

    error->phase = phase;
    error->line = line;
    error->column = column;
    snprintf(error->message, sizeof(error->message), "%s", message != NULL ? message : "");
}
