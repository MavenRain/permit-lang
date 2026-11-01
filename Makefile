# Builds the permit-lang kernel port (the intent-lang kernel at 67445ae) on the
# tcc-wasm host with TinyCC. See README.md. src/front/lower.c, src/wasm.c and
# src/ir.h stay out of the build until the target slice (S3).
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c
SRC = src/main.c $(FRONT)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c,$(FRONT))
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c,$(FRONT))
HEADERS = $(wildcard src/*.h src/front/*.h)

.PHONY: build check check-clang clean

build: build/langc build/fronttool build/pathtool

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

build/fronttool: $(FRONT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/fronttool $(FRONT_TOOL)

build/pathtool: $(PATH_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/pathtool $(PATH_TOOL)

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)

check: build/langc build/fronttool build/pathtool check-clang
	sh test/gate.sh
	sh test/review.sh
	build/fronttool
	build/pathtool

clean:
	rm -rf build
