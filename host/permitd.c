/* permitd, the MCP gate of permit-lang (S9 U3, docs/MCP.md, Q-S9-4, 5, 6,
 * 9 (a)).
 *
 *   permitd [--state FILE] [--report FILE] [--approvals FILE | --approver tty]
 *           [--cost TOOL=N]... [--cred-file PATH | --cred-env NAME]
 *           [--cred-var NAME] MODULE -- CMD ARGS
 *
 * The agent talks to permitd on stdin and stdout. CMD is the upstream MCP
 * server: a child on two pipes (posix_spawn). Each `tools/call` goes
 * through the `decide` export of MODULE (the call steps of docs/MCP.md);
 * each other line goes through unchanged. Only main stops the process
 * (N6): each other function gives a result. Exit 0 at the end of the
 * input, 1 when MODULE does not load, 2 for a usage error, a bad state
 * file or a report file that does not open. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine.h"
#include "jcs.h"
#include "jsonin.h"
#include "report.h"
#include "sha256.h"
#include "wasmread.h"

extern char **environ;

enum {
  LINE_CAP = 1 << 20,          /* the bytes of one JSON-RPC line (MCP.md) */
  PATH_COUNT_MAX = 4096,       /* the permit.res literals */
  BIND_COUNT_MAX = 4096,       /* the permit.bind rows */
  COST_MAX = 64,               /* the --cost flags */
  BYTES_COUNT_MAX = 8,         /* the Bytes handles of one call */
  DIGEST_COUNT_MAX = 1 << 16,  /* the Digest handles of the process */
  PENDING_MAX = 256,           /* the forwarded calls with no answer yet */
  ID_CAP = 256,                /* the JCS bytes of one request id */
  ENV_COUNT_MAX = 4096,        /* the environment of the child */
  CRED_CAP = 1 << 16,          /* the bytes of a credential */
  STATE_PATH_CAP = 4096
};

#define ARENA_LIMIT_BYTES ((size_t)1 << 26)
#define DIGEST_HEX 64

/* The names of the Reason arms (domain/domain.lang:304): deny code R + 1. */
static const char *const reason_name[8] = {
    "scopeMiss", "boundSpent", "budgetShort", "noApproval",
    "digestDiffers", "approvalUsed", "widen", "malformed"};

/* The names of the Action arms (domain/domain.lang): arm A. */
static const char *const action_name[3] = {"inspect", "propose", "execute"};

/* The arm of the Action family that needs an approval. */
#define ACTION_EXECUTE 2

typedef struct {
  const unsigned char *tool;
  size_t tool_len;
  uint64_t arm;
  const char *pointer;
  size_t pointer_len;
} BindRow;

typedef struct {
  const char *tool;
  uint64_t cost;
} CostRow;

/* A Bytes handle of the call: the envelope of (TARGET, PAYLOAD), LEN
 * bytes at AT in the envelope buffer. */
typedef struct {
  uint64_t target;
  uint64_t payload;
  size_t at;
  size_t len;
} BytesRow;

typedef struct {
  unsigned char bytes[ID_CAP];
  size_t len;
} PendingId;

typedef struct {
  int fd;
  char *buf;
  size_t len;
  int over; /* the line is over LINE_CAP: drop it to its end */
} LineIn;

typedef struct {
  Engine engine;
  unsigned func_decide;
  unsigned func_malformed;
  uint64_t slots; /* K: the used digest words after the 3 state words */
  /* The Path handles: 0 .. path_count - 1 are the permit.res literals;
   * path_total is path_count + 1 when the call target is not a literal. */
  const unsigned char *path_text[PATH_COUNT_MAX + 1];
  size_t path_len[PATH_COUNT_MAX + 1];
  unsigned path_count;
  unsigned path_total;
  BindRow binds[BIND_COUNT_MAX];
  unsigned bind_count;
  /* The call: Payload handle 0 and the Bytes handles. */
  const JsonValue *payload;
  BytesRow bytes[BYTES_COUNT_MAX];
  unsigned bytes_count;
  size_t bytes_used;
  /* The Digest handles: handle H is digests[H - 1]; 0 is an empty slot. */
  unsigned digest_count;
  /* The flags. */
  const char *state_path;
  const char *report_path;
  FILE *report; /* the --report FILE, open to append, or NULL */
  const char *approvals_path;
  int approver_tty;
  CostRow costs[COST_MAX];
  unsigned cost_count;
  /* The child. */
  pid_t child;
  int child_in;
  int child_out;
  int child_dead;
  PendingId pending[PENDING_MAX];
  unsigned pending_count;
} Gate;

static unsigned char envelope_buf[2 * LINE_CAP + 4096];
static unsigned char digests[DIGEST_COUNT_MAX][32];
static unsigned char payload_buf[LINE_CAP];
static unsigned char call_env[2 * LINE_CAP + 4096];
static char agent_buf[LINE_CAP];
static char child_buf[LINE_CAP];

/* ---- output ---- */

static void out_bytes(const void *bytes, size_t len) {
  fwrite(bytes, 1, len, stdout);
}

static void out_text(const char *text) {
  out_bytes(text, strlen(text));
}

