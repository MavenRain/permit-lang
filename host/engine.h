/* The host engine of permitd, S9 U1 (Q-S9-1 (a)): relay-lang 4781ca7
 * host/engine.h (file commit 8d4382a) with the host import table
 * (EngineBinding), the data segments, the kept memory export and
 * engine_put_word.
 *
 * relay-lang text: the host engine of relayd, S8 P2b (rulings Q-S8-5 (a), Q-S8-17 (a) and
 * Q-S8-18 (a)). The engine loads a Wasm module of the writer (src/wasm.c)
 * and runs its entries. First, engine_load does the no-trap check of
 * src/wasmread.c (wasm_check): a module that is not good is a refusal. The
 * engine has a value stack bound and a call depth bound, and no step fuel.
 * It does not stop the process: each failure is a result code and a
 * message (engine_message). */
#ifndef PERMIT_ENGINE_H
#define PERMIT_ENGINE_H

#include <stddef.h>
#include <stdint.h>

enum {
  ENGINE_FUNC_MAX = 1 << 14,  /* the functions and the types of a module */
  ENGINE_EXPORT_MAX = 1 << 15,
  ENGINE_PARAM_MAX = 256,     /* the parameters of a function */
  ENGINE_LOCAL_MAX = 1 << 16, /* the locals of all the frames */
  ENGINE_GLOBAL_MAX = 16,
  ENGINE_CUSTOM_MAX = 8,      /* the custom sections */
  ENGINE_DATA_MAX = 16,       /* the data segments */
  ENGINE_PAGE_MAX = 256,      /* the memory bound: 256 pages (16 MiB) */
  ENGINE_STACK_MAX = 1 << 16, /* the value stack bound */
  ENGINE_LABEL_MAX = 1 << 14, /* the label stack bound */
  ENGINE_DEPTH_MAX = 256,     /* the call depth bound */
  ENGINE_MESSAGE_MAX = 160
};

/* The result of a load or a call. In relay-lang, relayd gives exit 1 for
 * ENGINE_REFUSE and exit 75 for ENGINE_HOST. */
typedef enum {
  ENGINE_OK,
  ENGINE_REFUSE, /* the module or the call is not good */
  ENGINE_HOST    /* a host error: a bound, a division by 0, no memory */
} EngineResult;

/* A host function: the module and the name of its import, the count of
 * its parameters (each an i64; the result is one i64), and FN. FN gives 1
 * and *RESULT, or 0 for a host error of the call. */
typedef struct {
  const char *module;
  const char *name;
  unsigned params;
  int (*fn)(void *ctx, const uint64_t *args, uint64_t *result);
} EngineHostFunc;

/* The host import table of a load: COUNT rows, and CTX for each FN. */
typedef struct {
  const EngineHostFunc *funcs;
  unsigned count;
  void *ctx;
} EngineBinding;

/* A function type: the count of the parameters and of the results (0 or
 * 1). */
typedef struct {
  unsigned params;
  unsigned results;
} EngineType;

/* A function: its type, its locals (with the parameters), and its code from
 * CODE to END (END is one byte after its last END opcode). */
typedef struct {
  unsigned type;
  unsigned locals;
  size_t code;
  size_t end;
} EngineFunc;

/* An exported function. Its name is LEN bytes at NAME in the module. */
typedef struct {
  size_t name;
  size_t len;
  unsigned func;
} EngineExport;

/* A custom section: the name (LEN bytes at NAME) and the data (SIZE bytes
 * at DATA) in the module. */
typedef struct {
  size_t name;
  size_t len;
  size_t data;
  size_t size;
} EngineCustom;

/* A label. A branch to it sets the value stack to HEIGHT and continues at
 * CONT. A branch to a loop label keeps the label. */
typedef struct {
  size_t height;
  size_t cont;
  int loop;
} EngineLabel;

/* A module and its state. MATCH is the side table of the load. For the
 * offset of each block, loop and if, it has the offset of the END (for an
 * if with an else, the offset of the ELSE). For the offset of each ELSE, it
 * has the offset of the END. */
typedef struct {
  unsigned char *bytes;
  size_t size;
  uint32_t *match;
  EngineType *types;
  unsigned type_count;
  const EngineHostFunc **imports;
  unsigned import_count;
  void *ctx;
  EngineFunc *funcs;
  unsigned func_count;
  EngineExport *exports;
  unsigned export_count;
  EngineCustom customs[ENGINE_CUSTOM_MAX];
  unsigned custom_count;
  uint64_t globals[ENGINE_GLOBAL_MAX];
  unsigned global_count;
  unsigned char *memory;
  uint64_t memory_size;
  int memory_export;
  uint64_t *stack;
  size_t sp;
  EngineLabel *labels;
  size_t lb;
  uint64_t *locals;
  size_t lp;
  int no_memory;
  char message[ENGINE_MESSAGE_MAX];
} Engine;

/* Loads the SIZE bytes of a module into E (the engine keeps a copy). Each
 * import of the module gets the row of BINDING (NULL: no rows) with its
 * module and name; an import with no row, or with a row of an other type,
 * is a refusal. The data segments go into the memory. Gives ENGINE_OK,
 * ENGINE_REFUSE for a module that is not good, or ENGINE_HOST when there is
 * no memory. After a failure, E has no tables. */
EngineResult engine_load(Engine *e, const unsigned char *bytes, size_t size, const EngineBinding *binding);

/* Gives 1 and the function index in *FUNC when NAME is an exported
 * function, else 0. The index counts the imports first. */
int engine_find(const Engine *e, const char *name, unsigned *func);

/* The count of the parameters of the function FUNC. */
unsigned engine_params(const Engine *e, unsigned func);

/* Calls the function FUNC with the COUNT words of ARGS. *RESULT gets the
 * result (0 for a function with no result). The memory and the globals
 * stay from call to call. */
EngineResult engine_call(Engine *e, unsigned func, const uint64_t *args, unsigned count, uint64_t *result);

/* Gives 1 and the word (8 bytes, little endian) at ADDRESS in *WORD when
 * the module exports its memory and the word is in it, else 0. */
int engine_word(const Engine *e, uint64_t address, uint64_t *word);

/* Writes WORD (8 bytes, little endian) at ADDRESS and gives 1 when the
 * module exports its memory and the word is in it, else 0. */
int engine_put_word(Engine *e, uint64_t address, uint64_t word);

/* Gives 1, the data and its size when the module has a custom section NAME,
 * else 0. */
int engine_section(const Engine *e, const char *name, const unsigned char **data, size_t *size);

/* The message of the last failure. */
const char *engine_message(const Engine *e);

/* Releases the tables and the memory of E. The message stays. */
void engine_free(Engine *e);

#endif
