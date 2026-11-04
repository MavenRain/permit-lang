/* The canonical printer (`langc fmt`), ported from accrue-lang f8a02d7 (the
   cairn-lang printer). Rulings: Q-S12-2 layout (the accrue layout and no
   blank line inside a bind run), Q-S12-3 spellings (`Sigma (x : A) B`, one
   name in each group, one `fun` for consecutive lambdas, the marks
   `(@0 x : A)`, `(@1 x : A)` and `A -o B`, verbatim strings, and the H1
   group), Q-S12-4 comments above the decl. */
#include "front/fmt.h"

#include <string.h>

#include "front/front.h"
#include "front/parser.h"

/* A decl whose flat text is longer breaks (Q-S12-2). */
#define FMT_WIDTH 100u

/* The precedence levels, low to high, as the parser reads them: an operand
   below the level that its place needs gets parentheses. */
typedef enum {
  LEVEL_TERM,
  LEVEL_JOIN,
  LEVEL_MEET,
  LEVEL_PAPP,
  LEVEL_APP,
  LEVEL_ATOM
} FmtLevel;

void fmt_text_init(FmtText *out, Arena *arena) {
  out->arena = arena;
  out->data = NULL;
  out->len = 0;
  out->cap = 0;
  out->oom = 0;
}

static void put_bytes(FmtText *out, const char *bytes, size_t n) {
  if (out->oom || n == 0) return;
  if (out->len + n > out->cap) {
    size_t cap = out->cap == 0 ? 256u : out->cap;
    char *data;
    while (cap < out->len + n) cap *= 2u;
    data = arena_alloc(out->arena, cap);
    if (data == NULL) {
      out->oom = 1;
      return;
    }
    if (out->len > 0) memcpy(data, out->data, out->len);
    out->data = data;
    out->cap = cap;
  }
  memcpy(out->data + out->len, bytes, n);
  out->len += n;
}

static void put_str(FmtText *out, const char *text) {
  put_bytes(out, text, strlen(text));
}

static void put_nat(FmtText *out, uint64_t n) {
  char digits[24];
  size_t at = sizeof digits;
  do {
    digits[--at] = (char)('0' + n % 10u);
    n /= 10u;
  } while (n != 0);
  put_bytes(out, digits + at, sizeof digits - at);
}

/* A string has no escapes (any byte but `"` and a newline), so the printer
   writes its bytes as they are. */
static void put_quoted(FmtText *out, const char *text) {
  put_str(out, "\"");
  put_str(out, text);
  put_str(out, "\"");
}

/* Type n and Sigma are parenthesized as an argument, so they sit at the
   application level; `~` binds tighter than application. A system and a
   branch are never parenthesized. */
static FmtLevel term_level(const Term *t) {
  switch (t->kind) {
    case TERM_LAM:
    case TERM_PI:
      return LEVEL_TERM;
    case TERM_IJOIN: return LEVEL_JOIN;
    case TERM_IMEET: return LEVEL_MEET;
    case TERM_PAPP: return LEVEL_PAPP;
    case TERM_APP:
    case TERM_SIGMA:
    case TERM_UNIV:
      return LEVEL_APP;
    case TERM_VAR:
    case TERM_NAT:
    case TERM_INEG:
    case TERM_SYSTEM:
    case TERM_BRANCH:
    case TERM_RES:
      break;
  }
  return LEVEL_ATOM;
}

/* 1 when NAME is a free variable of T. A binder of T hides NAME in its own
   right side; the text of a `res` literal is not a variable. */
static int names_free(const Term *t, const char *name) {
  if (t == NULL) return 0;
  switch (t->kind) {
    case TERM_VAR: return strcmp(t->name, name) == 0;
    case TERM_LAM:
    case TERM_PI:
    case TERM_SIGMA:
      return names_free(t->left, name)
          || ((t->name == NULL || strcmp(t->name, name) != 0) && names_free(t->right, name));
    case TERM_APP:
    case TERM_PAPP:
    case TERM_INEG:
    case TERM_IMEET:
    case TERM_IJOIN:
    case TERM_SYSTEM:
    case TERM_BRANCH:
      return names_free(t->left, name) || names_free(t->right, name);
    case TERM_NAT:
    case TERM_UNIV:
    case TERM_RES:
      break;
  }
  return 0;
}

