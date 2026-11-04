/* The `program` line of `langc report` (S10 U1, Q-S10-1 to Q-S10-3 (a)).
   It uses the checker, so permitd does not link it: the call lines of the
   report are host code. */
#ifndef PERMIT_REPORT_H
#define PERMIT_REPORT_H

#include <stdio.h>

#include "front/core.h"
#include "ir.h"
#include "jsonin.h"

/* Writes the program line of the checked program M, lowered into PROG, to
   OUT: one JSON object in its RFC 8785 (JCS) form, then '\n'. The keys are
   binds, budget, kind, laws, permit, scope and steps. A 64-bit word is a
   string of decimal digits. A program with no def start has a null
   budget, scope and steps. Returns 1, or 0 after a diagnostic. */
int report_program(Machine *m, const IrProgram *prog, FILE *out);

/* Builds the program line of M and PROG and reads it into VALUE with
   json_parse: the object of report_program before its JCS form. src/cedar.c
   reads it, so the two commands have one walker (S11 U1, Q-S11-1 (a)).
   Returns 1, or 0 after a diagnostic. */
int report_value(Machine *m, const IrProgram *prog, JsonValue *value);

#endif
