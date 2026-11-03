/* The lowering from checked values to the first-order IR. SHARED by the tcc
   kits.

   Each entry is evaluated on variables for its parameters. The normal form
   is the residual program, and this file writes it as IR statements. Each
   value becomes an Operand: one word (see src/ir.h). An eliminator on a heap
   value loads the tag and the fields of the block. A fold over a list or a
   linear family walks the spine into a reversed chain, then folds the chain
   from its last node. The chain is a list: a block [1, word, rest]. */
#include "front/lower.h"

#include <string.h>

#include "front/check.h"

#define IR_PRINT_DEPTH_MAX 4000u

/* A value at run time: its word, a constant or a local. */
typedef struct {
  const IrExpr *w;
} Operand;

/* The local of a result. */
typedef struct {
  uint32_t w;
} Slot;

typedef struct Buf Buf;

/* The operand of a value lowered earlier: one lowering per Value pointer
   (flag 51). A block finds the entries of its own memo and of the blocks it
   is placed in (flag 52), so a local set in a sibling arm is never read. */
typedef struct {
  const Value *key;
  Operand op;
} Memo;

struct Buf {
  const IrStmt **items;
  size_t count;
  size_t cap;
  const Buf *parent; /* the block this one is placed in, or NULL */
  Memo *memo;        /* open addressing on the Value pointer */
  size_t memo_count;
  size_t memo_cap; /* 0 or a power of two */
};

typedef struct {
  Machine *m;
  uint32_t local_count;
  Operand *levels; /* the operand of each de Bruijn level */
  uint32_t level_count;
  uint32_t level_cap;
  uint32_t imports; /* bit K: a call of host import K (S8) */
  int has_used;        /* S8 (R4): the decide or the malformed export */
  uint32_t used_level; /* the level of the used trace of the start */
  uint32_t count;      /* the local of the used digest count */
  int has_acc;         /* in the REPEAT of fm_len_used */
  uint32_t acc_level;  /* the level of the folded part */
  uint32_t acc_len;    /* the local of its len */
} Low;

static int oom(Low *l) {
  return diag_fail(l->m->diag, "OOM", l->m->def, "out of memory");
}

static int internal(Low *l, const char *what) {
  return diag_fail(l->m->diag, "INTERNAL", l->m->def, "the lowering found %s", what);
}

/* An operation that the Wasm host does not lower (S3). */
static int refuse(Low *l, const char *what) {
  return diag_fail(l->m->diag, "REFUSE_LOWER", l->m->def, "the Wasm host does not lower %s", what);
}

static IrExpr *expr_alloc(Low *l, IrExprKind kind) {
  IrExpr *e = arena_alloc(l->m->arena, sizeof *e);
  if (e == NULL)
    oom(l);
  else
    e->kind = kind;
  return e;
}

static const IrExpr *ex_const(Low *l, uint64_t value) {
  IrExpr *e = expr_alloc(l, IR_EXPR_CONST);
  if (e != NULL)
    e->value = value;
  return e;
}

static const IrExpr *ex_local(Low *l, uint32_t local) {
  IrExpr *e = expr_alloc(l, IR_EXPR_LOCAL);
  if (e != NULL)
    e->local = local;
  return e;
}

static const IrExpr *ex_bin(Low *l, IrOp op, const IrExpr *a, const IrExpr *b) {
  IrExpr *e = a == NULL || b == NULL ? NULL : expr_alloc(l, IR_EXPR_BINARY);
  if (e == NULL)
    return NULL;
  e->op = op;
  e->left = a;
  e->right = b;
  return e;
}

/* Word WORD of the block at ADDR. */
static const IrExpr *ex_load(Low *l, const IrExpr *base, uint32_t word) {
  IrExpr *e = base == NULL ? NULL : expr_alloc(l, IR_EXPR_LOAD);
  if (e == NULL)
    return NULL;
  e->left = base;
  e->field = word;
  return e;
}

/* S8 (Q-S8-2 (a)): host import IMP of A and B (B is NULL for one
   argument). */
static const IrExpr *ex_call(Low *l, IrImport imp, const IrExpr *a, const IrExpr *b) {
  IrExpr *e = a == NULL ? NULL : expr_alloc(l, IR_EXPR_CALL);
  if (e == NULL)
    return NULL;
  e->value = imp;
  e->left = a;
  e->right = b;
  l->imports |= 1u << imp;
  return e;
}

static int is_const(const IrExpr *e, uint64_t value) {
  return e != NULL && e->kind == IR_EXPR_CONST && e->value == value;
}

static Operand operand(const IrExpr *w) {
  Operand o;
  o.w = w;
  return o;
}

static int give(Low *l, Operand *out, uint64_t w) {
  *out = operand(ex_const(l, w));
  return out->w != NULL;
}

static uint32_t local_new(Low *l) {
  return l->local_count++;
}

static Slot slot_new(Low *l) {
  Slot s;
  s.w = local_new(l);
  return s;
}

static Operand slot_get(Low *l, Slot s) {
  return operand(ex_local(l, s.w));
}

/* A new block, placed in PARENT (NULL for the body of an entry). */
static Buf buf_under(const Buf *parent) {
  Buf b;
  memset(&b, 0, sizeof b);
  b.parent = parent;
  return b;
}

/* The slot of KEY in MEMO (CAP entries): its entry, or the free slot
   where it goes. */
static size_t memo_slot(const Memo *memo, size_t cap, const Value *key) {
  size_t i = (((size_t)(uintptr_t)key) >> 4) * 2654435761u & (cap - 1u);
  while (memo[i].key != NULL && memo[i].key != key)
    i = (i + 1u) & (cap - 1u);
  return i;
}

/* 1 and OUT when V was lowered into B or into a block B is placed in. */
static int memo_find(const Buf *b, const Value *v, Operand *out) {
  size_t i;
  for (; b != NULL; b = b->parent) {
    if (b->memo_cap == 0)
      continue;
    i = memo_slot(b->memo, b->memo_cap, v);
    if (b->memo[i].key == v) {
      *out = b->memo[i].op;
      return 1;
    }
  }
  return 0;
}

static int memo_grow(Low *l, Buf *b) {
  size_t cap = b->memo_cap == 0 ? 16u : b->memo_cap * 2u;
  Memo *grown = arena_alloc(l->m->arena, cap * sizeof *grown);
  size_t i;
  if (grown == NULL)
    return oom(l);
  memset(grown, 0, cap * sizeof *grown);
  for (i = 0; i < b->memo_cap; i++)
    if (b->memo[i].key != NULL)
      grown[memo_slot(grown, cap, b->memo[i].key)] = b->memo[i];
  b->memo = grown;
  b->memo_cap = cap;
  return 1;
}

/* Records V as O in the memo of B. The table stays at most half full. */
static int memo_put(Low *l, Buf *b, const Value *v, Operand o) {
  size_t i;
  if (2u * (b->memo_count + 1u) > b->memo_cap && !memo_grow(l, b))
    return 0;
  i = memo_slot(b->memo, b->memo_cap, v);
  if (b->memo[i].key == NULL)
    b->memo_count++;
  b->memo[i].key = v;
  b->memo[i].op = o;
  return 1;
}

/* A value with a lowering worth one memo entry: an operation or a stuck
   operation (flag 54). A number, a type, a variable and a closure are
   constants, a level or a diagnostic. */
static int memo_kind(const Value *v) {
  return v->kind == VAL_OP || v->kind == VAL_STUCK;
}

static int buf_grow(Low *l, Buf *b) {
  size_t cap = b->cap == 0 ? 8u : b->cap * 2u;
  const IrStmt **grown = arena_alloc(l->m->arena, cap * sizeof *grown);
  size_t i;
  if (grown == NULL)
    return oom(l);
  for (i = 0; i < b->count; i++)
    grown[i] = b->items[i];
  b->items = grown;
  b->cap = cap;
  return 1;
}

static int emit(Low *l, Buf *b, const IrStmt *s) {
  if (s == NULL || (b->count == b->cap && !buf_grow(l, b)))
    return 0;
  b->items[b->count++] = s;
  return 1;
}

static int splice(Low *l, Buf *b, const Buf *in) {
  size_t i;
  for (i = 0; i < in->count; i++)
    if (!emit(l, b, in->items[i]))
      return 0;
  return 1;
}

static IrBlock block_of(const Buf *b) {
  IrBlock out;
  out.items = b->items;
  out.count = b->count;
  return out;
}

static IrStmt *stmt_new(Low *l, IrStmtKind kind) {
  IrStmt *s = arena_alloc(l->m->arena, sizeof *s);
  if (s == NULL)
    oom(l);
  else
    s->kind = kind;
  return s;
}

/* A statement that reads the expression E. */
static IrStmt *stmt_expr(Low *l, IrStmtKind kind, const IrExpr *e) {
  IrStmt *s = e == NULL ? NULL : stmt_new(l, kind);
  if (s != NULL)
    s->expr = e;
  return s;
}

static int set(Low *l, Buf *b, uint32_t local, const IrExpr *e) {
  IrStmt *s = stmt_expr(l, IR_STMT_SET, e);
  if (s != NULL)
    s->local = local;
  return emit(l, b, s);
}

/* DST := SRC. */
static int store(Low *l, Buf *b, Slot dst, Operand src) {
  return src.w != NULL && set(l, b, dst.w, src.w);
}

/* E as a constant or a local. */
static const IrExpr *atom(Low *l, Buf *b, const IrExpr *e) {
  uint32_t t;
  if (e == NULL || e->kind == IR_EXPR_CONST || e->kind == IR_EXPR_LOCAL)
    return e;
  t = local_new(l);
  return set(l, b, t, e) ? ex_local(l, t) : NULL;
}

static int st_if(Low *l, Buf *b, const IrExpr *c, const Buf *yes, const Buf *no) {
  IrStmt *s = stmt_expr(l, IR_STMT_IF, c);
  if (s == NULL)
    return 0;
  s->body = block_of(yes);
  s->otherwise = block_of(no);
  return emit(l, b, s);
}