/* The JCS form of the LEN bytes of TEXT as a JSON string. */
static void out_string(const char *text, size_t len) {
  static unsigned char buf[4096];
  JsonValue v;
  JcsBuf b;
  memset(&v, 0, sizeof v);
  v.kind = JSON_STRING;
  v.text = text;
  v.len = len;
  b.bytes = buf;
  b.cap = sizeof buf;
  b.len = 0;
  if (jcs_write(&v, &b) == JCS_OK)
    out_bytes(buf, b.len);
  else
    out_text("\"\"");
}

/* {"jsonrpc":"2.0","id":ID with ID the JCS bytes (NULL: null). */
static void out_head(const PendingId *id) {
  out_text("{\"jsonrpc\":\"2.0\",\"id\":");
  if (id)
    out_bytes(id->bytes, id->len);
  else
    out_text("null");
}

/* A tools/call result with one text item and isError true (a deny or
 * toolFailed: the model must see it, MCP.md). */
static void reply_is_error(const PendingId *id, const char *text) {
  out_head(id);
  out_text(",\"result\":{\"content\":[{\"type\":\"text\",\"text\":");
  out_string(text, strlen(text));
  out_text("}],\"isError\":true}}\n");
  fflush(stdout);
}

static void reply_error(const PendingId *id, int code, const char *message) {
  char num[32];
  out_head(id);
  snprintf(num, sizeof num, "%d", code);
  out_text(",\"error\":{\"code\":");
  out_text(num);
  out_text(",\"message\":");
  out_string(message, strlen(message));
  out_text("}}\n");
  fflush(stdout);
}

static void reply_deny(const PendingId *id, uint64_t code) {
  char text[64];
  snprintf(text, sizeof text, "permit-lang deny %s", reason_name[code - 1]);
  reply_is_error(id, text);
}

/* ---- hex and numbers ---- */

static void hex_of(const unsigned char digest[32], char hex[DIGEST_HEX + 1]) {
  static const char digit[] = "0123456789abcdef";
  unsigned i;
  for (i = 0; i < 32; i++) {
    hex[2 * i] = digit[digest[i] >> 4];
    hex[2 * i + 1] = digit[digest[i] & 15];
  }
  hex[DIGEST_HEX] = '\0';
}

static int hex_digit(char c, unsigned *v) {
  if (c >= '0' && c <= '9') {
    *v = (unsigned)(c - '0');
    return 1;
  }
  if (c >= 'a' && c <= 'f') {
    *v = (unsigned)(c - 'a' + 10);
    return 1;
  }
  return 0;
}

/* Gives 1 when the LEN bytes of TEXT are 64 lower-case hex characters,
 * and their 32 bytes in DIGEST. */
static int digest_of(const char *text, size_t len, unsigned char digest[32]) {
  unsigned i, hi, lo;
  if (len != DIGEST_HEX)
    return 0;
  for (i = 0; i < 32; i++) {
    if (!hex_digit(text[2 * i], &hi) || !hex_digit(text[2 * i + 1], &lo))
      return 0;
    digest[i] = (unsigned char)(hi << 4 | lo);
  }
  return 1;
}

/* A decimal value that fits in uint64_t. */
static int decimal(const char *text, size_t len, uint64_t *v) {
  size_t i;
  uint64_t n = 0;
  if (len == 0 || len > 20)
    return 0;
  for (i = 0; i < len; i++) {
    if (text[i] < '0' || text[i] > '9')
      return 0;
    uint64_t digit = (uint64_t)(text[i] - '0');
    if (n > (UINT64_MAX - digit) / 10)
      return 0;
    n = n * 10 + digit;
  }
  *v = n;
  return 1;
}

/* ---- the handle tables and the imports (O5, s9/U3-state.md) ---- */

/* Gives 1 and the handle of DIGEST (equal digests get one handle). */
static int intern(Gate *g, const unsigned char digest[32], uint64_t *handle) {
  unsigned i;
  for (i = 0; i < g->digest_count; i++)
    if (memcmp(digests[i], digest, 32) == 0) {
      *handle = (uint64_t)i + 1;
      return 1;
    }
  if (g->digest_count == DIGEST_COUNT_MAX)
    return 0;
  memcpy(digests[g->digest_count], digest, 32);
  g->digest_count++;
  *handle = g->digest_count;
  return 1;
}

static int host_envelope(void *ctx, const uint64_t *args, uint64_t *result) {
  Gate *g = ctx;
  JcsBuf b;
  unsigned i;
  if (args[0] >= g->path_total || args[1] != 0 || !g->payload)
    return 0;
  for (i = 0; i < g->bytes_count; i++)
    if (g->bytes[i].target == args[0] && g->bytes[i].payload == args[1]) {
      *result = i;
      return 1;
    }
  if (g->bytes_count == BYTES_COUNT_MAX)
    return 0;
  b.bytes = envelope_buf + g->bytes_used;
  b.cap = sizeof envelope_buf - g->bytes_used;
  b.len = 0;
  if (jcs_envelope((const char *)g->path_text[args[0]], g->path_len[args[0]], g->payload, &b) != JCS_OK)
    return 0;
  g->bytes[g->bytes_count].target = args[0];
  g->bytes[g->bytes_count].payload = args[1];
  g->bytes[g->bytes_count].at = g->bytes_used;
  g->bytes[g->bytes_count].len = b.len;
  g->bytes_used += b.len;
  *result = g->bytes_count;
  g->bytes_count++;
  return 1;
}

static int host_hash(void *ctx, const uint64_t *args, uint64_t *result) {
  Gate *g = ctx;
  unsigned char digest[32];
  if (args[0] >= g->bytes_count)
    return 0;
  sha256(envelope_buf + g->bytes[args[0]].at, g->bytes[args[0]].len, digest);
  return intern(g, digest, result);
}

