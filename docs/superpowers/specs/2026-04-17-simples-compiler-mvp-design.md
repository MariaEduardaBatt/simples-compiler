# Design Spec — SIMPLES Compiler MVP Vertical Slice

## Context

The source PRD defines a full compiler for the SIMPLES language targeting NASM x86 32-bit on Linux. It covers lexer, parser, semantic analysis, code generation, multiple control-flow constructs, and a phased TDD plan.

That scope is too large for a single implementation-ready first spec. This design narrows the first milestone to an end-to-end MVP that proves the full compiler pipeline on a small, canonical subset of the language.

## Goal

Deliver a tiny but complete compiler that:

1. Reads a `.simples` source file.
2. Lexes and parses a canonical integer-only subset of SIMPLES.
3. Performs basic semantic validation.
4. Generates runnable NASM x86 32-bit assembly for Linux.

The MVP is successful when small programs using declarations, assignment, arithmetic, and output compile into assembly that can be assembled, linked, and run.

## Canonicalization Rules

The PRD contains a few inconsistencies between the token table, EBNF, and examples. For this MVP, the spec is normalized instead of trying to support conflicting forms.

Canonical rules for the MVP:

1. Assignment uses `<-`.
2. Program structure uses `programa ... inicio ... fim`.
3. Commands in the MVP end with `;`.
4. Only `inteiro` is supported.
5. Control flow is deferred.

Conflicting examples in the PRD should be treated as documentation bugs for later cleanup, not as alternate syntax accepted by the MVP.

## In Scope

The MVP supports:

- Program declaration with a program name
- Global integer variable declarations
- Integer literals
- Integer identifiers
- Arithmetic expressions with `+`, `-`, `*`, and `div`
- Parenthesized expressions
- Assignment statements
- `escreva` and `escreval`
- Generation of one `.asm` file targeting NASM x86 32-bit Linux
- Line and column metadata on tokens and diagnostics

## Out of Scope for This MVP

The MVP does not include:

- `flutuante`
- `se`, `senao`, `fimse`
- `enquanto`, `faca`, `fimenquanto`
- `para`, `fimpara`
- Procedures
- Semantic recovery
- Code optimization
- Multiple scopes

Those features become later design/implementation slices after the compiler backbone is stable.

## Canonical Grammar

```ebnf
<programa>    ::= "programa" ID <declaracoes> "inicio" <comandos> "fim"
<declaracoes> ::= { "inteiro" ID { "," ID } ";" }
<comandos>    ::= { <atribuicao> ";" | <escrita> ";" }
<atribuicao>  ::= ID "<-" <expressao>
<escrita>     ::= ("escreva" | "escreval") <expressao>
<expressao>   ::= <termo> { ("+" | "-") <termo> }
<termo>       ::= <fator> { ("*" | "div") <fator> }
<fator>       ::= ID | NUM_INT | "(" <expressao> ")"
```

## Architecture

The implementation keeps the PRD's high-level pipeline:

```text
source file
  -> lexer
  -> token stream
  -> parser
  -> AST
  -> semantic analysis
  -> validated AST
  -> code generator
  -> .asm output
```

Each stage has one clear responsibility:

- **Lexer:** scan characters and emit tokens with source positions.
- **Parser:** consume tokens and build an AST for declarations, commands, and expressions.
- **Semantic analyzer:** build the symbol table and reject invalid identifier usage.
- **Code generator:** emit NASM assembly from semantically valid AST input.
- **CLI/main:** load the source file, orchestrate the pipeline, and write output.

The code generator must never compensate for parser or semantic failures. Invalid programs should stop before assembly generation.

## Component Design

### Lexer

The lexer performs a single left-to-right scan and emits a growable token list ending in `TOK_EOF`.

Required token classes:

- Keywords: `programa`, `inicio`, `fim`, `inteiro`, `escreva`, `escreval`, `div`
- Identifiers
- Integer literals
- Operators: `<-`, `+`, `-`, `*`
- Delimiters: `(`, `)`, `,`, `;`

Each token carries:

- `type`
- `lexeme`
- `line`
- `column`

### Parser and AST

The parser should be implemented as a recursive-descent parser over the token list.

The AST only needs node kinds required by the MVP:

- Program
- Declaration list
- Command list
- Assignment
- Write statement
- Binary expression
- Identifier expression
- Integer literal expression

Declarations and commands remain explicit in the tree. Expressions are normalized into binary operator nodes so code generation has a predictable structure.

### Semantic Analysis

The semantic pass walks the AST once and owns a flat global symbol table.

Responsibilities:

- Record declared variables
- Reject duplicate declarations
- Reject uses of undeclared identifiers

No type coercion or multi-scope logic is needed because the MVP is integer-only and program-global.

### Code Generation

The code generator consumes only a semantically valid AST and emits one NASM file.

Expected structure:

- `.data` section for global integer variables
- `.text` section with `_start`
- Assembly for assignments and arithmetic evaluation
- Output support for `escreva` and `escreval`
- Linux `int 0x80` syscall for process exit

Code generation can use a straightforward stack-based or accumulator-based expression strategy, as long as the emitted assembly is deterministic and testable.

## Data Flow and Interfaces

The phase boundaries should be explicit and narrow:

- Lexer output: `TokenList`
- Parser output: `ASTNode *`
- Semantic output: validated AST plus symbol table context
- Codegen output: heap-allocated assembly string or direct file emission through a narrow interface

Recommended contract direction:

1. `lexer(source)` returns tokens or a lexical error.
2. `parse(tokens)` returns an AST or a syntax error.
3. `analyze(ast)` returns success or a semantic error.
4. `generate(ast, symbols)` returns assembly text.

This keeps later features additive instead of forcing early rewrites.

## Error Handling

Error handling is fail-fast.

The compiler stops on the first error and prints a message that includes:

- phase (`lexer`, `parser`, `semantic`)
- human-readable description
- line and column when available

Required MVP failures:

- invalid character
- malformed token sequence
- duplicate declaration
- undeclared variable use

Error recovery is explicitly out of scope.

## Testing Strategy

Testing follows the same vertical-slice design as the implementation.

### Unit tests

- Lexer tests for keywords, identifiers, literals, operators, delimiters, and positions
- Parser tests for declarations, assignments, writes, and expression precedence
- Semantic tests for duplicate declarations and undeclared variables
- Codegen tests for recognizable NASM output fragments for assignments, arithmetic, and output

### End-to-end tests

Golden-path tests should compile full source snippets and verify either:

- stable assembly structure, or
- assembled program output

Minimum end-to-end coverage:

1. Declarations plus assignment
2. Nested arithmetic expression
3. `escreva`/`escreval` output path

## Acceptance Criteria

This MVP is ready for the next slice when:

1. A canonical integer-only SIMPLES program parses and generates NASM successfully.
2. Generated assembly can be assembled and linked for Linux x86 32-bit.
3. A program using arithmetic and output runs with the expected result.
4. Duplicate declarations fail with a semantic diagnostic.
5. Undeclared identifier usage fails with a semantic diagnostic.
6. Tests cover each compiler stage and the full pipeline.

## Follow-On Slice

The next design should add control flow on top of this stable core, starting with `se`/`senao` or `enquanto`, rather than expanding syntax breadth and backend complexity at the same time.
