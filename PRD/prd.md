# PRD — Compilador da Linguagem SIMPLES para NASM x32

> **PRD — Product Requirements Document** | Disciplina de Compiladores | IFSULDEMINAS Campus Poços de Caldas
> 

---

## Visão Geral

Este documento especifica os requisitos para implementação de um **compilador da linguagem SIMPLES** que gera código Assembly NASM 32-bit para Linux. O compilador será desenvolvido em **C (C99)**, seguindo a arquitetura clássica de front-end + back-end, com TDD obrigatório em todas as fases.

> 💡 **Analogia:** O compilador funciona como um tradutor jurãmentado: recebe um texto em português estruturado (linguagem SIMPLES), entende cada construção com precisão cirúrgica, e produz uma tradução fiel em japonês técnico (NASM x32). Não há espaço para ambiguidades.
> 

---

## Objetivo

Construir um compilador funcional que:

1. Lê um arquivo-fonte escrito na **linguagem SIMPLES**
2. Executa as fases de análise léxica, sintática e semântica
3. Gera um arquivo `.asm` válido para o **NASM 32-bit**
4. O arquivo gerado pode ser montado e executado no Linux:

```bash
nasm -f elf32 programa.asm -o programa.o
ld -m elf_i386 programa.o -o programa
./programa
```

---

## Stack Técnica

| Componente | Tecnologia |
| --- | --- |
| Linguagem do compilador | C (C99) |
| Compilador do compilador | GCC |
| Testes | Unity Test Framework |
| Build | Makefile |
| Alvo | NASM x86 32-bit (ELF Linux) |
| Syscalls | Linux 32-bit (`int 0x80`) |
| Convenção de registradores | `eax`, `ebx`, `ecx`, `edx` |

---

## A Linguagem SIMPLES — Gramática e Tokens

### Tokens reconhecidos (49 tokens)

**Palavras-chave:**

| Token | Lexema | Categoria |
| --- | --- | --- |
| `TOK_PROGRAMA` | `programa` | Palavra-chave |
| `TOK_INICIO` | `inicio` | Palavra-chave |
| `TOK_FIM` | `fim` | Palavra-chave |
| `TOK_INTEIRO` | `inteiro` | Tipo |
| `TOK_FLUTUANTE` | `flutuante` | Tipo |
| `TOK_STRING` | `string` | Tipo |
| `TOK_VAZIO` | `vazio` | Tipo |
| `TOK_SE` | `se` | Controle |
| `TOK_ENTAO` | `entao` | Controle |
| `TOK_SENAO` | `senao` | Controle |
| `TOK_FIMSE` | `fimse` | Controle |
| `TOK_ENQUANTO` | `enquanto` | Laço |
| `TOK_FIMENQUANTO` | `fimenquanto` | Laço |
| `TOK_PARA` | `para` | Laço |
| `TOK_DE` | `de` | Laço |
| `TOK_ATE` | `ate` | Laço |
| `TOK_PASSO` | `passo` | Laço |
| `TOK_FACA` | `faca` | Laço |
| `TOK_FIMPARA` | `fimpara` | Laço |
| `TOK_LEIA` | `leia` | E/S |
| `TOK_ESCREVA` | `escreva` | E/S |
| `TOK_ESCREVAL` | `escreval` | E/S (com newline) |
| `TOK_E` | `e` | Lógico |
| `TOK_OU` | `ou` | Lógico |
| `TOK_NAO` | `nao` | Lógico |
| `TOK_DIV` | `div` | Operador (divisão inteira) |
| `TOK_PROCEDIMENTO` | `procedimento` | Subprograma |
| `TOK_RETORNA` | `retorna` | Subprograma |

**Literais e identificadores:**

| Token | Padrão | Exemplo |
| --- | --- | --- |
| `TOK_ID` | `[A-Za-z_][A-Za-z_0-9]*` | `x`, `soma`, `_val` |
| `TOK_NUM_INT` | `[0-9]+` | `42`, `0`, `100` |
| `TOK_NUM_FLOAT` | `[0-9]+\.[0-9]+` | `3.14`, `0.5` |
| `TOK_STRING_LITERAL` | `"..."` | `"ana"`, `"oi"` |

