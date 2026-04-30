# SIMPLES Procedure Strings and Float Backend Implementation Plan

> **For agentic workers:** Track execution by updating the checkboxes in this file. Do not skip status updates when a phase completes or when scope changes.

**Goal:** Finish the remaining language slice by adding `string` parameters to procedures by reference and delivering backend support for `flutuante` in controlled phases, while preserving the working integer pipeline and the already-implemented string storage/I/O behavior.

**Architecture:** Reuse the current lexer/parser/AST/semantic/codegen pipeline. Extend the existing procedure metadata and calling convention instead of introducing a second procedure system. Deliver `flutuante` backend in four slices: metadata alignment, scalar storage/expressions, procedure support, and I/O.

**Status Summary:**

- [x] Existing integer procedures are implemented
- [x] Existing string variables, literals, indexing, and I/O are implemented
- [x] Final-slice documentation and metadata alignment
- [x] `string` procedure parameters by reference
- [x] `flutuante` backend in main program
- [x] `flutuante` backend in procedures
- [x] `flutuante` I/O
- [x] PRD/examples/E2E finalization

---

## File Map

### Compiler core

- Modify: `src/ast.h`
- Modify: `src/ast.c`
- Modify: `src/parser.c`
- Modify: `src/semantic.h`
- Modify: `src/semantic.c`
- Modify: `src/codegen.c`
- Modify: `src/codegen.h` if helper exposure is needed for tests

### Tests

- Modify: `tests/test_parser.c`
- Modify: `tests/test_semantic.c`
- Modify: `tests/test_codegen.c`
- Modify: `tests/test_e2e.sh`

### Docs and examples

- Modify: `PRD/prd.md`
- Create: `examples/procedure_string_param.simples`
- Create: `examples/float_main.simples`
- Create: `examples/float_procedure.simples`
- Create later: `examples/float_echo.simples`

---

## Phase 0: Align Spec, Grammar, and Semantic Metadata

**Intent:** Remove the current mismatch where parser, semantic layer, PRD, and codegen disagree about `string` and `flutuante` procedure capabilities.

### Scope

1. Document the final target behavior in the PRD.
2. Restrict procedure return grammar to `inteiro | flutuante | vazio`.
3. Extend semantic signature metadata to preserve parameter storage and capacity.
4. Keep unsupported backend paths rejected until their implementation phase arrives.

### Files

- Modify: `PRD/prd.md`
- Modify: `src/parser.c`
- Modify: `src/semantic.h`
- Modify: `src/semantic.c`
- Test: `tests/test_parser.c`
- Test: `tests/test_semantic.c`

- [x] **Step 1: Add failing parser and semantic tests for the target surface**

Cover:

1. parser rejects `procedimento string nome()`
2. parser accepts `procedimento vazio p(string nome[16])`
3. semantic stores string parameter storage/capacity in procedure signatures
4. semantic rejects `procedimento vazio p(string nome)` without capacity
5. semantic rejects indexed integer/vector parameters that remain out of scope

- [x] **Step 2: Run focused tests and confirm failure**

Run: `make test-parser test-semantic`

- [x] **Step 3: Implement metadata alignment**

Required changes:

1. make `<tipo_retorno>` exclude `string`
2. preserve `AST_STORAGE_INDEXED` and `capacity` for string parameters
3. replace `ASTType *parameter_types` with richer per-parameter signature metadata
4. update existing semantic call validation to use the richer metadata

- [x] **Step 4: Re-run focused tests**

Run: `make test-parser test-semantic`

- [x] **Step 5: Mark phase status**

Update:

- [x] Phase 0 complete

Expected state after completion:

1. PRD, parser, semantic layer, and backend rejection boundaries describe the same language surface
2. the compiler can represent string parameters correctly even before codegen support lands

---

## Phase 1: Support `string` Parameters by Reference

**Intent:** Allow procedures to receive caller-owned string buffers safely, without adding string returns or by-value copies.

### Scope

1. string parameter syntax with fixed capacity
2. semantic validation of argument type and capacity
3. codegen support for passing string addresses
4. codegen support for reading/writing/indexing/string-literal assignment through string parameters

### Files

