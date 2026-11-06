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
leaf-lang 47a6f36 (section 6, "S4 result"). S5 DONE: `Resource`,
`Action`, `Scope`, `StepBound`, the affine `Allowance`, `Decision` and
`Reason` (sections 4 and 5; the S5 row of section 12). S6 DONE: the
`Budget` ops `spend` and `split`, `Capability`, `delegate`,
`REFUSE_DELEGATE_WIDEN` and `REFUSE_BUDGET_OVERSPEND` (sections 4, 6
and 7; the S6 row of section 12). S7 DONE: `Payload`, `Digest`,
`Mutation`, `Ticket`, `Call`, `Authority`, `judge`, `decide`, the
first-order `Plan`, `Trace` and `REFUSE_EXECUTE_UNAPPROVED` (sections 3,
4 and 7; the S7 row of section 12). S8 DONE: the Wasm module of a
program with `bind` lines: `res` literals, `bind` lines, `def start`, the
`permit` imports, the state region, the exports `decide`, `malformed` and
`memory`, the custom sections `permit.res` and `permit.bind`, and the
`wasm runs`, differential and decide rows on node and wasmtime (sections
1, 4 and 8.2; the S8 row of section 12). S9 DONE: `permitd`, the Wasm
engine of the host, SHA-256, the canonical bytes, the approver, the state
file, the stdio MCP proxy, `fakedeploy` and the demo rows D1 to D11
(sections 8.3 and 10; the S9 row of section 12). S10 DONE: the JSON
authority report: `langc report`, `permitd --report FILE` and
`docs/REPORT-JSON.md` (sections 8.2 and 10; the S10 row of section 12).
S11 DONE: the Cedar export: `langc cedar` and `docs/CEDAR.md` (sections
1, 8.2, 10 and 13; the S11 row of section 12). S12 DONE: A1, the
canonical forms and `langc fmt` (sections 1, 9 and 10; the S12 row of
section 12). S13 DONE: A2, `langc check --json` (the S13 row of section
12). S14 DONE: A4, `langc mcp`, `langc explain`, `langc base` and `langc
law` (the S14 row of section 12). S15 DONE: A3 and A5,
`skills/permit-lang/SKILL.md` and the graded examples G1 to G5 (the S15
row of section 12). S16 DONE: A6, the token comparison (the S16 row of
section 12). S17 DONE: the close, `SPEC.md`, `probe/CAPABILITY.md`,
`docs/VALIDATION.md` and the final gate (the S17 row of section 12). The
plan is CLOSED (Q-S17-3 (a)): O3 and O20 to O25 stay OPEN (section 13).

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
| 2026-11-01 | Q-S5-1 `Action`: "(a) Domain family (Recommended)". `family Action` with the arms `inspect`, `propose` and `execute` in `domain/domain.lang`. A program cannot declare a family (rule R1), so the sum is a domain sum. `actEq` is equality; there is no action order. Edit: section 4. | USER, S5 |
| 2026-11-01 | Q-S5-2 Time words: "(a) Union of 10 (Recommended)". `REFUSE_TIME` names 10 words: `now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `date`, `ttl`, `deadline` and `expires`. The hint says to count operations: a deadline is a `StepBound` (a count of later operations). Edit: section 5; section 13, O14 CLOSED. | USER, S5 |
| 2026-11-01 | Q-S5-3 `Resource`: "(a) Sort + kit-only intro (Recommended)". `Resource` is a kernel sort as `Budget`, but copyable. The kernel ops are `resEq` and `within : Resource -> Resource -> Flag`. The introductions `resRoot : Nat -> Resource` and `resChild : Resource -> Nat -> Resource` are kit-only (`REFUSE_KIT` in guest mode). `within r s` is `flagYes` when the segments of `s` are a prefix of the segments of `r`. Edit: section 4. | USER, S5 |
| 2026-11-01 | Q-S5-4 `Allowance`: "(b) Affine Allowance". `StepBound := Size`. `cofamily Allowance (A : Type 0) := alCorec (eps : A) (advance : Call -> Allowance A)`. A new `resource_shape` rule makes the cofamily a resource type, so a value is used at most one time, and `advance` takes it with `-o`. The S5 stand-in is `family Call := call (target : Resource) (action : Action)`; S7 widens it. Eval and check only; Wasm at S8. Edit: section 4. | USER, S5 |
| 2026-11-01 | Q-S5-5 Laws: "(a) Closed-row parts now (Recommended)". Refl laws plus eval rows in `examples/scope.lang`: L4 on `advance` with a saturating `tick` (`countdown`, `lawAdvance`, `lawSaturate`), L6 on `scopeGate` (`lawScopeIn`, `lawScopeOut`, `lawActionOut`). The `decide` forms come at S7. Edit: section 12. | USER, S5 |
| 2026-11-01 | Q-S5-6 `alOpen`: "(a) One affine `alOpen` (Recommended)". `alOpen : Allowance A -o Prod A (Call -o Allowance A)`. `eps` and `advance` stay one-use views. `alCorec` stays guest-visible: the gate holds the one live value. A kit-only corecursor is a later question, if a row needs it. Edit: section 4; section 13, O17. | USER, S5 |
| 2026-11-02 | Q-S5-7 The affine marker: "(a) opt-in modifier (Recommended)". The keyword `affine` before `cofamily` (`affine cofamily Allowance ...`) sets `affine_pos` on the `D.pos` family. `resource_shape` has a new arm, `affine_arrow`: a Pi on an `affine_pos` family is PRESENT, and so is a `-o` Pi whose codomain is such a Pi. `Stream` is unchanged. The checker emits no `AllowanceBisim` for an affine cofamily, because its `D A` binders are omega (O18). Edit: section 4; section 13, O18. | USER, S5 U1c |
| 2026-11-02 | Q-S5-8 The `alOpen` pair: "(a) PRESENT + split builtin (Recommended)". `Prod A (Call -o Allowance A)` is PRESENT (the `-o` arm of `affine_arrow`), so the pair is affine. The checker generates a trusted `AllowanceOpen` (`fun .A .x => pair (eps .A .x) (fun .e => advance .A .x .e)`), and `alOpen` is that name. `unpack p f` on a `Prod` pair gives `f (first p) (second p)` and charges `p` one time; no new builtin. Edit: section 4. | USER, S5 U1c |
| 2026-11-02 | Q-S6-1 Budget values: "(a) (Recommended)". `budget : Nat -> Budget` and `budgetLeft : Budget -o Nat` are kit-only (`REFUSE_KIT` in guest mode), as `resRoot`. Eval gets a budget value. The gate makes budgets at S9. L1 and L2 are closed refl rows in the kit example `examples/capability.lang` (the Q-S5-5 (a) pattern). Edit: sections 4 and 6. | USER, S6 |
| 2026-11-02 | Q-S6-2 `spend` and `split`: "(a) (Recommended)". Kernel builtins with a new `RULE_BUDGET` shape, guest-visible: `spend : Budget -o Nat -> Option Budget`, `split : Budget -o Nat -> Option (Prod Budget Budget)`. `split b n` gives the `n` units first and the rest second. Each gives `none` when `b` has fewer than `n` units. `spend`, `split`, `budget`, `budgetLeft`, `Capability`, `cap`, `capScope`, `capSteps`, `capBudget`, `sizeLe` and `delegate` are core names (`REFUSE_NAME`), so `examples/orbit.lang` renames its `split` to `pick`, and two `affine-*.lang` rows rename their `spend` to `drain`. Edit: sections 4 and 6. | USER, S6 |
| 2026-11-02 | Q-S6-3 The guest Nat: "(a) (Recommended)". No new guest Nat route at S6: a guest Nat is `len` or a Nat field of a domain value. A guest cannot name `Nat` or write a Nat literal (`REFUSE_KIT`). The host cost comes with the `Call` record at S7. Kit rows use literals. Edit: section 4. | USER, S6 |
| 2026-11-02 | Q-S6-4 `Capability` and `delegate`: "(a) (Recommended)". Domain `family Capability := cap (capScope : Scope) (capSteps : StepBound) (capBudget : Budget)`, a domain def `sizeLe : Size -> Size -> Flag` and a domain def `delegate : Capability -o Scope -> StepBound -> Nat -> Option (Prod Capability Capability)`. The result is (child, rest): the child has the new scope, the new steps and `n` units; the rest keeps the parent scope, the parent steps and the remaining units. Edit: section 4. | USER, S6 |
| 2026-11-02 | Q-S6-5 `REFUSE_DELEGATE_WIDEN`: "(a) (Recommended)". At check time, at a term level of 1 (not in a law type): a `delegate` application whose child scope and parent scope are closed, and `scopeWithin` evaluates to `flagNo`. A `Resource` is kit-only, so the rows are kit rows (`test/check/delegate-widen.lang`). Edit: section 7. | USER, S6 |
| 2026-11-02 | Q-S6-6 `REFUSE_BUDGET_OVERSPEND`: "(a) (Recommended)". At check time, at a term level of 1: a closed `spend`, `split` or `delegate` application whose budget part reduces to `none` (`test/check/budget-overspend-{spend,split}.lang`, `delegate-overspend.lang`). The sum along `Plan` paths comes at S7. Edit: section 7. | USER, S6 |
| 2026-11-02 | Q-S6-7 O17: "(a) (Recommended)". No change; O17 CLOSED. The gate holds the state, and a guest cannot make a `Budget`, so a rebuild cannot add units. Edit: section 13. | USER, S6 |
| 2026-11-02 | Q-S6-8 O18: "(a) (Recommended)". No change; O18 CLOSED. No S6 law needs a bisimulation; the `Allowance` laws are closed refl rows. Edit: section 13. | USER, S6 |
| 2026-11-02 | Q-S7-1 Opaque forms: (a), USER "Take recommendations for questions 1 through 9". Kernel sorts `Payload`, `Bytes` and `Digest` (copyable); kit-only `payloadOf`; guest `payloadEq`, `digestEq`, `hash` and `envelope`; domain `canon`. Eval reads `hash` as injective (the O5 model). Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-2 `Mutation`: (a). Domain `family Mutation := mutation (mTarget : Resource) (mAction : Action) (mPayload : Payload)`. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-3 `Approval`: (a). `Approval` stays the S4 kernel affine sort; kit-only `approval : Digest -> Approval`; guest `approvalDigest : Approval -o Digest`. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-4 `Call`: (a), with a probe first. The probe refused `advance : Call -> Allowance A` (`REFUSE_AFFINE_OMEGA`), so Q-S7-11 replaces the `advance` part. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-5 The host cost: (a). `callCost` is a domain Nat field; kit rows use literals. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-6 `decide`: (a). Domain `Authority`, `judge` (the section 2 deny order) and `decide : Allowance Authority -o Call -o Prod Decision (Allowance Authority)` through `alCorec`; L3 and L7 (L4, L6) rows in `examples/decide.lang`. Q-S7-12 replaces the `Authority` shape. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-7 `Plan` and `Trace`: (a), with a probe first. The probe refused a function field in `Plan` and a `Trace` cofamily with no parameter (R1), so Q-S7-10 replaces both. `Reply` stays. Edit: section 3. | USER, S7 |
| 2026-11-02 | Q-S7-8 `REFUSE_EXECUTE_UNAPPROVED`: (a). At check time, at a term level of 1, on a closed `Plan`; Q-S7-10 gives the first-order path rule. Edit: section 7. | USER, S7 |
| 2026-11-02 | Q-S7-9 The `Plan` path sum: (a). The sum of the costs along a `Plan` path is O20; the S6 closed-term rule stays. Edit: sections 7 and 13. | USER, S7 |
| 2026-11-02 | Q-S7-10 `Plan`: "First-order Plan (Recommended)". `family Plan (A : Type 0) := planDone (planOf : A) \| planInspect (piTarget : Resource) (piOk) (piDenied) (piFailed : Plan A) \| planPropose (ppMutation : Mutation) (ppApproved) (ppRefused : Plan A) \| planExecute (peMutation : Mutation) (peOk) (peDenied) (peFailed : Plan A)`. No `Approval` field: the gate pairs an execute with the approval that its propose got. The plan does not see the deny `Reason`. `def Trace : Type 0 := Stream Entry` (R1: a cofamily has one type parameter). Edit: section 3. | USER, S7 |
| 2026-11-02 | Q-S7-11 `Ticket`: "Ticket split (Recommended)". The affine `Call := call (callMutation : Mutation) (callCost : Nat) (callApproval : Option Approval)` is the `decide` input. The copyable `Ticket := ticket (tkMutation : Mutation) (tkCost : Nat) (tkDigest : Option Digest)` drives `advance : Ticket -> Allowance A` and `alOpen : Allowance A -o Prod A (Ticket -o Allowance A)`. `decide` uses the approval one time (`approvalDigest`), then advances with the ticket. Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S7-12 `Authority`: "Copyable state, Nat left (Recommended)". Copyable `Authority := authority (auScope : Scope) (auSteps : StepBound) (auLeft : Nat) (auUsed : FreeMon Digest)` under `alCorec`; `judge` compares the cost with `auLeft`; L2 stays on the `Budget` ops; the gate holds the `Budget` (S9). Edit: section 4. | USER, S7 |
| 2026-11-02 | Q-S8-1 The `decide` export: "(a) State in module". `langc build` writes an export `decide` that takes one call as flat words and gives a decision code (0 = `permit`, 1 to 8 = the `Reason` arms). The module keeps the `Authority` in linear memory, runs the step of `judge` on it for each call and stores the next `Authority`. The instance is the one live `Allowance` (O17). Edit: sections 8.2 and 12. | USER, S8 |
| 2026-11-02 | Q-S8-2 Opaque sorts at run time: "(a) Handles + imports". Each of `Payload`, `Bytes`, `Digest` and `Resource` is one `i64` handle that the host makes. `payloadEq` and `digestEq` lower to `i64.eq`; `envelope`, `hash` and `within` lower to calls of the imports `permit.envelope`, `permit.hash` and `permit.within`. The node and wasmtime runs bind the O5 model; `permitd` binds JCS, SHA-256 and the segment prefix test (S9). `test/ir/payload.lang` lowers. Edit: section 4; `docs/host/README.md`. | USER, S8 |
| 2026-11-02 | Q-S8-3 Resource names: "(a) `res "a/b"` literal". A guest literal `res "deploy/prod/web"` of type `Resource`; the checker tests the text (`REFUSE_RES`). `langc` writes the paths into the custom section `permit.res` in program order, and the handle of a literal is its index. Eval reads `res "a/b"` as `resChild (resRoot i) j`. Edit: section 4. | USER, S8 |
| 2026-11-02 | Q-S8-4 The `bind` form: "(a) bind + malformed export". A top-level `bind "restart" execute "/target"` (the tool name, an `Action` arm, an RFC 6901 pointer); the checker refuses a second `bind` of one tool, an unknown action and a bad pointer (`REFUSE_BIND`). `langc` writes the table into the custom section `permit.bind`. A call with no binding goes to the export `malformed` (domain `judgeMalformed`, one step). Edit: section 4. | USER, S8 |
| 2026-11-02 | Q-S8-5 The start authority: "(a) Closed `start`, cap = k". A program with a `bind` line has `def start : Authority` with a closed `StepBound` k; `langc build` refuses a `bind` with no `start` (`REFUSE_LOWER`). `auUsed` holds at most k digests (F9), so the memory is fixed from k (N3). Edit: section 4. | USER, S8 |
| 2026-11-02 | Q-S8-6 The differential and `wasm runs` lines: "(a) node + wasmtime now". The two relay-lang 5c386bb rows come back on node and wasmtime (a Wasm trap is a mismatch), with a `decide` script row set in `test/decide/`. The own C99 engine stays at S9. Edit: section 13, O12. | USER, S8 |
| 2026-11-02 | Q-S8-7 The `needsApproval` mark: "(a) Not at S8, O21". `judge` keeps `execute` as the one marked action. Edit: section 13, O21. | USER, S8 |
| 2026-11-02 | Q-S8-8 The start literal: (a). A Nat literal is a guest item in `def start` only; a literal in a law about `start` stays `REFUSE_KIT` (`test/check/start-literal-other.lang`). Edit: section 4. | USER, S8 |
| 2026-11-02 | Q-S8-9 The state step: (a), targeted lowering. A `Size` is a Nat word; an elim over a `Size` is a `REPEAT`; an elim over a run-time nullary family is a switch on the word; `len` over a closed-shape `FreeMon` tree is the sum of the leaf words; `usedHas` is a bounded `REPEAT`; `decide` and `malformed` are synthesized exports over a state region of 3 + k words. Edit: `docs/host/README.md`. | USER, S8 |
| 2026-11-03 | Q-S8-10 The memory export: (a). Memory 0 is exported as `memory` only in a module with `bind` lines; a host reads the steps (word 0) and the budget left (word 1). Edit: `docs/host/README.md`. | USER, S8 |
| 2026-11-03 | Q-S8-11 wasmtime: (b). Each Wasm row runs on wasmtime through `test/run-wasm.mjs --wasmtime`. Edit: `docs/host/README.md`. | USER, S8 |
| 2026-11-03 | Q-S8-12 The decide oracle: (a). The oracle is `langc --kit eval` of the judge chain that `test/run-wasm.mjs --lang` prints. Edit: `docs/host/README.md`. | USER, S8 |
| 2026-11-03 | Q-S9-1 The engine port and the import binding: (a) ("take the recommendations, Q-S9-1 to Q-S9-10 all (a)"). `host/engine.c` and `host/wasmread.c` are copies of relay-lang 4781ca7 host/engine.c and src/wasmread.c (file commit 8d4382a). U1 adds the import section (the function index moves by the import count), the data section (active segments at a constant offset), a host import table bound by (module, name) at load (an import with no row is a load refusal), a memory word write and the kept `memory` export. Two bindings: O5 (the `test/run-wasm.mjs` stubs) and real (JCS envelope, SHA-256, segment prefix). L8 row: `engine decide scripts`. Edit: section 11; `docs/host/README.md`. | USER, S9 |
| 2026-11-03 | Q-S9-2 The SHA-256 port: (a). `host/sha256.c` and `host/sha256.h` from quorum-lang be2c329 src/crypto/sha256.c (file commit 8e18ebf, no U256). `test/shatool.c` from cairn-lang 60768e8 with the 4 FIPS 180-4 vectors and 3 pad rows (55, 56 and 64 bytes). Gate line `sha256 vectors: 7 checked`. Edit: section 11; `docs/host/README.md`. | USER, S9 |
| 2026-11-03 | Q-S9-3 The JSON reader and the JCS number rule: (a). `host/jsonin.c` from relay-lang 4781ca7 src/front/jsonin.c with duplicate key detection (`deny malformed`). A new `host/jcs.c`: keys sorted by UTF-16 code units (RFC 8785 3.2.3), the RFC 8785 string escapes only, no white space. A number is `-?(0\|[1-9][0-9]*)` with a magnitude of at most 2^53 - 1; `-0` writes `0`; a fraction, an exponent or a larger magnitude is malformed. Rows: `test/jcstool.c`. Edit: S9 U2. | USER, S9 |
| 2026-11-03 | Q-S9-4 The approver channel: (a). The agent names the approval in `params._meta["permit/approval"]` (64 lower-case hex characters). The gate removes that key before it makes the payload. Test mode (O9): `--approvals FILE`, one digest a line, read again at each `tools/call`. Terminal mode: `--approver tty` writes the canonical envelope bytes and their digest to /dev/tty and reads `y`. The gate records who approved. Edit: S9 U3. | USER, S9 |
| 2026-11-03 | Q-S9-5 The state across a restart: (a). One text state file `--state FILE`: the steps, the budget left, the used count, then each used digest as 64 hex characters. The gate writes it (a temp file, `fsync`, `rename`) after `decide` or `malformed` and before it forwards a permitted call. At start the gate interns each used digest again; a file that does not parse is exit 2 before any call. Row: D6 after a restart. Edit: S9 U3. | USER, S9 |
| 2026-11-03 | Q-S9-6 The MCP pin, the proxy subset and the deny form: (a). MCP 2025-11-25 is pinned in `docs/MCP.md`. `permitd ... -- CMD ARGS` starts the upstream server with `posix_spawn` on two pipes, one JSON-RPC message a line (1 MiB cap). Only `tools/call` goes through `decide`. A deny is an `isError` tool result that never reaches the child; an upstream failure is the `Outcome` `toolFailed`. Credential (O11): `--cred-file PATH` or `--cred-env NAME`, given only to the child in the variable that `--cred-var NAME` names. Edit: S9 U3. | USER, S9 |
| 2026-11-03 | Q-S9-7 D9: (a). D9 is a static row: `langc check` refuses a sub-agent program that delegates a closed scope wider than its parent (`REFUSE_DELEGATE_WIDEN`, as `test/check/delegate-widen.lang`). The run-time `deny widen` is O22. Edit: sections 2, 8.3 and 13. | USER, S9 |
| 2026-11-03 | Q-S9-8 The unit split: (a). U1: the engine, SHA-256, `test/shatool.c` and the L8 rows. U2: the JSON reader, JCS, the envelope, the RFC 6901 pointer and `test/jcstool.c`. U3: the proxy, the child, the approver, the state, `fakedeploy`, the N6 rows and `docs/MCP.md`. U4: the demo D1 to D11 (`test/hosttool.c`), the mutants, the docs and the close. Edit: none. | USER, S9 |
| 2026-11-03 | Q-S9-9 The cost of a call: (a). Each call costs 1 unit (the budget counts calls); `--cost TOOL=N` changes it for one tool. No kit change. One host row gives `budgetShort` with `--cost`. Edit: S9 U3. | USER, S9 |
| 2026-11-03 | Q-S9-10 The demo tools and program: (a). `fakedeploy` has `inspect`, `propose`, `execute`, `sync_left` and `sync_right` (D10), `restart` and `delete` (D11). One demo program `test/host/deploy.lang` has the 7 bind lines and a `start` whose scope holds `prod` and `dev`. `fakedeploy` keeps its state in one file. Edit: section 8.3 at S9 U4. | USER, S9 |
| 2026-11-03 | Q-S9-U4-1 The D10 mutant: (a). The payload holds the tool name (section 4, D11), so the D10 deny holds also when the envelope has no target. The D10 mutant drops both the target of the envelope and the name of the payload. A row where only the target changes is O23. Edit: section 13, O23. | USER, S9 |
| 2026-11-03 | Q-S10-1 The entry points and the writers: (a). `langc report PROG` (`src/report.c`) writes the static part. `permitd --report FILE` (`host/report.c`) appends one line for each decided call, with `fflush` and `fsync`, after the state write and before the answer or the forward. Edit: section 8.2, the T2 row. | USER, S10 |
| 2026-11-03 | Q-S10-2 The format: (a). JSON Lines: each line is one object in its JCS form, then `\n`, with `"permit":1` and `"kind"` (`program`, `call` or `malformed`). Both writers make the bytes with `json_parse` and `jcs_write`. No time field. Edit: none. | USER, S10 |
| 2026-11-03 | Q-S10-3 The fields: (a). Each 64-bit word is a JSON string of decimal digits. The keys of each line are in `docs/REPORT-JSON.md`. The line does not hold the upstream outcome. Edit: section 13, O24. | USER, S10 |
| 2026-11-03 | Q-S10-4 The unit split: (a). U1: `langc report` and the `test/report/` rows. U2: `permitd --report` and the hosttool rows R1 to R4. U3: `docs/REPORT-JSON.md`, `docs/host/README.md`, this plan and the close. Edit: none. | USER, S10 |
| 2026-11-03 | Q-S11-1 The entry point and the walker: (a). `langc [--kit] cedar PROG` writes to stdout, exit 0, 1 or 2. `report_value` in `src/report.c` makes the JSON object of the `program` line; `report_program` writes its JCS form (the bytes do not change) and `src/cedar.c` walks the same object (one walker). Edit: `docs/CEDAR.md`. | USER, S11 |
| 2026-11-03 | Q-S11-2 The policy form: (a). One `permit` for each grant, in the scope walk order, duplicates kept; `action == Action::"ARM"`, `resource in Resource::"TEXT"`, principal open, no annotation. Edit: `docs/CEDAR.md`. | USER, S11 |
| 2026-11-03 | Q-S11-3 The text layout: (a). The 3-line header, then the policies, then the 6 "Not exported" comments in a fixed order (steps, budget, approval binding, sequence guard, laws, bind lines); a fixed text for no `def start` and for a scope with no grant. Edit: `docs/CEDAR.md`. | USER, S11 |
| 2026-11-03 | Q-S11-4 The unit split: (a). U1: the code and the probe. U2: the golden rows, `test/report/empty.lang`, the gate line, the write error check and the mutants. U3: the docs, Q-S11-5, Q-S11-6 and the close. Edit: none. | USER, S11 |
| 2026-11-03 | Q-S11-5 The doc-check debt of S9: (a). `docs/MCP.md` writes each of the two JSON answers in a `json` block, with one key or one closing brace on each line, so no line has two closing braces together. The bytes of `permitd` do not change. Edit: `docs/MCP.md`. | USER, S11 |
| 2026-11-03 | Q-S11-6 The C8c survivor: (a). `test/report/big.lang` (60 binds, a report line of 9959 bytes, 2 doublings of the 4096-byte JCS buffer), `big.json` and `test/cedar/big.cedar`: 7 report rows and 7 cedar rows. The rerun killed C8c. Edit: section 10. | USER, S11 |
| 2026-11-03 | Q-S12-1 The CLI: (a). `langc [--kit] fmt PROG [--check]`, as accrue-lang Q-S11-4. `fmt` writes to stdout only, never in place. Exit 0: the text is written, or `PROG` is canonical under `--check` (no output). Exit 1: a lex or parse error, a failed reparse, or a `--check` mismatch (`PROG:LINE: not canonical` on stderr at the first line that is different). Exit 2: a usage or IO error. `--kit` has no effect. Edit: `docs/host/README.md`. | USER, S12 |
| 2026-11-03 | Q-S12-2 The layout: (a). The accrue-lang layout: the line width is 100; a longer decl breaks only after `:=`, with the body at a 2-space indent; each constructor after the first is on its own line `| c`; an axiom never breaks; a blank line comes before each decl, but not before an `and` sort. One permit rule: no blank line between two `bind` lines that follow each other (a bind run). Edit: `docs/host/README.md`. | USER, S12 |
| 2026-11-03 | Q-S12-3 The spellings: (a). The accrue-lang spellings (`Sigma (x : A) B`, one `fun` for a lambda chain, a bare name for a binder with no type, the minimum parentheses, one name in each group), the marks (`(@0 x : A)`, `(@1 x : A)`, `A -o B`) and each string as it is (`res "TEXT"`, `bind "TOOL" ARM "POINTER"`). H1: a group whose type names an earlier name of the group stays one group `(x y : T)` (Q-S12-7 gives the reason). The parser keeps the second forms. Edit: `src/front/fmt.c`. | USER, S12 |
| 2026-11-03 | Q-S12-4 The comments: (a). As accrue-lang Q-S11-3: each comment stays as it is, on its own line at column 0. A comment inside a decl, or after code on its line, moves above the decl. The comments after the last decl come after one blank line. Edit: `src/front/fmt.c`. | USER, S12 |
| 2026-11-03 | Q-S12-5 The gate block: (a). Port the accrue-lang fmt block of `test/gate.sh` and make the repository canonical. `test/fmt/refused.txt` lists the files that `fmt` refuses. `build/fronttool dump` is scoped: each variable prints the distance to its binder or `global`, and each binder prints its mark. 10 golden rows and 3 witness rows (`piname`, `mark`, `capture`). Each old gate count stays the same. Edit: `test/gate.sh`, `test/fmt/`, `test/fronttool.c`. | USER, S12 |
| 2026-11-03 | Q-S12-6 The unit split: (a). U1: the comment table of the lexer, the end line of a decl, `src/front/fmt.c`, `langc fmt`, the scoped dump and a probe of all files. U2: the rewrite of the repository, the `test/fmt` rows and the gate block. U3: the mutants, the docs and the close. Edit: none. | USER, S12 |
| 2026-11-03 | Q-S12-7 The H1 reason: (a). The parser gives `(x y : T)` with one T for the two binders, and the checker checks the T of y with x in scope. Thus the split `(x : T) -> (y : T) -> B` has the same meaning and the same dump. The H1 guard stays: it keeps the source group, and it makes no claim of capture. Edit: `src/front/fmt.c`, `test/gate.sh`. | USER, S12 |
| 2026-11-04 | Q-S12-8 A NUL byte in a string: (a). Before S12, a `res` string with a NUL byte passed `langc check`, and `langc fmt` dropped the text after the NUL, because the name is a C string. Now `lex_string` refuses a NUL byte in a string with `LEX_STRING`. A NUL byte in a comment stays (`fmt` writes a comment with its length). New row `test/parse/string-nul.lang`: parse refusals go from 22 to 23. Edit: `src/front/lexer.c`, `docs/host/CAPABILITY.md`. | USER, S12 |
| 2026-11-04 | Q-S12-9 The M9 survivor: (a). Mutant M9 (`test/fronttool.c`: each bound variable prints `#0`) survives the gate. Each gate use of the dump compares 2 dumps, and the distance follows from the kinds, names and nesting that the dump prints, so M9 cannot change a compare. M9 is EQUIVALENT under the gate oracle. No new row. Edit: section 10. | USER, S12 |
| 2026-11-04 | Q-S13-1 The entry and the contract: (a). `langc [--kit] check --json PROG`, with `--json` right after `check` (`check PROG --json` is `USAGE`). One JSON line and `\n`, with 0 or 1 diagnostic (the first failure). Exit 0 or 1: the line on stdout and an empty stderr. Exit 2 (`OOM`, `IO`, `USAGE`): no line and the text diagnostic on stderr. Edit: none. | USER, S13 |
| 2026-11-04 | Q-S13-2 The diagnostic: (a). The keys are `code`, `span`, `message` and `fix`; there is no `expected` key and no `found` key. `span` is `{file,line,col,end_line,end_col}` (1-based byte columns, exclusive end), `null` when there is no position; a lexer or parser failure has the span of its token, a check failure the span of the failing decl. `message` is `DEF: MSG` (DEF `-`). Edit: the A2 row. | USER, S13 |
| 2026-11-04 | Q-S13-3 The progress: (a). One object, no progress lines: `parsed`, `typed`, `laws_discharged`, `laws_total`, for the program decls only. A law is a program def whose type ends in a Path (the `out_laws` rule of `report.c`); a syntax scan counts the decls after the failure. The 4 counts are 0 after a `LEX_` or `PARSE_` failure. No `affine_checked`: one pass checks the types and the multiplicities, so it is `typed`. Edit: the A2 row. | USER, S13 |
| 2026-11-04 | Q-S13-4 One writer: (a). The `JsonOut` helpers of `report.c` move to `src/jsonout.c` and `src/jsonout.h`. `check --json` makes the text, then `json_parse` and `jcs_write`, so the line is JCS as `langc report`. `out_bytes` maps invalid UTF-8 to U+FFFD. The report bytes do not change. Edit: none. | USER, S13 |
| 2026-11-04 | Q-S13-5 The fixes: (a). The port sites that match: `diag_fix` and `FAIL_FIX`; `LEX_CHAR` (2 lexer sites), `REFUSE_CELL` (`List` to `FreeMon`), `REFUSE_SIG` (the slot type; `null` when the decl has no type), `REFUSE_STATE` (delete the second state decl). Else the fix is `null`. Each refused row has a hand `X.fix`, and `build/fronttool fix` applies it. Edit: none. | USER, S13 |
| 2026-11-04 | Q-S13-6 The rows: (a). One row for each `REFUSE_` code of `check.c`, plus `ok`, `kit`, `LEX_CHAR`, `PARSE_EXPECT`, `TYPE_MISMATCH`, `TYPE_MISMATCH.after` and `TYPE_MISMATCH.splice` (the kernel still splices the prelude). A row runs with `--kit` when guest mode gives `REFUSE_KIT`. Each new `.lang` row is canonical; the `LEX_CHAR` and `PARSE_EXPECT` rows are in `test/fmt/refused.txt`. Edit: none. | USER, S13 |
| 2026-11-04 | Q-S13-7 The mutants: (a). 8 mutants: the accrue-lang M1 (span end column), M2 (`parsed` in the `typed` slot), M3 (`laws_total`), M4 (`fix_set`), M6 (the diagnostic on stderr) and M7 (one code row less), and the new M8 (`laws_discharged` off by one) and M9 (the start column of a check span). The accrue-lang M5 is dropped (the escapes are JCS, S9). Edit: section 10. | USER, S13 |
| 2026-11-04 | Q-S13-8 The unit split: (a). U1: the spans of the lexer, the parser and the check, the fix sites, the progress and `fronttool fix`. U2: the shared writer, the entry of `src/main.c`, the `test/json/` rows and the gate block. U3: `docs/CHECK-JSON.md`, this plan, `docs/host/README.md`, the mutants and the close. Edit: none. | USER, S13 |
| 2026-11-04 | Q-S13-9 `REFUSE_LAW` (U2): (a). `langc check` cannot give `REFUSE_LAW`: the domain and the kernel splices have no law def and no sealed family, and a program cannot declare one (`REFUSE_DATA` in guest and `--kit` mode). The gate skips `REFUSE_LAW` with a comment; the `law_guest` rows of `build/fronttool` test it. Rows: 21 codes and 7 more, 28. The count line stays `json refusal codes: 21 of 21`. Edit: `docs/CHECK-JSON.md`. | USER, S13 |
| 2026-11-04 | Q-S14-1 The pin: (a). `MCP_PROTOCOL_VERSION` `2025-11-25` in `src/mcp.h`, the same pin as `permitd` (`docs/MCP.md`). `initialize` always gives it and does not compare the client value; it needs a string `params.protocolVersion`, else -32602. Edit: `docs/MCP.md`. | USER, S14 |
| 2026-11-04 | Q-S14-2 `explain`: (a). `src/front/codes.c` and `src/front/codes.h` (`CodeRow` {code, section, meaning}, `code_count`, `code_at`, `code_find`). The meaning is the `docs/host/README.md` text. Output `CODE: MEANING (docs/host/README.md, SECTION)`; an unknown code exits 1 with an `EXPLAIN` diagnostic. The README first gets the 6 missing codes (`REFUSE_CELL`, `REFUSE_HISTORY_WRITE`, `REFUSE_MAP`, `REFUSE_SIG`, `REFUSE_STATE`, `REFUSE_REPORT`). Each `REFUSE_` code of `src` (`codes.c` excluded) has one row: 26. CLI verb `langc explain CODE`. Edit: `docs/host/README.md`. | USER, S14 |
| 2026-11-04 | Q-S14-3 `list-base`: (a). A static C table of the 20 rows of section 4 (Name, Kind, Line), `BASE_ROWS` of `src/front/plantab.c`. CLI verb `langc base` (no PROG), one line a row. The tool takes no arguments. Edit: none. | USER, S14 |
| 2026-11-04 | Q-S14-4 `show-law`: (a). A static table of the laws L1 to L8 of section 7, `LAW_ROWS` of `src/front/plantab.c`. CLI verb `langc law LN`, tool argument `law` (`L1` to `L8`). Output one line `LN NAME: STATEMENT`. An unknown law exits 1 (`isError` true), as `explain`. Edit: none. | USER, S14 |
| 2026-11-04 | Q-S14-5 JSON: (a). The reader is `host/jsonin.c` (already linked). Its stricter parse (a duplicate key, a lone surrogate, invalid UTF-8) gives -32700, and `docs/MCP.md` says so. `src/jsonout.c` gets a `FILE *` string writer with the accrue-lang escape set (`json_string_n`). The replies keep the member order of the port (not JCS). The report and `check --json` bytes do not change. Edit: `docs/MCP.md`. | USER, S14 |
| 2026-11-04 | Q-S14-6 The build targets: (a). `main` (`-o OUT`) and `runtime` (`-o OUT --runtime`), `McpTarget` {MAIN, RUNTIME}. Gate `mcp builds: 2 of 2` (each file is byte equal to the CLI build). Edit: none. | USER, S14 |
| 2026-11-04 | Q-S14-7 The rows: (a). The 9 port rows (`build`, `check`, `errors`, `eval`, `explain`, `fmt`, `kit-check`, `kit-eval`, `lifecycle`), `list-base` and `show-law`: 11. Gate lines: `mcp rows: 11 passed`, `mcp negatives: 3 of 3`, `mcp tools: 7 of 7`, `mcp builds: 2 of 2`, `mcp long line: 1 of 1`, `explain codes: 26 of 26` and `base names` (Q-S14-U2-1). Edit: none. | USER, S14 |
| 2026-11-04 | Q-S14-8 The unit split: (a), as S13. U1: the source (`src/mcp.c`, `src/mcp.h`, the code, base and law tables, the `FILE *` writer, the verbs of `src/main.c` and the out and err routing), the `Makefile` and `test/mcptool.c`. U2: the `test/mcp/` rows and the gate block (mcp, explain codes, base names). U3: the docs (`docs/MCP.md`, the README codes, this plan), the mutants and the close. Edit: none. | USER, S14 |
| 2026-11-04 | Q-S14-U2-1 The base names (U2): (b), domain names only. The gate finds each name of a `BASE_ROWS` row as a def, family, cofamily, affine cofamily or sealed family of `domain/domain.lang`: `base names: 15 of 15`. A fixed list skips the 12 other names: the kernel sorts `Resource`, `Budget`, `Payload`, `Bytes`, `Digest` and `Approval`, the kernel ops `hash` and `envelope`, the syntax forms `res`, `bind` and `start`, and the mark `needsApproval` (O21, Q-S8-7 (a): not in `src` or the domain). Edit: `test/gate.sh`. | USER, S14 |
| 2026-11-04 | Q-S14-U2-2 The README text of 11 codes (U2): (a). 11 codes had only a prose or Gate sentence. U3 adds one rules table row in `docs/host/README.md` for each of `REFUSE_AFFINE_REUSE`, `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX`, `REFUSE_AFFINE_PATH`, `REFUSE_PARTIAL`, `REFUSE_LOWER`, `REFUSE_KIT`, `REFUSE_EXECUTE_UNAPPROVED`, `REFUSE_RES`, `REFUSE_BIND` and `REFUSE_LAW`, and `src/front/codes.c` takes the row text. Edit: `docs/host/README.md`, `src/front/codes.c`. | USER, S14 |
| 2026-11-05 | Q-S15-1 The port source: (a). The accrue-lang S14 skill on accrue HEAD 2c804b7 (in flight, not committed) and the cairn-lang 2df0354 `test/skilltool.c`. The skill block of `test/gate.sh` is the accrue block, after `# mcp rows: end`. U1 delta: the pin of the accrue `skills/accrue-lang/SKILL.md` moved from b038b471 to 67a748e9. Edit: `Makefile`, `test/gate.sh`. | USER, S15 |
| 2026-11-05 | Q-S15-2 Guest or kit: (a). A G file is a guest file where it can be. G5 is the demo `test/host/deploy.lang`, a guest file. A file is a kit file only when it needs a kit-only name (`budget`, `budgetLeft`, `payloadOf`, `approval`, `resRoot`, `resChild`): G1 to G4. `examples/skill/kit.txt` names the kit files only. The loop and the examples show both forms. Edit: none. | USER, S15 |
| 2026-11-05 | Q-S15-3 The G3 repair pair: (a). One repair pair on the refused file `test/check/delegate-widen.lang` (D9), and no new refused file. U1 delta: the file is a kit file, so the pair is `build/langc --kit check --json test/check/delegate-widen.lang` (exit 1, the code `REFUSE_DELEGATE_WIDEN` and its fix) and `build/langc explain REFUSE_DELEGATE_WIDEN` (exit 0). Edit: none. | USER, S15 |
| 2026-11-05 | Q-S15-4 The docs: (a), the cairn shape. `AGENTS.md` is rewritten: Routing rule, The loop, Status (S15), Commands that work now, Rules for a change, When to use permit-lang (Task, permit-lang?, Use instead: G1 to G5 are yes rows, then Cedar, zkpolicy-lang, mandate-lang and general code are no rows) and Files. The README table "When to reach for permit-lang" takes the same 3 columns and rows. README Status and Layout are true at S15. Edit: `AGENTS.md`, `README.md`. | USER, S15 |
| 2026-11-05 | Q-S16-0 The S15 base: (a). S16 builds on USER 9ac759a (tree 674456c9). The USER commit removes the last blank line of `test/skill/bad-exit.md`, `changed-quote.md` and `no-text.md`; the base gate is SAME. Edit: none. | USER, S16 |
| 2026-11-05 | Q-S16-1 The tokenizer: (a), the cairn-lang route. `docs/TOKEN-BASELINE.md` and `make check` use the offline estimate ceil(bytes / 4) (cairn-lang Q-S15-10 (c)). `make tokens` holds the `count_tokens` route: the newest model from `GET /v1/models`, 1 frame message `x`, and the net count is N minus the frame. The USER runs it outside the sandbox with `ANTHROPIC_API_KEY`. It is not in `make check`. Edit: `Makefile`, `test/tokcount.c`, `docs/TOKEN-BASELINE.md`. | USER, S16 |
| 2026-11-05 | Q-S16-2 The programs and the outputs: (a). 5 programs, G1 to G5 of `examples/skill/` (G5 is the demo, Q-S15-2 (a)). Each Python and each Rust copy prints the 3 `langc eval` lines of its G in the SKILL.md order, byte equal to `langc --kit eval` (G1 to G4) or `langc eval` (G5). 15 eval rows and 15 counted files (5 `.lang`, 5 `.py` and 5 `.rs`). Edit: `test/baseline/`. | USER, S16 |
| 2026-11-05 | Q-S16-3 The layout and the gate: (a), the cairn-lang shape. `test/baseline/g1-scope.py` to `g5-deploy.py` (the standard library only); `test/baseline/rust/` (1 crate, bins g1 to g5, `sha2` 0.10 from crates.io for the G4 digest, `Cargo.lock`); the `.gitignore` line `test/baseline/rust/target/`; `test/tokcount.c` with `host/sha256.c` (the modes `--check`, `--estimate`, `--model`, `--request` and `--table`); the `make check` rows `token report hashes: 15 of 15` and `token estimate: passed`; the token recipe block of `test/review.sh` (cairn-lang +46). Edit: `.gitignore`, `Makefile`, `test/review.sh`, `test/tokcount.c`, `test/baseline/`. | USER, S16 |
| 2026-11-05 | Q-S17-0 The SPEC form: (a), the cairn-lang and relay-lang form. `SPEC.md` fills all 10 sections, with no template lines, a status line and a pointer. Sections 5 and 6 cite sections of this plan and lines of `domain/domain.lang`. Section 9 gives the open and standing O items with cites to their lines here. Section 10 maps the slices to the kit milestones by content: M0 = S0 to S5, M1 = S6 and S7, M2 = S8 to S11, A = S12 to S16, M3 PARTIAL (no slice), Close = S17. Edit: `SPEC.md`. | USER, S17 |
| 2026-11-05 | Q-S17-1 The probe: (a), new runs. `probe/CAPABILITY.md` takes the cairn-lang and relay-lang form, from new runs in this repo with `probe/guard.py` and `build/langc` (each step below 4 GB and 120 s): the cost at N = 100 and N = 1000, the permit facts (the `permitd` Wasm engine, SHA-256, JCS, `build/tokcount`), a replay block and the consequences. The kit paths that permit-lang does not have are removed. Edit: `probe/CAPABILITY.md`. | USER, S17 |
| 2026-11-05 | Q-S17-2 VALIDATION: (a), a header and one paragraph for each slice. `docs/VALIDATION.md` replaces the template header (the date, the tcc version, `build/langc` at the commit, a `make check` paragraph with the count lines and the final gate seconds) and adds one short paragraph for each slice S0 to S14 from section 12. The S15 and S16 paragraphs stay. `docs/STATUS.md` stays template (cairn-lang and relay-lang). Edit: `docs/VALIDATION.md`. | USER, S17 |
| 2026-11-05 | Q-S17-3 The open items: (a), all stay OPEN. O3 and O20 to O25 stay OPEN (the cairn-lang O37 precedent), and `SPEC.md` section 9 lists them with cites. Section 13 marks the 10 unmarked items O1, O4 to O11 and O16 as standing (v1 scope). The plan ends after S17, with no S18. Edit: this plan. | USER, S17 |
| 2026-11-05 | Q-S17-4 The unit split: (a), 3 units. U1: `SPEC.md`, all 10 sections, one anchored edit script. U2: `probe/CAPABILITY.md` (new `probe/guard.py` runs, the cost at N = 100 and 1000, the replay block). U3: the VALIDATION header and slice paragraphs, this plan (Q-S17-0 to Q-S17-6, the S17 row, the standing marks, the status), `AGENTS.md` `## Status (S17)`, the README Status, the final gate and the stage. Not asked (the relay-lang Q-S16-4 precedent): `AGENTS.md` and the README Status take an S17 sentence and the `SPEC.md` pointer. Edit: `AGENTS.md`, `README.md`. | USER, S17 |
| 2026-11-05 | Q-S17-5 The `PENDING_MAX` limit (U3): (a), record only. `permitd` keeps at most 256 open calls (`host/permitd.c:45`). A burst of 1000 `tools/call` lines with no wait gives 256 results and 744 errors -32603 `permit-lang too many pending calls`. `probe/CAPABILITY.md` section 6 and consequence 9 record it. No `docs/MCP.md` line and no new O item. Edit: none. | USER, S17 |
| 2026-11-05 | Q-S17-6 The kit cite drift in `formers/tcc-wasm.md` (U3): (a), record only. Line 40 names `docs/CAPABILITY.md` (the file is `docs/host/CAPABILITY.md`), and line 28 cites `domain/domain.lang:329` (a comment line). `SPEC.md` section 3 records both. Edit: none. | USER, S17 |

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
- A delegation that widens the parent: `deny widen` at run time (O22: no
  run-time row at S9), and `REFUSE_DELEGATE_WIDEN` at check time when the
  checker can see it (D9).
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
  signature. Each tool call is a cell: `planInspect`, `planPropose`,
  `planExecute` (S7, Q-S7-10 (a)). The plan is first order: each cell has
  one branch field for each reply arm, no function field and no
  `Approval` field. Rule R1 refuses a field of a function type
  (`REFUSE_CELL`, R1 fact P1).
