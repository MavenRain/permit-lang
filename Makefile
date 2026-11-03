# Builds the permit-lang kernel port (the intent-lang kernel at 67445ae) on the
# tcc-wasm host with TinyCC. See README.md. src/front/lower.c, src/wasm.c and
# src/ir.h are in the build from S3.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c src/front/lower.c
TARGET = src/wasm.c
SRC = src/main.c $(FRONT) $(TARGET)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
NOTRAP_TOOL = test/notraptool.c
SHA_TOOL = test/shatool.c host/sha256.c
ENGINE_TOOL = test/enginetool.c host/engine.c host/wasmread.c
JCS_TOOL = test/jcstool.c host/jsonin.c host/jcs.c host/sha256.c src/front/base.c
PERMITD = host/permitd.c host/engine.c host/wasmread.c host/jsonin.c host/jcs.c host/sha256.c src/front/base.c
FAKEDEPLOY = test/fakedeploy.c host/jsonin.c host/jcs.c src/front/base.c
HOST_TOOL = test/hosttool.c
HEADERS = $(wildcard src/*.h src/front/*.h)
HOST_HEADERS = $(wildcard host/*.h)

.PHONY: build check check-clang clean

build: build/langc build/fronttool build/pathtool build/notraptool build/shatool build/enginetool build/jcstool build/permitd build/fakedeploy build/hosttool

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

build/notraptool: $(NOTRAP_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/notraptool $(NOTRAP_TOOL)

build/shatool: $(SHA_TOOL) $(HOST_HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/shatool $(SHA_TOOL)

build/enginetool: $(ENGINE_TOOL) $(HOST_HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/enginetool $(ENGINE_TOOL)

build/jcstool: $(JCS_TOOL) $(HOST_HEADERS) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/jcstool $(JCS_TOOL)

build/permitd: $(PERMITD) $(HOST_HEADERS) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/permitd $(PERMITD)

build/fakedeploy: $(FAKEDEPLOY) $(HOST_HEADERS) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/fakedeploy $(FAKEDEPLOY)

build/hosttool: $(HOST_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/hosttool $(HOST_TOOL)

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)
	$(CC) $(CLANG_FLAGS) $(NOTRAP_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(SHA_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(ENGINE_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(JCS_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(PERMITD)
	$(CC) $(CLANG_FLAGS) -Ihost $(FAKEDEPLOY)
	$(CC) $(CLANG_FLAGS) $(HOST_TOOL)

check: build/langc build/fronttool build/pathtool build/notraptool build/shatool build/enginetool build/jcstool build/permitd build/fakedeploy build/hosttool check-clang
	sh test/gate.sh
	sh test/review.sh
	build/fronttool
	build/pathtool

clean:
	rm -rf build
