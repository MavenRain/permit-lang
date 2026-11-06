# Host capability probe: tcc-wasm

Date: 2026-11-05. Host tree: `/Users/oobi/Documents/permit-lang` at
b7e1228 (tree 0d15389f), with the S17 edit of `SPEC.md` not committed
(u2-probe.log:3-4). The probe read the tree and did not change it.
Driver: `build/langc` (file time 2026-11-05 13:38). `make -q` exits 0 for
`build/langc`, `build/permitd`, `build/hosttool`, `build/fakedeploy`,
`build/shatool` and `build/jcstool`, thus the binaries agree with their
sources (u2-probe.log:9-10). The tools are tcc 0.9.28rc 2026-09-04
mob@0fb54300 (AArch64 Darwin), node v23.10.0, wasm-validate 1.0.39 and
wasm-objdump 1.0.39 (u2-probe.log:5-8), the versions of `PIN` lines 1, 3,
4 and 5. Other work loaded the machine (load averages 8.58 to 9.78,
u2-probe.log:2, :707). Thus each wall time is an upper bound. A cite
`u2-probe.log:N` is a line of the run log
`permit-lang-work/s17/u2-probe.log`, which is outside the repo.

The probe ran `zsh -f u2-probe.sh` in `permit-lang-work/s17/` (run 4: 72
steps, EXIT 0). Each step is one `step` line of the script. To run some
steps, keep the setup lines and those `step` lines. The script writes the
generated inputs below `u2/in/`, the outputs below `u2/out/`, the log
`u2-probe.log` and the summary `u2-probe.sum`, all in that directory.
Each step runs below `probe/guard.py --rss-mb 4096 --timeout 120`, which
stops a command above 4096 MB resident memory or above 120 s. The guard
prints one JSON line for each step, with the wall time and two resident
peaks (u2-probe.log:14). The largest process in the probe used 189.8 MB
(u2-probe.log:256, the check of `examples/circle.lang`). The guard rows
at the end of this file give each step. In run 3, the run steps gave
`build/fakedeploy` no LOG argument. The child stopped at once, and each
call gave `toolFailed closed`. Run 4 gives the child its log file
(`test/fakedeploy.c:229-230`). The script, the generated inputs and the
logs are local artifacts, not files of this repository. They are
provenance for this report, not a repository-local replay command.

To recheck the repository fixtures from the repository root, run:

```sh
make check
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc check test/host/deploy.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check examples/sigma.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check test/check/self.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check test/check/rec.lang
python3 probe/guard.py --rss-mb 4096 --timeout 120 -- build/langc --kit check test/check/axiom.lang
```

`make check` runs the gate (`Makefile:113-114`, `test/gate.sh`). The
first two guard rows print `ok` and exit 0 (u2-probe.log:32, :48). The
`self.lang` and `rec.lang` rows exit 1 with REFUSE_REC (u2-probe.log:52,
:60). The last row exits 1 with REFUSE_AXIOM (u2-probe.log:268). These
commands cover the checked-in fixtures, not the generated probe inputs or
the cost measurements in section 6.

The base span: `langc check` refuses a program with a `Nat` or another
kit item. The line is REFUSE_KIT `T: Nat is a kit item outside the base
span` (u2-probe.log:68, `test/check/kit-type-nat.lang`), and `langc
explain REFUSE_KIT` gives the text of the code (u2-probe.log:108). The
gate runs a program with `--kit` when `examples/kit.txt` or
`test/check/kit.txt` lists it (`test/gate.sh:13`, :35), and so do the kit
steps of the probe. `examples/kit.txt` lists 24 examples, among them
`sigma`, `capability`, `circle`, `equality`, `paths`, `quot` and `trunc`,
but not `gate`. `test/host/deploy.lang`
checks `ok` in the base span (u2-probe.log:32), and so do the generated
cost files (u2-probe.log:290, :410). `langc base` prints the sorts of the
base span, first `Resource kernel sort, copyable` (u2-probe.log:120).

## Questions

Each host must answer these questions. Each answer is one numbered section
below. An answer sets the status of a former in `formers/tcc-wasm.md`.