/* IF F {YES} ELSE {NO}. A constant F keeps one block. */
static int if_flag(Low *l, Buf *b, const IrExpr *f, const Buf *yes, const Buf *no) {
  if (f == NULL)
    return 0;
  if (is_const(f, 0) || is_const(f, 1))
    return splice(l, b, is_const(f, 1) ? yes : no);
  return st_if(l, b, f, yes, no);
}

static int st_switch(Low *l, Buf *b, const IrExpr *e, const IrBlock *arms, size_t n) {
  IrStmt *s = stmt_expr(l, IR_STMT_SWITCH, e);
  if (s == NULL)
    return 0;
  s->arms = arms;
  s->arm_count = n;
  return emit(l, b, s);
}

/* REPEAT COUNT {BODY}, with a new counter local. */
static int st_repeat(Low *l, Buf *b, const IrExpr *count, const Buf *body) {
  IrStmt *s = stmt_expr(l, IR_STMT_REPEAT, count);
  if (s == NULL)
    return 0;
  s->counter = local_new(l);
  s->body = block_of(body);
  return emit(l, b, s);
}

/* WHILE COUNT, LOCAL {BODY}, with a new counter local. */
static int st_while(Low *l, Buf *b, const IrExpr *count, uint32_t local, const Buf *body) {
  IrStmt *s = stmt_expr(l, IR_STMT_WHILE, count);
  if (s == NULL)
    return 0;
  s->counter = local_new(l);
  s->local = local;
  s->body = block_of(body);
  return emit(l, b, s);
}

/* A new block [TAG, the word of each of the N operands]. */
static int block_ops(Low *l, Buf *b, uint64_t tag, const Operand *ops, uint32_t n, Operand *out) {
  const IrExpr **fields = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *fields);
  IrStmt *s = stmt_new(l, IR_STMT_ALLOC);
  uint32_t i;
  if (fields == NULL || s == NULL)
    return fields == NULL ? oom(l) : 0;
  fields[0] = ex_const(l, tag);
  for (i = 0; i < n; i++)
    fields[1u + i] = ops[i].w;
  s->local = local_new(l);
  s->fields = fields;
  s->field_count = (size_t)n + 1u;
  *out = operand(ex_local(l, s->local));
  return fields[0] != NULL && emit(l, b, s);
}

/* The operand of field K of the block at ADDR. */
static Operand field_op(Low *l, const IrExpr *base, uint32_t k) {
  return operand(ex_load(l, base, 1u + k));
}

/* Field K of the block at ADDR, copied into a new local. */
static int field_at(Low *l, Buf *b, const IrExpr *base, uint32_t k, Operand *out) {
  Slot s = slot_new(l);
  *out = slot_get(l, s);
  return store(l, b, s, field_op(l, base, k));
}

static int levels_grow(Low *l) {
  uint32_t cap = l->level_cap == 0 ? 64u : l->level_cap * 2u;
  Operand *grown = arena_alloc(l->m->arena, (size_t)cap * sizeof *grown);
  uint32_t i;
  if (grown == NULL)
    return oom(l);
  for (i = 0; i < l->level_count; i++)
    grown[i] = l->levels[i];
  l->levels = grown;
  l->level_cap = cap;
  return 1;
}

/* A new variable that stands for the operand O. Levels never repeat. */
static const Value *fresh(Low *l, Operand o) {
  if (o.w == NULL || (l->level_count == l->level_cap && !levels_grow(l)))
    return NULL;
  l->levels[l->level_count] = o;
  return val_var(l->m, l->level_count++);
}

static const char *op_word(IrOp op) {
  switch (op) {
  case IR_OP_ADD:
    return "add";
  case IR_OP_SUB:
    return "sub";
  case IR_OP_MUL:
    return "mul";
  case IR_OP_EQ:
    return "eq";
  case IR_OP_LE:
    return "le";
  case IR_OP_ADD_CARRY:
    return "add_carry";
  case IR_OP_DIV:
    return "div";
  case IR_OP_MOD:
    return "mod";
  }
  return "?";
}

#define IR_IMPORT_ROW(name, text, params) {text, params},
static const struct {
  const char *text;
  uint32_t params;
} import_rows[] = {IR_IMPORTS(IR_IMPORT_ROW)};
#undef IR_IMPORT_ROW

static void expr_print(const IrExpr *e, FILE *out, unsigned depth) {
  if (depth > IR_PRINT_DEPTH_MAX) {
    fputs("...", out);
    return;
  }
  switch (e->kind) {
  case IR_EXPR_CONST:
    fprintf(out, "%llu", (unsigned long long)e->value);
    return;
  case IR_EXPR_LOCAL:
    fprintf(out, "l%u", (unsigned)e->local);
    return;
  case IR_EXPR_BINARY:
    fprintf(out, "%s(", op_word(e->op));
    expr_print(e->left, out, depth + 1u);
    fputs(", ", out);
    expr_print(e->right, out, depth + 1u);
    fputs(")", out);
    return;
  case IR_EXPR_LOAD:
    fputs("load(", out);
    expr_print(e->left, out, depth + 1u);
    fprintf(out, ", %u)", (unsigned)e->field);
    return;
  case IR_EXPR_CALL:
    fprintf(out, "call permit.%s(", import_rows[e->value].text);
    expr_print(e->left, out, depth + 1u);
    if (e->right != NULL) {
      fputs(", ", out);
      expr_print(e->right, out, depth + 1u);
    }
    fputs(")", out);
    return;
  }
}

static void block_print(const IrBlock *b, FILE *out, unsigned depth);

static void indent(FILE *out, unsigned depth) {
  fprintf(out, "%*s", (int)(2u * depth), "");
}

static void close_print(FILE *out, unsigned depth) {
  indent(out, depth);
  fputs("}\n", out);
}

