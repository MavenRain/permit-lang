/* The Wasm back end. It writes a Wasm 1.0 (MVP) binary module directly.

   Module layout:
   - The functions 0 to I - 1 are the host imports of module permit that an
     entry calls (S8, Q-S8-2 (a)), in IR_IMPORTS order. I is 0 when no entry
     calls one, and then the module has no import section.
   - Function I is the internal allocator `alloc (bytes : i32) -> i64`. It
     bumps the mutable i64 global 0 (the heap pointer) over memory 0, with no
     check. Memory 0 has a fixed size (minimum = maximum): the pages of the
     largest heap bound of an entry (IrFunc heap_words), at least 1 page.
     When the program has a bind line, memory 0 is exported as `memory`
     after the entries (S8, Q-S8-10 (a)), so a host reads the state region.
   - Function I + 1 + K is IR entry K, exported as its name. Each parameter and the
     result is an i64. Each entry first resets the heap pointer to the heap
     start (8 * the state region words, S8; 0 when the program has no bind
     line), and it sets a Flag parameter to 1 when it is not 0.
   - A word is 8 bytes. Word K of the block at address A is at A + 8K.
   - After the code section (S8): the custom section permit.res when the
     program has a path literal (a vector of the path names in program
     order; the handle of a literal is its index, Q-S8-3 (a)), then the
     custom section permit.bind when it has a bind line (a vector of rows:
     the tool name, the Action arm number as a uleb, the pointer name;
     Q-S8-4 (a)).

   Each entry has three i64 scratch locals after the IR locals: S and S + 1
   hold the operands of SUB and ADD_CARRY, and S + 2 holds the
   address of an ALLOC. An operator reads its operands into S and S + 1 only
   after both operand expressions are done, so a nested operator cannot
   overwrite them.

   Each sized part (a section, a function body) is written in two passes: the
   first pass counts the bytes, the second pass writes them after the size.
   So the back end needs no buffer. */
#include <stdint.h>
#include <string.h>

#include "target.h"
#include "wasmop.h"

#define WASM_PAGE_BYTES 65536u
#define WASM_DEPTH_MAX 2000u

/* The opcodes: the list of src/wasmop.h. */
#define WASM_OP_ENUM(name, byte, imm) name = byte,
enum {
  WASM_OPS(WASM_OP_ENUM)
  TYPE_I32 = 0x7F,
  TYPE_I64 = 0x7E,
  TYPE_FUNC = 0x60,
  TYPE_EMPTY = 0x40,
  ALIGN_WORD = 3
};
#undef WASM_OP_ENUM

enum {
  SECTION_CUSTOM = 0,
  SECTION_TYPE = 1,
  SECTION_IMPORT = 2,
  SECTION_FUNCTION = 3,
  SECTION_MEMORY = 5,
  SECTION_GLOBAL = 6,
  SECTION_EXPORT = 7,
  SECTION_CODE = 10,
  SECTION_DATA = 11
};

/* A byte sink. With out == NULL it only counts. */
typedef struct {
  FILE *out;
  size_t count;
  FILE *err;
  uint32_t imports; /* bit K: host import K is in the module (S8) */
  uint64_t base;    /* the heap start in bytes, 8 * the region words (S8) */
} Sink;

typedef int (*Part)(Sink *s, const void *arg);

const char target_name[] = "wasm";

static int put_byte(Sink *s, unsigned v) {
  s->count++;
  return s->out == NULL || fputc((int)(v & 0xFFu), s->out) != EOF;
}

static int put_uleb(Sink *s, uint64_t v) {
  int ok = 1;
  for (; ok && v >= 0x80u; v >>= 7) ok = put_byte(s, (unsigned)(v & 0x7Fu) | 0x80u);
  return ok && put_byte(s, (unsigned)v);
}

