# permit-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
permit-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---

## 1. The request (USER, 2026-11-01, verbatim)

One change only: the paste had one trailing space after "description:" and
one after "criteria:". Those spaces are not here, because `git diff --check`
refuses trailing white space.

I'd like to write a new programming language with the following characteristics:

First, do not allow any runtime errors, period.

For type constructors/higher-kinded types, admit only cell monads and their duals as represented computationally in a cubical type theory.

For base types and operations, admit the minimum required to satisfy the following description:

<pasted 1>
permit-lang: executable capability protocols for agents
The problem: An agent may need a narrow operation while the underlying SaaS credential grants much broader access. This HN thread about agent over-permissioning describes that mismatch directly.
The language: Describe authority over a sequence of actions: scoped resources, expiration, delegation, consumable budgets, and approval tied to an exact proposed mutation.
First compelling demo: An agent can inspect a deployment and propose a change, but can execute only the exact approved change. Modifying the target or payload invalidates that approval.
Overlap matters here: You already have zkpolicy-lang, which checks policy decisions with zero-knowledge proofs. This proposal would focus on enforcing authority at the actual tool boundary. I would first investigate whether it belongs as an extension there. Existing languages such as Cedar also mean ordinary authorization rules alone would be insufficient differentiation.
</pasted 1>


Finally, I want agents to reach for this language when it fits their use case.  To that end, please admit the following additional criteria:

<pasted 2>
  The plan for making agents reach for the language includes:
  - one canonical form per operation and a langc fmt canonical printer;
  - langc check --json, giving fix hints and progress counts (parsed, typed, laws
    discharged);
  - a SKILL.md with 3 to 5 graded examples;
  - an MCP server written in TinyCC (langc mcp);
  - AGENTS.md and a README table saying when to use the language;
  - a token-count comparison against Python and Rust.
</pasted 2>

Note: the HN thread link in pasted 1 did not survive the paste (no URL in the text).

## 2. The S0 rulings (USER, 2026-11-01, AskUserQuestion, labels verbatim)

- Home: "New repo, zkpolicy later (Recommended)": seed MavenRain/permit-lang from lang-template (tcc-wasm kit); a later slice can add zkpolicy-lang as an optional target (prove in ZK that a permit check passed).
- Targets (multi-select): "MCP gate: Wasm + C host (Recommended)" (langc compiles a permit program to a Wasm module; a TinyCC-built C host sits between the agent and the real tool/MCP server, holds the broad SaaS credential, and forwards only calls the program permits; this is the demo), "JSON authority report (Recommended)" (per run: each call with permit/deny, reason code, remaining budget, approval digest checked; plus the static summary), "Cedar export" (the static scope part as Cedar policies; budgets, sequences and approval binding have no Cedar form and stay in the gate). NOT chosen: "C99 enforcer library".
- Approval: "Digest, abstract hash (Recommended)": the approval carries the digest of the canonical (target, payload) bytes; one opaque hash in the language with its laws; v1 binds SHA-256; the C host records approvals from an approver channel (CLI prompt) and refuses a call whose digest differs.
- Time: "Operation counts only": no time at all (cairn-lang precedent); expiry = a call-count or step bound. NOT chosen: host ticks, both.
- Host TinyCC: implied by A4 ("an MCP server written in TinyCC (langc mcp)"), not asked.

## 3. Overlap and prior art (2026-11-01)

### 3.1 Overlap (pasted 1 asks for it first)

zkpolicy-lang (HEAD 1d263fb):
- Job: a zero-knowledge proof shows that a DECISION obeys its policy (DAOs, edge). Targets zkml-lang Circuit IR, Wasm edge, EVM verifier (P10 not started).
- Formers: full cubical TT with paths, HITs and QIITs (README). This is wider than R1.
- Traps exist: the P7b-nat work keeps a trapping natAdd and lowers the trap. This conflicts with R0.
- It has no tool boundary: no proxy, no credential custody, no MCP. Its delegation/budget/approval hits are 1 to 3 lines per doc (host CAPABILITY.md = the kit host capability doc, not agent authority).
- Reading: an extension would need an R0 + R1 profile inside zkpolicy-lang (a sublanguage with its own checker gate), so the extension costs more than a seed and weakens both. Proposed: new repo; zkpolicy-lang is an OPTIONAL later target (prove in ZK that a permit decision passed). USER rules.

mandate-lang (HEAD d368d5f): agent PAYMENT mandates (AP2/x402), EVM EIP-7702 delegate. It has budgets and delegation (59 hits in docs/PLAN.md) but only on chain and only for payments. permit-lang is off chain at a SaaS tool call. Port candidate: mandate-lang PLAN budget and delegation readings (cite file:line at S0).

### 3.2 Prior art (web search, 2026-11-01)

- Cedar: no float type, `like` with `*` only (no regex), no external data during evaluation; AgentCore Cedar has no date/time comparison. Approvals in Cedar policies appear as context presence checks (`context has approval_id`), which check presence, not validity, and do not bind the approval to the request. Sources: https://docs.aws.amazon.com/bedrock-agentcore/latest/devguide/policy-limitations-section.md , https://phantomcorgi.pages.dev/blog/2026-04-16-cedar-not-enough , https://repo.jesusperez.pro/jesus/provisioning/raw/branch/main/config/cedar-policies/README.md
- Attenuating delegation for agents: IETF draft-niyikiza-oauth-attenuating-agent-tokens (March 2026, tool-scoped tokens a holder narrows offline); draft-prakash-aip (Biscuit chained mode, Datalog); RFC 8693 `act` claim is informational and does not enforce narrowing at depth 2 or more; macaroons = holder-side attenuation, one trust domain. Sources: https://datatracker.ietf.org/doc/html/draft-niyikiza-oauth-attenuating-agent-tokens-00 , https://www.ietf.org/archive/id/draft-prakash-aip-00.html , https://arxiv.org/pdf/2603.24775
- OAuth scopes are coarse; RFC 9396 Rich Authorization Requests carry task detail; vendor figure (SailPoint, unverified): 80% of organisations saw agents act beyond scope. Source: https://securityboulevard.com/2026/05/oauth-scopes-for-ai-agents-how-to-design-permissions-in-agentic-applications/
- Differentiation reading: none of these checks, BEFORE run, that a whole SEQUENCE of actions stays inside the authority, that a budget is consumed at most once (affine), or that an approval is equal to the digest of the exact mutation the tool sends. permit-lang does these in the checker and enforces them again in the gate that holds the credential.