static void stmt_print(const IrStmt *s, FILE *out, unsigned depth) {
  size_t i;
  indent(out, depth);
  switch (s->kind) {
  case IR_STMT_SET:
    fprintf(out, "l%u := ", (unsigned)s->local);
    expr_print(s->expr, out, 0);
    fputs("\n", out);
    return;
  case IR_STMT_ALLOC:
    fprintf(out, "l%u := alloc [", (unsigned)s->local);
    for (i = 0; i < s->field_count; i++) {
      fputs(i == 0 ? "" : ", ", out);
      expr_print(s->fields[i], out, 0);
    }
    fputs("]\n", out);
    return;
  case IR_STMT_IF:
    fputs("if ", out);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    block_print(&s->body, out, depth + 1u);
    indent(out, depth);
    fputs("} else {\n", out);
    block_print(&s->otherwise, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_SWITCH:
    fputs("switch ", out);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    for (i = 0; i < s->arm_count; i++) {
      indent(out, depth);
      fprintf(out, "arm %zu:\n", i);
      block_print(&s->arms[i], out, depth + 1u);
    }
    close_print(out, depth);
    return;
  case IR_STMT_REPEAT:
    fprintf(out, "repeat l%u := ", (unsigned)s->counter);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    block_print(&s->body, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_WHILE:
    fprintf(out, "while l%u := ", (unsigned)s->counter);
    expr_print(s->expr, out, 0);
    fprintf(out, ", l%u {\n", (unsigned)s->local);
    block_print(&s->body, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_STORE:
    fputs("store(", out);
    expr_print(s->expr, out, 0);
    fprintf(out, ", %u) := ", (unsigned)s->field);
    expr_print(s->fields[0], out, 0);
    fputs("\n", out);
    return;
  case IR_STMT_RETURN:
    fputs("return ", out);
    expr_print(s->expr, out, 0);
    fputs("\n", out);
    return;
  }
}

static void block_print(const IrBlock *b, FILE *out, unsigned depth) {
  size_t i;
  if (depth > IR_PRINT_DEPTH_MAX) {
    fputs("...\n", out);
    return;
  }
  for (i = 0; i < b->count; i++)
    stmt_print(b->items[i], out, depth);
}

static const char *scalar_word(IrScalar s) {
  switch (s) {
  case IR_SCALAR_NAT:
    return "nat";
  case IR_SCALAR_FLAG:
    return "flag";
  }
  return "?";
}

void ir_print(const IrProgram *prog, FILE *out) {
  size_t i;
  size_t k;
  for (i = 0; i < prog->func_count; i++) {
    const IrFunc *fn = &prog->funcs[i];
    fprintf(out, "func %s (", fn->name);
    for (k = 0; k < fn->param_count; k++)
      fprintf(out, "%s%s", k == 0 ? "" : ", ", scalar_word(fn->params[k]));
    fprintf(out, ") -> %s, %u locals, %u heap words\n", scalar_word(fn->result), (unsigned)fn->local_count,
            (unsigned)fn->heap_words);
    block_print(&fn->body, out, 1u);
  }
  for (i = 0; i < prog->res_count; i++)
    fprintf(out, "res %u \"%s\"\n", (unsigned)i, prog->res_paths[i]);
  for (i = 0; i < prog->bind_count; i++)
    fprintf(out, "bind \"%s\" action %u \"%s\"\n", prog->binds[i].tool, (unsigned)prog->binds[i].action,
            prog->binds[i].pointer);
}

static int lower(Low *l, Buf *b, const Value *v, Operand *out);

static int closure(Low *l) {
  return diag_fail(l->m->diag, "REFUSE_RUNTIME_CLOSURE", l->m->def, "a function value must exist at run time (HOST-LIMIT)");
}

static int nat_bin(Low *l, Buf *b, const Value *v, IrOp op, Operand *out);

/* S8 (Q-S8-2 (a)): within, hash and envelope call the host imports
   permit.within, permit.hash and permit.envelope. With too few arguments
   the operation is a function. */
static int host_call(Low *l, Buf *b, const Value *v, IrImport imp, Operand *out) {
  Operand x;
  Operand y = operand(NULL);
  if (v->argc < import_rows[imp].params)
    return closure(l);
  if (!lower(l, b, v->args[0], &x) || (v->argc > 1u && !lower(l, b, v->args[1], &y)))
    return 0;
  *out = operand(atom(l, b, ex_call(l, imp, x.w, y.w)));
  return out->w != NULL;
}

/* S8 (Q-S8-3 (a)): 1 when the closed chain C spells T[0..END): each
   segment number names the segment text at its place, from the last
   segment back to the root. Each step uses one segment and a '/', so the
   depth is at most END. */
static int res_spells(const Machine *m, const Value *c, const char *t, size_t end) {
  int child = val_is(c, OP_RES_CHILD) && c->argc == 2u;
  const Value *seg = child ? c->args[1] : val_is(c, OP_RES_ROOT) && c->argc == 1u ? c->args[0] : NULL;
  size_t len = seg != NULL && seg->kind == VAL_NAT && seg->nat < m->res_seg_count ? m->res_segs[seg->nat].len : end + 1u;
  if (len > end || memcmp(t + end - len, m->res_segs[seg->nat].text, len) != 0)
    return 0;
  return child ? end > len && t[end - len - 1u] == '/' && res_spells(m, c->args[0], t, end - len - 1u) : end == len;
}

/* S8 (Q-S8-3 (a)): a closed chain is the index of its path literal in
   m->res_paths, the handle of the host (custom section permit.res). The
   literal "a/b" does not enter "a". A chain with no literal is refused. */
static int res_handle(Low *l, const Value *v, Operand *out) {
  uint32_t i;
  for (i = 0; i < l->m->res_path_count; i++)
    if (res_spells(l->m, v, l->m->res_paths[i], strlen(l->m->res_paths[i])))
      return give(l, out, i);
  return refuse(l, "a Resource with no path literal (S8)");
}

/* 1 when the evaluator gave a value, else 0 after a diagnostic. */
static int got(Low *l, const Value *v) {
  return v != NULL ? 1 : (l->m->diag->set ? 0 : internal(l, "a missing value"));
}

/* RES := the lowering of V. */
static int lower_into(Low *l, Buf *b, const Value *v, Slot res) {
  Operand o;
  return lower(l, b, v, &o) && store(l, b, res, o);
}

/* RES := FN applied to N new variables, one for each operand of ARGS. */
static int apply_into(Low *l, Buf *b, const Value *fn, const Operand *args, uint32_t n, Slot res) {
  const Value *v = fn;
  const Value *x;
  uint32_t i;
  for (i = 0; i < n && v != NULL; i++) {
    x = fresh(l, args[i]);
    v = x == NULL ? NULL : apply_value(l->m, v, x);
  }
  return got(l, v) && lower_into(l, b, v, res);
}

/* A new block [TAG, the lowering of each of the N values of ARGS]. */
static int ctor_block(Low *l, Buf *b, uint64_t tag, const Value *const *args, uint32_t n, Operand *out) {
  Operand *ops = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *ops);
  uint32_t i;
  if (ops == NULL)
    return oom(l);
  for (i = 0; i < n; i++)
    if (!lower(l, b, args[i], &ops[i]))
      return 0;
  return block_ops(l, b, tag, ops, n, out);
}

/* A constructor with fewer arguments than its arity is a function. */
static int ctor_full(Low *l, Buf *b, const Value *v, uint32_t skip, uint32_t n, uint64_t tag, Operand *out) {
  return v->argc < skip + n ? closure(l) : ctor_block(l, b, tag, v->args + skip, n, out);
}

/* S8 (Q-S8-9 (a)): the domain family Size is a Nat word at run time, sz0
   = 0 and szS x = x + 1, so a fold or an elim over it is a REPEAT. */
static int is_size(const Low *l, uint32_t family) {
  const FamilyInfo *fam = &l->m->families[family];
  return strcmp(fam->name, "Size") == 0 && fam->ctor_count == 2u && fam->param_count == 0u;
}

/* OUT := X + Y. */
static int word_add(Low *l, Buf *b, Operand x, Operand y, Operand *out) {
  *out = operand(atom(l, b, ex_bin(l, IR_OP_ADD, x.w, y.w)));
  return out->w != NULL;
}

/* sz0 is the word 0 and szS X is X + 1 (TAG is the constructor index). */
static int size_ctor(Low *l, Buf *b, const Value *v, uint32_t tag, Operand *out) {
  Operand x;
  if (tag == 0u)
    return give(l, out, 0);
  if (v->argc < 1u)
    return closure(l);
  return lower(l, b, v->args[0], &x) && word_add(l, b, x, operand(ex_const(l, 1)), out);
}

/* S8 (R5): 1 when argument K of V is the used trace of the start. */
static int is_used(const Low *l, const Value *v, uint32_t k) {
  return l->has_used && v->args[k]->kind == VAL_VAR && v->args[k]->nat == l->used_level;
}

/* S8 (R5): the used trace is the word 0 at run time and the epilogue of
   decide reads one new digest, so a constructor takes the trace only as
   fmMul (fmEta x) used (fmCons d used). C is the constructor of V, P the
   param count of its family. */
static int used_ok(const Low *l, const Value *v, const CtorInfo *c, uint32_t p) {
  const Value *eta = v->argc == p + 2u ? v->args[p] : NULL;
  int any = 0;
  uint32_t k;
  for (k = 0; k < v->argc; k++)
    any = any || is_used(l, v, k);
  return !any || strcmp(l->m->families[c->family].name, "FreeMon") != 0 || (strcmp(c->name, "fmMul") == 0 && eta != NULL && is_used(l, v, p + 1u) && eta->kind == VAL_OP &&
                  eta->op == OP_CTOR && strcmp(l->m->ctors[eta->inst].name, "fmEta") == 0);
}

static int lower_ctor(Low *l, Buf *b, const Value *v, Operand *out) {
  const CtorInfo *c = &l->m->ctors[v->inst];
  const FamilyInfo *fam = &l->m->families[c->family];
  if (!used_ok(l, v, c, fam->param_count))
    return refuse(l, "a used trace other than fmCons d used (S8)");
  return is_size(l, c->family) ? size_ctor(l, b, v, v->inst - fam->first_ctor, out)
                               : ctor_full(l, b, v, fam->param_count, c->field_count, v->inst - fam->first_ctor, out);
}

/* A canonical value. An eliminator here has too few arguments, so it is a
   function. */
static int lower_canon(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->op) {
  case OP_NAT:
  case OP_FLAG:
  case OP_BUDGET:
  case OP_APPROVAL:
  case OP_PAYLOAD_T:
  case OP_BYTES:
  case OP_DIGEST:
  case OP_RESOURCE:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_NO:
  case OP_INTERVAL:
  case OP_SYSTEM:
    return give(l, out, 0);
  case OP_TRANSP:
  case OP_HCOMP:
    return internal(l, "a stuck cubical operation");
  case OP_FLAG_YES:
    return give(l, out, 1);
  case OP_PAIR:
  case OP_PACK:
    return ctor_full(l, b, v, 0, 2, 0, out);
  case OP_INL:
    return ctor_full(l, b, v, 0, 1, 0, out);
  case OP_INR:
  case OP_SOME:
    return ctor_full(l, b, v, 0, 1, 1, out);
  case OP_NONE:
  case OP_NIL:
    return ctor_full(l, b, v, 0, 0, 0, out);
  case OP_CONS:
    return ctor_full(l, b, v, 0, 2, 1, out);
  case OP_CTOR:
    return lower_ctor(l, b, v, out);
  case OP_FIRST:
  case OP_SECOND:
  case OP_EITHER:
  case OP_OPTION_ELIM:
  case OP_PURE:
  case OP_MAP:
  case OP_BIND:
  case OP_FOLD_NAT:
  case OP_FOLD_LIST:
  case OP_FOLD_FAMILY:
  case OP_UNFOLD:
  case OP_FILTER:
  case OP_WITNESS:
  case OP_PAYLOAD:
  case OP_NAT_ADD:
  case OP_NAT_SUB:
  case OP_NAT_EQ:
  case OP_NAT_LE:
  case OP_FLAG_IF:
  case OP_PROJ:
  case OP_NAT_DIV:
  case OP_NAT_MOD:
  case OP_NONZERO_OF:
  case OP_NONZERO_VALUE:
    return closure(l);
  case OP_NONZERO:
    return give(l, out, 0);
  case OP_RES_ROOT:
    return v->argc < 1u ? closure(l) : res_handle(l, v, out);
  case OP_RES_CHILD:
    return v->argc < 2u ? closure(l) : res_handle(l, v, out);
  /* S8 (Q-S8-3 (a)): the host interns the paths, so resEq is i64.eq. */
  case OP_RES_EQ:
    return v->argc < 2u ? closure(l) : nat_bin(l, b, v, IR_OP_EQ, out);
  case OP_RES_WITHIN:
    return host_call(l, b, v, IR_IMPORT_WITHIN, out);
  case OP_BUDGET_MK:
  case OP_BUDGET_LEFT:
  case OP_SPEND:
  case OP_SPLIT:
    return refuse(l, "a Budget at run time (S8)");
  /* S8 (Q-S8-2 (a)): a Payload is a host handle, so payloadOf n is the
     word n. */
  case OP_PAYLOAD_OF:
    return v->argc < 1u ? closure(l) : lower(l, b, v->args[0], out);
  case OP_PAYLOAD_EQ:
  case OP_DIGEST_EQ:
    return closure(l);
  case OP_HASH:
    return host_call(l, b, v, IR_IMPORT_HASH, out);
  case OP_ENVELOPE:
    return host_call(l, b, v, IR_IMPORT_ENVELOPE, out);
  case OP_APPROVAL_MK:
  case OP_APPROVAL_DIGEST:
    return refuse(l, "an Approval at run time (S8)");
  case OP_FM_LEN:
    return refuse(l, "len (the op count of a FreeMon trace at run time)");
  case OP_ELIM:
  case OP_KMAP:
  case OP_KMAP_OF:
  case OP_KMAP_GET:
  case OP_KMAP_SET:
  case OP_FIN:
  case OP_FIN_ADD:
  case OP_FIN_NEG:
  case OP_FIN_CASE:
  case OP_GLUE:
  case OP_GLUE_ELEM:
  case OP_UNGLUE:
    return refuse(l, "an elim, KMap, Fin or Glue operation");
  }
  return internal(l, "an unknown operation");
}

/* A Nat operation on the two arguments of V. Each op is total. */
static int nat_bin(Low *l, Buf *b, const Value *v, IrOp op, Operand *out) {
  Operand x;
  Operand y;
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  *out = operand(atom(l, b, ex_bin(l, op, x.w, y.w)));
  return out->w != NULL;
}

/* natAdd X Y: an Option block. ADD_CARRY selects the tag: none when the sum
   is larger than 2^64 - 1, else some (X + Y). */
static int nat_add(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand y;
  Operand sum;
  Operand some;
  Operand none;
  Slot res = slot_new(l);
  Buf carry = buf_under(b);
  Buf fits = buf_under(b);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  sum = operand(ex_bin(l, IR_OP_ADD, x.w, y.w));
  return sum.w != NULL && block_ops(l, &carry, 0, NULL, 0, &none) && store(l, &carry, res, none) &&
         block_ops(l, &fits, 1, &sum, 1, &some) && store(l, &fits, res, some) &&
         if_flag(l, b, ex_bin(l, IR_OP_ADD_CARRY, x.w, y.w), &carry, &fits);
}

/* nonZero X: none when X is 0, else some X. */
static int nonzero_of(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand some;
  Operand none;
  Slot res = slot_new(l);
  Buf zero = buf_under(b);
  Buf other = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &x) && block_ops(l, &zero, 0, NULL, 0, &none) && store(l, &zero, res, none) &&
         block_ops(l, &other, 1, &x, 1, &some) && store(l, &other, res, some) &&
         if_flag(l, b, ex_bin(l, IR_OP_EQ, x.w, ex_const(l, 0)), &zero, &other);
}

