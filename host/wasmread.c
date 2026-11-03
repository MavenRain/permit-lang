/* permitd copy (S9 U1, Q-S9-1 (a)): relay-lang 4781ca7 src/wasmread.c
 * (file commit 8d4382a), with no change to the check.
 *
 * The shared reader and the no-trap check of a module of the writer
 * (src/wasm.c). S8 P2a moved them from test/notraptool.c (S7 P5b, rulings
 * Q-S7-12 (a), Q-S7-26 (a) and Q-S7-30 (a)). See src/wasmread.h.
 *
 * wasm_check reads the header (the magic and version 1), then each section
 * by its id and size. Memory section: each memory must have a maximum, and
 * the maximum must be equal to the minimum. Code section: the check skips
 * the locals of each body, then decodes each instruction with the list of
 * src/wasmop.h. A body must not have `unreachable`, `memory.grow`, a byte
 * that is not in the list, or an i64.div_u or i64.rem_u that does not come
 * after the guard of divisor() in src/wasm.c. */
#include <stdio.h>
#include <string.h>

#include "wasmread.h"

enum { GUARD_LEN = 5 }; /* the instructions of the divisor guard */

#define WASMREAD_ROW(name, byte, imm) {byte, imm},
static const Op ops[] = {WASM_OPS(WASMREAD_ROW)};
#undef WASMREAD_ROW

/* A decoded instruction: the opcode and the first number of its immediate
 * (0 when it has no immediate). */
typedef struct {
  unsigned op;
  uint64_t imm;
} Insn;

int read_fail(Reader *r, const char *why) {
  if (r->why == NULL) r->why = why;
  return 0;
}

int get_byte(Reader *r, unsigned *v) {
  if (r->pos >= r->end) return read_fail(r, "truncated");
  *v = r->bytes[r->pos++];
  return 1;
}

int get_leb(Reader *r, unsigned bits, uint64_t *v) {
  unsigned shift;
  unsigned b = 0x80u;
  int ok = 1;
  *v = 0;
  for (shift = 0; ok && (b & 0x80u) != 0; shift += 7) {
    ok = (shift < bits || read_fail(r, "LEB128 too long")) && get_byte(r, &b);
    *v |= ok ? (uint64_t)(b & 0x7Fu) << shift : 0;
  }
  return ok;
}

/* After the last byte, SHIFT is the number of bits read. When bit 6 of the
 * last byte is 1, the value is negative: the bits above SHIFT become 1. */
int get_sleb(Reader *r, unsigned bits, uint64_t *v) {
  unsigned shift;
  unsigned b = 0x80u;
  int ok = 1;
  *v = 0;
  for (shift = 0; ok && (b & 0x80u) != 0; shift += 7) {
    ok = (shift < bits || read_fail(r, "LEB128 too long")) && get_byte(r, &b);
    *v |= ok ? (uint64_t)(b & 0x7Fu) << shift : 0;
  }
  if (ok && shift < 64u && (b & 0x40u) != 0) *v |= ~(uint64_t)0 << shift;
  return ok;
}

const Op *find_op(unsigned byte) {
  size_t k;
  for (k = 0; k < sizeof ops / sizeof ops[0]; k++)
    if (ops[k].byte == byte) return &ops[k];
  return NULL;
}

int immediate(Reader *r, int kind, uint64_t *first) {
  uint64_t n = 0;
  uint64_t k;
  uint64_t label;
  int ok;
  switch (kind) {
    case WASM_IMM_NONE:
      return 1;
    case WASM_IMM_BLOCKTYPE: /* a type byte, or a type index (33 bits) */
      return get_leb(r, 33, first);
    case WASM_IMM_LABEL:
    case WASM_IMM_INDEX:
      return get_leb(r, 32, first);
    case WASM_IMM_LABELS: /* N, then N labels and the default label */
      ok = get_leb(r, 32, &n);
      for (k = 0; k <= n && ok; k++) ok = get_leb(r, 32, &label);
      return ok;
    case WASM_IMM_MEMARG:
      return get_leb(r, 32, first) && get_leb(r, 32, &label);
    case WASM_IMM_I32:
      return get_leb(r, 32, first);
    case WASM_IMM_I64:
      return get_leb(r, 64, first);
  }
  return read_fail(r, "an immediate kind outside the list");
}

/* The guard of divisor() in src/wasm.c: local.get Y, local.get Y, i64.eqz,
 * i64.extend_i32_u, i64.or leave Y | (Y = 0), which is never 0. LAST holds
 * the GUARD_LEN instructions before the division, the oldest first. */
