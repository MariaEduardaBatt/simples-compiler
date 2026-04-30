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
expected_usage="usage: $compiler <input.simples> [-o] <output.asm>"
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

"$compiler" examples/assign.simples -o "$e2e_dir/assign.asm"
grep -F "x dd 0" "$e2e_dir/assign.asm" >/dev/null
grep -F "mov dword [x], 10" "$e2e_dir/assign.asm" >/dev/null

"$compiler" examples/print.simples -o "$e2e_dir/print.asm"
grep -F "imul eax, ebx" "$e2e_dir/print.asm" >/dev/null
grep -F "sub eax, ebx" "$e2e_dir/print.asm" >/dev/null
grep -F "call print_newline" "$e2e_dir/print.asm" >/dev/null
nasm -f elf32 "$e2e_dir/print.asm" -o "$e2e_dir/print.o"
ld -m elf_i386 "$e2e_dir/print.o" -o "$e2e_dir/print"
output=$("$e2e_dir/print")
[ "$output" = "1515" ]

"$compiler" examples/if.simples -o "$e2e_dir/if.asm"
nasm -f elf32 "$e2e_dir/if.asm" -o "$e2e_dir/if.o"
ld -m elf_i386 "$e2e_dir/if.o" -o "$e2e_dir/if"
[ "$("$e2e_dir/if")" = "1" ]

"$compiler" examples/if_then.simples -o "$e2e_dir/if_then.asm"
nasm -f elf32 "$e2e_dir/if_then.asm" -o "$e2e_dir/if_then.o"
ld -m elf_i386 "$e2e_dir/if_then.o" -o "$e2e_dir/if_then"
[ "$("$e2e_dir/if_then")" = "7" ]

"$compiler" examples/if_then_else.simples -o "$e2e_dir/if_then_else.asm"
nasm -f elf32 "$e2e_dir/if_then_else.asm" -o "$e2e_dir/if_then_else.o"
ld -m elf_i386 "$e2e_dir/if_then_else.o" -o "$e2e_dir/if_then_else"
[ "$("$e2e_dir/if_then_else")" = "0" ]

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
"$compiler" "$e2e_dir/if_false.simples" -o "$e2e_dir/if_false.asm"
nasm -f elf32 "$e2e_dir/if_false.asm" -o "$e2e_dir/if_false.o"
ld -m elf_i386 "$e2e_dir/if_false.o" -o "$e2e_dir/if_false"
[ "$("$e2e_dir/if_false")" = "0" ]

"$compiler" examples/while.simples -o "$e2e_dir/while.asm"
nasm -f elf32 "$e2e_dir/while.asm" -o "$e2e_dir/while.o"
ld -m elf_i386 "$e2e_dir/while.o" -o "$e2e_dir/while"
[ "$("$e2e_dir/while")" = "123" ]

"$compiler" examples/for.simples -o "$e2e_dir/for.asm"
nasm -f elf32 "$e2e_dir/for.asm" -o "$e2e_dir/for.o"
ld -m elf_i386 "$e2e_dir/for.o" -o "$e2e_dir/for"
[ "$("$e2e_dir/for")" = "15" ]

"$compiler" examples/vector_sum.simples -o "$e2e_dir/vector_sum.asm"
nasm -f elf32 "$e2e_dir/vector_sum.asm" -o "$e2e_dir/vector_sum.o"
ld -m elf_i386 "$e2e_dir/vector_sum.o" -o "$e2e_dir/vector_sum"
[ "$("$e2e_dir/vector_sum")" = "10" ]

cat >"$e2e_dir/for_negative_step.simples" <<'EOF'
programa demo
inteiro i;
inicio
  para i de 3 ate 1 passo -1 faca
    escreva i;
  fimpara
fim
EOF
"$compiler" "$e2e_dir/for_negative_step.simples" -o "$e2e_dir/for_negative_step.asm"
nasm -f elf32 "$e2e_dir/for_negative_step.asm" -o "$e2e_dir/for_negative_step.o"
ld -m elf_i386 "$e2e_dir/for_negative_step.o" -o "$e2e_dir/for_negative_step"
[ "$("$e2e_dir/for_negative_step")" = "321" ]

cat >"$e2e_dir/for_zero_step.simples" <<'EOF'
programa demo
inteiro i;
inicio
  para i de 1 ate 3 passo 0 faca
    escreva i;
  fimpara
fim
EOF
"$compiler" "$e2e_dir/for_zero_step.simples" -o "$e2e_dir/for_zero_step.asm"
nasm -f elf32 "$e2e_dir/for_zero_step.asm" -o "$e2e_dir/for_zero_step.o"
ld -m elf_i386 "$e2e_dir/for_zero_step.o" -o "$e2e_dir/for_zero_step"
[ "$("$e2e_dir/for_zero_step")" = "" ]

