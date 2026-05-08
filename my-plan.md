# CSE 430 - Compiler Design Lab - My Plan for the Project

## My Plan & Idea

So, My idea for a mini-compiler is create a unique but interesting programming language called "competitive-lang". This language will have the syntax of C++ but with competitive programming features. The main features of this language will include:

- Variables and data types (int, float, string, etc.)
- Control structures (if-else, loops)
- Arrays
- Functions and recursion
- Input and output operations
- Built-in functions for common competitive programming tasks (e.g., sorting, searching)
- And many more features that are commonly used in competitive programming.

## Design

The design of the compiler will consist of the following phases:

1. Lexical Analysis: This phase will tokenize the source code into meaningful tokens.
2. Syntax Analysis: This phase will parse the tokens and create an Abstract Syntax Tree (AST).
3. Semantic Analysis: This phase will check for semantic errors and ensure that the code adheres to the rules of the language.
4. Intermediate Code Generation: This phase will generate an intermediate representation of the code, which will be easier to optimize and translate into machine code.
5. Optimization: This phase will optimize the intermediate code for better performance.
6. Code Generation: This phase will translate the optimized intermediate code into assembly code, which can be executed on a machine.

## Implementation

The implementation of the compiler will be done in using flex and bison for the lexical and syntax analysis phases, respectively. The intermediate code generation, optimization, and code generation phases will be implemented in C++. The compiler will be designed to be modular, allowing for easy maintenance and future enhancements. I am using Ubuntu 22.04 as my operating system, and I will be using GCC as the compiler for C++. Flex and Bison is already installed in my system, so I will be using them for the lexical and syntax analysis phases. I will also be using Git for version control to keep track of my changes. But don't commit yourself. You will tell me I will commit when you tell me to commit. I will also be using Markdown for writing the project report, which will include the design, implementation, and results of the compiler. I will make sure to document my code properly and include comments for better understanding. I will also be testing my compiler with various test cases to ensure its correctness and robustness.
