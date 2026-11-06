# Validation

Date: 2026-11-05. tcc: 0.9.28rc 2026-09-04 mob@0fb54300 (AArch64 Darwin).
Host executable: `build/langc` at b7e1228.

`make check` passes: it builds `build/langc`, the C host
`build/permitd`, the fake upstream server `build/fakedeploy` and the
test tools, and it checks the examples, the refusals, the evaluator, the
IR, the Wasm modules, the no-trap checks, the differential cases, the
decide scripts, the engine, SHA-256 and JCS rows, the host rows, the
report and Cedar rows, `fmt`, `check --json`, `mcp`, `explain`, the
skill and the token report. It prints 52 count lines with zero failures
(`gate: 0 failures`), and the review, domain and path regressions pass.
28 compile lines use `-Werror`. The final gate run of `make -B check`
took 71 seconds of wall time (`make check` prints no time).

Each paragraph below gives the gate of one slice, from its row in
`docs/PLAN.md` section 12 (section 10 gives the mutants).
`probe/CAPABILITY.md` gives the probe runs and the cost.

The S0 gate (the seed from tcc-wasm) passes with 9 parse refusals, 19
check refusals, 32 evals, 10 lowered programs, 13 valid Wasm modules and
813 differential cases with 0 mismatches. The review and domain
regressions pass.

S1 has no code change. The gate passes with the S0 counts.

The S2 gate (the Cubical TT kernel, ported from relay-lang 05dd63b)
passes with 29 examples, 16 parse refusals, 151 check refusals, 10
kit-only refusals, 1 time hint, 78 evals and 1 build refusal. The
review, domain and path regressions pass. The mutants of `REFUSE_CELL`
and `REFUSE_TIME` are killed.

The S3 gate (R0, no trap, ported from relay-lang 5c386bb) passes with 30
examples, 16 parse refusals, 155 check refusals, 79 evals, 1 build, 1 ir
lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad
rows and 10 spine evals. The mutants of `REFUSE_PARTIAL` and
`test/notraptool.c` are killed.

The S4 gate (the affine layer, ported from leaf-lang 47a6f36) passes
with 30 examples, 20 parse refusals, 200 check refusals and the S3 IR
and notrap counts. The mutants of `REFUSE_AFFINE_REUSE`, the context
split of a case, the family field loop (Q-S4-12) and `mult_scale` are
killed.

The S5 gate (`Resource`, `Scope`, `StepBound` and the affine
`Allowance`) passes with 31 examples, 21 parse refusals, 213 check
refusals, 11 kit-only refusals, 85 evals and 2 natops evals. Of 17
mutants, 16 are killed and 1 is equivalent.

The S6 gate (`Budget`, `Capability` and `delegate`) passes with 32
examples, 219 check refusals and 88 evals. The new rows check
`REFUSE_BUDGET_OVERSPEND`, `REFUSE_DELEGATE_WIDEN`,
`REFUSE_AFFINE_REUSE` and `REFUSE_AFFINE_OMEGA`. 18 mutants, 18 killed.

The S7 gate (`Payload`, `Mutation`, `Digest` and `Approval`) passes with
33 examples, 224 check refusals, 12 kit-only refusals, 92 evals and 6 ir
refusals. 21 mutants, 21 killed.

The S8 gate (T1 part 1, the Wasm module of `decide`) passes with 35
examples, 22 parse refusals, 236 check refusals, 13 kit-only refusals, 8
ir refusals, 10 ir lowerings, 3 custom sections, 5 notrap modules, 11
notrap bad rows, 16 wasm runs, 694 differential cases with 0 mismatches
and 8 decide scripts (12 calls, 0 mismatches). 26 mutants, 26 killed.

The S9 gate (T1 part 2, `permitd`) adds 8 engine decide scripts (12
calls, 0 mismatches), 2 engine refusals, 2 engine hash rows, 7 sha256
vectors, 32 jcs rows and 28 host rows to the S8 counts. 32 mutants, 32
killed.

The S10 gate (T2, the JSON authority report) has the S9 counts, 5 report
rows, 36 host rows (R1 to R8 are new) and 2 report output checks. 14
mutants, 14 killed.

The S11 gate (T3, the Cedar export) has the S10 counts with 7 report
rows, plus 7 cedar rows and 2 cedar write errors. 13 mutants: 12 killed,
1 equivalent.

The S12 gate (A1, `langc fmt`) has the S11 counts with 23 parse
refusals, plus 300 fmt canonical, 312 fmt round trip, 22 fmt refused, 13
fmt rows and 11 fmt check. 11 mutants: 10 killed, 1 equivalent.

The S13 gate (A2, `langc check --json`) has the S12 counts with 326 fmt
canonical, 338 fmt round trip and 24 fmt refused, plus 28 json rows, 21
of 21 json refusal codes and 26 json fixes. 8 mutants, 8 killed.

The S14 gate (A4, `langc mcp`) has the S13 counts, plus 11 mcp rows, 3
of 3 mcp negatives, 7 of 7 mcp tools, 2 of 2 mcp builds, 1 of 1 mcp long
line, 26 of 26 explain codes and 15 of 15 base names. 21 mutants: 20
killed, 1 survivor.

The S15 skill gate (`test/gate.sh` and `build/skilltool`) checks the 5
graded examples of `skills/permit-lang/SKILL.md`: `skill checks: 5 of 5`,
`skill quotes: 5 of 5`, `skill outputs: 22 of 22` and `skill negatives:
3 of 3`.

The S16 token rows of `make check` (`build/tokcount`) read the 15 counted
files of `docs/TOKEN-BASELINE.md`: G1 to G5 in permit-lang, Python and
Rust. `token report hashes: 15 of 15`: the SHA-256 of each file is equal
to its row. `token estimate: passed`: the estimate block of the report is
equal to the `build/tokcount --estimate` output, ceil(bytes / 4) of each
file. The 10 Python and Rust programs print the `langc eval` lines of
their G, byte for byte (`token outputs: 10 of 10`, s16/u1-probe.sh).
`make tokens` (the `count_tokens` route) is not run, because no API key is
available.
