/* The rows of host/jsonin.c and host/jcs.c (S9 U2, Q-S9-3 (a)): the RFC
 * 8785 (JCS) form of a JSON text, the envelope of (target, payload), the
 * SHA-256 of an envelope (the real `hash` binding) and the RFC 6901 pointer.
 * Prints `jcs rows: N checked` and exits 0, or prints a
 * FAIL line on stderr for each bad row and exits 1. */
#include <stdio.h>
#include <string.h>

#include "jcs.h"
#include "sha256.h"

typedef struct {
  char kind;        /* j: JCS, f: JCS into a 4-byte buffer, e: envelope, h: SHA-256 of the envelope, p: pointer */
  const char *arg;  /* e: the path, p: the pointer, else "" */
  const char *text; /* the JSON text */
  const char *want; /* the bytes, or PARSE, MALFORMED, FULL, none */
} Row;

static const Row rows[] = {
    {'j', "", "{\"b\":1,\"a\":2}", "{\"a\":2,\"b\":1}"},
    {'j', "", "{ \"x\" : [ 1 , true , null , false ] }", "{\"x\":[1,true,null,false]}"},
    {'j', "", "\"\\u0007\\b\\t\\n\\f\\r\\\"\\\\\\/\xc3\xa9\\u001f\"", "\"\\u0007\\b\\t\\n\\f\\r\\\"\\\\/\xc3\xa9\\u001f\""},
    /* U+E000 before U+1F600 in UTF-8 bytes, after it in UTF-16 units */
    {'j', "", "{\"\\ue000\":1,\"\\ud83d\\ude00\":2}", "{\"\xf0\x9f\x98\x80\":2,\"\xee\x80\x80\":1}"},
    {'j', "", "{\"z\":{\"y\":1,\"x\":[{\"b\":0,\"a\":0}]},\"a\":[]}", "{\"a\":[],\"z\":{\"x\":[{\"a\":0,\"b\":0}],\"y\":1}}"},
    {'j', "", "[9007199254740991,-9007199254740991]", "[9007199254740991,-9007199254740991]"},
    {'j', "", "9007199254740992", "MALFORMED"},
    {'j', "", "-9007199254740992", "MALFORMED"},
    {'j', "", "[-0,0]", "[0,0]"},
    {'j', "", "1.5", "MALFORMED"},
    {'j', "", "1e3", "MALFORMED"},
    {'j', "", "{\"a\":1,\"a\":2}", "PARSE"},
    {'j', "", "{\"a\":{\"b\":1},\"b\":{\"b\":2}}", "{\"a\":{\"b\":1},\"b\":{\"b\":2}}"},
    {'j', "", "\"\\ud800\"", "PARSE"},
    {'j', "", "\"\xff\"", "PARSE"},
    {'j', "", "\"\\u0000\"", "\"\\u0000\""},
    {'f', "", "\"abcd\"", "FULL"},
    {'e', "docs/report", "{\"name\":\"restart\",\"arguments\":{\"b\":2,\"a\":1}}",
     "{\"payload\":{\"arguments\":{\"a\":1,\"b\":2},\"name\":\"restart\"},\"target\":[\"docs\",\"report\"]}"},
    {'e', "", "{}", "{\"payload\":{},\"target\":[]}"},
    {'e', "a\"b/c", "1", "{\"payload\":1,\"target\":[\"a\\\"b\",\"c\"]}"},
    /* the first envelope row: printf %s BYTES | shasum -a 256 */
    {'h', "docs/report", "{\"name\":\"restart\",\"arguments\":{\"b\":2,\"a\":1}}",
     "c0c0c326a66a39dfea80d1162a69e76d4162921f6f0a37786abb8167a6830939"},
    {'p', "/a~1b/m~0n/1", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "20"},
    {'p', "", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "{\"\":3,\"a/b\":{\"m~n\":[10,20]},\"c\":[{\"d\":\"x\"}]}"},
    {'p', "/", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "3"},
    {'p', "/c/0/d", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "\"x\""},
    {'p', "/a~1b/m~0n/2", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "/a~1b/m~0n/01", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "/c/-", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "/a~2b", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "/a~", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "a", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
    {'p', "/a~1b/m~1n", "{\"a/b\":{\"m~n\":[10,20]},\"\":3,\"c\":[{\"d\":\"x\"}]}", "none"},
};

/* The text of RES into GOT (CAP bytes): the bytes of B, or the name. */
static void result_text(JcsResult res, const JcsBuf *b, char *got, size_t cap) {
  switch (res) {
    case JCS_OK:
      snprintf(got, cap, "%.*s", (int)b->len, (const char *)b->bytes);
      return;
    case JCS_MALFORMED:
      snprintf(got, cap, "MALFORMED");
      return;
    case JCS_FULL:
      snprintf(got, cap, "FULL");
      return;
  }
}

/* The SHA-256 of the bytes of B as 64 lower-case hex characters into GOT
   (CAP bytes), or the name of RES. */
static void digest_text(JcsResult res, const JcsBuf *b, char *got, size_t cap) {
  unsigned char digest[32];
  if (res != JCS_OK) {
    result_text(res, b, got, cap);
    return;
  }
  sha256(b->bytes, b->len, digest);
  for (size_t i = 0; i < 32 && 2 * i + 2 < cap; i++) snprintf(got + 2 * i, cap - 2 * i, "%02x", digest[i]);
}

/* The text of ROW into GOT (CAP bytes). */
static void run(const Row *row, char *got, size_t cap) {
  static unsigned char bytes[4096];
  JcsBuf b = {bytes, row->kind == 'f' ? 4 : sizeof bytes, 0};
  Arena arena;
  Diag diag;
  JsonValue v;
  const JsonValue *at;
  arena_init(&arena, 1u << 20);
  diag_init(&diag);
  if (!json_parse(&arena, row->text, strlen(row->text), &v, &diag))
    snprintf(got, cap, "PARSE");
  else if (row->kind == 'p' && (at = jcs_pointer(&v, row->arg, strlen(row->arg))) == NULL)
    snprintf(got, cap, "none");
  else if (row->kind == 'p')
    result_text(jcs_write(at, &b), &b, got, cap);
  else if (row->kind == 'e')
    result_text(jcs_envelope(row->arg, strlen(row->arg), &v, &b), &b, got, cap);
  else if (row->kind == 'h')
    digest_text(jcs_envelope(row->arg, strlen(row->arg), &v, &b), &b, got, cap);
  else
    result_text(jcs_write(&v, &b), &b, got, cap);
  arena_release(&arena);
}

int main(void) {
  size_t count = sizeof rows / sizeof rows[0], bad = 0, i;
  for (i = 0; i < count; i++) {
    char got[4200];
    run(&rows[i], got, sizeof got);
    if (strcmp(got, rows[i].want) != 0) {
      fprintf(stderr, "FAIL jcs row %zu (%c %s): want %s, got %s\n", i + 1, rows[i].kind, rows[i].arg, rows[i].want, got);
      bad++;
    }
  }
  if (bad > 0)
    return 1;
  printf("jcs rows: %zu checked\n", count);
  return 0;
}
