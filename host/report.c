/* The call lines of `permitd --report FILE`. See report.h. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "jcs.h"
#include "report.h"

#define REPORT_ARENA_BYTES ((size_t)1 << 26)

/* The JSON text of the line, on the arena: the JsonOut layout of
   src/report.c (evolve-lang 550e73b src/json.c), a copy and not a link
   (Q-S10-1 (a)). The buffer starts at 4096 bytes and doubles when the
   text does not fit. Each put returns 1, or 0 after a diagnostic, so the
   calls chain with &&. The text is not canonical: JCS makes the bytes
   (Q-S10-2 (a)). */
typedef struct {
  Arena *arena;
  Diag *diag;
  char *data; /* NUL-terminated; NULL before the first put */
  size_t len;
  size_t cap;
} JsonOut;

static int out_put(JsonOut *out, const char *text, size_t n) {
  size_t cap = out->cap == 0 ? 4096u : out->cap;
  char *data;
  while (cap < out->len + n + 1u)
    cap *= 2u;
  if (cap != out->cap) {
    data = arena_alloc(out->arena, cap);
    if (data == NULL)
      return diag_fail(out->diag, "OOM", "report", "out of memory");
    if (out->len > 0)
      memcpy(data, out->data, out->len);
    out->data = data;
    out->cap = cap;
  }
  memcpy(out->data + out->len, text, n);
  out->len += n;
  out->data[out->len] = '\0';
  return 1;
}

static int out_text(JsonOut *out, const char *text) {
  return out_put(out, text, strlen(text));
}

/* Adds the N bytes at TEXT inside a JSON string: '"', '\\' and each byte
   below 0x20 as a \u escape, each other byte as it is. */
static int out_bytes(JsonOut *out, const char *text, size_t n) {
  char esc[8];
  size_t i;
  int ok = 1;
  for (i = 0; ok && i < n; i++) {
    unsigned char c = (unsigned char)text[i];
    snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
    ok = c >= 0x20u && c != '"' && c != '\\' ? out_put(out, text + i, 1u) : out_text(out, esc);
  }
  return ok;
}

/* Adds TEXT as a JSON string, or null when TEXT is NULL. */
static int out_string(JsonOut *out, const char *text) {
  return text == NULL ? out_text(out, "null")
                      : out_text(out, "\"") && out_bytes(out, text, strlen(text)) && out_text(out, "\"");
}

/* Adds the 64-bit word W as a JSON string of decimal digits (Q-S10-3 (a)):
   JCS refuses a number above 2^53 - 1. */
static int out_word(JsonOut *out, uint64_t w) {
  char buf[32];
  snprintf(buf, sizeof buf, "\"%llu\"", (unsigned long long)w);
  return out_text(out, buf);
}

/* Adds the id: its JCS bytes as they are (a JSON string or number), or
   null. */
static int out_id(JsonOut *out, const unsigned char *id, size_t id_len) {
  return id == NULL ? out_text(out, "null") : out_put(out, (const char *)id, id_len);
}

/* The JCS form of the text, then '\n', appended to F with fflush and
   fsync. */
static int put_line(JsonOut *text, FILE *f) {
  JsonValue value;
  JcsBuf buf = {NULL, 0, 0};
  if (!json_parse(text->arena, text->data, text->len, &value, text->diag))
    return 0;
  buf.cap = 2u * text->len + 16u;
  buf.bytes = arena_alloc(text->arena, buf.cap);
  if (buf.bytes == NULL || jcs_write(&value, &buf) != JCS_OK)
    return 0;
  return fwrite(buf.bytes, 1u, buf.len, f) == buf.len && fputc('\n', f) != EOF && fflush(f) == 0 &&
         fsync(fileno(f)) == 0;
}

int report_call(FILE *f, const ReportCall *call) {
  Arena arena;
  Diag diag;
  JsonOut text;
  int ok;
  arena_init(&arena, REPORT_ARENA_BYTES);
  diag_init(&diag);
  text.arena = &arena;
  text.diag = &diag;
  text.data = NULL;
  text.len = 0;
  text.cap = 0;
  ok = out_text(&text, "{\"kind\":\"call\",\"permit\":1,\"tool\":") && out_string(&text, call->tool) &&
       out_text(&text, ",\"action\":") && out_string(&text, call->action) && out_text(&text, ",\"target\":\"") &&
       out_bytes(&text, call->target, call->target_len) && out_text(&text, "\",\"id\":") &&
       out_id(&text, call->id, call->id_len) && out_text(&text, ",\"digest\":") &&
       out_string(&text, call->digest) && out_text(&text, ",\"approval\":") && out_string(&text, call->approval) &&
       out_text(&text, ",\"approver\":") && out_string(&text, call->approver) && out_text(&text, ",\"cost\":") &&
       out_word(&text, call->cost) && out_text(&text, ",\"decision\":") &&
       out_string(&text, call->reason == NULL ? "permit" : "deny") && out_text(&text, ",\"reason\":") &&
       out_string(&text, call->reason) && out_text(&text, ",\"steps\":") && out_word(&text, call->steps) &&
       out_text(&text, ",\"budget\":") && out_word(&text, call->budget) && out_text(&text, "}") &&
       put_line(&text, f);
  arena_release(&arena);
  return ok;
}

int report_malformed(FILE *f, const unsigned char *id, size_t id_len, uint64_t steps, uint64_t budget) {
  Arena arena;
  Diag diag;
  JsonOut text;
  int ok;
  arena_init(&arena, REPORT_ARENA_BYTES);
  diag_init(&diag);
  text.arena = &arena;
  text.diag = &diag;
  text.data = NULL;
  text.len = 0;
  text.cap = 0;
  ok = out_text(&text, "{\"kind\":\"malformed\",\"permit\":1,\"id\":") && out_id(&text, id, id_len) &&
       out_text(&text, ",\"decision\":\"deny\",\"reason\":\"malformed\",\"steps\":") && out_word(&text, steps) &&
       out_text(&text, ",\"budget\":") && out_word(&text, budget) && out_text(&text, "}") && put_line(&text, f);
  arena_release(&arena);
  return ok;
}
