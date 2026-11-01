# permit-lang plan

permit-lang is a Cubical TT language for one job: executable capability
protocols for agents. An agent often needs a narrow operation, but the SaaS
credential behind the tool grants much more. A permit program states the
authority over a sequence of tool calls: the scoped resources, a step bound
(the expiry), attenuating delegation, consumable budgets, and approval that
is bound to the digest of the exact proposed mutation. The checker checks
these before a run. A gate that holds the credential enforces them again at
the tool boundary. No program can trap.

The design is `design/DESIGN.md`: the request, verbatim, the S0 rulings,
verbatim, and the overlap and prior-art facts with their sources. This file
gives the readings of that request, the route, the slices and the open
questions. Section 14 lists the questions for the USER. A reading that is
not ruled stays a recommendation.

Status: S0 DONE (seed 1e69513): the tcc-wasm kit and this plan. S1 DONE:
the USER took each recommendation of section 14 (section 1). Next: S2
(port the Cubical TT kernel, section 8.1).

## 1. Rulings

| Date | Ruling | Source |
|---|---|---|
| 2026-11-01 | Name: permit-lang, from the pasted description. | DESIGN.md 1 |
| 2026-11-01 | Home: "New repo, zkpolicy later (Recommended)". permit-lang is a new repo from the lang-template tcc-wasm kit. zkpolicy-lang is an optional later target (section 8.4). | DESIGN.md 2, 3.1 |
| 2026-11-01 | Targets: "MCP gate: Wasm + C host" (T1), "JSON authority report" (T2), "Cedar export" (T3). Not chosen: "C99 enforcer library". | DESIGN.md 2 |
| 2026-11-01 | Approval: "Digest, abstract hash (Recommended)". An approval carries the digest of the canonical (target, payload) bytes. The language has one opaque hash with laws. v1 binds SHA-256. The C host records approvals from an approver channel (a CLI prompt) and refuses a call whose digest is different. | DESIGN.md 2 |
| 2026-11-01 | Time: "Operation counts only". No time at all. Expiry is a call-count bound. Not chosen: host ticks, both. | DESIGN.md 2 |
| 2026-11-01 | Host: TinyCC. All of `langc` is C99 that TinyCC builds, `langc mcp` too. The T1 gate is C99 that TinyCC builds. Implied by A4, not asked. | DESIGN.md 1, 2 |
| 2026-11-01 | Q-S0-1 Wasm engine: "(a) Own C99 interp (Recommended)". The gate runs `decide` in an own interpreter for the MVP subset that `langc` emits, in C99 that TinyCC builds. node and wasmtime are differential oracles in the gate. Copy the relay-lang engine when it exists. | USER, S1 |
| 2026-11-01 | Q-S0-2 Kernel source: "(a) cairn kernel, no crypto (Recommended)". S2 ports the Cubical TT kernel that cairn-lang took from intent-lang, without the crypto code. Edit: section 8.1 cites this ruling. | USER, S1 |
| 2026-11-01 | Q-S0-3 Resource form: "(a) Segment path (Recommended)". A `Resource` is a path of segments (for example `deploy/prod/web`). `within` is prefix containment by segments. The gate interns the segments. There is no wildcard. Edit: section 4, the `Resource` row. | USER, S1 |
| 2026-11-01 | Q-S0-4 Canonical bytes: "(a) RFC 8785 JCS (Recommended)". The gate makes the RFC 8785 (JCS) form of the call arguments and forwards those bytes unchanged. Review clarification: the approval digest binds the effective target and the exact canonical tool call in the envelope of section 4, to preserve the S0 approval ruling. | USER, S1; review clarification |
| 2026-11-01 | Q-S0-5 Binding: "(a) `bind` in program (Recommended)". A `bind` declaration in the permit program gives the tool name, the `Action` and the JSON pointer of the target. `langc` compiles it into the module: one source. | USER, S1 |
| 2026-11-01 | Q-S0-6 Steps: "(a) Each decided call (Recommended)". Each call that the gate decides (permit or deny) takes one step, so an agent cannot probe without end. | USER, S1 |
| 2026-11-01 | Q-S0-7 Upstream failure: "(a) Consume on forward (Recommended)". A permitted `execute` uses its approval, its step and its cost when the gate forwards it, whatever the upstream result (at most once). Edit: section 2, the item on an upstream failure. | USER, S1 |
| 2026-11-01 | Q-S0-8 Gate state: "(a) SQLite file (Recommended)". One SQLite file holds the used approvals, the steps and the budget left, and the trace. A restart cannot make a used approval good again. | USER, S1 |
| 2026-11-01 | Q-S0-9 Clock refusal name: "(a) REFUSE_TIME (Recommended)". The code is `REFUSE_TIME`, with the cairn-lang word list plus `ttl`, `deadline` and `expires`. Edit: section 7, the line on the refusal names. | USER, S1 |
| 2026-11-01 | Q-S0-10 Token comparison: "(a) cairn-lang rulings (Recommended)". S16 uses the Anthropic `count_tokens` endpoint for the newest Claude model (the model id and the date go in the report). The Python and Rust programs run, and their outputs are equal to the permit-lang output. | USER, S1 |
| 2026-11-01 | Q-S0-11 Which calls need an approval: "(a) needsApproval mark (Recommended)". The program marks each `Action` that needs an approval (`needsApproval execute`). A call of a marked action without a matching approval is `deny noApproval`. | USER, S1 |