| Question | Section | Formers |
|---|---|---|
| Which commands check a file, print a normal form, build and run? | 1 | all |
| Does the host take several files, or must the driver join them? | 2 | all |
| Which recursion does the host accept, and how does it refuse the rest? | 3 | F6, F7 |
| Which `Nat` primitives exist? Does `Nat` have an eliminator? | 4 | F6, F7 |
| How does a result leave the host? | 5 | target |
| What do check and build cost (time and resident memory) at N = 100 and N = 1000? | 6 | all |
| Can a family take a parameter? Can a constructor of such a family appear in a term? | 7 | F4, F12, F15 |
| Can a program project a Sigma? Does the kernel accept Sigma eta? | 8 | F11 |
| How does a program use equality: transport, cong, an index type? | 9 | F12, F13 |
| How does the host report an axiom? | 10 | R2 |

## 1. Commands

| Job | Command | Result |
| --- | --- | --- |
| Check | `build/langc [--kit] check [--json] PROG` | `ok` and exit 0 for `test/host/deploy.lang` (u2-probe.log:32), or one line `langc: CODE: NAME: TEXT` and exit 1 (u2-probe.log:52). `check --json` prints one JSON object (section 10) |
| Print normal form | `build/langc [--kit] eval PROG NAME [ARGS...]` | The normal form of NAME. For `start` in `test/host/deploy.lang`: an `authority` term of 394 bytes that starts `authority (fmMul (Prod Resource Action)` (u2-probe.log:36). Exit 0 |
| Build | `build/langc [--kit] build PROG -o OUT [--runtime]` | One wasm module. For `test/host/deploy.lang`: 9019 bytes, and `wasm-validate` accepts it (u2-probe.log:83, :86, :687). With `--runtime` the module is also 9019 bytes (u2-probe.log:89, :688) |
| Run | `build/permitd [FLAGS] MODULE -- CMD ARGS` | `permitd` loads MODULE and reads JSON-RPC lines on its input. For each permitted `tools/call` it writes `permitd: permit inspect approver none` on its error output and sends the call to the child CMD. With `build/fakedeploy LOG` as the child, 100 calls give 100 results `ok inspect` with `isError` false, exit 0 (u2-probe.log:300). `build/hosttool` runs the host rows: `host rows: 36 checked` (u2-probe.log:280) |

- `langc` with no command gives USAGE `expected a command and a program
  path` and exit 2. It prints the usage of 12 commands: `check
  [--json]`, `eval`, `ir`, `abi [--text]`, `build` (with `-o` and
  `--runtime`), `report`, `cedar`, `fmt [--check]`, `explain CODE`,
  `base`, `law LN` and `mcp` (u2-probe.log:15-31; `src/main.c:55-71`).
  There is no `sim`, `sql` or `ts` command.
- One file for each command: `check` with two files gives USAGE `too
  many arguments` and exit 2 (u2-probe.log:135, section 2).
- `ir` prints the lowered decision, `func decide (nat, nat, nat, nat,
  nat, nat) -> nat, 288 locals, 58 heap words` (u2-probe.log:72).
  `abi --text` prints the two lines `decide` and `malformed`
  (u2-probe.log:78).
- `report` prints one JSON object of 648 bytes. Its `binds` give the
  tool, the action and the pointer of each bind (u2-probe.log:92, :691).
  `cedar` prints the static scope of `def start` as Cedar permit
  policies, 1204 bytes (u2-probe.log:96, :692).
- `fmt PROG --check` prints nothing and exits 0 for `examples/sigma.lang`
  and for `test/host/deploy.lang` (u2-probe.log:102, :105). `explain
  CODE` prints the text of a code (u2-probe.log:108, :112, :116). `law L1`
  prints the L1 attenuation law (u2-probe.log:126).
- `mcp` reads JSON-RPC lines on its input and exits 0 at the end of the
  input. An `initialize` with empty params gives error -32602 `Invalid
  params`, and `tools/list` gives the tools, first `check`
  (u2-probe.log:130).
- `permitd` with no arguments prints its usage and exits 2: the flags
  `--state FILE`, `--report FILE`, `--approvals FILE` or `--approver
  tty`, `--cost TOOL=N`, `--cred-file PATH` or `--cred-env NAME`, and
  `--cred-var NAME` (u2-probe.log:284; `host/permitd.c:970-975`). The
  engine is `host/engine.c` and `host/wasmread.c` (`Makefile:21`, :23).
- The host tools: `build/shatool` gives `sha256 vectors: 7 checked`
  (u2-probe.log:272; `test/gate.sh:535-536`), and `build/jcstool` gives
  `jcs rows: 32 checked` (u2-probe.log:276; `test/gate.sh:545-547`).
  `build/hosttool` runs `build/permitd` with `build/fakedeploy` as the
  child (`test/gate.sh:865-883`).
