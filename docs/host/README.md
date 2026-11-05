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
- the host engine of `permitd` (S9): `host/engine.c`, `host/engine.h`,
  `host/wasmread.c`, `host/wasmread.h`, `host/sha256.c`,
  `host/sha256.h`, `host/jsonin.c`, `host/jsonin.h`, `host/jcs.c` and
  `host/jcs.h`
- the MCP gate (S9 U3): `host/permitd.c`
- the JSON authority report (S10): `src/report.c`, `src/report.h`,
  `host/report.c` and `host/report.h` (`docs/REPORT-JSON.md`)
- the Cedar export (S11): `src/cedar.c` and `src/cedar.h`
  (`docs/CEDAR.md`)
- the tests: `test/parse/`, `test/check/`, `test/eval/`, `test/ir/`,
  `test/notrap/`, `test/engine/`, `test/host/deploy.lang`, `test/report/`, `test/cedar/`,
  `test/review.sh`, `test/fronttool.c`, `test/pathtool.c`,
  `test/notraptool.c`, `test/enginetool.c`, `test/shatool.c`,
  `test/jcstool.c`, `test/fakedeploy.c`, `test/hosttool.c` and
  `test/reporttool.c`
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
`build` and `check` also build `build/shatool`, `build/enginetool`,
`build/jcstool`, `build/permitd` and `build/fakedeploy` (S9) with
`-Ihost`, and `build/hosttool` (S9). `check-clang` compiles their
sources.
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
| R1 | `REFUSE_CELL` | a type constructor that is not one cell signature or one cocell signature, a cons-list name in place of a cell monad, or a sealed family that is not a cell monad |
| R2 | `REFUSE_AXIOM` | an `axiom` declaration |
| R3 | `REFUSE_REC` | `def rec`, or a definition that refers to itself or to a later definition |
| R4 | `REFUSE_NAME` | a core name, a domain name, a reserved name or a repeated name as a new definition |
| R4 | `REFUSE_TIME` | one of 10 time words (`now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `date`, `ttl`, `deadline`, `expires`) as a name or a use; the message says to count operations: a deadline is a `StepBound` (a count of later operations) |
| L1 | `REFUSE_DELEGATE_WIDEN` | a closed `delegate` application whose child scope is not within the parent scope, at a term level of 1 |
| L2 | `REFUSE_BUDGET_OVERSPEND` | a closed `spend`, `split` or `delegate` application whose budget part reduces to `none`, at a term level of 1 |
| D-9 | `REFUSE_HISTORY_WRITE` | a history field `h` that is not `(h t)`, `natAdd (h t) e` or the variable of `bind (natAdd (h t) e)`, a history constructor without all of its arguments, or a definition with `State` in its type that refers to a name that the history rule does not check |
| - | `REFUSE_MAP` | a `Map` key type that is not `Nat`, or a `Map` value type that is not `Nat` or `Flag` |
| - | `REFUSE_SIG` | a name of the design signature (PLAN section 3): a slot with the wrong type, a reserved name that no slice gives yet, or a defined or primitive name that the kernel gives |
| - | `REFUSE_STATE` | a second state, a state type with a name other than `State`, a state with parameters, an index telescope or not one constructor, a state field that is not first-order, or a history field that is not `Nat` |
| - | `REFUSE_AFFINE_REUSE` | a variable that one evaluation path uses more times than its multiplicity allows (PLAN 6) |
| - | `REFUSE_AFFINE_OMEGA` | a variable at multiplicity 1 that goes to an omega binder, to a closure that can run more than one time, to a polymorphic payload that can become copyable, or to a `map` result that can be copyable |
| - | `REFUSE_AFFINE_FIX` | a variable at multiplicity 1 that a fold step or an elimination case uses, when that step or case can run more than one time |
| - | `REFUSE_AFFINE_PATH` | a variable at multiplicity 1 that is free under an interval binder (`fun i =>`, `refl`, `sym`, `cong`, `funExt`, `transp`, `hcomp`, `Glue`, `glue` or `unglue`): both end points of the path hold it |
| R0 | `REFUSE_PARTIAL` | a partial name, a literal 0 as a `NonZero Nat`, or an `Option` result in the place of a value |
| - | `REFUSE_LOWER` | an entry that the Wasm host cannot lower: heap use with no static bound, a recursive fold over a block of families, a `start` that is not closed, a `bind` line with no `def start`, or an entry with the name of a bind export (`langc ir` and `langc build` only) |
| - | `REFUSE_KIT` | a kit item outside the base span (PLAN section 4) in guest mode; `langc --kit` accepts it |
| - | `REFUSE_EXECUTE_UNAPPROVED` | a `planExecute` that is not in the `ppApproved` branch of an earlier `planPropose` of the same mutation on its path |
| - | `REFUSE_RES` | a resource path that is not segments of `a-z`, `0-9`, `_` and `-` with one `/` between two segments, a `*`, or more resource paths or segments than the table holds |
| - | `REFUSE_BIND` | a second `bind` line of one tool, an action that is not an arm of `Action`, a pointer that is not an RFC 6901 pointer, or more `bind` lines than the table holds |
| O7 | `REFUSE_LAW` | a definition that uses a law-only name (`decrypt`, `observe`, an elim, fold or projection of a sealed family) but has a type that is not a law (a `Path` or `Trunc1` codomain); `langc check` cannot give it (Q-S13-9) |

## CLI

```sh
build/langc check [--json] PROG
build/langc eval PROG NAME [ARGS...]
build/langc ir PROG
build/langc abi PROG
build/langc build PROG -o OUT [--runtime]
build/langc report PROG
build/langc cedar PROG
build/langc fmt PROG [--check]
build/langc explain CODE
build/langc base
build/langc law LN
build/langc [--kit] mcp
```

- `check` prints `ok`, or refuses the program. With `--json`, it prints
  one JSON line on stdout in place of `ok` or the diagnostic
  (`docs/CHECK-JSON.md`).
- `eval` prints the normal form of `NAME`. Each argument is a decimal
  number. An entry has no trap (see "No traps").
- `ir` prints the first-order IR. It has one `func` for each entry.
- `abi` prints the name of each entry, one for each line.
- `build` writes the Wasm module to `OUT`. On this target, `--runtime`
  gives the same bytes.
- `report` prints the `program` line of the JSON authority report
  (S10, `docs/REPORT-JSON.md`).
  `REFUSE_REPORT` refuses a `def start` that is not a closed `authority`,
  or that has steps or a scope that are not closed.
- `cedar` prints the start scope as Cedar `permit` policies (S11,
  `docs/CEDAR.md`).
- `fmt` prints the canonical text of the program on stdout (S12, A1). It
  does not change `PROG`. It parses `PROG` and does not check it, and
  `--kit` has no effect. The line width is 100. A longer decl breaks only
  after `:=`, and the body is at a 2-space indent. Each constructor after
  the first is on its own line, as `| c` at a 2-space indent. An axiom
  never breaks. A blank line comes before each decl, but not before an
  `and` sort and not between two `bind` lines that follow each other.
  The spellings are `Sigma (x : A) B`, one `fun` for a lambda chain, a
  bare name for a binder with no type, the minimum parentheses and one
  name in each binder group. The marks print as `(@0 x : A)`,
  `(@1 x : A)` and `A -o B`. Each string prints as it is in the source.
  A group `(x y : T)` stays one group when `T` names an earlier name of
  the group (H1, Q-S12-7). Each comment stays as it is, on its own line
  at column 0. A comment inside a decl, or after code on its line, moves
  above the decl. With `--check`, `fmt` prints nothing and exits 0 when
  `PROG` is canonical. Else it prints `PROG:LINE: not canonical` on
  stderr, where `LINE` is the first line that is different, and it exits
  with 1. On a lex or parse error, `fmt` writes the diagnostic on stderr,
  writes nothing on stdout and exits with 1. Before it writes the text,
  `fmt` lexes and parses the canonical text again. If that text is too
  large or does not parse, `fmt` exits with 1. A usage error, an IO
  error or a full arena gives exit 2.
- `explain` prints one line `CODE: MEANING (docs/host/README.md, SECTION)`
  for a refusal code (S14, A4). The meaning is the text of the code in
  this file. An unknown code gives exit 1 and an `EXPLAIN` diagnostic.
- `base` prints one line `NAME KIND LINE` for each of the 20 rows of the
  base span (`docs/PLAN.md` section 4).
- `law` prints one line `LN NAME: STATEMENT` for the law `LN` (`L1` to
  `L8`) of `docs/PLAN.md` section 7. An unknown law gives exit 1 and a
  `LAW` diagnostic.
- `mcp` is the MCP server of these verbs on stdin and stdout
  (`docs/MCP.md`, `## langc mcp`). With `--kit`, each tool that reads a
  program runs in the kit mode.

The exit status is 0 for success, 1 for a refused program and 2 for a
usage or IO error. A diagnostic on stderr has the form
`langc: CODE: DEF: message`. `fmt --check` also exits 1 when the
program is not canonical, and then it writes only the line
`PROG:LINE: not canonical` on stderr.

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

## Host engine

`permitd` (S9) runs the gate module in its own C99 engine,
`host/engine.c`. The engine is relay-lang 4781ca7 `host/engine.c` and
`src/wasmread.c` (file commit 8d4382a) with these changes (Q-S9-1 (a)):

- Bindings. `engine_load` takes an `EngineBinding`: a table of host
  functions (a module name, an import name, a parameter count and a C
  function each) and one context pointer for all of them. Each import of
  the module gets the row with its module and name. Two bindings use the
  table: O5 (the stubs of `test/run-wasm.mjs`, in `test/enginetool.c`)
  and the real binding of `permitd` (S9 U2 and U3).
- Load refusals. An import with no row is `unbound import`. An import
  whose type does not have the parameter count of its row and 1 result is
  `import type`. Each is `ENGINE_REFUSE`, and `engine_message` gives the
  text.
- Import index. The imports come first in the function index space. A
  `call` with an index below the import count calls the host function. A
  larger index calls the module function at the index minus the import
  count.
- Data. At load, an active data segment at an `i32.const` offset goes
  into the memory. A segment of another form is `data segment`, a
  segment with no memory is `data with no memory`, and a segment outside
  the memory is `data bounds`.
- Memory export. The engine keeps a note of an export of memory 0 (any
  name). `engine_word` reads and `engine_put_word` writes an 8-byte
  little-endian word at an address of the memory. Each gives 0 when the
  module does not export its memory or the word is not in the memory.

`build/enginetool SCRIPT FILE` loads FILE with the O5 binding and runs a
`test/decide` script on one instance. Before the first call it reads the
words 0 and 1 (the steps and the budget left) and writes each back with
`engine_put_word`. The U3 state file puts its saved words on this path
(Q-S9-5 (a)). `host/sha256.c` is the SHA-256 of quorum-lang be2c329
`src/crypto/sha256.c` (file commit 8e18ebf, Q-S9-2 (a)), and
`build/shatool` checks it.

## JSON, JCS, envelope and pointer

`permitd` reads the arguments of a `tools/call`, makes their canonical
bytes and hashes them (S9 U2, Q-S9-3 (a)).

- `host/jsonin.c` and `host/jsonin.h` are the JSON reader of relay-lang
  4781ca7 `src/front/jsonin.c` (file commit 4781ca7). `json_parse` reads
  RFC 8259 text into a tree of `JsonValue` in an arena. Invalid UTF-8, a
  lone surrogate escape and a depth above 64 fail. The permit change: a
  duplicate key in one object fails (`a duplicate key`). Each failure is
  `deny malformed` (PLAN sec 4).
- `jcs_write` (`host/jcs.c`, `host/jcs.h`) writes the RFC 8785 (JCS) form
  of a value. The keys of an object are in the order of their UTF-16 code
  units (RFC 8785 3.2.3). A string has the RFC 8785 3.2.2.2 escapes only.
  There is no white space. A number is `-?(0|[1-9][0-9]*)` with a
  magnitude of at most 2^53 - 1, and `-0` writes `0`. A fraction, an
  exponent or a larger magnitude gives `JCS_MALFORMED`. A full buffer
  gives `JCS_FULL`.
- `jcs_envelope` writes `{"payload":P,"target":[S...]}`. P is the JCS
  form of the canonical tool-call object, and each S is one segment string
  of the target path (PLAN sec 4, the review clarification of Q-S0-4).
  The `hash` of the real binding is the SHA-256 of these bytes.
- `jcs_pointer` finds the value at an RFC 6901 pointer: `~1` is `/`, `~0`
  is `~`, and an array index is `0` or `[1-9][0-9]*` below the item
  count. A bad escape, a `~` at the end, a missing key or `-` gives no
  value. The `bind` pointer of `permit.bind` selects the payload with it.

`build/jcstool` checks these files on 32 rows: 17 JCS rows, 3 envelope
rows, 1 envelope digest row (the SHA-256 of the first envelope row, from
`shasum -a 256`) and 11 pointer rows.

## permitd

`build/permitd` is the MCP gate (S9 U3, Q-S9-4, 5, 6 and 9 (a)):

```sh
permitd [--state FILE] [--report FILE] [--approvals FILE | --approver tty]
        [--cost TOOL=N]... [--cred-file PATH | --cred-env NAME]
        [--cred-var NAME] MODULE -- CMD ARGS
```

The agent talks to `permitd` on stdin and stdout. `permitd` starts CMD,
the upstream MCP server, as a child on two pipes. Each `tools/call` goes
through the `decide` export of MODULE in the steps of "The call steps" in
`docs/MCP.md`. Each other line goes through unchanged. `docs/MCP.md` also
gives the upstream failures, the approvals, the state file and the
credential.

With `--report FILE`, `permitd` opens FILE to append and writes one line
of the JSON authority report for each decided call, after the state
write and before the answer or the forward (S10 U2,
`docs/REPORT-JSON.md`).

The exit codes of `permitd`:

- 0: the end of the input
- 1: MODULE does not load
- 2: a usage error, a bad state file or a report file that does not open

For each permitted call, `permitd` writes one line to stderr:
`permitd: permit TOOL approver WHO`. WHO is `FILE:LINE` (the approvals
file and the line number of the digest), `tty`, or `none` when the call
has no approval.

## Gate

`make check` prints these lines when it is GREEN (`test/gate.sh`, then
`test/review.sh`, `build/fronttool` and `build/pathtool`):

| Line | What the gate does |
|---|---|
| `examples: 35 checked, 0 failed` | `langc check` accepts each `examples/*.lang` file; the files in `examples/kit.txt` run with `--kit` (`scope.lang`, the S5 demo, `capability.lang`, the S6 demo, and `decide.lang`, the S7 demo, are three of them; S8 adds `resources.lang`). `examples/gate.lang` (S8) is the bind demo: `bind` lines and a `def start` |
| `parse refusals: 23 checked` | each `test/parse` file fails with the code in `test/parse/expect.txt`; one file has 1100 nested parentheses. 4 files (S4) have a bad mark (`@2`, a marked parameter, a marked field) or `-ops`. 1 file (S5, `affine-no-cofamily.lang`) has `affine` with no `cofamily` after it. 1 file (S8, `unclosed-string.lang`) has a string with no end (`LEX_STRING`); `string.lang` gives `PARSE_EXPECT` at S8, because a string lexes for `res` and `bind`. 1 file (S12, `string-nul.lang`) has a NUL byte in a string (`LEX_STRING`, Q-S12-8) |
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
| `engine decide scripts: 8 checked, 12 calls, 0 mismatches` | `build/enginetool` runs each `test/decide/DN.txt` on the module of `examples/gate.lang` in the own engine (`host/engine.c`) with the O5 binding. Each line is equal to the node line of the `decide scripts` row (S9, Q-S9-1 (a), L8) |
| `engine refusals: 2 checked` | `build/enginetool` refuses the modules of `test/engine/unbound.wat` (`unbound import`: no O5 row for `permit` `nope`) and `test/engine/badtype.wat` (`import type`: `permit` `hash` with 2 parameters) at load. Each gives exit 1 and the one line `enginetool: load: MESSAGE` (S9) |
| `sha256 vectors: 7 checked` | `build/shatool` checks `host/sha256.c` on the 4 FIPS 180-4 vectors and 3 pad rows (55, 56 and 64 bytes of `a`) (S9, Q-S9-2 (a)) |
| `engine hash rows: 2 checked` | `build/enginetool` script line `hash N` gives the O5 hash stub of N. The 2 rows (N 0 and 9223372036854775809) compare it with the number of the `test/run-wasm.mjs` formula `(N * 1000003 + 7) mod 2^64` (S9 U2, mutant O1) |
| `jcs rows: 32 checked` | `build/jcstool` checks `host/jsonin.c` and `host/jcs.c`: 17 JCS rows, 3 envelope rows, 1 envelope digest row and 11 pointer rows (S9 U2, Q-S9-3 (a)) |
| `report rows: 7 checked` | `langc report` writes the `program` line of each `test/report/*.lang` file: exit 0, no stderr and the bytes of its `.json` file (S10 U1; `empty` S11 U2, `big` S11 U3; `docs/REPORT-JSON.md`) |
| `cedar rows: 7 checked` | `langc cedar` writes the Cedar text of each `test/report/*.lang` file: exit 0, no stderr and the bytes of `test/cedar/NAME.cedar`; the count must be the report row count (S11, `docs/CEDAR.md`) |
| `report write errors: 2 checked` | `build/reporttool` checks the same valid report with writable and read-only stdout: exit 0 and exit 1 |
| `cedar write errors: 2 checked` | `build/reporttool` checks `langc cedar` on the same program the same way: exit 0 and exit 1 (S11 U2) |
| `json rows: 28 passed` | for each `test/json/X.lang`, `langc check --json` exits 0 (the rows `ok` and `kit`) or 1 (each other row, whose first code is the file name up to the first dot), writes nothing on stderr, and gives the bytes of `test/json/X.json`; the 11 rows of `json_kit` run with `--kit` (S13, A2, `docs/CHECK-JSON.md`) |
| `json refusal codes: 21 of 21` | each `REFUSE_` code of `src/front/check.c` has a row `test/json/CODE.lang`, and the list of codes is not empty; `REFUSE_LAW` has no row, because `langc check` cannot give it (Q-S13-9) |
| `json fixes: 26 passed` | `build/fronttool fix` applies each `test/json/X.fix` to its row, and `langc check --json` of the result exits 0 or gives a first code other than the code of the row |
| `host rows: 36 checked` | `build/hosttool` runs `build/permitd` on the module of `test/host/deploy.lang` with `build/fakedeploy` as the upstream server, one line at a time. The 12 rows of S9 U3: a line that is not JSON, a tool with no bind row, a target that is not a string, a bad approval hex, `--error` and `--exit` of the upstream server, a bad state file (exit 2), the forwarded bytes (no `permit/approval` key and no empty `_meta`), the `isError` of a deny, `--cost`, an approvals line added between two calls, and `--state` in a missing directory (N6). Six review rows check refusal of batches and scalars before forwarding, the retained metadata key with an embedded NUL, the maximum 64-bit cost, cost overflow, and a large state budget across a restart. The 10 demo rows of S9 U4 (PLAN section 8.3): D1 to D8, D10 and D11, each with the want of its PLAN row; D9 is the static row `test/check/delegate-widen.lang` in `check refusals`. The script line `! L` closes the input of permitd, wants exit 0 and no extra answer, and starts permitd again with the same flags (row D6: the approval stays used after a restart). The 8 report rows, R1 to R8: the script line `# L` wants L as the next line of the `--report` file (`docs/REPORT-JSON.md`) |
| `fmt canonical: 326 passed` | `langc fmt` gives the same bytes for each accepted `.lang` file of `domain/`, `examples/`, `examples/*/` and `test/*/` outside `test/fmt/` (S12, A1) |
| `fmt round trip: 338 passed` | each accepted file (the 326 files and the 12 `test/fmt/*.lang` files) and its canonical text give the same `build/fronttool dump`. The dump has no line numbers and no comments. It is scoped (`test/fronttool.c`): each variable prints the distance to its binder or `global`, and each binder prints its mark |
| `fmt refused: 24 checked` | each file that `langc fmt` refuses is in `test/fmt/refused.txt` (the 22 `test/parse` files and the `test/json` rows `LEX_CHAR` and `PARSE_EXPECT`), and no accepted file is |
| `fmt rows: 13 passed` | `langc fmt test/fmt/X.lang` gives the bytes of `test/fmt/X.canon` for each of the 10 golden rows (`aliases`, `binds`, `capture`, `check`, `comment`, `decls`, `lambda`, `layout`, `marks`, `strings`). The 3 witness rows: the dump of `piname.lang` is not the dump of `lambda.canon` (a Pi name), the dump of `mark.lang` is not the dump of `marks.canon` (a mark), and `capture.canon` keeps the group `(x y : x)` (H1, Q-S12-7) |
| `fmt check: 11 passed` | `langc fmt X.canon --check` exits 0 with no output for each of the 10 golden rows, and `langc fmt test/fmt/check.lang --check` exits 1 with `test/fmt/check.lang:3: not canonical` |
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
