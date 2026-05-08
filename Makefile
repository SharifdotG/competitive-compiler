CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wno-unused-function -g
CC       := gcc
CFLAGS   := -Wall -g

SRC := src
GEN := build/gen
OBJ := build/obj
BIN := competc

OBJECTS := \
	$(OBJ)/main.o          \
	$(OBJ)/parser.tab.o    \
	$(OBJ)/lex.yy.o        \
	$(OBJ)/ast.o           \
	$(OBJ)/symtab.o        \
	$(OBJ)/sema.o          \
	$(OBJ)/ir.o            \
	$(OBJ)/irgen.o         \
	$(OBJ)/opt.o           \
	$(OBJ)/codegen.o

.PHONY: all clean test

all: $(BIN) runtime.o

$(BIN): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Bison generates both parser.tab.cpp and parser.tab.hpp from one invocation.
$(GEN)/parser.tab.cpp $(GEN)/parser.tab.hpp: $(SRC)/parser.y
	@mkdir -p $(GEN)
	bison -d -o $(GEN)/parser.tab.cpp $(SRC)/parser.y

$(GEN)/lex.yy.cpp: $(SRC)/lexer.l $(GEN)/parser.tab.hpp
	@mkdir -p $(GEN)
	flex -o $(GEN)/lex.yy.cpp $(SRC)/lexer.l

$(OBJ)/main.o: $(SRC)/main.cpp $(GEN)/parser.tab.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/ast.o: $(SRC)/ast.cpp $(SRC)/ast.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/symtab.o: $(SRC)/symtab.cpp $(SRC)/symtab.hpp $(SRC)/ast.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/sema.o: $(SRC)/sema.cpp $(SRC)/sema.hpp $(SRC)/symtab.hpp $(SRC)/ast.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/ir.o: $(SRC)/ir.cpp $(SRC)/ir.hpp $(SRC)/ast.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/irgen.o: $(SRC)/irgen.cpp $(SRC)/irgen.hpp $(SRC)/ir.hpp $(SRC)/ast.hpp $(SRC)/symtab.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/opt.o: $(SRC)/opt.cpp $(SRC)/opt.hpp $(SRC)/ir.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/codegen.o: $(SRC)/codegen.cpp $(SRC)/codegen.hpp $(SRC)/ir.hpp $(SRC)/symtab.hpp $(SRC)/ast.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/parser.tab.o: $(GEN)/parser.tab.cpp $(GEN)/parser.tab.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -c $< -o $@

$(OBJ)/lex.yy.o: $(GEN)/lex.yy.cpp $(GEN)/parser.tab.hpp
	@mkdir -p $(OBJ)
	$(CXX) $(CXXFLAGS) -I$(GEN) -I$(SRC) -Wno-register -c $< -o $@

runtime.o: $(SRC)/runtime.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(BIN) runtime.o a.out *.s

test: $(BIN)
	@bash tests/e2e/run_tests.sh
