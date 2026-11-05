---
name: permit-lang
description: Write, check and repair permit-lang programs (a Cubical TT language for agent capability protocols at the tool boundary) with the langc checker. Use it when the task is an agent capability protocol: a scope of resources and actions, a step bound as the expiry, an attenuated delegation to a sub-agent, an affine budget, or an approval that is the digest of the exact mutation.
---

# permit-lang

## Routing rule

Use permit-lang when the task is an agent capability protocol at the tool
boundary: an agent gets a narrow, bounded slice of a broad credential. The
examples are a read-only scope with a step bound (G1), a budget with costs
and a split (G2), an attenuated delegation to a sub-agent (G3), the propose
and execute of one approved mutation (G4) and a deploy tool set behind the
tool gate (G5). The approval of a mutation is the digest of its exact
bytes, and a delegated capability cannot widen. No program reads a clock:
the expiry is a count of calls (a `StepBound`), and a time word gives
`REFUSE_TIME`. Use something else for ordinary authorization rules only
(Cedar: permit-lang exports its static scope as Cedar), for a
zero-knowledge proof that a decision obeys a policy (zkpolicy-lang), for
agent payments on chain (mandate-lang) and for general-purpose code. The
README table (README.md, "When to reach for permit-lang") gives the yes
and no rows.

## The loop

1. Write FILE.lang: one `def NAME : TYPE := TERM` for each value. A kit
   item (for example `resRoot`, `payloadOf`, `budget` or a Nat literal
   outside `def start`) needs the kit mode: a kit list names the file, and
   `langc --kit` checks it. G1 to G4 are kit files
   (examples/skill/kit.txt). G5 is a guest file.
2. Run `build/langc --kit check --json FILE.lang` for a kit file
   (`build/langc check --json FILE.lang` for a guest file). It writes one
   JSON line on stdout, with the `diagnostics` array and the `progress`
   object (docs/CHECK-JSON.md). Exit 0: the checker accepts the file. Exit
   1: the checker refuses the file. Exit 2: there is no JSON line, and
   stderr has the text diagnostic `OOM`, `IO` or `USAGE`.
3. For each diagnostic, `build/langc explain CODE` gives one line: the
   code, its meaning and its section of docs/host/README.md. Change the
   file, then do step 2 again.
4. Run `build/langc fmt FILE.lang --check`. No output and exit 0: the text
   is canonical. Else it prints `FILE.lang:LINE: not canonical` (LINE is
   the first line that differs) and exits 1. Then write the output of
   `build/langc fmt FILE.lang` to a new file, and replace FILE.lang with
   it.
5. Run `build/langc --kit eval FILE.lang NAME` (`build/langc eval
   FILE.lang NAME` for a guest file) for the value of the def NAME. A
   Decision prints as `permit` or as `deny REASON` (`deny scopeMiss`). A
   Flag prints as `1` or `0`, and a Nat as `4`. An Option prints as `some
   (...)`. A Resource prints as `resRoot 0` or `resChild (resRoot 0) 1`,
   and a StepBound as `szS (szS sz0)`.

`build/langc mcp` gives the same verbs to an agent as a Model Context
Protocol server on stdin and stdout (docs/MCP.md). Its 7 tools are
`check`, `fmt`, `eval`, `build`, `explain`, `list-base` and `show-law`.
With `--kit`, each tool that reads a program runs in the kit mode.

## Verbs

`build/langc` with no argument writes a USAGE line and the usage text on
stderr, and exits with code 2:

```text
langc: USAGE: -: expected a command and a program path
usage: langc [--kit] check [--json] PROG
       langc [--kit] eval PROG NAME [ARGS...]
       langc [--kit] ir PROG
       langc [--kit] abi PROG [--text]
       langc [--kit] build PROG -o OUT [--runtime]
       langc [--kit] report PROG
       langc [--kit] cedar PROG
       langc [--kit] fmt PROG [--check]
       langc explain CODE
       langc base
       langc law LN
       langc [--kit] mcp
--kit: the kit mode, the trusted test scope of the kit gate
```

- `check [--json] PROG`: check PROG. Text: `ok` on stdout, or one line on
  stderr.
- `eval PROG NAME [ARGS...]`: the value of the def NAME.
- `fmt PROG [--check]`: print the canonical text of PROG. With `--check`,
  no output and exit 0 when PROG is canonical.
