# SIMPLES Arrays and Strings Design

## Problem

The current compiler already supports scalar `inteiro`, partial front-end support for `flutuante`, control flow, I/O, and typed procedures with integer-only backend support. The PRD still leaves arrays/vectors and strings out of scope, but they are the next feature with the best value-to-risk ratio if we want to keep building the language without destabilizing the compiler.

We want to add one-dimensional vectors and a first-class `string` type while avoiding large refactors in the lexer/parser/semantic/codegen pipeline that is already working. We explicitly want to defer `flutuante` backend work until later, and also keep two-dimensional matrices out of this slice.

## Goals

1. Add one-dimensional fixed-size vectors.
2. Add a first-class fixed-capacity `string` type.
3. Support indexed access for vectors and strings.
4. Support basic assignment and `leia` / `escreva` flows for strings.
5. Preserve compatibility with existing scalar/program/procedure behavior.
6. Keep the implementation incremental and localized.

## Non-Goals

1. Two-dimensional matrices.
2. String concatenation.
3. Lexicographic string comparisons.
4. Passing vectors or strings by value between procedures.
5. Runtime bounds checking in this slice.
6. Full `flutuante` backend support.

## Recommended Approach

Use a declaration-driven extension of the existing type system:

1. Keep `inteiro` and `flutuante` scalar declarations working as-is.
2. Extend declarations to optionally carry a fixed capacity via `[N]`.
3. Introduce `string` as a dedicated type that always requires a fixed capacity.
4. Represent indexed access explicitly in the AST instead of encoding it as special identifier text.
5. Reuse the same infrastructure for both vector indexing and string character access.

This is the lowest-risk path because it layers on top of the current declaration/expression model without requiring a redesign of procedures, scopes, or expression evaluation.

## Syntax and Grammar

## Types and declarations

```ebnf
<tipo>                ::= "inteiro" | "flutuante" | "string"

<declaracao>          ::= <tipo> <item_declaracao> { "," <item_declaracao> } ";"
<item_declaracao>     ::= ID [ "[" NUM_INT "]" ]
```

Examples:

```simples
inteiro x;
inteiro nums[10];
string nome[32];
```

### Type rules

1. `inteiro x;` is a scalar integer.
2. `inteiro nums[10];` is a one-dimensional integer vector with 10 elements.
3. `string nome[32];` is a fixed-capacity string buffer with 32 bytes.
4. `string nome;` is invalid in this slice.
5. Matrices such as `inteiro tab[3][4];` remain out of scope.

## Indexed access and assignment

```ebnf
<atribuivel>          ::= ID | ID "[" <expressao> "]"
<atribuicao>          ::= <atribuivel> "<-" <expressao> ";"

<fator>               ::= ID
                        | ID "[" <expressao> "]"
                        | NUM_INT
                        | NUM_FLOAT
                        | STRING_LITERAL
                        | ID "(" [ <argumentos> ] ")"
                        | "(" <expressao> ")"
                        | "nao" <fator>
                        | "-" <fator>
```

Examples:

```simples
nums[i] <- 42;
total <- nums[i] + 1;
nome <- "ana";
escreval nome;
```

## New tokens

Add only the minimum lexical surface needed:

1. `TOK_STRING` for the `string` type keyword
2. `TOK_STRING_LITERAL` for literals like `"abc"`
3. `TOK_ABRE_COL` for `[`
4. `TOK_FECHA_COL` for `]`

## AST Design

## Declarations

Extend declarations with:

1. base type
2. whether storage is scalar or indexed
3. fixed capacity when indexed

This lets the compiler keep existing declaration flows while distinguishing `x`, `nums[10]`, and `nome[32]` cleanly.

## Expressions and assignment targets

Add:

1. string literal expression node
2. indexed access expression node
3. assignable target node or equivalent explicit representation for scalar vs indexed assignment

The important design constraint is to avoid overloading identifiers with embedded syntax like `"nums[i]"`. Indexing should stay structural in the AST.

## Semantic Rules

## Declaration validation

1. Capacities must be positive integer literals.
2. `string` declarations must always include capacity.
3. Existing duplicate-name checks still apply.
4. Scalars, vectors, and strings share the same namespace rules already used in each scope.

