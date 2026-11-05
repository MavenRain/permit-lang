# AGENTS.md

## Routing rule

Use permit-lang when the task is in a "yes" row of the table "When to use
permit-lang" below (the README table "When to reach for permit-lang" has
the same rows): an agent capability protocol at the tool boundary. An
agent gets a narrow, bounded slice of a broad credential. The yes rows are
a read-only scope with a step bound (G1), a budget with costs and a split
(G2), an attenuated delegation to a sub-agent (G3), the propose and
execute of one approved mutation (G4) and a deploy tool set behind the
tool gate (G5). The approval of a mutation is the digest of its exact
bytes, and a delegated capability cannot widen. No program reads a clock:
the expiry is a count of calls (a `StepBound`), and a time word gives
`REFUSE_TIME`. Do not use it for ordinary authorization rules only, a
zero-knowledge proof that a decision obeys a policy, agent payments on
chain or general code. The skill `skills/permit-lang/SKILL.md` gives the
routing rule, the loop and 5 graded examples (G1 to G5, the files of
`examples/skill/`).

## The loop

1. Write FILE.lang: one `def NAME : TYPE := TERM` for each value. A kit
   item (for example `resRoot`, `payloadOf`, `budget` or a Nat literal
   outside `def start`) needs the kit mode: a kit list names the file
   (`examples/skill/kit.txt`), and `build/langc --kit` checks it. G1 to G4
   are kit files. G5 is a guest file.
2. Run `build/langc --kit check --json FILE.lang` for a kit file
   (`build/langc check --json FILE.lang` for a guest file). It writes one
   JSON line on stdout, with the `diagnostics` array and the `progress`
   object (`docs/CHECK-JSON.md`). Exit 0: the checker accepts the file.
   Exit 1: the checker refuses the file. Exit 2: there is no JSON line,
   and stderr has the text diagnostic `OOM`, `IO` or `USAGE`.
3. For each diagnostic, `build/langc explain CODE` gives one line: the
   code, its meaning and its section of `docs/host/README.md`. There are
   26 refusal codes (`src/front/codes.c`). Change the file, then do
   step 2 again.
4. Run `build/langc fmt FILE.lang --check`. No output and exit 0: the
   text is canonical. Else it prints `FILE.lang:LINE: not canonical`
   (LINE is the first line that differs) and exits 1. Then write the
   output of `build/langc fmt FILE.lang` to a new file, and replace
   FILE.lang with it.
5. Run `build/langc --kit eval FILE.lang NAME` (`build/langc eval
   FILE.lang NAME` for a guest file) for the value of the def NAME. A
   Decision prints as `permit` or as `deny REASON` (`deny scopeMiss`).

`build/langc mcp` gives the tools `check`, `fmt`, `eval`, `build`,
`explain`, `list-base` and `show-law` as an MCP server on stdin and stdout
(`docs/MCP.md`). With `--kit`, each tool that reads a program runs in the
kit mode.

## Status (S15)

The slices S0 to S14 give the kernel with the affine layer (S2 to S4),
the domain (S5 to S7), the three targets (the tool gate `permitd`, T1, S8
and S9; the JSON authority report, T2, S10; the Cedar export, T3, S11),
`langc fmt` (S12), `langc check --json` (S13), and `langc mcp`, `langc
explain`, `langc base` and `langc law` (S14). S15 adds
`skills/permit-lang/SKILL.md`, the graded examples of `examples/skill/`,
`test/skilltool.c`, the fixtures of `test/skill/`, this file and the
README table. `docs/PLAN.md` section 12 gives each slice and the next
slice (S16: the token comparison, A6).

## Commands that work now

- `make`: build `build/langc`, the tool gate `build/permitd`, the fake
  deploy tool `build/fakedeploy` and the 10 test tools (`build/*tool`).
- `make check`: build, then run the gate `test/gate.sh`, `test/review.sh`,
  `build/fronttool` and `build/pathtool`. The gate prints count lines (for
  example `skill checks: 5 of 5`) and `gate: 0 failures` at its end. Each
  failure prints a line that starts with `FAIL`. For a log that you
  compare byte for byte, run `make check` without `-j`: a parallel make
  changes the order of the build lines.
