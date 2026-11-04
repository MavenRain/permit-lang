/* The `langc cedar` text (S11 U1, Q-S11-1 to Q-S11-3 (a)): T3, the static
   scope of def start as Cedar permit policies. It reads the program line of
   report.c (report_value), so the two commands have one walker. */
#ifndef PERMIT_CEDAR_H
#define PERMIT_CEDAR_H

#include <stdio.h>

#include "front/core.h"
#include "ir.h"

/* Writes the Cedar text of the checked program M, lowered into PROG, to
   OUT: the header, one permit policy for each grant of the start scope in
   walk order (duplicates kept), then one comment for each part that the
   export leaves out. A program with no def start gets header line 1 and
   one comment. Returns 1, or 0 after a diagnostic (IO for a failed
   write). */
int cedar_program(Machine *m, const IrProgram *prog, FILE *out);

#endif