"$compiler" examples/read.simples -o "$e2e_dir/read.asm"
nasm -f elf32 "$e2e_dir/read.asm" -o "$e2e_dir/read.o"
ld -m elf_i386 "$e2e_dir/read.o" -o "$e2e_dir/read"
output=$(printf '42\n' | "$e2e_dir/read")
[ "$output" = "42" ]

"$compiler" examples/string_echo.simples -o "$e2e_dir/string_echo.asm"
nasm -f elf32 "$e2e_dir/string_echo.asm" -o "$e2e_dir/string_echo.o"
ld -m elf_i386 "$e2e_dir/string_echo.o" -o "$e2e_dir/string_echo"
output=$(printf 'ana\n' | "$e2e_dir/string_echo")
[ "$output" = "ana" ]

cat >"$e2e_dir/string_index.simples" <<'EOF'
programa demo
string nome[8];
inicio
  nome[0] <- 65;
  nome[1] <- 66;
  nome[2] <- 0;
  escreval nome;
fim
EOF
"$compiler" "$e2e_dir/string_index.simples" -o "$e2e_dir/string_index.asm"
nasm -f elf32 "$e2e_dir/string_index.asm" -o "$e2e_dir/string_index.o"
ld -m elf_i386 "$e2e_dir/string_index.o" -o "$e2e_dir/string_index"
[ "$("$e2e_dir/string_index")" = "AB" ]

cat >"$e2e_dir/string_literal_write.simples" <<'EOF'
programa demo
inicio
  escreval "abc";
fim
EOF
"$compiler" "$e2e_dir/string_literal_write.simples" -o "$e2e_dir/string_literal_write.asm"
nasm -f elf32 "$e2e_dir/string_literal_write.asm" -o "$e2e_dir/string_literal_write.o"
ld -m elf_i386 "$e2e_dir/string_literal_write.o" -o "$e2e_dir/string_literal_write"
[ "$("$e2e_dir/string_literal_write")" = "abc" ]

output=$(printf '%s\n' '-17' | "$e2e_dir/read")
[ "$output" = "-17" ]

output=$(printf '%s\n' '42abc' | "$e2e_dir/read")
[ "$output" = "42" ]

output=$(printf '%s\n' '-5xyz' | "$e2e_dir/read")
[ "$output" = "-5" ]

# INT_MAX and INT_MIN are accepted exactly
output=$(printf '%s\n' '2147483647' | "$e2e_dir/read")
[ "$output" = "2147483647" ]

output=$(printf '%s\n' '-2147483648' | "$e2e_dir/read")
[ "$output" = "-2147483648" ]

# positive overflow clamps to INT_MAX
output=$(printf '%s\n' '2147483648' | "$e2e_dir/read")
[ "$output" = "2147483647" ]

output=$(printf '%s\n' '9999999999' | "$e2e_dir/read")
[ "$output" = "2147483647" ]

# negative overflow clamps to INT_MIN
output=$(printf '%s\n' '-2147483649' | "$e2e_dir/read")
[ "$output" = "-2147483648" ]

# extra digits after reaching the negative boundary must not resume accumulation
output=$(printf '%s\n' '-21474836480' | "$e2e_dir/read")
[ "$output" = "-2147483648" ]

output=$(printf '%s\n' '-21474836489' | "$e2e_dir/read")
[ "$output" = "-2147483648" ]

"$compiler" examples/procedure_sum.simples -o "$e2e_dir/procedure_sum.asm"
nasm -f elf32 "$e2e_dir/procedure_sum.asm" -o "$e2e_dir/procedure_sum.o"
ld -m elf_i386 "$e2e_dir/procedure_sum.o" -o "$e2e_dir/procedure_sum"
[ "$("$e2e_dir/procedure_sum")" = "5" ]

"$compiler" examples/procedure_void.simples -o "$e2e_dir/procedure_void.asm"
nasm -f elf32 "$e2e_dir/procedure_void.asm" -o "$e2e_dir/procedure_void.o"
ld -m elf_i386 "$e2e_dir/procedure_void.o" -o "$e2e_dir/procedure_void"
[ "$("$e2e_dir/procedure_void")" = "1" ]

"$compiler" examples/procedure_string_param.simples -o "$e2e_dir/procedure_string_param.asm"
nasm -f elf32 "$e2e_dir/procedure_string_param.asm" -o "$e2e_dir/procedure_string_param.o"
ld -m elf_i386 "$e2e_dir/procedure_string_param.o" -o "$e2e_dir/procedure_string_param"
output=$("$e2e_dir/procedure_string_param")
[ "$output" = "$(printf 'ana\nbia')" ]

