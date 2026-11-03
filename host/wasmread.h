/* permitd copy (S9 U1, Q-S9-1 (a)): relay-lang 4781ca7 src/wasmread.h
 * (file commit 8d4382a), with the import (2) and data (11) section ids.
 *
 * The shared reader of a Wasm module of the writer (src/wasm.c), S8 P2a
 * (rulings Q-S8-2 (a) and Q-S8-3 (a)). test/notraptool.c and the host engine
 * use it. A Reader reads the bytes from POS to END. Each read checks the
 * size first, so a short module fails with `truncated`. At a failure, the
 * function keeps the reason in WHY (only the first reason) and gives 0.
 * wasm_check is the no-trap check of S7 P5b (src/wasmread.c). */
#ifndef LANG_WASMREAD_H
#define LANG_WASMREAD_H

#include <stddef.h>
#include <stdint.h>

#include "wasmop.h"

#define WASMREAD_ENUM(name, byte, imm) name = byte,
enum { WASM_OPS(WASMREAD_ENUM) };
#undef WASMREAD_ENUM

enum {
  WASM_MODULE_MAX = 1 << 20, /* the largest module: 1 MiB */
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

/* One row of the list: the opcode byte and the kind of its immediate. */
typedef struct {
  unsigned byte;
  int imm;
} Op;

/* The module bytes from POS to END, and the reason of the first failure. */
typedef struct {
  const unsigned char *bytes;
  size_t pos;
  size_t end;
  const char *why;
  char text[48];
} Reader;

/* Keeps WHY when R has no reason yet. Gives 0. */
int read_fail(Reader *r, const char *why);

/* One byte. */
int get_byte(Reader *r, unsigned *v);

/* A LEB128 of BITS bits or less. For a signed LEB128, get_leb keeps the
 * bits and does not extend the sign. */
int get_leb(Reader *r, unsigned bits, uint64_t *v);

/* A signed LEB128 of BITS bits or less. The sign extends to 64 bits (the
 * immediate of i32.const and i64.const). */
int get_sleb(Reader *r, unsigned bits, uint64_t *v);

/* The row of the opcode BYTE in the list, or NULL. */
const Op *find_op(unsigned byte);

/* Reads an immediate of the kind KIND. *FIRST gets its first number. */
int immediate(Reader *r, int kind, uint64_t *first);

/* The no-trap check of the module from R->bytes to R->end. Gives 1 when the
 * module is good. At the first failure, gives 0 and keeps the reason in
 * R->why. */
int wasm_check(Reader *r);

#endif