/* Signed LEB128 of the two's complement value of V. */
static int put_sleb(Sink *s, uint64_t v) {
  uint64_t fill = (v >> 63) != 0 ? ~(~(uint64_t)0 >> 7) : 0;
  unsigned low = (unsigned)(v & 0x7Fu);
  uint64_t rest = (v >> 7) | fill;
  int last = (rest == 0 && (low & 0x40u) == 0) || (rest == ~(uint64_t)0 && (low & 0x40u) != 0);
  int ok = put_byte(s, last ? low : low | 0x80u);
  return ok && (last || put_sleb(s, rest));
}

static int put_name(Sink *s, const char *name) {
  size_t i;
  size_t len = strlen(name);
  int ok = put_uleb(s, len);
  for (i = 0; i < len && ok; i++) ok = put_byte(s, (unsigned char)name[i]);
  return ok;
}

/* Writes the byte size of PART, then PART. */
static int sized(Sink *s, Part part, const void *arg) {
  Sink count = {NULL, 0, s->err, s->imports, s->base};
  return part(&count, arg) && put_uleb(s, count.count) && part(s, arg);
}

#define IR_IMPORT_ROW(name, text, params) {text, params},
static const struct {
  const char *text;
  uint32_t params;
} import_rows[] = {IR_IMPORTS(IR_IMPORT_ROW)};
#undef IR_IMPORT_ROW

/* The function index of host import K (S8): the imports of the module
   before it. With K = IR_IMPORT_COUNT it is the import count, the index of
   alloc. */
static uint32_t import_index(uint32_t mask, uint32_t k) {
  uint32_t n = 0;
  uint32_t i;
  for (i = 0; i < k; i++) n += (mask >> i) & 1u;
  return n;
}

/* The type of host import K: its i64 parameters, one i64 result. */
static int import_type(Sink *s, uint32_t k) {
  uint32_t p;
  int ok = put_byte(s, TYPE_FUNC) && put_uleb(s, import_rows[k].params);
  for (p = 0; p < import_rows[k].params && ok; p++) ok = put_byte(s, TYPE_I64);
  return ok && put_uleb(s, 1) && put_byte(s, TYPE_I64);
}

static int section(Sink *s, unsigned id, Part part, const void *arg) {
  return put_byte(s, id) && sized(s, part, arg);
}

static int op2(Sink *s, unsigned op, uint64_t imm) {
  return put_byte(s, op) && put_uleb(s, imm);
}

static int i64_const(Sink *s, uint64_t v) {
  return put_byte(s, OP_I64_CONST) && put_sleb(s, v);
}

static int i32_const(Sink *s, uint64_t v) {
  return put_byte(s, OP_I32_CONST) && put_sleb(s, v);
}

static int memarg(Sink *s, unsigned op, uint32_t word) {
  return put_byte(s, op) && put_uleb(s, ALIGN_WORD) && put_uleb(s, 8u * (uint64_t)word);
}

static int deep(Sink *s) {
  fprintf(s->err, "langc: WASM_DEPTH: -: the IR nests deeper than %u levels\n", WASM_DEPTH_MAX);
  return 0;
}

static int emit_expr(Sink *s, const IrExpr *e, uint32_t t, unsigned depth);

/* The operands are on the stack. Moves them to the scratch locals T, T + 1. */
static int operands(Sink *s, uint32_t t) {
  return op2(s, OP_LOCAL_SET, t + 1u) && op2(s, OP_LOCAL_SET, t);
}

/* The operands are on the stack. Leaves a, then b | (b = 0): the divisor is
   never 0, so div_u and rem_u can not trap. The lowering gives a NonZero b
   (S3); the guard stays for notraptool. */
static int divisor(Sink *s, uint32_t t) {
  return operands(s, t) && op2(s, OP_LOCAL_GET, t) && op2(s, OP_LOCAL_GET, t + 1u) && op2(s, OP_LOCAL_GET, t + 1u) &&
         put_byte(s, OP_I64_EQZ) && put_byte(s, OP_I64_EXTEND_I32_U) && put_byte(s, OP_I64_OR);
}