**Operadores:**

| Token | Lexema | Categoria |
| --- | --- | --- |
| `TOK_ATRIB` | `<-` | Atribuição |
| `TOK_MAIS` | `+` | Aritmético |
| `TOK_MENOS` | `-` | Aritmético |
| `TOK_MULT` | `*` | Aritmético |
| `TOK_MAIOR` | `>` | Relacional |
| `TOK_MENOR` | `<` | Relacional |
| `TOK_IGUAL` | `=` | Relacional |
| `TOK_DIFERENTE` | `<>` | Relacional |
| `TOK_MAIOR_IGUAL` | `>=` | Relacional |
| `TOK_MENOR_IGUAL` | `<=` | Relacional |

**Delimitadores:**

| Token | Lexema | Uso |
| --- | --- | --- |
| `TOK_ABRE_PAR` | `(` | Agrupamento |
| `TOK_FECHA_PAR` | `)` | Agrupamento |
| `TOK_ABRE_COL` | `[` | Indexação |
| `TOK_FECHA_COL` | `]` | Indexação |
| `TOK_VIRGULA` | `,` | `inteiro a, b, c` |
| `TOK_PONTO_VIRGULA` | `;` | Separador |
| `TOK_EOF` | fim de arquivo | Especial |

> ⚠️ **Atenção:** `TOK_INTEIRO` e `TOK_FLUTUANTE` são palavras-chave de **declaração de tipo** — não confundir com `TOK_NUM_INT` e `TOK_NUM_FLOAT` que são **literais numéricos**. O operador de divisão inteira é `div` (`TOK_DIV`), não `/`. A atribuição usa `<-` (`TOK_ATRIB`), não `:=`.
> 

### Gramática EBNF completa

```
<arquivo>         ::= { <procedimento> } <programa>

<programa>        ::= "programa" ID <declaracoes_globais> "inicio" <comandos> "fim"

<procedimento>    ::= "procedimento" <tipo_retorno> ID "(" [ <parametros> ] ")"
                      "inicio" <declaracoes_locais> <comandos> "fim"
<parametros>      ::= <parametro> { "," <parametro> }
<parametro>       ::= <tipo> ID [ "[" NUM_INT "]" ] [ "valor" ]
<tipo_retorno>    ::= "inteiro" | "flutuante" | "vazio" | "string" "[" NUM_INT "]"

<declaracoes_globais> ::= { <declaracao> }
<declaracoes_locais>  ::= { <declaracao> }
<declaracao>      ::= <tipo> <item_declaracao> { "," <item_declaracao> } ";"
<item_declaracao> ::= ID [ "[" NUM_INT "]" ]
<tipo>            ::= "inteiro" | "flutuante" | "string"

<comandos>        ::= { <comando> }
<comando>         ::= <atribuicao>
                    | <cmd_leia>
                    | <cmd_escreva>
                    | <cmd_se>
                    | <cmd_enquanto>
                    | <cmd_para>
                    | <cmd_chamada>
                    | <cmd_retorna>

<atribuivel>      ::= ID | ID "[" <expressao> "]"
<atribuicao>      ::= <atribuivel> "<-" <expressao> ";"
<cmd_leia>        ::= "leia" ID ";"
<cmd_escreva>     ::= ("escreva" | "escreval") <expressao> ";"
<cmd_chamada>     ::= ID "(" [ <argumentos> ] ")" ";"
<cmd_retorna>     ::= "retorna" [ <expressao> ] ";"
<argumentos>      ::= <expressao> { "," <expressao> }

<cmd_se>          ::= "se" <expressao> "entao" <comandos>
                      [ "senao" <comandos> ] "fimse"

<cmd_enquanto>    ::= "enquanto" <expressao> "faca" <comandos> "fimenquanto"

<cmd_para>        ::= "para" ID "de" <expressao> "ate" <expressao>
                      "passo" <expressao> "faca" <comandos> "fimpara"

<expressao>       ::= <expr_logica>
<expr_logica>     ::= <expr_relacional> { ("e" | "ou") <expr_relacional> }
<expr_relacional> ::= <expr_aditiva> [ <op_relacional> <expr_aditiva> ]
<op_relacional>   ::= ">" | "<" | "=" | "<>" | ">=" | "<="
<expr_aditiva>    ::= <expr_mult> { ("+" | "-") <expr_mult> }
<expr_mult>       ::= <fator> { ("*" | "div") <fator> }
<fator>           ::= ID
                    | ID "[" <expressao> "]"
                    | NUM_INT
                    | NUM_FLOAT
                    | STRING_LITERAL
                    | "inteiro" "(" <expressao> ")"
                    | "flutuante" "(" <expressao> ")"
                    | ID "(" [ <argumentos> ] ")"
                    | "(" <expressao> ")"
                    | "nao" <fator>
                    | "-" <fator>
```

