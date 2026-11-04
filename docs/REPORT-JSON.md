# JSON authority report

This file gives the schema of the T2 target (PLAN section 8.2, S10,
Q-S10-1 to Q-S10-4 (a)). The report is JSON Lines. Two programs write
the lines:

- `langc report PROG` (`src/report.c`) writes one `program` line to
  stdout: the static authority of PROG.
- `permitd --report FILE` (`host/report.c`) appends one `call` line or
  one `malformed` line to FILE for each decided call.

The full report of a run is the `program` line, then the lines of the
gate:

```sh
build/langc report PROG > R
build/permitd --report R MODULE -- CMD ARGS
```

## Rules for each line

- A line is one JSON object in its RFC 8785 (JCS) form, then `\n`
  (Q-S10-2 (a)). The writer makes the JSON text, then `json_parse`
  (`host/jsonin.c`) and `jcs_write` (`host/jcs.c`) make the bytes. Thus
  the keys are in JCS order and the line has no white space. The two
  writers use the same canonicalizer, so the rows compare bytes.
- Each line has `"permit":1` (the schema version) and `"kind"`:
  `"program"`, `"call"` or `"malformed"`.
- Each 64-bit word (`budget`, `cost`, `steps`) is a JSON string of
  decimal digits, for example `"18446744073709551615"`. JCS refuses a
  number above 2^53 - 1 (Q-S10-3 (a)).
- A line has no time field (PLAN section 5).

## The program line

`langc [--kit] report PROG` checks and lowers PROG, then writes one line
with these keys:

| Key | Value |
|---|---|
| `binds` | the `bind` lines in program order, `[{"action","pointer","tool"}]`; `action` is the name of the `Action` arm; `[]` when PROG has no `bind` line |
| `budget` | the budget left of `def start` (a word), or `null` when PROG has no `def start` |
| `kind` | `"program"` |
| `laws` | the names of the program definitions whose type is a Path, closed or after a chain of Pi types, in definition order; each passed `check`, so each is discharged |
| `permit` | `1` |
| `scope` | the grants of the closed `start` scope, left to right, `[{"action","resource"}]`; `resource` is the segment path text; `null` when PROG has no `def start` |
| `steps` | the step bound of `def start` (a word), or `null` when PROG has no `def start` |

`REFUSE_REPORT` refuses a `def start` that is not a closed `authority`,
or that has steps or a scope that are not closed. The exit status is 0
for the line, 1 for a refused program or a failed write, and 2 for a
usage error.

The line of `test/report/laws.lang`:

```json
{"binds":[{"action":"inspect","pointer":"/path","tool":"read"}],"budget":"10","kind":"program","laws":["lawMalformed","lawRead","lawAnyPayload","lawTwice"],"permit":1,"scope":[{"action":"inspect","resource":"docs"}],"steps":"2"}
```

## The lines of permitd

`permitd --report FILE` opens FILE to append after it loads the state
file. When FILE does not open, `permitd` writes `permitd: cannot open
the report file FILE` to stderr and exits with 2. Without `--report`,
`permitd` writes no line.

The report and state must use different files. The gate refuses the same
file, also through another path, with exit 2. A state rename would replace
the report. The upstream child does not inherit the report descriptor.

`permitd` appends one line for each call that `decide` or `malformed`
decides. The line comes after the state write and before the answer to
the agent or the forward to the upstream server (step 9 of "The call
steps" in `docs/MCP.md`). After each line, `permitd` does `fflush` and
`fsync`. Thus each forwarded call has its line in FILE before the
upstream server gets the call. A restart with the same FILE appends to
the lines in FILE.

A call with no decision has no line: a failure of the engine, a failure
of the state write, a full digest table or too many pending calls. The
line does not hold the upstream result, because the line comes before
the forward (PLAN section 13, O24).

### The call line

A `tools/call` that `decide` decides (code 0 to 8) gives a `call` line
with these keys:

| Key | Value |
|---|---|
| `action` | the `Action` arm of the bind row of the tool (`"inspect"`, `"propose"` or `"execute"`), or `null` |
| `approval` | the named digest of the call (64 lower-case hex digits), or `null` |
| `approver` | the approver: `FILE:LINE` (the approvals file and the line of the named digest, 0 when no line has it), `"tty"`, or `null` |
| `budget` | the budget left after `decide` (state word 1), a word |
| `cost` | the cost of the call (1, or N of `--cost TOOL=N`), a word |
| `decision` | `"permit"` or `"deny"` |
| `digest` | the SHA-256 (hex) of the envelope that the gate checked |
| `id` | the JSON-RPC id as the agent sent it, in its JCS form |
| `kind` | `"call"` |
| `permit` | `1` |
| `reason` | the name of the `Reason` arm of a deny (`"scopeMiss"`, `"boundSpent"`, `"budgetShort"`, `"noApproval"`, `"digestDiffers"`, `"approvalUsed"`, `"widen"` or `"malformed"`), or `null` for a permit |
| `steps` | the steps left after `decide` (state word 0), a word |
| `target` | the target path text that the bind pointer selects |
| `tool` | the tool name |

### The malformed line

A call that `malformed ()` decides (docs/MCP.md: a line that is not
JSON, a batch, a scalar, a tool with no bind row, a target that is not a
string or a bad approval hex) gives a `malformed` line with these keys:
`budget`, `decision` (`"deny"`), `id` (the JSON-RPC id, or `null` for a
parse error), `kind` (`"malformed"`), `permit` (`1`), `reason`
(`"malformed"`) and `steps`.

### Steps and budget

Each line takes one step: `steps` falls by 1 on each line, a deny too,
until it is 0. Only a permit spends: `budget` falls by `cost` on a
`permit` line only.

### Write errors

When a line does not write (the write, `fflush` or `fsync` fails),
`permitd` writes `permitd: cannot write the report file FILE` to stderr
and answers the call with the JSON-RPC error -32603 `permit-lang report
error`. It does not forward the call. The state write comes first, so
the state counts the call.

## Rows

- `test/report/*.lang` and `test/report/*.json` (S10 U1): `test/gate.sh`
  runs `langc report` on each `.lang` file, wants exit 0 and no stderr,
  and compares the bytes with the `.json` file. The 7 rows are `big`
  (60 binds, a line of 9959 bytes, S11 U3), `empty` (a scope with no
  grant and a tab in a tool name, S11 U2), `laws`
  (4 laws), `nostart` (no `def start`), `scope` (a left-nested and a
  right-nested `fmMul`, the 3 `Action` arms), `unit` (`fmUnit`, steps
  and budget 0) and `wide` (budget 18446744073709551615). Gate line:
  `report rows: 7 checked`.
- `build/hosttool` (`test/hosttool.c`, S10 U2): the script line `# L`
  wants L as the next line of the report file. R1 has an `execute`
  permit with an approvals file, then the `approvalUsed`, `scopeMiss`,
  `digestDiffers` and `noApproval` lines. R2 has a malformed line with
  id `null` (not JSON) and with id `"a"` (no target). R3 has
  `budgetShort` with `--cost inspect=11`, 5 `propose` permits (budget 9
  to 5, steps 4 to 0), then `boundSpent`. R4 has a permit, a restart
  with the same `--state` and `--report`, and a permit: FILE holds the
  two lines. R5 checks that the child has no report descriptor. R6 and R7
  refuse a report that uses the state file, also through a path alias.
  R8 checks an open failure.
- `build/reporttool` checks `langc report` with writable and read-only
  stdout. The same valid program must exit 0 for the first and 1 for the
  buffered write error. Gate line: `report write errors: 2 checked`.