static void put_term(FmtText *out, const Term *t, FmtLevel level);

static void put_mark(FmtText *out, Mult mult) {
  switch (mult) {
    case MULT_OMEGA: break;
    case MULT_ZERO: put_str(out, "@0 "); break;
    case MULT_ONE: put_str(out, "@1 "); break;
  }
}

/* The number of nodes from T on that came from one binder group (parser.c
   wrap): one kind, one mark and one type pointer. T is a named Pi, a Sigma
   or a typed lambda. */
static size_t group_len(const Term *t) {
  size_t count = 1;
  for (const Term *next = t->right;
       next != NULL && next->kind == t->kind && next->name != NULL && next->mult == t->mult && next->left == t->left;
       next = next->right) {
    count++;
  }
  return count;
}

/* H1 (Q-S12-3, Q-S12-7): 1 when T names an earlier name of the group. Then
   the printer keeps the source group `(x y : T)`. The split
   `(x : T) -> (y : T)` has the same meaning: the checker also checks the T
   of node y with x in scope. */
static int group_kept(const Term *t, size_t count) {
  const Term *node = t;
  int keep = 0;
  for (size_t k = 0; k + 1 < count; k++) {
    keep |= names_free(t->left, node->name);
    node = node->right;
  }
  return keep;
}

/* `(x : A)`, `(@0 x : A)` or `(@1 x : A)` for the group that starts at T:
   one name in each group, or the whole group when T names an earlier
   name of the group (H1). Returns the node after the printed names. */
static const Term *put_binders(FmtText *out, const Term *t) {
  size_t count = group_len(t);
  size_t take = group_kept(t, count) ? count : 1u;
  const Term *node = t;
  put_str(out, "(");
  put_mark(out, t->mult);
  for (size_t k = 0; k < take; k++) {
    put_str(out, node->name);
    put_str(out, " ");
    node = node->right;
  }
  put_str(out, ": ");
  put_term(out, t->left, LEVEL_TERM);
  put_str(out, ")");
  return node;
}

/* All leading lambdas go into one binder head; a NULL type is a bare name. */
static void put_lambda(FmtText *out, const Term *t) {
  const Term *body = t;
  put_str(out, "fun");
  while (body->kind == TERM_LAM) {
    put_str(out, " ");
    if (body->left == NULL) {
      put_str(out, body->name);
      body = body->right;
    } else {
      body = put_binders(out, body);
    }
  }
  put_str(out, " => ");
  put_term(out, body, LEVEL_TERM);
}

/* A named Pi `(x : A) -> B`; an unnamed Pi `A -> B`, or `A -o B` at 1. */
static void put_pi(FmtText *out, const Term *t) {
  const Term *body = t->right;
  if (t->name == NULL) {
    put_term(out, t->left, LEVEL_JOIN);
  } else {
    body = put_binders(out, t);
  }
  put_str(out, t->name == NULL && t->mult == MULT_ONE ? " -o " : " -> ");
  put_term(out, body, LEVEL_TERM);
}

static void put_sigma(FmtText *out, const Term *t) {
  put_str(out, "Sigma ");
  const Term *body = put_binders(out, t);
  put_str(out, " ");
  put_term(out, body, LEVEL_ATOM);
}

/* `[ k1 -> t1, k2 -> t2 ]`; the one cell of `[ ]` has no branch. */
static void put_system(FmtText *out, const Term *t) {
  put_str(out, "[");
  for (const Term *cell = t; cell != NULL; cell = cell->right) {
    if (cell->left != NULL) {
      put_str(out, cell == t ? " " : ", ");
      put_term(out, cell->left, LEVEL_ATOM);
    }
  }
  put_str(out, " ]");
}

/* A left-associative operator: the left operand at the level of the
   operator, the right operand one level up. */
static void put_binary(FmtText *out, const Term *t, const char *op, FmtLevel left, FmtLevel right) {
  put_term(out, t->left, left);
  put_str(out, op);
  put_term(out, t->right, right);
}