static int emit_op(Sink *s, IrOp op, uint32_t t) {
  switch (op) {
    case IR_OP_ADD:
      return put_byte(s, OP_I64_ADD);
    case IR_OP_SUB: /* select(a - b, 0, a >= b) */
      return operands(s, t) && op2(s, OP_LOCAL_GET, t) && op2(s, OP_LOCAL_GET, t + 1u) && put_byte(s, OP_I64_SUB) &&
             i64_const(s, 0) && op2(s, OP_LOCAL_GET, t) && op2(s, OP_LOCAL_GET, t + 1u) && put_byte(s, OP_I64_GE_U) &&
             put_byte(s, OP_SELECT);
    case IR_OP_MUL:
      return put_byte(s, OP_I64_MUL);
    case IR_OP_EQ:
      return put_byte(s, OP_I64_EQ) && put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_LE:
      return put_byte(s, OP_I64_LE_U) && put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_ADD_CARRY: /* a + b < a, modulo 2^64 */
      return operands(s, t) && op2(s, OP_LOCAL_GET, t) && op2(s, OP_LOCAL_GET, t + 1u) && put_byte(s, OP_I64_ADD) &&
             op2(s, OP_LOCAL_GET, t) && put_byte(s, OP_I64_LT_U) && put_byte(s, OP_I64_EXTEND_I32_U);
    case IR_OP_DIV:
      return divisor(s, t) && put_byte(s, OP_I64_DIV_U);
    case IR_OP_MOD:
      return divisor(s, t) && put_byte(s, OP_I64_REM_U);
  }
  return 0;
}

static int emit_expr(Sink *s, const IrExpr *e, uint32_t t, unsigned depth) {
  if (depth > WASM_DEPTH_MAX) return deep(s);
  switch (e->kind) {
    case IR_EXPR_CONST:
      return i64_const(s, e->value);
    case IR_EXPR_LOCAL:
      return op2(s, OP_LOCAL_GET, e->local);
    case IR_EXPR_BINARY:
      return emit_expr(s, e->left, t, depth + 1u) && emit_expr(s, e->right, t, depth + 1u) && emit_op(s, e->op, t);
    case IR_EXPR_LOAD:
      return emit_expr(s, e->left, t, depth + 1u) && put_byte(s, OP_I32_WRAP_I64) && memarg(s, OP_I64_LOAD, e->field);
    case IR_EXPR_CALL:
      return emit_expr(s, e->left, t, depth + 1u) && (e->right == NULL || emit_expr(s, e->right, t, depth + 1u)) &&
             op2(s, OP_CALL, import_index(s->imports, (uint32_t)e->value));
  }
  return 0;
}

static int emit_block(Sink *s, const IrBlock *b, uint32_t t, unsigned depth);

/* local := alloc(8 * n); word k := fields[k]. The address waits in T + 2. */
static int emit_alloc(Sink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  size_t k;
  int ok = i32_const(s, 8u * (uint64_t)st->field_count) && op2(s, OP_CALL, import_index(s->imports, IR_IMPORT_COUNT)) &&
           op2(s, OP_LOCAL_SET, t + 2u);
  for (k = 0; k < st->field_count && ok; k++) {
    ok = op2(s, OP_LOCAL_GET, t + 2u) && put_byte(s, OP_I32_WRAP_I64) && emit_expr(s, st->fields[k], t, depth + 1u) &&
         memarg(s, OP_I64_STORE, (uint32_t)k);
  }
  return ok && op2(s, OP_LOCAL_GET, t + 2u) && op2(s, OP_LOCAL_SET, st->local);
}

/* Blocks: exit, then arm n - 1 down to arm 0 (the innermost). br_table
   label k ends the block of arm k, so the code of arm k follows. The last
   arm is the default label n - 1, so a value of n - 1 or more runs it. After
   arm k, label n - 1 - k is the exit block. With no arm, nothing runs: a
   value of a family with no constructor does not exist. */
