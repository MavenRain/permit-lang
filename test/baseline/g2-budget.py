"""G2: a budget with costs and a split (g2-budget.lang).

split b n gives the n units first and the rest second, and spend b n gives
the rest. A cost that is more than the budget gives none.
"""
from collections import namedtuple

Budget = namedtuple("Budget", "units")


def split(b, n):
    return None if n > b.units else (Budget(n), Budget(b.units - n))


def spend(b, n):
    return None if n > b.units else Budget(b.units - n)


def budget_left(b):
    return b.units


def app(head, *args):
    """A constructor application in the langc normal form."""
    return " ".join((head,) + tuple(f"({a})" if " " in a else a for a in args))


def show_budget(b):
    return app("budget", str(b.units))


def show_option(value, show):
    return "none" if value is None else app("some", show(value))


halves = split(Budget(5), 3)
rest = spend(Budget(5), 3)
left = budget_left(Budget(4))
print(show_option(halves, lambda p: app("pair", show_budget(p[0]), show_budget(p[1]))))
print(show_option(rest, show_budget))
print(left)
