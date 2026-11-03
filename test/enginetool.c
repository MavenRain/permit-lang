/* permit-lang S9 U1 (Q-S9-1 (a), L8): runs a decide script on the gate
 * module in the own engine (host/engine.c) with the O5 binding of
 * test/run-wasm.mjs: envelope t p = ((t << 32) | p) mod 2^64, hash e =
 * (e * 1000003 + 7) mod 2^64, and within r g = 1 when path g is path r or
 * an ancestor of r (the paths of the custom section permit.res).
 *
 *   build/enginetool SCRIPT FILE
 *
 * A script line is `decide PATH ACTION PAYLOAD COST DIGEST`, `malformed` or
 * `hash N` (the O5 hash stub of N, not a module call)
 * (DIGEST: `-`, or `PATH:N` for hash (envelope (res PATH) N)). The tool
 * makes the calls on one instance and prints the codes one per line, then
 * `steps N` and `left N` (the words 0 and 1 of the exported memory): the
 * lines of `run-wasm.mjs --decide`. Before the first call the tool reads
 * the words 0 and 1 and writes each back with engine_put_word: the start
 * path of the U3 state file (Q-S9-5 (a)). Exit 1: a bad script, module or
 * call, or no state words. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "wasmread.h"

enum { LINE_MAX = 1024, PATH_MAX_COUNT = 4096 };

/* The paths of permit.res: LEN bytes at TEXT each. */
typedef struct {
  const unsigned char *text[PATH_MAX_COUNT];
  size_t len[PATH_MAX_COUNT];
  unsigned count;
} Paths;

static int envelope(void *ctx, const uint64_t *args, uint64_t *result) {
  (void)ctx;
  *result = (args[0] << 32) | args[1];
  return 1;
}

static uint64_t hash_word(uint64_t e) {
  return e * 1000003u + 7u;
}

static int hash(void *ctx, const uint64_t *args, uint64_t *result) {
  (void)ctx;
  *result = hash_word(args[0]);
  return 1;
}

/* 1 when path G is path R or an ancestor of R. A handle that is not a path
 * is a host error. */
static int within(void *ctx, const uint64_t *args, uint64_t *result) {
  const Paths *p = ctx;
  size_t r, g;
  if (args[0] >= p->count || args[1] >= p->count)
    return 0;
  r = (size_t)args[0];
  g = (size_t)args[1];
  *result = p->len[r] >= p->len[g] && memcmp(p->text[r], p->text[g], p->len[g]) == 0 &&
            (p->len[r] == p->len[g] || p->text[r][p->len[g]] == '/');
  return 1;
}

static const EngineHostFunc O5[] = {
    {"permit", "envelope", 2, envelope},
    {"permit", "hash", 1, hash},
    {"permit", "within", 2, within},
};

/* The paths of the custom section permit.res (none when there is no
 * section). */
static int load_paths(const Engine *e, Paths *p) {
  const unsigned char *data = NULL;
  size_t size = 0;
  uint64_t n = 0, i;
  Reader r;
  if (!engine_section(e, "permit.res", &data, &size))
    return 1;
  memset(&r, 0, sizeof r);
  r.bytes = data;
  r.end = size;
  if (!get_leb(&r, 32, &n) || n > PATH_MAX_COUNT)
    return 0;
  for (i = 0; i < n; i++) {
    uint64_t len = 0;
    if (!get_leb(&r, 32, &len) || len > r.end - r.pos)
      return 0;
    p->text[i] = data + r.pos;
    p->len[i] = (size_t)len;
    r.pos += (size_t)len;
  }
  p->count = (unsigned)n;
  return 1;
}

/* The handle of PATH (LEN bytes): its index in permit.res. */
static int handle(const Paths *p, const char *path, size_t len, uint64_t *h) {
  unsigned i;
  for (i = 0; i < p->count; i++)
    if (p->len[i] == len && memcmp(p->text[i], path, len) == 0) {
      *h = i;
      return 1;
    }
  return 0;
}

/* 1 and the value when S is a run of decimal digits. */
static int number(const char *s, uint64_t *v) {
  const char *c;
  if (*s == '\0')
    return 0;
  for (c = s; *c; c++)
    if (*c < '0' || *c > '9')
      return 0;
  *v = strtoull(s, NULL, 10);
  return 1;
}

/* The 6 words of a decide line: target, action, payload, cost, has_digest,
 * digest. F holds the 6 fields of the line. */