The seed is `bin/new-lang.sh permit-lang tcc-wasm` from lang-template
5265a2a. The kit gives the parser, the checker, the evaluator, the lowering
and the Wasm writer. No kit emits a C host, a JSON report or Cedar text, so
the T1 gate, the T2 writer and the T3 writer are new code.

Carried default, not ruled for permit-lang: proofs are guest-language terms
only (quorum-lang ruling, 2026-10-25). Lean export is not a target.

## 2. The reading of R0: no trap

R0: no program can trap or surface an exception, on any target.

The reading is the accrue-lang reading (accrue-lang d1017f1 docs/PLAN.md
lines 160 to 185) with the relay-lang host rule N6 (relay-lang seed 145f58f
docs/PLAN.md section 2). Each operation is total on its declared type. Each
run of each target gives a value of the result type. A denial is a value,
never an error:

- A call outside the scope: `deny scopeMiss`.
- The step bound is at 0: `deny boundSpent`.
- The budget is smaller than the cost: `deny budgetShort`.
- An execute with no approval: `deny noApproval`.
- An approval whose digest is not the digest of the call: `deny digestDiffers`.
- An approval that the gate saw before: `deny approvalUsed`.
- A delegation that widens the parent: `deny widen` at run time, and
  `REFUSE_DELEGATE_WIDEN` at check time when the checker can see it.
- A request from the agent that the gate cannot read: `deny malformed`.
- The upstream tool fails (a socket error, an HTTP 5xx): the gate gives the
  `Outcome` value `toolFailed code`. The failure never reaches the program
  as an error. The gate used the approval, the step and the cost of the
  call when it forwarded the call, so they stay used (Q-S0-7 (a): at most
  once).

| Rule | Statement | Where |
|---|---|---|
| N1 | Each partial operation has an `Option` result or a proof argument. The kit `natAdd` and `natMul` trap on overflow; permit-lang gives them `Option` forms. `spend` gives `none` when the budget is too small, and `decide` maps that to `deny budgetShort`. The step counter cannot go below 0, because `decide` reads 0 as `deny boundSpent`. | kernel types, eval, each writer |
| N2 | No recursion at run time, no run-time closure, no run-time tree fold (the kit HOST-LIMITs `REFUSE_RUNTIME_CLOSURE`, `REFUSE_RUNTIME_TREE`). No target can exhaust its stack. | kit, `REFUSE_REC` |
| N3 | Each run-time list has a capacity that the program fixes: the scope, the set of used approvals of one run. An operation past the capacity gives `none`, and `decide` gives a deny. Memory is static: no `memory.grow`. The kit `alloc` trap at 1 GiB goes. | lowering, Wasm writer |
| N4 | The writers emit no trap form. Wasm: no `unreachable` (the kit lazy-trap strict point goes, because the result of `decide` is a `Decision`, never a trap), no `br_table` default that traps, each `div` and `rem` has a divisor that the type proves is not zero. The T2 and T3 writers are total text writers over a checked program. | writers, `test/notrap/` |
| N5 | `langc eval` never aborts on a checked program. | eval |
| N6 | A host error never reaches the program. The gate maps each host failure (a socket error, an upstream status, a malformed agent request, a closed approver channel) to a `Decision` or an `Outcome` value. The gate code has no `abort` and no `assert`. | T1 gate, `test/notrap/`, `test/host/` |

New refusal code `REFUSE_PARTIAL`: a program uses a partial kit word
without its `Option` form or its proof argument. The fix hint names the
total form.

Gate for R0: a C tool `test/notraptool.c` (port from relay-lang 5c386bb)
reads each built module and fails on each trap opcode and on each unguarded
`div` or `rem`. The kit gate row "a trap is equal to a trap"
(`docs/host/README.md` line 172) changes at S3: no checked program gives a
trap, so the row becomes "no row gives a trap".

## 3. The reading of R1

R1: the type constructors are the cell monads and their duals, as
represented computationally in Cubical TT.

permit-lang takes the accrue-lang reading without change (accrue-lang
d1017f1 docs/PLAN.md lines 186 to 214):

- The ambient structure is not limited: Pi, Sigma, `Path`, `PathP`, the
  interval `I`, `transp`, `hcomp`, the universe `U`, `Glue` and `ua`.