/* 1 when G is R or a '/' ancestor of R (the segment prefix). */
static int host_within(void *ctx, const uint64_t *args, uint64_t *result) {
  Gate *g = ctx;
  size_t rl, gl;
  if (args[0] >= g->path_total || args[1] >= g->path_total)
    return 0;
  rl = g->path_len[args[0]];
  gl = g->path_len[args[1]];
  *result = (gl == rl && memcmp(g->path_text[args[0]], g->path_text[args[1]], rl) == 0) ||
            (gl < rl && g->path_text[args[0]][gl] == '/' &&
             memcmp(g->path_text[args[0]], g->path_text[args[1]], gl) == 0);
  return 1;
}

static const EngineHostFunc imports[3] = {
    {"permit", "envelope", 2, host_envelope},
    {"permit", "hash", 1, host_hash},
    {"permit", "within", 2, host_within}};

/* ---- the module ---- */

static int read_paths(Gate *g) {
  const unsigned char *data = NULL;
  size_t size = 0;
  uint64_t n = 0, i;
  Reader r;
  g->path_count = 0;
  if (!engine_section(&g->engine, "permit.res", &data, &size))
    return 1;
  memset(&r, 0, sizeof r);
  r.bytes = data;
  r.end = size;
  if (!get_leb(&r, 32, &n) || n > PATH_COUNT_MAX)
    return 0;
  for (i = 0; i < n; i++) {
    uint64_t len = 0;
    if (!get_leb(&r, 32, &len) || len > r.end - r.pos)
      return 0;
    g->path_text[i] = data + r.pos;
    g->path_len[i] = (size_t)len;
    r.pos += (size_t)len;
  }
  g->path_count = (unsigned)n;
  g->path_total = g->path_count;
  return 1;
}

static int read_binds(Gate *g) {
  const unsigned char *data = NULL;
  size_t size = 0;
  uint64_t n = 0, i;
  Reader r;
  g->bind_count = 0;
  if (!engine_section(&g->engine, "permit.bind", &data, &size))
    return 1;
  memset(&r, 0, sizeof r);
  r.bytes = data;
  r.end = size;
  if (!get_leb(&r, 32, &n) || n > BIND_COUNT_MAX)
    return 0;
  for (i = 0; i < n; i++) {
    uint64_t len = 0, arm = 0, plen = 0;
    if (!get_leb(&r, 32, &len) || len > r.end - r.pos)
      return 0;
    g->binds[i].tool = data + r.pos;
    g->binds[i].tool_len = (size_t)len;
    r.pos += (size_t)len;
    if (!get_leb(&r, 32, &arm) || !get_leb(&r, 32, &plen) || plen > r.end - r.pos)
      return 0;
    g->binds[i].arm = arm;
    g->binds[i].pointer = (const char *)data + r.pos;
    g->binds[i].pointer_len = (size_t)plen;
    r.pos += (size_t)plen;
  }
  g->bind_count = (unsigned)n;
  return 1;
}

static const BindRow *bind_of(const Gate *g, const char *tool) {
  size_t len = strlen(tool);
  unsigned i;
  for (i = 0; i < g->bind_count; i++)
    if (g->binds[i].tool_len == len && memcmp(g->binds[i].tool, tool, len) == 0)
      return &g->binds[i];
  return NULL;
}

/* Gives 1 when the module loads and has the exports and the state words;
 * else 0 and a message on stderr. */
static int load_module(Gate *g, const char *path) {
  static unsigned char bytes[WASM_MODULE_MAX + 1];
  EngineBinding binding;
  FILE *m = fopen(path, "rb");
  size_t size;
  if (!m) {
    fprintf(stderr, "permitd: cannot open %s\n", path);
    return 0;
  }
  size = fread(bytes, 1, sizeof bytes, m);
  fclose(m);
  binding.funcs = imports;
  binding.count = 3;
  binding.ctx = g;
  if (engine_load(&g->engine, bytes, size, &binding) != ENGINE_OK) {
    fprintf(stderr, "permitd: load: %s\n", engine_message(&g->engine));
    return 0;
  }
  if (!engine_find(&g->engine, "decide", &g->func_decide) ||
      engine_params(&g->engine, g->func_decide) != 6 ||
      !engine_find(&g->engine, "malformed", &g->func_malformed) ||
      engine_params(&g->engine, g->func_malformed) != 0) {
    fprintf(stderr, "permitd: %s has no decide (6) and malformed (0) exports\n", path);
    return 0;
  }
  if (!engine_word(&g->engine, 0, &g->slots) || !engine_word(&g->engine, 8 * (3 + g->slots - 1), &(uint64_t){0})) {
    fprintf(stderr, "permitd: %s has no state words\n", path);
    return 0;
  }
  if (!read_paths(g) || !read_binds(g)) {
    fprintf(stderr, "permitd: %s has a bad permit.res or permit.bind section\n", path);
    return 0;
  }
  return 1;
}

/* ---- the state file (Q-S9-5 (a)) ---- */

/* One line of F without its '\n' in LINE (CAP bytes): gives the length,
 * or -1 at the end of F or for a line over CAP - 1 bytes. */