static int emit_switch(Sink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  size_t k;
  size_t n = st->arm_count;
  int ok = 1;
  if (n == 0)
    return 1;
  for (k = 0; k <= n && ok; k++) ok = put_byte(s, OP_BLOCK) && put_byte(s, TYPE_EMPTY);
  ok = ok && emit_expr(s, st->expr, t, depth + 1u) && op2(s, OP_LOCAL_TEE, t) && put_byte(s, OP_I32_WRAP_I64) &&
       i32_const(s, n) && op2(s, OP_LOCAL_GET, t) && i64_const(s, n) && put_byte(s, OP_I64_LT_U) &&
       put_byte(s, OP_SELECT) && op2(s, OP_BR_TABLE, n - 1u);
  for (k = 0; k < n && ok; k++) ok = put_uleb(s, k);
  for (k = 0; k < n && ok; k++) {
    ok = put_byte(s, OP_END) && emit_block(s, &st->arms[k], t, depth + 1u) && op2(s, OP_BR, n - 1u - k);
  }
  return ok && put_byte(s, OP_END);
}

/* counter := expr; then, at the top of each run, leave when counter = 0 or
   (for WHILE) local = 0. counter decreases by 1 after each run. */
static int emit_loop(Sink *s, const IrStmt *st, int is_while, uint32_t t, unsigned depth) {
  return emit_expr(s, st->expr, t, depth + 1u) && op2(s, OP_LOCAL_SET, st->counter) && put_byte(s, OP_BLOCK) &&
         put_byte(s, TYPE_EMPTY) && put_byte(s, OP_LOOP) && put_byte(s, TYPE_EMPTY) &&
         op2(s, OP_LOCAL_GET, st->counter) && put_byte(s, OP_I64_EQZ) && op2(s, OP_BR_IF, 1) &&
         (!is_while || (op2(s, OP_LOCAL_GET, st->local) && put_byte(s, OP_I64_EQZ) && op2(s, OP_BR_IF, 1))) &&
         emit_block(s, &st->body, t, depth + 1u) && op2(s, OP_LOCAL_GET, st->counter) && i64_const(s, 1) &&
         put_byte(s, OP_I64_SUB) && op2(s, OP_LOCAL_SET, st->counter) && op2(s, OP_BR, 0) && put_byte(s, OP_END) &&
         put_byte(s, OP_END);
}

static int emit_stmt(Sink *s, const IrStmt *st, uint32_t t, unsigned depth) {
  if (depth > WASM_DEPTH_MAX) return deep(s);
  switch (st->kind) {
    case IR_STMT_SET:
      return emit_expr(s, st->expr, t, depth + 1u) && op2(s, OP_LOCAL_SET, st->local);
    case IR_STMT_ALLOC:
      return emit_alloc(s, st, t, depth);
    case IR_STMT_IF: /* expr = 0: otherwise, else: body */
      return emit_expr(s, st->expr, t, depth + 1u) && put_byte(s, OP_I64_EQZ) && put_byte(s, OP_IF) &&
             put_byte(s, TYPE_EMPTY) && emit_block(s, &st->otherwise, t, depth + 1u) && put_byte(s, OP_ELSE) &&
             emit_block(s, &st->body, t, depth + 1u) && put_byte(s, OP_END);
    case IR_STMT_SWITCH:
      return emit_switch(s, st, t, depth);
    case IR_STMT_REPEAT:
      return emit_loop(s, st, 0, t, depth);
    case IR_STMT_WHILE:
      return emit_loop(s, st, 1, t, depth);
    case IR_STMT_RETURN:
      return emit_expr(s, st->expr, t, depth + 1u) && put_byte(s, OP_RETURN);
    case IR_STMT_STORE:
      return emit_expr(s, st->expr, t, depth + 1u) && put_byte(s, OP_I32_WRAP_I64) &&
             emit_expr(s, st->fields[0], t, depth + 1u) && memarg(s, OP_I64_STORE, st->field);
  }
  return 0;
}

static int emit_block(Sink *s, const IrBlock *b, uint32_t t, unsigned depth) {
  size_t i;
  int ok = depth <= WASM_DEPTH_MAX || deep(s);
  for (i = 0; i < b->count && ok; i++) ok = emit_stmt(s, b->items[i], t, depth + 1u);
  return ok;
}

/* alloc (bytes : i32) -> i64: old := heap; heap := old + bytes; return old.
   No check: memory 0 holds the heap bound of each entry. */
