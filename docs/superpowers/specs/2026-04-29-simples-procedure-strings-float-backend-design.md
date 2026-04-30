# SIMPLES Procedure Strings and Float Backend Design

## Problem

The compiler already supports:

1. integer programs end-to-end
2. typed procedures with integer-only backend support
3. first-class fixed-capacity `string` storage, literals, indexing, and I/O
4. `flutuante` support delivered end-to-end in the backend slice

The remaining gap at the start of this design was the final language slice:

1. `string` values still could not participate in procedure signatures with a stable ABI
2. aggregate pass-by-value and string return were still undocumented in the language surface
3. `flutuante` support had to be delivered incrementally through code generation

We need a documented path that closes these gaps without destabilizing the working integer pipeline.

## Goals

1. Support `string` parameters in procedures.
2. Keep `string` behavior compatible between the main program and procedure bodies.
3. Implement backend support for `flutuante` incrementally instead of in one large change.
4. Preserve existing integer behavior and existing string storage/I/O behavior.
5. Make every phase testable and independently mergeable.
6. Keep the calling convention understandable and explicit in the docs.

## Non-Goals

1. String concatenation.
2. Lexicographic string comparisons.
3. Runtime bounds checking for vector or string indexing.
4. Full optimizer work.

## Resolved Decisions

### Strings in procedure signatures

1. `string` parameters are supported.
2. `string` parameters use fixed-capacity syntax in the signature: `string nome[32]`.
3. `string` parameters without `valor` are passed by reference.
4. `string` parameters with `valor` are passed by value with a local copy in the callee.
5. Aggregate parameters use the same `valor` marker for by-value semantics.
6. Procedure return type `string[32]` is supported and copies into a caller-owned buffer.
7. The declared capacity is part of the parameter and return contract and is preserved in semantic metadata.

### Float model

1. `flutuante` is represented as 64-bit `double`.
2. Global `flutuante` variables reserve 8 bytes.
3. Local `flutuante` variables reserve 8 bytes in the stack frame.
4. Float arithmetic and comparisons use the x87 FPU backend in this slice.
5. Integer-only operator `div` remains integer-only.
6. Real-number division is out of scope until the language gains an explicit float division operator.
7. Implicit `inteiro -> flutuante` promotion is allowed where the target operation or destination type is `flutuante`.
8. Implicit `flutuante -> inteiro` conversion is rejected semantically.

## Recommended Delivery Order

1. Align the language surface and documentation around `string` procedure parameters.
2. Implement `string` procedure parameters by reference.
3. Implement `flutuante` backend for storage, assignment, expressions, and scalar movement.
4. Extend `flutuante` backend to procedures, calls, and returns.
5. Add `flutuante` I/O last.

This order minimizes risk because `string` parameters are mostly a calling-convention extension on top of already-working string storage, while `flutuante` requires a separate runtime and codegen model.

## Grammar Changes

### Procedures

Use the existing declaration syntax consistently in parameters:

```ebnf
<procedimento>        ::= "procedimento" <tipo_retorno> ID "(" [ <parametros> ] ")"
                          "inicio" <declaracoes_locais> <comandos> "fim"

<parametros>          ::= <parametro> { "," <parametro> }
<parametro>           ::= <tipo_parametro> ID [ "[" NUM_INT "]" ] [ "valor" ]

<tipo_parametro>      ::= "inteiro" | "flutuante" | "string"
<tipo_retorno>        ::= "inteiro" | "flutuante" | "vazio" | "string" "[" NUM_INT "]"
```

### Signature rules

1. `inteiro x` is a scalar integer parameter.
2. `flutuante x` is a scalar float parameter.
3. `string nome[32]` is a string buffer parameter passed by reference.
4. `string nome[32] valor` is a string buffer parameter passed by value.
5. `inteiro nums[10] valor` is a vector parameter passed by value.
6. `string nome` is invalid.
7. `inteiro nums[10]` without `valor` is rejected.
8. `procedimento string ...` is valid only with fixed capacity, like `procedimento string[32] ...`.

## Semantic Rules

### String parameter semantics

1. `string` parameters must include fixed capacity.
2. String parameter names share the same uniqueness rules as other parameters.
3. A string argument for by-reference parameters must be a named string variable, not a string literal.
4. The argument must have type `string`.
5. The argument capacity must be greater than or equal to the declared parameter capacity.
6. By-value string parameters accept any string expression whose source capacity fits the formal capacity.
7. By-value vector parameters accept named vector variables whose capacity fits the formal capacity.
8. Inside the callee, a by-value aggregate parameter behaves like owned storage in the callee frame; by-reference string parameters still alias the caller buffer.

### Float semantics

1. `inteiro` expressions may flow into `flutuante` destinations by promotion.
2. `flutuante` values may not flow into `inteiro` destinations without an explicit conversion.
3. Arithmetic with at least one `flutuante` operand yields `flutuante`.
4. Integer `div` requires both operands to be `inteiro`.
5. Real `div` with a `flutuante` operand yields `flutuante`.
5. Relational operators accept:
   - `inteiro` with `inteiro`
   - `flutuante` with `flutuante`
   - mixed `inteiro`/`flutuante`, promoting to `flutuante`
6. Logical operators still operate on integer truth values as today.
7. Procedure call argument checks must account for `inteiro -> flutuante` promotion.
8. Return validation must account for `inteiro -> flutuante` promotion when the procedure return type is `flutuante`.