static long get_line(FILE *f, char *line, size_t cap) {
  size_t n = 0;
  int c = getc(f);
  if (c == EOF)
    return -1;
  for (; c != EOF && c != '\n'; c = getc(f)) {
    if (n + 1 >= cap)
      return -1;
    line[n++] = (char)c;
  }
  line[n] = '\0';
  return (long)n;
}

/* Gives 1 when there is no file or the file is good (its words are put
 * back), else 0. */
static int load_state(Gate *g) {
  char line[128];
  uint64_t words[3], i;
  long n;
  int ok = 1;
  FILE *f;
  if (!g->state_path)
    return 1;
  f = fopen(g->state_path, "r");
  if (!f)
    return errno == ENOENT;
  for (i = 0; i < 3 && ok; i++) {
    n = get_line(f, line, sizeof line);
    ok = n > 0 && decimal(line, (size_t)n, &words[i]);
  }
  ok = ok && words[0] <= g->slots && words[2] <= g->slots;
  for (i = 0; i < 3 && ok; i++)
    ok = engine_put_word(&g->engine, 8 * i, words[i]);
  for (i = 0; ok && i < words[2]; i++) {
    unsigned char digest[32];
    uint64_t handle = 0;
    n = get_line(f, line, sizeof line);
    ok = n > 0 && digest_of(line, (size_t)n, digest) && intern(g, digest, &handle) &&
         engine_put_word(&g->engine, 8 * (3 + i), handle);
  }
  ok = ok && getc(f) == EOF;
  fclose(f);
  return ok;
}

/* Writes the state words to a temp file, fsync, and renames it over the
 * state file. Gives 1, or 0 when a step fails. */
static int save_state(Gate *g) {
  static char tmp[STATE_PATH_CAP + 8];
  uint64_t words[3], handle, i;
  char hex[DIGEST_HEX + 1];
  int ok = 1;
  FILE *f;
  if (!g->state_path)
    return 1;
  snprintf(tmp, sizeof tmp, "%s.tmp", g->state_path);
  for (i = 0; i < 3 && ok; i++)
    ok = engine_word(&g->engine, 8 * i, &words[i]);
  if (!ok || words[2] > g->slots)
    return 0;
  f = fopen(tmp, "w");
  if (!f)
    return 0;
  fprintf(f, "%llu\n%llu\n%llu\n", (unsigned long long)words[0], (unsigned long long)words[1],
          (unsigned long long)words[2]);
  for (i = 0; i < words[2] && ok; i++) {
    ok = engine_word(&g->engine, 8 * (3 + i), &handle) && handle >= 1 && handle <= g->digest_count;
    if (ok) {
      hex_of(digests[handle - 1], hex);
      fprintf(f, "%s\n", hex);
    }
  }
  ok = ok && fflush(f) == 0 && fsync(fileno(f)) == 0;
  ok = fclose(f) == 0 && ok;
  return ok && rename(tmp, g->state_path) == 0;
}

/* ---- the approver (MCP.md "Approvals") ---- */

/* Gives 1 and the line number when HEX is a line of the approvals file.
 * The file is read again at each call (O9 test mode). */
static int approved_by_file(const Gate *g, const char *hex, unsigned long *line_no) {
  char line[DIGEST_HEX];
  unsigned long no = 0;
  size_t n = 0;
  int c, found = 0, over = 0;
  FILE *f = fopen(g->approvals_path, "r");
  if (!f)
    return 0;
  while (!found) {
    c = getc(f);
    if (c == '\n' || c == EOF) {
      if (c == EOF && n == 0 && !over)
        break;
      no++;
      found = !over && n == DIGEST_HEX && memcmp(line, hex, DIGEST_HEX) == 0;
      n = 0;
      over = 0;
      if (c == EOF)
        break;
    } else if (n < DIGEST_HEX) {
      line[n++] = (char)c;
    } else {
      over = 1;
    }
  }
  fclose(f);
  *line_no = no;
  return found;
}

/* Writes the envelope and HEX to /dev/tty and reads one line: 1 for "y". */
static int approved_by_tty(const unsigned char *env, size_t env_len, const char *hex) {
  char answer[8];
  int yes;
  FILE *t = fopen("/dev/tty", "r+");
  if (!t)
    return 0;
  fwrite(env, 1, env_len, t);
  fprintf(t, "\n%s\napprove? [y/N] ", hex);
  fflush(t);
  yes = fgets(answer, sizeof answer, t) != NULL && answer[0] == 'y' &&
        (answer[1] == '\n' || answer[1] == '\0');
  fclose(t);
  return yes;
}

/* ---- the child ---- */

/* Gives 1 when all of the LEN bytes go to the child. */
static int child_write(Gate *g, const void *bytes, size_t len) {
  const unsigned char *p = bytes;
  while (len > 0 && !g->child_dead) {
    ssize_t n = write(g->child_in, p, len);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    p += n;
    len -= (size_t)n;
  }
  return !g->child_dead;
}

/* The child stopped or a pipe closed: each pending call is toolFailed. */
static void child_closed(Gate *g) {
  unsigned i;
  if (g->child_dead)
    return;
  g->child_dead = 1;
  for (i = 0; i < g->pending_count; i++)
    reply_is_error(&g->pending[i], "permit-lang toolFailed closed");
  g->pending_count = 0;
  if (g->child_in >= 0)
    close(g->child_in);
  g->child_in = -1;
}