/* Field K of the block X. */
static int field_of(Low *l, Buf *b, const Value *xv, uint32_t k, Operand *out) {
  Operand x;
  Slot res = slot_new(l);
  *out = slot_get(l, res);
  return lower(l, b, xv, &x) && store(l, b, res, field_op(l, x.w, k));
}

static int flag_if(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand c;
  Slot res = slot_new(l);
  Buf yes = buf_under(b);
  Buf no = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &c) && lower_into(l, &yes, v->args[1], res) && lower_into(l, &no, v->args[2], res) &&
         if_flag(l, b, c.w, &yes, &no);
}

/* SWITCH on the tag of X, N arms. Arm I applies CASES[I] to the FIELDS[I]
   fields of the block (with 0 fields, the arm lowers CASES[I]). */
static int cases_of(Low *l, Buf *b, const Value *xv, const Value *const *cases, const uint32_t *fields, uint32_t n, Operand *out) {
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *arms);
  Operand *args;
  Operand x;
  Slot res = slot_new(l);
  Buf arm;
  uint32_t i;
  uint32_t k;
  *out = slot_get(l, res);
  if (arms == NULL)
    return oom(l);
  if (!lower(l, b, xv, &x))
    return 0;
  for (i = 0; i < n; i++) {
    arm = buf_under(b);
    args = arena_alloc(l->m->arena, ((size_t)fields[i] + 1u) * sizeof *args);
    if (args == NULL)
      return oom(l);
    for (k = 0; k < fields[i]; k++)
      if (!field_at(l, &arm, x.w, k, &args[k]))
        return 0;
    if (!apply_into(l, &arm, cases[i], args, fields[i], res))
      return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, x.w, 0), arms, n);
}

static int two_cases(Low *l, Buf *b, const Value *v, uint32_t none_fields, Operand *out) {
  uint32_t fields[2];
  fields[0] = none_fields;
  fields[1] = 1;
  return cases_of(l, b, v->args[2], v->args, fields, 2, out);
}

/* The fuel of a walk over a heap spine. */
#define SPINE_FUEL ((uint64_t)1 << 63)

/* A constructor without a recursive field. */
#define NO_REC UINT32_MAX

/* The recursive field of each list tag: nil has none, cons has the tail. */
static const uint32_t LIST_REC[2] = {NO_REC, 1u};

/* The locals of a walk: the reversed chain, its length, the node where the
   walk stopped, and the WHILE condition. */
typedef struct {
  uint32_t rev;
  uint32_t count;
  uint32_t cur;
  uint32_t cont;
} Walk;

static Walk walk_new(Low *l) {
  Walk w;
  w.rev = local_new(l);
  w.count = local_new(l);
  w.cur = local_new(l);
  w.cont = local_new(l);
  return w;
}

/* REV := a new chain cell [1, the word of OP, REV], and COUNT increases by
   1. */
static int chain_push(Low *l, Buf *b, const Walk *w, Operand op) {
  Operand ops[2];
  Operand c;
  ops[0] = op;
  ops[1] = operand(ex_local(l, w->rev));
  return block_ops(l, b, 1, ops, 2u, &c) && set(l, b, w->rev, c.w) &&
         set(l, b, w->count, ex_bin(l, IR_OP_ADD, ex_local(l, w->count), ex_const(l, 1)));
}

/* A walk step at a node whose recursive field is K: push the node, then go
   to field K. */
static int walk_step(Low *l, Buf *b, const Walk *w, uint32_t k) {
  const IrExpr *cur = ex_local(l, w->cur);
  return chain_push(l, b, w, operand(cur)) && set(l, b, w->cur, field_op(l, cur, k).w);
}

/* The spine walk of X. A node with tag I has the recursive field REC[I] (or
   NO_REC). A node with a recursive field goes on the chain. The walk stops
   at a node without one (CUR). The cell of the last node comes first. */
static int walk(Low *l, Buf *b, Operand x, const uint32_t *rec, uint32_t n, Walk *w) {
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *arms);
  Buf body = buf_under(b);
  Buf arm;
  uint32_t i;
  *w = walk_new(l);
  if (arms == NULL)
    return oom(l);
  for (i = 0; i < n; i++) {
    arm = buf_under(&body);
    if (!(rec[i] == NO_REC ? set(l, &arm, w->cont, ex_const(l, 0)) : walk_step(l, &arm, w, rec[i])))
      return 0;
    arms[i] = block_of(&arm);
  }
  return set(l, b, w->rev, ex_const(l, 0)) && set(l, b, w->count, ex_const(l, 0)) && set(l, b, w->cur, x.w) &&
         set(l, b, w->cont, ex_const(l, 1)) &&
         st_switch(l, &body, ex_load(l, ex_local(l, w->cur), 0), arms, n) &&
         st_while(l, b, ex_const(l, SPINE_FUEL), w->cont, &body);
}

/* The start of a pass over the chain of W: NODE := field 0 of the cell. */
static int chain_node(Low *l, Buf *body, const Walk *w, const IrExpr **node) {
  uint32_t t = local_new(l);
  *node = ex_local(l, t);
  return set(l, body, t, field_op(l, ex_local(l, w->rev), 0).w);
}

/* The start of a pass over a chain of list nodes: H := the head. */
static int chain_head(Low *l, Buf *body, const Walk *w, Operand *h) {
  const IrExpr *node;
  return chain_node(l, body, w, &node) && field_at(l, body, node, 0, h);
}

/* The end of a pass: REV := the next cell, and REPEAT COUNT {BODY}. */
static int chain_end(Low *l, Buf *b, const Walk *w, Buf *body) {
  return set(l, body, w->rev, field_op(l, ex_local(l, w->rev), 1).w) && st_repeat(l, b, ex_local(l, w->count), body);
}

/* OUT := nil. */
static int tail_init(Low *l, Buf *b, Slot out) {
  Operand nil;
  return block_ops(l, b, 0, NULL, 0, &nil) && store(l, b, out, nil);
}

/* OUT := a new cons cell [1, H, OUT]. */
static int prepend(Low *l, Buf *b, Operand h, Slot out) {
  Operand ops[2];
  Operand c;
  ops[0] = h;
  ops[1] = slot_get(l, out);
  return block_ops(l, b, 1, ops, 2u, &c) && store(l, b, out, c);
}

/* A right fold over a list. F takes the head, then the acc. */
static int fold_list(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand args[2];
  Walk w;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(b);
  *out = slot_get(l, acc);
  args[1] = slot_get(l, acc);
  return lower(l, b, v->args[2], &x) && walk(l, b, x, LIST_REC, 2u, &w) && lower_into(l, &in, v->args[1], acc) &&
         splice(l, b, &in) && chain_head(l, &body, &w, &args[0]) &&
         apply_into(l, &body, v->args[0], args, 2u, acc) && chain_end(l, b, &w, &body);
}

/* MAP on a list. */
static int list_map(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Walk w;
  Slot res = slot_new(l);
  Slot ys = slot_new(l);
  Buf body = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[0], &h, 1u, ys) &&
         prepend(l, &body, slot_get(l, ys), res) && chain_end(l, b, &w, &body);
}

/* BIND on a list. The pass starts at the last node and puts the items of
   each result of F in front. */
static int list_bind(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Operand y;
  Walk w;
  Walk u;
  Slot res = slot_new(l);
  Slot rs = slot_new(l);
  Buf body = buf_under(b);
  Buf inner = buf_under(&body);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[1], &h, 1u, rs) &&
         walk(l, &body, slot_get(l, rs), LIST_REC, 2u, &u) &&
         chain_head(l, &inner, &u, &y) && prepend(l, &inner, y, res) && chain_end(l, &body, &u, &inner) &&
         chain_end(l, b, &w, &body);
}

/* FILTER on a list. The pass starts at the last node and keeps each item
   whose test is 1. */