- Modify: `src/semantic.c`
- Modify: `src/codegen.c`
- Test: `tests/test_semantic.c`
- Test: `tests/test_codegen.c`
- Test: `tests/test_e2e.sh`
- Create: `examples/procedure_string_param.simples`

- [x] **Step 1: Add failing semantic and codegen tests**

Cover:

1. passing a string variable to `string nome[16]`
2. rejecting string literal arguments like `saudacao("ana")`
3. rejecting string actuals with capacity smaller than the formal parameter capacity
4. allowing `escreval nome;` inside the callee
5. allowing `nome[0] <- 65;` inside the callee
6. allowing `nome <- "abc";` inside the callee

- [x] **Step 2: Run focused tests and confirm failure**

Run: `make test-semantic test-codegen`

- [x] **Step 3: Implement semantic call rules**

Required changes:

1. detect string formal parameters in procedure signatures
2. require actual arguments to be named string variables
3. compare actual capacity against formal capacity
4. keep vector parameters rejected

- [x] **Step 4: Implement codegen support**

Required changes:

1. add helper for pushing string base addresses as call arguments
2. distinguish local/global string buffers from string-parameter references
3. route string parameter `leia`/`escreva`/indexing/copy through the referenced address

- [x] **Step 5: Add and wire an E2E example**

Program:

1. main allocates a string
2. main writes a literal into it
3. main calls a procedure that prints and mutates the string
4. main prints again to prove by-reference behavior

- [x] **Step 6: Re-run focused and integration tests**

Run:

1. `make test-semantic test-codegen`
2. `make e2e`

- [x] **Step 7: Mark phase status**

Update:

- [x] Phase 1 complete

---

## Phase 2: Support `flutuante` Storage and Expressions in the Main Program

**Intent:** Remove the current backend rejection for float values in the main program, without yet solving float procedure calls or float I/O.

### Scope

1. float global/local storage layout
2. float literals
3. float assignment and movement
4. float arithmetic
5. float comparisons for control flow
6. integer-to-float promotion in expressions and assignments

### Files

- Modify: `src/semantic.c`
- Modify: `src/codegen.c`
- Test: `tests/test_semantic.c`
- Test: `tests/test_codegen.c`
- Test: `tests/test_e2e.sh`
- Create: `examples/float_main.simples`

- [x] **Step 1: Add failing semantic and codegen tests**

Cover:

1. `flutuante x; x <- 1.5;`
2. `flutuante y; y <- 2;` via promotion
3. `inteiro x; x <- 1.5;` rejected semantically
4. `flutuante z; z <- x + y;`
5. float comparisons in `se` and `enquanto`
6. integer `div` still rejecting float operands

- [x] **Step 2: Run focused tests and confirm failure**

Run: `make test-semantic test-codegen`

- [x] **Step 3: Implement semantic promotion rules**

Required changes:

1. assignment compatibility for `inteiro -> flutuante`
2. binary expression typing for mixed int/float arithmetic
3. mixed relational typing rules
4. keep `flutuante -> inteiro` invalid
5. keep `div` integer-only

- [x] **Step 4: Implement float storage and expression codegen**

Required changes:

1. reserve 8-byte slots for scalar floats
2. emit literal storage labels or inline constant lowering
3. load/store through x87
4. lower arithmetic and comparisons
5. keep integer path unchanged

- [x] **Step 5: Add an E2E example without float I/O**

Program strategy:

1. compute float expressions
2. branch on float comparisons
3. store results
4. print only an integer witness for pass/fail until float I/O exists

- [x] **Step 6: Re-run focused and integration tests**

Run:

1. `make test-semantic test-codegen`
2. `make e2e`

- [x] **Step 7: Mark phase status**

Update:

- [x] Phase 2 complete

---

## Phase 3: Support `flutuante` Procedures, Calls, and Returns

**Intent:** Extend the working float scalar model into procedure signatures and calling convention.

### Scope

1. float parameters
2. mixed integer/float call-site promotion
3. float return values
4. float locals in procedures
5. float expressions inside procedure bodies

### Files

- Modify: `src/semantic.c`
- Modify: `src/codegen.c`
- Test: `tests/test_semantic.c`
- Test: `tests/test_codegen.c`
- Test: `tests/test_e2e.sh`
- Create: `examples/float_procedure.simples`

