# MiniCC — A Mini C++ Compiler

**A compiler for a subset of C++, written from scratch in C++17.**
Hand-written lexer, recursive-descent parser, semantic analysis, AST optimizer, peephole optimizer, and an x86-64 code generator. It emits AT&T-syntax assembly that is assembled and linked with `gcc`.

No LLVM. No GCC frontend. No parser generators. Just a lexer, a parser, a type checker, an optimizer, and a code generator.

---

## Overview

| | |
| :--- | :--- |
| **What** | A compiler for a subset of C++. Compiles `.cpp` source to x86-64 assembly. |
| **Why it's interesting** | It implements a real type system (ints, doubles, chars, bools, pointers, structs, arrays), an AST-level optimizer (constant folding, algebraic identities, branch elimination, dead-code removal), and a peephole optimizer over the emitted assembly. |
| **No shortcuts** | The lexer, parser, semantic analyzer, optimizer, and code generator are all hand-written. |
| **Output** | AT&T-syntax x86-64 assembly on stdout, assembled with `gcc`. |
| **Tested** | A shell harness compiles, assembles, runs, and diffs every test against an expected-output file, and checks error tests for expected diagnostics. |
| **Stack** | C++17, CMake ≥ 3.16, System V AMD64 ABI, Linux x86-64 |

---

## Quick start

Requires `cmake`, a C++17 compiler, and `gcc`.

```bash
cmake -S . -B build
cmake --build build
```

Compile a program to assembly and run it:

```bash
./build/minicc --compile tests/hello.cpp > out.s
gcc out.s -o out
./out
```

Run the full test suite:

```bash
bash tests/run_tests.sh ./build/minicc -O1
```

---

## Usage

```text
Usage: minicc [options] <mode> <file>
Modes:
  --compile       emit x86-64 assembly (AT&T syntax) on stdout
  --dump-tokens   print the token stream
  --dump-ast      print the parsed AST
  --dump-typed    print the AST after semantic analysis (with types)
  --dump-opt      print the AST after optimization (with types)
Options:
  -O0 | -O1       disable / enable the optimizer (default: -O1)
  --stats         print optimizer statistics to stderr
```

Example:

```bash
./build/minicc --dump-opt -O1 --stats tests/opt_peephole.cpp
```

---

## The language

A C-like subset of C++.

### Types

| Type | Notes |
| :--- | :--- |
| `int` | 64-bit signed |
| `double` | 64-bit IEEE-754 |
| `char` | 8-bit signed |
| `bool` | 8-bit, `true` / `false` |
| `void` | functions only |
| `T*` | pointers, arbitrary depth |
| `struct Name { ... }` | user structs, value and pointer access |
| `T name[N]` | fixed-size arrays, local and global |
| `T name[] = { ... }` | inferred-size arrays |
| `char s[] = "..."` | string-initialized char arrays |

### Expressions

Arithmetic, comparison, logical `&&` / `||` with short-circuit evaluation, bitwise `& | ^ ~ << >>`, unary `- ! ~`, `++` / `--` (prefix and postfix), compound assignment (`+= -= *= /= %= &= |= ^= <<= >>=`), ternary `?:`, casts `(T)expr`, `sizeof(T)`, address-of `&`, dereference `*`, indexing `a[i]`, member access `.` and `->`, function calls.

### Statements

`if` / `else` / `else if`, `while`, `do ... while`, `for`, `break`, `continue`, `return`, blocks with lexical scoping, declarations with initializers, expression statements.

### Functions

Return values, up to arbitrary argument counts (first six in registers, the rest on the stack), forward declarations, recursion.

### Globals

Scalar and array globals with constant initializers, string globals, struct globals.

### Built-ins

`print_int(int)`, `print_char(int)`, `print_str(char*)`, plus a `print_double(double)` implemented in the language itself as a prelude. The prelude is parsed with the same parser and prepended to every user program.

### Example

```cpp
struct Point { int x; int y; };

int area(struct Rect* r) {
    return (r->max.x - r->min.x) * (r->max.y - r->min.y);
}

int main() {
    int v[8];
    for (int i = 0; i < 8; i = i + 1) { v[i] = i * i; }
    print_int(v[3]);
    struct Point p;
    p.x = 3; p.y = 4;
    print_int(p.x * p.x + p.y * p.y);
    return 0;
}
```

