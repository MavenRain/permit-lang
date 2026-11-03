# TinyCC Wasm host kit

This kit compiles a small typed language to a WebAssembly MVP module.
One C99 program, `build/langc`, holds the parser, the checker, the
evaluator, the lowering to a first-order IR and the Wasm writer.
TinyCC builds it.

## Files

These files make the language and its Wasm target:

- the front end: `src/front/`, `src/ir.h`, `src/target.h` and
  `src/main.c`
- the target files: `src/wasm.c` and its opcode list `src/wasmop.h`
- the domain and its embed tool: `domain/` and `gen/`
- the examples: `examples/`
- the tests: `test/parse/`, `test/check/`, `test/eval/`, `test/ir/`,
  `test/notrap/`, `test/review.sh`, `test/fronttool.c`,
  `test/pathtool.c` and `test/notraptool.c`
- the differential files: `test/grid.awk` and `test/run-wasm.mjs`. They
  are out of the gate until S8 (see "Gate").
- the build and gate files: `Makefile`, `test/gate.sh` and `PIN`
- the host docs: `docs/host/README.md`, `docs/host/CAPABILITY.md` and
  `formers/tcc-wasm.md`

## Build

From the root of this repository:

```sh
make build
make check
make check-clang
```

`build` runs `tcc -run gen/embed.c` to copy `domain/domain.lang` into
`build/domain.c`. Then it builds `build/langc` with
`tcc -std=c99 -Wall -Werror`. The domain is part of the executable.
`check-clang` compiles the same sources with
`cc -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only`.
`check` does both steps, then runs `test/gate.sh`.
The gate needs `wasm-validate`, `wasm-objdump` and `wat2wasm` on the
`PATH`. It runs no `node` until S8.
`PIN` records the tool versions of the last GREEN gate.
The build and the gate need no network access.

## Source language

A program is a sequence of `def NAME : TYPE := TERM`. A line comment
starts with `--`. Lambdas are `fun (x : A) => t`. There is no text type.

In kit mode, names include types `Nat Flag Prod Sum Option List Path PathP I`, values
`flagYes flagNo pair first second inl inr either none some option nil cons
pure map bind fold unfold filter pack witness payload refl sym trans
transport cong`, Nat operations `natAdd natSub natDiv natMod natEq natLe`,
the divisor type `NonZero Nat` with `nonZero` and `nonZeroValue`, and the
Flag eliminator `flagIf b yes no`. `Type 0 : Type 1` is the universe
chain. `Sigma (x : A) B` and `(x : A) * B` are dependent pairs.
`FORMERS.md` gives the status of each former.

The affine layer (S4, `docs/PLAN.md` section 6) adds these forms. A
binder can have the mark `@0` or `@1` (its multiplicity). `A -o B` is a
function that uses its argument one time. `unpack p (fun a b => t)` uses
the pair `p` one time and binds `a` and `b` at 1. `Budget : Type 0` and
`Approval : Type 0` are kernel sorts with no constructor, so a program
cannot make one. A use that the multiplicity does not allow gives
`REFUSE_AFFINE_REUSE`, `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` or
`REFUSE_AFFINE_PATH`.

S5 (`docs/PLAN.md` sections 4 and 5) adds these forms. `Resource` is a
kernel sort, copyable. `resEq` and `within : Resource -> Resource ->
Flag` test two values. The introductions `resRoot : Nat -> Resource`
and `resChild : Resource -> Nat -> Resource` are kit-only. The domain
keyword `affine` before `cofamily` makes the cofamily a resource type: a
value of `Allowance A` is used at most one time. `unpack p (fun a b =>
t)` also opens a `Prod` pair `p` one time. The domain gives `Action`,
`Reason`, `Decision`, `StepBound`, `Call`, `Scope` and `Allowance` with
`alOpen`. A guest position closure cannot capture a resource or an
unknown payload: `alOpen` can observe more than one position. Such a
capture gives `REFUSE_AFFINE_OMEGA`. Copyable captures and linear
continuations that move an Allowance are allowed.