- `PIN` gives the tool versions of the last green host gate, and the
  probe tools are the same (u2-probe.log:5-8). `PIN` line 7 names `make
  -C hosts/tcc-wasm check`. This repo has no `hosts/` directory, thus the
  probe has no step for it.
- Not probed: a run of a built module in node, the `ARGS` of `eval`, the
  tool calls of `mcp`, the `permitd` flags, and `make tokens`
  (`build/tokcount`, `Makefile:29`; `tokens` needs ANTHROPIC_API_KEY,
  `Makefile:140-141`).

## 2. Several files

No. A program is one file, with no import form. A file that starts with
`import domain` gives PARSE_EXPECT `expected 'def', 'axiom', 'family',
'cofamily', 'state', 'sealed' or 'law', found a name` and exit 1
(u2-probe.log:152). `langc check` with two files gives USAGE `too many
arguments` and exit 2 (u2-probe.log:135). A program uses the domain names
with no import: `def d0 : Size := sz0` checks `ok` (u2-probe.log:156).
Thus a driver must join the parts of a program into one file. The cost
rows do this: the probe joins a `Size` chain and `test/host/deploy.lang`
into one file (u2-probe.sh:42), and the check and the build of that file
exit 0 (u2-probe.log:290, :294, :410, :414; section 6).

## 3. Recursion and totality

No recursive definition checks. `def rec` (`test/check/rec.lang`) gives
REFUSE_REC `loop: def rec is refused (rule R3); use fold or unfold` and
exit 1 (u2-probe.log:60). With `--kit`, a call of the definition itself
(`test/check/self.lang`) or of a later name (`test/check/later.lang`)
gives REFUSE_REC and exit 1: `spin: spin is this definition or a later
one; a definition can use only earlier ones (rule R3)` (u2-probe.log:52),
and the same line for `late` in `early` (u2-probe.log:56). In the base
span `test/check/self.lang` stops first at REFUSE_KIT (u2-probe.log:64;
the base-span paragraph above). `langc explain REFUSE_REC` gives the text
of the code (u2-probe.log:112). Recursion is `fold` and `unfold`, as the
REFUSE_REC line says. F6 (algebra fold) is DONE over Nat, List and the
domain families, and F7 (algebra unfold) is DONE into List with a Nat
step limit (`formers/tcc-wasm.md:19-20`). The probe ran no `fold` or
`unfold` of its own.

## 4. Nat

The primitives are `natAdd`, `natSub`, `natDiv` and `natMod`, with the
comparisons `natEq` and `natLe` (`src/front/check.c:211-214`, :235-236).
`natAdd` gives an `Option Nat` (:211): `natAdd 2 3` is `5`, and `natAdd
18446744073709551615 1` is `none` (u2-probe.log:168, :172). `natSub 4 9`
is 0 (u2-probe.log:160). The divisor of `natDiv` and `natMod` is a
`NonZero Nat`: `natDiv 7 2` is 3 (u2-probe.log:176), and the literal 0
gives REFUSE_PARTIAL `divz: the literal 0 is not a value of NonZero Nat:
use nonZero, which gives an Option` and exit 1 (u2-probe.log:180), and
the same for `natMod` (u2-probe.log:184). `natMul` and `natRec` give
NAME_UNKNOWN and exit 1 (u2-probe.log:188, :192): `Nat` has no eliminator
primitive, and the eliminator is the algebra `fold` (F6, section 3). The
base span refuses `Nat`: `test/check/kit-type-nat.lang` gives REFUSE_KIT
and exit 1 (u2-probe.log:68). Thus the steps above run with `--kit`. The
largest value is 18446744073709551615, that is 2^64 - 1
(u2-probe.log:164). A literal above it gives LEX_NAT_RANGE `a Nat literal
is larger than 2^64 - 1` (u2-probe.log:196). No step of the probe
trapped. A definition cannot recurse on a `Nat` (section 3). The base
span counts with the domain family `Size` (`domain/domain.lang:56`).

## 5. How a result leaves the host

As printed text, as one wasm module, or as the answer of `permitd`.
`eval` prints the normal form (u2-probe.log:36). `ir` and `abi --text`
print the lowered decision and the interface (u2-probe.log:72, :78).
`build` writes one wasm module with `-o` (u2-probe.log:83, :687), and
`wasm-validate` accepts each module of the probe (u2-probe.log:86, :297,
:417). `report` prints the binds as JSON (u2-probe.log:92), and `cedar`
prints Cedar permit policies (u2-probe.log:96). `check --json` prints the
diagnostics as one JSON object (u2-probe.log:260). `permitd` runs a
module at the tool boundary: it sends a permitted call to the child, and
the answer of the child goes back as the JSON-RPC result
(u2-probe.log:300).

