#include "jcs.h"

#include <string.h>

static JcsResult put(JcsBuf *b, const void *bytes, size_t n) {
  if (n > b->cap - b->len) return JCS_FULL;
  memcpy(b->bytes + b->len, bytes, n);
  b->len += n;
  return JCS_OK;
}

static JcsResult put_text(JcsBuf *b, const char *text) {
  return put(b, text, strlen(text));
}

/* The scalar of the valid UTF-8 at S (N > 0 bytes) into *CP. Returns its
   byte count. */
static size_t decode(const unsigned char *s, size_t n, unsigned long *cp) {
  size_t width = s[0] < 0x80 ? 1 : s[0] < 0xE0 ? 2 : s[0] < 0xF0 ? 3 : 4;
  unsigned long v = width == 1 ? s[0] : s[0] & (0x7Fu >> width);
  width = width > n ? n : width;
  for (size_t i = 1; i < width; i++) v = (v << 6) | (s[i] & 0x3Fu);
  *cp = v;
  return width;
}

/* The UTF-16 code units of one key, one at a time. */
typedef struct {
  const unsigned char *s;
  size_t len;
  size_t pos;
  unsigned long low; /* the low surrogate after a high one, or 0 */
} Units;

/* The next code unit, or -1 at the end. */
static long next_unit(Units *u) {
  unsigned long cp, low = u->low;
  u->low = 0;
  if (low != 0) return (long)low;
  if (u->pos >= u->len) return -1;
  u->pos += decode(u->s + u->pos, u->len - u->pos, &cp);
  if (cp < 0x10000) return (long)cp;
  u->low = 0xDC00 + ((cp - 0x10000) & 0x3FF);
  return (long)(0xD800 + ((cp - 0x10000) >> 10));
}

/* The order of the keys of A and B by UTF-16 code units (RFC 8785 3.2.3). */
static int key_cmp(const JsonValue *a, const JsonValue *b) {
  Units x = {(const unsigned char *)a->key, a->key_len, 0, 0};
  Units y = {(const unsigned char *)b->key, b->key_len, 0, 0};
  long p, q;
  do {
    p = next_unit(&x);
    q = next_unit(&y);
  } while (p == q && p >= 0);
  return (p > q) - (p < q);
}

/* The member of OBJECT with the least key above the key of AFTER (NULL:
   the least key). The keys are distinct (json_parse). */
static const JsonValue *next_member(const JsonValue *object, const JsonValue *after) {
  const JsonValue *best = NULL;
  for (const JsonValue *m = object->first; m != NULL; m = m->next)
    if ((after == NULL || key_cmp(m, after) > 0) && (best == NULL || key_cmp(m, best) < 0)) best = m;
  return best;
}

/* A JSON string of the N bytes at TEXT with the RFC 8785 3.2.2.2 escapes:
   \" \\ \b \t \n \f \r, \u00xx (lower case) for the other bytes below
   0x20, each other byte as it is. */