- A declared type constructor must be one of two classes:
  - CM, cell monad: the free algebra monad of a cell signature. The
    parameter occurs only in the generator `eta : A -> T A`. Each other
    cell takes closed base data and a family of arguments of type `T A`,
    indexed by a closed base type (its arity). Unit is `eta`, bind is the
    recursor, and the laws are paths by induction.
  - CC, cocell comonad: the cofree coalgebra comonad of a cocell
    signature. One observation `eps : D A -> A`, each other field `D A ->
    D A` or `D A -> E -> D A`. Counit is `eps`, extend is the corecursor.
- `REFUSE_CELL` refuses each other parameterized declaration. The cons
  list is refused. The free monoid `FreeMon` is the admitted list.

The duality is the core of permit-lang. The agent side is a CM, and the
gate side is its dual CC:

- The plan of the agent is a term of `Plan A`, the free monad over the tool
  signature. Each tool call is a cell: `inspect`, `propose`, `execute`.
- The authority is an `Allowance`, the cofree comonad of `Call -> -`.
  `eps` gives the authority that is left now (the scope, the steps, the
  budget, the used approvals). `advance : Allowance -> Call -> Allowance`
  gives the authority after one call. This is the accrue-lang `advance`
  shape with a `Call` in place of a `Stamp`.
- The record of a run is a `Trace`, the cofree comonad over the record
  signature. `eps` gives the current entry (the call, the decision, the
  steps and the budget left, the digest that the gate checked). The T2
  report is a fold over a finite prefix of the `Trace`.
- Authority over a sequence: the pairing runs a `Plan` against an
  `Allowance` and gives a `Trace`. The checker shows, on each path of the
  plan, that each `execute` pairs with an approval of the same mutation
  (L5).

| Former | Class | Reason |
|---|---|---|
| `Plan A` | CM | Free over the cells `inspect r k`, `propose m k`, `execute m a k` (closed data: `Resource`, `Mutation`, `Approval`; arity: `Outcome`). `eta` is `done`. |
| `Allowance` | CC | Cofree over `Call -> -`: `eps` = the authority now, `advance` = the authority after one call. The dual of `Plan`. |
| `Trace` | CC | Cofree over the record signature: `eps` = the current entry, `next : Trace -> Trace`. |
| `Outcome R` | CM | `eta` = `done`, cells `denied reason` and `toolFailed code` (closed base data). The exception monad as a value. |
| `Option A` | CM | `eta` = `some`, one cell `none` with no data. |
| `FreeMon A` | CM | The admitted list: the scope entries, the used approvals of one run. |
| `Capability`, `Mutation`, `Call`, `Approval`, a user `Action` sum, `Decision`, `Reason`, `Prod`, `Sum` | LCC structure | Finite products and coproducts. No parameter, so not a type constructor. |
| cons `List`, `Ref`, `IO`, `Promise`, a general JSON value type, an exception type with a `throw` form | REFUSED | Not a CM or CC (`List`, `Ref`, JSON), or a control path that R0 refuses (`throw`). `IO` and `Promise` are replaced by `Plan`. |

## 4. The minimal base signature (R2)

R2: the base types and the operations are the minimum that pasted 1 needs.
Each admitted name cites the line it serves. The kit base data is `Nat` and
`Flag`.

The lines of pasted 1 (DESIGN.md 1):

- P1 "An agent may need a narrow operation while the underlying SaaS
  credential grants much broader access."
- P2 "Describe authority over a sequence of actions"
- P3 "scoped resources"
- P4 "expiration"
- P5 "delegation"
- P6 "consumable budgets"
- P7 "approval tied to an exact proposed mutation"
- P8 "An agent can inspect a deployment and propose a change, but can
  execute only the exact approved change."
- P9 "Modifying the target or payload invalidates that approval."
- P10 "enforcing authority at the actual tool boundary"

