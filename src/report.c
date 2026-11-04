/* The `program` line of `langc report`. See report.h. */
#include <stdio.h>
#include <string.h>

#include "jcs.h"
#include "report.h"

/* The JSON text of the line, on the arena: the JsonOut layout of
   evolve-lang 550e73b src/json.c. The buffer starts at 4096 bytes and
   doubles when the text does not fit. Each put returns 1, or 0 after a
   diagnostic, so the calls chain with &&. The text is not canonical: JCS
   makes the bytes (Q-S10-2 (a)). */
typedef struct {
  Arena *arena;
  Diag *diag;
  char *data; /* NUL-terminated; NULL before the first put */
  size_t len;
  size_t cap;
} JsonOut;

static int out_put(JsonOut *out, const char *text, size_t n) {
  size_t cap = out->cap == 0 ? 4096u : out->cap;
  char *data;
  while (cap < out->len + n + 1u)
    cap *= 2u;
  if (cap != out->cap) {
    data = arena_alloc(out->arena, cap);
    if (data == NULL)
      return diag_fail(out->diag, "OOM", "report", "out of memory");
    if (out->len > 0)
      memcpy(data, out->data, out->len);
    out->data = data;
    out->cap = cap;
  }
  memcpy(out->data + out->len, text, n);
  out->len += n;
  out->data[out->len] = '\0';
  return 1;
}

static int out_text(JsonOut *out, const char *text) {
  return out_put(out, text, strlen(text));
}

/* Adds the N bytes at TEXT inside a JSON string: '"', '\\' and each byte
   below 0x20 as a \u escape, each other byte as it is. */
static int out_bytes(JsonOut *out, const char *text, size_t n) {
  char esc[8];
  size_t i;
  int ok = 1;
  for (i = 0; ok && i < n; i++) {
    unsigned char c = (unsigned char)text[i];
    snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
    ok = c >= 0x20u && c != '"' && c != '\\' ? out_put(out, text + i, 1u) : out_text(out, esc);
  }
  return ok;
}

/* Adds TEXT as a JSON string, or null when TEXT is NULL. */
static int out_string(JsonOut *out, const char *text) {
  return text == NULL ? out_text(out, "null")
                      : out_text(out, "\"") && out_bytes(out, text, strlen(text)) && out_text(out, "\"");
}

/* Adds the 64-bit word W as a JSON string of decimal digits (Q-S10-3 (a)):
   JCS refuses a number above 2^53 - 1. */
static int out_word(JsonOut *out, uint64_t w) {
  char buf[32];
  snprintf(buf, sizeof buf, "\"%llu\"", (unsigned long long)w);
  return out_text(out, buf);
}

/* Adds ",", except before item 0. */
static int out_comma(JsonOut *out, size_t i) {
  return i == 0 || out_text(out, ",");
}

static int refuse(Machine *m, const char *what) {
  return diag_fail(m->diag, "REFUSE_REPORT", m->def, "%s (S10)", what);
}

static int is_ctor(const Machine *m, const Value *v, const char *name) {
  return v != NULL && v->kind == VAL_OP && v->op == OP_CTOR && strcmp(m->ctors[v->inst].name, name) == 0;
}

/* The name of the arm ACTION of the domain family Action, or NULL. */
static const char *action_name(const Machine *m, uint32_t action) {
  uint32_t f;
  for (f = 0; f < m->family_count; f++)
    if (strcmp(m->families[f].name, "Action") == 0)
      return action < m->families[f].ctor_count ? m->ctors[m->families[f].first_ctor + action].name : NULL;
  return NULL;
}

/* Adds the bind rows of PROG in program order as
   [{"action","pointer","tool"}]. */
static int out_binds(const Machine *m, JsonOut *out, const IrProgram *prog) {
  size_t i;
  int ok = out_text(out, ",\"binds\":[");
  for (i = 0; ok && i < prog->bind_count; i++)
    ok = out_comma(out, i) && out_text(out, "{\"tool\":") && out_string(out, prog->binds[i].tool) &&
         out_text(out, ",\"action\":") && out_string(out, action_name(m, prog->binds[i].action)) &&
         out_text(out, ",\"pointer\":") && out_string(out, prog->binds[i].pointer) && out_text(out, "}");
  return ok && out_text(out, "]");
}

/* 1 when the type T ends in a Path: T is a path type, or a Pi whose
   codomain at a new variable of LEVEL ends in a Path. */
static int ends_in_path(Machine *m, const Value *t, uint32_t level) {
  const Value *cod;
  if (t == NULL || t->kind != VAL_PI)
    return t != NULL && t->kind == VAL_PATHP;
  cod = t->body != NULL ? closure_apply(m, t, val_var(m, level)) : t->arg;
  return ends_in_path(m, cod, level + 1u);
}

/* Adds the laws (Q-S10-3 (a)): the names of the program defs whose type
   ends in a Path, in def order. Each passed check, so each is discharged. */
static int out_laws(Machine *m, JsonOut *out) {
  uint32_t i;
  size_t n = 0;
  int law;
  int ok = out_text(out, ",\"laws\":[");
  for (i = 0; ok && i < m->def_count; i++) {
    m->def = m->defs[i].name;
    m->fuel = EVAL_FUEL_STEPS;
    law = m->defs[i].origin == ORIGIN_PROGRAM && m->defs[i].name != NULL && ends_in_path(m, m->defs[i].type, 0u);
    ok = !m->diag->set && (!law || (out_comma(out, n++) && out_string(out, m->defs[i].name)));
  }
  return ok && out_text(out, "]");
}