---

## Arquitetura do Compilador

```
arquivo.simples
      ↓
[1. Lexer]          → lista de tokens
      ↓
[2. Parser]         → AST (Abstract Syntax Tree)
      ↓
[3. SemanticAnalyzer] → AST anotada + tabela de símbolos
      ↓
[4. CodeGenerator]  → arquivo .asm (NASM 32-bit)
```

### Estrutura de diretórios do projeto

```jsx
simples_compiler/
├── src/
│   ├── lexer.c           # Análise léxica
│   ├── lexer.h
│   ├── parser.c          # Análise sintática + AST
│   ├── parser.h
│   ├── semantic.c        # Análise semântica
│   ├── semantic.h
│   ├── codegen.c         # Geração de código NASM
│   ├── codegen.h
│   ├── ast.c             # Nós da AST
│   ├── ast.h
│   ├── token.h           # Definição de tokens
│   └── main.c            # Entry point do compilador
├── tests/
│   ├── test_lexer.c
│   ├── test_parser.c
│   ├── test_semantic.c
│   └── test_codegen.c
├── examples/             # Programas de exemplo em SIMPLES
│   ├── hello.simples
│   ├── fatorial.simples
│   └── fibonacci.simples
└── Makefile
```

---

## Exemplos SIMPLES → NASM x32

### Exemplo 1 — Atribuição simples

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro x, y, z;
inicio
  x <- 10;
  y <- 20;
  z <- x + y;
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    x resd 1
    y resd 1
    z resd 1

section .text
    global _start

_start:
    ; x := 10
    mov dword [x], 10

    ; y := 20
    mov dword [y], 20

    ; z := x + y
    mov eax, [x]
    add eax, [y]
    mov [z], eax

    ; exit(0)
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

### Exemplo 2 — Leitura e escrita (syscall)

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro x;
inicio
  leia x;
  escreva x;
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    x      resd 1
    buf    resb 12      ; buffer para leitura

section .text
    global _start

_start:
    ; leia x  → sys_read(stdin, buf, 12)
    mov eax, 3          ; sys_read
    mov ebx, 0          ; stdin
    mov ecx, buf
    mov edx, 12
    int 0x80
    ; converter ASCII -> inteiro e armazenar em x
    mov eax, 0
    mov esi, buf
.parse_loop:
    movzx ecx, byte [esi]
    cmp ecx, 10         ; newline
    je .parse_done
    cmp ecx, 13         ; carriage return
    je .parse_done
    sub ecx, 48         ; ASCII '0' = 48
    imul eax, eax, 10
    add eax, ecx
    inc esi
    jmp .parse_loop
.parse_done:
    mov [x], eax

    ; escreva x  → sys_write(stdout, ...)
    mov eax, [x]
    ; converter inteiro -> ASCII em buf
    mov ecx, buf + 11
    mov byte [ecx], 10  ; newline
    dec ecx
.write_loop:
    xor edx, edx
    mov ebx, 10
    div ebx
    add dl, 48
    mov [ecx], dl
    dec ecx
    test eax, eax
    jnz .write_loop
    inc ecx
    ; calcular comprimento
    mov edx, buf + 12
    sub edx, ecx
    ; sys_write
    mov eax, 4
    mov ebx, 1
    int 0x80

    ; exit(0)
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

