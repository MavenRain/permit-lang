"""G5: the demo of test/host/deploy.lang (g5-deploy.lang).

The start authority: 4 grants on prod and dev, 6 steps and 10 units. Each
of the 7 tools of test/fakedeploy.c has an action and the RFC 6901 pointer
to the target in its arguments. An execute needs an approval.
"""
import functools
from collections import namedtuple

Authority = namedtuple("Authority", "scope steps left used")

PATHS = ("prod", "dev")
SEGMENTS = tuple(dict.fromkeys(s for p in PATHS for s in p.split("/")))


def res(path):
    """The chain of segment numbers, in the order of the first use."""
    return tuple(SEGMENTS.index(s) for s in path.split("/"))


def app(head, *args):
    """A constructor application in the langc normal form."""
    return " ".join((head,) + tuple(f"({a})" if " " in a else a for a in args))


def show_res(r):
    return functools.reduce(lambda acc, seg: app("resChild", acc, str(seg)), r[1:], app("resRoot", str(r[0])))


def show_size(k):
    return functools.reduce(lambda acc, _: app("szS", acc), range(k), "sz0")


prod = res("prod")
dev = res("dev")
sc = ((prod, "inspect"), (prod, "propose"), (prod, "execute"), (dev, "execute"))
start = Authority(sc, 6, 10, ())
start_steps = start.steps
tools = {
    "inspect": ("inspect", "/target"),
    "propose": ("propose", "/target"),
    "execute": ("execute", "/target"),
    "sync_left": ("execute", "/left"),
    "sync_right": ("execute", "/right"),
    "restart": ("execute", "/target"),
    "delete": ("execute", "/target"),
}
print(show_res(prod))
print(show_res(dev))
print(show_size(start_steps))