static int pending_take(Gate *g, const PendingId *id) {
  unsigned i;
  for (i = 0; i < g->pending_count; i++)
    if (g->pending[i].len == id->len && memcmp(g->pending[i].bytes, id->bytes, id->len) == 0) {
      g->pending[i] = g->pending[g->pending_count - 1];
      g->pending_count--;
      return 1;
    }
  return 0;
}

/* Gives 1 and the JCS bytes of the id of MESSAGE when it is a string or a
 * number. */
static int id_of(const JsonValue *message, PendingId *id) {
  const JsonValue *v = json_member(message, "id");
  JcsBuf b;
  if (!v || (v->kind != JSON_STRING && v->kind != JSON_NUMBER))
    return 0;
  b.bytes = id->bytes;
  b.cap = sizeof id->bytes;
  b.len = 0;
  if (jcs_write(v, &b) != JCS_OK)
    return 0;
  id->len = b.len;
  return 1;
}

/* ---- the call steps (MCP.md) ---- */

/* A shallow copy of OBJECT in OUT: the member KEY is dropped (VALUE NULL)
 * or has VALUE. Gives 0 when there is no memory. */
static int edit_member(Arena *arena, const JsonValue *object, const char *key, const JsonValue *value,
                       JsonValue *out) {
  const JsonValue *m;
  JsonValue *last = NULL;
  *out = *object;
  out->first = NULL;
  out->count = 0;
  for (m = object->first; m; m = m->next) {
    JsonValue *copy;
    int hit = m->key_len == strlen(key) && memcmp(m->key, key, m->key_len) == 0;
    if (hit && !value)
      continue;
    copy = arena_alloc(arena, sizeof *copy);
    if (!copy)
      return 0;
    *copy = hit ? *value : *m;
    copy->key = m->key;
    copy->key_len = m->key_len;
    copy->next = NULL;
    if (last)
      last->next = copy;
    else
      out->first = copy;
    last = copy;
    out->count++;
  }
  return 1;
}

static uint64_t cost_of(const Gate *g, const char *tool) {
  unsigned i;
  for (i = 0; i < g->cost_count; i++)
    if (strcmp(g->costs[i].tool, tool) == 0)
      return g->costs[i].cost;
  return 1;
}

/* Steps and budget: words 0 and 1 of the state after decide or malformed. */
static int state_words(Gate *g, uint64_t *steps, uint64_t *budget) {
  return engine_word(&g->engine, 0, steps) && engine_word(&g->engine, 8, budget);
}

/* Appends the malformed line of the call with id ID (NULL for a parse
 * error) to the report file (Q-S10-1 (a)). Gives 1 when there is no
 * --report FILE. */
static int report_bad(Gate *g, const PendingId *id) {
  uint64_t steps = 0, budget = 0;
  return !g->report || (state_words(g, &steps, &budget) &&
                        report_malformed(g->report, id ? id->bytes : NULL, id ? id->len : 0, steps, budget));
}

/* Runs `malformed ()`, writes the state and answers: the deny with the id,
 * or -32700 with id null. */
static void call_malformed(Gate *g, const PendingId *id) {
  uint64_t code = 0;
  g->payload = NULL;
  g->path_total = g->path_count;
  g->bytes_count = 0;
  g->bytes_used = 0;
  if (engine_call(&g->engine, g->func_malformed, NULL, 0, &code) != ENGINE_OK) {
    fprintf(stderr, "permitd: malformed: %s\n", engine_message(&g->engine));
    reply_error(id, -32603, "permit-lang host error");
    return;
  }
  if (!save_state(g)) {
    fprintf(stderr, "permitd: cannot write the state file %s\n", g->state_path);
    reply_error(id, -32603, "permit-lang state error");
    return;
  }
  if (!report_bad(g, id)) {
    fprintf(stderr, "permitd: cannot write the report file %s\n", g->report_path);
    reply_error(id, -32603, "permit-lang report error");
    return;
  }
  if (!id)
    reply_error(NULL, -32700, "permit-lang deny malformed");
  else if (code >= 1 && code <= 8)
    reply_deny(id, code);
  else
    reply_error(id, -32603, "permit-lang host error");
}

/* Appends the call line of a decided call (CODE 0 to 8) to the report file
 * (Q-S10-1 (a)): after the state write, before the answer or the forward.
 * NAMED is the named digest or NULL; WHO is the approver or "". Gives 1
 * when there is no --report FILE. */
static int report_decided(Gate *g, const char *name, const BindRow *row, const char *path, size_t path_len,
                          const PendingId *id, const unsigned char digest[32], const unsigned char *named,
                          const char *who, uint64_t cost, uint64_t code) {
  char digest_hex[DIGEST_HEX + 1], named_hex[DIGEST_HEX + 1];
  ReportCall call;
  if (!g->report)
    return 1;
  hex_of(digest, digest_hex);
  if (named)
    hex_of(named, named_hex);
  call.tool = name;
  call.action = row->arm < 3 ? action_name[row->arm] : NULL;
  call.target = path;
  call.target_len = path_len;
  call.id = id->bytes;
  call.id_len = id->len;
  call.digest = digest_hex;
  call.approval = named ? named_hex : NULL;
  call.approver = who[0] != '\0' ? who : NULL;
  call.reason = code >= 1 ? reason_name[code - 1] : NULL;
  call.cost = cost;
  return state_words(g, &call.steps, &call.budget) && report_call(g->report, &call);
}