### Exemplo 3 — Condicional (se/entao/senao/fimse)

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro x;
inicio
  x <- 10;
  se x > 5 entao
    escreva x;
  senao
    escreva 0;
  fimse
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    x resd 1

section .text
    global _start

_start:
    mov dword [x], 10

    ; se x > 5 entao
    mov eax, [x]
    cmp eax, 5
    jle .senao_0       ; NOT (x > 5) → vai para senao

    ; bloco entao: escreva x
    ; ... (rotina de escrita)
    jmp .fimse_0

.senao_0:
    ; bloco senao: escreva 0
    ; ...

.fimse_0:
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

> **Mapeamento de operadores relacionais (condição negada para pulo):**
> 

> `>` → `jle` (pula se NÃO maior) | `<` → `jge` | `=` → `jne`
> 

> | SIMPLES | NASM (negado) |
> 

> |---------|---------------|
> 

> | `>` | `jle .senao_N` |
> 

> | `<` | `jge .senao_N` |
> 

> | `=` | `jne .senao_N` |
> 

> | `<>` | `je .senao_N` |
> 

> | `>=` | `jl .senao_N` |
> 

> | `<=` | `jg .senao_N` |
> 

---

### Exemplo 4 — Laço enquanto/fimenquanto

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro x;
inicio
  x <- 1;
  enquanto x < 6 faca
    escreva x;
    x <- x + 1;
  fimenquanto
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    x resd 1

section .text
    global _start

_start:
    mov dword [x], 1

.enquanto_0:
    mov eax, [x]
    cmp eax, 6
    jge .fimenquanto_0  ; NOT (x < 6) → sai do loop

    ; escreva x
    ; ... (rotina de escrita)

    ; x <- x + 1
    mov eax, [x]
    add eax, 1
    mov [x], eax

    jmp .enquanto_0

.fimenquanto_0:
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

### Exemplo 5 — Laço para/fimpara

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro i;
inicio
  para i de 1 ate 5 passo 1 faca
    escreva i;
  fimpara
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    i resd 1

section .text
    global _start

_start:
    mov dword [i], 1    ; valor inicial

.para_0:
    mov eax, [i]
    cmp eax, 5
    jg .fimpara_0       ; i > 5 → sai do loop

    ; escreva i
    ; ... (rotina de escrita)

    ; passo 1: i <- i + 1
    mov eax, [i]
    add eax, 1
    mov [i], eax

    jmp .para_0

.fimpara_0:
    mov eax, 1
    xor ebx, ebx
    int 0x80
```

> **Passo negativo:** use `sub` em vez de `add` e inverta a condição para `jl .fimpara_N`.
> 

---

### Exemplo 6 — Expressões aritméticas compostas

**Entrada (linguagem SIMPLES):**

```
programa teste
  inteiro a, b, c;
inicio
  a <- 3;
  b <- 4;
  c <- a * b + 2;
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    a resd 1
    b resd 1
    c resd 1

section .text
    global _start

_start:
    mov dword [a], 3
    mov dword [b], 4

    ; c := a * b + 2
    ; Avaliação da expressão com pilha implícita
    mov eax, [a]
    imul eax, [b]       ; eax = a * b
    add eax, 2          ; eax = (a * b) + 2
    mov [c], eax

    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

### Exemplo 7 — Fatorial (programa completo)

**Entrada (linguagem SIMPLES):**

```
programa fatorial
  inteiro n, fat, contador;
inicio
  leia n;
  fat <- 1;
  contador <- 1;
  enquanto contador < n faca
    contador <- contador + 1;
    fat <- fat * contador;
  fimenquanto
  escreva fat;
fim
```

**Saída (NASM 32-bit):**

