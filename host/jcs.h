/* The canonical bytes of permitd (S9 U2, Q-S9-3 (a)): the RFC 8785 (JCS)
   form of a JSON value, the envelope of (target, payload) and the RFC
   6901 pointer of a `bind` line (PLAN sec 8). */
#ifndef PERMIT_HOST_JCS_H
#define PERMIT_HOST_JCS_H

#include <stddef.h>

#include "jsonin.h"

/* The largest magnitude of a JCS number (2^53 - 1). */
#define JCS_NUMBER_MAX "9007199254740991"

typedef enum {
  JCS_OK,
  JCS_MALFORMED, /* a fraction, an exponent or a magnitude over 2^53 - 1 */
  JCS_FULL       /* the bytes do not fit in the buffer */
} JcsResult;

/* An output buffer: CAP bytes at BYTES, LEN of them written. */
typedef struct {
  unsigned char *bytes;
  size_t cap;
  size_t len;
} JcsBuf;

/* Writes the RFC 8785 form of VALUE at the end of OUT: the members of an
   object sorted by the UTF-16 code units of their keys (3.2.3), the string
   escapes of 3.2.2.2 only, no white space. A number is
   -?(0|[1-9][0-9]*) with a magnitude of at most 2^53 - 1; -0 writes 0. */
JcsResult jcs_write(const JsonValue *value, JcsBuf *out);

/* Writes the envelope {"payload":P,"target":[S...]} at the end of OUT: P is
   the JCS form of PAYLOAD, each S is one segment of the LEN bytes of PATH
   (split at '/'; an empty PATH gives no segment). */
JcsResult jcs_envelope(const char *path, size_t len, const JsonValue *payload, JcsBuf *out);

/* The value at the RFC 6901 pointer of the LEN bytes of POINTER in ROOT, or
   NULL when the pointer is not good (a '~' not followed by '0' or '1') or
   names no value. "" is ROOT; an array token is 0 or [1-9][0-9]*. */
const JsonValue *jcs_pointer(const JsonValue *root, const char *pointer, size_t len);

#endif
