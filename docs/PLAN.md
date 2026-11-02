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
the USER took each recommendation of section 14 (section 1). S2 DONE: the
Cubical TT kernel, ported from relay-lang 05dd63b (section 8.1). S3 DONE:
R0 in the kernel and the Wasm writer, ported from relay-lang 5c386bb
(section 2, "S3 result"). S4 DONE: the affine layer, ported from
leaf-lang 47a6f36 (section 6, "S4 result"). Next: S5 (`Resource`,
`Scope`, `StepBound` and `Reason`, section 12).

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
| 2026-11-01 | Q-S2-1 Port source: "(a) relay-lang 05dd63b (Recommended)". Apply the relay-lang diff 145f58f to 05dd63b (195 paths, not docs/PLAN.md), keep the permit dash exclusion of test/gate.sh, and rename the relay lines to permit. Edit: section 8.1. | USER, S2 |
| 2026-11-01 | Q-S2-2 Cut decisions: "(a) Keep all cuts (Recommended)". No prove command, no `langc run`, no `event` declaration, no WORD, `U256` or `Addr`. permit-lang needs none of these parts (O1). | USER, S2 |
| 2026-11-01 | Q-S2-3 Lost gate lines: "(a) Accept until S3 (Recommended)". The lines of the IR, the Wasm modules and the differential cases go until S3. `langc build` gives `REFUSE_LOWER` "target pending S3". Edit: section 13, O12. | USER, S2 |
| 2026-11-01 | Q-S3-1 Port source: "(a) Port relay S7 (Recommended)". Port the relay-lang 5c386bb R0 and Wasm hunks, without the relay domain (relay S3 to S6) and `REFUSE_CLOCK`. Edit: section 2, "S3 result". | USER, S3 |
| 2026-11-01 | Q-S3-2 Lost gate lines (O12): "(b) Notrap rows only". Reading: S3 keeps the writer rows that run no node and no wasmtime (the ir rows, the build rows, `notrap modules`, `notrap bad rows`). The `differential cases` row and the `wasm runs` row run node, so they are out of the S3 gate and come back at S8. The S3 changes of `test/grid.awk` and `test/run-wasm.mjs` are in the repo, out of the gate. Edit: sections 12 and 13. | USER, S3 |
| 2026-11-01 | Q-S3-3 The kernel `state` form in the Wasm host: "(b) Lower as a plain family". Reading: `build`, `ir` and `abi` lower a `state` family as an ordinary family. Result: `langc build examples/state-no-history.lang` exits 0, and `wasm-validate` and `build/notraptool` accept the module, with no change to the writer. Its differential row waits for S8 (Q-S3-2 (b)). | USER, S3 |
| 2026-11-01 | Q-S4-1 Port route: "(a) Hand port, U1a+U1b (Recommended)". Port the TAKE hunks of leaf-lang 47a6f36 by hand at the permit sites, with the four codes `REFUSE_AFFINE_REUSE`, `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and `REFUSE_AFFINE_PATH` and ASCII messages that cite section 6. Part U1a is the multiplicity plumbing with no refusal. Part U1b is the counts, the refusals and the rows. No blob is shared, so `git apply` is not possible. Edit: section 6, "S4 result". | USER, S4 |
| 2026-11-01 | Q-S4-2 Resource sorts: "(a) Kernel Ops, no ctor (Recommended)". `Budget : Type 0` and `Approval : Type 0` are kernel Ops with no constructor, as the leaf `Leaf`. A program cannot make one. S6 and S7 add the ops (`spend`, `split`, the approval digest). `Capability` stays an S6 family in `domain/domain.lang`, and the family-field rule makes it a resource type. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-3 `unpack`: "(a) Take unpack at S4 (Recommended)". `unpack p (fun a b => t)` uses `p` one time and binds `a` and `b` at 1. `split` (S6) gives a pair of budgets, and `witness` and `payload` each use the pair, so a program needs `unpack` to use both budgets. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-4 Permit forms with no leaf rule: "(a) Sound fallback now (Recommended)". A part that can run more than one time (the step of `fold_block`, `elim` and `unfold`, the eta closure, the `filter` predicate, a Map op function) is checked at omega, so a 1-variable in it is `REFUSE_AFFINE_FIX` or `REFUSE_AFFINE_OMEGA`. The arms of `either` and finAll take the max, as `option`. `funExt` goes through the path rule. `first` and `second` use the pair. A precise rule comes only when a demo row needs one. Edit: section 6; section 13, O16. | USER, S4 |
| 2026-11-01 | Q-S4-5 A function type: (a+). A Pi is copyable (`RESOURCE_NONE`), and arm 2 of `either` is a case arrow. Each introduction that an eliminator can copy (`inl`, `inr`, `some`, `cons`, `pure`, a constructor field) checks its payload at omega when the payload type is copyable. Reason: before the ruling, a closure that captures a `Budget` could go through `inr` and run two times (a double spend). Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-6 A field of a type variable: "(a) Sound: 1, fix prelude". A field or an induction binder whose type is a type variable binds at 1, as in leaf. This is sound at each instantiation. The prelude changes: `fmFold` takes `f : A -o M`. Not chosen: a type variable at omega (a polymorphic fold that copies its field spends a `Budget` two times). Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-7 A copyable 1-variable: "(a) Copyable exempt". A use at omega does not refuse a 1-variable whose type is copyable. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-8 `fmBind` and the path rule: "(a) Mono B + path exempt". `fmFold` stays polymorphic and linear. The Q-S4-7 rule also covers `REFUSE_AFFINE_PATH`: a copyable 1-variable free under an interval binder is not refused. `fmBindNat`, `fmBindSize`, `fmMapNat` and `fmMapSize` replace `fmBind` and `fmMap`. `B` is `Nat` or `Size`, so `FreeMon B` is copyable. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-9 The examples with a polymorphic map: (a) mono. `map0` and `map1` are at `A = B = Nat` (`f : Nat -> Nat`). `truncExt` is at `T = Trunc0 Size`. `orbitNExt` has `T` fixed for each caller, and `orbitNExtBag` is new. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-10 The set proof of `fromAt`: (a). `isSetAtLin : (n : Size) -> isSetLin (OrbAt n)` is a `sel5` of the set proofs for each size (`isSetLinN k` and `isSetLinO`: etas of `isSetOrbitN k` and `isSetOrbit` at copyable types, Q-S4-7). No checker change. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-11 Erased definitions: "(a) keep leaf H". A definition of an erased type is checked at 0, as in leaf. A second check at 1 records its refusal (`runtime_refusal`), and a run-time reference to the definition gives that code. So `p : (@1 b : Budget) -> Path Budget b b := fun b => fun (i : I) => b` checks, and a non-linear law still cannot run. Not chosen: (b) erasure of universe types only; (c) no erasure. Edit: section 6. | USER, S4 |
| 2026-11-01 | Q-S4-12 A family with no parameters: (a). `resource_shape` stopped its field loop on a NULL env, but an empty env is NULL (a family without parameters). So `family Box := boxed (item : Budget)` was copyable, and `dup : Box -> Prod Box Box` checked ok. Fix: only a failed push stops the loop. Row F09 in `test/fronttool.c` wants `REFUSE_AFFINE_OMEGA`. Edit: section 6. | USER, S4 |

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

S3 result (2026-11-01). Port source: relay-lang 5c386bb "Implement
no-trap kernel arithmetic and Wasm emission (S7)" (tree be47f962), on the
S2 base relay-lang 05dd63b (Q-S3-1 (a)). Taken: the R0 hunks of the
kernel (`natAdd` gives `Option Nat`, the `NonZero Nat` divisor of
`natDiv` and `natMod`, `REFUSE_PARTIAL`), the lowering, the Wasm writer
(`src/wasm.c`, `src/wasmop.h`), `test/notraptool.c` and
`test/notrap/bad/`. Dropped: the relay domain of relay S3 to S6,
`REFUSE_CLOCK`, `langc sim`, the relay target refusals, and the
`reasonCode` row of `test/ir/natops.lang` (it folds a relay
`DeadReason`; rule R1, O15). The kit row "a trap is equal to a trap" is
gone (O2). At S8 the differential row comes back with the relay-lang
5c386bb text: a Wasm `trap` is a mismatch.

| Row | At S3 |
|---|---|
| N1 | Partly. `natAdd` gives `Option Nat`. `natDiv` and `natMod` need a `NonZero Nat` divisor. `natSub` stops at 0. The kernel has no `natMul` word, so there is nothing to change. `spend` and `decide` come at S6 and S7. |
| N2 | Covered. `REFUSE_REC`, `REFUSE_RUNTIME_CLOSURE` and `REFUSE_RUNTIME_TREE` are in the kernel and the lowering. |
| N3 | Partly. The heap bound is static and the memory has a fixed size: no `memory.grow`, minimum = maximum (notrap bad rows `memory-grow`, `no-max`, `max-above-min`). The per-list capacity of the scope and of the approvals comes at S5 to S7. |
| N4 | Covered for Wasm. No `unreachable`, a guard before each `div_u` and `rem_u`, and a `br_table` default that runs the last arm; `test/notraptool.c` and 7 bad rows. The T2 and T3 writers come at S10 and S11. |
| N5 | Covered. `langc eval` has no trap values and no trap flags. `EVAL_FUEL` is a host limit, not a trap. |
| `REFUSE_PARTIAL` | Covered. `partial_hint` names the total form for `head`, `tail`, `pred` and the partial kit forms; 4 check rows. |

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

S4 result (2026-11-01). Port source: leaf-lang 47a6f36 "S3: affine
layer, Durable and the sealed Keystore" (Q-S4-1 (a)). No src/front blob
is shared, so the port reads each leaf hunk and writes it at the permit
site. Taken: the marks `@0` and `@1` on binders, `A -o B` (a function
that uses its argument one time), the QTT context split, `unpack`
(Q-S4-3 (a)), `runtime_refusal`, and four codes: `REFUSE_AFFINE_REUSE`,
`REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and `REFUSE_AFFINE_PATH`. Each
message cites section 6. Dropped: the leaf `Leaf` sort, `Durable`,
`Stream`, the sealed `Keystore` (`REFUSE_SEALED`), the Agda clauses,
`langc fmt` (S12) and the `check --json` progress field (S13).

