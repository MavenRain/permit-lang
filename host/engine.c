/* The host engine of permitd (S9 U1): relay-lang 4781ca7 host/engine.c
 * (file commit 8d4382a) with the import section, the data section, the host
 * import table, the kept memory export and a word write. See host/engine.h.
 * The engine reads the module with the reader of host/wasmread.c. It has no exit and no
 * abort: each failure is a result code and a message in E->message. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "wasmread.h"

/* The opcode list of src/wasmop.h as a named type. The switch of step has
 * a case for each opcode, so -Wswitch-enum (make check-clang) finds an
 * opcode with no case. */
#define ENGINE_OP(name, byte, imm) ENGINE_##name = byte,
typedef enum { WASM_OPS(ENGINE_OP) } EngineOp;
#undef ENGINE_OP

enum {
  BLOCK_EMPTY = 0x40, /* the block type with no value */
  VALUE_I32 = 0x7F,
  VALUE_I64 = 0x7E,
  TYPE_FORM = 0x60,
  PAGE_BYTES = 65536
};

/* Keeps WHY in the message of E. Gives RESULT. */
static EngineResult fail(Engine *e, EngineResult result, const char *why) {
  snprintf(e->message, sizeof e->message, "%s", why ? why : "bad module");
  return result;
}

/* Keeps WHY in the message of E. Gives 0 (a host error of a call). */
static int host(Engine *e, const char *why) {
  snprintf(e->message, sizeof e->message, "%s", why ? why : "bad code");
  return 0;
}

/* The low 32 bits: the value of an i32. */
static uint64_t low(uint64_t v) {
  return v & 0xFFFFFFFFu;
}

/* A count of at most MAX. */
static int get_count(Reader *r, uint64_t max, unsigned *n) {
  uint64_t v = 0;
  if (!get_leb(r, 32, &v))
    return 0;
  if (v > max)
    return read_fail(r, "count too large");
  *n = (unsigned)v;
  return 1;
}

/* A value type: i32 or i64. */
static int get_valtype(Reader *r) {
  unsigned t = 0;
  if (!get_byte(r, &t))
    return 0;
  if (t != VALUE_I32 && t != VALUE_I64)
    return read_fail(r, "value type");
  return 1;
}

/* Goes over N bytes. */
static int skip(Reader *r, uint64_t n) {
  if (n > r->end - r->pos)
    return read_fail(r, "truncated");
  r->pos += (size_t)n;
  return 1;
}

/* An allocation failed: the load gives ENGINE_HOST. */
static int no_memory(Engine *e, Reader *r) {
  e->no_memory = 1;
  return read_fail(r, "out of memory");
}

/* The type section: a form, the parameter types and the result types. */
static int load_types(Engine *e, Reader *r) {
  unsigned n = 0, i, j;
  if (!get_count(r, ENGINE_FUNC_MAX, &n))
    return 0;
  e->types = calloc(n ? n : 1, sizeof *e->types);
  if (!e->types)
    return no_memory(e, r);
  e->type_count = n;
  for (i = 0; i < n; i++) {
    unsigned form = 0, params = 0, results = 0;
    if (!get_byte(r, &form))
      return 0;
    if (form != TYPE_FORM)
      return read_fail(r, "type form");
    if (!get_count(r, ENGINE_PARAM_MAX, &params))
      return 0;
    for (j = 0; j < params; j++)
      if (!get_valtype(r))
        return 0;
    if (!get_count(r, 1, &results))
      return 0;
    for (j = 0; j < results; j++)
      if (!get_valtype(r))
        return 0;
    e->types[i].params = params;
    e->types[i].results = results;
  }
  return 1;
}

/* The import section: functions only. Each import gets the row of the
 * binding with its module and name, and the same parameter count. */
static int same(const Engine *e, size_t at, uint64_t len, const char *s) {
  return strlen(s) == len && memcmp(e->bytes + at, s, (size_t)len) == 0;
}