/* The call steps 2 to 9 for a parsed tools/call MESSAGE with id ID. */
static void tools_call(Gate *g, Arena *arena, const JsonValue *message, const PendingId *id) {
  const JsonValue *params = json_member(message, "params");
  const JsonValue *meta, *approval, *target;
  const char *name, *path, *named_hex = NULL;
  static JsonValue payload, meta_left;
  unsigned char named[32], digest[32];
  char hex[DIGEST_HEX + 1], who[STATE_PATH_CAP + 32];
  const BindRow *row;
  JcsBuf pb, eb;
  uint64_t args[6], code = 0, handle = 0;
  int has = 0;
  unsigned i;
  name = json_get_string(json_member(params, "name"));
  if (!params || params->kind != JSON_OBJECT || !name || !json_member(params, "arguments") ||
      json_member(params, "arguments")->kind != JSON_OBJECT) {
    call_malformed(g, id);
    return;
  }
  /* Step 2: the approval key leaves params._meta. */
  payload = *params;
  meta = json_member(params, "_meta");
  approval = meta ? json_member(meta, "permit/approval") : NULL;
  if (approval) {
    named_hex = json_get_string(approval);
    if (!named_hex || !digest_of(named_hex, approval->len, named) ||
        !edit_member(arena, meta, "permit/approval", NULL, &meta_left) ||
        !edit_member(arena, params, "_meta", meta_left.count ? &meta_left : NULL, &payload)) {
      call_malformed(g, id);
      return;
    }
  }
  /* Step 3: the payload bytes. */
  pb.bytes = payload_buf;
  pb.cap = sizeof payload_buf;
  pb.len = 0;
  /* Step 4: the target of the bind row. */
  row = bind_of(g, name);
  target = row ? jcs_pointer(json_member(&payload, "arguments"), row->pointer, row->pointer_len) : NULL;
  path = json_get_string(target);
  if (jcs_write(&payload, &pb) != JCS_OK || !path) {
    call_malformed(g, id);
    return;
  }
  /* Step 5: the digest of the envelope. */
  eb.bytes = call_env;
  eb.cap = sizeof call_env;
  eb.len = 0;
  if (jcs_envelope(path, target->len, &payload, &eb) != JCS_OK) {
    call_malformed(g, id);
    return;
  }
  sha256(call_env, eb.len, digest);
  /* The per-call handles: the target path and Payload 0. */
  g->payload = &payload;
  g->bytes_count = 0;
  g->bytes_used = 0;
  g->path_total = g->path_count;
  args[0] = g->path_count;
  for (i = 0; i < g->path_count; i++)
    if (g->path_len[i] == target->len && memcmp(g->path_text[i], path, target->len) == 0) {
      args[0] = i;
      break;
    }
  if (args[0] == g->path_count) {
    g->path_text[g->path_count] = (const unsigned char *)path;
    g->path_len[g->path_count] = target->len;
    g->path_total = g->path_count + 1;
  }
  /* Step 6: the approval. */
  who[0] = '\0';
  if (g->approvals_path && named_hex) {
    unsigned long line_no = 0;
    has = approved_by_file(g, named_hex, &line_no);
    snprintf(who, sizeof who, "%s:%lu", g->approvals_path, line_no);
  } else if (g->approver_tty && row->arm == ACTION_EXECUTE) {
    hex_of(named_hex ? named : digest, hex);
    has = approved_by_tty(call_env, eb.len, hex);
    snprintf(who, sizeof who, "tty");
    if (has && !named_hex) {
      memcpy(named, digest, 32);
      named_hex = hex;
    }
  }
  if (named_hex && !intern(g, named, &handle)) {
    reply_error(id, -32603, "permit-lang digest table full");
    return;
  }
  if (g->pending_count == PENDING_MAX) {
    reply_error(id, -32603, "permit-lang too many pending calls");
    return;
  }
  /* Steps 7 and 8: the cost and decide. */
  args[1] = row->arm;
  args[2] = 0;
  args[3] = cost_of(g, name);
  args[4] = (uint64_t)has;
  args[5] = handle;
  if (engine_call(&g->engine, g->func_decide, args, 6, &code) != ENGINE_OK) {
    fprintf(stderr, "permitd: decide: %s\n", engine_message(&g->engine));
    reply_error(id, -32603, "permit-lang host error");
    return;
  }
  /* Step 9: the state, then the decision. */
  if (!save_state(g)) {
    fprintf(stderr, "permitd: cannot write the state file %s\n", g->state_path);
    reply_error(id, -32603, "permit-lang state error");
    return;
  }
  if (code <= 8 && !report_decided(g, name, row, path, target->len, id, digest, named_hex ? named : NULL, who,
                                   args[3], code)) {
    fprintf(stderr, "permitd: cannot write the report file %s\n", g->report_path);
    reply_error(id, -32603, "permit-lang report error");
    return;
  }
  if (code >= 1 && code <= 8) {
    reply_deny(id, code);
    return;
  }
  if (code != 0) {
    reply_error(id, -32603, "permit-lang host error");
    return;
  }
  fprintf(stderr, "permitd: permit %s approver %s\n", name, has ? who : "none");
  if (g->child_dead) {
    reply_is_error(id, "permit-lang toolFailed closed");
    return;
  }
  g->pending[g->pending_count] = *id;
  g->pending_count++;
  if (!child_write(g, "{\"jsonrpc\":\"2.0\",\"id\":", 22) || !child_write(g, id->bytes, id->len) ||
      !child_write(g, ",\"method\":\"tools/call\",\"params\":", 32) ||
      !child_write(g, payload_buf, pb.len) || !child_write(g, "}\n", 2))
    child_closed(g);
}

