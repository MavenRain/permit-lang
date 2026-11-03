# MCP subset

This file pins the Model Context Protocol (MCP) version and the JSON-RPC
subset of the permit-lang tools (PLAN O7). The `permitd` part is S9
(Q-S9-6 (a)). The `langc mcp` part comes at S14.

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
{"jsonrpc":"2.0","id":ID,"result":{"content":[{"type":"text","text":"permit-lang deny scopeMiss"}],"isError":true}}
```

The text is `permit-lang deny ` and the name of the `Reason` arm. When the
gate cannot read the id of a malformed line, it answers
`{"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"permit-lang
deny malformed"}}`.

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