```nasm
section .bss
    n        resd 1
    fat      resd 1
    contador resd 1
    buf      resb 12

section .text
    global _start

_start:
    ; leia n
    mov eax, 3
    mov ebx, 0
    mov ecx, buf
    mov edx, 12
    int 0x80
    ; parse buf -> n (omitido por brevidade, igual exemplo 2)

    ; fat := 1
    mov dword [fat], 1

    ; contador := 1
    mov dword [contador], 1

.loop_0:
    ; enquanto contador < n
    mov eax, [contador]
    mov ebx, [n]
    cmp eax, ebx
    jge .fim_loop_0

    ; contador := contador + 1
    mov eax, [contador]
    inc eax
    mov [contador], eax

    ; fat := fat * contador
    mov eax, [fat]
    imul eax, [contador]
    mov [fat], eax

    jmp .loop_0

.fim_loop_0:
    ; escreva fat
    ; ... (rotina de escrita, igual exemplo 2)

    mov eax, 1
    xor ebx, ebx
    int 0x80
```

---

### Exemplo 8 — Procedimento com retorno inteiro

**Entrada (linguagem SIMPLES):**

```
procedimento inteiro soma(inteiro a, inteiro b)
inicio
  retorna a + b;
fim

programa demo
inteiro x;
inicio
  x <- soma(2, 3);
  escreval x;
fim
```

**Saída (NASM 32-bit):**

```nasm
global _start

section .data
x dd 0
newline db 10
print_buffer times 12 db 0
read_buffer times 16 db 0

section .text
_start:
    push 3
    push 2
    call proc_soma
    add esp, 8
    mov dword [x], eax
    mov eax, dword [x]
    call print_int
    call print_newline
    mov eax, 1
    xor ebx, ebx
    int 0x80

proc_soma:
    push ebp
    mov ebp, esp
    mov eax, dword [ebp+8]
    push eax
    mov eax, dword [ebp+12]
    mov ebx, eax
    pop eax
    add eax, ebx
    jmp .proc_soma_epilogue
.proc_soma_epilogue:
    mov esp, ebp
    pop ebp
    ret
```

> `jmp .proc_soma_epilogue` desvia para o epílogo compartilhado do procedimento, permitindo que múltiplos `retorna` reutilizem o mesmo bloco de desmontagem do frame.

---

### Exemplo 9 — Procedimento vazio

**Entrada (linguagem SIMPLES):**

```
procedimento vazio imprime_um()
inicio
  escreval 1;
  retorna;
fim

programa demo
inicio
  imprime_um();
fim
```

**Saída (NASM 32-bit):**

```nasm
global _start

section .data
newline db 10
print_buffer times 12 db 0
read_buffer times 16 db 0

section .text
_start:
    call proc_imprime_um
    mov eax, 1
    xor ebx, ebx
    int 0x80

proc_imprime_um:
    push ebp
    mov ebp, esp
    mov eax, 1
    call print_int
    call print_newline
    jmp .proc_imprime_um_epilogue
.proc_imprime_um_epilogue:
    mov esp, ebp
    pop ebp
    ret

; helpers read_int, print_int e print_newline omitidos por brevidade
```

---

### Exemplo 10 — Programa sem procedimentos

**Entrada (linguagem SIMPLES):**

```
programa demo
inteiro x;
inicio
  x <- 1;
  escreval x;
fim
```

---

## Mapeamento Completo: SIMPLES → NASM