- The authority is an `Allowance`, the cofree comonad of `Ticket -> -`
  (S7, Q-S7-11 (a)). `eps` gives the authority that is left now (an
  `Authority`: the scope, the steps, the units left, the used digests).
  `advance : Ticket -> Allowance A` gives the authority after one call.
  This is the accrue-lang `advance` shape with a `Ticket` in place of a
  `Stamp`. The affine `Call` holds the approval; `decide` uses it one
  time and advances with the copyable `Ticket`.
- The record of a run is a `Trace`, the cofree comonad over the record
  signature. S7 (Q-S7-10 (a)): `def Trace : Type 0 := Stream Entry` with
  `Entry := entry (enMutation : Mutation) (enDecision : Decision)
  (enSteps : StepBound) (enLeft : Nat)`. A cofamily has one type
  parameter (R1 fact P3), so `Trace` is not a cofamily of its own.
  `sHead` gives the current entry (the mutation, the decision, the steps
  and the units left). The T2 report is a fold over a finite prefix of
  the `Trace`.
- Authority over a sequence: the pairing runs a `Plan` against an
  `Allowance` and gives a `Trace`. The checker shows, on each path of the
  plan, that each `planExecute` lies in the `ppApproved` branch of a
  `planPropose` of the same mutation (L5).