/* ` @ ` keeps its spaces: `p @0` lexes as a mark. */
static void put_term(FmtText *out, const Term *t, FmtLevel level) {
  int paren = term_level(t) < level;
  if (paren) put_str(out, "(");
  switch (t->kind) {
    case TERM_VAR: put_str(out, t->name); break;
    case TERM_NAT: put_nat(out, t->nat); break;
    case TERM_APP: put_binary(out, t, " ", LEVEL_APP, LEVEL_ATOM); break;
    case TERM_LAM: put_lambda(out, t); break;
    case TERM_PI: put_pi(out, t); break;
    case TERM_SIGMA: put_sigma(out, t); break;
    case TERM_UNIV:
      put_str(out, "Type ");
      put_nat(out, t->nat);
      break;
    case TERM_PAPP: put_binary(out, t, " @ ", LEVEL_PAPP, LEVEL_APP); break;
    case TERM_INEG:
      put_str(out, "~");
      put_term(out, t->left, LEVEL_ATOM);
      break;
    case TERM_IMEET: put_binary(out, t, " /\\ ", LEVEL_MEET, LEVEL_PAPP); break;
    case TERM_IJOIN: put_binary(out, t, " \\/ ", LEVEL_JOIN, LEVEL_MEET); break;
    case TERM_SYSTEM: put_system(out, t); break;
    case TERM_BRANCH: put_binary(out, t, " -> ", LEVEL_JOIN, LEVEL_TERM); break;
    case TERM_RES:
      put_str(out, "res ");
      put_quoted(out, t->name);
      break;
  }
  if (paren) put_str(out, ")");
}

/* The groups of a parameter or field list. The fields of one group share
   one type pointer and one history flag (parser.c parse_fields); they
   print as one group only when the type names an earlier name of the
   group (H1). A state field after the history marker prints
   `history (x : A)`. A field has no mark. */
static void put_fields(FmtText *out, const Field *fields, size_t count) {
  size_t k = 0;
  while (k < count) {
    size_t n = 1;
    int keep = 0;
    while (k + n < count && fields[k + n].type == fields[k].type && fields[k + n].history == fields[k].history) n++;
    for (size_t j = 0; j + 1 < n; j++) keep |= names_free(fields[k].type, fields[k + j].name);
    size_t take = keep ? n : 1u;
    put_str(out, fields[k].history ? " history (" : " (");
    for (size_t j = 0; j < take; j++) {
      put_str(out, fields[k + j].name);
      put_str(out, " ");
    }
    put_str(out, ": ");
    put_term(out, fields[k].type, LEVEL_TERM);
    put_str(out, ")");
    k += take;
  }
}

static void put_ctor(FmtText *out, const Ctor *c) {
  put_str(out, c->name);
  put_fields(out, c->fields, c->field_count);
  if (c->result != NULL) {
    put_str(out, " : ");
    put_term(out, c->result, LEVEL_TERM);
  }
  if (c->boundary != NULL) {
    put_str(out, " ");
    put_system(out, c->boundary);
  }
}

static const char *family_head(const Decl *d) {
  if (d->is_state) return "state ";
  return d->is_sealed ? "sealed family " : "family ";
}

/* The keywords and the name. A sort after the first of its block is an
   `and` sort. A bind line has no head (put_bind). */
static void put_head(FmtText *out, const Decl *decls, size_t i) {
  const Decl *d = &decls[i];
  if (d->block_first != i) {
    put_str(out, "and ");
  } else {
    switch (d->kind) {
      case DECL_DEF:
        put_str(out, d->is_law ? "law def " : "def ");
        if (d->is_rec) put_str(out, "rec ");
        break;
      case DECL_AXIOM: put_str(out, "axiom "); break;
      case DECL_FAMILY: put_str(out, family_head(d)); break;
      case DECL_COFAMILY: put_str(out, d->is_affine ? "affine cofamily " : "cofamily "); break;
      case DECL_BIND: break;
    }
  }
  put_str(out, d->name);
}

/* BROKEN: a def body goes on line 2 at a 2-space indent; an axiom never
   breaks. */
static void put_def(FmtText *out, const Decl *decls, size_t i, int broken) {
  const Decl *d = &decls[i];
  put_head(out, decls, i);
  put_str(out, " : ");
  put_term(out, d->type, LEVEL_TERM);
  if (d->kind == DECL_AXIOM) return;
  put_str(out, broken ? " :=\n  " : " := ");
  put_term(out, d->body, LEVEL_TERM);
}