static int guarded(const Insn *last) {
  return last[0].op == OP_LOCAL_GET && last[1].op == OP_LOCAL_GET && last[1].imm == last[0].imm &&
         last[2].op == OP_I64_EQZ && last[3].op == OP_I64_EXTEND_I32_U && last[4].op == OP_I64_OR;
}

/* Decodes one instruction, then adds it to LAST. */
static int check_insn(Reader *r, Insn *last) {
  Insn cur = {0, 0};
  const Op *row;
  int ok = get_byte(r, &cur.op);
  if (!ok) return 0;
  if (cur.op == 0x00u) return read_fail(r, "unreachable");
  if (cur.op == 0x40u) return read_fail(r, "memory.grow");
  row = find_op(cur.op);
  if (row == NULL) {
    snprintf(r->text, sizeof r->text, "opcode 0x%02X outside the list", cur.op);
    return read_fail(r, r->text);
  }
  if ((cur.op == OP_I64_DIV_U || cur.op == OP_I64_REM_U) && !guarded(last))
    return read_fail(r, "div_u or rem_u with no guard");
  ok = immediate(r, row->imm, &cur.imm);
  memmove(last, last + 1, (GUARD_LEN - 1) * sizeof *last);
  last[GUARD_LEN - 1] = cur;
  return ok;
}

/* One function body: its size, the locals (a count, then each group: a
 * count and a type), then the instructions. */
static int check_body(Reader *r) {
  Insn last[GUARD_LEN] = {{0, 0}};
  size_t outer = r->end;
  uint64_t size = 0;
  uint64_t n = 0;
  uint64_t i;
  uint64_t count;
  unsigned type;
  int ok = get_leb(r, 32, &size) && (size <= r->end - r->pos || read_fail(r, "truncated"));
  if (!ok) return 0;
  r->end = r->pos + (size_t)size;
  ok = get_leb(r, 32, &n);
  for (i = 0; i < n && ok; i++) ok = get_leb(r, 32, &count) && get_byte(r, &type);
  while (ok && r->pos < r->end) ok = check_insn(r, last);
  r->end = outer;
  return ok;
}

static int check_code(Reader *r) {
  uint64_t n = 0;
  uint64_t i;
  int ok = get_leb(r, 32, &n);
  for (i = 0; i < n && ok; i++) ok = check_body(r);
  return ok;
}

/* One memory: the flags (1: a minimum and a maximum), then the limits. */
static int check_limits(Reader *r) {
  unsigned flags = 0;
  uint64_t min = 0;
  uint64_t max = 0;
  int ok = get_byte(r, &flags) && get_leb(r, 32, &min);
  if (!ok) return 0;
  if (flags == 0x00u) return read_fail(r, "memory with no max");
  if (flags != 0x01u) {
    snprintf(r->text, sizeof r->text, "memory flags 0x%02X", flags);
    return read_fail(r, r->text);
  }
  return get_leb(r, 32, &max) && (max == min || read_fail(r, "memory max is not equal to min"));
}

static int check_memory(Reader *r) {
  uint64_t n = 0;
  uint64_t i;
  int ok = get_leb(r, 32, &n);
  for (i = 0; i < n && ok; i++) ok = check_limits(r);
  return ok;
}

/* One section: its id, its size, then its contents. The check reads the
 * memory section and the code section, and skips the other sections. */
static int check_section(Reader *r) {
  size_t outer = r->end;
  unsigned id = 0;
  uint64_t size = 0;
  int ok = get_byte(r, &id) && get_leb(r, 32, &size) && (size <= r->end - r->pos || read_fail(r, "truncated"));
  if (!ok) return 0;
  r->end = r->pos + (size_t)size;
  switch (id) {
    case SECTION_MEMORY:
      ok = check_memory(r);
      break;
    case SECTION_CODE:
      ok = check_code(r);
      break;
    default:
      r->pos = r->end;
      break;
  }
  ok = ok && (r->pos == r->end || read_fail(r, "a section size does not match its contents"));
  r->end = outer;
  return ok;
}

int wasm_check(Reader *r) {
  static const unsigned char header[8] = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};
  int ok = (r->end >= sizeof header || read_fail(r, "truncated")) &&
           (memcmp(r->bytes, header, sizeof header) == 0 || read_fail(r, "no wasm magic and version 1"));
  r->pos = ok ? sizeof header : 0;
  while (ok && r->pos < r->end) ok = check_section(r);
  return ok;
}