- `build/langc [--kit] check [--json] PROG`: check PROG. Text: `ok` on
  stdout, or one line on stderr. `--json`: one JSON line
  (`docs/CHECK-JSON.md`).
- `build/langc [--kit] eval PROG NAME [ARGS...]`: the value of the def
  NAME.
- `build/langc [--kit] fmt PROG [--check]`: print the canonical text of
  PROG. With `--check`: no output and exit 0 when PROG is canonical.
- `build/langc explain CODE`: one line on the diagnostic code CODE.
- `build/langc base`: one line for each of the 20 rows of the base span
  (`docs/PLAN.md` section 4).
- `build/langc law LN`: one line `LN NAME: STATEMENT` for the law LN, L1
  to L8 (`docs/PLAN.md` section 7).
- `build/langc [--kit] mcp`: the MCP server (`docs/MCP.md`).
- `build/langc [--kit] report PROG`: the JSON authority report
  (`docs/REPORT-JSON.md`). `build/langc [--kit] cedar PROG`: the static
  scope as Cedar policies (`docs/CEDAR.md`).
- `build/langc [--kit] ir PROG`, `abi PROG [--text]` and `build PROG -o
  OUT [--runtime]`: the back end and the Wasm target. `build/permitd`
  runs a built module as the tool gate (`docs/host/README.md`, section
  `permitd`).

`build/langc` with no argument writes a USAGE line and the usage text on
stderr, and exits 2.
`build/langc --kit check examples/skill/g1-scope.lang` exits 0 and prints
this line:

```text
ok
```

## Rules for a change

- `design/DESIGN.md` is verbatim. It is the only source of the core types
  and the core operations. Do not edit it. A design change is a new paste
  and an open item.
- No run-time error: a failure that the domain knows is a value (R0,
  `docs/PLAN.md` section 2).
- Type constructors: only cell monads and their duals (R1, section 3).
- Base names: only the names of section 4. Each one cites the line of the
  request that it serves.
- No clock words. Expiry is a call count (section 5).
- Keep each `.lang` file canonical: `build/langc fmt PROG --check` gives
  no output and exit 0. The fmt rows of `test/gate.sh` give the
  exceptions (the `test/fmt/` files).
- Put a test in a C tool in `test/` or in a `.lang` file, with a row in
  `test/gate.sh`. Do not add a new node, Python or shell test harness.
- Each example block of SKILL.md is byte equal to its file in
  `examples/skill/`, and each output block is the output of its command.
  The gate checks both (`skill quotes`, `skill outputs`). Change the file
  and the block together.
- No em-dash or en-dash outside `design/DESIGN.md`. The gate checks it.
- Repo text uses short sentences in the active voice.
- The gate does not compare the verbs in this file with the usage text.
  After a change to the usage text, compare them by hand.

## When to use permit-lang

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

## Files

- `design/DESIGN.md`: the request, the S0 rulings, the overlap and the
  prior art. It is the only source of the core types and the core
  operations.
- `docs/PLAN.md`: the plan: the rulings, the readings, the route, the
  slices and the open items.
- `README.md`: the overview and the table "When to reach for
  permit-lang".
- `skills/permit-lang/SKILL.md`: the skill for an agent. Its examples G1
  to G5 are the files of `examples/skill/`.
- `SPEC.md`: the specification. Each section is filled by a slice.
- `src/`: `langc` (C99, TinyCC): the front end `src/front/`, the Wasm
  writer, the report, the Cedar export and the MCP server.
- `host/`: `permitd`, the tool gate, with its Wasm engine, SHA-256 and JCS.
- `domain/domain.lang`: the domain. `examples/`: the demos.
- `test/`: the gate `test/gate.sh`, the C test tools (`test/*tool.c`) and
  their rows. `test/skill/` holds the 3 negative SKILL.md fixtures.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate
  results.
- `docs/host/README.md`: how to build and test the host kit, the CLI, the
  codes and the gate lines. `docs/CHECK-JSON.md`, `docs/MCP.md`,
  `docs/REPORT-JSON.md` and `docs/CEDAR.md`: the JSON line, the MCP
  server, the report and the Cedar export.
