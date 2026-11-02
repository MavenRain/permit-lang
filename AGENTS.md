# AGENTS.md

How an agent works in permit-lang. This file is true at S5 (the domain
types). The final form comes at slice S15 (`docs/PLAN.md` section 9,
A5).

## State at S5

- The code is the Cubical TT kernel of intent-lang (67445ae), ported from
  relay-lang 05dd63b without the crypto code: `langc` (C99, TinyCC) and
  its domain in `domain/`. S3 adds R0 from relay-lang 5c386bb: the Wasm
  writer `src/wasm.c` is in the build and emits no trap form, and
  `test/notraptool.c` checks each built module.
- S4 adds the affine layer from leaf-lang 47a6f36 (`docs/PLAN.md`
  section 6): the marks `@0` and `@1` on binders, `A -o B`, `unpack`,
  the resource sorts `Budget` and `Approval`, and the codes
  `REFUSE_AFFINE_REUSE`, `REFUSE_AFFINE_OMEGA`, `REFUSE_AFFINE_FIX` and
  `REFUSE_AFFINE_PATH`. A `Budget` or an `Approval` is used at most one
  time.
- S5 adds the domain types (`docs/PLAN.md` sections 4 and 5): `Action`,
  `Reason`, `Decision`, `StepBound := Size`, the `Call` stand-in, `Scope`
  with `grants`, `allows` and `scopeWithin`, and `affine cofamily
  Allowance` with the generated `AllowanceOpen` behind `alOpen`. The
  kernel has the copyable sort `Resource` with `resEq`, `within` and the
  kit-only `resRoot` and `resChild`. `REFUSE_TIME` names 10 words. An
  `affine` cofamily is a resource type: `resource_shape` marks a Pi on
  its `D.pos` family, and a `-o` Pi whose codomain is one. `unpack` opens
  a `Prod` pair one time. A guest position closure cannot capture an
  affine value: `alOpen` observes distinct positions. Generated destructors
  preserve positions and can move their input. `examples/scope.lang` holds
  the L4 and L6 rows and the copyable-capture and linear-continuation checks.
- The other permit-lang types, refusals and targets (`spend`, `split`,
  `Capability`, `delegate`, the approval forms, the gate) are a plan
  only: `docs/PLAN.md` sections 4 to 8.
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
