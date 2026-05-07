# Aula Técnica: AST real e geração de assembly no compilador SIMPLES

## Objetivo

Este material mostra uma visão mais fiel à implementação do compilador SIMPLES.

Em vez de trabalhar apenas com a ideia geral do pipeline, aqui o foco é:

- a AST real usada pelo projeto
- os tipos e estruturas definidos em `src/ast.h`
- a passagem da AST para trechos reais do assembly gerado

## Programa-base

O estudo de caso desta aula é o arquivo `examples/if_then.simples`:

```simples
programa demo
inteiro x;
inicio
  x <- 7;
  se x > 0 entao
    escreval x;
  fimse
fim
```

Esse exemplo é pequeno, mas já contém os elementos necessários para discutir:

- declaração
- atribuição
- expressão binária relacional
- comando `if`
- comando de escrita

## 1. Mapa das estruturas reais da AST

Para esse programa, as estruturas mais importantes em `src/ast.h` são:

- `ASTProgram`
- `ASTDeclaration`
- `ASTCommand`
- `ASTAssignmentCommand`
- `ASTIfCommand`
- `ASTWriteCommand`
- `ASTExpression`
- `ASTBinaryOp`

Em termos de organização, o compilador representa o programa como:

- um `ASTProgram`
- com uma lista de declarações (`ASTDeclaration`)
- e uma lista de comandos (`ASTCommand`)

Dentro dessa lista de comandos, o nosso exemplo possui:

1. uma atribuição `x <- 7`
2. um comando `if`

Dentro do `if`, a condição `x > 0` é uma expressão binária, e o bloco `then` contém um comando `escreval x`.