## Indexed access

1. Only vectors and strings may be indexed.
2. Index expressions must have type `inteiro`.
3. Scalars may not be indexed.
4. Vectors may not be used as plain scalar expressions without indexing.

## Assignment rules

1. Scalar integer assignment still behaves as today.
2. `nums[i] <- expr;` requires `expr` compatible with the vector element type.
3. `nome <- "abc";` is valid for `string`.
4. `nome <- outroNome;` may be supported only if we intentionally add buffer-copy semantics; otherwise reject it in this slice.
5. `nome[0] <- expr;` is allowed and treated as byte/integer character storage.

## I/O rules

1. `leia x;` remains valid for integer scalars.
2. `leia nome;` reads a string into the fixed-capacity buffer.
3. `escreva nome;` and `escreval nome;` print a string until the terminating `0`.
4. `escreva "abc";` and `escreval "abc";` print string literals directly.
5. `leia nums;` is invalid.
6. `escreva nums;` is invalid unless indexed.

## Procedures

To minimize implementation risk in this slice:

1. string and vector declarations are allowed inside procedures using the same syntax.
2. Passing strings or vectors by value as parameters is out of scope.
3. Procedure signatures should remain restricted to the already-supported scalar model unless we explicitly expand them later.

This keeps the feature focused on storage, indexing, and I/O instead of mixing in by-value aggregate calling semantics.

## Code Generation Design

## Integer vectors

1. Global vectors reserve contiguous 32-bit slots in global storage.
2. Local vectors reserve contiguous frame space on the stack.
3. `nums[i]` computes base address plus `i * 4`.
4. Reads/writes use the resulting address as a normal l-value/r-value location.

## Strings

1. Global strings reserve `N` bytes.
2. Local strings reserve `N` bytes in the frame.
3. String literal assignment copies bytes into the target buffer and appends `0`.
4. `leia nome;` reads up to capacity minus terminator and writes `0` at the end.
5. `escreva nome;` scans until `0` and calls `sys_write`.
6. `escreva "abc";` may lower via literal data emission or a transient labeled buffer, whichever best matches current codegen style.

## Safety boundary for this slice

Do not add runtime bounds checking here. Semantic analysis should validate index type, but not index range. This avoids a large expansion of control-flow-heavy backend code and keeps vectors/strings aligned with the current low-level codegen model.

## PRD Updates Required

1. Add `string`, `STRING_LITERAL`, `[` and `]` to the token tables.
2. Update the EBNF with declaration capacities, indexed access, and string literals.
3. Remove `Vetores e strings` from out-of-scope.
4. Add `Matrizes 2D` to out-of-scope explicitly.
5. Add functional requirements for:
   - vector parsing
   - string parsing
   - indexed semantic validation
   - string I/O
   - vector element codegen
6. Add examples for:
   - integer vector indexing
   - fixed-capacity string input/output
   - a normal scalar-only program to confirm compatibility

## Testing Strategy

1. Lexer tests for `string`, string literals, and bracket tokens.
2. Parser tests for indexed declarations, indexed expressions, and string literals.
3. Semantic tests for:
   - invalid scalar indexing
   - invalid vector use without index
   - non-integer index types
   - missing string capacity
   - invalid `leia`/`escreva` targets
4. Codegen tests for:
   - vector element address calculation
   - local vector/frame layout
   - string literal copy
   - string read/write lowering
5. E2E examples for:
   - integer vector accumulation
   - reading and printing a string

## Implementation Notes to Minimize Risk

1. Reuse existing declaration arrays and symbol metadata instead of inventing a second declaration pipeline.
2. Keep procedure signatures scalar-only for this slice to avoid aggregate calling convention changes.
3. Reuse one indexed-access AST node for both vectors and strings.
4. Keep matrix syntax and semantics out entirely, not partially implemented.

## Open Decisions Already Resolved

1. Implement vectors 1D and strings now; matrices 2D later.
2. Use a dedicated `string` type.
3. Require fixed capacity with bracket syntax, e.g. `string nome[32];`.
4. Support only basic operations now: indexed access, assignment, and `leia`/`escreva`.
5. Skip runtime bounds checking in this slice.
