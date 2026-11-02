# permit-lang

permit-lang is a language for executable capability protocols for agents.
An agent often needs a narrow operation, but the SaaS credential behind the
tool grants much broader access. A permit program states the authority over
a sequence of tool calls:

- the resources and actions in scope;
- the expiry, as a count of calls (no clock);
- delegation that can only narrow the authority;
- budgets that a call consumes, at most once;
- approval that is bound to the digest of the exact proposed mutation.

The first demo: an agent can inspect a deployment and propose a change, but
it can execute only the exact approved change. A change to the target or to
the payload makes the approval not valid.

The checker checks these rules before a run. A gate (`permitd`, a C program
that TinyCC builds) holds the credential, sits between the agent and the
real tool, and enforces the same rules again on each call. No program can
trap: a denial is a value with a reason code.

## When to reach for permit-lang

| Use permit-lang when | Use something else when |
|---|---|
| An agent needs a narrow, bounded slice of a broad credential. | You need ordinary authorization rules only: use Cedar. permit-lang exports its static scope as Cedar. |
| A sequence of calls must stay inside one authority (scope, call count, budget). | You need a zero-knowledge proof that a decision obeys a policy: use zkpolicy-lang. |
| A mutation must run only after an approval of those exact bytes. | You need agent payments on chain: use mandate-lang. |
| A sub-agent gets a delegated capability that must not widen. | You need general-purpose code. |

## Status

S6: the budget ops, `Capability` and `delegate`. The repo holds the Cubical TT kernel of intent-lang
(67445ae), ported from relay-lang 05dd63b without the crypto code, on the
tcc-wasm host, and the plan. S3 adds R0 from relay-lang 5c386bb: total
Nat operations, `REFUSE_PARTIAL`, and a Wasm writer that emits no trap
form (`test/notraptool.c` checks each built module). S4 adds the affine
layer from leaf-lang 47a6f36: the marks `@0` and `@1`, `A -o B`,
`unpack`, the resource sorts `Budget` and `Approval`, and the codes
`REFUSE_AFFINE_REUSE`, `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and
`REFUSE_AFFINE_PATH`. The prelude has `fmBindNat`, `fmBindSize`,
`fmMapNat`, `fmMapSize`, `fmEtaNat` and `fmEtaSize` in place of `fmBind`
and `fmMap`. The examples have `orbitNExtBag` and `isSetAtLin`, and
`map0`, `map1` and `truncExt` are at fixed types. S5 adds the domain
types in `domain/domain.lang`: `Action`, `Reason`, `Decision`,
`StepBound := Size`, the `Call` stand-in, `Scope` with `allows` and
`scopeWithin`, and the affine `cofamily Allowance` with `alOpen`. The
kernel has the sort `Resource` with `resEq` and `within`, and
`REFUSE_TIME` names 10 time words. `examples/scope.lang` is the S5
demo (L4, L6). S6 adds the guest ops `spend` and `split` on a `Budget`
(kit-only `budget` and `budgetLeft`), the domain `Capability`, `sizeLe`
and `delegate`, and the codes `REFUSE_DELEGATE_WIDEN` and
`REFUSE_BUDGET_OVERSPEND` on closed rows. `examples/capability.lang` is
the S6 demo (L1, L2). The other permit-lang types and refusals (`Call`,
`Plan`, `decide`, the approval forms), the gate and the three targets
do not exist yet.
`docs/PLAN.md` gives the readings, the route, the slices and the open
questions.

## Layout

- `design/DESIGN.md`: the request, the S0 rulings, the overlap and the
  prior art. It is the only source of the core types and the core
  operations.
- `docs/PLAN.md`: the plan.
- `AGENTS.md`: how an agent works in this repo.
- `SPEC.md`: the specification. Each section is filled by a slice.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate
  results.
- `docs/host/README.md`: how to build and test the host kit.

## Build and test

```
make -j 2 check
```

## License

MIT OR Apache-2.0.