static int load_imports(Engine *e, Reader *r, const EngineBinding *b) {
  unsigned n = 0, i, j;
  if (!get_count(r, ENGINE_FUNC_MAX, &n))
    return 0;
  e->imports = calloc(n ? n : 1, sizeof *e->imports);
  if (!e->imports)
    return no_memory(e, r);
  for (i = 0; i < n; i++) {
    uint64_t mlen = 0, nlen = 0, type = 0;
    unsigned kind = 0;
    size_t mod, name;
    const EngineHostFunc *row = NULL;
    if (!get_leb(r, 32, &mlen))
      return 0;
    mod = r->pos;
    if (!skip(r, mlen) || !get_leb(r, 32, &nlen))
      return 0;
    name = r->pos;
    if (!skip(r, nlen) || !get_byte(r, &kind))
      return 0;
    if (kind != 0)
      return read_fail(r, "import kind");
    if (!get_leb(r, 32, &type))
      return 0;
    if (type >= e->type_count)
      return read_fail(r, "type index");
    for (j = 0; b && j < b->count && !row; j++)
      if (same(e, mod, mlen, b->funcs[j].module) && same(e, name, nlen, b->funcs[j].name))
        row = &b->funcs[j];
    if (!row)
      return read_fail(r, "unbound import");
    if (row->params != e->types[type].params || e->types[type].results != 1)
      return read_fail(r, "import type");
    e->imports[i] = row;
  }
  e->import_count = n;
  e->ctx = b ? b->ctx : NULL;
  return 1;
}

/* The function section: the type of each function. */
static int load_funcs(Engine *e, Reader *r) {
  unsigned n = 0, i;
  if (!get_count(r, ENGINE_FUNC_MAX, &n))
    return 0;
  e->funcs = calloc(n ? n : 1, sizeof *e->funcs);
  if (!e->funcs)
    return no_memory(e, r);
  e->func_count = n;
  for (i = 0; i < n; i++) {
    uint64_t type = 0;
    if (!get_leb(r, 32, &type))
      return 0;
    if (type >= e->type_count)
      return read_fail(r, "type index");
    e->funcs[i].type = (unsigned)type;
  }
  return 1;
}

/* The memory section: 0 or 1 memory. The memory is all zero. */
static int load_memory(Engine *e, Reader *r) {
  unsigned n = 0, flags = 0;
  uint64_t min = 0, max = 0;
  if (!get_count(r, 1, &n))
    return 0;
  if (n == 0)
    return 1;
  if (!get_byte(r, &flags) || !get_leb(r, 32, &min))
    return 0;
  if (flags > 1)
    return read_fail(r, "memory flags");
  if (flags == 1 && !get_leb(r, 32, &max))
    return 0;
  if (flags == 1 && max < min)
    return read_fail(r, "memory maximum");
  if (min > ENGINE_PAGE_MAX)
    return read_fail(r, "memory too large");
  e->memory_size = min * PAGE_BYTES;
  e->memory = calloc(min ? (size_t)e->memory_size : 1, 1);
  if (!e->memory)
    return no_memory(e, r);
  return 1;
}

/* The global section: each init is i64.const or i32.const, then end. */
static int load_globals(Engine *e, Reader *r) {
  unsigned n = 0, i;
  if (!get_count(r, ENGINE_GLOBAL_MAX, &n))
    return 0;
  for (i = 0; i < n; i++) {
    unsigned mut = 0, op = 0, end = 0;
    uint64_t v = 0;
    if (!get_valtype(r) || !get_byte(r, &mut) || !get_byte(r, &op))
      return 0;
    if (mut > 1)
      return read_fail(r, "global mutability");
    if (op != OP_I64_CONST && op != OP_I32_CONST)
      return read_fail(r, "global init");
    if (!get_sleb(r, op == OP_I64_CONST ? 64 : 32, &v) || !get_byte(r, &end))
      return 0;
    if (end != OP_END)
      return read_fail(r, "global init");
    e->globals[i] = op == OP_I64_CONST ? v : low(v);
  }
  e->global_count = n;
  return 1;
}

