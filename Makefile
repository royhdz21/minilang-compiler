CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -Isrc

FRONT = src/common.c src/lexer/lexer.c src/parser/parser.c src/parser/ast.c \
        src/semantics/semantics.c src/ir/codegen.c

all: bin/compile bin/run

bin/compile: $(FRONT) src/main_compile.c $(wildcard src/*/*.h) src/common.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(FRONT) src/main_compile.c

bin/run: src/runtime/tacvm.c src/main_run.c src/runtime/tacvm.h
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ src/runtime/tacvm.c src/main_run.c

test: all
	@bash tests/run_tests.sh

clean:
	rm -rf bin

.PHONY: all test clean