| Name | Kind | Line | Note |
|---|---|---|---|
| `Resource` | opaque base type; `resEq`, `within : Resource -> Resource -> Flag` | P3 | A path of segments (for example `deploy/prod/web`). `within` is prefix containment by segments, with no wildcard. The gate interns the segments. A program never parses a string (Q-S0-3 (a)). |
| `Action` | a user sum, declared in each program | P2, P8 | For example `inspect`, `propose`, `execute`. |
| `needsApproval` | a mark on an `Action` | P7, P8 | Q-S0-11. |
| `Scope` | `FreeMon (Resource * Action)`; `allows`, `scopeWithin` | P3, P5 | Finite. `scopeWithin child parent` is decidable. |
| `StepBound` | a `Nat` count of calls left | P4 | Expiry under the time ruling (section 5). |
| `Budget` | a `Nat` count of units, affine; `spend`, `split` | P6 | Section 6. `spend` and `split` give an `Option`. |
| `Capability` | record: scope, steps, budget | P1, P5 | A resource type, because it holds a `Budget`. |
| `delegate` | `Capability -o Scope -> StepBound -> Nat -> Option (Capability * Capability)` | P5 | Attenuation only: the child scope is within the parent scope, the child steps are not more than the parent steps, and the child budget is split from the parent budget (L1). |
| `Payload` | opaque canonical tool-call bytes; equality and `canon` input only | P7, P9, P10 | The gate canonicalizes the tool name, arguments and each forwarded call parameter before it constructs a `Payload`. Raw JSON spellings are not payload values. |
| `Mutation` | record: target `Resource`, `Action`, `Payload` | P7, P9 | |
| `canon` | `Mutation -> Bytes`: the canonical bytes of (target, payload) | P9 | Q-S0-4; the target envelope below. |
| `Digest`, `hash` | opaque; `digestEq`; `hash : Bytes -> Digest` with laws only | P7, P9 | v1 binds SHA-256 (ruling). |
| `Approval` | record: one `Digest`; affine | P7 | Section 6. The gate records who approved; the program does not read it. |
| `Call` | record: `Mutation`, cost `Nat`, an optional `Approval` | P2, P10 | One request from the agent. |
| `Decision`, `Reason` | `permit` or `deny Reason`; `Reason` is the closed sum of section 2 | P3 to P9 | A denial is a value (R0). |
| `decide` | `Allowance -> Call -> Decision * Allowance` | P10 | The one entry of the T1 module. |

Review clarification of Q-S0-4 (a) under the S0 approval ruling (L3):

- Both the approver channel and the call gate validate the arguments under
  [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785) and make their JCS
  form. A `Payload` holds the canonical upstream tool-call object, including
  its tool name, arguments and each forwarded call parameter. Invalid input,
  including duplicate object keys or invalid Unicode, gives `deny malformed`.
  White space and object key order do not distinguish payload values.
- The gate extracts the effective target through the compiled `bind`, from
  those canonical arguments. It does not trust a separate target from the
  agent. `canon m` is the JCS form of the object with two fields: `target`
  is the ordered array of resource segment strings, and `payload` is the
  canonical tool-call object. The target uses segment strings, never the
  gate's intern ids, so its bytes are stable across a restart.
- The gate hashes this whole envelope. It forwards the canonical tool-call
  object, with the same bytes that occur in the envelope's
  `payload` field. The envelope is not an extra upstream argument.
  Hashing the arguments alone would lose the effective target when two
  bindings select different JSON pointers in the same object. The tool name
  also stays bound: identical arguments for different tools are different
  payloads, even if their effective targets and actions are equal.
- L3 is injectivity on target and canonical payload values. It is not
  injectivity on raw JSON text: for the same tool, arguments `{"x":1}` and
  `{ "x": 1 }` give the same payload. S7 checks this equality, a changed
  canonical payload, and equal payloads with different targets.
  S9 checks D10 and D11 below and that the forwarded bytes equal the payload
  bytes. It also checks that the canonical argument bytes stay unchanged.

Refused, with the reason:

| Name | Reason |
|---|---|
| floats, reals | No line needs them. A cost is a count of units. |
| string operations, regex, a `like` pattern | `Resource` gives containment by segments. No line needs a pattern. |
| a JSON value type, a JSON path in a guard | A payload is opaque. The program never parses it. The gate finds the target with the binding (Q-S0-5). |
| a clock, a date, a duration, a timer | Ruling: operation counts only (section 5). |
| random numbers | No line needs them. |
| threads, async | The gate decides one call at a time. |
| HTTP, sockets, files | The gate does all IO (N6). |
| free attribute conditions (Cedar style) | Pasted 1: ordinary authorization rules alone are not enough. T3 exports only the static scope. |
| signatures, keys | The approval comes from the approver channel of the gate (ruling). A signed approval is O10. |
| an approver id, roles, groups, revocation lists | No line needs the program to read them. The T2 report records the approver from the gate. |
| integers below zero | No line needs them. |

## 5. Time

The ruling: "Operation counts only". The reading is the cairn-lang reading
(cairn-lang 60768e8 docs/PLAN.md lines 235 to 248): a bound is a count of
later operations, never a clock value.

- Expiry is the `StepBound` of a capability. `eps` of the `Allowance` gives
  the steps left. Each call that the gate decides takes one step (Q-S0-6).
  The steps never increase (L4). At 0, each call gives `deny boundSpent`.
- New refusal code `REFUSE_TIME`: a program names a clock word (`now`,
  `clock`, `time`, `timestamp`, `date`, `epoch`, `ttl`, `deadline`,
  `expires`). The fix hint names `StepBound` (Q-S0-9).
- The gate never reads a clock to make a decision. The T2 report has no
  time field in v1.

## 6. The affine layer

