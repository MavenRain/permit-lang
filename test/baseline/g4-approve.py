"""G4: propose and execute of one approved mutation (g4-approve.lang).

The plan executes m1 only in the approved branch of the propose of m1, and
judge permits the exact approved ticket one time. canon m is the bytes of
the target and the payload; the digest of an approval is SHA-256 of canon m.
"""
import hashlib
from collections import namedtuple

Authority = namedtuple("Authority", "scope steps left used")
Mutation = namedtuple("Mutation", "target action payload")
Ticket = namedtuple("Ticket", "mutation cost digest")

PATHS = ("prod",)
SEGMENTS = tuple(dict.fromkeys(s for p in PATHS for s in p.split("/")))


def res(path):
    """The chain of segment numbers, in the order of the first use."""
    return tuple(SEGMENTS.index(s) for s in path.split("/"))


def within(r, g):
    """True when g is r or an ancestor of r."""
    return r[: len(g)] == g


def allows(scope, r, a):
    return any(within(r, g) and a == b for g, b in scope)


def canon(m):
    return ("/".join(map(str, m.target)) + " " + str(m.payload)).encode()


def digest(m):
    return hashlib.sha256(canon(m)).digest()


def take(s, cost, used):
    """The authority after a decision: one step less, cost units less."""
    return Authority(s.scope, max(s.steps - 1, 0), max(s.left - cost, 0), used)


def judge(s, t):
    """The denials in the kit order: scopeMiss, boundSpent, budgetShort,
    noApproval, digestDiffers, approvalUsed. A permit of an execute adds the
    digest to the used digests."""
    m = t.mutation
    keep = take(s, 0, s.used)
    if not allows(s.scope, m.target, m.action):
        return "deny scopeMiss", keep
    if s.steps == 0:
        return "deny boundSpent", keep
    if t.cost > s.left:
        return "deny budgetShort", keep
    if m.action != "execute":
        return "permit", take(s, t.cost, s.used)
    if t.digest is None:
        return "deny noApproval", keep
    if t.digest != digest(m):
        return "deny digestDiffers", keep
    if t.digest in s.used:
        return "deny approvalUsed", keep
    return "permit", take(s, t.cost, s.used + (t.digest,))


prod = res("prod")
m1 = Mutation(prod, "execute", 7)
m2 = Mutation(prod, "execute", 8)
a0 = Authority(((prod, "execute"),), 2, 5, ())
t_ok = Ticket(m1, 2, digest(m1))
t_changed = Ticket(m2, 2, digest(m1))
stop = ("done",)
approve_plan = ("propose", m1, ("execute", m1, stop, stop, stop), stop)
print(judge(a0, t_ok)[0])
print(judge(judge(a0, t_ok)[1], t_ok)[0])
print(judge(a0, t_changed)[0])