- [x] **Step 1: Add failing semantic and codegen tests**

Cover:

1. `procedimento flutuante dup(flutuante x)`
2. `retorna 2;` into `flutuante` return via promotion
3. mixed argument calls such as `media(2, 4.0)`
4. float locals inside procedures
5. integer-return procedures still using `eax`
6. float-return procedures using `ST0`

- [x] **Step 2: Run focused tests and confirm failure**

Run: `make test-semantic test-codegen`

- [x] **Step 3: Implement semantic procedure rules**

Required changes:

1. permit `inteiro -> flutuante` in argument matching
2. permit `inteiro -> flutuante` in return expressions
3. keep `flutuante -> inteiro` invalid for calls and returns

- [x] **Step 4: Implement float call/return codegen**

Required changes:

1. assign correct stack offsets for mixed 4-byte and 8-byte parameters
2. push 8-byte float actuals
3. load float formals from frame offsets
4. return float values through `ST0`
5. consume float call results correctly in callers

- [x] **Step 5: Add an E2E example**

Program strategy:

1. call a float-return procedure
2. compare the result against another float
3. print an integer witness until float I/O exists

- [x] **Step 6: Re-run focused and integration tests**

Run:

1. `make test-semantic test-codegen`
2. `make e2e`

- [x] **Step 7: Mark phase status**

Update:

- [x] Phase 3 complete

---

## Phase 4: Support `flutuante` I/O

**Intent:** Finish the user-visible float feature set after arithmetic and procedures are stable.

### Scope

1. `leia` for `flutuante`
2. `escreva` and `escreval` for `flutuante`
3. documented formatting rules

### Files

- Modify: `src/codegen.c`
- Test: `tests/test_codegen.c`
- Test: `tests/test_e2e.sh`
- Create: `examples/float_echo.simples`
- Modify: `PRD/prd.md`

- [x] **Step 1: Add failing tests that define formatting behavior**

Cover:

1. reading `3.5`
2. writing `3.5`
3. writing `2.0`
4. negative float values
5. float values produced by procedure calls

- [x] **Step 2: Run focused tests and confirm failure**

Run: `make test-codegen`

- [x] **Step 3: Implement float read/write helpers**

Required changes:

1. add ASCII-to-float runtime helper
2. add float-to-ASCII runtime helper
3. route `leia`/`escreva`/`escreval` by static type

- [x] **Step 4: Add an E2E example and re-run integration**

Run:

1. `make test-codegen`
2. `make e2e`

- [x] **Step 5: Mark phase status**

Update:

- [x] Phase 4 complete

---

## Phase 5: Final PRD and Regression Sweep

**Intent:** Remove stale documentation and close the slice cleanly.

### Scope

1. update PRD examples and requirements
2. remove obsolete “not supported yet” branches that no longer apply
3. keep explicit rejection only for features still out of scope
4. run the full regression suite

### Files

- Modify: `PRD/prd.md`
- Modify: `tests/test_e2e.sh`
- Modify: any affected tests with stale expected diagnostics

- [x] **Step 1: Update PRD**

Required changes:

1. grammar for string parameters and non-string returns
2. out-of-scope cleanup
3. functional requirements for float backend
4. examples for string parameters and float procedures

- [x] **Step 2: Remove obsolete rejection paths**

Keep explicit rejection only for:

1. `string` return procedures
2. vector parameters
3. any float feature intentionally deferred after Phase 4, if any remain

- [x] **Step 3: Run the full suite**

Run:

1. `make test`
2. `make e2e`

- [x] **Step 4: Mark final status**

Update:

- [x] Phase 5 complete
- [x] Final slice complete

---

## Cross-Phase Guardrails

1. Do not change integer procedure calling convention unless a mixed int/float frame layout forces a localized extension.
2. Do not introduce `string` return values in any phase of this plan.
3. Do not add vector parameters while implementing string parameters.
4. Do not enable `div` for float operands.
5. Do not merge a phase without tests and E2E coverage for that phase.
6. Update this file immediately when a phase finishes so the repository remains the source of truth.
