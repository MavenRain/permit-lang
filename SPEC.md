# permit-lang specification

Status: closed at S17 (2026-11-05). M0 to M2 and the agent work are DONE,
and M3 is PARTIAL (section 10). `permit-lang` is a working name.

`docs/PLAN.md` holds the rulings and the slices. `docs/VALIDATION.md`
holds the evidence.

## 1. Purpose

permit-lang is a Cubical TT language for one job: executable capability
protocols for agents at the tool boundary (`design/DESIGN.md` section 1).
A program gives the authority of an agent over a sequence of tool calls:
scoped resources, a step bound in place of an expiry, delegation,
consumable budgets and an approval that is tied to the digest of one exact
proposed mutation. The checker refuses a delegation that widens a scope, a
spend that is more than the budget, and an execute that has no earlier
approved propose of the same mutation. No program can trap (R0, PLAN
section 2, :206).

The first demo (PLAN section 8.3): an agent can inspect a deployment and
propose a change, but it can execute only the exact approved change. A
change of the target or of the payload makes the approval invalid
(`deny digestDiffers`).

A law is a term of a `Path` type that the kernel checks (PLAN section 7).
The compiler `langc` checks the program, its types and its laws, and then
writes the targets.

The host is TinyCC (`PIN`). The S0 rulings (`design/DESIGN.md` section 2)
give three targets (PLAN section 8.2, :646):

- T1, the MCP gate: one Wasm module for each program (`langc build PROG
  -o OUT`) and the C host `build/permitd`. The gate sits between the agent
  and the real tool. It holds the broad credential and forwards only the
  calls that the program permits.
- T2, the JSON authority report (`langc report PROG` and `permitd --report
  FILE`, `docs/REPORT-JSON.md`).
- T3, the Cedar export of the static scope (`langc cedar PROG`,
  `docs/CEDAR.md`).

`src/main.c` :55 to :71 gives the 12 commands, and :73 to :88 gives the
usage. There is no `run` command and no `prove` command.

Its type formers are F1 to F15 of `formers/FORMERS.md`. Its core data types
and core operations are only the types and operations of
`design/DESIGN.md` (the design). The design holds the request and the S0
rulings, verbatim. PLAN section 4 gives the readings of the design as
names, so sections 5 and 6 cite PLAN sections.

The overlap ruling (`design/DESIGN.md` section 2): permit-lang is a new
repo, and zkpolicy-lang is an optional later target (PLAN section 8.4,
O6). Cedar rules alone are not enough: T3 exports only the static scope.

The tools for agents are in PLAN section 9 (A1 to A6, :687):

- `langc fmt` prints the one canonical form of a program (A1, S12).
- `langc check --json` gives the diagnostics with fix hints and the
  progress counts `parsed`, `typed`, `laws_discharged` and `laws_total`
  (A2, S13, `docs/CHECK-JSON.md`).
- `langc mcp`, an MCP server that TinyCC builds, has 7 tools. The verbs
  `langc explain`, `langc base` and `langc law` also serve agents (A4,
  S14, `docs/MCP.md`).
- `skills/permit-lang/SKILL.md` has the graded examples G1 to G5.
  `AGENTS.md` and the README table say when to use the language (A3, A5,
  S15).
- `docs/TOKEN-BASELINE.md` compares the tokens of G1 to G5 in permit-lang,
  Python and Rust (A6, S16).

`langc mcp` serves the author of a program. `permitd` enforces the program
at run time.

## 2. Programs

A program is a sequence of declarations and definitions. A definition has
this form:

```
def NAME : TYPE := TERM
```

The permit forms are these (PLAN section 4, :370):

- `res "a/b"`: a `Resource` literal, a chain of segments. A bad path gives
  `REFUSE_RES`.
- `bind "tool" action "/pointer"`: a top-level line. It gives the `Action`
  of one tool and the JSON pointer of the target in the call arguments
  (Q-S8-4 (a)). A bad line gives `REFUSE_BIND`.
- `def start : Authority`: the start authority of the gate, with a closed
  `StepBound`. `langc build` refuses a `bind` with no `start`
  (`REFUSE_LOWER`).
- `def NAME : Plan A := TERM`: a plan of tool calls (section 4).

The compiler refuses these host forms in a program (`formers/tcc-wasm.md`
:48 to :50; `test/check/expect.txt` gives the code of each mutant):