## 6. Cost

Each file is `test/host/deploy.lang` with a chain of N `Size` definitions
before it (`def kI : Size := szS kJ`, u2-probe.sh:42). `start` uses `kN`
as the operation count and N as the budget. Check runs `langc check` on
the file. Build runs `langc build FILE -o OUT`. Run is `permitd` on the
module with N `tools/call` lines for `inspect` on `prod` on its input
(u2-probe.sh:43), and `build/fakedeploy LOG` as the child. All rows run
in the base span. Each cell gives the wall time and the peak resident
memory, the larger of the two `probe/guard.py` peaks (u2-probe.log:290,
:294, :300, :410, :414, :420). An earlier run with one nested `szS`
literal 1000 deep gave PARSE_DEPTH, thus the probe uses a chain.

| Step | N = 100 | N = 1000 |
| --- | --- | --- |
| Check (`langc check FILE`) | 0.026 s, 13.3 MB | 0.050 s, 18.0 MB |
| Build (`langc build FILE -o OUT`) | 0.023 s, 14.0 MB | 0.051 s, 18.7 MB |
| Run (`permitd MODULE -- build/fakedeploy LOG`, N calls) | 0.026 s, 2.9 MB | 0.356 s, 5.8 MB |

- At N = 100 each call gives a result `ok inspect` with `isError` false,
  and the log of the child has 100 lines (u2-probe.log:300, :700, :704).
- At N = 1000, 256 calls give a result, and 744 calls give error -32603
  `permit-lang too many pending calls` (u2-probe.log:420, :701-702). The
  log of the child has 256 lines (u2-probe.log:705). `permitd` keeps at
  most 256 sent calls with no answer (`PENDING_MAX`,
  `host/permitd.c:45`), and above that it gives this error
  (`host/permitd.c:838`). The probe writes the 1000 lines with no wait,
  thus the input is faster than the answers of the child. A client must
  wait for answers and keep at most 256 calls open. The probe did not
  measure a client that waits at N = 1000.
- No result of either run has `"isError":true` (u2-probe.log:703; `rg -c`
  prints no count for 0 matches).
- The modules are 9773 and 16973 bytes (u2-probe.log:689-690), and the
  module of `test/host/deploy.lang` alone is 9019 bytes
  (u2-probe.log:687). `start` uses `kN`, thus the build lowers the chain.
- A chain of N `Size` definitions alone checks in 0.027 s at 13.2 MB for
  N = 100 and in 0.027 s at 18.1 MB for N = 1000 (u2-probe.log:406,
  :682).

## 7. Parametric families

A program cannot declare a family: `test/check/data.lang` gives
REFUSE_DATA `Coin: a program cannot declare a family (rule R1); only
domain/domain.lang can` and exit 1 (u2-probe.log:200), and a program with
a family `Box` gives REFUSE_DATA for `Box` (u2-probe.log:204). In
`domain/domain.lang`, `Trunc1` (:42) and `Trunc0` (:48) take a type
parameter. `Pos` (:163) and `Word` (:170) take a `Size` index, with the
type `Size -> Type 0`. The constructor `trunc1` takes its type as an
explicit argument: with the type left out it gives TYPE_ARITY `trunc1
needs 2 arguments, found 1` (u2-probe.log:208), and with the type it
checks `ok` (u2-probe.log:212). A constructor of an index family appears
in a term: `def p : Pos (szS sz0) := posFirst sz0` (u2-probe.sh:34)
checks `ok` (u2-probe.log:216). `examples/quot.lang` and
`examples/trunc.lang` check `ok` with `--kit` (u2-probe.log:220, :224).
F15 stays PARTIAL: only the domain file declares an indexed family
(`formers/tcc-wasm.md:28`). That row cites `domain/domain.lang:329`, a
comment line in this repo. `Pos` is at `domain/domain.lang:163`. Probe
files: the step files of u2-probe.sh:31-34 and `examples/trunc.lang`.

## 8. Sigma projection and eta

