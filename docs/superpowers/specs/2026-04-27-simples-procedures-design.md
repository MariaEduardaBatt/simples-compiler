# SIMPLES Procedures Design

## Problem

The compiler already supports a single `programa ... inicio ... fim` entrypoint plus declarations, expressions, control flow, I/O, and integer code generation to NASM 32-bit. The PRD also mentions `flutuante`, `vazio`, `procedimento`, and `retorna`, but the current grammar and implementation do not yet support procedures, calls, or typed returns.

We want to extend SIMPLES so users may optionally declare typed procedures, call them either as expressions or commands, and keep the language coherent with the existing Portuguese syntax. This change must also update the PRD with grammar, examples, and an implementation path that preserves the current compiler architecture.

## Goals

1. Add optional top-level procedure declarations to SIMPLES.
2. Support typed parameters and typed returns: `inteiro`, `flutuante`, and `vazio`.
3. Allow calls in expressions for non-`vazio` procedures.
4. Allow calls as standalone commands, especially for `vazio`.
5. Keep procedure scope isolated to parameters and local declarations.
6. Support the full feature end-to-end for `inteiro`.
7. Accept `flutuante` in grammar, AST, and semantic analysis, while keeping backend rejection explicit until float code generation exists.

## Non-Goals

1. Shared access from procedures into program globals.
2. Full NASM code generation for `flutuante`.
3. Nested procedures.
4. Overloading by arity or type.
5. Multiple `programa` blocks per file.

## Recommended Approach

Use an incremental design that fits the current compiler pipeline:

1. Extend the lexer/parser/AST/semantic layers for typed procedures, calls, and returns.
2. Implement complete code generation only for `inteiro` procedures.
3. Accept `flutuante` declarations and signatures in the front-end, but fail explicitly in code generation if a float procedure or float call would require backend support.

This keeps the language surface consistent now without forcing premature x87/FPU design decisions.

## Syntax and Grammar

Top-level file structure:

```ebnf
<arquivo>             ::= <procedimentos> <programa>
<procedimentos>       ::= { <procedimento> }
<programa>            ::= "programa" ID <declaracoes_globais> "inicio" <comandos> "fim"
```

Procedures:

```ebnf
<procedimento>        ::= "procedimento" <tipo_retorno> ID "(" [ <parametros> ] ")"
                          "inicio" <declaracoes_locais> <comandos> "fim"
<parametros>          ::= <parametro> { "," <parametro> }
<parametro>           ::= <tipo> ID
<declaracoes_globais> ::= { <declaracao_variavel> }
<declaracoes_locais>  ::= { <declaracao_variavel> }
<declaracao_variavel> ::= <tipo> ID { "," ID } ";"
<tipo>                ::= "inteiro" | "flutuante"
<tipo_retorno>        ::= "inteiro" | "flutuante" | "vazio"
```

Commands and calls:

```ebnf
<comandos>            ::= { <comando> }
<comando>             ::= <atribuicao>
                        | <cmd_leia>
                        | <cmd_escreva>
                        | <cmd_se>
                        | <cmd_enquanto>
                        | <cmd_para>
                        | <cmd_chamada>
                        | <cmd_retorna>

<cmd_chamada>         ::= ID "(" [ <argumentos> ] ")" ";"
<cmd_retorna>         ::= "retorna" [ <expressao> ] ";"
<argumentos>          ::= <expressao> { "," <expressao> }
```

Expression extension:

```ebnf
<fator>               ::= ID
                        | NUM_INT
                        | NUM_FLOAT
                        | ID "(" [ <argumentos> ] ")"
                        | "(" <expressao> ")"
                        | "nao" <fator>
                        | "-" <fator>
```

### Syntax Notes

1. Procedures are optional. A file may start directly with `programa`.
2. Procedures appear before the single `programa` block.
3. `retorna` is a normal command and may appear anywhere inside a procedure body.
4. The main `programa` block does not allow `retorna`.
5. Using semicolons on declarations, calls, and returns keeps the new grammar unambiguous and aligned with the implementation-friendly style already used in tests.

## Semantic Rules

### Procedure declarations

1. Procedure names must be unique.
2. Parameter names and local variable names must be unique within the same procedure scope.
3. Procedures do not capture or access program globals.
4. The program block continues to use only global declarations plus its own commands.

### Calls

1. Every call must reference a declared procedure.
2. Argument count must match parameter count exactly.
3. Argument types must match parameter types.
4. A non-`vazio` procedure may be used as an expression.
5. A `vazio` procedure may only be used as a command.
6. A non-`vazio` procedure may also be called as a command, discarding the result.