static int list_filter(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Walk w;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf body = buf_under(b);
  Buf keep = buf_under(&body);
  Buf drop = buf_under(&body);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[0], &h, 1u, ts) && prepend(l, &keep, h, res) &&
         if_flag(l, &body, ex_local(l, ts.w), &keep, &drop) &&
         chain_end(l, b, &w, &body);
}

/* Push the item of the pair at P, then SEED := the next seed. */
static int unfold_push(Low *l, Buf *b, const Walk *w, const IrExpr *p, Slot seed) {
  Operand a;
  Operand s;
  return field_at(l, b, p, 0, &a) && field_at(l, b, p, 1, &s) && chain_push(l, b, w, a) && store(l, b, seed, s);
}

/* The WHILE body of an unfold: R := G SEED. None stops; some(pair) pushes
   the item. */
static int unfold_step(Low *l, Buf *b, const Walk *w, const Value *g, Slot seed) {
  Operand s = slot_get(l, seed);
  Operand p;
  Slot rs = slot_new(l);
  Operand r = slot_get(l, rs);
  Buf some = buf_under(b);
  Buf none = buf_under(b);
  return apply_into(l, b, g, &s, 1u, rs) && field_at(l, &some, r.w, 0, &p) && unfold_push(l, &some, w, p.w, seed) &&
         set(l, &none, w->cont, ex_const(l, 0)) && if_flag(l, b, ex_load(l, r.w, 0), &some, &none);
}

/* UNFOLD G LIMIT SEED. The WHILE pushes the items on a chain, then a pass
   from the last item builds the list. */
static int unfold(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand lim;
  Operand h;
  Walk w = walk_new(l);
  Slot res = slot_new(l);
  Slot seed = slot_new(l);
  Buf in = buf_under(b);
  Buf step = buf_under(&in);
  Buf body = buf_under(&in);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &lim) && set(l, &in, w.rev, ex_const(l, 0)) && set(l, &in, w.count, ex_const(l, 0)) &&
         set(l, &in, w.cont, ex_const(l, 1)) && lower_into(l, &in, v->args[2], seed) &&
         unfold_step(l, &step, &w, v->args[0], seed) && st_while(l, &in, lim.w, w.cont, &step) && tail_init(l, &in, res) &&
         field_at(l, &body, ex_local(l, w.rev), 0, &h) && prepend(l, &body, h, res) && chain_end(l, &in, &w, &body) &&
         splice(l, b, &in);
}

/* A fold step at the node ADDR of the constructor C: ACC := FN applied to
   the fields, with ACC for the recursive field R. */
static int fam_case(Low *l, Buf *b, const IrExpr *base, const Value *fn, const CtorInfo *c, uint32_t r, Slot acc) {
  Operand *args = arena_alloc(l->m->arena, ((size_t)c->field_count + 1u) * sizeof *args);
  uint32_t k;
  if (args == NULL)
    return oom(l);
  for (k = 0; k < c->field_count; k++)
    if (k != r && !field_at(l, b, base, k, &args[k]))
      return 0;
  if (r < c->field_count)
    args[r] = slot_get(l, acc);
  return apply_into(l, b, fn, args, c->field_count, acc);
}

/* SWITCH on the tag of the node at ADDR into ACC. When REC_ARMS is 1, the
   arms of the constructors with a recursive field apply their case (else
   the arms of the ones without). The other arms are not reached; they are
   empty. */
static int fam_switch(Low *l, Buf *b, const IrExpr *base, const Value *v, const uint32_t *rec, int rec_arms, Slot acc) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *arms);
  Buf arm;
  uint32_t i;
  if (arms == NULL)
    return oom(l);
  for (i = 0; i < fam->ctor_count; i++) {
    arm = buf_under(b);
    if ((rec[i] != NO_REC) == rec_arms && !fam_case(l, &arm, base, v->args[i], &l->m->ctors[fam->first_ctor + i], rec[i], acc))
      return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, base, 0), arms, fam->ctor_count);
}

/* A fold over a linear family: the walk, the base node into ACC, then a
   pass from the last recursive node with ACC for its recursive field. */
static int family_spine(Low *l, Buf *b, const Value *v, const uint32_t *rec, Operand *out) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  const IrExpr *node;
  Operand x;
  Walk w;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(b);
  *out = slot_get(l, acc);
  return lower(l, b, v->args[v->argc - 1u], &x) && walk(l, b, x, rec, fam->ctor_count, &w) &&
         fam_switch(l, &in, ex_local(l, w.cur), v, rec, 0, acc) && splice(l, b, &in) &&
         chain_node(l, &body, &w, &node) && fam_switch(l, &body, node, v, rec, 1, acc) && chain_end(l, b, &w, &body);
}

/* ACC := Z, then ACC := STEP ACC, N times. */
static int repeat_fold(Low *l, Buf *b, const Value *nv, const Value *z, const Value *step, Operand *out) {
  Operand n;
  Operand a;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(&in);
  *out = slot_get(l, acc);
  a = slot_get(l, acc);
  return lower(l, b, nv, &n) && lower_into(l, &in, z, acc) && apply_into(l, &body, step, &a, 1, acc) &&
         st_repeat(l, &in, n.w, &body) && splice(l, b, &in);
}

/* elim P Z S N over Size (S8, Q-S8-9 (a)): ACC := Z and I := 0, then N
   times ACC := S I ACC and I := I + 1. */
static int size_elim(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand n;
  Operand args[2];
  Slot acc = slot_new(l);
  Slot i = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(&in);
  *out = slot_get(l, acc);
  args[0] = slot_get(l, i);
  args[1] = slot_get(l, acc);
  return lower(l, b, v->args[v->argc - 1u], &n) && lower_into(l, &in, v->args[1], acc) && set(l, &in, i.w, ex_const(l, 0)) &&
         apply_into(l, &body, v->args[2], args, 2, acc) &&
         set(l, &body, i.w, ex_bin(l, IR_OP_ADD, ex_local(l, i.w), ex_const(l, 1))) && st_repeat(l, &in, n.w, &body) &&
         splice(l, b, &in);
}

/* 1 when the constructor C has no recursive field, no path field and no
   interval; *N := its field count. */
static int plain_ctor(const CtorInfo *c, uint32_t *n) {
  uint32_t k;
  for (k = 0; k < c->field_count; k++)
    if (c->fields[k].recursive || c->fields[k].over_recursive || c->fields[k].path)
      return 0;
  *n = c->field_count;
  return c->interval_count == 0u;
}

/* elim P C1 .. Cn X (S8, Q-S8-9 (a)). The field of V is the family + 1 (0
   at Sum and Unit); the cases start at args[1] and X is last. Over Size it
   is size_elim; over a family of plain constructors it is a SWITCH on the
   tag (cases_of). Any other elim refuses. */
static int lower_elim(Low *l, Buf *b, const Value *v, Operand *out) {
  const FamilyInfo *fam = v->field != 0u ? &l->m->families[v->field - 1u] : NULL;
  uint32_t *fields = fam != NULL ? arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *fields) : NULL;
  uint32_t i;
  if (fam != NULL && is_size(l, v->field - 1u))
    return size_elim(l, b, v, out);
  if (fam != NULL && fields == NULL)
    return oom(l);
  for (i = 0; fam != NULL && i < fam->ctor_count; i++)
    if (!plain_ctor(&l->m->ctors[fam->first_ctor + i], &fields[i]))
      fam = NULL;
  return fam != NULL && v->argc == fam->ctor_count + 2u
             ? cases_of(l, b, v->args[v->argc - 1u], v->args + 1, fields, fam->ctor_count, out)
             : refuse(l, "an elim, KMap, Fin or Glue operation");
}

static int fm_len(Low *l, Buf *b, const Value *x, Operand *out);

/* len (flagIf F T E) is IF F {len T} ELSE {len E}. */
static int fm_len_if(Low *l, Buf *b, const Value *x, Operand *out) {
  Operand f;
  Operand n;
  Slot res = slot_new(l);
  Buf yes = buf_under(b);
  Buf no = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, x->args[0], &f) && fm_len(l, &yes, x->args[1], &n) && store(l, &yes, res, n) &&
         fm_len(l, &no, x->args[2], &n) && store(l, &no, res, n) && if_flag(l, b, f.w, &yes, &no);
}

/* OUT := the local LOCAL. */
static int local_op(Low *l, uint32_t local, Operand *out) {
  *out = operand(ex_local(l, local));
  return out->w != NULL;
}

/* S8 (R4): 1 when X is a fold over FreeMon (a block of one) of the used
   trace. */
static int used_fold(const Low *l, const Value *x) {
  const FamilyInfo *fam = &l->m->families[x->inst];
  const Value *u = x->argc == fam->ctor_count + 2u ? x->args[x->argc - 1u] : NULL;
  return l->has_used && u != NULL && u->kind == VAL_VAR && u->nat == l->used_level && fam->block_count == 1u &&
         strcmp(fam->name, "FreeMon") == 0;
}

/* len of the fold X of the used trace (S8, R4). The used digests are the
   words 3 .. 3 + count - 1 of the region, the first one innermost
   (fmCons). ACC := len of the fmUnit case and I := 0, then count times:
   E := word 3 + I, ACC := len of the fmMul case applied to the fmEta case
   of E and to a variable whose len is ACC, I := I + 1. The variable has
   no operand (lower_value refuses it), so a strange fmMul case refuses. */
