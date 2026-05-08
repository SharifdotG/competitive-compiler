# competitive-lang — a mini-compiler

CSE 430 Compiler Design Lab project. A C++-flavored language with a competitive-programming standard library, compiled to real x86-64 Linux assembly via Flex / Bison / C++.

## Building

```
make            # builds ./competc and runtime.o
make test       # runs the e2e test suite
```

Requires: flex, bison, g++ (C++17), gcc — all standard on Ubuntu.

## Usage

```
competc [flags] FILE.cl
  --tokens     dump tokens and exit
  --ast        dump AST and exit
  --ir         dump IR and exit
  --opt-ir     dump optimized IR and exit
  -S           emit assembly to FILE.s
  -o NAME      output executable name (default a.out)
  -h, --help   show this message
```

Default action is full compile to executable: `competc fib.cl -o fib`.

## Quick demo

```
./competc examples/fib.cl -o fib
echo 15 | ./fib            # → 610

./competc examples/sort_search.cl -o ss
echo "5 3 1 4 1 5 4" | ./ss
```

## Project layout

```
src/        compiler source (Flex, Bison, C++ passes, runtime.c)
examples/   sample programs in competitive-lang
tests/e2e/  golden end-to-end tests
report/     final project report
```

## Pipeline

```
.cl → Lexer (Flex) → Parser+AST (Bison) → Sema → IR → Optimizer → Codegen → .s → gcc → a.out
```

See [report/REPORT.md](report/REPORT.md) for full design and implementation details.