/* The export section. The engine keeps the exported functions only. */
static int load_exports(Engine *e, Reader *r) {
  unsigned n = 0, i;
  if (!get_count(r, ENGINE_EXPORT_MAX, &n))
    return 0;
  e->exports = calloc(n ? n : 1, sizeof *e->exports);
  if (!e->exports)
    return no_memory(e, r);
  for (i = 0; i < n; i++) {
    uint64_t len = 0, index = 0;
    unsigned kind = 0;
    size_t name;
    if (!get_leb(r, 32, &len))
      return 0;
    name = r->pos;
    if (!skip(r, len) || !get_byte(r, &kind) || !get_leb(r, 32, &index))
      return 0;
    if (kind > 3)
      return read_fail(r, "export kind");
    if (kind == 0 && index >= e->import_count + e->func_count)
      return read_fail(r, "export index");
    if (kind == 2 && (index != 0 || !e->memory))
      return read_fail(r, "export index");
    if (kind == 2)
      e->memory_export = 1;
    if (kind == 0) {
      EngineExport *x = &e->exports[e->export_count++];
      x->name = name;
      x->len = (size_t)len;
      x->func = (unsigned)index;
    }
  }
  return 1;
}

/* A custom section: the name, then the data to the end of the section. */
static int load_custom(Engine *e, Reader *r) {
  EngineCustom *c;
  uint64_t len = 0;
  if (e->custom_count == ENGINE_CUSTOM_MAX)
    return read_fail(r, "too many custom sections");
  c = &e->customs[e->custom_count];
  if (!get_leb(r, 32, &len))
    return 0;
  c->name = r->pos;
  c->len = (size_t)len;
  if (!skip(r, len))
    return 0;
  c->data = r->pos;
  c->size = r->end - r->pos;
  r->pos = r->end;
  e->custom_count++;
  return 1;
}

/* The scan of one body from R->pos to R->end: it fills the side table
 * MATCH. The value stack is empty at the load, so the scan keeps its block
 * stack (the offsets of the open block, loop, if and else opcodes) there.
 * Each block type must be empty (0x40). The last END must be the last byte
 * of the body. */
static int scan(Engine *e, Reader *r) {
  uint64_t *open = e->stack;
  size_t depth = 0;
  while (r->pos < r->end) {
    size_t at = r->pos;
    unsigned byte = 0, kind = 0;
    uint64_t first = 0;
    const Op *op;
    if (!get_byte(r, &byte))
      return 0;
    op = find_op(byte);
    if (!op)
      return read_fail(r, "opcode");
    switch (byte) {
    case OP_BLOCK:
    case OP_LOOP:
    case OP_IF:
      if (!get_byte(r, &kind))
        return 0;
      if (kind != BLOCK_EMPTY)
        return read_fail(r, "block type");
      if (depth == ENGINE_LABEL_MAX)
        return read_fail(r, "blocks too deep");
      open[depth++] = at;
      break;
    case OP_ELSE:
      if (depth == 0 || e->bytes[open[depth - 1]] != OP_IF)
        return read_fail(r, "else with no if");
      e->match[open[depth - 1]] = (uint32_t)at;
      open[depth - 1] = at;
      break;
    case OP_END:
      if (depth == 0 && r->pos != r->end)
        return read_fail(r, "code after the end");
      if (depth == 0)
        return 1;
      depth--;
      e->match[open[depth]] = (uint32_t)at;
      break;
    default:
      if (!immediate(r, op->imm, &first))
        return 0;
      break;
    }
  }
  return read_fail(r, "no end");
}

/* The code section: for each function, the local groups, then the code. */
static int load_code(Engine *e, Reader *r) {
  size_t outer = r->end;
  unsigned n = 0, i;
  if (!get_count(r, ENGINE_FUNC_MAX, &n))
    return 0;
  if (n != e->func_count)
    return read_fail(r, "code count");
  for (i = 0; i < n; i++) {
    EngineFunc *f = &e->funcs[i];
    uint64_t size = 0, locals = e->types[f->type].params;
    unsigned groups = 0, g;
    if (!get_leb(r, 32, &size))
      return 0;
    if (size > outer - r->pos)
      return read_fail(r, "truncated");
    r->end = r->pos + (size_t)size;
    if (!get_count(r, ENGINE_LOCAL_MAX, &groups))
      return 0;
    for (g = 0; g < groups; g++) {
      uint64_t k = 0;
      if (!get_leb(r, 32, &k) || !get_valtype(r))
        return 0;
      locals += k;
      if (locals > ENGINE_LOCAL_MAX)
        return read_fail(r, "too many locals");
    }
    f->locals = (unsigned)locals;
    f->code = r->pos;
    f->end = r->end;
    if (!scan(e, r))
      return 0;
    r->end = outer;
  }
  return 1;
}

