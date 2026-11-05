"""G3: an attenuated delegation to a sub-agent (g3-delegate.lang).

delegate gives (child, rest) when the child scope is within the parent
scope, the child step bound is not longer and the budget covers the cost.
The child has the n units, and the rest keeps the parent scope, the parent
step bound and the remaining units.
"""
import functools
from collections import namedtuple

Cap = namedtuple("Cap", "scope steps budget")

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


def scope_within(s, t):
    """True when t allows each grant of s."""
    return all(allows(t, g, b) for g, b in s)


def size_le(a, b):
    return a <= b


def split(b, n):
    return None if n > b else (n, b - n)


def delegate(c, s, k, n):
    halves = split(c.budget, n) if scope_within(s, c.scope) and size_le(k, c.steps) else None
    return None if halves is None else (Cap(s, k, halves[0]), Cap(c.scope, c.steps, halves[1]))


def app(head, *args):
    """A constructor application in the langc normal form."""
    return " ".join((head,) + tuple(f"({a})" if " " in a else a for a in args))


def show_res(r):
    return functools.reduce(lambda acc, seg: app("resChild", acc, str(seg)), r[1:], app("resRoot", str(r[0])))


def show_size(k):
    return functools.reduce(lambda acc, _: app("szS", acc), range(k), "sz0")


def show_scope(scope):
    """The G3 scopes have one grant each: fmEta of the grant."""
    ((g, b),) = scope
    return app("fmEta", "Prod Resource Action", app("pair", show_res(g), b))


def show_cap(c):
    return app("cap", show_scope(c.scope), show_size(c.steps), app("budget", str(c.budget)))


def show_option(value, show):
    return "none" if value is None else app("some", show(value))


docs = res("docs")
report = res("docs/report")
read_docs = ((docs, "inspect"),)
read_report = ((report, "inspect"),)
one = 1
two = 2
child = delegate(Cap(read_docs, two, 5), read_report, one, 3)
print(show_option(child, lambda p: app("pair", show_cap(p[0]), show_cap(p[1]))))
print(int(size_le(one, two)))
print(int(size_le(two, one)))