---

## Architecture

```text
Source → Lexer → Tokens → Parser → AST ──→ Sema ──→ Optimizer → Peephole → x86-64 asm
```

| File | Responsibility |
| :--- | :--- |
| `tokens.h` | Token enum and `Token` struct |
| `lexer.cpp` | Hand-written lexer: keywords, identifiers, integer / double / char / string literals, all operators, line/column tracking, `//` and `/* */` comments |
| `ast.h` / `ast_ext.h` | AST node definitions: literals, unary/binary, calls, assignments, casts, address-of/deref, indexing, members, compound assignment, `++`/`--`, ternary, arrays, `do ... while`, globals |
| `parser.cpp` | Recursive-descent parser with precedence climbing, cast disambiguation, array suffix parsing, struct parsing with alignment |
| `types.h` | Type representation (`Base`, pointer depth, struct id), size and alignment, struct layout, name parsing |
| `sema.cpp` | Type checker: declarations, conversions, operators, lvalues, control flow, function signatures, global initializers, lvalue checks, error messages |
| `optimizer.cpp` | AST-level optimizer: constant folding (matching runtime semantics, including 64-bit wraparound and masked shifts), algebraic identities, canonicalisation, branch elimination, dead-statement removal, unreachable-code pruning |
| `codegen.cpp` | x86-64 code generator: SysV calling convention, argument spilling, stack alignment, doubles carried as raw 64-bit patterns in `%rax`, pointer scaling, struct field offsets, scope-aware variable slots |
| `peephole.cpp` | Peephole optimizer over the emitted assembly: `push %rax`/`pop REG` → register moves, `setcc`/`movzbq`/`cmp`/`jcc` → direct conditional jumps, immediate-operand substitution, dead instruction removal after unconditional jumps and returns, jump-to-next-label elimination |
| `ast_dump.cpp` | Pretty-printer for `--dump-ast` / `--dump-typed` / `--dump-opt` |
| `prelude.h` | `print_double` written in the language itself |
| `main.cpp` | CLI driver, prelude injection, sema, optional optimization, optional peephole, `--stats` output |

---

## Optimizer

### AST optimizer (`optimizer.cpp`)

Runs after semantic analysis (it needs expression types) and before code generation.

- **Constant folding.** Integer arithmetic folds with the same semantics the runtime uses: 64-bit wraparound, arithmetic right shift, shift counts masked to 6 bits. Division by zero is never folded, and `INT64_MIN / -1` is never folded.
- **Short-circuit folding.** `0 && x` → `false`, `1 || x` → `true`, with the other operand dropped.
- **Algebraic identities.** `x + 0`, `x - 0`, `x * 1`, `x / 1`, `x | 0`, `x ^ 0`, `x << 0`, `x >> 0` → `x`; `x * 0`, `x & 0` → `0` when `x` is pure.
- **Canonicalisation.** The constant operand is moved to the right of commutative operators; comparisons flip at the same time.
- **Branch elimination.** `if (const)`, `while (0)`, `for` with a constant condition: dead branches and dead loops are removed entirely.
- **Dead statements.** Pure expression statements are removed.
- **Unreachable code.** Everything after a `return`, `break`, `continue`, or a terminating `if`/`else` is pruned.

The `--stats` flag prints folded / simplified / branches / dead-statement counts.

### Peephole optimizer (`peephole.cpp`)

Operates on the emitted assembly, line-oriented, over multiple rounds until a fixpoint.