/* The data section: each segment is active in memory 0 at the offset
 * `i32.const K; end`, and its bytes go into the memory at K. */
static int load_data(Engine *e, Reader *r) {
  unsigned n = 0, i;
  if (!get_count(r, ENGINE_DATA_MAX, &n))
    return 0;
  for (i = 0; i < n; i++) {
    unsigned flag = 0, op = 0, end = 0;
    uint64_t k = 0, len = 0;
    if (!get_byte(r, &flag) || !get_byte(r, &op))
      return 0;
    if (flag != 0 || op != OP_I32_CONST)
      return read_fail(r, "data segment");
    if (!get_sleb(r, 32, &k) || !get_byte(r, &end) || !get_leb(r, 32, &len))
      return 0;
    k = low(k);
    if (!e->memory)
      return read_fail(r, "data with no memory");
    if (end != OP_END)
      return read_fail(r, "data segment");
    if (k > e->memory_size || len > e->memory_size - k || len > r->end - r->pos)
      return read_fail(r, "data bounds");
    memcpy(e->memory + k, e->bytes + r->pos, (size_t)len);
    r->pos += (size_t)len;
  }
  return 1;
}

static int load_section(Engine *e, Reader *r, unsigned id, const EngineBinding *b) {
  switch (id) {
  case SECTION_CUSTOM:
    return load_custom(e, r);
  case SECTION_TYPE:
    return load_types(e, r);
  case SECTION_IMPORT:
    return load_imports(e, r, b);
  case SECTION_FUNCTION:
    return load_funcs(e, r);
  case SECTION_MEMORY:
    return load_memory(e, r);
  case SECTION_GLOBAL:
    return load_globals(e, r);
  case SECTION_EXPORT:
    return load_exports(e, r);
  case SECTION_CODE:
    return load_code(e, r);
  case SECTION_DATA:
    return load_data(e, r);
  default:
    return read_fail(r, "section id");
  }
}

/* The sections after the header. Each section that is not custom comes
 * after the section before it, in the order of the ids. */
static int load_sections(Engine *e, Reader *r, const EngineBinding *b) {
  unsigned last = 0;
  int code = 0;
  while (r->pos < r->end) {
    size_t outer = r->end;
    unsigned id = 0;
    uint64_t len = 0;
    if (!get_byte(r, &id) || !get_leb(r, 32, &len))
      return 0;
    if (len > r->end - r->pos)
      return read_fail(r, "truncated");
    if (id != SECTION_CUSTOM && id <= last)
      return read_fail(r, "section order");
    last = id == SECTION_CUSTOM ? last : id;
    code = code || id == SECTION_CODE;
    r->end = r->pos + (size_t)len;
    if (!load_section(e, r, id, b))
      return 0;
    if (r->pos != r->end)
      return read_fail(r, "section size");
    r->end = outer;
  }
  if (e->func_count != 0 && !code)
    return read_fail(r, "no code section");
  return 1;
}