static int decide_words(const Paths *p, char **f, uint64_t *w) {
  static const char *const actions[] = {"inspect", "propose", "execute"};
  const char *colon = strchr(f[5], ':');
  unsigned a;
  uint64_t n = 0, d = 0;
  if (!handle(p, f[1], strlen(f[1]), &w[0]))
    return 0;
  for (a = 0; a < 3 && strcmp(f[2], actions[a]) != 0; a++)
    ;
  if (a == 3 || !number(f[3], &w[2]) || !number(f[4], &w[3]))
    return 0;
  w[1] = a;
  w[4] = 0;
  w[5] = 0;
  if (strcmp(f[5], "-") == 0)
    return 1;
  if (!colon || colon == f[5] || !handle(p, f[5], (size_t)(colon - f[5]), &d) || !number(colon + 1, &n))
    return 0;
  w[4] = 1;
  w[5] = hash_word((d << 32) | n);
  return 1;
}

/* Splits LINE at each space into at most 6 fields. Gives the count, or 7
 * for more than 6. */
static unsigned split(char *line, char **f) {
  unsigned n = 0;
  char *t;
  for (t = strtok(line, " "); t; t = strtok(NULL, " ")) {
    if (n == 6)
      return 7;
    f[n++] = t;
  }
  return n;
}

/* Reads the words 0 and 1 (steps and left, at the addresses 0 and 8) and
 * writes each back with engine_put_word. The U3 state file (Q-S9-5 (a))
 * puts its saved words on this path before the first call. Gives 1 when
 * each read and write is good. */
static int round_trip(Engine *e) {
  uint64_t steps = 0, left = 0;
  return engine_word(e, 0, &steps) && engine_word(e, 8, &left) && engine_put_word(e, 0, steps) &&
         engine_put_word(e, 8, left);
}

/* Runs the lines of S on E. Gives 1 when each line is good and each call
 * gives ENGINE_OK. */
static int run_script(Engine *e, const Paths *p, FILE *s, const char *name) {
  unsigned decide = 0, malformed = 0;
  char line[LINE_MAX];
  uint64_t steps = 0, left = 0;
  if (!engine_find(e, "decide", &decide) || !engine_find(e, "malformed", &malformed)) {
    fprintf(stderr, "enginetool: no decide or malformed export\n");
    return 0;
  }
  while (fgets(line, sizeof line, s)) {
    char *f[6];
    uint64_t w[6], code = 0;
    unsigned n;
    EngineResult res;
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0')
      continue;
    n = split(line, f);
    if (n == 1 && strcmp(f[0], "malformed") == 0)
      res = engine_call(e, malformed, NULL, 0, &code);
    else if (n == 2 && strcmp(f[0], "hash") == 0 && number(f[1], &w[0]))
      res = hash(NULL, w, &code) ? ENGINE_OK : ENGINE_HOST;
    else if (n == 6 && strcmp(f[0], "decide") == 0 && decide_words(p, f, w))
      res = engine_call(e, decide, w, 6, &code);
    else {
      fprintf(stderr, "enginetool: %s: bad line\n", name);
      return 0;
    }
    if (res != ENGINE_OK) {
      fprintf(stderr, "enginetool: %s: call: %s\n", name, engine_message(e));
      return 0;
    }
    printf("%llu\n", (unsigned long long)code);
  }
  if (!engine_word(e, 0, &steps) || !engine_word(e, 8, &left)) {
    fprintf(stderr, "enginetool: no state words\n");
    return 0;
  }
  printf("steps %llu\nleft %llu\n", (unsigned long long)steps, (unsigned long long)left);
  return 1;
}

int main(int argc, char **argv) {
  static unsigned char bytes[WASM_MODULE_MAX + 1];
  static Paths paths;
  EngineBinding binding = {O5, 3, &paths};
  Engine e;
  FILE *m, *s;
  size_t size;
  int ok;
  if (argc != 3) {
    fprintf(stderr, "usage: enginetool SCRIPT FILE\n");
    return 2;
  }
  m = fopen(argv[2], "rb");
  if (!m) {
    fprintf(stderr, "enginetool: cannot open %s\n", argv[2]);
    return 1;
  }
  size = fread(bytes, 1, sizeof bytes, m);
  fclose(m);
  if (engine_load(&e, bytes, size, &binding) != ENGINE_OK) {
    fprintf(stderr, "enginetool: load: %s\n", engine_message(&e));
    return 1;
  }
  if (!load_paths(&e, &paths)) {
    fprintf(stderr, "enginetool: bad permit.res\n");
    engine_free(&e);
    return 1;
  }
  if (!round_trip(&e)) {
    fprintf(stderr, "enginetool: no state words\n");
    engine_free(&e);
    return 1;
  }
  s = fopen(argv[1], "r");
  if (!s) {
    fprintf(stderr, "enginetool: cannot open %s\n", argv[1]);
    engine_free(&e);
    return 1;
  }
  ok = run_script(&e, &paths, s, argv[1]);
  fclose(s);
  engine_free(&e);
  return ok ? 0 : 1;
}