static int part_alloc(Sink *s, const void *arg) {
  (void)arg;
  return put_uleb(s, 0) && op2(s, OP_GLOBAL_GET, 0) && op2(s, OP_GLOBAL_GET, 0) && op2(s, OP_LOCAL_GET, 0) &&
         put_byte(s, OP_I64_EXTEND_I32_U) && put_byte(s, OP_I64_ADD) && op2(s, OP_GLOBAL_SET, 0) && put_byte(s, OP_END);
}

/* A Flag parameter K is true when it is not 0: K := K = 0 ? 0 : 1. */
static int flag_norm(Sink *s, const IrFunc *fn, size_t k) {
  return fn->params[k] == IR_SCALAR_NAT ||
         (i64_const(s, 0) && i64_const(s, 1) && op2(s, OP_LOCAL_GET, k) && put_byte(s, OP_I64_EQZ) &&
          put_byte(s, OP_SELECT) && op2(s, OP_LOCAL_SET, k));
}

static int part_entry(Sink *s, const void *arg) {
  const IrFunc *fn = arg;
  uint32_t t = fn->local_count;
  size_t k;
  int ok = put_uleb(s, 1) && put_uleb(s, (uint64_t)(fn->local_count - fn->param_count) + 3u) &&
           put_byte(s, TYPE_I64) && i64_const(s, s->base) && op2(s, OP_GLOBAL_SET, 0);
  for (k = 0; k < fn->param_count && ok; k++) ok = flag_norm(s, fn, k);
  /* The body ends in RETURN (lower_entry), so no code follows it. */
  return ok && emit_block(s, &fn->body, t, 0) && put_byte(s, OP_END);
}

static int part_types(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  size_t k;
  int ok = put_uleb(s, 1u + prog->func_count + import_index(s->imports, IR_IMPORT_COUNT)) && put_byte(s, TYPE_FUNC) &&
           put_uleb(s, 1) && put_byte(s, TYPE_I32) && put_uleb(s, 1) && put_byte(s, TYPE_I64);
  for (i = 0; i < prog->func_count && ok; i++) {
    ok = put_byte(s, TYPE_FUNC) && put_uleb(s, prog->funcs[i].param_count);
    for (k = 0; k < prog->funcs[i].param_count && ok; k++) ok = put_byte(s, TYPE_I64);
    ok = ok && put_uleb(s, 1) && put_byte(s, TYPE_I64);
  }
  for (i = 0; i < IR_IMPORT_COUNT && ok; i++) ok = ((s->imports >> i) & 1u) == 0 || import_type(s, (uint32_t)i);
  return ok;
}

/* The host imports that the entries call (S8, Q-S8-2 (a)): module permit.
   The type of an import comes after the entry types. */
static int part_imports(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  uint32_t i;
  int ok = put_uleb(s, import_index(s->imports, IR_IMPORT_COUNT));
  for (i = 0; i < IR_IMPORT_COUNT && ok; i++)
    ok = ((s->imports >> i) & 1u) == 0 || (put_name(s, "permit") && put_name(s, import_rows[i].text) && put_byte(s, 0) &&
                                            put_uleb(s, 1u + prog->func_count + import_index(s->imports, i)));
  return ok;
}

static int part_functions(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_uleb(s, 1u + prog->func_count);
  for (i = 0; i <= prog->func_count && ok; i++) ok = put_uleb(s, i);
  return ok;
}

/* One memory: minimum = maximum = the pages of the state region (S8) and
   the largest heap bound of an entry, at least 1. */
static int part_memory(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  uint64_t words = 0;
  uint64_t pages;
  size_t i;
  for (i = 0; i < prog->func_count; i++) words = prog->funcs[i].heap_words > words ? prog->funcs[i].heap_words : words;
  pages = (8u * (words + prog->region_words) + WASM_PAGE_BYTES - 1u) / WASM_PAGE_BYTES;
  pages = pages == 0 ? 1u : pages;
  return put_uleb(s, 1) && put_byte(s, 1) && put_uleb(s, pages) && put_uleb(s, pages);
}