EngineResult engine_load(Engine *e, const unsigned char *bytes, size_t size, const EngineBinding *binding) {
  static const unsigned char header[8] = {0, 'a', 's', 'm', 1, 0, 0, 0};
  EngineResult result;
  Reader r;
  memset(e, 0, sizeof *e);
  if (size > WASM_MODULE_MAX)
    return fail(e, ENGINE_REFUSE, "module too large");
  memset(&r, 0, sizeof r);
  r.bytes = bytes;
  r.end = size;
  if (!wasm_check(&r))
    return fail(e, ENGINE_REFUSE, r.why);
  if (size < sizeof header || memcmp(bytes, header, sizeof header) != 0)
    return fail(e, ENGINE_REFUSE, "header");
  e->bytes = malloc(size);
  e->size = size;
  e->match = calloc(size, sizeof *e->match);
  e->stack = malloc(ENGINE_STACK_MAX * sizeof *e->stack);
  e->labels = malloc(ENGINE_LABEL_MAX * sizeof *e->labels);
  e->locals = malloc(ENGINE_LOCAL_MAX * sizeof *e->locals);
  if (!e->bytes || !e->match || !e->stack || !e->labels || !e->locals) {
    engine_free(e);
    return fail(e, ENGINE_HOST, "out of memory");
  }
  memcpy(e->bytes, bytes, size);
  memset(&r, 0, sizeof r);
  r.bytes = e->bytes;
  r.pos = sizeof header;
  r.end = size;
  if (load_sections(e, &r, binding))
    return ENGINE_OK;
  result = e->no_memory ? ENGINE_HOST : ENGINE_REFUSE;
  fail(e, result, r.why);
  engine_free(e);
  return result;
}

void engine_free(Engine *e) {
  char keep[ENGINE_MESSAGE_MAX];
  memcpy(keep, e->message, sizeof keep);
  free(e->bytes);
  free(e->match);
  free(e->types);
  free(e->imports);
  free(e->funcs);
  free(e->exports);
  free(e->memory);
  free(e->stack);
  free(e->labels);
  free(e->locals);
  memset(e, 0, sizeof *e);
  memcpy(e->message, keep, sizeof keep);
}

/* One frame of a call: the value stack height and the label stack height at
 * the start of the body, the locals, and the call depth. */
typedef struct {
  size_t base;
  size_t lbase;
  uint64_t *locals;
  unsigned count;
  unsigned depth;
} Frame;

static EngineResult run(Engine *e, unsigned func, unsigned depth, size_t bottom);

/* Calls the import V: its parameters come off the value stack of FR, and
 * its result goes on it. */
static int call_host(Engine *e, const Frame *fr, unsigned v) {
  const EngineHostFunc *f = e->imports[v];
  uint64_t result = 0;
  if (e->sp - fr->base < f->params)
    return host(e, "value stack underflow");
  e->sp -= f->params;
  if (!f->fn(e->ctx, e->stack + e->sp, &result))
    return host(e, "host import");
  e->stack[e->sp++] = result;
  return 1;
}

/* Gives 1 when OK, else a host error with the reason of R. */
static int got(Engine *e, const Reader *r, int ok) {
  return ok ? 1 : host(e, r->why);
}

static int push(Engine *e, uint64_t v) {
  if (e->sp == ENGINE_STACK_MAX)
    return host(e, "value stack bound");
  e->stack[e->sp++] = v;
  return 1;
}

static int pop(Engine *e, const Frame *fr, uint64_t *v) {
  if (e->sp <= fr->base)
    return host(e, "value stack underflow");
  *v = e->stack[--e->sp];
  return 1;
}

/* Pops B, then A (A was pushed first). */
static int pop2(Engine *e, const Frame *fr, uint64_t *a, uint64_t *b) {
  return pop(e, fr, b) && pop(e, fr, a);
}

/* Pushes a label. A branch to it continues at CONT with the value stack at
 * the height of now. */
static int label(Engine *e, size_t cont, int loop) {
  EngineLabel *l;
  if (e->lb == ENGINE_LABEL_MAX)
    return host(e, "label stack bound");
  l = &e->labels[e->lb++];
  l->height = e->sp;
  l->cont = cont;
  l->loop = loop;
  return 1;
}

/* A branch to the label L of the frame FR. Gives 2 for the label of the
 * body (a return), 1 for a label in the body, 0 for a host error. */