/* One line of the agent. */
static void take_agent(Gate *g, const char *line, size_t len, int over) {
  Arena arena;
  Diag diag;
  JsonValue v;
  PendingId id;
  const char *method;
  if (len > 0 && line[len - 1] == '\r')
    len--;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  diag_init(&diag);
  if (over || !json_parse(&arena, line, len, &v, &diag) || v.kind != JSON_OBJECT) {
    call_malformed(g, NULL);
    arena_release(&arena);
    return;
  }
  method = v.kind == JSON_OBJECT ? json_get_string(json_member(&v, "method")) : NULL;
  if (method && strcmp(method, "tools/call") == 0) {
    if (id_of(&v, &id))
      tools_call(g, &arena, &v, &id);
    else
      call_malformed(g, NULL);
  } else if (!g->child_dead) {
    if (!child_write(g, line, len) || !child_write(g, "\n", 1))
      child_closed(g);
  } else if (v.kind == JSON_OBJECT && id_of(&v, &id)) {
    reply_error(&id, -32603, "permit-lang upstream closed");
  }
  arena_release(&arena);
}

/* One line of the child. */
static void take_child(Gate *g, const char *line, size_t len, int over) {
  Arena arena;
  Diag diag;
  JsonValue v;
  PendingId id;
  if (over) {
    fprintf(stderr, "permitd: an upstream line over %d bytes is dropped\n", LINE_CAP);
    return;
  }
  arena_init(&arena, ARENA_LIMIT_BYTES);
  diag_init(&diag);
  if (json_parse(&arena, line, len, &v, &diag) && v.kind == JSON_OBJECT && id_of(&v, &id) &&
      pending_take(g, &id) && json_member(&v, "error")) {
    reply_is_error(&id, "permit-lang toolFailed error");
  } else {
    out_bytes(line, len);
    out_text("\n");
    fflush(stdout);
  }
  arena_release(&arena);
}

/* Reads what IN has and takes each whole line. Gives 1, or 0 at the end
 * of the input or on a read failure (the last part line is taken). */
static int pump(Gate *g, LineIn *in, void (*take)(Gate *, const char *, size_t, int)) {
  static char chunk[1 << 16];
  ssize_t n = read(in->fd, chunk, sizeof chunk);
  ssize_t i;
  if (n < 0 && (errno == EINTR || errno == EAGAIN))
    return 1;
  if (n <= 0) {
    if (in->len > 0 || in->over)
      take(g, in->buf, in->len, in->over);
    in->len = 0;
    in->over = 0;
    return 0;
  }
  for (i = 0; i < n; i++) {
    if (chunk[i] == '\n') {
      take(g, in->buf, in->len, in->over);
      in->len = 0;
      in->over = 0;
    } else if (in->len < LINE_CAP) {
      in->buf[in->len++] = chunk[i];
    } else {
      in->over = 1;
    }
  }
  return 1;
}

/* ---- the start ---- */

static int usage(void) {
  fprintf(stderr, "usage: permitd [--state FILE] [--report FILE] [--approvals FILE | --approver tty]\n"
                  "               [--cost TOOL=N]... [--cred-file PATH | --cred-env NAME] [--cred-var NAME]\n"
                  "               MODULE -- CMD ARGS\n");
  return 2;
}

/* The environment of the child: ours without VAR, and VAR=CRED when the
 * credential is set (O11). */
static int child_env(char **env, const char *var, const char *cred) {
  static char entry[CRED_CAP + 256];
  size_t vl = var ? strlen(var) : 0;
  unsigned n = 0, i;
  for (i = 0; environ[i]; i++) {
    if (var && strncmp(environ[i], var, vl) == 0 && environ[i][vl] == '=')
      continue;
    if (n + 2 >= ENV_COUNT_MAX)
      return 0;
    env[n++] = environ[i];
  }
  if (var && cred) {
    if (vl + strlen(cred) + 2 > sizeof entry)
      return 0;
    snprintf(entry, sizeof entry, "%s=%s", var, cred);
    env[n++] = entry;
  }
  env[n] = NULL;
  return 1;
}

static int read_cred(const char *path, char *cred, size_t cap) {
  FILE *f = fopen(path, "rb");
  size_t n;
  if (!f)
    return 0;
  n = fread(cred, 1, cap - 1, f);
  fclose(f);
  if (n == cap - 1)
    return 0;
  if (n > 0 && cred[n - 1] == '\n')
    n--;
  cred[n] = '\0';
  return memchr(cred, '\0', n) == NULL;
}

static int spawn_child(Gate *g, char **argv, char **env) {
  posix_spawn_file_actions_t fa;
  int in[2], out[2], rc;
  if (pipe(in) != 0)
    return 0;
  if (pipe(out) != 0) {
    close(in[0]);
    close(in[1]);
    return 0;
  }
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, in[0], 0);
  posix_spawn_file_actions_adddup2(&fa, out[1], 1);
  posix_spawn_file_actions_addclose(&fa, in[0]);
  posix_spawn_file_actions_addclose(&fa, in[1]);
  posix_spawn_file_actions_addclose(&fa, out[0]);
  posix_spawn_file_actions_addclose(&fa, out[1]);
  rc = posix_spawnp(&g->child, argv[0], &fa, NULL, argv, env);
  posix_spawn_file_actions_destroy(&fa);
  close(in[0]);
  close(out[1]);
  if (rc != 0) {
    close(in[1]);
    close(out[0]);
    return 0;
  }
  g->child_in = in[1];
  g->child_out = out[0];
  return 1;
}

