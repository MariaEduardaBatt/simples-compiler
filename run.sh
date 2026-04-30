#!/bin/sh
set -eu

compiler="./build/simplesc"

if [ ! -x "$compiler" ]; then
    printf 'erro: %s nao encontrado. Compile o projeto com make primeiro.\n' "$compiler" >&2
    exit 1
fi

printf 'Arquivo de entrada .simples: '
IFS= read -r input_path

printf 'Arquivo de saida binario: '
IFS= read -r binary_path

if [ -z "$input_path" ] || [ -z "$binary_path" ]; then
    printf 'erro: entrada e saida sao obrigatorias.\n' >&2
    exit 1
fi

if [ ! -f "$input_path" ]; then
    printf 'erro: arquivo de entrada nao encontrado: %s\n' "$input_path" >&2
    exit 1
fi

asm_path="${binary_path}.asm"
obj_path="${binary_path}.o"

"$compiler" "$input_path" -o "$asm_path"
nasm -f elf32 "$asm_path" -o "$obj_path"
ld -m elf_i386 "$obj_path" -o "$binary_path"

printf 'binario gerado em %s\n' "$binary_path"
