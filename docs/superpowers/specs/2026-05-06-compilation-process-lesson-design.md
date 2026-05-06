# Design Spec — Aula sobre o Processo de Compilação no SIMPLES

## Contexto

O repositório implementa um compilador da linguagem SIMPLES em C, com pipeline clássico de compilação: análise léxica, análise sintática, análise semântica e geração de código NASM x86 32-bit para Linux.

O objetivo desta entrega não é alterar o compilador, mas criar um material didático em Markdown com diagramas Mermaid para explicar esse pipeline a iniciantes, usando um exemplo real do projeto com `se ... entao`.

## Objetivo

Produzir um material em `docs/` que funcione como roteiro de aula, pronto para apresentação, cobrindo:

1. O que significa compilar um programa.
2. O que cada fase do compilador recebe e devolve.
3. Como um programa SIMPLES simples atravessa o pipeline.
4. Como o comando `se ... entao` aparece da leitura do fonte até a geração de assembly.

## Público-alvo

- Iniciantes em compiladores.
- Leitores que precisam de uma explicação guiada e visual.
- Público com pouca familiaridade prévia com AST, tabela de símbolos e geração de código.

## Exemplo-base

O material usará como exemplo principal o arquivo `examples/if_then.simples`:

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

Esse exemplo é pequeno, realista dentro do repositório e suficiente para mostrar:

- declaração
- atribuição
- expressão relacional
- estrutura de controle `se ... entao`
- saída com `escreval`

## Formato da Entrega

O material final será um arquivo Markdown em `docs/`, com linguagem didática e blocos Mermaid embutidos.

O texto deve ser organizado como roteiro de aula em tópicos, não como documentação de referência nem como especificação de implementação.

## Estrutura Proposta da Aula

### 1. Abertura

Explicar, em linguagem simples, que compilar é transformar um programa escrito em uma linguagem de mais alto nível em uma forma executável ou mais próxima da máquina.

### 2. Visão Geral do Pipeline

Mostrar um diagrama Mermaid com o fluxo:

`código-fonte -> tokens -> AST -> validação semântica -> assembly`

Essa seção deve deixar claro o contrato entre as fases: a saída de uma é a entrada da próxima.

### 3. Programa de Exemplo

Apresentar o programa SIMPLES base e destacar que ele será acompanhado ao longo de toda a aula.

### 4. Análise Léxica

Explicar:

- o que é um token
- como palavras-chave, identificadores, números e operadores são reconhecidos
- como o `se`, `>`, `0` e `entao` aparecem nessa etapa

Essa seção deve incluir uma tabela de tokens simplificada do exemplo.

### 5. Análise Sintática

Explicar:

- que o parser verifica a estrutura do programa
- que o resultado é uma AST
- que a AST representa intenção e estrutura, não o texto original literal

Essa seção deve incluir um diagrama Mermaid simplificado da árvore do exemplo, com destaque para o nó do `if`.

### 6. Análise Semântica

Explicar:

- declaração antes do uso
- compatibilidade básica de tipos
- validação da condição do `se`

Para o exemplo, o foco é mostrar que `x` foi declarado, recebeu um valor inteiro e pode ser usado na comparação e na impressão.

### 7. Geração de Código

Explicar, em nível didático:

- que o backend transforma a AST válida em assembly
- que o `se` vira comparação + salto condicional + rótulos
- que `escreval` vira uma sequência de saída no assembly gerado

Essa seção deve incluir um diagrama Mermaid de fluxo para o `se`, sem exigir leitura aprofundada de NASM.

### 8. Fechamento

Encerrar com um resumo curto:

- o que cada fase faz
- o que entra e sai de cada fase
- por que separar o compilador em fases ajuda a entender, testar e evoluir o projeto

## Diagramas Mermaid Planejados

O material final deve conter, no mínimo, estes diagramas:

1. **Pipeline do compilador**
   - fonte -> lexer -> parser -> semantic -> codegen
2. **AST simplificada do exemplo**
   - programa, atribuição e `if`
3. **Fluxo do `se ... entao` no backend**
   - avaliar condição -> comparar -> desviar -> executar bloco -> continuar

## Nível de Profundidade

O texto deve ser tecnicamente fiel ao compilador do repositório, mas com simplificações didáticas:

- evitar detalhes internos demais do código em C quando não agregarem à aula
- usar vocabulário acessível
- introduzir AST e semântica com exemplos curtos
- priorizar entendimento sobre exaustão técnica

## Fora de Escopo

O material não precisa:

- explicar toda a implementação interna dos arquivos C
- cobrir todas as construções da linguagem SIMPLES
- detalhar otimizações
- mostrar assembly completo linha por linha
- virar apostila longa

## Critérios de Sucesso

O material será considerado bem-sucedido se:

1. Um iniciante conseguir entender a função de cada fase do compilador.
2. O exemplo com `se ... entao` conectar todas as etapas da explicação.
3. Os diagramas Mermaid ajudarem a visualizar o fluxo sem depender de conhecimento prévio.
4. O arquivo final puder ser usado como base de aula ou apresentação com pouca adaptação.
