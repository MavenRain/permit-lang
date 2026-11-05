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