`Budget : Type 0` and `Approval : Type 0` are kernel sorts with no
constructor (Q-S4-2 (a)). A program cannot make one. `resource_shape`
finds the resource types: a type that holds a `Budget` or an `Approval`
in a pair, a sum, an option, a list or a family field. A function type
is copyable (Q-S4-5 (a+)). A type variable is not copyable, so a field
of a type variable binds at 1 (Q-S4-6 (a)). `resource_shape` also reads
the fields of a family with no parameters, so
`family Box := boxed (item : Budget)` is a resource type (Q-S4-12 (a);
fronttool row F09 gives `REFUSE_AFFINE_OMEGA`).
It follows recursive fields through sibling sorts too. A cycle stops
when it returns to a sort already on the current path.

The arms of a case give the max of their counts, not the sum: `option`,
`either`, `flagIf`, finAll, and `elim` on `Sum` and `Unit`. A fronttool
row has a fold step with an `option` case after `b` is used, and it
checks. A part that can run more than one time is checked at omega
(Q-S4-4 (a)):

- the step of `fold` and `fold_block`, and a recursive case of
  `elim_on_family`: a 1-variable in it is `REFUSE_AFFINE_FIX`;
- every case of a branching family fold or elimination, including a
  leaf case: two recursive children can repeat that leaf case;