- R1 `test/check/data.lang`: a program declares a data type.
- R2 `test/check/axiom.lang`: a program adds an unproved fact.
- R3 `test/check/rec.lang`: general recursion. Recursion comes only from
  `fold` and `unfold` (F6, F7).
- R4 `test/check/core-name.lang`: a definition uses a core name.

`src/front/codes.c` :10 to :37 has 26 codes. `langc explain CODE` prints
the meaning and the README section of each. `langc check --json` gives
the diagnostic's fix when one is available. Each code is a refusal:

- The permit codes: `REFUSE_DELEGATE_WIDEN`, `REFUSE_BUDGET_OVERSPEND`
  and `REFUSE_EXECUTE_UNAPPROVED` (PLAN section 7), `REFUSE_TIME` (PLAN
  section 5, 10 clock words), `REFUSE_RES`, `REFUSE_BIND` and
  `REFUSE_REPORT`.
- The affine codes of the S4 port (PLAN section 6): `REFUSE_AFFINE_REUSE`,
  `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and `REFUSE_AFFINE_PATH`.
- The R0 code `REFUSE_PARTIAL` (S3).
- The kernel and kit codes: `REFUSE_AXIOM`, `REFUSE_CELL`, `REFUSE_DATA`,
  `REFUSE_HISTORY_WRITE`, `REFUSE_KIT`, `REFUSE_LAW`, `REFUSE_LOWER`,
  `REFUSE_MAP`, `REFUSE_NAME`, `REFUSE_REC`, `REFUSE_SIG` and
  `REFUSE_STATE`.
- The lowering codes `REFUSE_RUNTIME_CLOSURE` and `REFUSE_RUNTIME_TREE`:
  the HOST-LIMITs of F9 and F6 (`formers/tcc-wasm.md` :37, :31).

The lexer and parser codes `LEX_CHAR`, `LEX_NAT_RANGE`, `LEX_STRING`,
`PARSE_DEPTH` and `PARSE_EXPECT` are not in the table (`src/front/lexer.c`
:196). `test/check/expect.txt` has 236 rows. Each row gives the code of
one file of `test/check/`.

A denial at run time is not a refusal. It is a value: `deny r` with one
`Reason` r (section 5, R0).

The compiler writes the targets. A program cannot.

## 3. Type formers

The type formers are F1 to F15 of `formers/FORMERS.md`. This language uses
the tcc-wasm column of the realization matrix. `formers/tcc-wasm.md` gives
the host form of each former. F1 to F4 (F4 is `List`), F11 and F13 are DONE
with no limit. The table gives only the other formers.

| ID | Status on tcc-wasm | Effect on this language | Open item |
|---|---|---|---|
| F5 | DONE over Option, Sum E and List | The guest `FreeMon` gives `fmBindNat` and `fmMapNat` (section 4) | none |
| F6 | DONE over Nat, List and domain families. HOST-LIMIT: a run-time tree | `REFUSE_RUNTIME_TREE` when a tree exists only at run time. `Hop` and `Echo` (`domain/domain.lang` :270) have no fold on Wasm (`test/ir/hop.lang`) | none |
| F7 | DONE into List with a Nat step limit | none | none |
| F8 | DONE over Option and List | none | none |
| F9 | DONE. HOST-LIMIT: a function value at run time | `REFUSE_RUNTIME_CLOSURE`; a lambda stays only as the direct argument of an eliminator | none |
| F10 | DONE over `Type 0` and over Nat | none | none |
| F12 | DONE. `trans` is a guest def by `hcomp` (`domain/domain.lang` :21) | A path erases on Wasm | none |
| F14 | DONE for Type 0 and Type 1, without cumulativity | none | none |
| F15 | PARTIAL: only the domain file declares an indexed family | A program cannot declare a family (R1). The permit families take a type parameter only, for example `Plan` (`domain/domain.lang` :413) | none |

`docs/host/CAPABILITY.md` gives the HOST-LIMIT reasons. Two kit cites of
`formers/tcc-wasm.md` do not fit this repo: the F9 note (:40) names
`docs/CAPABILITY.md`, which this repo does not have, and the F15 row (:28)
cites `domain/domain.lang:329`, which is a comment line here.

## 4. Structures

- **Monad**: `pure`, `map`, `bind` on Option, Sum E and List (F5). The
  guest free monoid `FreeMon` (`domain/domain.lang` :112) has `fmFold` (:130),
  `fmBindNat` (:139) and `fmMapNat` (:147). A plan is a `Plan A` (:413), the cell
  monad (CM) of PLAN section 3 (:273): the free monad over the three tool
  cells `planInspect`, `planPropose` and `planExecute`. It is first order,
  with one branch for each `Reply` (:407).
- **Algebra**: `fold` on Nat, List and the domain families (F6). `fmFold`
  folds a `FreeMon` into a set. `allows`, `scopeWithin` and `usedHas`
  (:326, :331, :368) count the matches with `fmBindNat` and `len`, because the
  domain cannot prove the `Flag` monoid laws. `unfold` makes a List with a
  Nat step limit (F7). The cofamily `Stream` (:63) has `sExtend` (:67)
  and `sDuplicate` (:70). The `Trace` is a `Stream` of `Entry` (:422, :419), a
  cocell comonad (CC) of PLAN section 3. The affine cofamily `Allowance`
  (:336) is the other CC: `eps` is the authority now, and `advance` gives
  the allowance after one ticket. `alOpen` (:340) gives the two as one pair.
- **Filterable**: `filter` on Option and List (F8).

The host does not give these carriers: `FreeMon`, `Stream`, `Size`
(:56), `Plan`, `Allowance` and `Trace`. The domain file declares each of them.

## 5. Core types

The PLAN section 4 table (:358 to :377) is the source. The kit base data
`Nat` and `Flag` stay (O1). A resource type (affine) holds a `Budget` or
an `Approval` (PLAN section 6, :441).

| Type | Meaning (PLAN section) | Definition |
|---|---|---|
| `Resource` | A scoped resource: a chain of segments (4, :358) | A kernel sort, copyable. Kit-only `resRoot`, `resChild`, `resEq` and `within`; the guest literal `res "a/b"` |
| `Action` | `inspect`, `propose` or `execute` (4, :359) | `domain/domain.lang` :276. Only `execute` needs an approval (`needsApproval`, :360, O21) |
| `Scope` | The grants of a capability (4, :361) | `domain/domain.lang` :321: `FreeMon (Prod Resource Action)` |
| `StepBound` | The expiry as a count of later calls (4, :362; 5) | `domain/domain.lang` :290: `Size`. A tick is `sizePred`, and it stops at `sz0` |
| `Budget` | A consumable budget (4, :363; 6) | A kernel sort, affine. Kit-only `budget` and `budgetLeft`; guest `spend` and `split` |
| `Capability` | A scope, a step bound and a budget (4, :364) | `domain/domain.lang` :349. A resource type, because it holds a `Budget` |
| `Payload` | The canonical tool-call bytes (4, :366) | A kernel sort, copyable, opaque. Kit-only `payloadOf`; guest `payloadEq` |
| `Mutation` | A target, an action and a payload (4, :367) | `domain/domain.lang` :294 |
| `Bytes`, `Digest` | The canonical bytes and their digest (4, :369) | Kernel sorts, copyable, opaque. Guest `hash : Bytes -> Digest`, `envelope` and `digestEq`. v1 binds SHA-256 in the gate |
| `Approval` | An approval over one digest (4, :371; 6) | The S4 kernel sort, affine. Kit-only `approval : Digest -> Approval`; guest `approvalDigest : Approval -o Digest` |
| `Call` | One request from the agent (4, :372) | `domain/domain.lang` :304. A resource type, because it holds an `Approval` |
| `Ticket` | The copyable record of one call (4, :373) | `domain/domain.lang` :300 |
| `Decision`, `Reason` | `permit`, or `deny` with one of 8 reasons (4, :374; 2) | `domain/domain.lang` :287, :278: `scopeMiss`, `boundSpent`, `budgetShort`, `noApproval`, `digestDiffers`, `approvalUsed`, `widen`, `malformed` |
| `Authority` | The copyable state of the allowance: scope, steps, units left, used digests (4, :377) | `domain/domain.lang` :361 |
| `Allowance` | The affine CC over tickets (4, :375; 3) | `domain/domain.lang` :336 |
| `Reply`, `Plan` | The reply of the host, and a plan of tool calls, the CM (3) | `domain/domain.lang` :407, :413 |
| `Entry`, `Trace` | One decided call, and the trace of a run, a CC (3) | `domain/domain.lang` :419, :422 |
| `Size` | The step index (5) | `domain/domain.lang` :56: `sz0`, `szS` |

## 6. Core operations

The guest operations are a model of the gate. The laws hold on the model.
The C host `build/permitd` runs the real gate (section 7). L3 rests on the
collision resistance of `hash`, an assumption (O5).

| Operation | Definition | Meaning (PLAN section) |
|---|---|---|
| `canon` | `domain/domain.lang` :296 | The bytes that an approval signs: the envelope of the target and the payload (4, :368) |
| `ticketWith`, `someDigest`, `callTicket` | `domain/domain.lang` :306, :309, :311 | A call gives its ticket and uses the approval one time (4, :373; 6) |
| `actEq`, `grants`, `allows`, `scopeWithin` | `domain/domain.lang` :314, :323, :326, :331 | L6: a permit only when the scope allows the target and the action (7) |
| `spend`, `split` | guest builtins on `Budget` | L2: the budget is conserved (6, 7) |
| `sizeLe`, `delegate` | `domain/domain.lang` :344, :355 | L1: the child scope is within the parent scope, the child steps are at most the parent steps, and the budgets sum. A widening gives `none` (4, :365; 7) |
| `stepLeft`, `usedHas`, `auTake` | `domain/domain.lang` :364, :368, :373 | L4: the steps never increase. L7: a used approval gives `deny approvalUsed` (7) |
| `judge`, `judgeMalformed` | `domain/domain.lang` :383, :388 | The decision on a ticket. The denials come in the order `scopeMiss`, `boundSpent`, `budgetShort`, `noApproval`, `digestDiffers`, `approvalUsed`. A malformed call takes one step (2, 4) |
| `allowanceOf`, `decideTicket`, `decide` | `domain/domain.lang` :393, :399, :402 | L3: a permit of an execute only when the approval digest is `hash (canon m)`. `decide` is the one entry of the T1 module (4, :376; 8.2) |
| the plan check | the checker | L5: each `planExecute m` is in the `ppApproved` branch of an earlier `planPropose m` (`REFUSE_EXECUTE_UNAPPROVED`, 7) |

Laws (PLAN section 7, :582): L1 to L4, L6 and L7 are paths that the kernel
checks, with eval rows in `examples/capability.lang`, `examples/scope.lang`
and `examples/decide.lang`. L5 is a check row
(`test/check/execute-unapproved-none.lang`, `-other.lang` and
`-refused.lang`). L8 is a gate row, not a law term: `langc eval` of
`decide` and the Wasm module give the same `Decision` on each row.

The C host parts are in `host/`: `permitd.c` (the stdio MCP gate, the
approver channel, the state and the cost), `engine.c` (the Wasm engine),
`wasmread.c` (the module reader), `sha256.c` (SHA-256), `jsonin.c` (the
JSON input), `jcs.c` (the RFC 8785 canonical bytes, the envelope and the
RFC 6901 pointer) and `report.c` (the `--report` lines). The demo (PLAN
section 8.3, :658) runs `permitd` in front of `test/fakedeploy.c`, a fake
deployment tool with 7 tools:

- D1 to D3: inspect, propose, and execute the exact approved change give
  `permit`. The execute gives `permit` one time.
- D4 and D5: a changed target or a changed payload gives `deny
  digestDiffers`.
- D6: a second execute with the same approval gives `deny approvalUsed`.
- D7: a call with 0 steps left gives `deny boundSpent`.
- D8: a call outside the scope gives `deny scopeMiss`.
- D9: a sub-agent that widens a closed scope gets `REFUSE_DELEGATE_WIDEN`
  from the checker. There is no run-time row (O22).
- D10 and D11: the same arguments at a different pointer or on a different
  tool give `deny digestDiffers`.

D1 to D8, D10 and D11 are rows of `test/hosttool.c` on
`test/host/deploy.lang`. D9 is `test/check/delegate-widen.lang`.

## 7. Target and instance encoding

The compiler has one front end and three writers (PLAN section 8.2):

- `src/wasm.c` writes one `.wasm` for each program. The module exports
  `decide` (the target, the action, the payload, the cost, the digest flag
  and the digest, each an i64, to an i64), `malformed` and the memory.
  `build/permitd` runs it (`docs/host/README.md`). The usage is
  `permitd [--state FILE] [--report FILE] [--approvals FILE | --approver
  tty] [--cost TOOL=N]... [--cred-file PATH | --cred-env NAME] [--cred-var
  NAME] MODULE -- CMD ARGS` (`host/permitd.c` :971 to :973).
- `src/report.c` writes the `program` line of the T2 report (`langc report
  PROG`). `host/report.c` adds a `call` line or a `malformed` line for
  each call (`permitd --report FILE`). `docs/REPORT-JSON.md` gives the JSON
  Lines schema.
- `src/cedar.c` writes the static scope of `def start` as Cedar `permit`
  policies, one for each grant, and 6 "Not exported" comments (`langc
  cedar PROG`, `docs/CEDAR.md`). The budgets, the step bounds, the
  sequences and the approval binding have no Cedar form. They stay in the
  gate. No cedar CLI is on PATH (O3).

An instance is one run of the gate on one module. The gate keeps the used
approvals, the steps and the budget, and checks them again at run time
(PLAN section 6). `docs/CHECK-JSON.md` gives the JSON line of `langc check
--json`.

| Type | Encoding |
|---|---|
| `Payload` | The RFC 8785 (JCS) form of the upstream tool-call object: the tool name, the arguments and each forwarded call parameter. Invalid input gives `deny malformed` (PLAN section 4) |
| `Resource` | The target that the `bind` pointer (RFC 6901) selects in the call arguments (O23) |
| `canon`, `Digest` | `canon` gives the envelope bytes of the target and the payload; `Digest` is their SHA-256 (`host/jcs.c`, `host/sha256.c`) |
| `Approval` | A digest that the approver channel records (`--approvals FILE` or `--approver tty`, O9) |
| `Budget` | The units left in the gate state. `--cost TOOL=N` gives the cost of a tool |
| `Decision` | The i64 result of `decide`. The gate forwards a call only on `permit` |

Types, universes, proof contents and paths erase on Wasm. Each op is total,
and a value is one word (`formers/tcc-wasm.md` :10, F12).

R0 (PLAN section 2) holds on the output. `test/notraptool.c` checks each
Wasm module: no `unreachable` and static memory (S3). `auUsed` holds at
most k digests for the closed `StepBound` k of `start`, so the memory is
fixed (N3).

## 8. Host and target

The host is TinyCC. The repo has a `PIN` file (7 lines). It records `tcc
-v` 0.9.28rc (mob@0fb54300, AArch64 Darwin) and its path, `node`
v23.10.0, and `wasm-validate` and `wasm-objdump` 1.0.39. The gate uses node
and the wabt tools from `/opt/homebrew/bin`. `PIN` also records the kit
host check `make -C hosts/tcc-wasm check` GREEN on 2026-10-08. This repo
has no `hosts/` directory. No make target reads `PIN`.

The kernel is the relay-lang 05dd63b port (S2, PLAN section 8.1, :621):
cairn-lang d076765 with the intent-lang 67445ae kernel. R0 comes from
relay-lang 5c386bb (S3), and the affine layer from leaf-lang 47a6f36 (S4).

The targets are T1 (the Wasm module and `build/permitd`), T2 (the report)
and T3 (Cedar) (section 7). `docs/host/CAPABILITY.md` holds the host facts
and the HOST-LIMIT reasons. `probe/CAPABILITY.md` holds the host probe
(S17). The host facts of the design come from the former rows F1 to F15 of
`formers/tcc-wasm.md` (section 3). `permitd` and `langc mcp` pin the MCP
version 2025-11-25 (`docs/MCP.md` :9, O7).

The checks on the output are in `make check` (`Makefile` :113): `sh
test/gate.sh` and `sh test/review.sh`. `Makefile` :36 gives the builds:
`build/langc`, `build/permitd`, `build/fakedeploy`, `build/tokcount` and
the test tools `build/fronttool`, `build/pathtool`, `build/notraptool`,
`build/shatool`, `build/enginetool`, `build/jcstool`, `build/hosttool`,
`build/reporttool`, `build/mcptool` and `build/skilltool`.

## 9. Open items

The open items are the O rows of `docs/PLAN.md` section 13 (:869, O1 to
O25). Each row gives its slice and its ruling. 8 rows are CLOSED: O2,
O12, O13, O14, O15, O17, O18 and O19.

These 7 rows stay OPEN at the close (Q-S17-3 (a), as cairn-lang O37). No
slice owns them, and the plan has no slice after S17:

- O3. The cedar CLI is not on PATH. The 7 `test/cedar/` rows check T3
  (:877).
- O20. `REFUSE_BUDGET_OVERSPEND` checks one closed term. There is no cost
  sum over the paths of a `Plan`. At run time, `judge` gives `deny
  budgetShort` (:939).
- O21. `judge` keeps `execute` as the one marked action (:944).
- O22. D9 is a static row only. There is no run-time `deny widen`: the
  module has no child scope, and the proxy has no delegation (:948).
- O23. The target of a call comes from the bind pointer, with one bind
  line for each tool. A target-only row needs two modules (:953).
- O24. The report writes the `call` line before the forward, so it has no
  line for the upstream outcome (:960).
- O25. 2 mutants of the skill gate are LIVE: M1 and M3 (:965).

These 10 rows are standing facts or rules of the v1 scope (Q-S17-3 (a)):

- O1. The kit base data is `Nat` and `Flag`. PLAN section 4 adds a name
  only for a line of pasted 1 (:871).
- O4. The link to the thread in pasted 1 did not survive the paste
  (:880).
- O5. L3 rests on the collision resistance of `hash`, an assumption
  (:883).
- O6. zkpolicy-lang is a later target (PLAN section 8.4), not v1 (:886).
- O7. `docs/MCP.md` pins the MCP version and the JSON-RPC subset of
  `permitd` (S9) and of `langc mcp` (S14) (:888).
- O8. The `fix` of each refusal code (A2) is one concrete edit (:891).
- O9. The approver channel is a CLI prompt on the terminal (:894).
- O10. A signed approval (an approver key) is not in v1 (:897).
- O11. The gate reads the credential from a file or an environment
  variable that a flag names. It gives the credential only to the upstream
  child. A run on a real SaaS tool is a USER check (:899).
- O16. The S4 rule for the permit forms with no leaf rule is sound, but it
  refuses some sound programs (:922).

A change of `design/DESIGN.md` gets an item here (`design/DESIGN.md` :9).

## 10. Milestones

The plan maps its slices to the kit milestones by content (Q-S17-0 (a)).
PLAN section 12 (:844) gives each slice and its gate counts. The agent
slices S12 to S16 (A1 to A6) have their own row.

| Milestone | Slices | Content | Commits (date) | Status |
|---|---|---|---|---|
| M0 | S0 to S5 | Host probe (`probe/CAPABILITY.md`); the seed and the gate; the S1 rulings; the Cubical TT kernel from relay-lang 05dd63b; R0: N1 to N5, `REFUSE_PARTIAL`, `test/notraptool.c`; the affine layer from leaf-lang 47a6f36; `Resource`, `Action`, `Scope`, `StepBound`, `Allowance`, `Decision`, `Reason`, `REFUSE_TIME`; L4, L6 | 1e69513, 706b9ba, 3d47120, 18deea2, 505ca37 (2026-11-01); 6928c91 (2026-11-02) | DONE (S17 fills the probe file) |
| M1 | S6 and S7 | Core operations: `spend`, `split`, `Capability`, `delegate`, `REFUSE_DELEGATE_WIDEN`, `REFUSE_BUDGET_OVERSPEND`, L1, L2; `Payload`, `Mutation`, `canon`, `Digest`, `hash`, `Approval`, `Call`, `Plan`, `Trace`, `decide`, `REFUSE_EXECUTE_UNAPPROVED`, L3, L5, L7 | 97a1f2e, 08ed0a7 (2026-11-02) | DONE |
| M2 | S8 to S11 | Targets: the Wasm module of `decide` and the binding (S8); `permitd`, the demo D1 to D11 and L8 (S9); the JSON authority report (S10); the Cedar export (S11) | f308b6e, cce338a, f9d8a15, 65e9dbd (2026-11-03) | DONE |
| A | S12 to S16 | Agent work: `langc fmt` (A1), `langc check --json` (A2), SKILL.md, `AGENTS.md` and the README table (A3, A5), `langc mcp` and `langc explain` (A4), the token comparison (A6) | 3b55328, 9326cb2, dfb2539 (2026-11-04); 9ac759a, b7e1228 (2026-11-05) | DONE |
| M3 | none | Hardening: speed, limits, a checked certificate for the output | none | PARTIAL: the refusal codes, the mutant rows and the R0 check of `test/notraptool.c` give the limits; no speed work, no checked certificate |
| Close | S17 | SPEC.md, `probe/CAPABILITY.md`, `docs/VALIDATION.md`, the final gate | this commit (2026-11-05) | DONE |

Status 2026-11-05 (S17 closed): M0 to M2, the agent work and
the close (S17) are DONE, and M3 is PARTIAL. The plan is CLOSED.