| Construção SIMPLES | Instruções NASM | Observação |
| --- | --- | --- |
| `x <- N` (literal) | `mov dword [x], N` |  |
| `x <- y` (variável) | `mov eax, [y]` / `mov [x], eax` | via registrador |
| `x + y` | `mov eax, [x]` / `add eax, [y]` |  |
| `x - y` | `mov eax, [x]` / `sub eax, [y]` |  |
| `x * y` | `mov eax, [x]` / `imul eax, [y]` |  |
| `x div y` | `mov eax, [x]` / `cdq` / `idiv dword [y]` | quociente em `eax` |
| `se x > y entao` | `cmp eax, [y]`  • `jle .senao_N` | condição negada |
| `se x < y entao` | `cmp eax, [y]`  • `jge .senao_N` |  |
| `se x = y entao` | `cmp eax, [y]`  • `jne .senao_N` |  |
| `se x <> y entao` | `cmp eax, [y]`  • `je .senao_N` |  |
| `se x >= y entao` | `cmp eax, [y]`  • `jl .senao_N` |  |
| `se x <= y entao` | `cmp eax, [y]`  • `jg .senao_N` |  |
| `senao` | `jmp .fimse_N` antes + label `.senao_N:` | bloco else |
| `fimse` | label `.fimse_N:` |  |
| `enquanto C faca` | label `.enquanto_N:`  • cond negada + `jmp` |  |
| `fimenquanto` | label `.fimenquanto_N:` |  |
| `para i de A ate B passo P faca` | init + label + cmp + body + add/sub + `jmp` | passo positivo: `add` |
| `fimpara` | label `.fimpara_N:` |  |
| `leia x` | `sys_read`  • conversão ASCII→int | syscall 3 |
| `escreva x` | conversão int→ASCII + `sys_write` | syscall 4 |
| `escreval x` | idem + newline ao final |  |
| `leia x` com `x: flutuante` | `sys_read`  • conversão ASCII→float | syscall 3 |
| `escreva x` com `x: flutuante` | conversão float→ASCII + `sys_write` | syscall 4 |
| `escreval x` com `x: flutuante` | idem + newline ao final |  |
| `a div b` com operandos `flutuante` | divisão real via x87 (`fdivp`) | x87 |
| `inteiro(expr)` | conversão explícita com truncamento para `inteiro` | x87 |
| `e` (lógico) | avaliar ambas + `and` entre flags |  |
| `ou` (lógico) | avaliar ambas + `or` entre flags |  |
| `nao` (lógico) | inversão do jump condicional |  |

Formato inicial para saída de `flutuante`:

- usa notação decimal
- remove zeros redundantes no final da parte fracionária
- mantém pelo menos um dígito após a vírgula/ponto decimal

---

## Syscalls Linux 32-bit usadas

| Operação | `eax` | `ebx` | `ecx` | `edx` |
| --- | --- | --- | --- | --- |
| `sys_exit` | 1 | código de saída | — | — |
| `sys_read` | 3 | 0 (stdin) | buffer | tamanho |
| `sys_write` | 4 | 1 (stdout) | buffer | tamanho |

Todas invocadas via `int 0x80`.

---

## Requisitos Funcionais

| ID | Requisito | Prioridade |
| --- | --- | --- |
| RF01 | Lexer reconhece todos os tokens da tabela | Alta |
| RF02 | Parser constrói AST para todas as construções | Alta |
| RF03 | Análise semântica detecta variáveis não declaradas | Alta |
| RF04 | Análise semântica detecta incompatibilidade de tipos | Alta |
| RF05 | Code generator produz NASM válido para atribuição | Alta |
| RF06 | Code generator produz NASM válido para `leia`/`escreva` | Alta |
| RF07 | Code generator produz NASM válido para `se/entao/fim` | Alta |
| RF08 | Code generator produz NASM válido para `enquanto/faca/fim` | Alta |
| RF09 | Suporte a expressões aritméticas aninhadas | Alta |
| RF10 | Labels únicos gerados para condicionais e laços aninhados | Alta |
| RF11 | Mensagens de erro apontam linha e coluna do problema | Média |
| RF12 | Arquivo `.asm` gerado é montável pelo NASM sem warnings | Alta |
| RF13 | Parser aceita zero ou mais procedimentos antes do bloco `programa` | Alta |
| RF14 | Parser aceita declarações de procedimentos tipados com parâmetros | Alta |
| RF15 | Análise semântica valida assinaturas, aridade e tipos de argumentos | Alta |
| RF16 | Análise semântica valida presença de `retorna <expressao>;` em procedimentos não-`vazio` e rejeita `retorna` com expressão em procedimentos `vazio` | Alta |
| RF17 | Code generator produz NASM válido para chamadas e retornos de procedimentos `inteiro` | Alta |
| RF18 | Code generator produz NASM válido para `flutuante` em leitura e escrita | Alta |
| RF19 | Parser, semantic analyzer e code generator aceitam conversão explícita entre tipos numéricos | Alta |
| RF20 | Parser, semantic analyzer e code generator aceitam retorno `string` com capacidade fixa em procedimentos | Alta |
| RF21 | Parser, semantic analyzer e code generator aceitam parâmetros agregados por valor com cópia local | Alta |

