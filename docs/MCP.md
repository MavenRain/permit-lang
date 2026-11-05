# MCP subset

This file pins the Model Context Protocol (MCP) version and the JSON-RPC
subset of the permit-lang tools (PLAN O7). The `permitd` part is S9
(Q-S9-6 (a)). The `langc mcp` part is S14 (A4, `## langc mcp` at the end).

## Version

`permitd` follows MCP 2025-11-25 (the version of relay-lang 4781ca7
`src/mcp.h` line 15) over JSON-RPC 2.0. It does not read or change the
`protocolVersion` of `initialize`: the agent and the upstream server agree
on it through the gate.

## Transport

- The agent talks to `permitd` on stdin and stdout. The upstream MCP
  server is a child process of `permitd` (`posix_spawn`, two pipes). Its
  stderr is the stderr of `permitd`.
- One JSON-RPC message is one line of UTF-8 text. A line has at most
  1 MiB (1048576 bytes). The gate refuses JSON-RPC batches and other values that are not objects.
  Each refusal runs `malformed` and takes one step before any forwarding.
- `permitd` writes its own diagnostics to stderr only.

## Messages

| Message | From | The gate does |
|---|---|---|
| `tools/call` request | agent | the call steps below |
| each other request or notification (`initialize`, `notifications/initialized`, `ping`, `tools/list`, ...) | agent | sends the line to the child unchanged |
| a response to a forwarded `tools/call` | child | sends it to the agent unchanged; an `error` member gives `toolFailed` (below) |
| each other line | child | sends it to the agent unchanged |

## The call steps

For each `tools/call` request, in this order:

1. Read the line with `json_parse` (`host/jsonin.c`). A line that is not
   JSON, or a request with no `params` object, no string `params.name` or
   no object `params.arguments`, is malformed.
2. Remove the key `permit/approval` from `params._meta`. Its value must be
   64 lower-case hex characters (a SHA-256 digest), else the call is
   malformed. When `_meta` has no other key, the gate removes `_meta` too.
   The approval key is never in the payload, so a digest cannot hold
   itself (Q-S9-4 (a)).
3. The payload is the JCS form (RFC 8785, `jcs_write`) of the `params`
   object that stays. A number with a fraction, an exponent or a magnitude
   above 2^53 - 1 is malformed.
4. Find the `bind` row of `params.name` in the custom section
   `permit.bind`. Apply its RFC 6901 pointer to `params.arguments`
   (`jcs_pointer`). The value must be a string: the target path, segments
   split at `/`. No row, no value or a value that is not a string is
   malformed.
5. The digest of the call is the SHA-256 of the envelope
   `{"payload":P,"target":[S...]}` (`jcs_envelope`, PLAN sec 4).
6. The approval: when the call names a digest and the approver has it,
   `has_digest` is 1, else 0 (below).
7. The cost is 1 unit, or N for a tool that a `--cost TOOL=N` flag names
   (Q-S9-9 (a)). N is an unsigned 64-bit decimal integer.
8. Run the export `decide (target, action, payload, cost, has_digest,
   digest)` of the module, with `action` the `Action` arm of the row. A
   malformed call runs the export `malformed ()`. Each takes one step.
9. Write the state file (below), then act on the decision code.

A permit (code 0) sends the child the request with `params` replaced by
the payload bytes: `{"jsonrpc":"2.0","id":ID,"method":"tools/call",
"params":P}`, with ID the JCS form of the request id. The forwarded
`params` bytes are the payload bytes of the envelope.

A deny (code 1 to 8) never reaches the child. The gate answers the agent
with a result (not a JSON-RPC error: the model must see it):

```json
{
  "jsonrpc": "2.0",
  "id": ID,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "permit-lang deny scopeMiss"
      }
    ],
    "isError": true
  }
}
```

The gate writes the object on one line with no white space. The block
shows one key on each line.

The text is `permit-lang deny ` and the name of the `Reason` arm. When the
gate cannot read the id of a malformed line, it answers:

```json
{
  "jsonrpc": "2.0",
  "id": null,
  "error": {
    "code": -32700,
    "message": "permit-lang deny malformed"
  }
}
```

## Upstream failure

An upstream failure is the `toolFailed` outcome (N6): the child stops, a
pipe closes, or the child answers a forwarded `tools/call` with a
JSON-RPC `error`. The gate answers the agent with an `isError` result
with the text `permit-lang toolFailed error` (the child answered with
an `error`) or `permit-lang toolFailed closed` (the child stopped or a
pipe closed). The step, the cost
and the approval of the call stay used. When the child stops, a later
request that is not `tools/call` gets the JSON-RPC error -32603.