The layer is the leaf-lang layer (leaf-lang 5ccc494 docs/PLAN.md section
4.1, lines 561 to 600): multiplicities 0, 1 and omega on binders, the QTT
context split, weakening allowed, contraction refused, and
`REFUSE_AFFINE_REUSE` ("used 2, allowed 1").

- Resource sorts: `Budget` and `Approval`. `Capability` holds a `Budget`,
  so it is a resource type.
- `spend : Budget -o Nat -> Option Budget` uses the budget once and gives
  the rest. `split : Budget -o Nat -> Option (Budget * Budget)` gives two
  budgets whose sum is the input (L2). A copy of a budget is
  `REFUSE_AFFINE_REUSE`.
- `execute` uses its `Approval` once. A second use is
  `REFUSE_AFFINE_REUSE`.
- Weakening: a program can drop a budget or an approval. A dropped budget
  wastes authority and never widens it.
- The gate checks again at run time, because the agent is not trusted and
  can send any bytes. The gate keeps the used approvals and the steps and
  budget left (Q-S0-8). A second use of an approval is `deny approvalUsed`.
  The static check helps the author; the run-time check is the enforcement.

## 7. Laws

Each law is a guest-language term with a `Path` type. A law on closed rows
is `refl`; a general law uses induction where the kernel has it.
`langc check --json` counts each law in `laws_discharged` and `laws_total`.

| Law | Statement |
|---|---|
| L1 attenuation | `delegate` gives a child with `scopeWithin child parent`, child steps not more than parent steps, and child budget plus rest budget equal to the parent budget. A widening gives `none`. |
| L2 conservation | `split b n` gives budgets with sum `b`. `spend b n` gives `rest` with `rest + n = b`. |
| L3 approval binding | `decide` permits an `execute` of `m` with approval `a` only when `digestEq (digestOf a) (hash (canon m))`. `canon` is injective on (target, canonical payload) values as defined in section 4. The binding then rests on the collision resistance of `hash`, which is an assumption, not a law (O5). |
| L4 steps monotone | Along `advance`, the steps never increase. Each decided call takes one step until 0. At 0, each decision is `deny boundSpent`. |
| L5 sequence guard | On each path of a `Plan`, each `execute m` has an approval of `hash (canon m)` in scope, and a `propose m` before it. The checker refuses a path without them: `REFUSE_EXECUTE_UNAPPROVED`. |
| L6 scope soundness | `decide` permits a call only when `allows scope target action`. |
| L7 at most once | After a permitted `execute` with approval `a`, each later call with `a` gives `deny approvalUsed`. |
| L8 agreement | `langc eval` of `decide` and the Wasm module in the gate give the same `Decision` on each row. This is a gate row, not a law term. |

Static refusals: `REFUSE_EXECUTE_UNAPPROVED` (L5), `REFUSE_DELEGATE_WIDEN`
(L1, when the scopes are literals), `REFUSE_BUDGET_OVERSPEND` (the sum of
the literal costs on a path is more than the literal budget),
`REFUSE_AFFINE_REUSE` (section 6), `REFUSE_TIME` (section 5),
`REFUSE_PARTIAL` (section 2). S1 ruled the name `REFUSE_TIME` (Q-S0-9
(a)). The other names stay proposals until the slice that adds the code.

## 8. Route

### 8.1 Kernel

Port the Cubical TT kernel that cairn-lang took from intent-lang, without
the crypto code (Q-S0-2 (a), ruled at S1, as relay-lang ruled on
2026-10-27). Add the leaf-lang affine layer (section 6). S2 surveys the
candidates and records the hash.

### 8.2 The three targets

| Target | Code | Output | Check |
|---|---|---|---|
| T1 MCP gate | `src/wasm.c` (kit) writes one `.wasm` for each program, with the export `decide`. New `host/` in C99 that TinyCC builds: the gate `build/permitd`. It sits between the agent and the upstream MCP server (a child process on stdio), holds the credential, makes the canonical bytes of each call, hashes them (SHA-256), reads approvals from the approver channel, runs `decide` in its Wasm engine (Q-S0-1), and forwards only a permitted call. | one `.wasm` for each program, and `build/permitd` | node and wasmtime run each module as differential oracles (L8); the demo rows in `test/host/` |
| T2 JSON authority report | `src/report.c`: the static summary from `langc` (scope, steps, budgets, laws), and one line for each call from the gate (the call, the decision, the reason code, the steps and the budget left, the digest that the gate checked). Schema in `docs/REPORT-JSON.md`. | JSON text | byte-equal rows in `test/report/` |
| T3 Cedar export | `src/cedar.c`: the static scope as Cedar `permit` policies. Steps, budgets, the sequence guard and the approval binding have no Cedar form. The export writes one comment for each part that it leaves out. | `.cedar` text | golden files in `test/cedar/` (O3) |

