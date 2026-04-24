#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

static char *read_entire_file(const char *path) {
    FILE *file;
    long length;
    size_t bytes_read;
    char *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    length = ftell(file);
    if (length < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        fclose(file);
        return NULL;
    }

    buffer = malloc((size_t)length + 1);
    if (buffer == NULL) {
        fprintf(stderr, "out of memory\n");
        fclose(file);
        return NULL;
    }

    bytes_read = fread(buffer, 1, (size_t)length, file);
    if (bytes_read != (size_t)length) {
        fprintf(stderr, "%s: %s\n", path, ferror(file) ? strerror(errno) : "unexpected end of file");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[length] = '\0';

    if (fclose(file) != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        free(buffer);
        return NULL;
    }

    return buffer;
}

static int write_text_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    size_t length;

    if (file == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }

    length = strlen(text);
    if (fwrite(text, 1, length, file) != length) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }

    return 0;
}

static const char *phase_name(CompilerPhase phase) {
    switch (phase) {
        case COMPILER_PHASE_LEXER:
            return "lexer";
        case COMPILER_PHASE_PARSER:
            return "parser";
        case COMPILER_PHASE_SEMANTIC:
            return "semantic";
        default:
            return "compiler";
    }
}

static int report_error(const CompilerError *error) {
    fprintf(stderr, "%s:%d:%d: %s\n", phase_name(error->phase), error->line, error->column, error->message);
    return 1;
}

int compile_file(const char *input_path, const char *output_path) {
    char *source = NULL;
    char *assembly = NULL;
    TokenList tokens = {0};
    ASTProgram *program = NULL;
    SymbolTable symbols = {0};
    CompilerError error = {0};
    int exit_code = 1;

    source = read_entire_file(input_path);
    if (source == NULL) {
        goto cleanup;
    }

    token_list_init(&tokens);
    if (!lexer_scan(source, &tokens, &error)) {
        exit_code = report_error(&error);
        goto cleanup;
    }

    if (!parse_program(&tokens, &program, &error)) {
        exit_code = report_error(&error);
        goto cleanup;
    }

    if (!analyze_program(program, &symbols, &error)) {
        exit_code = report_error(&error);
        goto cleanup;
    }

    assembly = codegen_generate_program(program, &symbols);
    if (assembly == NULL) {
        fprintf(stderr, "codegen:1:1: Falha na geracao de codigo.\n");
        goto cleanup;
    }

    if (write_text_file(output_path, assembly) != 0) {
        goto cleanup;
    }

    exit_code = 0;

cleanup:
    free(assembly);
    symbol_table_free(&symbols);
    ast_program_free(program);
    token_list_free(&tokens);
    free(source);
    return exit_code;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.simples> <output.asm>\n", argv[0]);
        return 1;
    }

    return compile_file(argv[1], argv[2]);
}