## Approvals

- `--approvals FILE` (test mode, O9): one digest a line, 64 lower-case hex
  characters. The gate reads the file again at each `tools/call`, so a
  test can add a line between two calls.
- `--approver tty`: when an `execute` call names no approval, the gate
  writes the envelope bytes and their digest to `/dev/tty` (never to
  stdio) and reads one line. `y` approves that digest for this call.
- The gate writes the approver of each permitted call to stderr: the file
  and the line number, or `tty`.

## State

`--state FILE` keeps the state words across a restart (Q-S9-5 (a)). The
file is text: the steps, the budget left and the used count, one unsigned
64-bit decimal integer a line, then each used digest as 64 hex characters, one a line. The gate
writes a temp file in the same directory, calls `fsync`, and renames it
over FILE. It does this after each `decide` or `malformed` and before it
forwards a permitted call. At start, a state file replaces the start words
of the module, and the gate interns each used digest again. No file means
the start words. A file that does not parse stops the start with exit 2.

## Credential

`--cred-file PATH` or `--cred-env NAME` gives the credential, and
`--cred-var NAME` names the environment variable of the child that holds
it (O11). Only the child gets it. The agent never sees it.

## langc mcp

`langc mcp` (S14, A4) is an MCP server on stdin and stdout. It gives the
verbs `check --json`, `fmt`, `eval`, `build`, `explain`, `base` and `law`
of `langc` to an agent as 7 tools. The server is in `src/mcp.c`. It calls
no function of `src/main.c`: `src/main.c` gives it the verbs as a
callback. The server is a copy of the accrue-lang 02964ec server (the
cairn-lang S13 shape), with the permit-lang names and 2 new tools,
`list-base` and `show-law`. The rulings are Q-S14-1 to Q-S14-8,
Q-S14-U2-1 and Q-S14-U2-2 (`docs/PLAN.md`).

### Usage

```sh
build/langc mcp
build/langc --kit mcp
```

With `--kit`, each tool that reads a program runs in the kit mode, as
`langc --kit VERB`. At EOF the server exits 0, also after error replies.
It exits 2 when stdin cannot be read or stdout cannot be written.

### Framing

- The server reads one JSON-RPC 2.0 message on each line of stdin until
  EOF, and writes each reply on one line of stdout. A `\r` before the
  newline is not part of the message. An empty line gets no reply.
- The longest line is 1048576 bytes (1 MiB, `MCP_LINE_MAX_BYTES` in
  `src/mcp.h`), without the newline, as for `permitd`. A longer line
  gives -32700 with id null. The server drops the rest of that line and
  reads on.
- A line that is not JSON gives -32700 with id null. The reader is
  `host/jsonin.c`, as for `permitd`: a duplicate key, a lone surrogate or
  invalid UTF-8 is not JSON (Q-S14-5 (a)).
- An id is absent, a string or an integer. A message that is not a valid
  request gives -32600, with its id when the id is valid, else null.
- A notification (a valid request without an id) gets no reply, also
  when its method is not known.

### Protocol version

`initialize` always gives the protocol revision `2025-11-25`
(`MCP_PROTOCOL_VERSION` in `src/mcp.h`, the same pin as `permitd`), only
the capability `tools`, and the server info
`{"name":"permit-lang","version":"S14"}`. The request must have a
`protocolVersion` string in its params, else it gives -32602. The server
does not compare that string with its own revision (Q-S14-1 (a)).

### Methods

| Method | Result |
|---|---|
| `initialize` | the revision, the capabilities and the server info (Protocol version) |
| `ping` | `{}` |
| `tools/list` | the 7 tools, each with its `name`, `description` and `inputSchema` |
| `tools/call` | the tool result (Tools) |

Each other method gives -32601. The server does not need `initialize`
before another method.

### Tools

A tool that reads a program takes its path in `path`, not the source
text.

| Tool | Arguments (`inputSchema`) | The CLI verb |
|---|---|---|
| `check` | `path` | `langc check --json PATH` (`docs/CHECK-JSON.md`) |
| `fmt` | `path` | `langc fmt PATH` |
| `eval` | `path`, `name`, `args` (optional, an array of strings) | `langc eval PATH NAME ARGS...` |
| `build` | `path`, `target`, `out` | `langc build PATH` with the target |
| `explain` | `code` | `langc explain CODE` |
| `list-base` | none | `langc base` |
| `show-law` | `law` | `langc law LN` |

