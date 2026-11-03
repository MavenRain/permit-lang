/* The first-order IR between the front end and a target. SHARED by the tcc
   kits.

   Each value is one unsigned 64-bit word: a Nat, a Flag (0 or 1), a
   constructor tag, or a heap address. Locals are numbered from 0. The
   parameters of an entry are locals 0 to param_count - 1. A heap block is a
   run of words. A target sets its own word width in memory (Wasm: 8 bytes,
   EVM: 32 bytes) and resets its bump allocator at each call. An entry has a
   static heap bound (heap_words); the lowering refuses an entry without one.

   Each op is total (S3): no expression traps, and a value has no trap
   flag. ADD and MUL wrap; ADD_CARRY gives 1 on an overflow. A heap block is
   word 0 = the tag, then field K at word 1 + K. */
#ifndef LANG_IR_H
#define LANG_IR_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  IR_SCALAR_NAT,
  IR_SCALAR_FLAG
} IrScalar;

typedef enum {
  IR_OP_ADD,       /* the sum modulo 2^64 */
  IR_OP_SUB,       /* truncates at 0 */
  IR_OP_MUL,       /* the product modulo 2^64 */
  IR_OP_EQ,        /* 1 when equal, else 0 */
  IR_OP_LE,        /* 1 when left <= right, else 0 */
  IR_OP_ADD_CARRY, /* 1 when the sum is larger than 2^64 - 1, else 0 */
  IR_OP_DIV,       /* the quotient; the right operand is a NonZero (S3) */
  IR_OP_MOD        /* the remainder; the right operand is a NonZero (S3) */
} IrOp;

typedef enum {
  IR_EXPR_CONST,  /* value */
  IR_EXPR_LOCAL,  /* local */
  IR_EXPR_BINARY, /* op left right */
  IR_EXPR_LOAD,   /* word number field of the block at address left */
  IR_EXPR_CALL    /* host import number value of left and right (S8) */
} IrExprKind;

/* The host imports (S8, Q-S8-2 (a)): the functions of module permit, in
   import order. Each parameter and the result is an i64 word. X(name, text,
   params). within r g is the Flag word: 1 when g is r or an ancestor of r. */
#define IR_IMPORTS(X)                    \
  X(IR_IMPORT_ENVELOPE, "envelope", 2u) \
  X(IR_IMPORT_HASH, "hash", 1u)         \
  X(IR_IMPORT_WITHIN, "within", 2u)

#define IR_IMPORT_ENUM(name, text, params) name,
typedef enum { IR_IMPORTS(IR_IMPORT_ENUM) IR_IMPORT_COUNT } IrImport;
#undef IR_IMPORT_ENUM

typedef struct IrExpr IrExpr;
struct IrExpr {
  IrExprKind kind;
  uint64_t value;
  uint32_t local;
  IrOp op;
  uint32_t field;
  const IrExpr *left;
  const IrExpr *right;
};

typedef struct IrStmt IrStmt;

typedef struct {
  const IrStmt *const *items;
  size_t count;
} IrBlock;

typedef enum {
  IR_STMT_SET,    /* local := expr */
  IR_STMT_ALLOC,  /* local := address of a new block; word k := fields[k] */
  IR_STMT_IF,     /* expr != 0: body, else: otherwise */
  IR_STMT_SWITCH, /* run arms[expr]; a value outside 0 to arm_count - 1 runs the last arm */
  IR_STMT_REPEAT, /* counter := expr, then run body counter times */
  IR_STMT_WHILE,  /* counter := expr; run body while counter > 0 and local != 0,
                     counter decreasing by 1 at each run; body sets local */
  IR_STMT_RETURN, /* return expr */
  IR_STMT_STORE   /* word field of the block at address expr := fields[0] (S8) */
} IrStmtKind;

struct IrStmt {
  IrStmtKind kind;
  uint32_t local;
  uint32_t counter;
  const IrExpr *expr;
  const IrExpr *const *fields;
  size_t field_count;
  IrBlock body;
  IrBlock otherwise;
  const IrBlock *arms;
  size_t arm_count;
  uint32_t field; /* IR_STMT_STORE: the word number (S8) */
};

/* The largest heap bound: 1 GiB of 8-byte words, the arena limit of langc
   eval. */
#define IR_HEAP_WORDS_MAX ((uint64_t)1 << 27)

/* One exported entry. The body block ends in RETURN on each path. */
typedef struct {
  const char *name;
  const IrScalar *params;
  size_t param_count;
  IrScalar result;
  uint32_t local_count; /* includes the parameters */
  IrBlock body;
  uint64_t heap_words; /* the ALLOC words on the longest path (S3) */
  uint32_t imports;    /* bit K: the entry calls host import K (S8) */
} IrFunc;

/* One bind row (S8, Q-S8-4 (a)): the tool name, the arm number in Action
   and the RFC 6901 pointer. */
typedef struct {
  const char *tool;
  uint32_t action;
  const char *pointer;
} IrBind;

typedef struct {
  const IrFunc *funcs;
  size_t func_count;
  const char *const *res_paths; /* the path literals in program order (S8) */
  size_t res_count;
  const IrBind *binds; /* the bind rows in program order (S8) */
  size_t bind_count;
  uint32_t region_words;       /* the state region at address 0: R words (S8) */
  const uint64_t *region_init; /* the R initial words of the region (S8) */
} IrProgram;

#endif