- `explain CODE`: one line on the diagnostic code CODE.
- `base`: one line for each of the 20 rows of the base span (PLAN section
  4): the name, its meaning and its PLAN rows.
- `law LN`: one line `LN NAME: STATEMENT` for the law LN, L1 to L8 (PLAN
  section 7).
- `mcp`: the MCP server (docs/MCP.md).
- `report`, `cedar`, `ir`, `abi` and `build`: the back end and the targets
  (docs/host/README.md, docs/REPORT-JSON.md, docs/CEDAR.md). This skill
  does not use them.

## Examples

The examples go from easy to hard. Run each command from the repository
root. The block below each command is its stdout and its stderr. The gate
(test/skilltool.c) runs each command and compares the output and the exit
code with the text.

### G1

A read-only scope with a step bound. The authority `a0` can inspect
`docs` and each resource below it, one time. `readOk` inspects
`docs/report`, so it is permitted. `editMiss` is an execute, so it is
denied with `scopeMiss`. `readAgain` comes after the one step, so it is
denied with `boundSpent`. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g1-scope.lang

```lang
-- G1 (S15, Q-S15-2 (a)): a read-only scope with a step bound. The agent can
-- inspect docs and each resource below it, one time. Kit mode: payloadOf
-- and the Nat literals are kit items.
def docs : Resource := res "docs"

def report : Resource := res "docs/report"

def readDocs : Scope := fmEta (Prod Resource Action) (pair docs inspect)

def a0 : Authority := authority readDocs (szS sz0) 5 (fmUnit Digest)

def readReport : Ticket := ticket (mutation report inspect (payloadOf 0)) 1 none

def editReport : Ticket := ticket (mutation report execute (payloadOf 0)) 1 none

-- L6: an inspect in the scope is permitted and an execute is denied. L4:
-- the permit takes the one step, so the next call is denied.
def lawRead : Path Decision (first (judge a0 readReport)) permit := refl

def lawEdit : Path Decision (first (judge a0 editReport)) (deny scopeMiss) := refl

def lawSpent : Path Decision (first (judge (second (judge a0 readReport)) readReport)) (deny boundSpent) :=
  refl

def readOk : Decision := first (judge a0 readReport)

def editMiss : Decision := first (judge a0 editReport)

def readAgain : Decision := first (judge (second (judge a0 readReport)) readReport)
```

`build/langc --kit check examples/skill/g1-scope.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-scope.lang readOk` gives:

```text
permit
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-scope.lang editMiss` gives:

```text
deny scopeMiss
```

The exit code is 0.

`build/langc --kit eval examples/skill/g1-scope.lang readAgain` gives:

```text
deny boundSpent
```

The exit code is 0.

### G2

A budget with costs and a split. `split b n` gives the n units first
and the rest second, and `spend b n` gives the rest. `halves` splits 5
into 3 and 2, `rest` spends 3 of 5, and `left` reads a budget of 4. The
law `lawShort` shows that a cost more than the budget gives `none`. A
closed short `spend` as a value gives `REFUSE_BUDGET_OVERSPEND`. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g2-budget.lang

```lang
-- G2 (S15, Q-S15-2 (a)): a budget with costs and a split. split b n gives
-- the n units first and the rest second, and spend b n gives the rest. A
-- cost that is more than the budget gives none. Kit mode: budget,
-- budgetLeft and the Nat literals are kit items.
def lawSplit : Path (Option (Prod Budget Budget)) (split (budget 5) 3) (some (pair (budget 3) (budget 2))) :=
  refl

def lawSpend : Path (Option Budget) (spend (budget 5) 3) (some (budget 2)) := refl

def lawShort : Path (Option Budget) (spend (budget 2) 3) none := refl

def lawLeft : Path Nat (budgetLeft (budget 4)) 4 := refl

def halves : Option (Prod Budget Budget) := split (budget 5) 3

def rest : Option Budget := spend (budget 5) 3

def left : Nat := budgetLeft (budget 4)
```

`build/langc --kit check examples/skill/g2-budget.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-budget.lang halves` gives:

```text
some (pair (budget 3) (budget 2))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-budget.lang rest` gives:

```text
some (budget 2)
```

