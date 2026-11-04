/* The JSON text writer of `langc report` and `langc check --json` (S13
   U2, Q-S13-4 (a)): the JsonOut helpers of src/report.c, moved here. The
   text is not canonical: out_line reads it with json_parse and writes the
   JCS form (Q-S10-2 (a)). */
#ifndef PERMIT_JSONOUT_H
#define PERMIT_JSONOUT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "front/base.h"

/* The JSON text of a line, on the arena: the JsonOut layout of
   evolve-lang 550e73b src/json.c. The buffer starts at 4096 bytes and
   doubles when the text does not fit. Each put returns 1, or 0 after a
   diagnostic, so the calls chain with &&. DEF is the def of the
   diagnostics of the puts. */
typedef struct {
  Arena *arena;
  Diag *diag;
  const char *def;
  char *data; /* NUL-terminated; NULL before the first put */
  size_t len;
  size_t cap;
} JsonOut;

/* Adds the N bytes at TEXT. */
int out_put(JsonOut *out, const char *text, size_t n);
/* Adds TEXT. */
int out_text(JsonOut *out, const char *text);
/* Adds the N bytes at TEXT inside a JSON string: '"', '\\' and each byte
   below 0x20 as a \u escape, each byte that does not start a UTF-8 scalar
   as � (S13: json_parse refuses invalid UTF-8), each other byte as it
   is. */
int out_bytes(JsonOut *out, const char *text, size_t n);
/* Adds TEXT as a JSON string, or null when TEXT is NULL. */
int out_string(JsonOut *out, const char *text);
/* Adds the 64-bit word W as a JSON string of decimal digits (Q-S10-3 (a)):
   JCS refuses a number above 2^53 - 1. */
int out_word(JsonOut *out, uint64_t w);
/* Adds ",", except before item 0. */
int out_comma(JsonOut *out, size_t i);
/* Reads the text of OUT with json_parse, then writes its RFC 8785 (JCS)
   form and '\n' to FILE (S13 U2: the loop of report_program). */
int out_line(JsonOut *out, FILE *file);

#endif
