/* A small JSON reader for permitd (S9 U2, Q-S9-3 (a)): a copy of
   relay-lang 4781ca7 src/front/jsonin.h (relay S13 Q-S13-3, ported from
   cairn-lang 60768e8). Change: json_parse fails on a duplicate key in one
   object. */
#ifndef PERMIT_HOST_JSONIN_H
#define PERMIT_HOST_JSONIN_H

#include <stddef.h>

#include "front/base.h"

/* The deepest nesting of arrays and objects that json_parse accepts. */
#define JSON_DEPTH_MAX 64

typedef enum {
  JSON_NULL,
  JSON_FALSE,
  JSON_TRUE,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} JsonKind;

/* One JSON value in the arena. The items of an array and the members of
   an object are a list through NEXT. */
typedef struct JsonValue JsonValue;
struct JsonValue {
  JsonKind kind;
  const char *text;       /* STRING: the decoded UTF-8 bytes. NUMBER: the source text. NUL-terminated. */
  size_t len;             /* the byte count of TEXT, without the NUL */
  const JsonValue *first; /* ARRAY, OBJECT: the first item or member, or NULL */
  size_t count;           /* ARRAY, OBJECT: the count of items or members */
  const char *key;        /* a member of an object: the decoded key, NUL-terminated */
  size_t key_len;
  const JsonValue *next;  /* the next item or member, or NULL */
};

/* Reads the LEN bytes of TEXT as one JSON value (RFC 8259) with optional
   white space around it. A duplicate key in one object, a lone surrogate
   and invalid UTF-8 are failures. Returns 1, or 0 after a USAGE or OOM diagnostic. */
int json_parse(Arena *arena, const char *text, size_t len, JsonValue *out, Diag *diag);
/* The first member KEY of OBJECT, or NULL when OBJECT is not an object or
   has no member KEY. */
const JsonValue *json_member(const JsonValue *object, const char *key);
/* The text of VALUE when VALUE is a string without a NUL byte, else NULL. */
const char *json_get_string(const JsonValue *value);

#endif