static int fm_len_used(Low *l, Buf *b, const Value *x, Operand *out) {
  const int has = l->has_acc;
  const uint32_t level = l->acc_level;
  const uint32_t len = l->acc_len;
  Operand n;
  Slot acc = slot_new(l);
  Slot i = slot_new(l);
  Slot e = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(&in);
  const Value *p;
  const Value *q;
  const Value *v;
  int ok;
  *out = slot_get(l, acc);
  ok = fm_len(l, &in, x->args[1], &n) && store(l, &in, acc, n) && set(l, &in, i.w, ex_const(l, 0)) &&
       set(l, &body, e.w,
           ex_load(l, ex_bin(l, IR_OP_ADD, ex_const(l, 24), ex_bin(l, IR_OP_MUL, ex_local(l, i.w), ex_const(l, 8))), 0));
  p = ok ? fresh(l, slot_get(l, e)) : NULL;
  p = p != NULL ? apply_value(l->m, x->args[0], p) : NULL;
  q = p != NULL ? fresh(l, operand(ex_const(l, 0))) : NULL;
  l->has_acc = 1;
  l->acc_level = q != NULL ? (uint32_t)q->nat : 0u;
  l->acc_len = acc.w;
  v = q != NULL ? apply_value(l->m, x->args[2], p) : NULL;
  v = v != NULL ? apply_value(l->m, v, q) : NULL;
  ok = got(l, v) && fm_len(l, &body, v, &n) && store(l, &body, acc, n) &&
       set(l, &body, i.w, ex_bin(l, IR_OP_ADD, ex_local(l, i.w), ex_const(l, 1))) &&
       st_repeat(l, &in, ex_local(l, l->count), &body) && splice(l, b, &in);
  l->has_acc = has;
  l->acc_level = level;
  l->acc_len = len;
  return ok;
}

/* len X (S8, Q-S8-9 (a)): fmUnit is 0, fmEta is 1, fmMul is the sum of its
   two parts and a stuck flagIf is an IF on its flag. In decide and
   malformed (R4), the used trace is its count and a fold of it is
   fm_len_used. Any other trace refuses. */
static int fm_len(Low *l, Buf *b, const Value *x, Operand *out) {
  const CtorInfo *c = x->kind == VAL_OP && x->op == OP_CTOR ? &l->m->ctors[x->inst] : NULL;
  uint32_t p = c != NULL ? l->m->families[c->family].param_count : 0u;
  Operand y;
  Operand z;
  if (x->kind == VAL_STUCK && x->op == OP_FLAG_IF)
    return fm_len_if(l, b, x, out);
  if (x->kind == VAL_VAR && l->has_acc && x->nat == l->acc_level)
    return local_op(l, l->acc_len, out);
  if (x->kind == VAL_VAR && l->has_used && x->nat == l->used_level)
    return local_op(l, l->count, out);
  if (x->kind == VAL_STUCK && x->op == OP_FOLD_FAMILY && used_fold(l, x))
    return fm_len_used(l, b, x, out);
  if (c != NULL && strcmp(c->name, "fmUnit") == 0)
    return give(l, out, 0);
  if (c != NULL && strcmp(c->name, "fmEta") == 0)
    return give(l, out, 1);
  if (c != NULL && strcmp(c->name, "fmMul") == 0 && x->argc == p + 2u)
    return fm_len(l, b, x->args[p], &y) && fm_len(l, b, x->args[p + 1u], &z) && word_add(l, b, y, z, out);
  return refuse(l, "len (the op count of a FreeMon trace at run time)");
}

/* A fold over a family without a recursive field is a case split. With at
   most one recursive field in each constructor, it is a spine walk. The
   arguments are the cases of each constructor of the block, the target of
   each family of the block, then the value (stuck_fold). A family outside a
   block is a block of one. The cases of this family start at off. A
   recursive fold over a block of families refuses (S3). A fold over Size
   is a REPEAT on the word (S8, Q-S8-9 (a)). */
static int family_fold(Low *l, Buf *b, const Value *v, Operand *out) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  uint32_t off = fam->first_ctor - l->m->families[fam->block_first].first_ctor;
  uint32_t *fields = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *fields);
  uint32_t *recs = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *recs);
  const CtorInfo *c;
  uint32_t rec = 0;
  uint32_t here;
  uint32_t i;
  uint32_t k;
  if (is_size(l, v->inst))
    return repeat_fold(l, b, v->args[v->argc - 1u], v->args[0], v->args[1], out);
  if (fields == NULL || recs == NULL)
    return oom(l);
  for (i = 0; i < fam->ctor_count; i++) {
    c = &l->m->ctors[fam->first_ctor + i];
    here = 0;
    recs[i] = NO_REC;
    for (k = 0; k < c->field_count; k++) {
      here += c->fields[k].recursive ? 1u : 0u;
      recs[i] = c->fields[k].recursive ? k : recs[i];
    }
    if (here > 1u)
      return diag_fail(l->m->diag, "REFUSE_RUNTIME_TREE", l->m->def, "a fold over a tree of %s must exist at run time (HOST-LIMIT)", fam->name);
    rec += here;
    fields[i] = c->field_count;
  }
  if (rec != 0 && fam->block_count > 1u)
    return diag_fail(l->m->diag, "REFUSE_LOWER", l->m->def, "a recursive fold over a block of families");
  return rec != 0 ? family_spine(l, b, v, recs, out) : cases_of(l, b, v->args[v->argc - 1u], v->args + off, fields, fam->ctor_count, out);
}

/* The two arms of an option or a sum: a SWITCH on the tag of X. */
static int two_arms(Low *l, Buf *b, Operand x, const Buf *a0, const Buf *a1) {
  IrBlock *arms = arena_alloc(l->m->arena, 2u * sizeof *arms);
  if (arms == NULL)
    return oom(l);
  arms[0] = block_of(a0);
  arms[1] = block_of(a1);
  return st_switch(l, b, ex_load(l, x.w, 0), arms, 2u);
}

/* Lowers the option or sum XV into X. Arm 0 gets RES := X; arm 1 gets P :=
   the payload. */
static int carrier_begin(Low *l, Buf *b, const Value *xv, Slot res, Operand *x, Buf *a0, Buf *a1, Operand *p) {
  return lower(l, b, xv, x) && store(l, a0, res, *x) && field_at(l, a1, x->w, 0, p);
}

static int carrier_map(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Operand y;
  Operand t;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  *out = slot_get(l, res);
  t = slot_get(l, ts);
  return carrier_begin(l, b, v->args[1], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[0], &p, 1, ts) &&
         block_ops(l, &a1, 1, &t, 1, &y) && store(l, &a1, res, y) && two_arms(l, b, x, &a0, &a1);
}

static int carrier_bind(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Slot res = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  *out = slot_get(l, res);
  return carrier_begin(l, b, v->args[0], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[1], &p, 1, res) &&
         two_arms(l, b, x, &a0, &a1);
}

static int carrier_filter(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Operand y;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  Buf keep = buf_under(&a1);
  Buf drop = buf_under(&a1);
  *out = slot_get(l, res);
  return carrier_begin(l, b, v->args[1], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[0], &p, 1, ts) &&
         store(l, &keep, res, x) && block_ops(l, &drop, 0, NULL, 0, &y) && store(l, &drop, res, y) &&
         if_flag(l, &a1, ex_local(l, ts.w), &keep, &drop) &&
         two_arms(l, b, x, &a0, &a1);
}

/* FILTER on a list, or on a sum. */
static int not_option(Low *l, Buf *b, const Value *v, Operand *out) {
  return v->inst == CARRIER_LIST ? list_filter(l, b, v, out) : internal(l, "a filter over a sum");
}

/* foldNat: the arguments are STEP, Z, N. */
static int fold_nat(Low *l, Buf *b, const Value *v, Operand *out) {
  return repeat_fold(l, b, v->args[2], v->args[1], v->args[0], out);
}

/* An eliminator on a variable. */
static int lower_stuck(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->op) {
  case OP_NAT:
  case OP_FLAG:
  case OP_BUDGET:
  case OP_APPROVAL:
  case OP_PAYLOAD_T:
  case OP_BYTES:
  case OP_DIGEST:
  case OP_RESOURCE:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_CTOR:
  case OP_PURE:
  case OP_INTERVAL:
  case OP_SYSTEM:
    return internal(l, "a stuck canonical value");
  case OP_TRANSP:
  case OP_HCOMP:
    return internal(l, "a stuck cubical operation");
  case OP_FIRST:
  case OP_WITNESS:
    return field_of(l, b, v->args[0], 0, out);
  case OP_SECOND:
  case OP_PAYLOAD:
    return field_of(l, b, v->args[0], 1, out);
  case OP_PROJ:
    return field_of(l, b, v->args[0], v->field, out);
  case OP_EITHER:
    return two_cases(l, b, v, 1, out);
  case OP_OPTION_ELIM:
    return two_cases(l, b, v, 0, out);
  case OP_MAP:
    return v->inst == CARRIER_LIST ? list_map(l, b, v, out) : carrier_map(l, b, v, out);
  case OP_BIND:
    return v->inst == CARRIER_LIST ? list_bind(l, b, v, out) : carrier_bind(l, b, v, out);
  case OP_FILTER:
    return v->inst == CARRIER_OPTION ? carrier_filter(l, b, v, out) : not_option(l, b, v, out);
  case OP_FOLD_NAT:
    return fold_nat(l, b, v, out);
  case OP_FOLD_LIST:
    return fold_list(l, b, v, out);
  case OP_UNFOLD:
    return unfold(l, b, v, out);
  case OP_FOLD_FAMILY:
    return family_fold(l, b, v, out);
  case OP_NAT_SUB:
    return nat_bin(l, b, v, IR_OP_SUB, out);
  case OP_NAT_EQ:
    return nat_bin(l, b, v, IR_OP_EQ, out);
  case OP_NAT_LE:
    return nat_bin(l, b, v, IR_OP_LE, out);
  case OP_FLAG_IF:
    return flag_if(l, b, v, out);
  case OP_NAT_ADD:
    return nat_add(l, b, v, out);
  case OP_NAT_DIV:
    return nat_bin(l, b, v, IR_OP_DIV, out);
  case OP_NAT_MOD:
    return nat_bin(l, b, v, IR_OP_MOD, out);
  case OP_NONZERO_OF:
    return nonzero_of(l, b, v, out);
  case OP_NONZERO_VALUE:
    return lower(l, b, v->args[0], out);
  case OP_NONZERO:
    return internal(l, "a stuck canonical value");
  case OP_RES_ROOT:
  case OP_RES_CHILD:
    return refuse(l, "a Resource at run time (S8)");
  case OP_RES_EQ:
    return nat_bin(l, b, v, IR_OP_EQ, out);
  case OP_RES_WITHIN:
    return host_call(l, b, v, IR_IMPORT_WITHIN, out);
  case OP_BUDGET_MK:
  case OP_BUDGET_LEFT:
  case OP_SPEND:
  case OP_SPLIT:
    return refuse(l, "a Budget at run time (S8)");
  /* S8 (Q-S8-2 (a)): the host interns the Payload and Digest values, so
     equal values have equal handles and the test is i64.eq. */
  case OP_PAYLOAD_OF:
    return lower(l, b, v->args[0], out);
  case OP_PAYLOAD_EQ:
  case OP_DIGEST_EQ:
    return nat_bin(l, b, v, IR_OP_EQ, out);
  case OP_HASH:
    return host_call(l, b, v, IR_IMPORT_HASH, out);
  case OP_ENVELOPE:
    return host_call(l, b, v, IR_IMPORT_ENVELOPE, out);
  case OP_APPROVAL_MK:
  case OP_APPROVAL_DIGEST:
    return refuse(l, "an Approval at run time (S8)");
  case OP_FM_LEN:
    return fm_len(l, b, v->args[1], out);
  case OP_ELIM:
    return lower_elim(l, b, v, out);
  case OP_KMAP:
  case OP_KMAP_OF:
  case OP_KMAP_GET:
  case OP_KMAP_SET:
  case OP_FIN:
  case OP_FIN_ADD:
  case OP_FIN_NEG:
  case OP_FIN_CASE:
  case OP_GLUE:
  case OP_GLUE_ELEM:
  case OP_UNGLUE:
    return refuse(l, "an elim, KMap, Fin or Glue operation");
  }
  return internal(l, "an unknown operation");
}

