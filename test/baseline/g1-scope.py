"""G1: a read-only scope with a step bound (g1-scope.lang).

The agent can inspect docs and each resource below it, one time. judge
gives the decision on a ticket and the authority after it. G1 has no
approvals, thus the payload (payloadOf 0) plays no part and an execute in
the scope is denied with noApproval.
"""
from collections import namedtuple

Authority = namedtuple("Authority", "scope steps left")
Ticket = namedtuple("Ticket", "target action cost")

PATHS = ("docs", "docs/report")
SEGMENTS = tuple(dict.fromkeys(s for p in PATHS for s in p.split("/")))


def res(path):
    """The chain of segment numbers, in the order of the first use."""
    return tuple(SEGMENTS.index(s) for s in path.split("/"))


def within(r, g):
    """True when g is r or an ancestor of r."""
    return r[: len(g)] == g


def allows(scope, r, a):
    return any(within(r, g) and a == b for g, b in scope)


def take(s, cost):
    """The authority after a decision: one step less and cost units less."""
    return Authority(s.scope, max(s.steps - 1, 0), max(s.left - cost, 0))


def judge(s, t):
    """The denials in the kit order: scopeMiss, boundSpent, budgetShort."""
    if not allows(s.scope, t.target, t.action):
        return "deny scopeMiss", take(s, 0)
    if s.steps == 0:
        return "deny boundSpent", take(s, 0)
    if t.cost > s.left:
        return "deny budgetShort", take(s, 0)
    if t.action == "execute":
        return "deny noApproval", take(s, 0)
    return "permit", take(s, t.cost)


docs = res("docs")
report = res("docs/report")
a0 = Authority(((docs, "inspect"),), 1, 5)
read_report = Ticket(report, "inspect", 1)
edit_report = Ticket(report, "execute", 1)
print(judge(a0, read_report)[0])
print(judge(a0, edit_report)[0])
print(judge(judge(a0, read_report)[1], read_report)[0])