`langc mcp` (A4) is a different program from `permitd`. `langc mcp` is the
authoring server for agents that write permit-lang (check, fmt, explain).
`permitd` is the enforcement gate between an agent and a real tool.

### 8.3 The demo

A fake deployment tool `host/fakedeploy.c` (C99, TinyCC; state in one
file) has three tools: `inspect`, `propose` and `execute`. The agent gets
the permit program, never the credential. The rows:

| Row | Call | Want |
|---|---|---|
| D1 | `inspect` of a resource in scope | permit |
| D2 | `propose` of a change | permit |
| D3 | `execute` of the exact approved change | permit, one time |
| D4 | `execute` with a changed target | `deny digestDiffers` |
| D5 | `execute` with a changed payload | `deny digestDiffers` |
| D6 | a second `execute` with the same approval | `deny approvalUsed` |
| D7 | a call when the steps are at 0 | `deny boundSpent` |
| D8 | a call outside the scope | `deny scopeMiss` |
| D9 | a sub-agent with a delegated capability asks for a wider scope | `deny widen` |
| D10 | two tools bind `execute` with target pointers `/left` and `/right`; both targets are in scope, both calls have `{"left":"prod","right":"dev"}`, and an unused approval is for the first call | the second call is `deny digestDiffers`: the effective target changed from `prod` to `dev` even though the arguments did not |
| D11 | tools `restart` and `delete` both bind `execute` at `/target`, have identical arguments `{"target":"prod"}`, and an unused approval is for `restart` | `delete` is `deny digestDiffers`: the tool name changed even though the target, action and arguments did not |

### 8.4 Later target: zkpolicy-lang

A later slice can prove in zero knowledge that a permit decision passed,
with zkpolicy-lang (ruling "New repo, zkpolicy later"). It is not in v1
(O6).

## 9. Agent adoption (A1 to A6)

The request asks for six items so that agents reach for permit-lang when it
fits. Each item has one deliverable, one slice and one gate.

| Item | Pasted line | Deliverable | Slice | Gate |
|---|---|---|---|---|
| A1 | "one canonical form per operation and a langc fmt canonical printer" | No aliases, no second syntax. `langc fmt FILE` prints the canonical form. `langc fmt --check` exits 1 when a file is not canonical. | S12 | `fmt` is idempotent; `parse (fmt x)` has the same core term as `x`; each example and test file is canonical. |
| A2 | "langc check --json, giving fix hints and progress counts (parsed, typed, laws discharged)" | One JSON object: `diagnostics` (code, span, expected, found, `fix`: a concrete edit) and `progress` (`parsed`, `typed`, `affine_checked`, `laws_discharged`, `laws_total`). Schema in `docs/CHECK-JSON.md`. | S13 | Rows in `test/json/`, byte equal. Each refusal code of section 7 has a `fix` row. |
| A3 | "a SKILL.md with 3 to 5 graded examples" | `skills/permit-lang/SKILL.md` with five graded examples in `examples/skill/`: G1 a read-only scope with a step bound, G2 a budget with costs and a split, G3 an attenuated delegation to a sub-agent, G4 propose and execute of one approved mutation, G5 the demo. SKILL.md quotes each file. | S15 | The gate checks each example and checks that each SKILL.md block and its file are byte equal. |
| A4 | "an MCP server written in TinyCC (langc mcp)" | An MCP server over stdio (JSON-RPC 2.0, one message on each line), in C, in the same `langc`. Tools: `check`, `fmt`, `eval`, `build` (with a target), `explain` (a refusal code), `list-base` (section 4), `show-law` (section 7). The MCP spec version is pinned in `docs/MCP.md`. | S14 | A C tool `test/mcptool.c` sends `test/mcp/*.in` and compares the replies with `*.out`. |
| A5 | "AGENTS.md and a README table saying when to use the language" | `AGENTS.md` (the gate command, the layout, the rules) and the README table "When to reach for permit-lang". Both exist at S0, and S15 makes them final. | S0, S15 | Review only. |
| A6 | "a token-count comparison against Python and Rust" | G1 to G5 and the demo in permit-lang, Python and Rust, with equal outputs. Report `docs/TOKEN-BASELINE.md`: the tokenizer pin, each count, each file hash (Q-S0-10). | S16 | `make tokens`, not in `make check`. |

## 10. Tests

The kit layout stays: `test/parse`, `test/check`, `test/eval`, `test/ir`,
`test/fronttool.c`, `test/gate.sh`. New: `test/notrap/` and
`test/notraptool.c` (R0), `test/affine/` (section 6), `test/host/` (the
gate in stdio mode with the fake deployment tool, rows D1 to D11),
`test/report/` (T2), `test/cedar/` (T3), `test/json/` (A2), `test/mcp/` and
`test/mcptool.c` (A4), `test/fmt/` (A1). The SHA-256 rows use a C tool
ported from cairn-lang `test/shatool.c`. New tests are guest `.lang` files
and C tools. No new node, Python or shell harness; the kit harnesses stay.
Mutation: each slice that adds a refusal or a law kills at least one mutant
of it.