| Former | Class | Reason |
|---|---|---|
| `Plan A` | CM | Free over the cells `planInspect r ok denied failed`, `planPropose m approved refused` and `planExecute m ok denied failed` (closed data: `Resource`, `Mutation`; arity: the arms of `Reply`, one branch field each). `eta` is `planDone`. S7 (Q-S7-10 (a)): first order, no `Approval` field. |
| `Allowance` | CC | Cofree over `Ticket -> -` (Q-S7-11 (a)): `eps` = the authority now, `advance` = the authority after one call. The dual of `Plan`. |
| `Trace` | CC | S7: `Stream Entry` (Q-S7-10 (a)): `sHead` = the current entry, `sTail` = the rest of the trace. |
| `Outcome R` | CM | `eta` = `done`, cells `denied reason` and `toolFailed code` (closed base data). The exception monad as a value. S7 uses `family Reply := replyOk \| replyDenied (replyWhy : Reason) \| replyFailed` in its place (Q-S7-10 (a)). The plan branches on the reply arm and does not see the `Reason` in a branch. |
| `Option A` | CM | `eta` = `some`, one cell `none` with no data. |
| `FreeMon A` | CM | The admitted list: the scope entries, the used approvals of one run. |
| `Capability`, `Mutation`, `Call`, `Ticket`, `Authority`, `Reply`, `Entry`, `Approval`, a user `Action` sum, `Decision`, `Reason`, `Prod`, `Sum` | LCC structure | Finite products and coproducts. No parameter, so not a type constructor. |
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
| `Resource` | kernel sort, copyable (Q-S5-3 (a)); kit-only `resEq`, `within : Resource -> Resource -> Flag`, `resRoot : Nat -> Resource` and `resChild : Resource -> Nat -> Resource` | P3 | A path of segments (for example `deploy/prod/web`). `within` is prefix containment by segments, with no wildcard. The gate interns the segments. A program never parses a string (Q-S0-3 (a)). A guest `resRoot` or `resChild` is `REFUSE_KIT`: the gate makes the values (S8). `resEq` and `within` are kit-only too (as built at S5); a guest tests scopes with the domain defs `allows` and `scopeWithin`. |
| `Action` | a domain sum: `family Action` with the arms `inspect`, `propose` and `execute` in `domain/domain.lang` (Q-S5-1 (a)) | P2, P8 | A program cannot declare a family (rule R1). `actEq` is equality; there is no action order. |
| `needsApproval` | a mark on an `Action` | P7, P8 | Q-S0-11. |
| `Scope` | `FreeMon (Prod Resource Action)`; `grants`, `allows`, `scopeWithin` | P3, P5 | Finite. `scopeWithin child parent` is decidable. S5: `allows` and `scopeWithin` count the grants that match (`fmBindNat`, then `len`) and test the count with `natEq 0`. They do not fold into `Flag`: the domain cannot prove the `Flag` monoid laws of `fmFold` at an open `Flag` (permit has no `fmRun`). `within` does the resource test. |
| `StepBound` | `Size` (Q-S5-4 (b)): a count of calls left | P4 | Expiry under the time ruling (section 5). A tick is `sizePred`; it stops at `sz0` (L4). |
| `Budget` | kernel sort, affine; guest `spend` and `split` (Q-S6-2 (a)); kit-only `budget : Nat -> Budget` and `budgetLeft : Budget -o Nat` (Q-S6-1 (a)) | P6 | Section 6. `spend` and `split` give an `Option`. A guest cannot make a budget; the gate makes the values (S9). |
| `Capability` | domain `family Capability := cap (capScope : Scope) (capSteps : StepBound) (capBudget : Budget)` (Q-S6-4 (a)) | P1, P5 | A resource type, because it holds a `Budget` (`test/check/capability-{reuse,omega}.lang`). |
| `delegate` | a domain def, `Capability -o Scope -> StepBound -> Nat -> Option (Prod Capability Capability)` (Q-S6-4 (a)) | P5 | Attenuation only: the child scope is within the parent scope, the child steps are not more than the parent steps (`sizeLe`), and the child budget is split from the parent budget (L1). The result is (child, rest): the rest keeps the parent scope, the parent steps and the remaining units. |
| `Payload` | kernel sort, copyable, opaque canonical tool-call bytes; kit-only `payloadOf : Nat -> Payload`, guest `payloadEq` (Q-S7-1 (a)); equality and `canon` input only | P7, P9, P10 | The gate canonicalizes the tool name, arguments and each forwarded call parameter before it constructs a `Payload`. Raw JSON spellings are not payload values. At run time a `Payload` is one `i64` host handle (S8, Q-S8-2 (a)). |
| `Mutation` | domain `family Mutation := mutation (mTarget : Resource) (mAction : Action) (mPayload : Payload)` (Q-S7-2 (a)) | P7, P9 | Copyable. |
| `canon` | domain def `Mutation -> Bytes`, `envelope (mTarget m) (mPayload m)`: the canonical bytes of (target, payload) | P9 | Q-S0-4; the target envelope below. |
| `Bytes`, `Digest`, `hash`, `envelope` | kernel sorts `Bytes` and `Digest` (copyable, opaque); guest `digestEq`, `hash : Bytes -> Digest` and `envelope : Resource -> Payload -> Bytes` (Q-S7-1 (a)) | P7, P9 | v1 binds SHA-256 (ruling). Eval reads `hash` as injective (the O5 model). |
| `res`, `bind`, `start`, `judgeMalformed` | guest `res "a/b"` of type `Resource` (Q-S8-3 (a), `REFUSE_RES`); a top-level `bind "tool" action "/pointer"` (Q-S8-4 (a), `REFUSE_BIND`); `def start : Authority`, closed, with a closed `StepBound` k (Q-S8-5 (a)), the one place of a guest Nat literal (Q-S8-8 (a)); domain `judgeMalformed : Authority -> Prod Decision Authority` | P2, P3, P10 | S8. Eval reads `res "a/b"` as `resChild (resRoot i) j`, with i and j the segment numbers in program order. `langc` writes the paths and the bind table into the custom sections `permit.res` and `permit.bind` (`docs/host/README.md`). A call of a tool with no `bind`, or with no value at the pointer, goes to the export `malformed`: `judgeMalformed` gives `deny malformed` and takes one step (Q-S0-6). |
| `Approval` | the S4 kernel sort, affine, over one `Digest`; kit-only `approval : Digest -> Approval`, guest `approvalDigest : Approval -o Digest` (Q-S7-3 (a)) | P7 | Section 6. The gate records who approved; the program does not read it. |
| `Call` | domain `family Call := call (callMutation : Mutation) (callCost : Nat) (callApproval : Option Approval)` (Q-S7-4 (a)) | P2, P10 | One request from the agent, the `decide` input. A resource type, because it holds an `Approval`. `callCost` is the host cost (Q-S7-5 (a)). It replaces the S5 stand-in `call (target : Resource) (action : Action)` (Q-S5-4 (b)). |
| `Ticket` | domain `family Ticket := ticket (tkMutation : Mutation) (tkCost : Nat) (tkDigest : Option Digest)`, copyable; `callTicket : Call -o Ticket` (Q-S7-11 (a)) | P2, P10 | `callTicket` uses the approval one time (`approvalDigest`). Fact P7: a copyable constructor and `some` at a copyable `Option` take their arguments at omega, so the digest goes through a `-o` binder (`someDigest`, `ticketWith`). |
| `Decision`, `Reason` | `permit` or `deny Reason`; `Reason` is the closed sum of section 2 | P3 to P9 | A denial is a value (R0). S5: both are domain families; `Reason` has the 8 arms of section 2. |
| `Allowance` | `affine cofamily Allowance (A : Type 0) := alCorec (eps : A) (advance : Ticket -> Allowance A)`; `alOpen : Allowance A -o Prod A (Ticket -o Allowance A)` (S7: over `Ticket`, Q-S7-11 (a)) | P2, P4, P10 | Affine (Q-S5-4 (b), Q-S5-7 (a)): a value is used at most one time, and a second use is `REFUSE_AFFINE_REUSE`. `eps` and `advance` are one-use views. `alOpen` gives both parts; `unpack` on the pair binds each part at 1 (Q-S5-6 (a), Q-S5-8 (a)). Section 3 gives the CC reading. |
| `decide` | domain def `Allowance Authority -o Call -o Prod Decision (Allowance Authority)` (Q-S7-6 (a)) | P10 | The one entry of the T1 module (S7). It uses the approval one time (`callTicket`), then `decideTicket` judges the ticket. |
| `Authority` | domain `family Authority := authority (auScope : Scope) (auSteps : StepBound) (auLeft : Nat) (auUsed : FreeMon Digest)`, copyable (Q-S7-12 (a)); `judge : Authority -> Ticket -> Prod Decision Authority`; `allowanceOf : Authority -> Allowance Authority` | P3 to P10 | `judge` denies in the section 2 order: `scopeMiss`, `boundSpent`, `budgetShort`, `noApproval`, `digestDiffers`, `approvalUsed`. A permit takes one step and the cost (`auTake`) and, for an execute, adds the digest to `auUsed`; a deny takes one step and keeps the budget and used digests (Q-S0-6). Helpers: `stepLeft`, `usedHas`, `auTake`. L2 stays on the `Budget` ops; the gate holds the `Budget` (S9). |

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
  `StepBound` is `Size` (S5): a tick is `sizePred`, and it stops at `sz0`.