The projections check: the projection step (u2-probe.sh:35, `witness`
and `payload` of a `pack`) checks `ok` in the base span
(u2-probe.log:228), and `examples/sigma.lang` checks `ok` with `--kit`
(u2-probe.log:48). A Sigma pattern does not parse: a `fun` with the
pattern `(n, p)` (u2-probe.sh:36) gives PARSE_EXPECT `expected a binder
name, found '('` (u2-probe.log:232). The kernel accepts Sigma eta: the
eta step (u2-probe.sh:37) proves the path from `pack (witness s)
(payload s)` to `s` with `refl` and checks `ok` (u2-probe.log:236). F11
(Sigma) is DONE (`formers/tcc-wasm.md:24`).

## 9. Equality

A program writes equality as `Path A x y`, with `refl` and `cong` (F12
and F13, both DONE, `formers/tcc-wasm.md:25-26`). `transport`, `J` and
`trans` are definitions in `domain/domain.lang:7`, :13 and :21. A program
that uses `transport` checks `ok` (u2-probe.log:240), and so does a
program that uses `trans` (u2-probe.log:244). `S1`
(`domain/domain.lang:37`) is a higher inductive family with `loop (i :
I)`. `examples/paths.lang`, `examples/equality.lang` and
`examples/circle.lang` check `ok` with `--kit` (u2-probe.log:248, :252,
:256). The circle check takes 0.139 s at 189.8 MB (u2-probe.log:256).
`trunc1` takes its type as an argument (section 7), thus one family
serves each index type.

## 10. Axiom detection

No command lists the axioms of a file (the usage, u2-probe.log:15-31).
`langc check FILE` prints only `ok` for a file with no axiom
(u2-probe.log:156). `langc check --json FILE` prints one JSON object. For
a one-definition probe input with no axiom, it prints
`{"diagnostics":[],"progress":{"laws_discharged":0,"laws_total":0,"parsed":1,"typed":1}}`
and exits 0 (u2-probe.log:260). For `test/check/axiom.lang` with `--kit`,
it prints one diagnostic REFUSE_AXIOM `magic: an axiom has no proof (rule
R2)` with the span 1:1 to 1:18, and exits 1 (u2-probe.log:264). A plain
`check` of that file gives REFUSE_AXIOM and exit 1 (u2-probe.log:268).
`langc explain REFUSE_AXIOM` prints the text of the code
(u2-probe.log:116). Thus a program with an axiom does not check, and R2
holds at the check.

## Consequences for M0

1. A program is one file. The domain names need no import (section 2).
2. No `def rec`, and no call of the definition itself or of a later one:
   recursion is `fold` and `unfold` (section 3, R3).
3. A program with a kit type (`Nat`) runs with `langc --kit`, as the gate
   does when `examples/kit.txt` or `test/check/kit.txt` lists it. The base
   span refuses it with REFUSE_KIT (the base-span paragraph, section 4).
4. A `Nat` is at most 2^64 - 1. `natAdd` gives `none` above that value,
   `natSub` stops at 0, and a literal 0 divisor does not check. Thus a
   driver reads the `Option` of `natAdd`, not a trap (section 4).
5. One build writes one wasm module, which exports `decide`. `report` and
   `cedar` print the other outputs: the binds as JSON and the Cedar
   permit policies (sections 1 and 5).
6. A program cannot declare a family, thus F15 stays PARTIAL (section 7).
7. A check or a build at N = 1000 costs below 0.06 s and 19 MB. A chain
   that `start` uses adds bytes to the module (section 6).
8. An axiom stops the check with REFUSE_AXIOM and exit 1 (section 10, R2).
9. `permitd` is the run step. It sends each permitted call to the child
   and keeps at most 256 calls with no answer. Above that it gives error
   -32603 `permit-lang too many pending calls`, thus a client waits for
   answers (sections 1 and 6).

## Guard rows

One row for each step of run 4, from `u2-probe.sum`. The log line is the
`## NAME: CMD` line of the step. MB is the larger of the two guard peaks.