"$compiler" examples/string_return.simples -o "$e2e_dir/string_return.asm"
nasm -f elf32 "$e2e_dir/string_return.asm" -o "$e2e_dir/string_return.o"
ld -m elf_i386 "$e2e_dir/string_return.o" -o "$e2e_dir/string_return"
[ "$("$e2e_dir/string_return")" = "ana" ]

"$compiler" examples/vector_value.simples -o "$e2e_dir/vector_value.asm"
nasm -f elf32 "$e2e_dir/vector_value.asm" -o "$e2e_dir/vector_value.o"
ld -m elf_i386 "$e2e_dir/vector_value.o" -o "$e2e_dir/vector_value"
[ "$("$e2e_dir/vector_value")" = "6" ]

"$compiler" examples/float_main.simples -o "$e2e_dir/float_main.asm"
nasm -f elf32 "$e2e_dir/float_main.asm" -o "$e2e_dir/float_main.o"
ld -m elf_i386 "$e2e_dir/float_main.o" -o "$e2e_dir/float_main"
[ "$("$e2e_dir/float_main")" = "2" ]

"$compiler" examples/float_procedure.simples -o "$e2e_dir/float_procedure.asm"
nasm -f elf32 "$e2e_dir/float_procedure.asm" -o "$e2e_dir/float_procedure.o"
ld -m elf_i386 "$e2e_dir/float_procedure.o" -o "$e2e_dir/float_procedure"
[ "$("$e2e_dir/float_procedure")" = "1" ]

"$compiler" examples/float_echo.simples -o "$e2e_dir/float_echo.asm"
nasm -f elf32 "$e2e_dir/float_echo.asm" -o "$e2e_dir/float_echo.o"
ld -m elf_i386 "$e2e_dir/float_echo.o" -o "$e2e_dir/float_echo"
output=$(printf '3.5\n' | "$e2e_dir/float_echo")
[ "$output" = "3.5
2.25
-1.25
5.25" ]

"$compiler" examples/float_div_cast.simples -o "$e2e_dir/float_div_cast.asm"
nasm -f elf32 "$e2e_dir/float_div_cast.asm" -o "$e2e_dir/float_div_cast.o"
ld -m elf_i386 "$e2e_dir/float_div_cast.o" -o "$e2e_dir/float_div_cast"
[ "$("$e2e_dir/float_div_cast")" = "3.75
3.5
3" ]

"$compiler" examples/vector_float_multiply.simples -o "$e2e_dir/vector_float_multiply.asm"
nasm -f elf32 "$e2e_dir/vector_float_multiply.asm" -o "$e2e_dir/vector_float_multiply.o"
ld -m elf_i386 "$e2e_dir/vector_float_multiply.o" -o "$e2e_dir/vector_float_multiply"
[ "$("$e2e_dir/vector_float_multiply")" = "3.0 3.0 3.0 2.0 10.0 6.0 3.5 16.0 13.5 10.0" ]

"$compiler" examples/fibonacci.simples -o "$e2e_dir/fibonacci.asm"
nasm -f elf32 "$e2e_dir/fibonacci.asm" -o "$e2e_dir/fibonacci.o"
ld -m elf_i386 "$e2e_dir/fibonacci.o" -o "$e2e_dir/fibonacci"
output=$(printf '10\n' | "$e2e_dir/fibonacci")
expected=$(printf 'digite o numero: fibonacci ate 10:\n0 1 1 2 3 5 8 13 21 34 55')
[ "$output" = "$expected" ]

output=$(printf '47\n' | "$e2e_dir/fibonacci")
[ "$output" = "digite o numero: numero muito grande" ]

cat >"$e2e_dir/procedure_recursive_for.simples" <<'EOF'
procedimento inteiro fatorial_para(inteiro n)
inicio
  inteiro i, total;
  total <- 0;
  para i de n ate 1 passo -1 faca
    se n > 1 entao
      total <- total + fatorial_para(n - 1);
    senao
      total <- total + 1;
    fimse
  fimpara
  retorna total;
fim
programa demo
inteiro x;
inicio
  x <- fatorial_para(3);
  escreval x;
fim
EOF
"$compiler" "$e2e_dir/procedure_recursive_for.simples" -o "$e2e_dir/procedure_recursive_for.asm"
nasm -f elf32 "$e2e_dir/procedure_recursive_for.asm" -o "$e2e_dir/procedure_recursive_for.o"
ld -m elf_i386 "$e2e_dir/procedure_recursive_for.o" -o "$e2e_dir/procedure_recursive_for"
[ "$("$e2e_dir/procedure_recursive_for")" = "6" ]

printf 'e2e ok\n'