## Out of Scope (v1.0)

- Geração de código para procedimentos `flutuante` já está coberta nesta entrega
- Parâmetros vetoriais por referência em procedimentos
- Matrizes 2D
- Otimizações de código (constant folding, dead code elimination)
- Suporte a Windows (PE/COFF)
- Recuperação de erros (modo pânico) — o compilador para no primeiro erro

Regras atuais para assinaturas de procedimento:

- `procedimento string[32] ...` é válido e retorna em um buffer do chamador.
- Parâmetros `string` são permitidos apenas com capacidade fixa, como `string nome[32]`.
- Parâmetros `string` sem `valor` representam o buffer do chamador.
- Parâmetros agregados com `valor` usam cópia local no procedimento.
- `flutuante` já é aceito no backend para armazenamento, expressões, chamadas, retornos e I/O.
- Conversão explícita numérica usa `inteiro(expr)` ou `flutuante(expr)`.
- `div` com operandos `flutuante` produz divisão real.

---

## Plano de Desenvolvimento com TDD

### Fase 1 — Lexer

```c
/* tests/test_lexer.c */
#include "unity.h"
#include "../src/lexer.h"

void test_numero_inteiro(void) {
    TokenList *tokens = lexer("42");
    TEST_ASSERT_EQUAL(TOK_NUM_INT, tokens->items[0].type);
    TEST_ASSERT_EQUAL_STRING("42", tokens->items[0].lexeme);
    token_list_free(tokens);
}

void test_identificador(void) {
    TokenList *tokens = lexer("variavel");
    TEST_ASSERT_EQUAL(TOK_ID, tokens->items[0].type);
    TEST_ASSERT_EQUAL_STRING("variavel", tokens->items[0].lexeme);
    token_list_free(tokens);
}

void test_atribuicao(void) {
    /* operador de atribuição é <- não := */
    TokenList *tokens = lexer("x <- 10");
    TEST_ASSERT_EQUAL(TOK_ID,      tokens->items[0].type);
    TEST_ASSERT_EQUAL(TOK_ATRIB,   tokens->items[1].type);
    TEST_ASSERT_EQUAL(TOK_NUM_INT, tokens->items[2].type);
    token_list_free(tokens);
}

void test_palavras_reservadas(void) {
    TokenList *tokens = lexer("leia x\nescreva y");
    TEST_ASSERT_EQUAL(TOK_LEIA,    tokens->items[0].type);
    TEST_ASSERT_EQUAL(TOK_ESCREVA, tokens->items[2].type);
    token_list_free(tokens);
}

void test_operadores_relacionais(void) {
    TokenList *tokens = lexer(">= <= <>");
    TEST_ASSERT_EQUAL(TOK_MAIOR_IGUAL, tokens->items[0].type);
    TEST_ASSERT_EQUAL(TOK_MENOR_IGUAL, tokens->items[1].type);
    TEST_ASSERT_EQUAL(TOK_DIFERENTE,   tokens->items[2].type);
    token_list_free(tokens);
}

void test_tokens_laco_para(void) {
    TokenList *tokens = lexer("para i de 1 ate 10 passo 1 faca");
    TEST_ASSERT_EQUAL(TOK_PARA,  tokens->items[0].type);
    TEST_ASSERT_EQUAL(TOK_DE,    tokens->items[2].type);
    TEST_ASSERT_EQUAL(TOK_ATE,   tokens->items[4].type);
    TEST_ASSERT_EQUAL(TOK_PASSO, tokens->items[6].type);
    TEST_ASSERT_EQUAL(TOK_FACA,  tokens->items[8].type);
    token_list_free(tokens);
}

void test_tokens_se_senao_fimse(void) {
    TokenList *tokens = lexer("se entao senao fimse");
    TEST_ASSERT_EQUAL(TOK_SE,    tokens->items[0].type);
    TEST_ASSERT_EQUAL(TOK_ENTAO, tokens->items[1].type);
    TEST_ASSERT_EQUAL(TOK_SENAO, tokens->items[2].type);
    TEST_ASSERT_EQUAL(TOK_FIMSE, tokens->items[3].type);
    token_list_free(tokens);
}
```

