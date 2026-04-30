# Compilador SIMPLES → NASM x86 (32-bit)

Compilador da linguagem **SIMPLES** que gera código Assembly **NASM 32-bit** para Linux, implementado em **C (C99)**.

> Projeto acadêmico — Disciplina de Compiladores, IFSULDEMINAS Campus Poços de Caldas.

---

## Visão Geral

O compilador segue a arquitetura clássica **front-end + back-end**, dividida em quatro fases sequenciais. A saída de cada fase é a entrada da próxima, formando um *pipeline* de transformações que parte de um texto-fonte em linguagem SIMPLES e termina em código Assembly executável.

Todas as fases são desenvolvidas com **TDD (Test-Driven Development)** usando o framework [Unity](http://www.throwtheswitch.org/unity).

### Fases do Compilador

#### 1. Análise Léxica (`lexer`)

É a **primeira leitura** do código-fonte. O analisador léxico (ou *scanner*) percorre o texto caractere a caractere e agrupa sequências significativas em **tokens** — as menores unidades com significado da linguagem, como palavras-chave (`programa`, `se`), identificadores (`x`, `soma`), números (`42`, `3.14`) e operadores (`<-`, `+`).

Nesta fase também são descartados espaços em branco e comentários, e reportados erros como caracteres inválidos ou literais mal formados.

**Entrada:** texto-fonte (`.simples`) → **Saída:** fluxo de tokens.

#### 2. Análise Sintática (`parser`)

Recebe o fluxo de tokens e verifica se eles **formam frases válidas** segundo a gramática da linguagem SIMPLES. Se o código estiver sintaticamente correto, o parser constrói uma **Árvore Sintática Abstrata (AST)** — uma estrutura em árvore que representa hierarquicamente os comandos, expressões e blocos do programa.

Erros comuns detectados aqui: ponto-e-vírgula faltando, blocos mal aninhados, expressão sem operando, etc.

**Entrada:** tokens → **Saída:** AST.

#### 3. Análise Semântica (`semantic`)

Com a AST construída, a análise semântica verifica se o programa **faz sentido lógico**, mesmo que sintaticamente válido. Usa uma **tabela de símbolos** para rastrear variáveis, tipos e escopos, verificando regras como:

- Variáveis declaradas antes de usadas
- Compatibilidade de tipos em atribuições e operações (`inteiro` vs `flutuante`)
- Identificadores não redeclarados no mesmo escopo
- Procedimentos chamados com os argumentos corretos

**Entrada:** AST → **Saída:** AST anotada (com informações de tipo e símbolos) ou lista de erros semânticos.

#### 4. Geração de Código (`codegen`)

A última fase traduz a AST anotada em **código Assembly NASM x86 (32-bit)** para Linux. Cada nó da árvore é convertido em instruções de máquina equivalentes:

- Declarações de variáveis → reservas na seção `.bss` / `.data`
- Atribuições e expressões → sequências de `mov`, `add`, `sub`, `imul`, etc.
- Estruturas de controle (`se`, `enquanto`, `para`) → rótulos e desvios condicionais (`jmp`, `jz`, `jnz`)
- Comandos de E/S (`leia`, `escreva`, `escreval`) → chamadas de sistema Linux via `int 0x80`

O código gerado usa a convenção de registradores `eax`, `ebx`, `ecx`, `edx` e pode ser montado com `nasm` e executado diretamente em Linux 32-bit.

**Entrada:** AST anotada → **Saída:** arquivo `.asm` em NASM 32-bit.

---

## Estrutura do Projeto

```
compiler_c/
├── src/              # Código-fonte do compilador
│   ├── token.{c,h}   # Definição de tokens
│   ├── lexer.{c,h}   # Analisador léxico
│   ├── ast.{c,h}     # Árvore sintática abstrata
│   ├── parser.{c,h}  # Analisador sintático
│   ├── semantic.{c,h}# Analisador semântico
│   ├── codegen.{c,h} # Gerador de código NASM
│   ├── error.{c,h}   # Tratamento de erros
│   └── main.c        # Ponto de entrada do compilador
├── tests/            # Testes unitários (Unity) e E2E
├── examples/         # Programas de exemplo em SIMPLES
├── docs/             # Documentação
├── PRD/              # Product Requirements Document
├── third_party/      # Dependências externas (Unity)
└── Makefile          # Build e alvos de teste
```

---

## Requisitos

- **GCC** com suporte a C99
- **Make**
- **NASM** (para montar o código gerado)
- **ld** (linker GNU, suporte a `elf_i386`)
- **Linux** ou ambiente compatível para executar binários 32-bit

---

## Compilação

Para compilar o compilador e os testes:

```bash
make all
```

O binário principal é gerado em `build/simplesc`.

---

## Uso

Gerando Assembly com o compilador:

```bash
./build/simplesc programa.simples -o programa.asm
```

O formato antigo também continua aceito:

```bash
./build/simplesc programa.simples programa.asm
```

Para facilitar a compilação e o link final, há um wrapper interativo em `run.sh`:

```bash
./run.sh
```

Ele pergunta o arquivo SIMPLES de entrada e o binário de saída, gera o `.asm`, monta com `nasm` e linka com `ld`.

Para montar e executar o código gerado em Linux:

```bash
nasm -f elf32 programa.asm -o programa.o
ld -m elf_i386 programa.o -o programa
./programa
```

---

## Exemplo de Programa SIMPLES

```
programa demo
inteiro x;
inicio
    x <- (2 + 3) * (4 - 1);
    escreva x;
    escreval x;
fim
```

Mais exemplos em [`examples/`](examples/), incluindo:

- [`examples/fatorial.simples`](examples/fatorial.simples)
- [`examples/fibonacci.simples`](examples/fibonacci.simples)
- [`examples/vector_float_multiply.simples`](examples/vector_float_multiply.simples)
- [`examples/float_div_cast.simples`](examples/float_div_cast.simples)

---

## Testes

Executar a suíte completa de testes unitários:

```bash
make test
```

Executar fases individualmente:

```bash
make test-token      # Tokens
make test-lexer      # Analisador léxico
make test-parser     # Analisador sintático
make test-semantic   # Analisador semântico
make test-codegen    # Gerador de código
```

Executar testes end-to-end (compila → monta → executa):

```bash
make e2e
```

Limpar artefatos de build:

```bash
make clean
```

---

## A Linguagem SIMPLES

Linguagem didática em português estruturado com 47 tokens, incluindo:

- **Palavras-chave:** `programa`, `inicio`, `fim`, `se`, `entao`, `senao`, `enquanto`, `para`, `procedimento`, `retorna`, etc.
- **Tipos:** `inteiro`, `flutuante`, `vazio`
- **E/S:** `leia`, `escreva`, `escreval`
- **Operadores:** `<-` (atribuição), aritméticos, relacionais e lógicos (`e`, `ou`, `nao`)
- **Comentários:** `//` (linha) e `/* ... */` (bloco)

Especificação completa da gramática em [`PRD/prd.md`](PRD/prd.md).

---

## Gramática EBNF

```ebnf
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

Especificação completa e notas de compatibilidade continuam em [`PRD/prd.md`](PRD/prd.md).

---

## Stack Técnica

| Componente | Tecnologia | Papel no projeto |
| --- | --- | --- |
| **Linguagem do compilador** | C (C99) | Linguagem de implementação do compilador — escolhida pela proximidade com o hardware, controle manual de memória e por ser referência na literatura de compiladores. |
| **Compilador host** | GCC | Usado para compilar o próprio compilador (`gcc -std=c99 -Wall -Wextra -Werror`). Flags estritas garantem código limpo e sem *warnings*. |
| **Build system** | GNU Make | Automatiza a compilação modular, os alvos de teste por fase e os testes end-to-end via `Makefile`. |
| **Framework de testes** | [Unity Test Framework](http://www.throwtheswitch.org/unity) | Framework minimalista em C para testes unitários — usado em TDD para cada fase do compilador. Incluído em `third_party/unity`. |
| **Testes E2E** | Shell script (`sh`) | Script `tests/test_e2e.sh` que compila, monta (NASM), linka (ld) e executa os programas-exemplo, comparando a saída esperada. |
| **Linguagem-alvo** | NASM x86 32-bit (ELF Linux) | Assembly gerado pelo compilador. NASM é o *assembler* usado para produzir o binário ELF 32-bit. |
| **Syscalls** | Linux 32-bit via `int 0x80` | Interface com o sistema operacional para E/S (`sys_read`, `sys_write`, `sys_exit`) — convenção usada no código gerado. |
| **Convenção de registradores** | `eax`, `ebx`, `ecx`, `edx` | Registradores x86 de 32 bits utilizados para argumentos de syscalls e operações aritméticas no código gerado. |
| **Controle de versão** | Git + GitHub | Versionamento do código-fonte e colaboração. |

---

## Licença

Projeto acadêmico desenvolvido para fins educacionais.
