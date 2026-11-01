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

S2: the kernel. The repo holds the Cubical TT kernel of intent-lang
(67445ae), ported from relay-lang 05dd63b without the crypto code, on the
tcc-wasm host, and the plan. The Wasm writer is out of the build until S3.
The permit-lang types, the permit refusals, the gate and the three targets
do not exist yet. `docs/PLAN.md`
gives the readings, the route, the slices and the open questions.

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