### Fase 2 — Parser

```c
/* tests/test_parser.c */
#include "unity.h"
#include "../src/parser.h"

void test_atribuicao_literal(void) {
    ASTNode *ast = parse("x := 42");
    TEST_ASSERT_EQUAL(NODE_ATRIB, ast->type);
    TEST_ASSERT_EQUAL_STRING("x", ast->atrib.var);
    TEST_ASSERT_EQUAL(NODE_NUM, ast->atrib.expr->type);
    TEST_ASSERT_EQUAL(42, ast->atrib.expr->num.value);
    ast_free(ast);
}

void test_se_entao_fim(void) {
    ASTNode *ast = parse("se x > 0 entao\n  escreva x\nfim");
    TEST_ASSERT_EQUAL(NODE_SE, ast->type);
    TEST_ASSERT_EQUAL(NODE_COND, ast->se.cond->type);
    ast_free(ast);
}
```

### Fase 3 — Code Generator

```c
/* tests/test_codegen.c */
#include "unity.h"
#include "../src/codegen.h"
#include <string.h>

void test_atribuicao_gera_mov(void) {
    char *asm_out = compile_simples("x := 10");
    TEST_ASSERT_NOT_NULL(strstr(asm_out, "mov dword [x], 10"));
    free(asm_out);
}

void test_soma_usa_add(void) {
    char *asm_out = compile_simples("z := x + y");
    TEST_ASSERT_NOT_NULL(strstr(asm_out, "add eax"));
    TEST_ASSERT_NOT_NULL(strstr(asm_out, "mov [z], eax"));
    free(asm_out);
}

void test_enquanto_gera_label_e_jmp(void) {
    char *asm_out = compile_simples("enquanto x < 6 faca\n  x := x + 1\nfim");
    TEST_ASSERT_NOT_NULL(strstr(asm_out, ".loop_"));
    TEST_ASSERT_NOT_NULL(strstr(asm_out, "jmp .loop_"));
    TEST_ASSERT_NOT_NULL(strstr(asm_out, "jge .fim_loop_"));
    free(asm_out);
}
```

---

## Comandos de Build e Teste

```bash
# Compilar o compilador
make

# Rodar todos os testes
make test

# Compilar um programa SIMPLES
./simplesc examples/fatorial.simples -o fatorial.asm

# Montar e executar
nasm -f elf32 fatorial.asm -o fatorial.o
ld -m elf_i386 fatorial.o -o fatorial
./fatorial
```

**Makefile:**

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -g
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
BIN     = simplesc

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

test:
	$(CC) $(CFLAGS) -o run_tests tests/*.c src/lexer.c src/parser.c \
	    src/semantic.c src/codegen.c src/ast.c \
	    -Iunity/src unity/src/unity.c
	./run_tests

clean:
	rm -f src/*.o $(BIN) run_tests
```

---

## Critérios de Aceite

- [ ]  `make test` passa com 0 falhas em todos os módulos do compilador
- [ ]  O exemplo `fatorial.simples` compila, monta e executa corretamente
- [x]  O exemplo `fibonacci.simples` compila, monta e executa corretamente
- [ ]  Laços aninhados geram labels únicos sem conflito
- [ ]  Erros de variável não declarada são reportados com número de linha
- [ ]  O `.asm` gerado não produz warnings no NASM
- [ ]  O exemplo `procedure_sum.simples` compila, monta e executa produzindo o resultado correto
- [ ]  O exemplo `procedure_void.simples` compila, monta e executa produzindo o resultado correto
- [x]  Procedimentos `flutuante` compilam, montam e executam corretamente
- [ ]  Um arquivo sem procedimentos (só `programa`) continua compilando corretamente
