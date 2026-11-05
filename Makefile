# Builds the permit-lang kernel port (the intent-lang kernel at 67445ae) on the
# tcc-wasm host with TinyCC. See README.md. src/front/lower.c, src/wasm.c and
# src/ir.h are in the build from S3.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c src/front/fmt.c src/front/lower.c
TARGET = src/wasm.c
# S10 U1: langc report builds the JCS line with the host JSON reader. S11
# U1: langc cedar (src/cedar.c) reads that line. S13 U2: src/jsonout.c is
# the JSON text writer of langc report and langc check --json.
REPORT = src/report.c src/jsonout.c src/cedar.c host/jsonin.c host/jcs.c
# S14 U1: langc mcp (src/mcp.c), langc explain (src/front/codes.c), langc
# base and langc law (src/front/plantab.c).
SRC = src/main.c src/mcp.c src/front/codes.c src/front/plantab.c $(FRONT) $(TARGET) $(REPORT)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
NOTRAP_TOOL = test/notraptool.c
SHA_TOOL = test/shatool.c host/sha256.c
ENGINE_TOOL = test/enginetool.c host/engine.c host/wasmread.c
JCS_TOOL = test/jcstool.c host/jsonin.c host/jcs.c host/sha256.c src/front/base.c
PERMITD = host/permitd.c host/report.c host/engine.c host/wasmread.c host/jsonin.c host/jcs.c host/sha256.c src/front/base.c
FAKEDEPLOY = test/fakedeploy.c host/jsonin.c host/jcs.c src/front/base.c
HOST_TOOL = test/hosttool.c
REPORT_TOOL = test/reporttool.c
MCP_TOOL = test/mcptool.c
SKILL_TOOL = test/skilltool.c
# S16 U2: build/tokcount, the token tool of docs/TOKEN-BASELINE.md (A6).
TOK_TOOL = test/tokcount.c host/sha256.c
HEADERS = $(wildcard src/*.h src/front/*.h)
HOST_HEADERS = $(wildcard host/*.h)

.PHONY: build check check-clang clean tokens

build: build/langc build/fronttool build/pathtool build/notraptool build/shatool build/enginetool build/jcstool build/permitd build/fakedeploy build/hosttool build/reporttool build/mcptool build/skilltool build/tokcount

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) $(HOST_HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -Ihost -o build/langc $(SRC) build/domain.c

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

build/reporttool: $(REPORT_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/reporttool $(REPORT_TOOL)

build/mcptool: $(MCP_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/mcptool $(MCP_TOOL)

build/skilltool: $(SKILL_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/skilltool $(SKILL_TOOL)

build/tokcount: $(TOK_TOOL) $(HOST_HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -Ihost -o build/tokcount $(TOK_TOOL)

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) -Ihost $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)
	$(CC) $(CLANG_FLAGS) $(NOTRAP_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(SHA_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(ENGINE_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(JCS_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(PERMITD)
	$(CC) $(CLANG_FLAGS) -Ihost $(FAKEDEPLOY)
	$(CC) $(CLANG_FLAGS) $(HOST_TOOL)
	$(CC) $(CLANG_FLAGS) $(REPORT_TOOL)
	$(CC) $(CLANG_FLAGS) $(MCP_TOOL)
	$(CC) $(CLANG_FLAGS) $(SKILL_TOOL)
	$(CC) $(CLANG_FLAGS) -Ihost $(TOK_TOOL)

check: build/langc build/fronttool build/pathtool build/notraptool build/shatool build/enginetool build/jcstool build/permitd build/fakedeploy build/hosttool build/reporttool build/mcptool build/skilltool build/tokcount check-clang
	sh test/gate.sh
	sh test/review.sh
	build/fronttool
	build/pathtool
	build/tokcount --check docs/TOKEN-BASELINE.md
	build/tokcount --estimate > build/tokens-estimate.md
	awk '/^<!-- tokcount end -->/ {p = 0} p; /^<!-- tokcount begin -->/ {p = 1}' docs/TOKEN-BASELINE.md | cmp -s - build/tokens-estimate.md && echo "token estimate: passed" || { echo "FAIL tokens: the estimate block of docs/TOKEN-BASELINE.md is not the build/tokcount --estimate output"; exit 1; }

# S16 (A6, Q-S16-1 (a)): the counts of docs/TOKEN-BASELINE.md. Run it outside
# the sandbox with ANTHROPIC_API_KEY set: it builds the Rust crate and calls
# the Anthropic API. It is not in check (Q-S16-1 (a)); check runs the offline
# `build/tokcount --check` (Q-S16-3 (a)) and compares the estimate block of the
# report with the `build/tokcount --estimate` output (Q-S16-1 (a)). A
# `make tokens` run that replaces the estimate block must also remove that
# compare. G1 to G4 run in kit mode and G5 in guest mode (Q-S16-2 (a)).
TOKEN_API = https://api.anthropic.com/v1
TOKEN_HEADERS = -H "x-api-key: $$ANTHROPIC_API_KEY" -H "anthropic-version: 2023-06-01"
TOKEN_PROGRAMS = g1-scope g2-budget g3-delegate g4-approve g5-deploy
TOKEN_EVALS = g1-scope:readOk g1-scope:editMiss g1-scope:readAgain \
  g2-budget:halves g2-budget:rest g2-budget:left \
  g3-delegate:child g3-delegate:oneTwo g3-delegate:twoOne \
  g4-approve:exact g4-approve:usedTwice g4-approve:changed \
  g5-deploy:prod g5-deploy:dev g5-deploy:startSteps
TOKEN_FILES = $(foreach g,$(TOKEN_PROGRAMS),examples/skill/$(g).lang test/baseline/$(g).py test/baseline/rust/src/bin/$(firstword $(subst -, ,$(g))).rs)
RUST_BUILD ?= cargo build -j 2

tokens: build/langc build/tokcount
	@test -n "$$ANTHROPIC_API_KEY" || { echo "FAIL tokens: ANTHROPIC_API_KEY is not set"; exit 1; }
	rm -rf build/tokens
	mkdir -p build/tokens
	{ date -u +%Y-%m-%dT%H:%M:%SZ; python3 --version; rustc --version; cargo --version; } > build/tokens/versions.txt
	cd test/baseline/rust && $(RUST_BUILD)
	for row in $(TOKEN_EVALS); do g=$${row%%:*}; kit=--kit; test $$g = g5-deploy && kit=; build/langc $$kit eval examples/skill/$$g.lang $${row#*:} >> build/tokens/$$g.want || exit 1; done
	for g in $(TOKEN_PROGRAMS); do python3 -I test/baseline/$$g.py > build/tokens/$$g.py.out && cmp -s build/tokens/$$g.want build/tokens/$$g.py.out || { echo "FAIL tokens: test/baseline/$$g.py failed or its output is not the langc eval output"; exit 1; }; test/baseline/rust/target/debug/$${g%%-*} > build/tokens/$$g.rs.out && cmp -s build/tokens/$$g.want build/tokens/$$g.rs.out || { echo "FAIL tokens: the Rust bin $${g%%-*} failed or its output is not the langc eval output"; exit 1; }; done
	@echo "token outputs: 10 of 10"
	curl -sS --fail-with-body $(TOKEN_HEADERS) "$(TOKEN_API)/models?limit=1000" -o build/tokens/models.json
	build/tokcount --model build/tokens/models.json > build/tokens/model.txt
	model=$$(cut -d ' ' -f 1 build/tokens/model.txt); for f in frame $(TOKEN_FILES); do n=$${f##*/}; src=$$f; test $$f = frame && src=; build/tokcount --request "$$model" $$src > build/tokens/$$n.req.json || exit 1; curl -sS --fail-with-body $(TOKEN_HEADERS) -H "content-type: application/json" --data-binary @build/tokens/$$n.req.json "$(TOKEN_API)/messages/count_tokens" -o build/tokens/$$n.json || exit 1; done
	build/tokcount --table build/tokens > build/tokens/table.md
	cat build/tokens/model.txt build/tokens/versions.txt build/tokens/table.md

clean:
	rm -rf build