static int lower_value(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->kind) {
  case VAL_NAT:
    return give(l, out, v->nat);
  case VAL_UNIV:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
  case VAL_PATHP:
  case VAL_IVAL:
    return give(l, out, 0);
  case VAL_PAPP:
    return internal(l, "a stuck cubical operation");
  case VAL_LAM:
  case VAL_APP:
    return closure(l);
  case VAL_VAR:
    if (v->nat >= l->level_count)
      return internal(l, "a variable without an operand");
    if (l->has_acc && v->nat == l->acc_level)
      return refuse(l, "a used trace other than its len (S8)");
    *out = l->levels[v->nat];
    return 1;
  case VAL_OP:
    return lower_canon(l, b, v, out);
  case VAL_STUCK:
    return lower_stuck(l, b, v, out);
  }
  return internal(l, "an unknown value");
}

static int lower(Low *l, Buf *b, const Value *v, Operand *out) {
  int ok;
  if (v == NULL)
    return got(l, v);
  if (memo_kind(v) && memo_find(b, v, out))
    return 1;
  if (l->m->depth >= EVAL_DEPTH_LIMIT)
    return diag_fail(l->m->diag, "EVAL_DEPTH", l->m->def, "the lowering nests deeper than %u levels", EVAL_DEPTH_LIMIT);
  l->m->depth++;
  ok = lower_value(l, b, v, out);
  l->m->depth--;
  return ok && (!memo_kind(v) || memo_put(l, b, v, *out));
}

/* The heap bound (S3): the ALLOC words on the longest path of a block.
   An IF or a SWITCH takes its largest branch. A REPEAT or a WHILE with a
   constant count takes the count times its body. The bound stops at
   IR_HEAP_WORDS_MAX + 1. 0 (no static bound) when an ALLOC is under a
   REPEAT or a WHILE with a run-time count. */
static uint64_t heap_add(uint64_t a, uint64_t b) {
  return a + b > IR_HEAP_WORDS_MAX ? IR_HEAP_WORDS_MAX + 1u : a + b;
}

static uint64_t heap_times(uint64_t count, uint64_t words) {
  return words != 0 && count > IR_HEAP_WORDS_MAX / words ? IR_HEAP_WORDS_MAX + 1u : count * words;
}

static int heap_block(const IrBlock *b, uint64_t *out);

static int heap_stmt(const IrStmt *s, uint64_t *out) {
  uint64_t x = 0;
  uint64_t y = 0;
  size_t k;
  int ok = 1;
  *out = 0;
  switch (s->kind) {
    case IR_STMT_SET:
    case IR_STMT_STORE:
    case IR_STMT_RETURN:
      return 1;
    case IR_STMT_ALLOC:
      *out = heap_add(0, s->field_count);
      return 1;
    case IR_STMT_IF:
      ok = heap_block(&s->body, &x) && heap_block(&s->otherwise, &y);
      *out = x > y ? x : y;
      return ok;
    case IR_STMT_SWITCH:
      for (k = 0; k < s->arm_count && ok; k++) {
        ok = heap_block(&s->arms[k], &y);
        x = y > x ? y : x;
      }
      *out = x;
      return ok;
    case IR_STMT_REPEAT:
    case IR_STMT_WHILE:
      ok = heap_block(&s->body, &x) && (x == 0 || s->expr->kind == IR_EXPR_CONST);
      *out = s->expr->kind == IR_EXPR_CONST ? heap_times(s->expr->value, x) : 0;
      return ok;
  }
  return 0;
}

static int heap_block(const IrBlock *b, uint64_t *out) {
  uint64_t x = 0;
  size_t i;
  int ok = 1;
  *out = 0;
  for (i = 0; i < b->count && ok; i++) {
    ok = heap_stmt(b->items[i], &x);
    *out = heap_add(*out, x);
  }
  return ok;
}

/* The entry INDEX, applied to a variable for each parameter. */
static int lower_entry(Machine *m, uint32_t index, const Entry *e, IrFunc *fn) {
  Low l;
  Buf body = buf_under(NULL);
  IrScalar *params = arena_alloc(m->arena, ((size_t)e->param_count + 1u) * sizeof *params);
  const Value *v;
  const Value *x;
  Operand r;
  uint32_t k;
  memset(&l, 0, sizeof l);
  l.m = m;
  l.local_count = e->param_count;
  m->def = m->defs[index].name;
  m->fuel = EVAL_FUEL_STEPS;
  if (params == NULL)
    return oom(&l);
  v = def_value(m, index);
  for (k = 0; k < e->param_count && v != NULL; k++) {
    params[k] = e->param_flag[k] ? IR_SCALAR_FLAG : IR_SCALAR_NAT;
    x = fresh(&l, operand(ex_local(&l, k)));
    v = x == NULL ? NULL : apply_value(m, v, x);
  }
  if (!got(&l, v) || !lower(&l, &body, v, &r) || !emit(&l, &body, stmt_expr(&l, IR_STMT_RETURN, r.w)))
    return 0;
  fn->name = m->defs[index].name;
  fn->params = params;
  fn->param_count = e->param_count;
  fn->result = e->result_flag ? IR_SCALAR_FLAG : IR_SCALAR_NAT;
  fn->local_count = l.local_count;
  fn->body = block_of(&body);
  fn->heap_words = 0;
  fn->imports = l.imports;
  if (!heap_block(&fn->body, &fn->heap_words) || fn->heap_words > IR_HEAP_WORDS_MAX)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "heap use has no static bound");
  return 1;
}

/* 1 when the program has a def start (S8, Q-S8-5 (a)). */
static int has_start(const Machine *m) {
  uint32_t i;
  int found = 0;
  for (i = 0; i < m->def_count && !found; i++)
    found = m->defs[i].origin == ORIGIN_PROGRAM && m->defs[i].name != NULL && strcmp(m->defs[i].name, "start") == 0;
  return found;
}

static int is_ctor(const Machine *m, const Value *v, const char *name) {
  return v->kind == VAL_OP && v->op == OP_CTOR && strcmp(m->ctors[v->inst].name, name) == 0;
}

/* The def NAME of the program (PROGRAM 1) or of the domain or a kit
   (PROGRAM 0), else def_count (S8). */
static uint32_t def_named(const Machine *m, const char *name, int program) {
  uint32_t i;
  for (i = 0; i < m->def_count; i++)
    if ((m->defs[i].origin == ORIGIN_PROGRAM) == program && m->defs[i].name != NULL && strcmp(m->defs[i].name, name) == 0)
      return i;
  return m->def_count;
}

/* The state region (S8, R1 and R2): [steps, left, count, used_0 ..
   used_{K-1}], K the steps of start, from the closed start authority
   *START. */
static int region_of(Machine *m, IrProgram *prog, const Value **start) {
  const uint32_t si = def_named(m, "start", 1);
  const Value *s;
  const Value *k;
  uint64_t steps = 0;
  uint64_t *init;
  uint64_t i;
  m->def = "start";
  m->fuel = EVAL_FUEL_STEPS;
  s = si < m->def_count ? def_value(m, si) : NULL;
  if (s == NULL)
    return m->diag->set ? 0 : diag_fail(m->diag, "INTERNAL", m->def, "the lowering found a missing start");
  if (!is_ctor(m, s, "authority") || s->argc != 4u)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "start is not a closed authority (S8)");
  for (k = s->args[1]; is_ctor(m, k, "szS") && k->argc == 1u && steps <= IR_HEAP_WORDS_MAX; k = k->args[0])
    steps++;
  if (!is_ctor(m, k, "sz0") || steps > IR_HEAP_WORDS_MAX)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "start steps are not closed (S8)");
  if (s->args[2]->kind != VAL_NAT)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "start left is not closed (S8)");
  if (!is_ctor(m, s->args[3], "fmUnit"))
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "start used is not fmUnit (S8)");
  init = arena_alloc(m->arena, (size_t)(steps + 3u) * sizeof *init);
  if (init == NULL)
    return diag_fail(m->diag, "OOM", m->def, "out of memory");
  init[0] = steps;
  init[1] = s->args[2]->nat;
  for (i = 2; i < steps + 3u; i++)
    init[i] = 0;
  prog->region_words = (uint32_t)(steps + 3u);
  prog->region_init = init;
  *start = s;
  return 1;
}

