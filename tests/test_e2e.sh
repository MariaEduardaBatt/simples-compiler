#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
compiler=build/simplesc
e2e_dir=build/e2e

mkdir -p "$e2e_dir"

assert_compile_error() {
    input_path=$1
    output_path=$2
    expected=$3
    stdout_path=$4
    stderr_path=$5

    if "$compiler" "$input_path" "$output_path" >"$stdout_path" 2>"$stderr_path"; then
        printf 'expected %s to fail for %s\n' "$compiler" "$input_path" >&2
        exit 1
    fi

    [ ! -s "$stdout_path" ]
    actual=$(cat "$stderr_path")
    [ "$actual" = "$expected" ]
}

usage_stdout="$e2e_dir/noargs.stdout"
usage_stderr="$e2e_dir/noargs.stderr"
if "$compiler" >"$usage_stdout" 2>"$usage_stderr"; then
    printf 'expected %s without arguments to fail\n' "$compiler" >&2
    exit 1
else
    status=$?
fi
[ "$status" -eq 1 ]
expected_usage="usage: $compiler <input.simples> <output.asm>"
grep -Fx "$expected_usage" "$usage_stderr" >/dev/null

cat >"$e2e_dir/lexer_error.simples" <<'EOF'
programa demo
inicio
@
fim
EOF
assert_compile_error \
    "$e2e_dir/lexer_error.simples" \
    "$e2e_dir/lexer_error.asm" \
    "lexer:3:1: Caractere invalido." \
    "$e2e_dir/lexer_error.stdout" \
    "$e2e_dir/lexer_error.stderr"

cat >"$e2e_dir/parser_error.simples" <<'EOF'
programa demo
inicio
escreva 1
fim
EOF
assert_compile_error \
    "$e2e_dir/parser_error.simples" \
    "$e2e_dir/parser_error.asm" \
    "parser:4:1: Esperado ';' apos comando." \
    "$e2e_dir/parser_error.stdout" \
    "$e2e_dir/parser_error.stderr"

cat >"$e2e_dir/semantic_error.simples" <<'EOF'
programa demo
inicio
x <- 1;
fim
EOF
assert_compile_error \
    "$e2e_dir/semantic_error.simples" \
    "$e2e_dir/semantic_error.asm" \
    "semantic:3:1: Identificador 'x' nao declarado." \
    "$e2e_dir/semantic_error.stdout" \
    "$e2e_dir/semantic_error.stderr"

"$compiler" examples/assign.simples "$e2e_dir/assign.asm"
grep -F "x dd 0" "$e2e_dir/assign.asm" >/dev/null
grep -F "mov dword [x], 10" "$e2e_dir/assign.asm" >/dev/null

"$compiler" examples/print.simples "$e2e_dir/print.asm"
grep -F "imul eax, ebx" "$e2e_dir/print.asm" >/dev/null
grep -F "sub eax, ebx" "$e2e_dir/print.asm" >/dev/null
grep -F "call print_newline" "$e2e_dir/print.asm" >/dev/null
nasm -f elf32 "$e2e_dir/print.asm" -o "$e2e_dir/print.o"
ld -m elf_i386 "$e2e_dir/print.o" -o "$e2e_dir/print"
output=$("$e2e_dir/print")
[ "$output" = "1515" ]

"$compiler" examples/if.simples "$e2e_dir/if.asm"
nasm -f elf32 "$e2e_dir/if.asm" -o "$e2e_dir/if.o"
ld -m elf_i386 "$e2e_dir/if.o" -o "$e2e_dir/if"
[ "$("$e2e_dir/if")" = "1" ]

cat >"$e2e_dir/if_false.simples" <<'EOF'
programa demo
inteiro x;
inicio
  x <- -1;
  se x > 0 entao
    escreval x;
  senao
    escreval 0;
  fimse
fim
EOF
"$compiler" "$e2e_dir/if_false.simples" "$e2e_dir/if_false.asm"
nasm -f elf32 "$e2e_dir/if_false.asm" -o "$e2e_dir/if_false.o"
ld -m elf_i386 "$e2e_dir/if_false.o" -o "$e2e_dir/if_false"
[ "$("$e2e_dir/if_false")" = "0" ]

"$compiler" examples/while.simples "$e2e_dir/while.asm"
nasm -f elf32 "$e2e_dir/while.asm" -o "$e2e_dir/while.o"
ld -m elf_i386 "$e2e_dir/while.o" -o "$e2e_dir/while"
[ "$("$e2e_dir/while")" = "123" ]

"$compiler" examples/for.simples "$e2e_dir/for.asm"
nasm -f elf32 "$e2e_dir/for.asm" -o "$e2e_dir/for.o"
ld -m elf_i386 "$e2e_dir/for.o" -o "$e2e_dir/for"
[ "$("$e2e_dir/for")" = "15" ]

printf 'e2e ok\n'
