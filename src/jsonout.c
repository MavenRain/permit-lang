/* The JSON text writer. See jsonout.h. */
#include <stdio.h>
#include <string.h>

#include "jcs.h"
#include "jsonin.h"
#include "jsonout.h"

int out_put(JsonOut *out, const char *text, size_t n) {
  size_t cap = out->cap == 0 ? 4096u : out->cap;
  char *data;
  while (cap < out->len + n + 1u)
    cap *= 2u;
  if (cap != out->cap) {
    data = arena_alloc(out->arena, cap);
    if (data == NULL)
      return diag_fail(out->diag, "OOM", out->def, "out of memory");
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

int out_text(JsonOut *out, const char *text) {
  return out_put(out, text, strlen(text));
}

/* The byte count of the UTF-8 scalar at TEXT, or 0 when the bytes are not
   one: utf8_char_size of host/jsonin.c, static here. */
static size_t utf8_char_size(const char *text, size_t len) {
  unsigned char c;
  unsigned char next;
  size_t width;
  size_t i;
  if (len == 0)
    return 0;
  c = (unsigned char)text[0];
  width = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
  if (width == 0 || width > len)
    return 0;
  for (i = 1; i < width; i++)
    if (((unsigned char)text[i] & 0xc0) != 0x80)
      return 0;
  next = width >= 3 ? (unsigned char)text[1] : 0u;
  if (width >= 3 &&((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) || (c == 0xf0 && next < 0x90) ||
                     (c == 0xf4 && next >= 0x90)))
    return 0;
  return width;
}

int out_bytes(JsonOut *out, const char *text, size_t n) {
  char esc[8];
  size_t i;
  size_t width = 1;
  int ok = 1;
  for (i = 0; ok && i < n; i += width == 0 ? 1u : width) {
    unsigned char c = (unsigned char)text[i];
    width = utf8_char_size(text + i, n - i);
    snprintf(esc, sizeof esc, "\\u%04x", (unsigned)c);
    ok = width == 0  ? out_text(out, "\\ufffd")
         : width > 1 ? out_put(out, text + i, width)
         : c >= 0x20u && c != '"' && c != '\\' ? out_put(out, text + i, 1u)
                                                : out_text(out, esc);
  }
  return ok;
}

/* One byte of a JSON string of json_string_n. */
static void put_char(unsigned char c, FILE *file) {
  const char *escape = c == '"' ? "\\\"" : c == '\\' ? "\\\\" : c == '\n' ? "\\n" : c == '\t' ? "\\t" : NULL;
  if (escape != NULL) {
    fputs(escape, file);
    return;
  }
  if (c < 0x20u) {
    fprintf(file, "\\u%04x", (unsigned)c);
    return;
  }
  fputc(c, file);
}

void json_string_n(const char *text, size_t len, FILE *file) {
  size_t pos;
  size_t width;
  size_t i;
  fputc('"', file);
  for (pos = 0; pos < len; pos += width == 0 ? 1u : width) {
    width = utf8_char_size(text + pos, len - pos);
    /* Paths and truncated diagnostic buffers can hold invalid bytes. */
    if (width == 0)
      fputs("\\ufffd", file);
    for (i = 0; i < width; i++)
      put_char((unsigned char)text[pos + i], file);
  }
  fputc('"', file);
}

int out_string(JsonOut *out, const char *text) {
  return text == NULL ? out_text(out, "null")
                      : out_text(out, "\"") && out_bytes(out, text, strlen(text)) && out_text(out, "\"");
}

int out_word(JsonOut *out, uint64_t w) {
  char buf[32];
  snprintf(buf, sizeof buf, "\"%llu\"", (unsigned long long)w);
  return out_text(out, buf);
}

int out_comma(JsonOut *out, size_t i) {
  return i == 0 || out_text(out, ",");
}

int out_line(JsonOut *out, FILE *file) {
  JsonValue value;
  JcsBuf buf = {NULL, 0, 0};
  JcsResult res = JCS_FULL;
  size_t cap;
  if (!json_parse(out->arena, out->data, out->len, &value, out->diag))
    return 0;
  /* The buffer starts at 4096 bytes and doubles while the JCS form does not
     fit. */
  for (cap = 4096u; res == JCS_FULL; cap *= 2u) {
    buf.bytes = arena_alloc(out->arena, cap);
    if (buf.bytes == NULL)
      return diag_fail(out->diag, "OOM", out->def, "out of memory");
    buf.cap = cap;
    buf.len = 0;
    res = jcs_write(&value, &buf);
  }
  if (res != JCS_OK)
    return diag_fail(out->diag, "INTERNAL", out->def, "the JSON line has no JCS form");
  return fwrite(buf.bytes, 1u, buf.len, file) == buf.len && fputc('\n', file) != EOF && fflush(file) == 0
           ? 1
           : diag_fail(out->diag, "IO", out->def, "cannot write the JSON line");
}
