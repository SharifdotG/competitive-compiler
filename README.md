# competitive-lang

A mini-compiler for **competitive-lang**, a small C++-flavored programming language with built-in functions for competitive programming. Source files (`.cl`) compile through six classical phases — lex → parse → sema → IR → optimize → codegen — straight to native x86-64 Linux executables.

> CSE 430 Compiler Design Lab — individual project. ~3,300 lines of Flex / Bison / C++17 / C, building real x86-64 assembly through the System V AMD64 ABI.

---

## What it looks like

```cpp
// fib.cl — recursion + I/O
int fib(int n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int n;
    read(n);
    print(fib(n));
    return 0;
}
```

```cpp
// sort_search.cl — arrays + built-ins
int main() {
    int n;
    read(n);
    int a[100];
    for (int i = 0; i < n; i = i + 1) {
        read(a[i]);
    }
    sort(a, n);
    int q;
    read(q);
    int idx = binary_search(a, n, q);
    if (idx >= 0) { print("found at"); print(idx); }
    else          { print("not found"); }
    return 0;
}
```

---

## Quick start

```bash
make                                    # builds ./cpc and runtime.o
echo 15 | ./cpc examples/fib.cl -o fib && ./fib   # → 610
make test                               # runs the e2e suite
```

**Requires:** `flex`, `bison`, `g++` (C++17), `gcc` — all standard on Ubuntu 22.04+.

---

## Compiler usage

```text
cpc [flags] FILE.cl
  --tokens     dump tokens and exit
  --ast        dump AST and exit
  --ir         dump IR (three-address code) and exit
  --opt-ir     dump optimized IR and exit
  -S           emit assembly to FILE.s and exit
  -o NAME      output executable name (default a.out)
  -h, --help   show usage
```

The default action is the full compile pipeline: `cpc file.cl -o file` produces a native ELF executable that links against `runtime.o` (shipped with the compiler) and libc.

### Inspecting each phase

Each `.cl` file can be inspected at every stage of compilation:

```bash
./cpc --tokens   examples/fib.cl     # lexical analysis output
./cpc --ast      examples/fib.cl     # syntax tree
./cpc --ir       examples/fib.cl     # three-address intermediate code
./cpc --opt-ir   examples/optimization_demo.cl   # after constant folding + DCE
./cpc -S         examples/fib.cl                  # GAS Intel-syntax assembly
```

---

## Language features

| Category | What's supported |
|---|---|
| **Types** | `int` (i64), `float` (f64), `string`, `bool`, static arrays `T[N]` |
| **Expressions** | arithmetic, comparison, logical (short-circuit `&&` and `\|\|`), unary `-` / `!` |
| **Statements** | declarations, assignments (`=` / `+=` / `-=`), `if`/`else`, `while`, `for`, `break`, `continue`, `return` |
| **Functions** | parameters, return values, recursion, ≥ 7 args (System V ABI stack-passing) |
| **I/O** | `read(x)` and `print(x)` monomorphized at compile time per type |
| **Built-ins** | `sort`, `reverse`, `binary_search`, `gcd`, `lcm`, `max`, `min`, `abs`, `swap`, `len` |
| **Strings** | literals, concat (`+`), equality (`==`/`!=`), `len`, `read` |
| **Diagnostics** | one-pass error accumulation with `LINE:COL: error:` format |

---

## Compiler pipeline

```text
┌─────────┐   tokens    ┌────────┐   AST    ┌──────┐  typed AST  ┌───────┐
│ Lexer   │────────────▶│ Parser │─────────▶│ Sema │────────────▶│ IRGen │
│ (Flex)  │             │ (Bison)│          │      │             │       │
└─────────┘             └────────┘          └──────┘             └───┬───┘
                                                                     │ TAC
                                                                     ▼
┌─────┐  exe   ┌─────┐    .s    ┌─────────┐   TAC'   ┌───────────┐
│ run │◀──────│ gcc │◀────────│ Codegen │◀────────│ Optimizer │
└─────┘        └─────┘          └─────────┘          └───────────┘
                                  x86-64              const-fold + DCE
```

Each box is a real source file in [src/](src/); each arrow is an in-memory data structure passed between visitor passes.

---

## Project layout

```text
.
├── src/
│   ├── lexer.l                Flex tokenizer
│   ├── parser.y               Bison grammar + AST construction
│   ├── ast.{hpp,cpp}          AST node hierarchy + visitor base + AST printer
│   ├── symtab.{hpp,cpp}       scoped symbol table + function table
│   ├── sema.{hpp,cpp}         type checker + return-path analysis
│   ├── ir.{hpp,cpp}           TAC operand/instruction types + IR printer
│   ├── irgen.{hpp,cpp}        AST → TAC visitor (built-in monomorphization)
│   ├── opt.{hpp,cpp}          constant folding + DCE + intra-block prop
│   ├── codegen.{hpp,cpp}      TAC → x86-64 GAS Intel syntax
│   ├── runtime.c              built-in implementations (linked into output)
│   └── main.cpp               CLI driver
├── examples/                  sample programs
├── tests/e2e/                 golden end-to-end tests + runner
├── report/REPORT.md           full design + implementation + results report
└── Makefile
```

---

## Examples

| File | Demonstrates |
|---|---|
| [hello.cl](examples/hello.cl) | string literal + `print` |
| [fib.cl](examples/fib.cl) | recursion, `if`/`else`, `read`/`print` |
| [factorial.cl](examples/factorial.cl) | recursive vs iterative implementations |
| [gcd.cl](examples/gcd.cl) | the `gcd` and `lcm` built-ins |
| [sort_search.cl](examples/sort_search.cl) | arrays, `sort`, `binary_search`, loops |
| [circle.cl](examples/circle.cl) | float arithmetic + xmm codegen |
| [strings.cl](examples/strings.cl) | string variables, `+`, `==`, `len`, `read` |
| [optimization_demo.cl](examples/optimization_demo.cl) | side-by-side `--ir` vs `--opt-ir` |

Run any of them:

```bash
./cpc examples/sort_search.cl -o ss
echo "5 3 1 4 1 5 4" | ./ss
```

---

## Testing

```bash
make test
```

Eight programs in [tests/e2e/](tests/e2e/) are compiled, executed with paired `.stdin` / `.stdout`, and diffed:

```text
PASS builtins
PASS control_flow
PASS factorial
PASS fib
PASS gcd
PASS hello
PASS sort_search
PASS strings
------
passed: 8    failed: 0
```

---

## Further reading

- [report/REPORT.md](report/REPORT.md) — full project report: design (with Mermaid diagrams), grammar, type system, IR opcode reference, stack-frame layout, per-phase implementation notes, results, limitations, references.
- [my-plan.md](my-plan.md) — initial project plan.
- [project-instructions.md](project-instructions.md) — course brief.

---

## Limitations

This is a teaching compiler — `v1` deliberately defers several features to keep the codebase under 3 KLoC:

- No dynamic arrays (`push_back` / growable `vec<T>`); static arrays only.
- No string or array parameters in user-defined functions (locals only).
- No register allocator — every operand lives in a stack slot. Code is correct but slow.
- Optimizer covers constant folding + algebraic identities + intra-block constant propagation + dead-code elimination only. No CSE, LICM, SSA, or peephole.
- Single-file compilation; no modules, no globals.

See the [report](report/REPORT.md#5-limitations-and-future-work) for the full discussion.