- the step of `unfold`, the eta closure, the `filter` predicate, and the
  function of a List `map` or a List `bind`: a 1-variable in it is
  `REFUSE_AFFINE_OMEGA`;
- an Option `bind` and an Option or Sum `map` run their function at most
  one time, so they check it at the level of the term.

The KMap ops run no function for each entry, and `first` and `second`
use the pair one time, so they need no change.

Each introduction that an eliminator can copy (`inl`, `inr`, `some`,
`cons`, `pure`, either field of `pack` and a constructor field) checks its payload at omega when
the payload type is copyable (Q-S4-5 (a+)). A closure that captures a
`Budget` is copyable, so it cannot go into such a slot. The map element
test follows the same rule: when `map f` puts `f x` in a copyable type
and `x` has a resource type, `map` gives `REFUSE_AFFINE_OMEGA`. The test
reads the types only, so it also covers an `f` that is not a lambda. It
also refuses an `f` that drops its element (O16).

A copyable argument that can hold a closure checks at omega even at an
explicit 1-binder. A scalar result cannot retain a resource consumed to
compute it, so it still checks at 1. An Option or Sum `map` also checks
its function at omega when the result can hold a copyable closure.

An unknown payload can become copyable when instantiated. It can move
an affine input of that same type, as in `some x` or `some (f x)` with
`x : A` and `f : A -o A`. It cannot retain an affine input of a different
type. The same rule covers the captures and element of `map` when its
result type is unknown.