S6 (`docs/PLAN.md` sections 4, 6 and 7) adds the guest ops `spend :
Budget -o Nat -> Option Budget` and `split : Budget -o Nat -> Option
(Prod Budget Budget)`, and the kit-only `budget : Nat -> Budget` and
`budgetLeft : Budget -o Nat`. `split b n` gives the `n` units first and
the rest second. The domain gives `Capability`, `sizeLe` and
`delegate`; `delegate` gives (child, rest). A `Capability` holds a
`Budget`, so it is a resource type.

The compiler enforces these rules:

| Rule | Code | The compiler refuses |
|---|---|---|
| R1 | `REFUSE_DATA` | a `family` declaration in a program |
| R2 | `REFUSE_AXIOM` | an `axiom` declaration |
| R3 | `REFUSE_REC` | `def rec`, or a definition that refers to itself or to a later definition |
| R4 | `REFUSE_NAME` | a core name, a domain name, a reserved name or a repeated name as a new definition |
| R4 | `REFUSE_TIME` | one of 10 time words (`now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `date`, `ttl`, `deadline`, `expires`) as a name or a use; the message says to count operations: a deadline is a `StepBound` (a count of later operations) |
| L1 | `REFUSE_DELEGATE_WIDEN` | a closed `delegate` application whose child scope is not within the parent scope, at a term level of 1 |
| L2 | `REFUSE_BUDGET_OVERSPEND` | a closed `spend`, `split` or `delegate` application whose budget part reduces to `none`, at a term level of 1 |

## CLI

```sh
build/langc check PROG
build/langc eval PROG NAME [ARGS...]
build/langc ir PROG
build/langc abi PROG
build/langc build PROG -o OUT [--runtime]
```

- `check` prints `ok`, or refuses the program.
- `eval` prints the normal form of `NAME`. Each argument is a decimal
  number. An entry has no trap (see "No traps").
- `ir` prints the first-order IR. It has one `func` for each entry.
- `abi` prints the name of each entry, one for each line.
- `build` writes the Wasm module to `OUT`. On this target, `--runtime`
  gives the same bytes.

The exit status is 0 for success, 1 for a refused program and 2 for a
usage or IO error. A diagnostic on stderr has the form
`langc: CODE: DEF: message`.

## Entries

An entry is a top-level program definition whose declared type is `Nat`,
`Flag`, or a first-order function from `Nat` and `Flag` arguments to a
`Nat` or `Flag` result. `build` exports each entry by its name. Each
parameter and the result is an `i64`. Other definitions are not exported.

`Nat` is an unsigned 64-bit integer. `natAdd` gives an `Option Nat`:
`none` on an overflow past 2^64 - 1. `natSub` stops at 0. The divisor of
`natDiv` and `natMod` is a `NonZero Nat`. `Flag` is 0 or 1.

## Compilation

Compilation is partial evaluation. The compiler normalizes the body of
each entry with the parameters as free variables. Types and universes erase.
Proof contents erase. A lambda stays only as the direct function argument of
`fold`, `unfold`, `map`, `bind`, `filter`, `either` or `option`. The
lowering puts its body into a loop or a branch. The lowering refuses two
programs with a HOST-LIMIT code (see `docs/CAPABILITY.md`):

- `REFUSE_RUNTIME_CLOSURE`: a function value must exist at run time.
- `REFUSE_RUNTIME_TREE`: a fold walks a tree that exists only at run time.

## No traps

No entry can trap (R0, `docs/PLAN.md` section 2). Each op of the IR is
total, and a value is one word with no trap flag (N5). `natAdd` gives
`none` on an overflow (N1). The divisor of `natDiv` and `natMod` is a
`NonZero Nat`, so the checker refuses a literal 0 with `REFUSE_PARTIAL`.
`langc eval` and the Wasm module give the same value for each entry.

The Wasm writer emits no trap form (N4):

- No `unreachable`. A `SWITCH` on a constructor tag uses its last arm as
  the `br_table` default.
- No `memory.grow`. The memory minimum is equal to the maximum: the
  pages of the largest static heap bound of an entry (N3).
- Each `div_u` and `rem_u` has a guard on its divisor.

`test/notraptool.c` reads each built module and refuses each of these
forms. It also refuses an import that is not one of the `permit` imports
with its type (S8). The differential row runs each case on the module on
node and on wasmtime and compares the result with the `langc eval` result
(S8, Q-S8-6 (a)).

The lowering lowers a value one time in a block. The blocks in that
block use the same result, but the two arms of a branch do not share
results.

The evaluation fuel (`EVAL_FUEL`) is a host limit, not a trap (N5).

Decision record: `docs/PLAN.md` section 1 (Q-S3-1 to Q-S3-3) and
section 2 (N1 to N5 and the S3 result).

## Wasm layout

- The module has one memory of a fixed size and one mutable `i64` global,
  the heap pointer. The size is the pages of the state region (S8, below)
  and the largest heap bound of an entry (at least 1 page). `langc` adds the words of each heap block on the
  longest path of an entry. When an entry makes heap blocks in a loop with a
  run-time count, its heap use has no static bound: `langc ir` and `langc
  build` refuse it with `REFUSE_LOWER` (`heap use has no static bound`). A
  bound larger than 1 GiB gets the same refusal.
- The functions 0 to I - 1 are the host imports of module `permit` that
  an entry calls (S8, Q-S8-2 (a)): `envelope (i64, i64) -> i64`, `hash
  (i64) -> i64` and `within (i64, i64) -> i64`. A `Payload`, `Bytes`,
  `Digest` or `Resource` value is one `i64` handle that the host makes.
  `payloadEq` and `digestEq` lower to `i64.eq`. I is 0 when no entry
  calls an import, and then the module has no import section.
- Function I is the internal allocator `alloc (bytes : i32) -> i64`. It
  increases the heap pointer. It does not check the size, because the
  memory holds the heap bound of each entry.
- Function I + 1 + K is entry K. Each entry first sets the heap pointer to
  the heap start, so each call starts with an empty heap. The heap start
  is 0, or 8 * R when the program has a state region of R words (S8).
- A `Flag` argument is true when it is not 0. The entry sets it to 1.
- A word is 8 bytes. A heap block holds the tag in word 0 and the word of
  field K in word 1 + K. Each op is total; a value is one word.
- A tag switch is nested blocks and `br_table`. The last arm is the
  default, so a tag out of range runs the last arm.
- The S8 lowerings (Q-S8-9 (a)): a `Size` is a Nat word (`szS` adds 1,
  and `sizePred` stops at 0). An elim over a `Size` is a `REPEAT`, and an
  elim over a run-time nullary family (`Action`, `Reason`) is a switch on
  the word. `len` over a `FreeMon` tree with a closed shape is the sum of
  the leaf words. `usedHas` is a bounded `REPEAT` over the used words of
  the state region. `IR_STMT_STORE` writes one word of a block.
- A program with a `bind` line (Q-S8-4 (a)) has a `def start`
  (Q-S8-5 (a)). Its module has a state region of R = 3 + k words at
  address 0: the steps, the budget left, the used count, then k used
  words. k is the closed `StepBound` of `start`. The data section writes
  the start words. Two synthesized exports run the state step and write
  the next state: `decide (target, action, payload, cost, has_digest,
  digest : i64) -> i64` (one step of `judge`) and `malformed () -> i64`
  (`judgeMalformed`). The result is a decision code: 0 is `permit`, and 1
  to 8 are the arms of the domain `Reason` in order (`scopeMiss` is 1,
  `malformed` is 8). Memory 0 is exported as `memory` after the entries
  (Q-S8-10 (a)), so a host reads the steps in word 0 and the budget left
  in word 1. `langc` refuses an entry with the name `decide`, `malformed`
  or `memory` in this program.
- After the code section: the custom section `permit.res` when the
  program has a path literal (the paths in program order; the handle of
  a literal is its index, Q-S8-3 (a)), then the custom section
  `permit.bind` when it has a bind line (one row for each line: the tool
  name, the `Action` arm number and the pointer, Q-S8-4 (a)).

## Domain slot

`domain/domain.lang` holds the families and the operations of the domain.
Only this file can declare a `family` (rule R1). `domain/README.md` tells
how to replace the sample domain.

## Gate

`make check` prints these lines when it is GREEN (`test/gate.sh`, then
`test/review.sh`, `build/fronttool` and `build/pathtool`):

| Line | What the gate does |
|---|---|
| `examples: 35 checked, 0 failed` | `langc check` accepts each `examples/*.lang` file; the files in `examples/kit.txt` run with `--kit` (`scope.lang`, the S5 demo, `capability.lang`, the S6 demo, and `decide.lang`, the S7 demo, are three of them; S8 adds `resources.lang`). `examples/gate.lang` (S8) is the bind demo: `bind` lines and a `def start` |
| `parse refusals: 22 checked` | each `test/parse` file fails with the code in `test/parse/expect.txt`; one file has 1100 nested parentheses. 4 files (S4) have a bad mark (`@2`, a marked parameter, a marked field) or `-ops`. 1 file (S5, `affine-no-cofamily.lang`) has `affine` with no `cofamily` after it. 1 file (S8, `unclosed-string.lang`) has a string with no end (`LEX_STRING`); `string.lang` gives `PARSE_EXPECT` at S8, because a string lexes for `res` and `bind` |
| `check refusals: 236 checked` | each `test/check` file fails with the code in `test/check/expect.txt`; the `TYPE_REFL` message shows both normal forms. The 45 `affine-*.lang` files (S4) give `REFUSE_AFFINE_REUSE` (11), `REFUSE_AFFINE_OMEGA` (24), `REFUSE_AFFINE_FIX` (3), `REFUSE_AFFINE_PATH` (4) or `TYPE_MISMATCH` (3). 16 of them are in `test/check/kit.txt`, so they run with `--kit`. The 13 S5 files: `time-{date,ttl,deadline,expires}.lang` (`REFUSE_TIME`), `kit-res-root.lang` (`REFUSE_KIT`), `allowance-{reuse,open-twice,open-parts,unpack-twice}.lang` (`REFUSE_AFFINE_REUSE`: a second use of an `Allowance`, of its opened pair, or of its parts) and `allowance-{omega,capture-budget,capture-allowance,capture-polymorphic}.lang` (`REFUSE_AFFINE_OMEGA`: an omega binder or a position closure that captures a resource or an unknown payload). The 6 S6 files: `budget-overspend-{spend,split}.lang` and `delegate-overspend.lang` (`REFUSE_BUDGET_OVERSPEND`), `delegate-widen.lang` (`REFUSE_DELEGATE_WIDEN`), `capability-reuse.lang` (`REFUSE_AFFINE_REUSE`) and `capability-omega.lang` (`REFUSE_AFFINE_OMEGA`); the first 4 are in `test/check/kit.txt`. The 5 S7 files: `execute-unapproved-{none,other,refused}.lang` (`REFUSE_EXECUTE_UNAPPROVED`: an execute with no propose, in the branch of a propose of another mutation, or in the `ppRefused` branch), `digest-payload-changed.lang` (`PATH_END`: a changed payload changes the digest) and `kit-payload-of.lang` (`REFUSE_KIT`); the first 4 are in `test/check/kit.txt`. The 12 S8 files: `res-{upper,empty-segment,star,empty,trailing,leading}.lang` (`REFUSE_RES`: an upper-case letter, an empty segment, a wildcard, an empty path, a trailing or a leading slash), `bind-{twice,action,pointer,escape,tilde-end}.lang` (`REFUSE_BIND`: a second `bind` of one tool, an unknown action, a pointer with no leading slash, a bad `~` escape, a `~` at the end) and `start-literal-other.lang` (`REFUSE_KIT`: a guest Nat literal outside `def start`, Q-S8-8 (a)) |
| `kit-only refusals: 13 checked` | each `REFUSE_KIT` row refuses a kit item in guest mode only: `langc --kit check` accepts the file (`kit-res-root.lang`, S5, is a guest `resRoot`; `kit-payload-of.lang`, S7, is a guest `payloadOf`; `start-literal-other.lang`, S8, is a guest Nat literal outside `def start`) |
| `time hint: 1 checked` | a `REFUSE_TIME` refusal says to count operations: a deadline is a `StepBound` (a count of later operations) |
| `evals: 92 checked` | `langc eval` gives each result in `test/eval/expect.txt`; 6 rows (S5) run `examples/scope.lang` with `--kit`: `within` and `resEq` (yes and no), `afterOne` and `openAfterOne` (L4); 3 rows (S6) run `examples/capability.lang` with `--kit`: `sizeLe` (`oneTwo`, `twoOne`, `twoTwo`); 4 rows (S7) run `examples/decide.lang` with `--kit`: `leftAfter` (3), `stepsAfter` (`sz0`), `decisionOk` (`permit`) and `decisionUsed` (`deny approvalUsed`) |
| `builds: 1 checked` | `langc build examples/functions.lang` writes a module that `wasm-validate` accepts |
| `ir: 1 lowered` | `langc ir examples/functions.lang` gives one `func` line for each `abi` line |
| `ir refusals: 8 checked` | `langc --kit ir` refuses `test/ir/closure.lang`, `kmap.lang`, `tree.lang`, `spine.lang` and `hop.lang`, and (S8) `reschain.lang` (a `Resource` with no path literal), `bindnostart.lang` (a bind line and no `def start`) and `bindmemory.lang` (an entry with the name of a bind export), each with its code. `payload.lang` lowers at S8 (Q-S8-2 (a)) |
| `ir lowerings: 10 lowered` | the `langc --kit ir` text of each file is its `.ir` file: `test/ir/natops.lang` and `test/ir/block.lang`, and (S8) `payload.lang`, `resource.lang`, `sizestep.lang`, `judgesteps.lang`, `actcase.lang`, `decision.lang`, `fmlen.lang` and `judgeaction.lang` |
| `custom sections: 3 checked` | the custom sections of the modules of `test/ir/resource.lang` and `examples/gate.lang` (the name and the size of `permit.res` and `permit.bind`) are the lines of `test/ir/resource.sections` and `test/ir/gate.sections`, and the module of `examples/functions.lang` has no custom section (S8, Q-S8-3 (a), Q-S8-4 (a)) |
| `notrap modules: 5 checked` | `langc build` writes the modules of `test/ir/natops.lang`, `test/ir/block.lang`, `examples/state-no-history.lang` and `test/ir/resource.lang` (S8), and `wasm-validate` accepts each one. `build/notraptool` (`test/notraptool.c`) accepts these four modules and the module of `examples/functions.lang`: no `unreachable`, no `memory.grow`, only the opcodes of `src/wasmop.h`, a divisor guard before each `i64.div_u` and `i64.rem_u`, memory 0 with maximum = minimum, and (S8) only the `permit` imports with their types. `build` lowers the `state` family of `examples/state-no-history.lang` as a plain family (Q-S3-3 (b)) |
| `notrap bad rows: 11 checked` | `build/notraptool` refuses each module of `test/notrap/bad/` (`wat2wasm` makes it from its `.wat` file): exit 1 and the message of its line in `test/notrap/bad/expect.txt`. The 11 modules have `unreachable`, `memory.grow`, an `i64.div_u` and an `i64.rem_u` with no guard, a memory maximum above the minimum, a memory with no maximum, `i32.add` (an opcode that is not in `src/wasmop.h`), and (S8) an import of a module other than `permit`, an import name outside the list, an import type that does not match its name and an import that is not a function |
| `spine evals: 10 checked` | `langc --kit eval` gives each eval of `test/ir/spine.lang` at 4 |
| `natops evals: 2 checked` | `langc --kit eval` gives 1 for `reasonCode 0` and 2 for `reasonCode 1` of `test/ir/natops.lang` (the run-time case split over the domain `Reason`, O15) |
| `wasm runs: 16 checked` | node and wasmtime (`test/run-wasm.mjs`, `--wasmtime`) run 16 calls of the module of `test/ir/natops.lang`; each result is equal to the `langc --kit eval` result (S8, Q-S8-6 (a), Q-S8-11 (b)) |
| `differential cases: 694 checked, 0 mismatches` | `test/grid.awk` makes the cases from the `langc --kit ir` text of 10 modules (`natops`, `block`, `functions`, `state-no-history` and the S8 lowerings `sizestep`, `judgesteps`, `actcase`, `decision`, `fmlen` and `judgeaction`); node and wasmtime run each case, and each result is equal to the `langc --kit eval` result. `langc eval` has no trap, so a Wasm `trap` is a mismatch |
| `decide scripts: 8 checked, 12 calls, 0 mismatches` | each of `test/decide/D1.txt` to `D8.txt` is a call script on one instance of the module of `examples/gate.lang` (`test/run-wasm.mjs --decide`). node and wasmtime print the decision codes, then the steps and the budget left (words 0 and 1 of `memory`). Each line is equal to `langc --kit eval` of the judge chain that `test/run-wasm.mjs --lang` prints (Q-S8-10 (a), Q-S8-12 (a)). The 8 scripts give the codes 0 (`permit`); 1; 4; 8; 0 0 0 2 (the steps run out); 0 6 (a second use of one approval); 5; 3 |
| `gate: 0 failures` | the sum of all failures of `test/gate.sh` |
| `review regressions: passed` | `test/review.sh` passes |
| `domain regressions: passed` | `build/fronttool` (the domain declaration rows, the 54 affine rows of S4 and the 4 time rows and 3 affine cofamily rows of S5 and the 2 guest `spend` and `split` rows of S6) passes |
| `path regressions: passed` | `build/pathtool` (the path rows) passes |

The gate also checks the usage and IO exit codes, five `eval` refusals and
the absence of em-dashes and en-dashes outside `design/DESIGN.md`. It
checks memory 0 of the modules of `test/ir/natops.lang` and
`examples/functions.lang` (0 heap words): 1 page, minimum = maximum.
`langc eval` refuses a Flag of 2 with `EVAL_ARGS` (`pick 2 4 9`). The doc
check of lang-template (`perl bin/doc-check.pl` with this repository as
the root) must report 0 problems.

The `wasm runs` and `differential cases` rows of relay-lang 5c386bb ran
node only, so they were out of the S3 gate (Q-S3-2 (b)). They are back at
S8 (Q-S8-6 (a)): node and wasmtime run each call, and each result is
equal to the `langc eval` result. `test/run-wasm.mjs --wasmtime` runs one
`wasmtime run --invoke` for each call. When a module imports module
`permit`, the two runs bind the O5 model of `langc eval`: `within` reads
the `permit.res` table, and `envelope` and `hash` are injective stubs.
`permitd` binds the real imports at S9.

## Known limits

- A `fold` over a bare `nil` cannot infer its type (`TYPE_INFER`). Use a
  definition with a declared `List` type.
- With no expected type, `either`, `map` and `cong` take the result type
  of the function at a fresh variable. A dependent result needs a
  declared type.
- A definition with a `List`, `Prod` or other non-scalar result is not an
  entry. `ir` and `abi` skip it.
- `langc eval` stops after 20000000 steps (`EVAL_FUEL`). The Wasm module
  has no step limit, so a large loop count can run on Wasm and stop in
  `eval`. The grid of `test/grid.awk` uses large values only for entries
  with no loop.
- The arena of `langc` stops at 1 GiB. The Wasm memory has a fixed size:
  minimum = maximum = the pages of the largest heap bound of an entry, at
  least 1 page. An entry with no static heap bound refuses
  (`REFUSE_LOWER`, `test/ir/spine.lang`).
- A program with a `bind` line needs a `def start` that is a closed
  `authority`: closed steps, a closed budget left and `fmUnit` used.
  Otherwise `langc build` refuses it with `REFUSE_LOWER`
  (`test/ir/bindnostart.lang`, Q-S8-5 (a)).
- One `decide` call adds at most one digest to the used words: `judge`
  does one `fmCons` for a permitted call. The lowering refuses a next used
  trace of another form (`REFUSE_LOWER`, `a used trace other than fmCons
  d used`). `auUsed` holds at most k digests (F9), so k words hold the
  trace.
- Source files are at most 1 MiB. `Nat` literals are at most
  18446744073709551615.