### Return validation

1. `procedimento vazio` accepts either `retorna;` or no explicit return.
2. `procedimento inteiro` and `procedimento flutuante` require valid `retorna <expressao>;` on every successful control-flow path.
3. `retorna;` inside a non-`vazio` procedure is a semantic error.
4. `retorna <expressao>;` inside a `vazio` procedure is a semantic error.
5. `retorna` outside a procedure is a semantic error.

## AST Changes

## Top-level program model

Replace the current single-block AST shape with:

1. top-level procedures list
2. program name
3. global declarations
4. main commands

## New nodes

Add:

1. `ASTProcedure`
2. `ASTParameter`
3. `ASTExpressionCall`
4. `ASTCommandCall`
5. `ASTCommandReturn`
6. type metadata for declarations and procedure signatures

This keeps procedure definitions, call expressions, and return statements explicit instead of encoding them indirectly.

## Semantic Model

Extend semantic analysis with:

1. a procedure signature table: name, return type, parameter list
2. a local scope per procedure: parameters plus local declarations
3. expression typing that can infer call result types
4. control-flow validation for non-`vazio` returns

The current symbol-only model is insufficient because variables and procedures now live in different namespaces with typed signatures.

## Code Generation Design

## Supported in this slice

End-to-end NASM generation for `inteiro` procedures only.

### Procedure calling convention

1. Emit one label per procedure: `proc_<nome>`.
2. Pass arguments via stack, right to left.
3. Build a standard frame with `push ebp` / `mov ebp, esp`.
4. Address parameters and locals relative to `ebp`.
5. Return integer values in `eax`.
6. Lower `retorna <expr>;` by evaluating into `eax` and jumping to a shared epilogue label.

### Main block behavior

The main `programa` still lowers to `_start` and exits via Linux syscall as today.

### Float behavior

If code generation encounters a procedure declaration, return, argument passing, or call site requiring `flutuante` backend support, compilation must fail with an explicit diagnostic instead of silently miscompiling.

Recommended diagnostic shape:

```text
Code generation for flutuante procedures is not supported yet.
```

## PRD Updates Required

1. Replace the current top-level EBNF with the procedure-aware grammar.
2. Remove procedures from the PRD out-of-scope list.
3. Clarify that procedures are optional.
4. Clarify that `retorna` returns a value expression, not a type token.
5. Add functional requirements for:
   - declaring procedures
   - calling procedures with arguments
   - validating signatures
   - validating returns
   - generating NASM for integer procedures
6. Keep float code generation explicitly out of scope for now, even though float syntax and semantic support are introduced.

## PRD Examples Required

### Integer-return procedure

```simples
procedimento inteiro soma(inteiro a, inteiro b)
inicio
  retorna a + b;
fim

programa demo
inicio
  escreval soma(2, 3);
fim
```

### Void procedure

```simples
procedimento vazio saudacao()
inicio
  escreval 1;
  retorna;
fim

programa demo
inicio
  saudacao();
fim
```

### Program without procedures

```simples
programa demo
inteiro x;
inicio
  x <- 1;
  escreval x;
fim
```

## Testing Strategy

1. First verify current `flutuante` behavior so the baseline is explicit.
2. Add lexer tests for new keywords and punctuation patterns involving procedures.
3. Add parser tests for:
   - procedure declarations
   - empty parameter lists
   - typed parameter lists
   - call expressions
   - call commands
   - return commands
4. Add semantic tests for:
   - undeclared procedures
   - arity mismatch
   - type mismatch
   - illegal `retorna`
   - missing return in non-`vazio`
   - illegal global access from procedures
5. Add codegen tests for:
   - integer call/return lowering
   - stack argument passing
   - local variable offsets
   - explicit rejection of float procedures in backend
6. Add end-to-end examples for integer procedures only.

## Implementation Phases

1. Validate and document the current `flutuante` baseline.
2. Extend tokens, AST, parser, and semantic analysis for procedures.
3. Implement integer-only procedure code generation.
4. Update the PRD with grammar, requirements, and examples.
5. Add regression and end-to-end coverage.

## Open Decisions Already Resolved

1. Use `procedimento`, not `procedure`.
2. Procedures are optional.
3. Procedures appear before `programa`.
4. Calls are allowed both as expressions and as standalone commands.
5. `vazio` is command-only.
6. Procedure scope is isolated to parameters and locals.
7. `retorna` carries an optional expression and may appear anywhere in the body.
8. `flutuante` is front-end supported first, backend later.