static int branch(Engine *e, Reader *r, const Frame *fr, uint64_t l) {
  size_t count = e->lb - fr->lbase, at;
  EngineLabel to;
  if (l == count)
    return 2;
  if (l > count)
    return host(e, "label index");
  at = e->lb - 1 - (size_t)l;
  to = e->labels[at];
  if (e->sp < to.height)
    return host(e, "value stack underflow");
  e->sp = to.height;
  e->lb = to.loop ? at + 1 : at;
  r->pos = to.cont;
  return 1;
}

/* 1 when the 8 bytes at ADDRESS are in the memory. */
static int in_memory(const Engine *e, uint64_t address) {
  return address <= e->memory_size && e->memory_size - address >= 8;
}

static uint64_t load_word(const Engine *e, uint64_t address) {
  uint64_t v = 0;
  unsigned i;
  for (i = 0; i < 8; i++)
    v |= (uint64_t)e->memory[address + i] << (8 * i);
  return v;
}

static void store_word(Engine *e, uint64_t address, uint64_t v) {
  unsigned i;
  for (i = 0; i < 8; i++)
    e->memory[address + i] = (unsigned char)(v >> (8 * i));
}

/* A memarg (the alignment, then the offset) and the address on the value
 * stack. *ADDRESS gets the address plus the offset. */
static int memarg(Engine *e, Reader *r, const Frame *fr, uint64_t *address) {
  uint64_t align = 0, offset = 0, base = 0;
  if (!got(e, r, get_leb(r, 32, &align) && get_leb(r, 32, &offset)) || !pop(e, fr, &base))
    return 0;
  *address = low(base) + offset;
  return 1;
}

/* A local index of the frame FR. */
static int local(Engine *e, Reader *r, const Frame *fr, uint64_t *k) {
  if (!got(e, r, get_leb(r, 32, k)))
    return 0;
  if (*k >= fr->count)
    return host(e, "local index");
  return 1;
}

/* A global index. */
static int global(Engine *e, Reader *r, uint64_t *g) {
  if (!got(e, r, get_leb(r, 32, g)))
    return 0;
  if (*g >= e->global_count)
    return host(e, "global index");
  return 1;
}

/* Runs the instruction at R->pos. Gives 1 to go on, 2 at the end of the
 * body (a return), 0 for a host error. */
