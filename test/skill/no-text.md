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
