# Example Catalog Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand `examples/` with short SIMPLES programs that demonstrate the currently implemented language features without adding unsupported syntax.

**Architecture:** Keep examples as small standalone `.simples` files, one primary concept per file. Validate every new example by extending `tests/test_e2e.sh` first, then add only the files required to satisfy those failing checks.

**Tech Stack:** SIMPLES source files, shell E2E script, NASM/ld toolchain

---

### Task 1: Add failing E2E checks for the new examples

**Files:**
- Modify: `tests/test_e2e.sh`
- Test: `tests/test_e2e.sh`

- [ ] **Step 1: Write the failing checks**

```sh
"$compiler" examples/if_then.simples "$e2e_dir/if_then.asm"
nasm -f elf32 "$e2e_dir/if_then.asm" -o "$e2e_dir/if_then.o"
ld -m elf_i386 "$e2e_dir/if_then.o" -o "$e2e_dir/if_then"
[ "$("$e2e_dir/if_then")" = "7" ]

"$compiler" examples/if_then_else.simples "$e2e_dir/if_then_else.asm"
nasm -f elf32 "$e2e_dir/if_then_else.asm" -o "$e2e_dir/if_then_else.o"
ld -m elf_i386 "$e2e_dir/if_then_else.o" -o "$e2e_dir/if_then_else"
[ "$("$e2e_dir/if_then_else")" = "0" ]
```

- [ ] **Step 2: Run E2E to verify it fails**

Run: `make e2e`
Expected: FAIL because `examples/if_then.simples` and `examples/if_then_else.simples` do not exist yet.

- [ ] **Step 3: Commit the red test**

```bash
git add tests/test_e2e.sh
git commit -m "test: add E2E checks for example catalog"
```

### Task 2: Add the missing example programs

**Files:**
- Create: `examples/if_then.simples`
- Create: `examples/if_then_else.simples`
- Test: `tests/test_e2e.sh`

- [ ] **Step 1: Create the `if_then` example**

```text
programa demo
inteiro x;
inicio
  x <- 7;
  se x > 0 entao
    escreval x;
  fimse
fim
```

- [ ] **Step 2: Create the `if_then_else` example**

```text
programa demo
inteiro x;
inicio
  x <- -1;
  se x > 0 entao
    escreval x;
  senao
    escreval 0;
  fimse
fim
```

- [ ] **Step 3: Run E2E to verify the new examples pass**

Run: `make e2e`
Expected: PASS with `e2e ok`

- [ ] **Step 4: Run the full suite**

Run: `make test && make e2e`
Expected: all unit tests PASS and `e2e ok`

- [ ] **Step 5: Commit the green change**

```bash
git add examples/if_then.simples examples/if_then_else.simples tests/test_e2e.sh
git commit -m "feat: add control-flow examples"
```

### Task 3: Keep the catalog aligned with implemented features

**Files:**
- Verify only: `examples/print.simples`
- Verify only: `examples/while.simples`
- Verify only: `examples/for.simples`

- [ ] **Step 1: Confirm existing examples already cover prints, while, and for**

Check:
- `examples/print.simples` demonstrates `escreva` and `escreval`
- `examples/while.simples` demonstrates `enquanto`
- `examples/for.simples` demonstrates `para ... ate ... passo`

- [ ] **Step 2: Explicitly do not add a `leia` example**

Reason: the current branch does not implement `leia`, so every example in `examples/` must remain compilable with the shipped compiler.

- [ ] **Step 3: Verify working tree state**

Run: `git status --short`
Expected: only the intended example and E2E files appear.