static int step(Engine *e, Reader *r, const Frame *fr) {
  size_t at = r->pos, m = 0, end = 0;
  unsigned byte = 0, kind = 0;
  uint64_t a = 0, b = 0, c = 0, v = 0;
  if (!got(e, r, get_byte(r, &byte)))
    return 0;
  if (!find_op(byte))
    return host(e, "opcode");
  switch ((EngineOp)byte) {
  case ENGINE_OP_BLOCK:
    return got(e, r, get_byte(r, &kind)) && label(e, (size_t)e->match[at] + 1, 0);
  case ENGINE_OP_LOOP:
    return got(e, r, get_byte(r, &kind)) && label(e, r->pos, 1);
  case ENGINE_OP_IF:
    if (!got(e, r, get_byte(r, &kind)) || !pop(e, fr, &c))
      return 0;
    m = e->match[at];
    end = e->bytes[m] == OP_ELSE ? e->match[m] : m;
    if (low(c) != 0)
      return label(e, end + 1, 0);
    r->pos = m + 1;
    return e->bytes[m] == OP_ELSE ? label(e, end + 1, 0) : 1;
  case ENGINE_OP_ELSE:
    r->pos = e->match[at];
    return 1;
  case ENGINE_OP_END:
    if (e->lb == fr->lbase)
      return 2;
    e->lb--;
    return 1;
  case ENGINE_OP_BR:
    if (!got(e, r, get_leb(r, 32, &v)))
      return 0;
    return branch(e, r, fr, v);
  case ENGINE_OP_BR_IF:
    if (!got(e, r, get_leb(r, 32, &v)) || !pop(e, fr, &c))
      return 0;
    if (low(c) == 0)
      return 1;
    return branch(e, r, fr, v);
  case ENGINE_OP_BR_TABLE:
    /* The labels 0 to A - 1, then the default label at A. */
    if (!got(e, r, get_leb(r, 32, &a)) || !pop(e, fr, &c))
      return 0;
    c = low(c);
    for (b = 0; b <= a; b++) {
      if (!got(e, r, get_leb(r, 32, &v)))
        return 0;
      if (b == c || b == a)
        return branch(e, r, fr, v);
    }
    return host(e, "br_table");
  case ENGINE_OP_RETURN:
    return 2;
  case ENGINE_OP_CALL:
    if (!got(e, r, get_leb(r, 32, &v)))
      return 0;
    if (v >= e->import_count + e->func_count)
      return host(e, "function index");
    if (v < e->import_count)
      return call_host(e, fr, (unsigned)v);
    return run(e, (unsigned)v - e->import_count, fr->depth + 1, fr->base) == ENGINE_OK;
  case ENGINE_OP_SELECT:
    if (!pop(e, fr, &c) || !pop2(e, fr, &a, &b))
      return 0;
    return push(e, low(c) != 0 ? a : b);
  case ENGINE_OP_LOCAL_GET:
    return local(e, r, fr, &v) && push(e, fr->locals[v]);
  case ENGINE_OP_LOCAL_SET:
    return local(e, r, fr, &v) && pop(e, fr, &fr->locals[v]);
  case ENGINE_OP_LOCAL_TEE:
    if (!local(e, r, fr, &v))
      return 0;
    if (e->sp <= fr->base)
      return host(e, "value stack underflow");
    fr->locals[v] = e->stack[e->sp - 1];
    return 1;
  case ENGINE_OP_GLOBAL_GET:
    return global(e, r, &v) && push(e, e->globals[v]);
  case ENGINE_OP_GLOBAL_SET:
    return global(e, r, &v) && pop(e, fr, &e->globals[v]);
  case ENGINE_OP_I64_LOAD:
    if (!memarg(e, r, fr, &a))
      return 0;
    if (!in_memory(e, a))
      return host(e, "load out of bounds");
    return push(e, load_word(e, a));
  case ENGINE_OP_I64_STORE:
    if (!pop(e, fr, &v) || !memarg(e, r, fr, &a))
      return 0;
    if (!in_memory(e, a))
      return host(e, "store out of bounds");
    store_word(e, a, v);
    return 1;
  case ENGINE_OP_I32_CONST:
    return got(e, r, get_sleb(r, 32, &v)) && push(e, low(v));
  case ENGINE_OP_I64_CONST:
    return got(e, r, get_sleb(r, 64, &v)) && push(e, v);
  case ENGINE_OP_I64_EQZ:
    return pop(e, fr, &a) && push(e, a == 0);
  case ENGINE_OP_I64_EQ:
    return pop2(e, fr, &a, &b) && push(e, a == b);
  case ENGINE_OP_I64_LT_U:
    return pop2(e, fr, &a, &b) && push(e, a < b);
  case ENGINE_OP_I64_LE_U:
    return pop2(e, fr, &a, &b) && push(e, a <= b);
  case ENGINE_OP_I64_GE_U:
    return pop2(e, fr, &a, &b) && push(e, a >= b);
  case ENGINE_OP_I64_ADD:
    return pop2(e, fr, &a, &b) && push(e, a + b);
  case ENGINE_OP_I64_SUB:
    return pop2(e, fr, &a, &b) && push(e, a - b);
  case ENGINE_OP_I64_MUL:
    return pop2(e, fr, &a, &b) && push(e, a * b);
  case ENGINE_OP_I64_DIV_U:
    if (!pop2(e, fr, &a, &b))
      return 0;
    if (b == 0)
      return host(e, "divide by zero");
    return push(e, a / b);
  case ENGINE_OP_I64_REM_U:
    if (!pop2(e, fr, &a, &b))
      return 0;
    if (b == 0)
      return host(e, "divide by zero");
    return push(e, a % b);
  case ENGINE_OP_I64_OR:
    return pop2(e, fr, &a, &b) && push(e, a | b);
  case ENGINE_OP_I32_WRAP_I64:
  case ENGINE_OP_I64_EXTEND_I32_U:
    return pop(e, fr, &a) && push(e, low(a));
  }
  return host(e, "opcode");
}