## 11. Port sources

Pinned on 2026-11-01. A slice that ports from a repo records the hash that
it read.

| Repo | Hash | What | Where |
|---|---|---|---|
| lang-template | 5265a2a | the tcc-wasm kit, `bin/new-lang.sh` | whole kit |
| relay-lang | 5c386bb (seed 145f58f) | R0 rules N1 to N6, `test/notraptool.c`, the Wasm engine ruling, the C host shape | seed docs/PLAN.md sections 2 and 12 (Q-S0-1); docs/PLAN.md line 51 (S1 ruling) |
| accrue-lang | d1017f1 | R0 and R1 readings, the A1 to A6 table, `REFUSE_PARTIAL` | docs/PLAN.md lines 160 to 214, 374 to 397 |
| cairn-lang | 60768e8 | the no-time reading and `REFUSE_TIME`; the kernel source; `test/shatool.c` | docs/PLAN.md lines 235 to 248 |
| leaf-lang | 5ccc494 | the affine layer and `REFUSE_AFFINE_REUSE`; the dash skip for DESIGN.md | docs/PLAN.md lines 561 to 600; test/gate.sh line 889 |
| mandate-lang | 33556e9 | the budget law (`spent` does not decrease, `spent <= cap`); the `autoLimit` confirmation rule as a reading for Q-S0-11 | docs/PLAN.md line 583; lines 410 to 415 |
| zkpolicy-lang | 1d263fb | overlap only (DESIGN.md 3.1); the later target | none in v1 |

The overlap note in DESIGN.md 3 names mandate-lang d368d5f. Its HEAD is
33556e9 on 2026-11-01.

## 12. Slices

One session for each slice: scope, gate, mutants, docs, close.

| Slice | Work | Status |
|---|---|---|
| S0 | Seed from tcc-wasm, DESIGN.md, this plan, README table, AGENTS.md; gate EXIT 0 (9 parse refusals, 19 check refusals, 32 evals, 10 lowered, 13 Wasm modules valid, 813 differential cases, 0 mismatches, review and domain regressions passed); seed commit; private repo | DONE |
| S1 | The USER rules section 14. Record the rulings here. Each question takes (a) (section 1). Reading edits in sections 2, 4, 7 and 8.1; review clarifies the approval bytes and adds D10 and D11 (sections 4, 7, 8.3 and 10). No code change; gate EXIT 0 with the S0 counts. | DONE |
| S2 | Port the Cubical TT kernel (8.1, Q-S0-2). Survey first. | NEXT |
| S3 | R0 in the kernel and the Wasm writer: N1 to N5, `REFUSE_PARTIAL`, `test/notraptool.c`, no `unreachable`, static memory; the kit trap row (O2). | |
| S4 | The affine layer (section 6): multiplicities, `REFUSE_AFFINE_REUSE`, the resource sorts. | |
| S5 | `Resource`, `Action`, `Scope`, `StepBound`, `Allowance` (CC), `Decision`, `Reason`; `REFUSE_TIME`; L4, L6. | |
| S6 | `Budget`, `spend`, `split`, `Capability`, `delegate`; `REFUSE_DELEGATE_WIDEN`, `REFUSE_BUDGET_OVERSPEND`; L1, L2. | |
| S7 | `Payload`, `Mutation`, `canon`, `Digest`, `hash`, `Approval`, `Call`, `Plan` (CM), `Trace` (CC), `decide`; `REFUSE_EXECUTE_UNAPPROVED`; L3, L5, L7. | |
| S8 | T1 part 1: the Wasm module of `decide`, the binding (Q-S0-5), the notrap rows. | |
| S9 | T1 part 2: `permitd`: the Wasm engine, SHA-256, the canonical bytes, the approver channel, the state, the stdio MCP proxy, `fakedeploy`, N6; the demo D1 to D11; L8. | |
| S10 | T2: the JSON authority report, `docs/REPORT-JSON.md`. | |
| S11 | T3: the Cedar export, the golden files. | |
| S12 | A1: canonical forms and `langc fmt`. | |
| S13 | A2: `langc check --json`. | |
| S14 | A4: `langc mcp`. | |
| S15 | A3 and A5: SKILL.md, G1 to G5, AGENTS.md and the README table final. | |
| S16 | A6: the token comparison. | |
| S17 | Close: SPEC.md, probe/CAPABILITY.md, VALIDATION.md, final gate. | |

## 13. Open items

- O1 The kit base data is `Nat` and `Flag`. Section 4 adds names only with
  a pasted line. A slice that needs more records why here first.