/* Adds the path text of the closed Resource chain V: the segments from the
   root, joined with '/'. */
static int out_res(Machine *m, JsonOut *out, const Value *v) {
  int child = val_is(v, OP_RES_CHILD) && v->argc == 2u;
  const Value *seg = child ? v->args[1] : val_is(v, OP_RES_ROOT) && v->argc == 1u ? v->args[0] : NULL;
  if (seg == NULL || seg->kind != VAL_NAT || seg->nat >= m->res_seg_count)
    return refuse(m, "a scope resource is not closed");
  return (!child || (out_res(m, out, v->args[0]) && out_text(out, "/"))) &&
         out_bytes(out, m->res_segs[seg->nat].text, m->res_segs[seg->nat].len);
}

/* Adds the grant V, a closed pair (res P) action, as item I
   {"action","resource"}. */
static int out_grant(Machine *m, JsonOut *out, const Value *v, size_t i) {
  const Value *a = val_is(v, OP_PAIR) && v->argc == 2u ? v->args[1] : NULL;
  if (a == NULL || a->kind != VAL_OP || a->op != OP_CTOR)
    return refuse(m, "a scope grant is not closed");
  return out_comma(out, i) && out_text(out, "{\"action\":") && out_string(out, m->ctors[a->inst].name) &&
         out_text(out, ",\"resource\":\"") && out_res(m, out, v->args[0]) && out_text(out, "\"}");
}

/* Adds the grants of the closed scope V left to right. A FreeMon
   constructor has the parameter in args[0]: fmMul A l r, fmEta A g, fmUnit
   A. *COUNT is the count of grants so far. */
static int out_scope(Machine *m, JsonOut *out, const Value *v, size_t *count) {
  int ok = 1;
  for (; ok && is_ctor(m, v, "fmMul") && v->argc == 3u; v = v->args[2])
    ok = out_scope(m, out, v->args[1], count);
  if (!ok || (is_ctor(m, v, "fmUnit") && v->argc == 1u))
    return ok;
  if (!is_ctor(m, v, "fmEta") || v->argc != 2u)
    return refuse(m, "the start scope is not closed");
  return out_grant(m, out, v->args[1], (*count)++);
}

/* Adds budget, scope and steps of the closed start authority (the words of
   region_of in lower.c), or null for each when there is no def start. */
static int out_start(Machine *m, JsonOut *out) {
  uint32_t si = m->def_count;
  uint32_t i;
  const Value *s;
  const Value *k;
  uint64_t steps = 0;
  size_t count = 0;
  for (i = 0; i < m->def_count && si == m->def_count; i++)
    si = m->defs[i].origin == ORIGIN_PROGRAM && m->defs[i].name != NULL && strcmp(m->defs[i].name, "start") == 0 ? i : si;
  if (si == m->def_count)
    return out_text(out, ",\"budget\":null,\"scope\":null,\"steps\":null");
  m->def = "start";
  m->fuel = EVAL_FUEL_STEPS;
  s = def_value(m, si);
  if (s == NULL)
    return m->diag->set ? 0 : diag_fail(m->diag, "INTERNAL", m->def, "the report found a missing start");
  if (!is_ctor(m, s, "authority") || s->argc != 4u || s->args[2]->kind != VAL_NAT)
    return refuse(m, "start is not a closed authority");
  for (k = s->args[1]; is_ctor(m, k, "szS") && k->argc == 1u; k = k->args[0])
    steps++;
  if (!is_ctor(m, k, "sz0"))
    return refuse(m, "start steps are not closed");
  return out_text(out, ",\"budget\":") && out_word(out, s->args[2]->nat) && out_text(out, ",\"steps\":") &&
         out_word(out, steps) && out_text(out, ",\"scope\":[") && out_scope(m, out, s->args[0], &count) &&
         out_text(out, "]");
}

int report_value(Machine *m, const IrProgram *prog, JsonValue *value) {
  JsonOut text = {m->arena, m->diag, NULL, 0, 0};
  int ok = out_text(&text, "{\"kind\":\"program\",\"permit\":1") && out_binds(m, &text, prog) &&
           out_laws(m, &text) && out_start(m, &text) && out_text(&text, "}");
  return ok && json_parse(m->arena, text.data, text.len, value, m->diag);
}

int report_program(Machine *m, const IrProgram *prog, FILE *out) {
  JsonValue value;
  JcsBuf buf = {NULL, 0, 0};
  JcsResult res = JCS_FULL;
  size_t cap;
  if (!report_value(m, prog, &value))
    return 0;
  /* The buffer starts at 4096 bytes and doubles while the JCS form does not
     fit. */
  for (cap = 4096u; res == JCS_FULL; cap *= 2u) {
    buf.bytes = arena_alloc(m->arena, cap);
    if (buf.bytes == NULL)
      return diag_fail(m->diag, "OOM", "report", "out of memory");
    buf.cap = cap;
    buf.len = 0;
    res = jcs_write(&value, &buf);
  }
  if (res != JCS_OK)
    return diag_fail(m->diag, "INTERNAL", "report", "the report line has no JCS form");
  return fwrite(buf.bytes, 1u, buf.len, out) == buf.len && fputc('\n', out) != EOF && fflush(out) == 0
           ? 1
           : diag_fail(m->diag, "IO", "report", "cannot write the report");
}