static JcsResult write_string(const char *text, size_t n, JcsBuf *b) {
  static const char hex[] = "0123456789abcdef";
  JcsResult res = put(b, "\"", 1);
  for (size_t i = 0; i < n && res == JCS_OK; i++) {
    unsigned char c = (unsigned char)text[i];
    const char *short_form = c == '"' ? "\\\"" : c == '\\' ? "\\\\" : c == '\b' ? "\\b" : c == '\t' ? "\\t"
                           : c == '\n' ? "\\n" : c == '\f' ? "\\f" : c == '\r' ? "\\r" : NULL;
    char u[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
    res = short_form != NULL ? put_text(b, short_form) : c < 0x20 ? put(b, u, 6) : put(b, &c, 1);
  }
  return res == JCS_OK ? put(b, "\"", 1) : res;
}

/* A number: -?(0|[1-9][0-9]*) (json_parse checked the grammar) with a
   magnitude of at most 2^53 - 1. -0 writes 0. */
static JcsResult write_number(const JsonValue *v, JcsBuf *b) {
  size_t sign = v->text[0] == '-';
  const char *digits = v->text + sign;
  size_t n = v->len - sign;
  size_t max = strlen(JCS_NUMBER_MAX);
  if (strcspn(digits, ".eE") != n || n > max || (n == max && memcmp(digits, JCS_NUMBER_MAX, n) > 0))
    return JCS_MALFORMED;
  if (n == 1 && digits[0] == '0') return put(b, "0", 1);
  return put(b, v->text, v->len);
}

static JcsResult write_members(const JsonValue *object, JcsBuf *b) {
  JcsResult res = put(b, "{", 1);
  const JsonValue *m = next_member(object, NULL);
  for (size_t i = 0; m != NULL && res == JCS_OK; i++, m = next_member(object, m)) {
    res = i > 0 ? put(b, ",", 1) : JCS_OK;
    res = res == JCS_OK ? write_string(m->key, m->key_len, b) : res;
    res = res == JCS_OK ? put(b, ":", 1) : res;
    res = res == JCS_OK ? jcs_write(m, b) : res;
  }
  return res == JCS_OK ? put(b, "}", 1) : res;
}

static JcsResult write_items(const JsonValue *array, JcsBuf *b) {
  JcsResult res = put(b, "[", 1);
  const JsonValue *item = array->first;
  for (size_t i = 0; item != NULL && res == JCS_OK; i++, item = item->next) {
    res = i > 0 ? put(b, ",", 1) : JCS_OK;
    res = res == JCS_OK ? jcs_write(item, b) : res;
  }
  return res == JCS_OK ? put(b, "]", 1) : res;
}

JcsResult jcs_write(const JsonValue *value, JcsBuf *out) {
  switch (value->kind) {
    case JSON_NULL:
      return put_text(out, "null");
    case JSON_FALSE:
      return put_text(out, "false");
    case JSON_TRUE:
      return put_text(out, "true");
    case JSON_NUMBER:
      return write_number(value, out);
    case JSON_STRING:
      return write_string(value->text, value->len, out);
    case JSON_ARRAY:
      return write_items(value, out);
    case JSON_OBJECT:
      return write_members(value, out);
  }
  return JCS_MALFORMED;
}

JcsResult jcs_envelope(const char *path, size_t len, const JsonValue *payload, JcsBuf *out) {
  JcsResult res = put_text(out, "{\"payload\":");
  size_t start = 0;
  res = res == JCS_OK ? jcs_write(payload, out) : res;
  res = res == JCS_OK ? put_text(out, ",\"target\":[") : res;
  for (size_t i = 0; len > 0 && i <= len && res == JCS_OK; i++)
    if (i == len || path[i] == '/') {
      res = start > 0 ? put(out, ",", 1) : JCS_OK;
      res = res == JCS_OK ? write_string(path + start, i - start, out) : res;
      start = i + 1;
    }
  return res == JCS_OK ? put_text(out, "]}") : res;
}

/* 1 when each '~' of the N bytes at T is followed by '0' or '1'. */
static int escapes_ok(const char *t, size_t n) {
  size_t i = 0;
  while (i < n && (t[i] != '~' || (i + 1 < n && (t[i + 1] == '0' || t[i + 1] == '1')))) i += t[i] == '~' ? 2 : 1;
  return i >= n;
}

/* 1 when the escaped token T (N bytes) is the key K (KLEN bytes): ~1 is
   '/', ~0 is '~'. */
static int token_is(const char *t, size_t n, const char *k, size_t klen) {
  size_t i = 0, j = 0;
  while (i < n && j < klen && (t[i] == '~' ? (t[i + 1] == '1' ? '/' : '~') : t[i]) == k[j]) {
    i += t[i] == '~' ? 2 : 1;
    j++;
  }
  return i == n && j == klen;
}

/* The item of ARRAY at the token T (N bytes): 0 or [1-9][0-9]*. */
static const JsonValue *item_at(const JsonValue *array, const char *t, size_t n) {
  size_t index = 0, i = 0;
  const JsonValue *item = array->first;
  if (n == 0 || (n > 1 && t[0] == '0')) return NULL;
  while (i < n && t[i] >= '0' && t[i] <= '9' && index <= array->count) index = index * 10 + (size_t)(t[i++] - '0');
  if (i < n || index >= array->count) return NULL;
  for (; index > 0; index--) item = item->next;
  return item;
}

const JsonValue *jcs_pointer(const JsonValue *root, const char *pointer, size_t len) {
  const JsonValue *v = root;
  size_t pos = 0;
  if (!escapes_ok(pointer, len) || (len > 0 && pointer[0] != '/')) return NULL;
  while (v != NULL && pos < len) {
    size_t start = pos + 1, end = start;
    while (end < len && pointer[end] != '/') end++;
    const JsonValue *m = v->kind == JSON_OBJECT ? v->first : NULL;
    while (m != NULL && !token_is(pointer + start, end - start, m->key, m->key_len)) m = m->next;
    v = v->kind == JSON_ARRAY ? item_at(v, pointer + start, end - start) : m;
    pos = end;
  }
  return v;
}