/* Calls the function FUNC: its parameters are on the value stack, above
 * BOTTOM (the base of the caller). At the end, the result replaces them. */
static EngineResult run(Engine *e, unsigned func, unsigned depth, size_t bottom) {
  const EngineFunc *f = &e->funcs[func];
  const EngineType *t = &e->types[f->type];
  Frame fr;
  Reader r;
  int go = 1;
  uint64_t v = 0;
  if (depth > ENGINE_DEPTH_MAX)
    return fail(e, ENGINE_HOST, "call depth bound");
  if (e->sp - bottom < t->params)
    return fail(e, ENGINE_HOST, "value stack underflow");
  if (f->locals > ENGINE_LOCAL_MAX - e->lp)
    return fail(e, ENGINE_HOST, "locals bound");
  e->sp -= t->params;
  fr.base = e->sp;
  fr.lbase = e->lb;
  fr.locals = e->locals + e->lp;
  fr.count = f->locals;
  fr.depth = depth;
  memcpy(fr.locals, e->stack + e->sp, t->params * sizeof *fr.locals);
  memset(fr.locals + t->params, 0, (f->locals - t->params) * sizeof *fr.locals);
  e->lp += f->locals;
  memset(&r, 0, sizeof r);
  r.bytes = e->bytes;
  r.pos = f->code;
  r.end = f->end;
  while (go == 1)
    go = step(e, &r, &fr);
  e->lp -= f->locals;
  e->lb = fr.lbase;
  if (go == 0)
    return ENGINE_HOST;
  if (t->results == 0) {
    e->sp = fr.base;
    return ENGINE_OK;
  }
  if (e->sp <= fr.base)
    return fail(e, ENGINE_HOST, "no result");
  v = e->stack[e->sp - 1];
  e->stack[fr.base] = v;
  e->sp = fr.base + 1;
  return ENGINE_OK;
}

EngineResult engine_call(Engine *e, unsigned func, const uint64_t *args, unsigned count, uint64_t *result) {
  EngineResult res;
  if (func < e->import_count || func >= e->import_count + e->func_count)
    return fail(e, ENGINE_REFUSE, "function index");
  if (count != engine_params(e, func))
    return fail(e, ENGINE_REFUSE, "argument count");
  e->lb = 0;
  e->lp = 0;
  if (count != 0)
    memcpy(e->stack, args, count * sizeof *args);
  e->sp = count;
  res = run(e, func - e->import_count, 1, 0);
  if (res != ENGINE_OK)
    return res;
  *result = e->types[e->funcs[func - e->import_count].type].results != 0 ? e->stack[0] : 0;
  return ENGINE_OK;
}

int engine_find(const Engine *e, const char *name, unsigned *func) {
  size_t len = strlen(name);
  unsigned i;
  for (i = 0; i < e->export_count; i++) {
    const EngineExport *x = &e->exports[i];
    if (x->len == len && memcmp(e->bytes + x->name, name, len) == 0) {
      *func = x->func;
      return 1;
    }
  }
  return 0;
}

unsigned engine_params(const Engine *e, unsigned func) {
  return func >= e->import_count && func < e->import_count + e->func_count
             ? e->types[e->funcs[func - e->import_count].type].params
             : 0;
}

int engine_word(const Engine *e, uint64_t address, uint64_t *word) {
  if (!e->memory_export || !in_memory(e, address))
    return 0;
  *word = load_word(e, address);
  return 1;
}

int engine_put_word(Engine *e, uint64_t address, uint64_t word) {
  if (!e->memory_export || !in_memory(e, address))
    return 0;
  store_word(e, address, word);
  return 1;
}

int engine_section(const Engine *e, const char *name, const unsigned char **data, size_t *size) {
  size_t len = strlen(name);
  unsigned i;
  for (i = 0; i < e->custom_count; i++) {
    const EngineCustom *c = &e->customs[i];
    if (c->len == len && memcmp(e->bytes + c->name, name, len) == 0) {
      *data = e->bytes + c->data;
      *size = c->size;
      return 1;
    }
  }
  return 0;
}

const char *engine_message(const Engine *e) {
  return e->message;
}
