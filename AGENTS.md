# AGENTS.md

How an agent works in permit-lang. This file is true at S0 (the seed). The
final form comes at slice S15 (`docs/PLAN.md` section 9, A5).

## State at S0

- The code is the lang-template tcc-wasm kit: `langc` (C99, TinyCC), its
  sample domain in `domain/`, and the Wasm writer in `src/wasm.c`.
- The permit-lang types, refusals and targets are a plan only:
  `docs/PLAN.md` sections 4 to 8.
- `design/DESIGN.md` is verbatim. Do not edit it. A design change is a
  new paste and an open item.

## Gate

```
make -j 2 check
```

For a log that you compare byte for byte, run `make check` without `-j`:
a parallel make changes the order of the build lines.

## Rules

- No run-time error: a failure that the domain knows is a value (R0,
  `docs/PLAN.md` section 2).
- Type constructors: only cell monads and their duals (R1, section 3).
- Base names: only the names of section 4. Each one cites the line of the
  request that it serves.
- No clock words. Expiry is a call count (section 5).
- Tests are guest `.lang` files and C tools. Do not add a node, Python or
  shell harness.
- No em-dashes or en-dashes outside `design/DESIGN.md`. The gate fails on
  them.
- Repo text uses short sentences in the active voice.

## Where to start

Read `docs/PLAN.md` section 12 (slices) and section 14 (open questions).
