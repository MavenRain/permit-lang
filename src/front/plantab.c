/* The tables of `langc base` and `langc law LN` (S14 A4, Q-S14-3 (a),
   Q-S14-4 (a)). BASE_ROWS are the 20 rows of the docs/PLAN.md section 4
   table, in the same order: the Name cell without its backticks, and the
   Kind and Line cells verbatim. LAW_ROWS are the laws L1 to L8 of the
   docs/PLAN.md section 7 table: the id, the name and the statement
   verbatim. Ported in shape from accrue-lang src/front/codes.c (02964ec). */
#include <string.h>

#include "plantab.h"

static const BaseRow BASE_ROWS[] = {
  {"Resource", "kernel sort, copyable (Q-S5-3 (a)); kit-only `resEq`, `within : Resource -> Resource -> Flag`, `resRoot : Nat -> Resource` and `resChild : Resource -> Nat -> Resource`", "P3"},
  {"Action", "a domain sum: `family Action` with the arms `inspect`, `propose` and `execute` in `domain/domain.lang` (Q-S5-1 (a))", "P2, P8"},
  {"needsApproval", "a mark on an `Action`", "P7, P8"},
  {"Scope", "`FreeMon (Prod Resource Action)`; `grants`, `allows`, `scopeWithin`", "P3, P5"},
  {"StepBound", "`Size` (Q-S5-4 (b)): a count of calls left", "P4"},
  {"Budget", "kernel sort, affine; guest `spend` and `split` (Q-S6-2 (a)); kit-only `budget : Nat -> Budget` and `budgetLeft : Budget -o Nat` (Q-S6-1 (a))", "P6"},
  {"Capability", "domain `family Capability := cap (capScope : Scope) (capSteps : StepBound) (capBudget : Budget)` (Q-S6-4 (a))", "P1, P5"},
  {"delegate", "a domain def, `Capability -o Scope -> StepBound -> Nat -> Option (Prod Capability Capability)` (Q-S6-4 (a))", "P5"},
  {"Payload", "kernel sort, copyable, opaque canonical tool-call bytes; kit-only `payloadOf : Nat -> Payload`, guest `payloadEq` (Q-S7-1 (a)); equality and `canon` input only", "P7, P9, P10"},
  {"Mutation", "domain `family Mutation := mutation (mTarget : Resource) (mAction : Action) (mPayload : Payload)` (Q-S7-2 (a))", "P7, P9"},
  {"canon", "domain def `Mutation -> Bytes`, `envelope (mTarget m) (mPayload m)`: the canonical bytes of (target, payload)", "P9"},
  {"Bytes, Digest, hash, envelope", "kernel sorts `Bytes` and `Digest` (copyable, opaque); guest `digestEq`, `hash : Bytes -> Digest` and `envelope : Resource -> Payload -> Bytes` (Q-S7-1 (a))", "P7, P9"},
  {"res, bind, start, judgeMalformed", "guest `res \"a/b\"` of type `Resource` (Q-S8-3 (a), `REFUSE_RES`); a top-level `bind \"tool\" action \"/pointer\"` (Q-S8-4 (a), `REFUSE_BIND`); `def start : Authority`, closed, with a closed `StepBound` k (Q-S8-5 (a)), the one place of a guest Nat literal (Q-S8-8 (a)); domain `judgeMalformed : Authority -> Prod Decision Authority`", "P2, P3, P10"},
  {"Approval", "the S4 kernel sort, affine, over one `Digest`; kit-only `approval : Digest -> Approval`, guest `approvalDigest : Approval -o Digest` (Q-S7-3 (a))", "P7"},
  {"Call", "domain `family Call := call (callMutation : Mutation) (callCost : Nat) (callApproval : Option Approval)` (Q-S7-4 (a))", "P2, P10"},
  {"Ticket", "domain `family Ticket := ticket (tkMutation : Mutation) (tkCost : Nat) (tkDigest : Option Digest)`, copyable; `callTicket : Call -o Ticket` (Q-S7-11 (a))", "P2, P10"},
  {"Decision, Reason", "`permit` or `deny Reason`; `Reason` is the closed sum of section 2", "P3 to P9"},
  {"Allowance", "`affine cofamily Allowance (A : Type 0) := alCorec (eps : A) (advance : Ticket -> Allowance A)`; `alOpen : Allowance A -o Prod A (Ticket -o Allowance A)` (S7: over `Ticket`, Q-S7-11 (a))", "P2, P4, P10"},
  {"decide", "domain def `Allowance Authority -o Call -o Prod Decision (Allowance Authority)` (Q-S7-6 (a))", "P10"},
  {"Authority", "domain `family Authority := authority (auScope : Scope) (auSteps : StepBound) (auLeft : Nat) (auUsed : FreeMon Digest)`, copyable (Q-S7-12 (a)); `judge : Authority -> Ticket -> Prod Decision Authority`; `allowanceOf : Authority -> Allowance Authority`", "P3 to P10"},
};

static const LawRow LAW_ROWS[] = {
  {"L1", "attenuation", "`delegate` gives a child with `scopeWithin child parent`, child steps not more than parent steps, and child budget plus rest budget equal to the parent budget. A widening gives `none`."},
  {"L2", "conservation", "`split b n` gives budgets with sum `b`. `spend b n` gives `rest` with `rest + n = b`."},
  {"L3", "approval binding", "`decide` permits a `Call` of an execute `m` with approval `a` only when `digestEq (approvalDigest a) (hash (canon m))` (S7: `judge`, `examples/decide.lang`). `canon` is injective on (target, canonical payload) values as defined in section 4. The binding then rests on the collision resistance of `hash`, which is an assumption, not a law (O5)."},
  {"L4", "steps monotone", "Along `advance`, the steps never increase. Each decided call takes one step until 0. At 0, each decision is `deny boundSpent`."},
  {"L5", "sequence guard", "On each path of a `Plan`, each `planExecute m` lies in the `ppApproved` branch of an earlier `planPropose m'`, with `m'` convertible to `m` (Q-S7-10 (a)). The checker refuses a path without one: `REFUSE_EXECUTE_UNAPPROVED`. The gate pairs the execute with the approval that the propose got."},
  {"L6", "scope soundness", "`decide` permits a call only when `allows scope target action`."},
  {"L7", "at most once", "After a permitted `execute` with approval `a`, each later call with `a` gives `deny approvalUsed`."},
  {"L8", "agreement", "`langc eval` of `decide` and the Wasm module in the gate give the same `Decision` on each row. This is a gate row, not a law term."},
};

size_t base_count(void) { return sizeof BASE_ROWS / sizeof BASE_ROWS[0]; }

const BaseRow *base_at(size_t index) { return index < base_count() ? &BASE_ROWS[index] : NULL; }

size_t law_count(void) { return sizeof LAW_ROWS / sizeof LAW_ROWS[0]; }

const LawRow *law_at(size_t index) { return index < law_count() ? &LAW_ROWS[index] : NULL; }

const LawRow *law_find(const char *id) {
  for (size_t i = 0; i < law_count(); i++) {
    if (strcmp(LAW_ROWS[i].id, id) == 0) return &LAW_ROWS[i];
  }
  return NULL;
}
