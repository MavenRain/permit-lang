# Cedar export

This file gives the T3 target (PLAN section 8.2, S11, Q-S11-1 to Q-S11-6
(a)). The export writes the static scope of `def start` as Cedar
`permit` policies.

## Command

```sh
build/langc [--kit] cedar PROG
```

`langc cedar` checks and lowers PROG as `langc report` does, then writes
the Cedar text to stdout. The exit status is 0 for the text, 1 for a
refused program or a failed write, and 2 for a usage error. It refuses
the programs that `langc report` refuses (`REFUSE_REPORT`,
`docs/REPORT-JSON.md`).

## One walker

`report_value` (`src/report.c`) makes the JSON object of the `program`
line. `langc report` writes the JCS form of that object. `src/cedar.c`
walks the same object (Q-S11-1 (a)). Thus the two targets use one scope
walk, and the Cedar text and the report line of a program agree.

## The text

The text has 3 parts, in this order (Q-S11-3 (a)):

1. The 3-line header:

   ```text
   // permit-lang T3: the static scope of def start as Cedar permit policies.
   // Cedar denies each request that no policy permits; principal is open.
   // Each Resource::"a/b" must have the parent Resource::"a" in the request entities.
   ```

2. One policy for each grant of the start scope, in the walk order of
   the scope (left to right). Two equal grants give two equal policies
   (Q-S11-2 (a)):

   ```text
   permit (principal, action == Action::"ARM", resource in Resource::"TEXT");
   ```

   ARM is `inspect`, `propose` or `execute`. TEXT is the segment path
   text of the resource. The principal is open. A policy has no
   annotation. The policy uses `in`, because a grant also covers each
   resource below it.

3. The 6 "Not exported" comments, in this fixed order. These parts have
   no Cedar form. The gate (`permitd`) enforces them.

   | Comment | Text |
   |---|---|
   | steps | `// Not exported: steps. def start allows N calls.` |
   | budget | `// Not exported: budget. def start has N units; the gate takes the cost of each call.` |
   | approval binding | `// Not exported: approval binding. Each execute call needs an approval with the digest of its canonical target and payload, used one time.` |
   | sequence guard | `// Not exported: sequence guard (L5). On each Plan path, an execute comes after an approved propose of the same mutation.` |
   | laws | `// Not exported: laws. ` and the law names with `, ` between them, or `none` |
   | bind lines | `// Not exported: bind lines. ` and each `"TOOL" ARM "POINTER"` with `; ` between them, or `none` |

Two programs get a fixed text:

- No `def start`: the first header line, then
  `// No def start: no policy.` (2 lines).
- A start scope with no grant (`fmUnit`): the 3 header lines,
  `// The start scope has no grant: no policy.`, then the 6 comments.

## Entities

Cedar `in` follows the parents of an entity in the request entities. The
export writes no entity file. Thus a request on `Resource::"a/b"` must
give the parent `Resource::"a"` (and so on for each prefix), else
`resource in Resource::"a"` does not hold for it. The third header line
tells this rule.

## Escapes

A resource text goes in as it is: the checker keeps a resource to a-z,
0-9, `_`, `-` and `/` (`REFUSE_RES`). An action is an arm name. A tool
name and a pointer in the bind comment are in their JSON string form
(RFC 8785), with the quotes. Thus a tab byte in a tool name is `\t` and
the byte 0x01 is `\u0001`, as in the report line.

## Rows

- `test/cedar/NAME.cedar` is the `langc cedar` text of
  `test/report/NAME.lang` (one program, two targets). `test/gate.sh`
  wants exit 0, no stderr and the bytes of the `.cedar` file. Each report
  row has a cedar row. The 7 rows are `big` (60 binds, a report line of
  9959 bytes), `empty` (a scope with no grant, a tab in a tool name),
  `laws`, `nostart`, `scope`, `unit` and `wide`. Gate line:
  `cedar rows: 7 checked`.
- `build/reporttool` checks `langc cedar` with writable and read-only
  stdout: exit 0 and exit 1. Gate line: `cedar write errors: 2 checked`.

## Limits

The `cedar` CLI is not on PATH (PLAN section 13, O3). The rows compare
golden files only. A `cedar validate` row needs a USER install.
