# Design Spec — Aula Técnica do Processo de Compilação no SIMPLES

## Contexto

O repositório já possui um material introdutório sobre o pipeline do compilador em `docs/aula-processo-compilacao.md`.

Agora a proposta é criar um segundo material, mais técnico e independente, voltado para leitores que já entendem a visão geral e querem enxergar com mais fidelidade:

- a **AST real** formada pelo compilador
- os **tipos e estruturas reais** usados no projeto
- a relação entre essa AST e os **trechos reais de assembly** gerados pelo backend

O foco continua sendo o compilador SIMPLES deste repositório.

## Objetivo

Produzir um material em `docs/aula-processo-compilacao-mais-tecnico.md` que:

1. Use o mesmo programa `examples/if_then.simples` como estudo de caso.
2. Mostre a AST com os nomes reais do projeto, como `ASTProgram`, `ASTCommand`, `ASTExpression` e `ASTIfCommand`.
3. Explique como partes concretas dessa árvore se conectam com trechos reais do assembly gerado.
4. Funcione de forma independente, sem depender da aula introdutória para fazer sentido.

## Público-alvo

- Leitores com perfil mais técnico.
- Alunos que já entenderam o pipeline geral e querem estudar a implementação.
- Pessoas interessadas em parser, AST e code generation com maior fidelidade ao código-fonte do compilador.

## Exemplo-base

O material continuará usando `examples/if_then.simples`:

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

Esse programa é pequeno, mas suficiente para mostrar:

- um nó de declaração
- um comando de atribuição
- uma expressão binária relacional (`x > 0`)
- um comando `if`
- um comando de escrita

## Formato da Entrega

O material final será um arquivo Markdown em `docs/`, com seções técnicas, trechos de código e diagramas Mermaid.

Ele deve ser um material **independente** da aula introdutória. Pode citar que existe uma versão mais didática, mas não deve depender dela para explicar seus próprios conceitos.

## Estrutura Proposta do Material

### 1. Abertura técnica

Explicar que este material não mostra apenas o pipeline conceitual, mas a estrutura interna usada pelo compilador real.

### 2. Programa-base

Apresentar o mesmo programa `if_then.simples` e deixar claro que toda a análise técnica partirá dele.

### 3. Mapa das estruturas reais da AST

Introduzir, de forma objetiva, as estruturas de `src/ast.h` que participam do exemplo:

- `ASTProgram`
- `ASTDeclaration`
- `ASTCommand`
- `ASTAssignmentCommand`
- `ASTIfCommand`
- `ASTWriteCommand`
- `ASTExpression`
- `ASTBinaryOp`

Essa seção deve mostrar como esses tipos se encaixam no exemplo concreto.

### 4. AST real do programa

Mostrar a árvore do programa usando os nomes reais das estruturas e enums do projeto.

Esse trecho deve representar com fidelidade, em nível estrutural:

- o programa
- a lista de declarações
- a lista de comandos
- o comando de atribuição `x <- 7`
- o comando `if`
- a condição `x > 0`
- o `escreval x` dentro do `then`

Essa seção deve incluir:

1. uma árvore Mermaid
2. uma representação textual próxima do formato das structs

### 5. Onde essa árvore nasce no compilador

Explicar, sem virar walkthrough exaustivo, que:

- o parser reconhece `se ... entao ... fimse`
- o comando vira `AST_COMMAND_IF`
- a condição relacional vira `AST_EXPR_BINARY`
- o operador `>` vira `AST_BINARY_GT`

Essa seção deve citar `src/parser.c` e apontar conceitualmente onde isso acontece.

### 6. Papel da análise semântica nessa AST

Explicar o que a semântica confirma sobre essa árvore:

- que `x` foi declarada
- que a atribuição `x <- 7` é válida
- que a condição `x > 0` é aceitável
- que o comando dentro do bloco `then` é semanticamente válido

Essa seção deve citar `src/semantic.c` como o ponto onde essa validação ocorre.

### 7. Como a AST vira assembly real

Essa é a seção central do material.

Ela deve explicar como nós concretos da AST levam a trechos concretos de assembly, incluindo:

- atribuição de literal inteiro para variável
- carregamento do identificador `x`
- comparação com `0`
- salto condicional para pular o bloco do `if`
- trecho responsável pela escrita com `escreval`

O objetivo não é mostrar o assembly inteiro do arquivo gerado, mas sim **trechos representativos e reais**, com explicação.

### 8. Mapeamento AST → Assembly

Incluir uma tabela de correspondência, por exemplo:

- nó da AST
- papel semântico
- efeito no assembly

Essa tabela deve ajudar o leitor a ligar estrutura de alto nível com instruções de baixo nível.

### 9. Fechamento técnico

Encerrar resumindo que:

- o parser organiza
- a semântica valida
- o codegen baixa a AST para assembly

O fechamento deve reforçar que o `if` não “some”: ele muda de forma, saindo de uma árvore estruturada para comparações, rótulos e saltos.

## Diagramas Mermaid Planejados

O material final deve conter, no mínimo:

1. **AST real do programa**
   - com nomes reais de tipos e comandos
2. **Fluxo de lowering do `if`**
   - `ASTIfCommand -> condição -> comparação -> salto -> bloco then`
3. **Visão de mapeamento estrutural**
   - programa SIMPLES -> AST real -> trechos de assembly

## Nível de Profundidade

O texto deve ser mais técnico que a aula anterior, mas ainda didático.

Isso significa:

- usar os nomes reais das estruturas do projeto
- mostrar trechos reais de assembly
- evitar simplificações que distorçam o compilador
- evitar também virar comentário linha a linha de arquivos grandes como `codegen.c`

## Fora de Escopo

O material não precisa:

- mostrar o assembly completo do programa
- documentar todas as structs de `ast.h`
- cobrir todos os caminhos do codegen
- explicar todo o parser linha por linha
- substituir a documentação de referência do projeto

## Critérios de Sucesso

O material será considerado bem-sucedido se:

1. A AST apresentada corresponder, de forma reconhecível, às estruturas reais do projeto.
2. Os trechos de assembly escolhidos forem realmente representativos do código gerado para o exemplo.
3. O leitor conseguir relacionar `ASTIfCommand`, `AST_EXPR_BINARY` e `AST_BINARY_GT` com o comportamento do `if` no assembly.
4. O arquivo final puder ser lido isoladamente como uma aula técnica curta, sem depender da versão introdutória.