| Log line | Step | rc | s | MB |
| --- | --- | --- | --- | --- |
| 11 | `make` | 0 | 0.026 | 1.9 |
| 15 | `usage` | 2 | 0.027 | 1.4 |
| 32 | `check-deploy` | 0 | 0.027 | 12.9 |
| 36 | `eval-deploy` | 0 | 0.027 | 13.0 |
| 40 | `check-cap` | 0 | 0.027 | 12.6 |
| 44 | `check-gate` | 0 | 0.048 | 13.1 |
| 48 | `check-sigma` | 0 | 0.027 | 11.6 |
| 52 | `check-self` | 1 | 0.023 | 11.7 |
| 56 | `check-later` | 1 | 0.027 | 11.6 |
| 60 | `check-rec` | 1 | 0.025 | 11.6 |
| 64 | `self-base` | 1 | 0.025 | 12.7 |
| 68 | `natT-base` | 1 | 0.027 | 11.6 |
| 72 | `ir-deploy` | 0 | 0.024 | 13.4 |
| 78 | `abi-deploy` | 0 | 0.025 | 13.2 |
| 83 | `build-deploy` | 0 | 0.026 | 13.3 |
| 86 | `valid-deploy` | 0 | 0.027 | 2.5 |
| 89 | `build-deploy-rt` | 0 | 0.024 | 13.3 |
| 92 | `report-deploy` | 0 | 0.027 | 13.2 |
| 96 | `cedar-deploy` | 0 | 0.024 | 13.3 |
| 102 | `fmt-sigma` | 0 | 0.023 | 2.0 |
| 105 | `fmt-deploy` | 0 | 0.026 | 1.8 |
| 108 | `expl-kit` | 0 | 0.027 | 1.4 |
| 112 | `expl-rec` | 0 | 0.024 | 1.6 |
| 116 | `expl-axiom` | 0 | 0.025 | 1.4 |
| 120 | `base` | 0 | 0.022 | 1.4 |
| 126 | `law1` | 0 | 0.025 | 1.4 |
| 130 | `mcp` | 0 | 0.027 | 1.6 |
| 135 | `two-files` | 2 | 0.024 | 1.4 |
| 152 | `import` | 1 | 0.024 | 3.5 |
| 156 | `one` | 0 | 0.027 | 12.6 |
| 160 | `nat-sub` | 0 | 0.027 | 11.7 |
| 164 | `nat-big` | 0 | 0.026 | 12.6 |
| 168 | `nat-sum` | 0 | 0.026 | 12.6 |
| 172 | `nat-over` | 0 | 0.026 | 12.6 |
| 176 | `nat-div` | 0 | 0.027 | 12.6 |
| 180 | `nat-divz` | 1 | 0.026 | 12.7 |
| 184 | `nat-modz` | 1 | 0.027 | 12.6 |
| 188 | `nat-mul` | 1 | 0.022 | 12.6 |
| 192 | `nat-rec` | 1 | 0.026 | 12.6 |
| 196 | `nat-range` | 1 | 0.023 | 3.5 |
| 200 | `data` | 1 | 0.027 | 11.6 |
| 204 | `box` | 1 | 0.027 | 12.6 |
| 208 | `trunc-noarg` | 1 | 0.022 | 12.7 |
| 212 | `trunc-arg` | 0 | 0.026 | 12.7 |
| 216 | `pos` | 0 | 0.025 | 12.9 |
| 220 | `quot` | 0 | 0.026 | 16.9 |
| 224 | `trunc-ex` | 0 | 0.027 | 34.6 |
| 228 | `proj` | 0 | 0.027 | 12.9 |
| 232 | `pat` | 1 | 0.027 | 3.5 |
| 236 | `eta` | 0 | 0.027 | 12.7 |
| 240 | `transport` | 0 | 0.025 | 12.7 |
| 244 | `trans` | 0 | 0.027 | 12.6 |
| 248 | `paths` | 0 | 0.027 | 13.0 |
| 252 | `equality` | 0 | 0.025 | 11.9 |
| 256 | `circle` | 0 | 0.139 | 189.8 |
| 260 | `json-one` | 0 | 0.027 | 12.6 |
| 264 | `json-axiom` | 1 | 0.027 | 11.7 |
| 268 | `check-axiom` | 1 | 0.027 | 12.6 |
| 272 | `sha256` | 0 | 0.028 | 2.3 |
| 276 | `jcs` | 0 | 0.025 | 1.7 |
| 280 | `host` | 0 | 0.398 | 5.3 |
| 284 | `permitd-usage` | 2 | 0.024 | 1.4 |
| 290 | `check100` | 0 | 0.026 | 13.3 |
| 294 | `build100` | 0 | 0.023 | 14.0 |
| 297 | `valid100` | 0 | 0.024 | 2.6 |
| 300 | `run100` | 0 | 0.026 | 2.9 |
| 406 | `chain100` | 0 | 0.027 | 13.2 |
| 410 | `check1000` | 0 | 0.05 | 18.0 |
| 414 | `build1000` | 0 | 0.051 | 18.7 |
| 417 | `valid1000` | 0 | 0.026 | 2.5 |
| 420 | `run1000` | 0 | 0.356 | 5.8 |
| 682 | `chain1000` | 0 | 0.027 | 18.1 |