A 1-variable of a copyable type is not refused at omega (Q-S4-7 (a)) or
under an interval binder (Q-S4-8 (a)). In a fold step it is still
`REFUSE_AFFINE_FIX`.

A definition of an erased type (a universe or a path type) is checked at
0 (Q-S4-11 (a)). A second check at 1 records its refusal code in
`runtime_refusal`. A run-time reference to the definition gives that
code: `test/check/affine-path.lang` gives `REFUSE_AFFINE_PATH`.

The prelude and the examples follow the linear rules. `isSetLin A` is
the linear form of `isSet A`. `fmFold` takes `isSetLin M`,
`op : M -o M -o M` and `f : A -o M`. `fmBindNat`, `fmBindSize`,
`fmMapNat` and `fmMapSize` replace `fmBind` and `fmMap`. `fmEtaNat` and
`fmEtaSize` give `f a` to the `FreeMon` eta at a copyable type
(Q-S4-8 (a)). `map0` and `map1` are at `Nat`, `truncExt` is at
`Trunc0 Size`, and `orbitNExtBag` is new (Q-S4-9 (a)). `isSetAtLin`
gives the set proof of `fromAt` (Q-S4-10 (a)). The eta
`fun n x y p q => isSetAt n x y p q` at `isSetLin (OrbAt n)` checks:
`OrbAt n` holds no resource, so it is copyable.

The S4 rows: 45 `test/check/affine-*.lang` rows, 4 `test/parse` rows and
54 `test/fronttool.c` rows. The leaf rows use `Budget` for the leaf
`Leaf`, and a family row is a fronttool `domain()` row (rule R1). These
rows differ from leaf-lang:

- D08 (`affine-dup-closure.lang`) gives `REFUSE_AFFINE_OMEGA`. In
  leaf-lang it gives `REFUSE_AFFINE_REUSE`.
- F30, the family `Pile` with a `Nat` parameter, checks in permit-lang.
- The two marked-field parse rows (`affine-param-mark.lang` and
  `affine-field-mark.lang`) give `PARSE_EXPECT`. In leaf-lang they give
  `PARSE_CTOR_RESULT`. The permit-lang parser has one `parse_fields`.
- `mark-range.lang` (`@2`) gives `PARSE_EXPECT`. In leaf-lang it gives
  `LEX_CHAR`. In permit-lang, `@` is also path application.