- O2 The kit `docs/host/README.md` "Lazy traps" section (line 120) and the
  gate row "a trap is equal to a trap" (line 172) stay until S3. S3
  rewrites them.
- O3 The cedar CLI is not on PATH. T3 is checked by golden files only. A
  `cedar validate` row needs a USER install.
- O4 The link to the thread in pasted 1 did not survive the paste. The
  README states the problem without the link until the USER gives it.
- O5 L3 rests on the collision resistance of `hash`. That is an
  assumption, not a law. v1 binds SHA-256.
- O6 zkpolicy-lang is a later target (8.4), not v1.
- O7 The pinned MCP spec version and the JSON-RPC subset go in
  `docs/MCP.md`: the `langc mcp` part at S14, the `permitd` proxy part at
  S9.
- O8 The `fix` of each refusal code (A2) is one concrete edit. A code with
  no edit gives `fix: null` and a reason.
- O9 The approver channel is a CLI prompt on the terminal. The gate rows
  have no terminal, so they use a file of approvals (test mode).
- O10 A signed approval (an approver key) is not in v1.
- O11 The gate reads the credential from a file or an environment variable
  that its flags name, and gives it only to the upstream child. A run on a
  real SaaS tool is a USER-run check.

## 14. Questions for the USER (S0)

Each question has a recommendation (rec). S1 records the rulings: each
question takes (a) (section 1).

- Q-S0-1 Wasm engine in the gate. (a) rec: take the relay-lang ruling: an
  own interpreter for the MVP subset that `langc` emits, in C99 that
  TinyCC builds. node and wasmtime are differential oracles in the gate.
  Copy the relay-lang engine when it exists. (b) Vendor wasm3: a full
  engine, but a large C tree that the repo then owns. (c) The wasmtime C
  API: complete, but a Rust library that TinyCC links only through a
  prebuilt binary.
  RULED: (a), S1.
- Q-S0-2 Kernel source. (a) rec: port the Cubical TT kernel that
  cairn-lang took from intent-lang, without the crypto code (the
  relay-lang ruling). (b) Port the cairn-lang kernel whole. (c) Extend the
  kit checker to Cubical TT in place.
  RULED: (a), S1.
- Q-S0-3 Resource form. (a) rec: a path of segments (for example
  `deploy/prod/web`); `within` is prefix containment by segments; the gate
  interns the segments; no wildcard. (b) Flat opaque ids with equality
  only; a scope lists each resource.
  RULED: (a), S1.
- Q-S0-4 Canonical bytes. (a) rec: the gate makes the RFC 8785 (JCS) form
  of the call arguments and forwards those bytes unchanged. The hash binds
  the target and that exact canonical tool call (section 4, review
  clarification), so a change of binding cannot hide a changed target or
  tool. (b) Raw bytes as the agent sent
  them: a change of white space is a new digest.
  RULED: (a), S1.
- Q-S0-5 Binding of a tool call to (target, action, payload). (a) rec: a
  `bind` declaration in the permit program (the tool name, the `Action`,
  the JSON pointer of the target), compiled into the module: one source.
  (b) A separate host config file.
  RULED: (a), S1.
- Q-S0-6 Steps. (a) rec: each call that the gate decides (permit or deny)
  takes one step, so an agent cannot probe without end. (b) Only a
  permitted call takes a step.
  RULED: (a), S1.
- Q-S0-7 Upstream failure. (a) rec: a permitted `execute` uses its
  approval, its step and its cost when the gate forwards it, whatever the
  upstream result. The gate cannot know if the change was applied, so at
  most once is the safe side. (b) Give them back on an upstream refusal
  before apply (this needs a signal from each tool).
  RULED: (a), S1.
- Q-S0-8 Gate state. (a) rec: one SQLite file (the used approvals, the
  steps and the budget left, the trace), so a restart cannot make a used
  approval good again. (b) Memory only, for one session.
  RULED: (a), S1.
- Q-S0-9 Clock refusal name. (a) rec: `REFUSE_TIME` with the cairn-lang
  word list plus `ttl`, `deadline` and `expires`. (b) `REFUSE_CLOCK`, the
  accrue-lang and relay-lang name.
  RULED: (a), S1.
- Q-S0-10 Token comparison (A6). (a) rec: take the cairn-lang rulings: the
  Anthropic `count_tokens` endpoint for the newest Claude model (model id
  and date in the report), and the Python and Rust programs run with
  outputs equal to the permit-lang output. (b) A pinned offline
  tokenizer, text only.
  RULED: (a), S1.
- Q-S0-11 Which calls need an approval. (a) rec: the program marks each
  `Action` that needs an approval (`needsApproval execute`); a call of a
  marked action without a matching approval is `deny noApproval`. (b)
  Each call of a mutating action needs one. (c) The mandate-lang
  `autoLimit` reading: a call needs one only when its cost is above a
  limit.
  RULED: (a), S1.