## AST and Semantic Metadata Changes

### Parameter metadata

Keep using `ASTParameter`, but make its storage and capacity semantically meaningful:

1. scalar integer/float parameters use `AST_STORAGE_SCALAR`
2. string parameters use `AST_STORAGE_INDEXED`
3. string parameter capacity is stored in `capacity`

### Procedure signature metadata

The existing `ProcedureSignature` shape is no longer enough for string parameters because type alone cannot distinguish:

1. scalar `inteiro`
2. scalar `flutuante`
3. string-by-reference with capacity

Extend semantic procedure metadata so each parameter carries:

1. type
2. storage kind
3. capacity

Recommended shape:

```c
typedef struct {
    ASTType type;
    ASTStorageKind storage;
    size_t capacity;
} ParameterInfo;

typedef struct {
    char *name;
    ASTType return_type;
    ParameterInfo *parameters;
    size_t parameter_count;
} ProcedureSignature;
```

This change is required before the call checker can validate string arguments safely.

## Calling Convention Design

### String parameters

String parameters are passed as 32-bit addresses on the stack.

Caller:

1. computes the base address of the string variable
2. pushes that address as one 32-bit argument

Callee:

1. reads the parameter slot as an address
2. uses that address as the base for:
   - `leia nome;`
   - `escreva nome;`
   - `escreval nome;`
   - `nome[i]`
   - `nome <- "abc";`

No copy is performed on call entry or return.

### Float scalar parameters and returns

Float procedure support will use:

1. 8-byte stack arguments for scalar `flutuante` parameters
2. x87 `ST0` for `flutuante` return values
3. 8-byte local storage slots

Integer procedure behavior remains unchanged:

1. integer arguments use 4-byte stack slots
2. integer returns use `eax`

## Code Generation Design

### Phase A: string procedure parameters

Required backend behavior:

1. load string parameter address from `[ebp+offset]`
2. treat that address as the string base in read/write/index/copy helpers
3. distinguish local string buffers from referenced string parameters
4. reject string literals as call arguments for string parameters

### Phase B: float storage and expressions

Required backend behavior:

1. reserve 8 bytes for each scalar `flutuante`
2. load/store float values through x87 helpers
3. lower float literals into emitted constants
4. support:
   - assignment
   - unary minus
   - `+`
   - `-`
   - `*`
   - relational comparisons
5. keep `div` on the integer path only

### Phase C: float procedures

Required backend behavior:

1. push 8-byte float arguments
2. read float parameters from the stack frame with correct offsets
3. return floats through `ST0`
4. allow mixed integer/float calls with promotion at the call site

### Phase D: float I/O

Required runtime/backend behavior:

1. parse ASCII input into 64-bit floating-point representation
2. print floating-point values to text
3. define a fixed initial formatting rule

Recommended initial formatting rule:

1. print a decimal representation
2. trim redundant trailing zeros
3. always print at least one digit before and after the decimal point for non-integer float forms only if required by the chosen implementation

The exact formatting policy must be documented in tests before implementation starts.

## Safety and Simplicity Boundaries

1. Do not add string ownership tracking.
2. Do not add heap allocation.
3. Do not add hidden output buffers for string returns.
4. Do not add vector parameters in this slice.
5. Do not add general implicit numeric coercion everywhere; only permit `inteiro -> flutuante`.
6. Do not silently reinterpret 4-byte integer slots as 8-byte float slots.

## PRD Updates Required

1. Remove `Assinaturas de procedimento com string` from out-of-scope.
2. Clarify that `string` is allowed in parameter position only with fixed capacity and by-reference semantics.
3. Clarify that `procedimento string` is not part of the grammar.
4. Clarify that `flutuante` backend support is being delivered in phases.
5. Clarify that `div` is integer-only.
6. Add examples for:
   - string parameter by reference
   - float arithmetic in main
   - float-return procedure
   - float input/output once Phase D is delivered

## Example Programs

### String parameter by reference

```simples
procedimento vazio saudacao(string nome[16])
inicio
  escreval nome;
fim

programa demo
string pessoa[16];
inicio
  pessoa <- "ana";
  saudacao(pessoa);
fim
```

### Float arithmetic in main

```simples
programa demo
flutuante x, y, z;
inicio
  x <- 1.5;
  y <- 2;
  z <- x + y;
fim
```

### Float-return procedure

```simples
procedimento flutuante media(flutuante a, flutuante b)
inicio
  retorna (a + b) * 0.5;
fim

programa demo
flutuante x;
inicio
  x <- media(2.0, 4.0);
fim
```

## Testing Strategy

1. Add semantic tests before codegen work whenever a new acceptance rule changes.
2. Add codegen tests for every new calling-convention rule.
3. Add at least one E2E example per phase.
4. Keep old integer-only E2E programs in the regression suite.
5. Add explicit compile-fail tests for unsupported combinations that remain out of scope.

## Phase Status Tracking

- [x] Baseline integer procedures
- [x] Baseline string storage, literals, indexing, and I/O
- [x] Phase 0: align grammar, semantic metadata, and PRD for the final slice
- [x] Phase 1: support `string` parameters by reference
- [x] Phase 2: support `flutuante` storage and expressions in the main program
- [x] Phase 3: support `flutuante` procedures, calls, and returns
- [x] Phase 4: support `flutuante` I/O
- [x] Phase 5: remove obsolete backend rejection paths and finalize docs/examples
