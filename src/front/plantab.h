/* The tables of `langc base` and `langc law LN` (S14 A4, Q-S14-3 (a),
   Q-S14-4 (a)): the base span of docs/PLAN.md section 4 and the laws L1
   to L8 of docs/PLAN.md section 7, with the PLAN text verbatim. */
#ifndef LANG_FRONT_PLANTAB_H
#define LANG_FRONT_PLANTAB_H

#include <stddef.h>

/* One row of the docs/PLAN.md section 4 table: its Name, Kind and Line
   columns, verbatim. */
typedef struct {
  const char *name;
  const char *kind;
  const char *line;
} BaseRow;

/* One law of docs/PLAN.md section 7: its id ("L1" to "L8"), its name and
   its statement, verbatim. */
typedef struct {
  const char *id;
  const char *name;
  const char *statement;
} LawRow;

/* The number of base rows: the 20 rows of docs/PLAN.md section 4. */
size_t base_count(void);
/* Base row INDEX in the order of docs/PLAN.md, or NULL when INDEX is not
   below base_count(). */
const BaseRow *base_at(size_t index);
/* The number of laws: the 8 laws of docs/PLAN.md section 7. */
size_t law_count(void);
/* Law INDEX in the order of docs/PLAN.md, or NULL when INDEX is not below
   law_count(). */
const LawRow *law_at(size_t index);
/* The law of ID, or NULL when no law has that id. */
const LawRow *law_find(const char *id);

#endif