The exit code is 0.

`build/langc --kit eval examples/skill/g2-budget.lang left` gives:

```text
4
```

The exit code is 0.

### G3

An attenuated delegation to a sub-agent. `delegate` gives `some (pair
child rest)` when the child scope is within the parent scope, the child
step bound is not longer and the budget covers the cost. `child` gives
the inspect of `docs/report` with one step and a budget of 3 from a parent
with two steps and a budget of 5, and the rest keeps 2. `oneTwo` and
`twoOne` compare the step bounds. The checker refuses a closed `delegate`
that widens the scope: the last two rows check the kit file
test/check/delegate-widen.lang and explain its code. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g3-delegate.lang

```lang
-- G3 (S15, Q-S15-2 (a)): an attenuated delegation to a sub-agent. delegate
-- gives (child, rest) when the child scope is within the parent scope, the
-- child step bound is not longer and the budget covers the cost. Kit mode:
-- budget and the Nat literals are kit items.
def docs : Resource := res "docs"

def report : Resource := res "docs/report"

def readDocs : Scope := fmEta (Prod Resource Action) (pair docs inspect)

def readReport : Scope := fmEta (Prod Resource Action) (pair report inspect)

def one : StepBound := szS sz0

def two : StepBound := szS (szS sz0)

-- L1: a narrower scope gives some (child, rest), and a wider scope gives
-- none.
def lawDelegate : Path (Option (Prod Capability Capability)) (delegate (cap readDocs two (budget 5)) readReport one 3) (some (pair (cap readReport one (budget 3)) (cap readDocs two (budget 2)))) :=
  refl

def lawWiden : Path (Option (Prod Capability Capability)) (delegate (cap readReport two (budget 5)) readDocs one 3) none :=
  refl

def child : Option (Prod Capability Capability) :=
  delegate (cap readDocs two (budget 5)) readReport one 3

def oneTwo : Flag := sizeLe one two

def twoOne : Flag := sizeLe two one
```

`build/langc --kit check examples/skill/g3-delegate.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-delegate.lang child` gives:

```text
some (pair (cap (fmEta (Prod Resource Action) (pair (resChild (resRoot 0) 1) inspect)) (szS sz0) (budget 3)) (cap (fmEta (Prod Resource Action) (pair (resRoot 0) inspect)) (szS (szS sz0)) (budget 2)))
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-delegate.lang oneTwo` gives:

```text
1
```

The exit code is 0.

`build/langc --kit eval examples/skill/g3-delegate.lang twoOne` gives:

```text
0
```

The exit code is 0.

`build/langc --kit check --json test/check/delegate-widen.lang` gives:

```text
{"diagnostics":[{"code":"REFUSE_DELEGATE_WIDEN","fix":null,"message":"wide: delegate: the child scope is not within the parent scope (scopeWithin gives flagNo) (PLAN 7)","span":{"col":1,"end_col":58,"end_line":16,"file":"test/check/delegate-widen.lang","line":15}}],"progress":{"laws_discharged":0,"laws_total":0,"parsed":7,"typed":6}}
```

The exit code is 1.

`build/langc explain REFUSE_DELEGATE_WIDEN` gives:

```text
REFUSE_DELEGATE_WIDEN: a closed `delegate` application whose child scope is not within the parent scope, at a term level of 1 (docs/host/README.md, Source language)
```

The exit code is 0.

### G4

The propose and execute of one approved mutation. The ticket `tOk`
holds the digest `hash (canon m1)` of the exact mutation `m1`.
`approvePlan` executes `m1` only in the `ppApproved` branch of the propose
of `m1`. `exact` is the first use of the approval, so it is permitted.
`usedTwice` is a second use, so it is denied with `approvalUsed`.
`changed` has the payload of `m2` under the digest of `m1`, so it is
denied with `digestDiffers`. It is a kit
file (examples/skill/kit.txt), so each command uses `--kit`.

File: examples/skill/g4-approve.lang