| Row | At S4 |
|---|---|
| Resource sorts | Partly. `Budget` and `Approval` are kernel sorts with no constructor (Q-S4-2 (a)). `Capability` comes at S6; the family-field rule makes it a resource type. |
| `spend` | Not yet. The `-o` type and the resource rule of `Option` are in. The op comes at S6. |
| `split` | Partly. A pair of budgets is a resource type, and `unpack` uses both parts (Q-S4-3 (a)). The op and L2 come at S6. |
| A copy of a budget | Covered. `REFUSE_AFFINE_REUSE` ("used 2, allowed 1"); also `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and `REFUSE_AFFINE_PATH`. |
| `execute` | Partly. A 1-binder uses an `Approval` one time. `execute` comes at S7. |
| Weakening | Covered. A program can drop a budget (`burn : Budget -o Nat := fun l => 0`). |
| The run-time check | Not S4. `decide` comes at S7 and `permitd` at S9. |

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

S2 result (Q-S2-1 (a)): the source is relay-lang 05dd63b (tree cf6ea02f,
base seed 145f58f). That commit is cairn-lang d076765 (tree fd5a1e0e, the
intent-lang 67445ae kernel) with six cut parts done: the crypto, the ZK
prelude, the prove command, the contract runtime and `langc run`, the
chain words, and WORD, `U256` and `Addr` (Q-S2-2 (a)). The port takes 195
of the 196 relay paths (164 A, 30 M, 1 R; not docs/PLAN.md). 192 paths are
byte equal to 05dd63b. Three paths have edits: Makefile and src/main.c
(the name, and the target slice S3), and test/gate.sh (the same, and the
permit dash exclusion of design/DESIGN.md). The kernel keeps
`REFUSE_CELL`, `REFUSE_TIME` (the cairn word list), `FreeMon` and the
kernel `state` form (intent-lang, not a relay word). src/front/lower.c,
src/wasm.c and src/ir.h stay in the tree, out of the build, until S3.
Gate: the S1 lines of the IR (10 lowered), the Wasm modules (13 valid) and
the differential cases (813) go (O12). The examples go from 10 to 29, the
parse refusals from 9 to 16, the check refusals from 19 to 151 and the
evals from 32 to 78. New lines: 10 kit-only refusals, 1 time hint, 1 build
refusal, the path regressions. The counts are those of relay 05dd63b.

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
| S2 | Port the Cubical TT kernel (8.1, Q-S0-2). Survey first. Ported from relay-lang 05dd63b (Q-S2-1 to Q-S2-3 (a)); gate EXIT 0: 29 examples, 16 parse refusals, 151 check refusals, 10 kit-only refusals, 1 time hint, 78 evals, 1 build refusal, review, domain and path regressions passed; mutants of `REFUSE_CELL` and `REFUSE_TIME` killed. | DONE |
| S3 | R0 in the kernel and the Wasm writer: N1 to N5, `REFUSE_PARTIAL`, `test/notraptool.c`, no `unreachable`, static memory; the kit trap row (O2). Ported from relay-lang 5c386bb (Q-S3-1 to Q-S3-3); gate EXIT 0: 30 examples, 16 parse refusals, 155 check refusals, 10 kit-only refusals, 1 time hint, 79 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, review, domain and path regressions passed; mutants of `REFUSE_PARTIAL` and `test/notraptool.c` killed. | DONE |
| S4 | The affine layer (section 6): multiplicities, `REFUSE_AFFINE_REUSE`, the resource sorts. Ported from leaf-lang 47a6f36 (Q-S4-1 to Q-S4-12); gate EXIT 0: 30 examples, 20 parse refusals, 200 check refusals, 10 kit-only refusals, 1 time hint, 79 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, review, domain and path regressions passed; mutants of `REFUSE_AFFINE_REUSE`, the context split of a case, the family field loop (Q-S4-12) and `mult_scale` killed. | DONE |
| S5 | `Resource`, `Action`, `Scope`, `StepBound`, `Allowance` (CC), `Decision`, `Reason`; `REFUSE_TIME`; L4, L6. | NEXT |
| S6 | `Budget`, `spend`, `split`, `Capability`, `delegate`; `REFUSE_DELEGATE_WIDEN`, `REFUSE_BUDGET_OVERSPEND`; L1, L2. | |
| S7 | `Payload`, `Mutation`, `canon`, `Digest`, `hash`, `Approval`, `Call`, `Plan` (CM), `Trace` (CC), `decide`; `REFUSE_EXECUTE_UNAPPROVED`; L3, L5, L7. | |
| S8 | T1 part 1: the Wasm module of `decide`, the binding (Q-S0-5), the notrap rows; the differential and wasm runs lines (Q-S3-2 (b)). | |
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
- O2 CLOSED at S3. The kit "Lazy traps" section and the gate row "a trap
  is equal to a trap" are gone. `docs/host/README.md` has "No traps"
  (the relay-lang 5c386bb text in permit words) and the S3 gate rows.
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
- O12 The S2 kernel port had no Wasm writer in the build (Q-S2-3 (a)).
  S3 brings back the IR lines (`ir`, `ir refusals`, `ir lowerings`), the
  build line (`builds`) and the notrap lines (`notrap modules`, `notrap
  bad rows`) (Q-S3-2 (b)). The differential line and the `wasm runs`
  line run node: they stay open until S8.
- O13 CLOSED at S3. The port source is relay-lang 5c386bb (Q-S3-1 (a));
  section 2, "S3 result".
- O14 The kernel `REFUSE_TIME` has the cairn word list (`now`, `block`,
  `timestamp`, `time`, `clock`, `epoch`). S5 adds `ttl`, `deadline` and
  `expires` (Q-S0-9).
- O15 The natops row with a run-time case split (`reasonCode` of
  relay-lang 5c386bb `test/ir/natops.lang`) is not in S3. It folds a
  relay `DeadReason`, and a test program cannot declare a family (rule
  R1, `REFUSE_DATA`). The row comes back when the domain has a sum with
  no fields (the deny `Reason`, S5).
- O16 The S4 rule for the permit forms with no leaf rule is a sound
  fallback (Q-S4-4 (a)). A part that can run more than one time is
  checked at omega. So the checker refuses some sound programs: a `map`
  whose `f` drops a resource element, and a `map` from `Option Budget`
  to `Option Unit`. A precise rule for a form comes only when a demo row
  needs one.

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
