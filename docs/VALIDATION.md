# Validation

Date: YYYY-MM-DD. <Tool>: <version>. Host executable: `<path>` at
<commit>.

`<gate command>` passes: <what the gate checks and builds>, and it runs <N>
tests with zero failures. The test runner reported <S> seconds.

> Write here: one paragraph for each slice, in this form.

The <slice> review adds <N> cases in `<test file>`. They check <the forms
and the cases that work>. Refusals cover <the cases that the compiler must
refuse>.

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
