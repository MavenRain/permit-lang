/* The table of `langc explain CODE` (S14 A4, Q-S14-2 (a)), ported from
   accrue-lang src/front/codes.c (02964ec). Each row is a refusal code of
   docs/host/README.md, in the order of its first use there: the Source
   language rules, the CLI, Compilation, No traps, Wasm layout and Gate
   sections. The meaning is the README text verbatim. */
#include <string.h>

#include "codes.h"

static const CodeRow CODE_ROWS[] = {
  {"REFUSE_AFFINE_REUSE", "Source language", "a variable that one evaluation path uses more times than its multiplicity allows (PLAN 6)"},
  {"REFUSE_AFFINE_OMEGA", "Source language", "a variable at multiplicity 1 that goes to an omega binder, to a closure that can run more than one time, to a polymorphic payload that can become copyable, or to a `map` result that can be copyable"},
  {"REFUSE_AFFINE_FIX", "Source language", "a variable at multiplicity 1 that a fold step or an elimination case uses, when that step or case can run more than one time"},
  {"REFUSE_AFFINE_PATH", "Source language", "a variable at multiplicity 1 that is free under an interval binder (`fun i =>`, `refl`, `sym`, `cong`, `funExt`, `transp`, `hcomp`, `Glue`, `glue` or `unglue`): both end points of the path hold it"},
  {"REFUSE_DATA", "Source language", "a `family` declaration in a program"},
  {"REFUSE_CELL", "Source language", "a type constructor that is not one cell signature or one cocell signature, a cons-list name in place of a cell monad, or a sealed family that is not a cell monad"},
  {"REFUSE_AXIOM", "Source language", "an `axiom` declaration"},
  {"REFUSE_REC", "Source language", "`def rec`, or a definition that refers to itself or to a later definition"},
  {"REFUSE_NAME", "Source language", "a core name, a domain name, a reserved name or a repeated name as a new definition"},
  {"REFUSE_TIME", "Source language", "one of 10 time words (`now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `date`, `ttl`, `deadline`, `expires`) as a name or a use; the message says to count operations: a deadline is a `StepBound` (a count of later operations)"},
  {"REFUSE_DELEGATE_WIDEN", "Source language", "a closed `delegate` application whose child scope is not within the parent scope, at a term level of 1"},
  {"REFUSE_BUDGET_OVERSPEND", "Source language", "a closed `spend`, `split` or `delegate` application whose budget part reduces to `none`, at a term level of 1"},
  {"REFUSE_HISTORY_WRITE", "Source language", "a history field `h` that is not `(h t)`, `natAdd (h t) e` or the variable of `bind (natAdd (h t) e)`, a history constructor without all of its arguments, or a definition with `State` in its type that refers to a name that the history rule does not check"},
  {"REFUSE_MAP", "Source language", "a `Map` key type that is not `Nat`, or a `Map` value type that is not `Nat` or `Flag`"},
  {"REFUSE_SIG", "Source language", "a name of the design signature (PLAN section 3): a slot with the wrong type, a reserved name that no slice gives yet, or a defined or primitive name that the kernel gives"},
  {"REFUSE_STATE", "Source language", "a second state, a state type with a name other than `State`, a state with parameters, an index telescope or not one constructor, a state field that is not first-order, or a history field that is not `Nat`"},
  {"REFUSE_PARTIAL", "Source language", "a partial name, a literal 0 as a `NonZero Nat`, or an `Option` result in the place of a value"},
  {"REFUSE_LOWER", "Source language", "an entry that the Wasm host cannot lower: heap use with no static bound, a recursive fold over a block of families, a `start` that is not closed, a `bind` line with no `def start`, or an entry with the name of a bind export (`langc ir` and `langc build` only)"},
  {"REFUSE_KIT", "Source language", "a kit item outside the base span (PLAN section 4) in guest mode; `langc --kit` accepts it"},
  {"REFUSE_EXECUTE_UNAPPROVED", "Source language", "a `planExecute` that is not in the `ppApproved` branch of an earlier `planPropose` of the same mutation on its path"},
  {"REFUSE_RES", "Source language", "a resource path that is not segments of `a-z`, `0-9`, `_` and `-` with one `/` between two segments, a `*`, or more resource paths or segments than the table holds"},
  {"REFUSE_BIND", "Source language", "a second `bind` line of one tool, an action that is not an arm of `Action`, a pointer that is not an RFC 6901 pointer, or more `bind` lines than the table holds"},
  {"REFUSE_LAW", "Source language", "a definition that uses a law-only name (`decrypt`, `observe`, an elim, fold or projection of a sealed family) but has a type that is not a law (a `Path` or `Trunc1` codomain); `langc check` cannot give it (Q-S13-9)"},
  {"REFUSE_REPORT", "CLI", "a `def start` that is not a closed `authority`, or that has steps or a scope that are not closed"},
  {"REFUSE_RUNTIME_CLOSURE", "Compilation", "a function value must exist at run time."},
  {"REFUSE_RUNTIME_TREE", "Compilation", "a fold walks a tree that exists only at run time."},
};

size_t code_count(void) { return sizeof CODE_ROWS / sizeof CODE_ROWS[0]; }

const CodeRow *code_at(size_t index) { return index < code_count() ? &CODE_ROWS[index] : NULL; }

const CodeRow *code_find(const char *code) {
  for (size_t i = 0; i < code_count(); i++) {
    if (strcmp(CODE_ROWS[i].code, code) == 0) return &CODE_ROWS[i];
  }
  return NULL;
}