```lang
-- G4 (S15, Q-S15-2 (a)): propose and execute of one approved mutation. The
-- plan executes m1 only in the ppApproved branch of the propose of m1, and
-- judge permits the exact approved ticket one time. Kit mode: payloadOf and
-- the Nat literals are kit items.
def prod : Resource := res "prod"

def m1 : Mutation := mutation prod execute (payloadOf 7)

def m2 : Mutation := mutation prod execute (payloadOf 8)

def sc : Scope := fmEta (Prod Resource Action) (pair prod execute)

def a0 : Authority := authority sc (szS (szS sz0)) 5 (fmUnit Digest)

def tOk : Ticket := ticket m1 2 (some (hash (canon m1)))

def tChanged : Ticket := ticket m2 2 (some (hash (canon m1)))

def stop : Plan Unit := planDone Unit unit

def approvePlan : Plan Unit := planPropose Unit m1 (planExecute Unit m1 stop stop stop) stop

-- L3 and L7: the exact approved ticket is permitted, a second use of the
-- approval is denied, and a changed payload is denied.
def lawOk : Path Decision (first (judge a0 tOk)) permit := refl

def lawUsed : Path Decision (first (judge (second (judge a0 tOk)) tOk)) (deny approvalUsed) := refl

def lawDiffers : Path Decision (first (judge a0 tChanged)) (deny digestDiffers) := refl

def exact : Decision := first (judge a0 tOk)

def usedTwice : Decision := first (judge (second (judge a0 tOk)) tOk)

def changed : Decision := first (judge a0 tChanged)
```

`build/langc --kit check examples/skill/g4-approve.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-approve.lang exact` gives:

```text
permit
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-approve.lang usedTwice` gives:

```text
deny approvalUsed
```

The exit code is 0.

`build/langc --kit eval examples/skill/g4-approve.lang changed` gives:

```text
deny digestDiffers
```

The exit code is 0.

### G5

A deploy tool set behind the tool gate, the demo test/host/deploy.lang.
The scope `sc` holds `prod` and `dev`, and the authority `start` has 6
steps (the law `lawSteps`). Each `bind` line gives the action of one of
the 7 tools of test/fakedeploy.c and the RFC 6901 pointer to its target.
An execute needs an approval. The host test (test/hosttool.c, PLAN
section 8.3) runs the demo rows: an inspect in the scope, a propose and
one execute of the exact approved change are permitted. A changed target
or payload gives `deny digestDiffers`, a second execute gives `deny
approvalUsed`, no steps left gives `deny boundSpent`, and a call outside
the scope gives `deny scopeMiss`. It is a guest file, so the commands do
not use `--kit`.

File: examples/skill/g5-deploy.lang

```lang
-- G5 (S15, Q-S15-2 (a)): the demo of test/host/deploy.lang. A guest file:
-- res literals, Nat literals in def start only, and one bind line for each
-- of the 7 tools of test/fakedeploy.c. An execute needs an approval.
def prod : Resource := res "prod"

def dev : Resource := res "dev"

def sc : Scope :=
  fmMul (Prod Resource Action) (fmEta (Prod Resource Action) (pair prod inspect)) (fmMul (Prod Resource Action) (fmEta (Prod Resource Action) (pair prod propose)) (fmMul (Prod Resource Action) (fmEta (Prod Resource Action) (pair prod execute)) (fmEta (Prod Resource Action) (pair dev execute))))

def start : Authority := authority sc (szS (szS (szS (szS (szS (szS sz0)))))) 10 (fmUnit Digest)

-- L4: the start authority has 6 steps.
def lawSteps : Path StepBound (auSteps start) (szS (szS (szS (szS (szS (szS sz0)))))) := refl

def startSteps : StepBound := auSteps start

-- The tools: the action of each tool and the RFC 6901 pointer to the
-- target in its arguments.
bind "inspect" inspect "/target"
bind "propose" propose "/target"
bind "execute" execute "/target"
bind "sync_left" execute "/left"
bind "sync_right" execute "/right"
bind "restart" execute "/target"
bind "delete" execute "/target"
```

`build/langc check examples/skill/g5-deploy.lang` gives:

```text
ok
```

The exit code is 0.

`build/langc eval examples/skill/g5-deploy.lang prod` gives:

```text
resRoot 0
```

The exit code is 0.

`build/langc eval examples/skill/g5-deploy.lang dev` gives:

```text
resRoot 1
```

The exit code is 0.

`build/langc eval examples/skill/g5-deploy.lang startSteps` gives:

```text
szS (szS (szS (szS (szS (szS sz0)))))
```

The exit code is 0.
