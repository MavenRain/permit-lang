/* The `langc cedar` text. See cedar.h. */
#include <stdio.h>

#include "cedar.h"
#include "jcs.h"
#include "report.h"

/* The header (Q-S11-3 (a)). A program with no def start gets line 1 only. */
static const char HEAD_LINE1[] = "// permit-lang T3: the static scope of def start as Cedar permit policies.\n";
static const char HEAD_LINES23[] =
    "// Cedar denies each request that no policy permits; principal is open.\n"
    "// Each Resource::\"a/b\" must have the parent Resource::\"a\" in the request entities.\n";

/* Writes TEXT to OUT. Returns 1, or 0 when the write fails. */
static int put(FILE *out, const char *text) {
  return fputs(text, out) != EOF;
}

static int bad_line(Machine *m, const char *what) {
  return diag_fail(m->diag, "INTERNAL", "cedar", "the program line has %s", what);
}

/* Writes the string V as it is. */
static int put_string(Machine *m, FILE *out, const JsonValue *v) {
  const char *text = json_get_string(v);
  return text != NULL ? put(out, text) : bad_line(m, "a value that is not a string");
}

/* Writes the string V in its JSON escape form, the JCS form of the program
   line, so no raw control byte goes into a comment line. The JCS form of
   one byte has at most 6 bytes. */
static int put_json(Machine *m, FILE *out, const JsonValue *v) {
  JcsBuf buf = {NULL, 0, 0};
  if (v == NULL || v->kind != JSON_STRING)
    return bad_line(m, "a value that is not a string");
  buf.cap = 6u * v->len + 2u;
  buf.bytes = arena_alloc(m->arena, buf.cap);
  if (buf.bytes == NULL)
    return diag_fail(m->diag, "OOM", "cedar", "out of memory");
  if (jcs_write(v, &buf) != JCS_OK)
    return bad_line(m, "a string with no JCS form");
  return fwrite(buf.bytes, 1u, buf.len, out) == buf.len;
}

/* Writes the policy of the grant G, {"action","resource"} (Q-S11-2 (a)):
   `in`, because a grant also covers each resource below it. The text goes
   in as it is: an action is an arm name, and the checker keeps a resource
   to a-z, 0-9, _, - and / (REFUSE_RES). */
static int put_grant(Machine *m, FILE *out, const JsonValue *g) {
  return put(out, "permit (principal, action == Action::\"") && put_string(m, out, json_member(g, "action")) &&
         put(out, "\", resource in Resource::\"") && put_string(m, out, json_member(g, "resource")) &&
         put(out, "\");\n");
}

/* Writes the bind B, {"action","pointer","tool"}, as "TOOL" ACTION
   "POINTER". */
static int put_bind(Machine *m, FILE *out, const JsonValue *b) {
  return put_json(m, out, json_member(b, "tool")) && put(out, " ") && put_string(m, out, json_member(b, "action")) &&
         put(out, " ") && put_json(m, out, json_member(b, "pointer"));
}

/* Writes the items of the array A with ITEM and SEP between two items, or
   "none" for an empty array. */
static int put_list(Machine *m, FILE *out, const JsonValue *a, const char *sep,
                    int (*item)(Machine *, FILE *, const JsonValue *)) {
  const JsonValue *v;
  int ok = 1;
  if (a == NULL || a->kind != JSON_ARRAY)
    return bad_line(m, "a list that is not an array");
  if (a->first == NULL)
    return put(out, "none");
  for (v = a->first; ok && v != NULL; v = v->next)
    ok = (v == a->first || put(out, sep)) && item(m, out, v);
  return ok;
}

/* Writes the policies of the start scope SCOPE in walk order, or the
   comment for a scope with no grant. */
static int put_scope(Machine *m, FILE *out, const JsonValue *scope) {
  const JsonValue *g;
  int ok = 1;
  if (scope->first == NULL)
    return put(out, "// The start scope has no grant: no policy.\n");
  for (g = scope->first; ok && g != NULL; g = g->next)
    ok = put_grant(m, out, g);
  return ok;
}

/* Writes the comments for the parts that the export leaves out, in the
   order of Q-S11-3 (a). */
static int put_rest(Machine *m, FILE *out, const JsonValue *line) {
  return put(out, "// Not exported: steps. def start allows ") && put_string(m, out, json_member(line, "steps")) &&
         put(out, " calls.\n// Not exported: budget. def start has ") &&
         put_string(m, out, json_member(line, "budget")) &&
         put(out, " units; the gate takes the cost of each call.\n"
                  "// Not exported: approval binding. Each execute call needs an approval with the digest of its "
                  "canonical target and payload, used one time.\n"
                  "// Not exported: sequence guard (L5). On each Plan path, an execute comes after an approved "
                  "propose of the same mutation.\n"
                  "// Not exported: laws. ") &&
         put_list(m, out, json_member(line, "laws"), ", ", put_string) && put(out, "\n// Not exported: bind lines. ") &&
         put_list(m, out, json_member(line, "binds"), "; ", put_bind) && put(out, "\n");
}

int cedar_program(Machine *m, const IrProgram *prog, FILE *out) {
  JsonValue line;
  const JsonValue *scope;
  int ok;
  if (!report_value(m, prog, &line))
    return 0;
  scope = json_member(&line, "scope");
  if (scope == NULL || (scope->kind != JSON_NULL && scope->kind != JSON_ARRAY))
    return bad_line(m, "no scope");
  ok = put(out, HEAD_LINE1) &&
       (scope->kind == JSON_NULL ? put(out, "// No def start: no policy.\n")
                                 : put(out, HEAD_LINES23) && put_scope(m, out, scope) && put_rest(m, out, &line));
  if (!ok && m->diag->set)
    return 0;
  return ok && fflush(out) == 0 ? 1 : diag_fail(m->diag, "IO", "cedar", "cannot write the policies");
}