/* BROKEN: each constructor after the first goes on its own line `  | c`. */
static void put_family(FmtText *out, const Decl *decls, size_t i, int broken) {
  const Decl *d = &decls[i];
  put_head(out, decls, i);
  put_fields(out, d->params, d->param_count);
  if (d->index != NULL) {
    put_str(out, " : ");
    put_term(out, d->index, LEVEL_TERM);
  }
  put_str(out, " :=");
  for (size_t k = 0; k < d->ctor_count; k++) {
    put_str(out, k == 0 ? " " : broken ? "\n  | " : " | ");
    put_ctor(out, &d->ctors[k]);
  }
}

/* `bind "TOOL" ARM "POINTER"`: a bind line never breaks. */
static void put_bind(FmtText *out, const Decl *d) {
  put_str(out, "bind ");
  put_quoted(out, d->name);
  put_str(out, " ");
  put_str(out, d->bind_action);
  put_str(out, " ");
  put_quoted(out, d->bind_pointer);
}

static void put_decl(FmtText *out, const Decl *decls, size_t i, int broken) {
  switch (decls[i].kind) {
    case DECL_DEF:
    case DECL_AXIOM:
      put_def(out, decls, i, broken);
      break;
    case DECL_FAMILY:
    case DECL_COFAMILY:
      put_family(out, decls, i, broken);
      break;
    case DECL_BIND:
      put_bind(out, &decls[i]);
      break;
  }
}

void fmt_decl(FmtText *out, const Decl *decls, size_t i) {
  FmtText flat;
  fmt_text_init(&flat, out->arena);
  put_decl(&flat, decls, i, 0);
  out->oom |= flat.oom;
  if (flat.len <= FMT_WIDTH) {
    put_bytes(out, flat.data, flat.len);
  } else {
    put_decl(out, decls, i, 1);
  }
}

static void put_comment(FmtText *out, const Comment *c) {
  put_bytes(out, c->text, c->len);
  put_str(out, "\n");
}

/* Q-S12-2: no blank line between two bind lines that follow each other. */
void fmt_region(FmtText *out, const Decl *decls, size_t i, const TokenList *toks, size_t *next_comment) {
  int bind_run = i > 0 && decls[i].kind == DECL_BIND && decls[i - 1].kind == DECL_BIND;
  if (i > 0 && decls[i].block_first == i && !bind_run) put_str(out, "\n");
  while (*next_comment < toks->comment_count && toks->comments[*next_comment].line <= decls[i].end_line) {
    put_comment(out, &toks->comments[*next_comment]);
    (*next_comment)++;
  }
  fmt_decl(out, decls, i);
  put_str(out, "\n");
}

void fmt_tail(FmtText *out, size_t decl_count, const TokenList *toks, size_t next_comment) {
  if (decl_count > 0 && next_comment < toks->comment_count) put_str(out, "\n");
  for (size_t k = next_comment; k < toks->comment_count; k++) put_comment(out, &toks->comments[k]);
}

int fmt_program(FmtText *out, const Decl *decls, size_t count, const TokenList *toks) {
  size_t next_comment = 0;
  for (size_t i = 0; i < count; i++) fmt_region(out, decls, i, toks, &next_comment);
  fmt_tail(out, count, toks, next_comment);
  return !out->oom;
}

int fmt_validate(const FmtText *out, const char *source, Diag *diag) {
  if (out->len > SOURCE_MAX_BYTES)
    return diag_fail(diag, "FMT_SIZE", NULL, "%s: canonical text is larger than %zu bytes", source, SOURCE_MAX_BYTES);
  TokenList toks = {0};
  DeclList decls = {0};
  Diag parsed;
  diag_init(&parsed);
  const char *text = out->data == NULL ? "" : out->data;
  if (!lex_source(out->arena, "canonical text", text, out->len, &toks, &parsed)
      || !parse_source(out->arena, "canonical text", ORIGIN_PROGRAM, &toks, &decls, &parsed))
    return diag_fail(diag, parsed.code, parsed.def, "%s: cannot emit canonical text: %s", source, parsed.msg);
  return 1;
}
