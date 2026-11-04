#!/bin/sh
# The kit gate. `make check` runs it from the kit directory after the build.
set -eu
fail=0
count=0
# kit LIST FILE prints --kit when FILE is a line of LIST. A row whose text uses a
# kit name or a Nat or word literal is a kit row (orbit-lang S3); the others run
# in guest mode.
kit() {
  if [ -f "$1" ] && rg -q -x -F "$2" "$1"; then echo --kit; fi
}
for prog in examples/*.lang; do
  out=$(build/langc $(kit examples/kit.txt "${prog#examples/}") check "$prog" 2>&1) || true
  count=$((count + 1))
  if [ "$out" != "ok" ]; then
    echo "FAIL $prog: $out"
    fail=$((fail + 1))
  fi
done
echo "examples: $count checked, $fail failed"

tmp=${TMPDIR:-/tmp}/langc-gate.$$
mkdir -p "$tmp"
awk 'BEGIN { printf "def deep : Nat := " } { } END {
  for (i = 0; i < 1100; i++) printf "("; printf "1"; for (i = 0; i < 1100; i++) printf ")"; print "" }' </dev/null >"$tmp/deep.lang"

# refusals DIR LABEL: each line of DIR/expect.txt is `FILE CODE`. `langc check`
# must exit 1 and print `langc: CODE: ...` on stderr.
refusals() {
  checked=0
  while read -r file code; do
    path="$1/$file"
    [ "$file" = "deep.lang" ] && path="$tmp/deep.lang"
    status=0
    build/langc $(kit "$1/kit.txt" "$file") check "$path" 2>"$tmp/err" >/dev/null || status=$?
    first=$(head -n 1 "$tmp/err")
    checked=$((checked + 1))
    case "$status:$first" in
      "1:langc: $code: "*) ;;
      *) echo "FAIL $file: want exit 1 and $code, got exit $status: $first"; fail=$((fail + 1)) ;;
    esac
  done <"$1/expect.txt"
  echo "$2 refusals: $checked checked"
}
refusals test/parse parse
refusals test/check check

# A REFUSE_KIT row refuses a kit item in guest mode only: `langc --kit check`
# must print ok.
kit_only=0
while read -r file code; do
  [ "$code" = "REFUSE_KIT" ] || continue
  status=0
  out=$(build/langc --kit check "test/check/$file" 2>&1) || status=$?
  kit_only=$((kit_only + 1))
  if [ "$status:$out" != "0:ok" ]; then
    echo "FAIL $file: want ok under --kit, got exit $status: $out"
    fail=$((fail + 1))
  fi
done <test/check/expect.txt
echo "kit-only refusals: $kit_only checked"

# A refl refusal shows both normal forms.
build/langc $(kit test/check/kit.txt bad-refl.lang) check test/check/bad-refl.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the path at i1 is some 1201, but the type needs some 1200' "$tmp/err"; then
  echo "FAIL bad-refl.lang: the message does not show both normal forms"
  fail=$((fail + 1))
fi

# An affine cofamily type prints as D A in a refusal (S6, O19).
build/langc $(kit test/check/kit.txt allowance-omega.lang) check test/check/allowance-omega.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the domain of the arrow has the resource type Allowance A at omega' "$tmp/err"; then
  echo "FAIL allowance-omega.lang: the message does not print the cofamily type as Allowance A"
  fail=$((fail + 1))
fi

# A REFUSE_TIME refusal names the op-count form (S3, O3).
time_hint=0
build/langc $(kit test/check/kit.txt time-block.lang) check test/check/time-block.lang 2>"$tmp/err" >/dev/null || true
time_hint=$((time_hint + 1))
if ! rg -q -F 'count operations: a deadline is a StepBound (a count of later operations)' "$tmp/err"; then
  echo "FAIL time-block.lang: the message does not name the op-count form"
  fail=$((fail + 1))
fi
echo "time hint: $time_hint checked"

# Each line of test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`. `langc eval`
# must exit 0 and print OUTPUT.
evals=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  got=$(build/langc $(kit examples/kit.txt "$file.lang") eval "examples/$file.lang" "$name" "$@" 2>"$tmp/err") || status=$?
  first=$(head -n 1 "$tmp/err")
  evals=$((evals + 1))
  case "$status|$got|$first" in
    "0|$want|"*) ;;
    *) echo "FAIL eval $file $name: want $want, got exit $status: $got $first"; fail=$((fail + 1)) ;;
  esac
done <test/eval/expect.txt
echo "evals: $evals checked"

# eval_refused STATUS CODE NAME [ARGS]: `langc --kit eval examples/entries.lang
# NAME ARGS` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
eval_refused() {
  want_status=$1
  code=$2
  shift 2
  status=0
  build/langc --kit eval examples/entries.lang "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL eval $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
eval_refused 2 EVAL_ARGS addPrice 1
eval_refused 2 EVAL_ARGS addPrice x 1
eval_refused 2 EVAL_ARGS addPrice 18446744073709551616 1
eval_refused 2 EVAL_ARGS pick 2 4 9
eval_refused 1 EVAL_ENTRY nope

# The target (S3). The Wasm writer is in the build: `langc build
# examples/functions.lang` must exit 0 and write a module that wasm-validate
# accepts.
builds=0
status=0
build/langc $(kit examples/kit.txt functions.lang) build examples/functions.lang -o "$tmp/functions.wasm" 2>"$tmp/err" || status=$?
builds=$((builds + 1))
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/functions.wasm" 2>>"$tmp/err"; then
  echo "FAIL build functions: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
echo "builds: $builds checked"

# The IR (S3): `langc ir examples/functions.lang` must exit 0 and give one
# func line for each line of `langc abi`.
irs=0
status=0
build/langc $(kit examples/kit.txt functions.lang) ir examples/functions.lang >"$tmp/ir.txt" 2>"$tmp/ir.err" || status=$?
irs=$((irs + 1))
funcs=$(rg -c '^func ' "$tmp/ir.txt" || echo 0)
entries=$(build/langc $(kit examples/kit.txt functions.lang) abi examples/functions.lang | rg -c '.' || echo 0)
[ "$status" -eq 0 ] && [ "$funcs" -gt 0 ] && [ "$funcs" -eq "$entries" ] || { echo "FAIL ir functions: exit $status, $funcs funcs for $entries entries: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
echo "ir: $irs lowered"

# The IR refusals (S3): `langc --kit check` must accept each file, and
# `langc --kit ir` must exit 1 and print `langc: ` and the given text first.
# closure.lang gives a function value at run time (REFUSE_RUNTIME_CLOSURE).
# kmap.lang uses a KMap operation at run time (a refuse() arm of lower.c).
# tree.lang folds a tree that exists only at run time (REFUSE_RUNTIME_TREE).
# spine.lang walks lists that exist only at run time, so its heap use has no
# static bound (S3).
# hop.lang has a recursive fold over a block of families. The S8 files:
# reschain.lang has a Resource with no path literal, bindnostart.lang has a
# bind line and no def start, and bindmemory.lang has an entry with the name
# of a bind export (decide, malformed and memory are the bind exports).
irrefusals=0
ir_refused() {
  status=0
  build/langc --kit check "test/ir/$1" > /dev/null 2>&1 || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: check exit $status"; fail=$((fail + 1)); }
  status=0
  first=$(build/langc --kit ir "test/ir/$1" 2>&1 > /dev/null) || status=$?
  case "$status:$first" in
    "1:langc: $2"*) ;;
    *) echo "FAIL $1: want exit 1 and $2, got exit $status: $first"; fail=$((fail + 1)) ;;
  esac
  irrefusals=$((irrefusals + 1))
}
ir_refused closure.lang "REFUSE_RUNTIME_CLOSURE: bad: "
ir_refused kmap.lang "REFUSE_LOWER: got: the Wasm host does not lower an elim, KMap"
ir_refused tree.lang "REFUSE_RUNTIME_TREE: tips: "
ir_refused spine.lang "REFUSE_LOWER: trapHead: heap use has no static bound"
ir_refused hop.lang "REFUSE_LOWER: hopDepth: a recursive fold over a block of families"
ir_refused reschain.lang "REFUSE_LOWER: far: the Wasm host does not lower a Resource with no path literal (S8)"
ir_refused bindnostart.lang "REFUSE_LOWER: read_doc: a bind line needs a def start (S8, Q-S8-5 (a))"
ir_refused bindmemory.lang "REFUSE_LOWER: memory: the entry memory has the name of a bind export (S8)"
echo "ir refusals: $irrefusals checked"

# The IR lowerings (S3): `langc
# --kit check` must accept each file and `langc --kit ir` must exit 0. The
# IR text of natops.lang and of block.lang (S3) and of payload.lang (S8: a
# Payload is a host handle, Q-S8-2 (a)) and of resource.lang (S8: host
# calls, res handles and the res lines) must be test/ir/natops.ir,
# test/ir/block.ir, test/ir/payload.ir and test/ir/resource.ir (golden
# files: a change of the lowering changes the text).
irlowered=0
ir_lowered() {
  status=0
  build/langc --kit check "test/ir/$1" > /dev/null 2>&1 || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: check exit $status"; fail=$((fail + 1)); }
  status=0
  build/langc --kit ir "test/ir/$1" >"$tmp/$1.ir" 2>"$tmp/ir.err" || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: want ir exit 0, got exit $status: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
  irlowered=$((irlowered + 1))
}
ir_lowered natops.lang
cmp -s "$tmp/natops.lang.ir" test/ir/natops.ir || { echo "FAIL natops.lang: the IR text is not test/ir/natops.ir"; fail=$((fail + 1)); }
ir_lowered block.lang
cmp -s "$tmp/block.lang.ir" test/ir/block.ir || { echo "FAIL block.lang: the IR text is not test/ir/block.ir"; fail=$((fail + 1)); }
ir_lowered payload.lang
cmp -s "$tmp/payload.lang.ir" test/ir/payload.ir || { echo "FAIL payload.lang: the IR text is not test/ir/payload.ir"; fail=$((fail + 1)); }
ir_lowered resource.lang
cmp -s "$tmp/resource.lang.ir" test/ir/resource.ir || { echo "FAIL resource.lang: the IR text is not test/ir/resource.ir"; fail=$((fail + 1)); }
# S8 (Q-S8-9 (a)): Size is a Nat word, an elim over Size is a REPEAT, an
# elim over a family of plain constructors is a SWITCH on word 0, and len
# over a closed trace or a stuck flagIf is a count.
ir_lowered sizestep.lang
cmp -s "$tmp/sizestep.lang.ir" test/ir/sizestep.ir || { echo "FAIL sizestep.lang: the IR text is not test/ir/sizestep.ir"; fail=$((fail + 1)); }
ir_lowered judgesteps.lang
cmp -s "$tmp/judgesteps.lang.ir" test/ir/judgesteps.ir || { echo "FAIL judgesteps.lang: the IR text is not test/ir/judgesteps.ir"; fail=$((fail + 1)); }
ir_lowered actcase.lang
cmp -s "$tmp/actcase.lang.ir" test/ir/actcase.ir || { echo "FAIL actcase.lang: the IR text is not test/ir/actcase.ir"; fail=$((fail + 1)); }
ir_lowered decision.lang
cmp -s "$tmp/decision.lang.ir" test/ir/decision.ir || { echo "FAIL decision.lang: the IR text is not test/ir/decision.ir"; fail=$((fail + 1)); }
ir_lowered fmlen.lang
cmp -s "$tmp/fmlen.lang.ir" test/ir/fmlen.ir || { echo "FAIL fmlen.lang: the IR text is not test/ir/fmlen.ir"; fail=$((fail + 1)); }
ir_lowered judgeaction.lang
cmp -s "$tmp/judgeaction.lang.ir" test/ir/judgeaction.ir || { echo "FAIL judgeaction.lang: the IR text is not test/ir/judgeaction.ir"; fail=$((fail + 1)); }
echo "ir lowerings: $irlowered lowered"

# The writer (S3): `langc --kit build
# test/ir/natops.lang` must exit 0 and write a module that wasm-validate
# accepts. In `wasm-objdump -x`, memory 0 must have 1 page (minimum =
# maximum = the pages of the largest heap bound, at least 1). The module of
# examples/functions.lang (the builds rows) has 0 heap words: its memory 0
# must also have 1 page. The Wasm runs and the differential lines (S8,
# below) run each module on node and on wasmtime (Q-S8-6 (a)).

status=0
build/langc --kit build test/ir/natops.lang -o "$tmp/natops.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/natops.wasm" 2>>"$tmp/err"; then
  echo "FAIL build natops: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
# The module of test/ir/block.lang (S3): the same build
# check. The notrap row below uses it.
status=0
build/langc --kit build test/ir/block.lang -o "$tmp/block.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/block.wasm" 2>>"$tmp/err"; then
  echo "FAIL build block: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
# The module of test/ir/resource.lang (S8, Q-S8-2 (a)): the same build
# check. Its entries call the host imports of module permit; the notrap row
# below uses it.
status=0
build/langc --kit build test/ir/resource.lang -o "$tmp/resource.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/resource.wasm" 2>>"$tmp/err"; then
  echo "FAIL build resource: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
# The custom sections (S8, Q-S8-3 (a), Q-S8-4 (a)). After the code section,
# permit.res holds the path names in program order and permit.bind the bind
# rows (tool name, Action arm, pointer); the IR text shows the same tables
# as res and bind lines. examples/gate.lang must build to a module that
# wasm-validate and build/notraptool accept. In `wasm-objdump -h`, the
# custom sections of resource.wasm and gate.wasm (the size and the name)
# must be the lines of test/ir/NAME.sections, and functions.wasm (no path
# literal, no bind line) must have none.
customs=0
status=0
build/langc $(kit examples/kit.txt gate.lang) build examples/gate.lang -o "$tmp/gate.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/gate.wasm" 2>>"$tmp/err" || ! build/notraptool "$tmp/gate.wasm" >/dev/null 2>>"$tmp/err"; then
  echo "FAIL build gate: want exit 0, a valid module and notraptool rc 0, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
for mod in resource gate; do
  customs=$((customs + 1))
  wasm-objdump -h "$tmp/$mod.wasm" 2>/dev/null | rg -o '\(size=0x[0-9a-f]+\) "permit\.[a-z]+"' | cmp -s - "test/ir/$mod.sections" ||
    { echo "FAIL custom sections $mod: want the lines of test/ir/$mod.sections"; fail=$((fail + 1)); }
done
customs=$((customs + 1))
if wasm-objdump -h "$tmp/functions.wasm" 2>/dev/null | rg -q 'Custom'; then
  echo "FAIL custom sections functions: want no custom section"
  fail=$((fail + 1))
fi
echo "custom sections: $customs checked"
# The module of examples/state-no-history.lang (Q-S3-3 (b)): build lowers a
# state family as a plain family. The same build check; the notrap row
# below uses it, and the differential row (S8) runs it.
status=0
build/langc $(kit examples/kit.txt state-no-history.lang) build examples/state-no-history.lang -o "$tmp/state-no-history.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/state-no-history.wasm" 2>>"$tmp/err"; then
  echo "FAIL build state-no-history: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
for mod in natops functions; do
  wasm-objdump -x "$tmp/$mod.wasm" 2>/dev/null | rg -q -x -F ' - memory[0] pages: initial=1 max=1' ||
    { echo "FAIL $mod.wasm: memory 0 is not initial=1 max=1"; fail=$((fail + 1)); }
done
# build/notraptool (test/notraptool.c, S3) must accept the
# modules of natops.lang, block.lang, functions.lang,
# state-no-history.lang and resource.lang: exit 0 and no output. It fails a module with unreachable, memory.grow, an opcode that
# is not in src/wasmop.h, an i64.div_u or i64.rem_u with no divisor guard,
# or a memory with no maximum or a maximum that is not the minimum.
# It also fails an import that is not a function of module permit with
# its type (S8): envelope (i64 i64) -> i64, hash (i64) -> i64, within
# (i64 i64) -> i64.
notrapmods=0
for mod in natops block functions state-no-history resource; do
  notrapmods=$((notrapmods + 1))
  status=0
  got=$(build/notraptool "$tmp/$mod.wasm" 2>&1) || status=$?
  [ "$status" -eq 0 ] && [ -z "$got" ] ||
    { echo "FAIL notrap $mod.wasm: want exit 0 and no output, got exit $status: $got"; fail=$((fail + 1)); }
done
echo "notrap modules: $notrapmods checked"
# The bad rows (S3): each line of test/notrap/bad/expect.txt is
# `NAME MESSAGE`. wat2wasm makes $tmp/NAME.wasm from test/notrap/bad/NAME.wat.
# build/notraptool must exit 1 and print the one line
# `notraptool: $tmp/NAME.wasm: MESSAGE`.
notrapbad=0
while read -r name msg; do
  notrapbad=$((notrapbad + 1))
  status=0
  got=$(wat2wasm "test/notrap/bad/$name.wat" -o "$tmp/$name.wasm" 2>&1 && build/notraptool "$tmp/$name.wasm" 2>&1) || status=$?
  [ "$status" -eq 1 ] && [ "$got" = "notraptool: $tmp/$name.wasm: $msg" ] ||
    { echo "FAIL notrap bad $name: want exit 1 and $msg, got exit $status: $got"; fail=$((fail + 1)); }
done <test/notrap/bad/expect.txt
echo "notrap bad rows: $notrapbad checked"
# The kit evals of test/ir/spine.lang at 4. Each line is `NAME VALUE`.
spineevals=0
while read -r name want; do
  got=$(build/langc --kit eval test/ir/spine.lang "$name" 4 2>&1) || got="exit $?: $got"
  spineevals=$((spineevals + 1))
  [ "$got" = "$want" ] || { echo "FAIL spine $name 4: want $want, got $got"; fail=$((fail + 1)); }
done <<'EOF'
foldOnes 8
mapUp 20
bindUp 50
filterUp 3
trapHead 1
trapSum 3
filterTrap 7
bindTrap 1
sizeSum 20
sizeTrap 4
EOF
echo "spine evals: $spineevals checked"
# The kit evals of test/ir/natops.lang (S5, O15): reasonCode is a case
# split over the domain family Reason. Each line is `NAME ARG VALUE`.
natopsevals=0
while read -r name arg want; do
  got=$(build/langc --kit eval test/ir/natops.lang "$name" "$arg" 2>&1) || got="exit $?: $got"
  natopsevals=$((natopsevals + 1))
  [ "$got" = "$want" ] || { echo "FAIL natops $name $arg: want $want, got $got"; fail=$((fail + 1)); }
done <<'ROWS'
reasonCode 0 1
reasonCode 1 2
ROWS
echo "natops evals: $natopsevals checked"
# The Wasm runs (S8 U3, Q-S8-11 (b)): calls of natops.wasm. Each call must
# give on node and on wasmtime (test/run-wasm.mjs --wasmtime) the line of
# `langc --kit eval` (`exit N: MESSAGE` when eval fails). flagPick 2 4 9 (a
# Flag word that is not 0 or 1) must give 4. A Wasm `trap` is a mismatch.
wasmruns=0
cat >"$tmp/calls" <<'CALLS'
addOr 2 3
addOr 18446744073709551615 1
addOr 18446744073709551615 0
divBy 7 2
divBy 7 0
modBy 7 3
modBy 7 0
unwrap 0
unwrap 5
pickSecond 2 3
pickSecond 18446744073709551615 1
reasonCode 0
reasonCode 1
flagPick 0 4 9
flagPick 1 4 9
CALLS
while read -r call; do
  got=$(build/langc --kit eval test/ir/natops.lang $call 2>&1) || got="exit $?: $got"
  echo "$got"
done <"$tmp/calls" >"$tmp/eval.out"
echo "flagPick 2 4 9" >>"$tmp/calls"
echo 4 >>"$tmp/eval.out"
for run in node wasmtime; do
  flag=""
  [ "$run" = node ] || flag=--wasmtime
  status=0
  node test/run-wasm.mjs $flag "$tmp/natops.wasm" - <"$tmp/calls" >"$tmp/$run.out" 2>"$tmp/err" || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL run natops.wasm: $run exit $status: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)); }
done
paste -d '|' "$tmp/calls" "$tmp/eval.out" "$tmp/node.out" "$tmp/wasmtime.out" >"$tmp/runs"
while IFS='|' read -r call want got gotw; do
  wasmruns=$((wasmruns + 1))
  [ "$got" = "$want" ] && [ "$gotw" = "$want" ] ||
    { echo "FAIL wasm $call: want $want, got $got (node), $gotw (wasmtime)"; fail=$((fail + 1)); }
done <"$tmp/runs"
echo "wasm runs: $wasmruns checked"
# The differential (S8 U3): test/grid.awk makes the cases from the `langc
# --kit ir` text of each module below (the modules of the build rows, and the
# S8 lowerings). Each case must give on node and on wasmtime the line of
# `langc --kit eval` (eval-exit-N when eval fails). eval has no trap, so a
# Wasm `trap` is a mismatch. The compare is a string compare: awk compares
# two numbers as doubles, and 2^64 - 1 and 2^64 - 2 give the same double.
diffcases=0
mismatches=0
for f in test/ir/natops.lang test/ir/block.lang examples/functions.lang examples/state-no-history.lang test/ir/sizestep.lang test/ir/judgesteps.lang test/ir/actcase.lang test/ir/decision.lang test/ir/fmlen.lang test/ir/judgeaction.lang; do
  mod=$(basename "$f" .lang)
  [ -f "$tmp/$mod.wasm" ] || build/langc --kit build "$f" -o "$tmp/$mod.wasm" 2>"$tmp/err" ||
    { echo "FAIL differential $mod: build: $(head -n 1 "$tmp/err")"; fail=$((fail + 1)); continue; }
  build/langc --kit ir "$f" | awk -f test/grid.awk >"$tmp/cases.txt"
  while read -r call; do
    status=0
    want=$(build/langc --kit eval "$f" $call 2>/dev/null) || status=$?
    [ "$status" -eq 0 ] || want="eval-exit-$status"
    echo "$want"
  done <"$tmp/cases.txt" >"$tmp/want.txt"
  status=0
  node test/run-wasm.mjs "$tmp/$mod.wasm" - <"$tmp/cases.txt" >"$tmp/got.txt" 2>"$tmp/err" &&
    node test/run-wasm.mjs --wasmtime "$tmp/$mod.wasm" - <"$tmp/cases.txt" >"$tmp/gotw.txt" 2>>"$tmp/err" || status=$?
  if [ "$status" -ne 0 ]; then
    echo "FAIL differential $mod.wasm: run exit $status: $(head -n 1 "$tmp/err")"
    fail=$((fail + 1))
    continue
  fi
  diffcases=$((diffcases + $(rg -c '.' "$tmp/cases.txt" || echo 0)))
  paste -d '|' "$tmp/cases.txt" "$tmp/want.txt" "$tmp/got.txt" "$tmp/gotw.txt" |
    awk -F '|' '($2 "") != ($3 "") || ($2 "") != ($4 "")' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  mismatches=$((mismatches + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL differential $mod.wasm: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
done
echo "differential cases: $diffcases checked, $mismatches mismatches"
# The decide scripts (S8 U3, Q-S8-10 (a), Q-S8-12 (a)): each line of
# test/decide/DN.txt is one call on one instance of gate.wasm. node and
# wasmtime (test/run-wasm.mjs --decide) print the codes, then `steps N` and
# `left N` (words 0 and 1 of the memory export). The oracle is `langc --kit
# eval` of the judge chain that test/run-wasm.mjs --lang prints, on a copy of
# examples/gate.lang: permit is 0, deny R is the place of R in family Reason
# (domain/domain.lang) plus 1, and steps is the szS count.
decidescripts=0
decidecalls=0
decidebad=0
# The canonical layout puts each constructor after the first on its own line
# `  | c` (S12): join the lines of family Reason with ` | `.
reasons=$(awk '/^family Reason := /{p=1; r=substr($0, 18); next} p && /^  [|] /{r=r " | " substr($0, 5); next} {p=0} END {print r}' domain/domain.lang)
for s in test/decide/D*.txt; do
  d=$(basename "$s" .txt)
  decidescripts=$((decidescripts + 1))
  decidecalls=$((decidecalls + $(rg -c '.' "$s" || echo 0)))
  cp examples/gate.lang "$tmp/$d.lang"
  status=0
  node test/run-wasm.mjs --lang "$s" >>"$tmp/$d.lang" 2>"$tmp/err" &&
    node test/run-wasm.mjs --decide "$s" "$tmp/gate.wasm" >"$tmp/$d.node" 2>>"$tmp/err" &&
    node test/run-wasm.mjs --wasmtime --decide "$s" "$tmp/gate.wasm" >"$tmp/$d.wasmtime" 2>>"$tmp/err" || status=$?
  if [ "$status" -ne 0 ]; then
    echo "FAIL decide $d: run exit $status: $(head -n 1 "$tmp/err")"
    fail=$((fail + 1))
    continue
  fi
  for name in $(rg -o -r '$1' '^def (c[0-9]+|steps|left) :' "$tmp/$d.lang"); do
    got=$(build/langc --kit eval "$tmp/$d.lang" "$name" 2>&1) || got="eval-exit-$?"
    echo "$name $got"
  done | awk -v r="$reasons" 'BEGIN { n = split(r, w, / [|] /); for (i = 1; i <= n; i++) code[w[i]] = i }
    $1 == "steps" { print "steps " gsub(/szS/, ""); next }
    $1 == "left" { print "left " $2; next }
    $2 == "permit" && NF == 2 { print 0; next }
    $2 == "deny" && NF == 3 && ($3 in code) { print code[$3]; next }
    { print "eval " $0 }' >"$tmp/$d.eval"
  bad=$(paste -d '|' "$tmp/$d.eval" "$tmp/$d.node" "$tmp/$d.wasmtime" | awk -F '|' '($1 "") != ($2 "") || ($1 "") != ($3 "")' | rg -c '.' || echo 0)
  decidebad=$((decidebad + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL decide $d: $bad mismatches: eval $(paste -s -d ' ' "$tmp/$d.eval"), node $(paste -s -d ' ' "$tmp/$d.node"), wasmtime $(paste -s -d ' ' "$tmp/$d.wasmtime")"; fail=$((fail + 1)); }
done
echo "decide scripts: $decidescripts checked, $decidecalls calls, $decidebad mismatches"
# The engine rows (S9 U1, Q-S9-1 (a), L8): build/enginetool runs each
# test/decide/DN.txt on gate.wasm in the own C99 engine (host/engine.c) with
# the O5 binding and prints the lines of `test/run-wasm.mjs --decide`. Each
# line must be the node line of the decide block above.
enginescripts=0
enginecalls=0
enginebad=0
for s in test/decide/D*.txt; do
  d=$(basename "$s" .txt)
  enginescripts=$((enginescripts + 1))
  enginecalls=$((enginecalls + $(rg -c '.' "$s" || echo 0)))
  if [ ! -f "$tmp/$d.node" ]; then
    echo "FAIL engine $d: no node lines"
    fail=$((fail + 1))
    continue
  fi
  status=0
  build/enginetool "$s" "$tmp/gate.wasm" >"$tmp/$d.engine" 2>"$tmp/err" || status=$?
  if [ "$status" -ne 0 ]; then
    echo "FAIL engine $d: run exit $status: $(head -n 1 "$tmp/err")"
    fail=$((fail + 1))
    continue
  fi
  bad=$(paste -d '|' "$tmp/$d.node" "$tmp/$d.engine" | awk -F '|' '($1 "") != ($2 "")' | rg -c '.' || echo 0)
  enginebad=$((enginebad + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL engine $d: $bad mismatches: node $(paste -s -d ' ' "$tmp/$d.node"), engine $(paste -s -d ' ' "$tmp/$d.engine")"; fail=$((fail + 1)); }
done
echo "engine decide scripts: $enginescripts checked, $enginecalls calls, $enginebad mismatches"
# The engine load refusals (S9 U1): each line below is `NAME MESSAGE`.
# wat2wasm makes $tmp/engine-NAME.wasm from test/engine/NAME.wat.
# build/enginetool must exit 1 and print the one line
# `enginetool: load: MESSAGE` (an import with no O5 row, an import with the
# wrong type).
enginerefusals=0
while read -r name msg; do
  enginerefusals=$((enginerefusals + 1))
  status=0
  got=$(wat2wasm "test/engine/$name.wat" -o "$tmp/engine-$name.wasm" 2>&1 && build/enginetool test/decide/D1.txt "$tmp/engine-$name.wasm" 2>&1) || status=$?
  [ "$status" -eq 1 ] && [ "$got" = "enginetool: load: $msg" ] ||
    { echo "FAIL engine refusal $name: want exit 1 and $msg, got exit $status: $got"; fail=$((fail + 1)); }
done <<'EOF'
unbound unbound import
badtype import type
EOF
echo "engine refusals: $enginerefusals checked"
# The O5 hash stub against fixed numbers (S9 U2, the U1 mutant O1): each
# line below is `N WANT`, WANT from the test/run-wasm.mjs `hash` formula
# (N * 1000003 + 7) mod 2^64. The second row wraps at 2^64.
enginehash=0
while read -r n want; do
  enginehash=$((enginehash + 1))
  status=0
  got=$(echo "hash $n" >"$tmp/hash.txt" && build/enginetool "$tmp/hash.txt" "$tmp/gate.wasm" 2>&1 | head -n 1) || status=$?
  [ "$status" -eq 0 ] && [ "$got" = "$want" ] ||
    { echo "FAIL engine hash $n: want $want, got exit $status: $got"; fail=$((fail + 1)); }
done <<'EOF'
0 7
9223372036854775809 9223372036855775818
EOF
echo "engine hash rows: $enginehash checked"
# SHA-256 (S9 U1, Q-S9-2 (a)): build/shatool checks host/sha256.c on the 4
# FIPS 180-4 vectors and 3 pad rows (55, 56 and 64 bytes of "a").
status=0
build/shatool >"$tmp/sha.txt" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ]; then
  echo "FAIL sha256: exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
else
  cat "$tmp/sha.txt"
fi
# JSON, JCS, the envelope and the pointer (S9 U2, Q-S9-3 (a)): build/jcstool
# checks host/jsonin.c and host/jcs.c on its rows (RFC 8785 key order and
# escapes, the 2^53 - 1 bound, -0, a duplicate key, RFC 6901 escapes).
status=0
build/jcstool >"$tmp/jcs.txt" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ]; then
  echo "FAIL jcs: exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
else
  cat "$tmp/jcs.txt"
fi
# The authority report (S10 U1, Q-S10-3 (a)): test/report/NAME.json is the
# `langc report` line of test/report/NAME.lang (guest mode, no --kit). Each
# run exits 0 with no stderr, and the line equals NAME.json byte for byte.
reports=0
for want in $(printf '%s\n' test/report/*.json | LC_ALL=C sort); do
  reports=$((reports + 1))
  name=$(basename "$want" .json)
  status=0
  build/langc report "test/report/$name.lang" >"$tmp/r.out" 2>"$tmp/r.err" || status=$?
  if [ "$status" -ne 0 ] || [ -s "$tmp/r.err" ]; then
    echo "FAIL report $name: exit $status $(head -n 1 "$tmp/r.err" | cut -c1-120)"
    fail=$((fail + 1))
  elif ! cmp -s "$want" "$tmp/r.out"; then
    echo "FAIL report $name: the line is not $want: $(cmp "$want" "$tmp/r.out" 2>&1 | cut -c1-120)"
    fail=$((fail + 1))
  fi
done
echo "report rows: $reports checked"
# The Cedar export (S11 U2, Q-S11-2 and Q-S11-3 (a)): test/cedar/NAME.cedar
# is the `langc cedar` text of test/report/NAME.lang (one program, two
# targets). Each run exits 0 with no stderr, and the text equals NAME.cedar
# byte for byte. Each report row has a cedar row.
cedars=0
for want in $(printf '%s\n' test/cedar/*.cedar | LC_ALL=C sort); do
  cedars=$((cedars + 1))
  name=$(basename "$want" .cedar)
  status=0
  build/langc cedar "test/report/$name.lang" >"$tmp/c.out" 2>"$tmp/c.err" || status=$?
  if [ "$status" -ne 0 ] || [ -s "$tmp/c.err" ]; then
    echo "FAIL cedar $name: exit $status $(head -n 1 "$tmp/c.err" | cut -c1-120)"
    fail=$((fail + 1))
  elif ! cmp -s "$want" "$tmp/c.out"; then
    echo "FAIL cedar $name: the text is not $want: $(cmp "$want" "$tmp/c.out" 2>&1 | cut -c1-120)"
    fail=$((fail + 1))
  fi
done
echo "cedar rows: $cedars checked"
if [ "$cedars" -ne "$reports" ]; then
  echo "FAIL cedar: $cedars cedar rows for $reports report rows"
  fail=$((fail + 1))
fi
if ! build/reporttool build/langc test/report/unit.lang; then
  fail=$((fail + 1))
fi

# The MCP gate (S9 U3, N6, docs/MCP.md): build/hosttool runs build/permitd
# on test/host/deploy.lang with build/fakedeploy as the child, one line at
# a time (malformed lines, toolFailed, the state file, the forwarded bytes,
# the cost and the approvals file).
status=0
build/langc build test/host/deploy.lang -o "$tmp/deploy.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ]; then
  echo "FAIL host: langc exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
else
  mkdir -p "$tmp/host"
  build/hosttool build/permitd build/fakedeploy "$tmp/deploy.wasm" "$tmp/host" >"$tmp/host.txt" 2>"$tmp/err" || status=$?
  if [ "$status" -ne 0 ]; then
    echo "FAIL host: exit $status: $(head -n 1 "$tmp/err")"
    fail=$((fail + 1))
  else
    cat "$tmp/host.txt"
  fi
fi

# fmt (S12, ported from accrue-lang S11). Each repo program that `langc fmt`
# accepts is canonical (fmt F equals F byte for byte) and keeps its AST (the
# scoped fronttool dump of F equals the dump of fmt F). fmt refuses exactly
# the files in test/fmt/refused.txt. Each test/fmt/X.lang (not the witness
# files piname.lang and mark.lang) is not canonical by design: the canonical
# line skips it, it formats to X.canon, and `langc fmt --check` accepts each
# X.canon and refuses test/fmt/check.lang at line 3. The witness rows:
# test/fmt/piname.lang is test/fmt/lambda.canon with one Pi binder name
# changed, test/fmt/mark.lang is test/fmt/marks.canon with one mark changed,
# and each dump must not equal the dump of its canon file. The capture row
# shows that the H1 group `(x y : x)` stays one group in
# test/fmt/capture.canon. It does not claim a capture: the split text has
# the same meaning and the same dump (Q-S12-7).
same_dump() {
  build/fronttool dump "$1" >"$tmp/d1.txt" 2>&1 && build/fronttool dump "$2" >"$tmp/d2.txt" 2>&1 &&
    cmp -s "$tmp/d1.txt" "$tmp/d2.txt"
}
differ_dump() {
  build/fronttool dump "$1" >"$tmp/d1.txt" 2>&1 && build/fronttool dump "$2" >"$tmp/d2.txt" 2>&1 &&
    ! cmp -s "$tmp/d1.txt" "$tmp/d2.txt"
}
canon=0
trips=0
refused=0
for f in domain/domain.lang examples/*.lang examples/*/*.lang test/*/*.lang; do
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  listed=0
  rg -qxF -- "$f" test/fmt/refused.txt && listed=1
  if [ "$status" -ne 0 ]; then
    refused=$((refused + 1))
    [ "$listed" -eq 1 ] || { echo "FAIL fmt $f: refused, exit $status: $(head -n 1 "$tmp/fmt.err")"; fail=$((fail + 1)); }
    continue
  fi
  [ "$listed" -eq 0 ] || { echo "FAIL fmt $f: accepted, but test/fmt/refused.txt lists it"; fail=$((fail + 1)); }
  case $f in
    test/fmt/*) ;;
    *)
      if cmp -s "$f" "$tmp/fmt.out"; then
        canon=$((canon + 1))
      else
        echo "FAIL fmt $f: not canonical"
        fail=$((fail + 1))
      fi
      ;;
  esac
  if same_dump "$f" "$tmp/fmt.out"; then
    trips=$((trips + 1))
  else
    echo "FAIL fmt round trip $f: the AST dump changes after fmt"
    fail=$((fail + 1))
  fi
done
want=$(($(wc -l <test/fmt/refused.txt)))
[ "$refused" -eq "$want" ] || { echo "FAIL fmt: $refused files refused, test/fmt/refused.txt lists $want"; fail=$((fail + 1)); }
echo "fmt canonical: $canon passed"
echo "fmt round trip: $trips passed"
echo "fmt refused: $refused checked"
rows=0
checks=0
for f in test/fmt/*.lang; do
  case $f in test/fmt/piname.lang | test/fmt/mark.lang) continue ;; esac
  c=${f%.lang}.canon
  status=0
  build/langc fmt "$f" >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -eq 0 ] && cmp -s "$c" "$tmp/fmt.out"; then
    rows=$((rows + 1))
  else
    echo "FAIL fmt row $f: the output is not $c, exit $status"
    fail=$((fail + 1))
  fi
  status=0
  build/langc fmt "$c" --check >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
  if [ "$status" -eq 0 ] && [ ! -s "$tmp/fmt.out" ] && [ ! -s "$tmp/fmt.err" ]; then
    checks=$((checks + 1))
  else
    echo "FAIL fmt --check $c: want exit 0 and no output, got exit $status"
    fail=$((fail + 1))
  fi
done
printf 'test/fmt/check.lang:3: not canonical\n' >"$tmp/want.err"
status=0
build/langc fmt test/fmt/check.lang --check >"$tmp/fmt.out" 2>"$tmp/fmt.err" || status=$?
if [ "$status" -eq 1 ] && [ ! -s "$tmp/fmt.out" ] && cmp -s "$tmp/want.err" "$tmp/fmt.err"; then
  checks=$((checks + 1))
else
  echo "FAIL fmt --check test/fmt/check.lang: want exit 1 and line 3, got exit $status: $(head -n 1 "$tmp/fmt.err")"
  fail=$((fail + 1))
fi
if differ_dump test/fmt/lambda.canon test/fmt/piname.lang; then
  rows=$((rows + 1))
else
  echo "FAIL fmt witness: the dump of test/fmt/piname.lang equals the dump of test/fmt/lambda.canon (or a dump fails)"
  fail=$((fail + 1))
fi
if differ_dump test/fmt/marks.canon test/fmt/mark.lang; then
  rows=$((rows + 1))
else
  echo "FAIL fmt witness: the dump of test/fmt/mark.lang equals the dump of test/fmt/marks.canon (or a dump fails)"
  fail=$((fail + 1))
fi
if rg -qF '(x y : x)' test/fmt/capture.canon; then
  rows=$((rows + 1))
else
  echo "FAIL fmt witness: test/fmt/capture.canon does not keep the group (x y : x)"
  fail=$((fail + 1))
fi
echo "fmt rows: $rows passed"
echo "fmt check: $checks passed"

status=0
build/langc 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc --kit 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage --kit: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc check "$tmp/missing.lang" 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL missing file: want exit 2, got $status"; fail=$((fail + 1)); }
rm -rf "$tmp"

# design/DESIGN.md is verbatim and can hold a dash (as leaf-lang and bridge-lang).
if rg -n -g '!design/DESIGN.md' '\x{2013}|\x{2014}' . >/dev/null; then
  echo "FAIL: an em-dash or en-dash is in the kit"
  fail=$((fail + 1))
fi
echo "gate: $fail failures"
[ "$fail" -eq 0 ]