- New refusal code `REFUSE_TIME`: a program names one of 10 clock words
  (`now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `date`, `ttl`,
  `deadline`, `expires`; Q-S5-2 (a)). The fix hint says to count
  operations: a deadline is a `StepBound` (a count of later operations)
  (Q-S0-9).
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
  the rest. `split : Budget -o Nat -> Option (Prod Budget Budget)` gives
  two budgets whose sum is the input (L2): the `n` units first, the rest
  second. A copy of a budget is `REFUSE_AFFINE_REUSE`.
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

S6 result (2026-11-02). `spend` and `split` are kernel builtins with the
`RULE_BUDGET` shape, guest-visible (Q-S6-2 (a)). `budget` and
`budgetLeft` are kit-only (Q-S6-1 (a)). A budget value is a tagged
`Nat`. The domain has `Capability`, `sizeLe` and `delegate` (Q-S6-4
(a)). `spend`, `split`, `budget`, `budgetLeft`, `Capability`, `cap`, `capScope`, `capSteps`, `capBudget`, `sizeLe` and `delegate`
are core names (`REFUSE_NAME`). `Capability` holds a `Budget`, so it is
a resource type: `test/check/capability-reuse.lang`
(`REFUSE_AFFINE_REUSE`) and `capability-omega.lang`
(`REFUSE_AFFINE_OMEGA`). L2 is the refl laws `lawSplit`, `lawSpend`,
`lawShort` and `lawLeft` in `examples/capability.lang`. The rows above
that say "Not yet" or "Partly" for `spend`, `split` and `Capability`
are now covered.

## 7. Laws

Each law is a guest-language term with a `Path` type. A law on closed rows
is `refl`; a general law uses induction where the kernel has it.
`langc check --json` counts each law in `laws_discharged` and `laws_total`.

| Law | Statement |
|---|---|
| L1 attenuation | `delegate` gives a child with `scopeWithin child parent`, child steps not more than parent steps, and child budget plus rest budget equal to the parent budget. A widening gives `none`. |
| L2 conservation | `split b n` gives budgets with sum `b`. `spend b n` gives `rest` with `rest + n = b`. |
| L3 approval binding | `decide` permits a `Call` of an execute `m` with approval `a` only when `digestEq (approvalDigest a) (hash (canon m))` (S7: `judge`, `examples/decide.lang`). `canon` is injective on (target, canonical payload) values as defined in section 4. The binding then rests on the collision resistance of `hash`, which is an assumption, not a law (O5). |
| L4 steps monotone | Along `advance`, the steps never increase. Each decided call takes one step until 0. At 0, each decision is `deny boundSpent`. |
| L5 sequence guard | On each path of a `Plan`, each `planExecute m` lies in the `ppApproved` branch of an earlier `planPropose m'`, with `m'` convertible to `m` (Q-S7-10 (a)). The checker refuses a path without one: `REFUSE_EXECUTE_UNAPPROVED`. The gate pairs the execute with the approval that the propose got. |
| L6 scope soundness | `decide` permits a call only when `allows scope target action`. |
| L7 at most once | After a permitted `execute` with approval `a`, each later call with `a` gives `deny approvalUsed`. |
| L8 agreement | `langc eval` of `decide` and the Wasm module in the gate give the same `Decision` on each row. This is a gate row, not a law term. |

Static refusals: `REFUSE_EXECUTE_UNAPPROVED` (L5), `REFUSE_DELEGATE_WIDEN`
(L1, when the scopes are literals), `REFUSE_BUDGET_OVERSPEND` (the sum of
the literal costs on a path is more than the literal budget),
`REFUSE_AFFINE_REUSE` (section 6), `REFUSE_TIME` (section 5),
`REFUSE_PARTIAL` (section 2). S1 ruled the name `REFUSE_TIME` (Q-S0-9
(a)). S6 adds `REFUSE_DELEGATE_WIDEN` and `REFUSE_BUDGET_OVERSPEND` for
closed rows, at check time and at a term level of 1 (not in a law type):
a `delegate` application with closed scopes where `scopeWithin` gives
`flagNo` (Q-S6-5 (a)), and a closed `spend`, `split` or `delegate`
application whose budget part reduces to `none` (Q-S6-6 (a)). The sum
along `Plan` paths is O20 (Q-S7-9 (a)). S7 adds
`REFUSE_EXECUTE_UNAPPROVED` at check time, at a term level of 1, on a def
of type `Plan A`. The walk is on the value: a def name unfolds, and a
neutral term stops the walk. `planPropose m'` adds `m'` for the
`ppApproved` branch only. Each `planExecute m` needs a convertible `m'`.
There is no use count: a second execute of `m` under one propose checks,
and the gate denies the reuse at run time (`usedHas`, `deny
approvalUsed`; at S8 `usedHas` is a bounded `REPEAT` over the used words). The other names stay proposals until the slice that adds
the code.

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
| T1 MCP gate | `src/wasm.c` (kit) writes one `.wasm` for each program, with the exports `decide (target, action, payload, cost, has_digest, digest : i64) -> i64`, `malformed () -> i64` and `memory` (S8, `docs/host/README.md`): the code is 0 for `permit` and 1 to 8 for the `Reason` arms. New `host/` in C99 that TinyCC builds: the gate `build/permitd`. It sits between the agent and the upstream MCP server (a child process on stdio), holds the credential, makes the canonical bytes of each call, hashes them (SHA-256), reads approvals from the approver channel, runs `decide` in its Wasm engine (Q-S0-1), and forwards only a permitted call. | one `.wasm` for each program, and `build/permitd` | node and wasmtime run each module as differential oracles (L8); the demo rows in `test/host/` |
| T2 JSON authority report | `src/report.c`: the static summary from `langc report` (scope, steps, budgets, laws). `host/report.c`: one line for each call from the gate, `permitd --report FILE` (the call, the decision, the reason code, the steps and the budget left, the digest that the gate checked). Schema in `docs/REPORT-JSON.md`. | JSON text | byte-equal rows in `test/report/` and the report rows R1 to R8 of `test/hosttool.c` |
| T3 Cedar export | `src/cedar.c`: `langc cedar` writes the static scope as Cedar `permit` policies (S11). It walks the `program` object of `report_value`. Steps, budgets, the sequence guard and the approval binding have no Cedar form. The export writes one comment for each part that it leaves out (6 comments, with the laws and the bind lines). Text in `docs/CEDAR.md`. | `.cedar` text | golden files in `test/cedar/`, one for each `test/report/` row (O3) |

`langc mcp` (A4) is a different program from `permitd`. `langc mcp` is the
authoring server for agents that write permit-lang (check, fmt, explain).
`permitd` is the enforcement gate between an agent and a real tool.

### 8.3 The demo

A fake deployment tool `test/fakedeploy.c` (C99, TinyCC) has seven tools:
`inspect`, `propose`, `execute`, `sync_left`, `sync_right`, `restart` and
`delete` (Q-S9-10 (a)). The agent gets the permit program, never the
credential. The rows D1 to D8, D10 and D11 are rows of `test/hosttool.c`
on the module of `test/host/deploy.lang` (S9 U4). D9 is the static row
`test/check/delegate-widen.lang` (Q-S9-7 (a)). The rows:

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
| D9 | a sub-agent program delegates a closed scope wider than its parent | `REFUSE_DELEGATE_WIDEN` at check time (static, Q-S9-7 (a)); the run-time `deny widen` is O22 |
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
| A1 | "one canonical form per operation and a langc fmt canonical printer" | No aliases, no second syntax. `langc fmt PROG` prints the canonical form. `langc fmt PROG --check` exits 1 when a file is not canonical (Q-S12-1). | S12 | `fmt` is idempotent; `parse (fmt x)` has the same core term as `x`; each example and test file is canonical. |
| A2 | "langc check --json, giving fix hints and progress counts (parsed, typed, laws discharged)" | One JSON line in JCS form: `diagnostics` (0 or 1: `code`, `span`, `message`, `fix`: a concrete edit or `null`) and `progress` (`parsed`, `typed`, `laws_discharged`, `laws_total`), Q-S13-1 to Q-S13-3. Schema in `docs/CHECK-JSON.md`. | S13 | 28 rows in `test/json/`, byte equal. Each `REFUSE_` code of `src/front/check.c` has a row, except `REFUSE_LAW` (Q-S13-9); each refused row has a `.fix` row (26). |
| A3 | "a SKILL.md with 3 to 5 graded examples" | `skills/permit-lang/SKILL.md` with five graded examples in `examples/skill/`: G1 a read-only scope with a step bound, G2 a budget with costs and a split, G3 an attenuated delegation to a sub-agent, G4 propose and execute of one approved mutation, G5 the demo. SKILL.md quotes each file. | S15 DONE | The gate checks each example and checks that each SKILL.md block and its file are byte equal. |
| A4 | "an MCP server written in TinyCC (langc mcp)" | An MCP server over stdio (JSON-RPC 2.0, one message on each line), in C, in the same `langc`. Tools: `check`, `fmt`, `eval`, `build` (with a target), `explain` (a refusal code), `list-base` (section 4), `show-law` (section 7). The MCP spec version is pinned in `docs/MCP.md` (`## langc mcp`), Q-S14-1 to Q-S14-8. | S14 DONE | A C tool `test/mcptool.c` sends the 11 rows `test/mcp/*.in` and compares the replies with `*.out`, byte equal. 26 explain codes, 15 base names. |
| A5 | "AGENTS.md and a README table saying when to use the language" | `AGENTS.md` (the gate command, the layout, the rules) and the README table "When to reach for permit-lang". Both exist at S0, and S15 makes them final. | S0, S15 DONE | Review only. |
| A6 | "a token-count comparison against Python and Rust" | G1 to G5 and the demo in permit-lang, Python and Rust, with equal outputs. Report `docs/TOKEN-BASELINE.md`: the tokenizer pin, each count, each file hash (Q-S0-10). | S16 DONE | `make check`: `token report hashes: 15 of 15` and `token estimate: passed` (Q-S16-3 (a)). `make tokens` (the API route, Q-S16-1 (a)) is not in `make check`. |

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
of it. S7: 21 mutants, 15 killed in the first run. The 6 survivors got
rows (`examples/decide.lang` `nestedPlan`, `lawOrderScope`,
`lawOrderBound`, `lawOrderBudget` and `lawOtherRoot`;
`test/ir/payload.lang`; `test/check/kit-payload-of.lang`), and the rerun
killed each one. S8: 11 mutants (the `memory` export, the export count,
the bind export names, the `within` of the node and the wasmtime host, the
gate code map, the signed words, the steps and the budget left of the
`decide` epilogue, the deny code and the used count), 11 killed in the
first run. The bind export name row (`test/ir/bindmemory.lang`) came
before the run. A second S8 run added 15 mutants of the S8 checker,
parser, domain and lowering sites (`res_text_ok`: upper case, an empty
segment, `*`, a trailing `/` and the empty text; `pointer_ok`: no first
`/`, `~2`, `~` at the end; `check_bind`: two bind lines for one tool, an
action that is not an arm; `start_def` false and true; the `bind` stop
of `parse_app`; the reason and the step of `judgeMalformed`; the
`judgeMalformed` pick of the `malformed` export), 15 killed in the first
run by the S8 rows. S8 total: 26 mutants, 26 killed. S9: 32 mutants of the host (U1: 8, the engine, SHA-256 and
`test/enginetool.c`; U2: 7, the JSON reader, JCS and the pointer; U3: 7,
the proxy, the approver, the state and the cost; U4: 10, the demo rows D1 to D8,
D10 and D11), 31 killed in the first run. The survivor O1 (the hash of
`test/enginetool.c`) got the `engine hash` rows, and the rerun killed it.
C1 (the cost of a call) was a compile kill only; C1b keeps the function
in use, and the `--cost` row killed it. Each U4 mutant is killed by its
own D row (Q-S9-U4-1 (a) for D10). S9 total: 32 mutants, 32 killed.
S10: 14 mutants (U1: 7 of `src/report.c`, the JCS pass, a word as a
number, the scope order, the program-origin test, the Path-type test,
the no-start scope and the action name; U2: 7, R1 to R7, of
`host/report.c` and the report sites of `host/permitd.c`), 14 killed in
the first run, each by a report row. S10 total: 14 mutants, 14 killed.
S11: 13 mutants (U2: C1 to C8c) of `src/cedar.c` and the JCS buffer of
`src/report.c`: `in` to `==`, a fixed action, the first grant only, the
scope reversed, the no-start text with the full header, the comments
before the policies, the bind lines before the laws, a tool name with no
quotes, a tool name with no escape, the dropped `fflush` status, and 3
sites of the JCS buffer. 11 killed in the first run. C8a (the buffer
starts at 16) is EQUIVALENT: each row then goes through the doubling,
and the doubling gives the same bytes (C8b, 16 and no doubling, is
killed). C8c (4096 and no doubling) survived, because no row had a
report line over 4096 bytes. Q-S11-6 (a) added the `big` row, and the
rerun killed C8c. S11 total: 13 mutants, 12 killed, 1 equivalent.
S12: 11 mutants (U3: M1 to M11) of `src/front/fmt.c`, `src/front/lexer.c`,
`src/main.c`, `test/fronttool.c` and `test/fmt/refused.txt`: a dropped
refused row, exit 0 on a `--check` mismatch, parentheses at the same
level, a comment region that stops one line early, no bind run, `@1`
printed as `@0`, `-o` printed as `->`, no H1 group, a dump with no binder
distance, a string with no closing quote, and a NUL byte that lexes. 10
killed in the first run. M9 (each bound variable prints `#0`) is
EQUIVALENT under the gate oracle (Q-S12-9): each gate use of the dump
compares 2 dumps, and the distance follows from what the dump prints. S12
total: 11 mutants, 10 killed, 1 equivalent.
S13: 8 mutants (U3: M1 to M4 and M6 to M9, Q-S13-7 (a)) of `src/main.c`,
`src/front/check.c`, `src/front/base.c` and `test/json/`: the span end
column + 1, `parsed` in the `typed` slot, `laws_total` with no syntax
scan, `fix_set` 0, the diagnostic on stderr with `--json`, no
`REFUSE_DATA` row, `laws_discharged` off by one and the start column of a
check span + 1. 8 killed in the first run, each by the json block (M7 by
`json refusal codes: 20 of 21`). S13 total: 8 mutants, 8 killed.
S14: 21 mutants (U3: M1 to M21) of `src/mcp.c`, `src/mcp.h`,
`src/main.c`, `src/front/codes.c`, `src/front/plantab.c` and
`domain/domain.lang`: each of the 5 RPC error codes, a reply to a
notification, the `tools/list` bytes, an empty tool result, no
`protocolVersion` check, the protocol pin, the line cap, the
`REFUSE_LAW` explain row dropped, a base name, a law text, a base row
text, `tools/list` without `show-law`, the `show-law` dispatch, the
domain `family Size` renamed, and the accrue-lang M1 to M3 of
`src/main.c` (the `--json` flag, `fmt --check`, the argument count). 20
killed in the first run, each by the mcp block. M5 (the code of -32603)
SURVIVED the block and the full gate (exit 0, 0 FAIL): only a failure of
`open_memstream` or `fclose` gives -32603, and no row reaches it (as
accrue-lang S13 M15). The swap of the `main` and `runtime` build targets
is equivalent: the two give the same Wasm bytes (`src/target.h`), so it
has no mutant (as accrue-lang Q-S13-7). Each mutant was restored and
compared byte equal; the end rebuild gave the base counts. S14 total: 21
mutants, 20 killed, 1 survivor.
S15: 6 mutants (U3: M1 to M6, the cairn-lang S14 set) of
`test/skilltool.c` and the skill block of `test/gate.sh`, each with the
serial `test/gate.sh`. The bad-exit fixture kills M2 (the exit code is
not compared): `skill negatives: 2 of 3`. 4 `REFUSE_KIT` lines kill M4
(the skill block drops `--kit`): `skill checks: 1 of 5`. M1 (only the G1
program block is compared) and M3 (the block compare does not compare
the last byte of the file) are LIVE (O25). M5 (the skill block skips
`langc fmt --check`) gives exit 0, but the fmt canonical row reads
`examples/*/*.lang`: the probe M5P (M5 and a G5 file with one more
newline) fails with `FAIL fmt examples/skill/g5-deploy.lang: not
canonical`. Thus the M5 gap of cairn-lang O37 is not in permit-lang. M6
(a negative fixture that passes adds no failure) is equivalent, because
the 3 fixtures fail as given. The probe M26 (M2 and M6) writes `FAIL
skill negative bad-exit: exit 0` and `skill negatives: 2 of 3`, but the
gate exits 0 (O25). Each mutant was restored and its sha1 is the pin;
the end run gave the base counts. S15 total: 6 mutants, 2 killed, 2
LIVE, 1 covered, 1 equivalent.
S16: 6 mutants (U3: M1 to M6) of `test/tokcount.c`, the report and the
token recipes of the `Makefile`, each with the serial `make -B check`.
M1 (a counted path of `test/tokcount.c` names a file that is not there)
gives `token report hashes: 14 of 15` and `cannot read the file`. M2 (1
hash byte of the G1 row) gives `token report hashes: 14 of 15`. M3 (1
estimate number of the block) gives `FAIL tokens: the estimate block`.
M5 (no `|| exit 1` after the `langc eval` producer of the `for row in`
recipe) and M6 (no FAIL clause after the Python producer of the `for g
in` recipe) give `FAIL tokens: accepted failed langc producer` and
`accepted failed python producer` (the `test/review.sh` token block).
M4 (G5 also runs with `--kit`) gives exit 0, and it is equivalent: the
kit mode admits each guest file, and `langc --kit eval` of
`examples/skill/g5-deploy.lang` gives the same 3 lines as `langc eval`.
Each mutant was restored: the sha1 of each file is its pin and the `git
diff` sha is the STEP 1 value. S16 total: 6 mutants, 5 killed, 1
equivalent.

## 11. Port sources

Pinned on 2026-11-01. A slice that ports from a repo records the hash that
it read.

| Repo | Hash | What | Where |
|---|---|---|---|
| lang-template | 5265a2a | the tcc-wasm kit, `bin/new-lang.sh` | whole kit |
| relay-lang | 5c386bb (seed 145f58f) | R0 rules N1 to N6, `test/notraptool.c`, the Wasm engine ruling, the C host shape; S5: the natops case-split row `reasonCode` (O15) | seed docs/PLAN.md sections 2 and 12 (Q-S0-1); docs/PLAN.md line 51 (S1 ruling); test/ir/natops.lang lines 11 to 13 |
| accrue-lang | d1017f1 | R0 and R1 readings, the A1 to A6 table, `REFUSE_PARTIAL` | docs/PLAN.md lines 160 to 214, 374 to 397 |
| accrue-lang | 20852a1 | S5: the `cofamily Books` shape with its refl laws, read for the `Allowance` cofamily and the L4 rows | domain/domain.lang lines 680 to 690 |
| cairn-lang | 60768e8 | the no-time reading and `REFUSE_TIME`; the kernel source; `test/shatool.c` | docs/PLAN.md lines 235 to 248 |
| leaf-lang | 5ccc494 | the affine layer and `REFUSE_AFFINE_REUSE`; the dash skip for DESIGN.md | docs/PLAN.md lines 561 to 600; test/gate.sh line 889 |
| mandate-lang | 33556e9 | the budget law (`spent` does not decrease, `spent <= cap`); the `autoLimit` confirmation rule as a reading for Q-S0-11; S5: the `cofamily Machine` shape (`eps`, `mStep : Req -> Machine A`), the shape of `Allowance` with `Call` for `Req` | docs/PLAN.md line 583; lines 410 to 415; domain/domain.lang lines 70 to 71 |
| tranche-lang | 9aafbf8 | S5: the idiom of a prefix test over a `FreeMon` (`prefix`, `fmRun`), read for `scopeWithin`; permit counts the matches with `fmBindNat` and `len` instead, because it has no `fmRun` | domain/domain.lang lines 201 to 222 |
| zkpolicy-lang | 1d263fb | overlap only (DESIGN.md 3.1); the later target | none in v1 |
| relay-lang | 4781ca7 (file commit 8d4382a) | S9: the Wasm engine, copied to `host/engine.c` and `host/wasmread.c`; S9 adds the import section, the data section, the (module, name) binding, the memory word write and the kept `memory` export | host/engine.c, src/wasmread.c |
| quorum-lang | be2c329 (file commit 8e18ebf) | S9: SHA-256, copied to `host/sha256.c` and `host/sha256.h` (no U256 dependency) | src/crypto/sha256.c |
| cairn-lang | 60768e8 | S9: `test/shatool.c`, the 4 FIPS 180-4 vectors; S9 adds 3 pad rows (55, 56 and 64 bytes) | test/shatool.c |
| relay-lang | 4781ca7 (file commit 4781ca7) | S9 U2: the JSON reader, copied to `host/jsonin.c` and `host/jsonin.h`; S9 adds the duplicate key refusal and a static `utf8_char_size` | src/front/jsonin.c, src/front/jsonin.h |

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
| S5 | `Resource`, `Action`, `Scope`, `StepBound`, the affine `Allowance` (CC), `Decision`, `Reason`; `REFUSE_TIME` with 10 words; L4 and L6 as refl laws and eval rows in `examples/scope.lang`. Rulings Q-S5-1 to Q-S5-8 (section 1); sources in section 11; gate EXIT 0: 31 examples, 21 parse refusals, 213 check refusals, 11 kit-only refusals, 1 time hint, 85 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 2 natops evals, review, domain and path regressions passed. New rows: `test/check/time-{date,ttl,deadline,expires}.lang`, `kit-res-root.lang`, `allowance-{reuse,omega,open-twice,open-parts,unpack-twice,capture-budget,capture-allowance,capture-polymorphic}.lang`, `test/parse/affine-no-cofamily.lang`, 6 eval rows of `examples/scope.lang`, the `natops evals` line (O15). The original 17-mutant run killed 16; the live one (`trusted` for each generated decl of a cofamily, not only the last) is equivalent, because no other generated decl uses a 1-variable two times. Review adds three capture refusals: each rejects the checker before the review fix. Position closures cannot capture affine values, since `alOpen` observes more than one position. The generated destructors preserve positions; their other affine use checks still apply. Copyable captures and linear continuations still check (`lawConstant`, `lawKeep` in `examples/scope.lang`). | DONE |
| S6 | The `Budget` ops `spend` and `split` (kit-only `budget` and `budgetLeft`), `Capability`, `sizeLe`, `delegate`; `REFUSE_DELEGATE_WIDEN` and `REFUSE_BUDGET_OVERSPEND` on closed rows at a term level of 1; L1 and L2 as refl laws and eval rows in `examples/capability.lang`. Rulings Q-S6-1 to Q-S6-8 (section 1); O17 and O18 closed with no change; O19 fixed. Gate EXIT 0: 32 examples, 21 parse refusals, 219 check refusals, 11 kit-only refusals, 1 time hint, 88 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 2 natops evals, review, domain and path regressions passed. New rows: `test/check/budget-overspend-{spend,split}.lang` and `delegate-overspend.lang` (`REFUSE_BUDGET_OVERSPEND`), `delegate-widen.lang` (`REFUSE_DELEGATE_WIDEN`), `capability-reuse.lang` (`REFUSE_AFFINE_REUSE`), `capability-omega.lang` (`REFUSE_AFFINE_OMEGA`), 3 `sizeLe` eval rows of `examples/capability.lang`, 2 guest rows (`spend`, `split`) in `test/fronttool.c` and the O19 message row in `test/gate.sh`. 18 mutants, 18 killed. | DONE |
| S7 | `Payload`, `Mutation`, `canon`, `Digest`, `hash`, `Approval`, `Call`, `Plan` (CM), `Trace` (CC), `decide`; `REFUSE_EXECUTE_UNAPPROVED`; L3, L5, L7. Takes the host cost in `Call` (Q-S6-3 (a)) and the `Plan` path sum of `REFUSE_BUDGET_OVERSPEND` (Q-S6-6 (a)). Built: the kernel sorts `Payload`, `Bytes` and `Digest` with kit-only `payloadOf` and `approval`; `Mutation`, `canon`, the copyable `Ticket`, the affine `Call`, `Authority`, `judge`, `decide`, `Reply`, the first-order `Plan`, `Entry` and `Trace = Stream Entry`; `REFUSE_EXECUTE_UNAPPROVED` on a closed `Plan` def; `langc ir` refuses a `Payload` at run time (S8). Rulings Q-S7-1 to Q-S7-12 (section 1); the path sum is O20. Gate EXIT 0: 33 examples, 21 parse refusals, 224 check refusals, 12 kit-only refusals, 1 time hint, 92 evals, 1 build, 1 ir lowered, 6 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 2 natops evals, review, domain and path regressions passed. 21 mutants, 21 killed (section 10). | DONE |
| S8 | T1 part 1: the Wasm module of `decide`, the binding (Q-S0-5), the notrap rows; the differential and wasm runs lines (Q-S3-2 (b)). Takes a `Payload` at run time (the S7 `REFUSE_LOWER` row `test/ir/payload.lang`). Built: guest `res` literals (`REFUSE_RES`), `bind` lines (`REFUSE_BIND`), `def start` with its guest Nat literals, domain `judgeMalformed`; the host handles and the `permit` imports `envelope`, `hash` and `within`; the S8 lowerings (the `Size` word, the `REPEAT` elim, the nullary switch, `len`, `usedHas`, `IR_STMT_STORE`); the state region and its data section; the exports `decide`, `malformed` and `memory`; the custom sections `permit.res` and `permit.bind`; the notraptool import rows; `test/run-wasm.mjs` on node and wasmtime (`--wasmtime`, `--decide`, `--lang`) and the call scripts `test/decide/`. Rulings Q-S8-1 to Q-S8-12 (section 1); `needsApproval` is O21. Gate EXIT 0: 35 examples, 22 parse refusals, 236 check refusals, 13 kit-only refusals, 1 time hint, 92 evals, 1 build, 1 ir lowered, 8 ir refusals, 10 ir lowerings, 3 custom sections, 5 notrap modules, 11 notrap bad rows, 10 spine evals, 2 natops evals, 16 wasm runs, 694 differential cases with 0 mismatches, 8 decide scripts (12 calls, 0 mismatches), review, domain and path regressions passed. 26 mutants, 26 killed (section 10). | DONE |
| S9 | T1 part 2: `permitd`: the Wasm engine, SHA-256, the canonical bytes, the approver channel, the state, the stdio MCP proxy, `fakedeploy`, N6; the demo D1 to D11; L8. Built: `host/engine.c` and `host/wasmread.c` (the import and data sections, the (module, name) binding), `host/sha256.c`, `host/jsonin.c` and `host/jcs.c` (JCS, the envelope, the RFC 6901 pointer), `host/permitd.c` (the stdio MCP gate, the approver, the state file, the cost), `test/fakedeploy.c` (7 tools), `test/host/deploy.lang` (7 bind lines), the tools `test/shatool.c`, `test/enginetool.c`, `test/jcstool.c` and `test/hosttool.c`, `docs/MCP.md` (MCP 2025-11-25). Rulings Q-S9-1 to Q-S9-10 and Q-S9-U4-1 (section 1); the run-time `deny widen` is O22, a D10 row with the target only is O23. Gate EXIT 0: 35 examples, 22 parse refusals, 236 check refusals, 13 kit-only refusals, 1 time hint, 92 evals, 1 build, 1 ir lowered, 8 ir refusals, 10 ir lowerings, 3 custom sections, 5 notrap modules, 11 notrap bad rows, 10 spine evals, 2 natops evals, 16 wasm runs, 694 differential cases with 0 mismatches, 8 decide scripts (12 calls, 0 mismatches), 8 engine decide scripts (12 calls, 0 mismatches), 2 engine refusals, 2 engine hash rows, 7 sha256 vectors, 32 jcs rows, 28 host rows, review, domain and path regressions passed. 32 mutants, 32 killed (section 10). | DONE |
| S10 | T2: the JSON authority report, `docs/REPORT-JSON.md`. Built: `langc report` (`src/report.c`: the `program` line, the scope walker, the Path-type law walk), `permitd --report FILE` (`host/report.c`: the `call` and `malformed` lines, appended with `fflush` and `fsync` after the state write), 5 `test/report/` rows and the hosttool rows R1 to R8. Gate EXIT 0: the S9 counts, plus 5 report rows and 36 host rows and 2 report output checks. 14 mutants, 14 killed (section 10). | DONE |
| S11 | T3: the Cedar export, `docs/CEDAR.md`. Built: `langc cedar` (`src/cedar.c`: the header, one `permit` for each grant, the 6 "Not exported" comments; `report_value` in `src/report.c` is the one walker), 7 `test/cedar/` rows for the 7 `test/report/` rows (new: `empty` and `big`), the cedar write error check of `build/reporttool`, and the JSON blocks of `docs/MCP.md` (Q-S11-5). Gate EXIT 0: the S10 counts with 7 report rows, plus 7 cedar rows and 2 cedar write errors. 13 mutants, 12 killed, 1 equivalent (section 10). | DONE |
| S12 | A1: canonical forms and `langc fmt`. Built: `langc fmt PROG [--check]` (`src/front/fmt.c`, a port of accrue-lang S11 with the bind run, the marks, the strings and the H1 group; the comment table of the lexer and the end line of a decl), the scoped dump of `build/fronttool`, `LEX_STRING` for a NUL byte in a string (Q-S12-8), 178 files made canonical, `test/fmt/` (10 golden rows, 3 witness rows, `refused.txt`) and the fmt block of `test/gate.sh`, which also reads `examples/*/*.lang`. Rulings Q-S12-1 to Q-S12-9 (section 1). Gate EXIT 0: the S11 counts with 23 parse refusals, plus 300 fmt canonical, 312 fmt round trip, 22 fmt refused, 13 fmt rows and 11 fmt check. 11 mutants, 10 killed, 1 equivalent (section 10). | DONE |
| S13 | A2: `langc check --json`. Built: the spans of the lexer, the parser and the check in `Diag`, the fix sites (`LEX_CHAR`, `REFUSE_CELL`, `REFUSE_SIG`, `REFUSE_STATE`), the progress counts, `src/jsonout.c` (the JCS writer of `langc report`, now shared), `langc check --json PROG`, `build/fronttool fix`, the 28 `test/json/` rows and the json block of `test/gate.sh`, and `docs/CHECK-JSON.md`. Rulings Q-S13-1 to Q-S13-9 (section 1). Gate EXIT 0: the S12 counts, with 326 fmt canonical, 338 fmt round trip and 24 fmt refused, plus 28 json rows, 21 of 21 json refusal codes and 26 json fixes. 8 mutants, 8 killed (section 10). | DONE |
| S14 | A4: `langc mcp`. Built: `src/mcp.c` and `src/mcp.h` (the accrue-lang 02964ec server, 7 tools, pin `2025-11-25`), the verbs `langc explain`, `langc base`, `langc law` and `langc [--kit] mcp`, the tables `src/front/codes.c` (26 codes, the README text) and `src/front/plantab.c` (20 base rows, L1 to L8), the `FILE *` string writer of `src/jsonout.c`, `test/mcptool.c`, the 11 `test/mcp/` rows and the mcp block of `test/gate.sh`, 16 new README rules rows and the `REFUSE_REPORT` sentence, and `docs/MCP.md` `## langc mcp`. Rulings Q-S14-1 to Q-S14-8, Q-S14-U2-1 and Q-S14-U2-2 (section 1). Gate EXIT 0: the S13 counts, plus 11 mcp rows, 3 of 3 mcp negatives, 7 of 7 mcp tools, 2 of 2 mcp builds, 1 of 1 mcp long line, 26 of 26 explain codes and 15 of 15 base names. 21 mutants, 20 killed, 1 survivor (section 10). | DONE |
| S15 | A3 and A5: SKILL.md, G1 to G5, AGENTS.md and the README table final. Built: `skills/permit-lang/SKILL.md` (the routing rule, the loop, the verbs and the 5 graded examples G1 to G5; each program block is equal to its file in `examples/skill/`, and each row gives its output block and its exit code), `examples/skill/` (G1 to G4 are kit files, `kit.txt`; G5 is the demo, a guest file), `test/skilltool.c`, the 3 fixtures of `test/skill/` and the skill block of `test/gate.sh`, `AGENTS.md` (the cairn-lang shape), the README table "When to reach for permit-lang", README Status and Layout, 4 skill rows in `docs/host/README.md` and the skill gate paragraph in `docs/VALIDATION.md`. Rulings Q-S15-1 to Q-S15-4 (section 1). Gate EXIT 0: the S14 counts, with 331 fmt canonical and 343 fmt round trip (326 and 338 at S14; the 5 examples are new), plus 5 of 5 skill checks, 5 of 5 skill quotes, 22 of 22 skill outputs and 3 of 3 skill negatives. 6 mutants: 2 killed, 2 LIVE (O25), 1 covered by the fmt canonical row, 1 equivalent (section 10). | DONE |
| S16 | A6: the token comparison. Built: `test/baseline/g1-scope.py` to `g5-deploy.py` (the standard library only), `test/baseline/rust/` (1 crate, bins g1 to g5, `sha2` 0.10 for G4, `Cargo.lock`), the `.gitignore` line `test/baseline/rust/target/`, `test/tokcount.c` (the cairn-lang 0175ef1 tool with the permit-lang paths), the Makefile rows and the target `tokens`, the token recipe block of `test/review.sh` and `docs/TOKEN-BASELINE.md` (the bytes, the SHA-256 and the offline estimate ceil(bytes / 4) of each of the 15 files; the Python estimate is 1.63 times and the Rust estimate is 2.31 times the permit-lang estimate). Each Python and Rust program prints the `langc eval` lines of its G, byte for byte (`token outputs: 10 of 10`). `make tokens` is not run (no API key). Rulings Q-S16-0 to Q-S16-3 (section 1). Gate EXIT 0: the S15 counts, plus `token report hashes: 15 of 15` and `token estimate: passed`. 6 mutants: 5 killed, 1 equivalent (section 10). | DONE |
| S17 | Close: SPEC.md, probe/CAPABILITY.md, VALIDATION.md, final gate. Built: `SPEC.md` (all 10 sections, 91 cites), `probe/CAPABILITY.md` (new `probe/guard.py` runs: 72 guard rows, the cost at N = 100 and N = 1000, the `PENDING_MAX` limit of 256 open calls, a replay block and 9 consequences), `docs/VALIDATION.md` (the header and the S0 to S14 paragraphs), the rulings Q-S17-0 to Q-S17-6 (section 1), the standing marks of section 13, `AGENTS.md` `## Status (S17)` and the README Status. Gate EXIT 0: the S16 counts, 0 failures. A doc slice: no code change and no mutants. | DONE |

## 13. Open items

- O1 STANDING (v1 scope, Q-S17-3 (a)). The kit base data is `Nat` and
  `Flag`. Section 4 adds names only with a pasted line. A slice that needs
  more records why here first.
- O2 CLOSED at S3. The kit "Lazy traps" section and the gate row "a trap
  is equal to a trap" are gone. `docs/host/README.md` has "No traps"
  (the relay-lang 5c386bb text in permit words) and the S3 gate rows.
- O3 OPEN. The cedar CLI is not on PATH. T3 (S11) is checked by the 7
  golden files of `test/cedar/` only. A `cedar validate` row needs a USER
  install.
- O4 STANDING (v1 scope, Q-S17-3 (a)). The link to the thread in pasted 1
  did not survive the paste. The README states the problem without the
  link until the USER gives it.
- O5 STANDING (v1 scope, Q-S17-3 (a)). L3 rests on the collision
  resistance of `hash`. That is an assumption, not a law. v1 binds
  SHA-256.
- O6 STANDING (v1 scope, Q-S17-3 (a)). zkpolicy-lang is a later target
  (8.4), not v1.
- O7 STANDING (v1 scope, Q-S17-3 (a)). The pinned MCP spec version and the
  JSON-RPC subset go in `docs/MCP.md`: the `langc mcp` part at S14, the
  `permitd` proxy part at S9.
- O8 STANDING (v1 scope, Q-S17-3 (a)). The `fix` of each refusal code (A2)
  is one concrete edit. A code with no edit gives `fix: null` and a
  reason.
- O9 STANDING (v1 scope, Q-S17-3 (a)). The approver channel is a CLI
  prompt on the terminal. The gate rows have no terminal, so they use a
  file of approvals (test mode).
- O10 STANDING (v1 scope, Q-S17-3 (a)). A signed approval (an approver
  key) is not in v1.
- O11 STANDING (v1 scope, Q-S17-3 (a)). The gate reads the credential from
  a file or an environment variable that its flags name, and gives it only
  to the upstream child. A run on a real SaaS tool is a USER-run check.
- O12 CLOSED at S8. The S2 kernel port had no Wasm writer in the build
  (Q-S2-3 (a)).
  S3 brings back the IR lines (`ir`, `ir refusals`, `ir lowerings`), the
  build line (`builds`) and the notrap lines (`notrap modules`, `notrap
  bad rows`) (Q-S3-2 (b)). The differential line and the `wasm runs`
  line ran node, so they were out of the S3 gate. S8 brings them back on
  node and wasmtime (Q-S8-6 (a)).
- O13 CLOSED at S3. The port source is relay-lang 5c386bb (Q-S3-1 (a));
  section 2, "S3 result".
- O14 CLOSED at S5 (Q-S5-2 (a)). `REFUSE_TIME` has 10 words: the cairn
  list (`now`, `block`, `timestamp`, `time`, `clock`, `epoch`) and
  `date`, `ttl`, `deadline`, `expires`. The hint says to count
  operations: a deadline is a `StepBound`. Rows:
  `test/check/time-{date,ttl,deadline,expires}.lang` and 4 rows in
  `test/fronttool.c`.
- O15 CLOSED at S5. `reasonCode` in `test/ir/natops.lang` folds the 8
  arms of the domain `Reason` on `flagIf (natEq n 0) scopeMiss
  boundSpent`. `test/ir/natops.ir` grew from 92 to 120 lines (the old
  text is a prefix). The gate line `natops evals: 2 checked` runs
  `reasonCode 0` and `reasonCode 1`.
- O16 STANDING (v1 scope, Q-S17-3 (a)). The S4 rule for the permit forms
  with no leaf rule is a sound fallback (Q-S4-4 (a)). A part that can run
  more than one time is checked at omega. So the checker refuses some
  sound programs: a `map` whose `f` drops a resource element, and a `map`
  from `Option Budget` to `Option Unit`. A precise rule for a form comes
  only when a demo row needs one.
- O17 CLOSED at S6 (Q-S6-7 (a)), no change. `alCorec` stays
  guest-visible (Q-S5-6 (a)). A rebuild from the parts of an opened
  `Allowance` cannot add units: the gate holds the one live value (S8,
  S9), and a guest cannot make a `Budget`.
- O18 CLOSED at S6 (Q-S6-8 (a)), no change. The checker emits no
  `AllowanceBisim` for an affine cofamily. No S6 law needs one; the
  `Allowance` laws are closed refl rows.
- O19 CLOSED at S6. `value_show` in `src/front/eval.c` prints an affine
  cofamily type (the Pi `D.pos -> A`) as `D A` in a diagnostic, so the
  `REFUSE_AFFINE_OMEGA` message names `Allowance A`. Row: the
  `allowance-omega.lang` message row in `test/gate.sh`.
- O20 OPEN (Q-S7-9 (a)). `REFUSE_BUDGET_OVERSPEND` checks one closed
  `spend`, `split` or `delegate` term. No check sums the costs along a
  `Plan` path: a `Plan` has no cost field. The cost comes with the
  `Ticket` at run time, and `judge` gives `deny budgetShort` when it is
  more than `auLeft`.
- O21 OPEN (Q-S8-7 (a)). `judge` keeps `execute` as the one marked
  action. The `needsApproval` form comes when a demo row needs another
  marked action. The `Authority` shape and the 13 USER laws stay as they
  are.
- O22 OPEN (Q-S9-7 (a)). D9 is a static row (`REFUSE_DELEGATE_WIDEN`). No
  run-time path gives `deny widen`: the module does not hold the child
  scope, and the proxy has no delegation request. A run-time row needs
  both (for example a `permit.scope` custom section and a parent module
  in `permitd`).
- O23 OPEN (Q-S9-U4-1 (a)). The target of a call comes from the bind
  pointer of its tool into the arguments, and the payload holds the name
  and the arguments. In one module each tool has one bind line, so no
  host row changes the target of the envelope alone. The D10 mutant drops
  the target and the name. A row where only the target changes needs one
  tool name with two bind pointers, so two modules (a rebind with the
  same approvals file).
- O24 OPEN (Q-S10-3 (a)). The `call` line comes before the forward, so
  the report does not hold the upstream outcome (the result, an
  `isError` answer or a closed upstream). A line for the outcome needs a
  new line kind with the id, written after the answer of the upstream
  server.
- O25 OPEN (S15, as cairn-lang O37). 2 mutants of the skill gate are
  LIVE, and no row kills them. M1 (`build/skilltool` compares only the
  G1 program block) is LIVE, because each fixture holds only G1, so no
  fixture changes a block of G2 to G5; a fixture with 2 examples and a
  changed byte in G2 kills it. M3 (the block compare does not compare
  the last byte of the file) is LIVE, because no fixture differs from
  its file only in the last byte; a fixture whose file has no newline
  at the end kills it. With M2 and M6, the bad-exit fixture passes: the
  gate writes its FAIL line and `skill negatives: 2 of 3`, but it exits
  0. No slice owns this gap yet.

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
