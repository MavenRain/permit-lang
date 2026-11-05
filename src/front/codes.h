/* The refusal codes of `langc explain CODE` (S14 A4, Q-S14-2 (a)), ported
   from accrue-lang src/front/codes.h (02964ec). */
#ifndef LANG_FRONT_CODES_H
#define LANG_FRONT_CODES_H

#include <stddef.h>

/* One refusal code of docs/host/README.md. The section is the README
   section that gives the code; the meaning is its README text verbatim,
   with its Markdown. */
typedef struct {
  const char *code;
  const char *section;
  const char *meaning;
} CodeRow;

/* The number of rows: one for each REFUSE_ code of src (26 at S14). */
size_t code_count(void);
/* Row INDEX in the order of docs/host/README.md, or NULL when INDEX is not
   below code_count(). */
const CodeRow *code_at(size_t index);
/* The row of CODE, or NULL when no row has that code. */
const CodeRow *code_find(const char *code);

#endif