- `push %rax` / straight-line safe code / `pop REG` → `mov %rax, REG` / code (when the intermediate code doesn't touch `REG` or `%rsp`).
- `setCC %al` / `movzbq %al, %rax` / `cmp $0, %rax` / `jcc L` → a direct conditional jump.
- `mov %rax, %rcx` / `mov $N, %rax` / `<op> %rcx, %rax` → `<op> $N, %rax` (and `imul` by a power of two becomes `shl`).
- Jump-to-next-label removal, `mov %rax, X` / `mov X, %rax` merging, dead instructions after unconditional jumps and `ret`.

---

## Testing

The test harness is `tests/run_tests.sh`.

```bash
bash tests/run_tests.sh ./build/minicc -O0
bash tests/run_tests.sh ./build/minicc -O1
```

It covers three categories:

1. **Execution tests** (`tests/*.cpp`). Each program is compiled, assembled with `gcc`, run, and its stdout diffed against `tests/<name>.expected`.
2. **Error tests** (`tests/errors/*.cpp`). Each file begins with `// ERROR: <expected substring>`; the compiler must fail, and its stderr must contain the substring.
3. **Optimizer tests** (`tests/asm/*.cpp`, `-O1` only). Source files may carry directives:
   - `// ASM-HAS: text` — the `main` body must contain `text`
   - `// ASM-NOT: text` — the `main` body must not contain `text`
   - `// FILE-NOT: text` — the whole assembly file must not contain `text`

   The program must also still print `tests/asm/<name>.expected`.

Test categories include: arithmetic, loops, arrays, pointers, structs, doubles, strings, recursion, globals, conversions, operators, scoping, short-circuit evaluation, constant folding, algebraic identities, peephole patterns, branch fusion, and dead-code elimination.

---

## Design notes

1. **The lexer owns literals and comments.** Every token carries its line and column for diagnostics.
2. **The parser owns precedence.** Precedence is a table of token levels (`binary_levels()`), and casts are disambiguated by a token-level lookahead.
3. **The type checker owns conversions.** Implicit conversions follow a single rule set in `convertible()` and `require_convertible()`; explicit casts are validated against a per-type allow list.
4. **Semantic analysis records every expression's type** in a side table (`Sema::expr_types()`), so the optimizer and code generator can query types without re-deriving them.
5. **The code generator is stack-machine-like.** Expressions evaluate into `%rax`; the stack is only used for spill slots and for argument passing that overflows registers. Doubles are carried around as raw 64-bit patterns in `%rax` and moved through `%xmm0` / `%xmm1` only for arithmetic.
6. **The ABI is the SysV AMD64 ABI.** The first six integer/pointer arguments go into `%rdi %rsi %rdx %rcx %r8 %r9`; the rest are pushed on the stack. The caller pushes a stack frame and keeps `%rsp` aligned to 16 bytes at every call.
7. **Optimization levels are independent.** AST optimization and peephole optimization can each be disabled; `-O0` disables both.

---

## Known limitations

- **No C++ classes, references, templates, namespaces, or exceptions.** The language is a C subset plus `bool`, `double`, and a few conveniences (`++`/`--`, compound assignment, ternary, `sizeof`).
- **No function pointers.** Function names are only usable as call targets.
- **Structs are not first-class values.** Struct parameters must be passed by pointer, and struct values cannot be assigned, copied, or returned by value.
- **Arrays are not first-class values.** Arrays decay to pointers everywhere; `&arr` is rejected.
- **No global non-constant initializers.** Global initializers must be literals (possibly negated).
- **No arrays inside structs.** Struct members may be scalars or pointers, not arrays.
- **Strings are `char*`.** String literals live in `.rodata` and are not mutable.
- **`print_double` rounds by truncation.** The prelude prints exactly six fractional digits, without rounding.
- **No varargs, no `main(int, char**)`.** `main` takes no arguments.
- **The `main` function is required**, and every program must have a defined `main`.

---

## Project structure

```text
minicc/
|-- CMakeLists.txt
|-- src/
|   |-- main.cpp           CLI driver
|   |-- lexer.cpp/.h       Source → tokens
|   |-- tokens.h           Token enum and Token struct
|   |-- parser.cpp/.h      Tokens → AST
|   |-- ast.h              Base AST nodes
|   |-- ast_ext.h          Extended AST nodes (casts, indexing, members, ...)
|   |-- types.h            Types, sizes, struct layout
|   |-- sema.cpp/.h        Type checker
|   |-- optimizer.cpp/.h   AST-level optimizer
|   |-- peephole.cpp/.h    Peephole optimizer over assembly text
|   |-- codegen.cpp/.h     x86-64 code generator
|   |-- ast_dump.cpp/.h    AST pretty-printer
|   |-- prelude.h          print_double in minicc source
|-- tests/
|   |-- run_tests.sh       Execution, error, and optimizer tests
|   |-- *.cpp / *.expected Feature tests
|   |-- errors/            Negative tests
|   |-- asm/               Optimizer directive tests
```

---

## License

Released under the MIT license.