Each argument is a string, except `args`. Each argument is required,
except `args`. A string passed to a tool, including each eval argument,
must not contain a NUL byte; such a string gives -32602. The `target` of
`build` is `main` (`-o OUT`) or `runtime` (`--runtime -o OUT`)
(Q-S14-6 (a)). On this target the two give the same Wasm bytes
(`src/target.h`), thus a swap of the two is an equivalent mutant. The
`build` tool writes the file `out`.

- `explain` gives one line `CODE: MEANING (docs/host/README.md, SECTION)`.
  The meaning is the text of the code in `docs/host/README.md`
  (Q-S14-2 (a), Q-S14-U2-2 (a)). The table `src/front/codes.c` has 26
  rows, one for each `REFUSE_` code of `src`.
- `list-base` gives one line `NAME KIND LINE` for each of the 20 rows of
  `docs/PLAN.md` section 4 (`src/front/plantab.c`, Q-S14-3 (a)). It takes
  no arguments.
- `show-law` gives one line `LN NAME: STATEMENT` for the law `LN` (`L1`
  to `L8`) of `docs/PLAN.md` section 7 (Q-S14-4 (a)).

A tool result is

```json
{"content":[{"type":"text","text":"STDOUT"},{"type":"text","text":"STDERR"}],"isError":false}
```

The first item is the stdout of the CLI verb. The second item is its
stderr, and it is there only when the stderr is not empty. Invalid
UTF-8 bytes in either stream become `�`, including an incomplete
character in a truncated diagnostic. `isError` is `true` when the exit
code of the verb is not 0. Thus a refused program, an unknown code of
`explain` or an unknown law of `show-law` is a tool result with
`isError` true, not a JSON-RPC error. The replies keep the member order
of the port; they are not JCS (Q-S14-5 (a)).

### Errors

The `message` of each error is the JSON-RPC text (`Parse error`,
`Invalid Request`, `Method not found`, `Invalid params`, `Internal
error`), not a `permitd` deny text.

| Code | Cause |
|---|---|
| -32700 | The line is not JSON, or it is longer than 1 MiB. The id is null. |
| -32600 | The message is not a valid request: no `jsonrpc` `2.0`, no string `method`, an id that is not a string or an integer, or `params` that are not an object or an array. |
| -32601 | The method is not one of the 4 methods. |
| -32602 | `initialize` has no `protocolVersion` string, or a `tools/call` has no string `name`, `arguments` that are not an object, an unknown tool, a missing argument, an argument of the wrong type or an unknown `target`. |
| -32603 | The server cannot capture the output of the tool (`Internal error`). |

Only a failure of `open_memstream` or `fclose` gives -32603. No test row
gets it, thus the mutant M5 of S14 (a different code for -32603) survives
the gate (`docs/PLAN.md` section 10).

### Limits and state

- The server keeps no state between messages. Each message gets a fresh
  arena (1 GiB limit, `MCP_ARENA_LIMIT_BYTES` in `src/mcp.c`), and each
  `tools/call` runs its verb with fresh options and a fresh diagnostic.
- The server gives tools only: no `resources`, no `prompts`, no
  cancellation and no progress notices.
- Each tool reads its program from the file `path`. No tool takes the
  source text. A relative path starts at the working directory of the
  server.

### The test rows

`test/mcp/` has 11 rows: the 9 rows of the port (`build`, `check`,
`errors`, `eval`, `explain`, `fmt`, `kit-check`, `kit-eval`,
`lifecycle`), `list-base` and `show-law` (Q-S14-7 (a)). Each `X.in` is a
list of messages that starts with `initialize` and
`notifications/initialized`, and `X.out` has the exact bytes of the
replies. `build/mcptool` (`test/mcptool.c`) runs `langc mcp` on each row
(`langc --kit mcp` when `X` starts with `kit-`). The stdout must give the
bytes of `X.out`, the stderr must be empty and the exit status must be
0. The gate also runs `build/mcptool` on 3 bad inputs (a changed `.out`,
a missing `.out` and no rows), and each must give exit 1. The gate rows
are `mcp rows: 11 passed`, `mcp negatives: 3 of 3`, `mcp tools: 7 of 7`,
`mcp builds: 2 of 2`, `mcp long line: 1 of 1`, `explain codes: 26 of 26`
and `base names: 15 of 15` (the mcp block of `test/gate.sh`). A build
row is byte equal to the CLI build. `base names` finds each name of a
base row as a decl of `domain/domain.lang`; a fixed list skips the 12
names that are not domain decls (Q-S14-U2-1 (b)).