/* A new block of the N words W (S8: a tag known at run time). */
static int alloc_words(Low *l, Buf *b, const IrExpr *const *w, uint32_t n, Operand *out) {
  const IrExpr **fields = arena_alloc(l->m->arena, (size_t)n * sizeof *fields);
  IrStmt *s = stmt_new(l, IR_STMT_ALLOC);
  uint32_t i;
  if (fields == NULL || s == NULL)
    return fields == NULL ? oom(l) : 0;
  for (i = 0; i < n; i++)
    if ((fields[i] = w[i]) == NULL)
      return 0;
  s->local = local_new(l);
  s->fields = fields;
  s->field_count = n;
  *out = operand(ex_local(l, s->local));
  return emit(l, b, s);
}

/* Word FIELD of the block at ADDR := E (S8, R3). */
static int store_word(Low *l, Buf *b, const IrExpr *addr, uint32_t field, const IrExpr *e) {
  IrStmt *s = e == NULL ? NULL : stmt_expr(l, IR_STMT_STORE, addr);
  const IrExpr **fields = s == NULL ? NULL : arena_alloc(l->m->arena, sizeof *fields);
  if (fields == NULL)
    return s == NULL ? 0 : oom(l);
  fields[0] = e;
  s->field = field;
  s->fields = fields;
  s->field_count = 1;
  return emit(l, b, s);
}

/* S8 (R6): the export NAME, decide (PARAMS 6: target, action, payload,
   cost, has_digest, digest) or malformed (PARAMS 0). The prologue reads
   steps, left and count from the region; the authority is START with
   those 3 fields as variables (used is the word 0, R5). The result is
   judge of it and the ticket of the params, or judgeMalformed of it. The
   epilogue writes the new steps and left, adds a new used digest and
   returns the code of the decision: permit 0, deny k k + 1. */
static int lower_export(Machine *m, const Value *start, const char *name, uint32_t param_n, IrFunc *fn) {
  Low l;
  Buf body = buf_under(NULL);
  Buf yes = buf_under(&body);
  Buf deny = buf_under(&body);
  Buf none = buf_under(&body);
  IrScalar *params = arena_alloc(m->arena, ((size_t)param_n + 1u) * sizeof *params);
  const Value **args = arena_alloc(m->arena, 4u * sizeof *args);
  const uint32_t ji = def_named(m, param_n != 0u ? "judge" : "judgeMalformed", 0);
  const IrExpr *w[4];
  const Value *s;
  const Value *t;
  const Value *v;
  Operand r;
  Operand d;
  Operand a;
  Operand n;
  Operand act;
  Operand opt;
  Operand mut;
  Operand tk;
  Slot steps;
  Slot left;
  Slot code;
  uint32_t k;
  int ok;
  memset(&l, 0, sizeof l);
  l.m = m;
  l.local_count = param_n;
  m->def = name;
  m->fuel = EVAL_FUEL_STEPS;
  if (params == NULL || args == NULL)
    return oom(&l);
  if (ji >= m->def_count)
    return internal(&l, "no domain judge");
  for (k = 0; k < param_n; k++)
    params[k] = IR_SCALAR_NAT;
  steps = slot_new(&l);
  left = slot_new(&l);
  l.count = local_new(&l);
  ok = set(&l, &body, steps.w, ex_load(&l, ex_const(&l, 0), 0)) && set(&l, &body, left.w, ex_load(&l, ex_const(&l, 0), 1)) &&
       set(&l, &body, l.count, ex_load(&l, ex_const(&l, 0), 2));
  args[0] = start->args[0];
  args[1] = ok ? fresh(&l, slot_get(&l, steps)) : NULL;
  args[2] = args[1] != NULL ? fresh(&l, slot_get(&l, left)) : NULL;
  args[3] = args[2] != NULL ? fresh(&l, operand(ex_const(&l, 0))) : NULL;
  l.has_used = 1;
  l.used_level = args[3] != NULL ? (uint32_t)args[3]->nat : 0u;
  s = args[3] != NULL ? val_op(m, start->kind, start->op, start->inst, start->field, args, 4u) : NULL;
  v = s != NULL ? apply_value(m, def_value(m, ji), s) : NULL;
  if (v != NULL && param_n != 0u) {
    w[0] = ex_local(&l, 1);
    ok = alloc_words(&l, &body, w, 1, &act);
    w[0] = ex_local(&l, 4);
    w[1] = ex_local(&l, 5);
    ok = ok && alloc_words(&l, &body, w, 2, &opt);
    w[0] = ex_const(&l, 0);
    w[1] = ex_local(&l, 0);
    w[2] = act.w;
    w[3] = ex_local(&l, 2);
    ok = ok && alloc_words(&l, &body, w, 4, &mut);
    w[1] = mut.w;
    w[2] = ex_local(&l, 3);
    w[3] = opt.w;
    ok = ok && alloc_words(&l, &body, w, 4, &tk);
    t = ok ? fresh(&l, tk) : NULL;
    v = t != NULL ? apply_value(m, v, t) : NULL;
  }
  code = slot_new(&l);
  if (!got(&l, v) || !lower(&l, &body, v, &r))
    return 0;
  d = operand(atom(&l, &body, ex_load(&l, r.w, 1)));
  a = operand(atom(&l, &body, ex_load(&l, r.w, 2)));
  ok = d.w != NULL && a.w != NULL && store_word(&l, &body, ex_const(&l, 0), 0, ex_load(&l, a.w, 2)) &&
       store_word(&l, &body, ex_const(&l, 0), 1, ex_load(&l, a.w, 3));
  n = operand(ok ? atom(&l, &body, ex_load(&l, a.w, 4)) : NULL);
  ok = n.w != NULL &&
       store_word(&l, &yes, ex_bin(&l, IR_OP_ADD, ex_const(&l, 24), ex_bin(&l, IR_OP_MUL, ex_local(&l, l.count), ex_const(&l, 8))), 0,
                  ex_load(&l, ex_load(&l, n.w, 1), 1)) &&
       store_word(&l, &yes, ex_const(&l, 0), 2, ex_bin(&l, IR_OP_ADD, ex_local(&l, l.count), ex_const(&l, 1))) &&
       if_flag(&l, &body, n.w, &yes, &none) && set(&l, &body, code.w, ex_const(&l, 0)) &&
       set(&l, &deny, code.w, ex_bin(&l, IR_OP_ADD, ex_load(&l, ex_load(&l, d.w, 1), 0), ex_const(&l, 1))) &&
       if_flag(&l, &body, ex_load(&l, d.w, 0), &deny, &none) &&
       emit(&l, &body, stmt_expr(&l, IR_STMT_RETURN, ex_local(&l, code.w)));
  if (!ok)
    return 0;
  fn->name = name;
  fn->params = params;
  fn->param_count = param_n;
  fn->result = IR_SCALAR_NAT;
  fn->local_count = l.local_count;
  fn->body = block_of(&body);
  fn->heap_words = 0;
  fn->imports = l.imports;
  if (!heap_block(&fn->body, &fn->heap_words) || fn->heap_words > IR_HEAP_WORDS_MAX)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "heap use has no static bound");
  return 1;
}

/* 1 when NAME is the name of a bind export (S8, R6) or of the memory
   export (S8, Q-S8-10 (a)). */
static int is_export(const char *name) {
  return name != NULL &&
         (strcmp(name, "decide") == 0 || strcmp(name, "malformed") == 0 || strcmp(name, "memory") == 0);
}

int lower_program(Machine *m, IrProgram *prog) {
  IrFunc *funcs = arena_alloc(m->arena, ((size_t)m->def_count + 3u) * sizeof *funcs);
  IrBind *binds = arena_alloc(m->arena, ((size_t)m->bind_count + 1u) * sizeof *binds);
  Entry e;
  size_t n = 0;
  uint32_t i;
  int entry;
  const Value *start = NULL;
  prog->funcs = funcs;
  prog->func_count = 0;
  prog->res_paths = m->res_paths;
  prog->res_count = m->res_path_count;
  prog->binds = binds;
  prog->bind_count = 0;
  prog->region_words = 0;
  prog->region_init = NULL;
  if (funcs == NULL || binds == NULL)
    return diag_fail(m->diag, "OOM", m->def, "out of memory");
  if (m->bind_count > 0 && !has_start(m))
    return diag_fail(m->diag, "REFUSE_LOWER", m->binds[0].tool, "a bind line needs a def start (S8, Q-S8-5 (a))");
  for (i = 0; i < m->bind_count; i++) {
    binds[i].tool = m->binds[i].tool;
    binds[i].action = m->binds[i].action;
    binds[i].pointer = m->binds[i].pointer;
  }
  prog->bind_count = m->bind_count;
  if (m->bind_count > 0 && !region_of(m, prog, &start))
    return 0;
  for (i = 0; i < m->def_count; i++) {
    m->fuel = EVAL_FUEL_STEPS;
    entry = m->defs[i].origin == ORIGIN_PROGRAM && entry_of(m, m->defs[i].type, &e) && !e.result_option;
    if (entry && m->bind_count > 0 && is_export(m->defs[i].name))
      return diag_fail(m->diag, "REFUSE_LOWER", m->defs[i].name, "the entry %s has the name of a bind export (S8)", m->defs[i].name);
    if (entry && !lower_entry(m, i, &e, &funcs[n]))
      return 0;
    n += entry ? 1u : 0u;
  }
  if (m->bind_count > 0 &&
      (!lower_export(m, start, "decide", 6u, &funcs[n]) || !lower_export(m, start, "malformed", 0u, &funcs[n + 1u])))
    return 0;
  n += m->bind_count > 0 ? 2u : 0u;
  prog->func_count = n;
  return 1;
}