/* Keeps the report in the gate and apart from the state file. A state
 * rename must not replace the report while its old inode is open. */
static int open_report(Gate *g) {
  struct stat report, state;
  int ok;
  if (!g->report_path)
    return 1;
  g->report = fopen(g->report_path, "a");
  if (!g->report)
    return 0;
  ok = fcntl(fileno(g->report), F_SETFD, FD_CLOEXEC) == 0 && fstat(fileno(g->report), &report) == 0;
  if (ok && g->state_path && stat(g->state_path, &state) == 0)
    ok = report.st_dev != state.st_dev || report.st_ino != state.st_ino;
  if (!ok) {
    fclose(g->report);
    g->report = NULL;
  }
  return ok;
}

int main(int argc, char **argv) {
  static Gate g;
  static char *env[ENV_COUNT_MAX];
  static char cred_text[CRED_CAP];
  const char *module = NULL, *cred_file = NULL, *cred_env = NULL, *cred_var = NULL, *cred = NULL;
  LineIn agent = {0, agent_buf, 0, 0}, child = {-1, child_buf, 0, 0};
  int i, cmd = 0, agent_open = 1;
  for (i = 1; i < argc && !cmd; i++) {
    const char *a = argv[i];
    int more = i + 1 < argc;
    if (strcmp(a, "--") == 0) {
      cmd = i + 1;
    } else if (strcmp(a, "--state") == 0 && more && strlen(argv[i + 1]) < STATE_PATH_CAP) {
      g.state_path = argv[++i];
    } else if (strcmp(a, "--report") == 0 && more) {
      g.report_path = argv[++i];
    } else if (strcmp(a, "--approvals") == 0 && more && !g.approver_tty) {
      g.approvals_path = argv[++i];
    } else if (strcmp(a, "--approver") == 0 && more && strcmp(argv[i + 1], "tty") == 0 && !g.approvals_path) {
      g.approver_tty = 1;
      i++;
    } else if (strcmp(a, "--cost") == 0 && more && g.cost_count < COST_MAX) {
      char *eq = strchr(argv[i + 1], '=');
      if (!eq || eq == argv[i + 1] || !decimal(eq + 1, strlen(eq + 1), &g.costs[g.cost_count].cost))
        return usage();
      *eq = '\0';
      g.costs[g.cost_count].tool = argv[++i];
      g.cost_count++;
    } else if (strcmp(a, "--cred-file") == 0 && more && !cred_env) {
      cred_file = argv[++i];
    } else if (strcmp(a, "--cred-env") == 0 && more && !cred_file) {
      cred_env = argv[++i];
    } else if (strcmp(a, "--cred-var") == 0 && more) {
      cred_var = argv[++i];
    } else if (a[0] != '-' && !module) {
      module = a;
    } else {
      return usage();
    }
  }
  if (!module || !cmd || cmd >= argc || (!cred_var != (!cred_file && !cred_env)))
    return usage();
  if (cred_file) {
    if (!read_cred(cred_file, cred_text, sizeof cred_text)) {
      fprintf(stderr, "permitd: cannot read the credential file\n");
      return 2;
    }
    cred = cred_text;
  }
  if (cred_env) {
    cred = getenv(cred_env);
    if (!cred) {
      fprintf(stderr, "permitd: the credential variable %s is not set\n", cred_env);
      return 2;
    }
  }
  g.child_in = -1;
  g.child_out = -1;
  if (!load_module(&g, module))
    return 1;
  if (!load_state(&g)) {
    fprintf(stderr, "permitd: bad state file %s\n", g.state_path);
    engine_free(&g.engine);
    return 2;
  }
  if (!open_report(&g)) {
    fprintf(stderr, "permitd: cannot open the report file %s\n", g.report_path);
    engine_free(&g.engine);
    return 2;
  }
  if (!child_env(env, cred_var, cred)) {
    fprintf(stderr, "permitd: the child environment is too large\n");
    engine_free(&g.engine);
    return 2;
  }
  signal(SIGPIPE, SIG_IGN);
  if (!spawn_child(&g, argv + cmd, env)) {
    fprintf(stderr, "permitd: cannot start %s\n", argv[cmd]);
    engine_free(&g.engine);
    return 1;
  }
  child.fd = g.child_out;
  while (agent_open || child.fd >= 0) {
    struct pollfd fds[2];
    fds[0].fd = agent_open ? 0 : -1;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = child.fd;
    fds[1].events = POLLIN;
    fds[1].revents = 0;
    if (poll(fds, 2, -1) < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (fds[0].revents & (POLLIN | POLLHUP | POLLERR) && !pump(&g, &agent, take_agent)) {
      agent_open = 0;
      if (g.child_in >= 0)
        close(g.child_in);
      g.child_in = -1;
    }
    if (fds[1].revents & (POLLIN | POLLHUP | POLLERR) && !pump(&g, &child, take_child)) {
      close(child.fd);
      child.fd = -1;
      child_closed(&g);
    }
  }
  waitpid(g.child, NULL, 0);
  if (g.report)
    fclose(g.report);
  engine_free(&g.engine);
  return 0;
}
