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

| Task | permit-lang? | Use instead |
| --- | --- | --- |
| A read-only scope with a step bound: an inspect in the scope is permitted, an execute outside the scope or a call after the last step is denied (G1) | yes | |
| A budget with costs and a split, where a cost more than the budget gives `none` (G2) | yes | |
| An attenuated delegation to a sub-agent: the child scope, the step bound and the budget do not widen (G3) | yes | |
| The propose and execute of one approved mutation: the approval is the digest of the exact bytes, and it is used one time (G4) | yes | |
| A deploy tool set behind the tool gate `permitd`, which holds the credential and enforces the rules again on each call (G5) | yes | |
| Ordinary authorization rules only (a principal, an action and a resource, with no call count, budget or approval) | no | Cedar (permit-lang exports its static scope as Cedar) |
| A zero-knowledge proof that a decision obeys a policy | no | zkpolicy-lang |
| Agent payments on chain | no | mandate-lang |
| General code: a new data type, general recursion or an exception | no | a general-purpose language, for example Rust or Python |

`skills/permit-lang/SKILL.md` gives the examples G1 to G5 (the files of
`examples/skill/`). `AGENTS.md` gives the loop and the commands for an
agent.

## Status

S16: the token comparison (A6). G1 to G5 are also in Python and in Rust
(`test/baseline/`). `docs/TOKEN-BASELINE.md` gives the bytes, the SHA-256
and an offline token estimate of each of the 15 files. S15: the skill
(`skills/permit-lang/SKILL.md`), the graded examples G1 to
G5 (`examples/skill/`), `AGENTS.md` and this table. The repo holds the
Cubical TT kernel of intent-lang (67445ae), ported from relay-lang 05dd63b
without the crypto code, on the tcc-wasm host. The slices S3 to S7 add R0
(no trap), the affine layer, the domain (`domain/domain.lang`: `Scope`,
`StepBound`, `Capability`, `delegate`, `Mutation`, `Ticket`, `Call`,
`decide`, `Plan` and `Trace`) and the demos `examples/scope.lang`,
`examples/capability.lang` and `examples/decide.lang`. The three targets
exist: the tool gate `permitd` (T1, S8 and S9), the JSON authority report
`langc report` (T2, S10) and the Cedar export `langc cedar` (T3, S11).
The agent items A1, A2 and A4 exist: `langc fmt` (S12), `langc check
--json` (S13) and `langc mcp`, with `langc explain`, `langc base` and
`langc law` (S14). `make check` is GREEN. `docs/PLAN.md` gives the
readings, the route, the slices and the open questions. The next slice is
S17: the close (`SPEC.md`, `probe/CAPABILITY.md`, `docs/VALIDATION.md`
and the final gate).

## Layout

- `design/DESIGN.md`: the request, the S0 rulings, the overlap and the
  prior art. It is the only source of the core types and the core
  operations.
- `docs/PLAN.md`: the plan: the rulings, the readings, the route, the
  slices and the open items.
- `AGENTS.md`: how an agent works in this repo.
- `skills/permit-lang/SKILL.md`: the skill for an agent. Its examples G1
  to G5 are the files of `examples/skill/`.
- `SPEC.md`: the specification. Each section is filled by a slice.
- `src/`: `langc` (C99, TinyCC): the front end `src/front/`, the Wasm
  writer, the report, the Cedar export and the MCP server.
- `host/`: `permitd`, the tool gate, with its Wasm engine, SHA-256 and JCS.
- `domain/domain.lang`: the domain. `examples/`: the demos.
- `test/`: the gate `test/gate.sh`, the C test tools (`test/*tool.c`) and
  their rows. `test/skill/` holds the 3 negative SKILL.md fixtures.
  `test/baseline/` holds G1 to G5 in Python and in Rust (the crate
  `test/baseline/rust/`). `test/tokcount.c` is the token tool.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate
  results.
- `docs/host/README.md`: how to build and test the host kit, the CLI, the
  codes and the gate lines. `docs/CHECK-JSON.md`, `docs/MCP.md`,
  `docs/REPORT-JSON.md` and `docs/CEDAR.md`: the JSON line, the MCP
  server, the report and the Cedar export.
- `docs/TOKEN-BASELINE.md`: the token comparison of G1 to G5 in
  permit-lang, Python and Rust (A6).

## Build and test

```
make -j 2 check
```

## License

MIT OR Apache-2.0.
