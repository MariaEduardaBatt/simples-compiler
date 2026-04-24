# Compilador SIMPLES → NASM x86 (32-bit)

Compilador da linguagem **SIMPLES** que gera código Assembly **NASM 32-bit** para Linux, implementado em **C (C99)**.

> Projeto acadêmico — Disciplina de Compiladores, IFSULDEMINAS Campus Poços de Caldas.

---

## Visão Geral

O compilador segue a arquitetura clássica **front-end + back-end**, organizado nas seguintes fases:

1. **Análise Léxica** (`lexer`) — transforma o código-fonte em uma sequência de tokens.
2. **Análise Sintática** (`parser`) — constrói a Árvore Sintática Abstrata (AST) a partir dos tokens.
3. **Análise Semântica** (`semantic`) — valida tipos, escopos e declarações usando a tabela de símbolos.
4. **Geração de Código** (`codegen`) — emite código Assembly NASM x86 (32-bit, ELF Linux).

Todas as fases são desenvolvidas com **TDD** usando o framework [Unity](http://www.throwtheswitch.org/unity).

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

```bash
./build/simplesc programa.simples -o programa.asm
```

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

Mais exemplos em [`examples/`](examples/).

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

## Stack Técnica

| Componente | Tecnologia |
| --- | --- |
| Linguagem do compilador | C (C99) |
| Compilador host | GCC |
| Framework de testes | Unity Test Framework |
| Build | Makefile |
| Alvo | NASM x86 32-bit (ELF Linux) |
| Syscalls | Linux 32-bit (`int 0x80`) |

---

## Licença

Projeto acadêmico desenvolvido para fins educacionais.