static int part_global(Sink *s, const void *arg) {
  (void)arg;
  return put_uleb(s, 1) && put_byte(s, TYPE_I64) && put_byte(s, 1) && i64_const(s, 0) && put_byte(s, OP_END);
}

static int part_exports(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_uleb(s, prog->func_count + (prog->bind_count > 0 ? 1u : 0u));
  for (i = 0; i < prog->func_count && ok; i++) {
    ok = put_name(s, prog->funcs[i].name) && put_byte(s, 0) && put_uleb(s, import_index(s->imports, IR_IMPORT_COUNT) + 1u + i);
  }
  return ok && (prog->bind_count == 0 || (put_name(s, "memory") && put_byte(s, 2) && put_uleb(s, 0)));
}

static int part_code(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_uleb(s, 1u + prog->func_count) && sized(s, part_alloc, NULL);
  for (i = 0; i < prog->func_count && ok; i++) ok = sized(s, part_entry, &prog->funcs[i]);
  return ok;
}

/* The custom section permit.res (S8, Q-S8-3 (a)). */
static int part_res(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_name(s, "permit.res") && put_uleb(s, prog->res_count);
  for (i = 0; i < prog->res_count && ok; i++) ok = put_name(s, prog->res_paths[i]);
  return ok;
}

/* The custom section permit.bind (S8, Q-S8-4 (a)). */
static int part_bind(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_name(s, "permit.bind") && put_uleb(s, prog->bind_count);
  for (i = 0; i < prog->bind_count && ok; i++)
    ok = put_name(s, prog->binds[i].tool) && put_uleb(s, prog->binds[i].action) && put_name(s, prog->binds[i].pointer);
  return ok;
}

/* The data section (S8): one active segment of memory 0 at address 0, the
   initial words of the state region, little-endian. */
static int part_data(Sink *s, const void *arg) {
  const IrProgram *prog = arg;
  size_t i;
  int ok = put_uleb(s, 1) && put_uleb(s, 0) && i32_const(s, 0) && put_byte(s, OP_END) &&
           put_uleb(s, 8u * (uint64_t)prog->region_words);
  for (i = 0; i < 8u * (size_t)prog->region_words && ok; i++)
    ok = put_byte(s, (unsigned)((prog->region_init[i / 8u] >> (8u * (i % 8u))) & 0xFFu));
  return ok;
}

int target_abi_line(const IrFunc *fn, FILE *out, FILE *err) {
  (void)err;
  fprintf(out, "%s\n", fn->name);
  return 1;
}

/* Both parts are the module bytes. The data section of the state region
   and the custom sections permit.res and permit.bind come after the code
   section, each only when its table is not empty (S8). */
int target_write(const IrProgram *prog, TargetPart part, FILE *out, FILE *err) {
  static const unsigned char header[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  Sink s = {out, 0, err, 0, 8u * (uint64_t)prog->region_words};
  size_t i;
  int ok = 1;
  (void)part;
  for (i = 0; i < prog->func_count; i++) s.imports |= prog->funcs[i].imports;
  for (i = 0; i < sizeof header && ok; i++) ok = put_byte(&s, header[i]);
  ok = ok && section(&s, SECTION_TYPE, part_types, prog) &&
       (s.imports == 0 || section(&s, SECTION_IMPORT, part_imports, prog)) &&
       section(&s, SECTION_FUNCTION, part_functions, prog) &&
       section(&s, SECTION_MEMORY, part_memory, prog) && section(&s, SECTION_GLOBAL, part_global, NULL) &&
       section(&s, SECTION_EXPORT, part_exports, prog) && section(&s, SECTION_CODE, part_code, prog) &&
       (prog->region_words == 0 || section(&s, SECTION_DATA, part_data, prog)) &&
       (prog->res_count == 0 || section(&s, SECTION_CUSTOM, part_res, prog)) &&
       (prog->bind_count == 0 || section(&s, SECTION_CUSTOM, part_bind, prog));
  if (!ok && ferror(out)) fputs("langc: IO: -: cannot write the wasm module\n", err);
  return ok;
}
