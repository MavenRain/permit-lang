/* The call lines of `permitd --report FILE` (S10 U2, Q-S10-1 to Q-S10-3
   (a)). Host code with no checker: the program line of the report is
   `langc report` (src/report.c). */
#ifndef PERMIT_HOST_REPORT_H
#define PERMIT_HOST_REPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* One decided `tools/call` (one Entry of the Trace, PLAN sec 3). */
typedef struct {
  const char *tool;         /* the tool name */
  const char *action;       /* the Action arm name, or NULL */
  const char *target;       /* the target path text, TARGET_LEN bytes */
  size_t target_len;
  const unsigned char *id;  /* the JCS bytes of the JSON-RPC id, ID_LEN of them */
  size_t id_len;
  const char *digest;       /* the hex SHA-256 of the envelope that the gate checked */
  const char *approval;     /* the hex of the named digest, or NULL */
  const char *approver;     /* FILE:LINE, tty, or NULL */
  const char *reason;       /* the Reason arm name of a deny, or NULL for a permit */
  uint64_t cost;
  uint64_t steps;           /* word 0 after decide */
  uint64_t budget;          /* word 1 after decide */
} ReportCall;

/* Appends the call line of CALL to F: one JSON object in its RFC 8785
   (JCS) form, then '\n', then fflush and fsync. The keys are action,
   approval, approver, budget, cost, decision, digest, id, kind, permit,
   reason, steps, target and tool. A 64-bit word is a string of decimal
   digits. Returns 1, or 0 when a step fails. */
int report_call(FILE *f, const ReportCall *call);

/* Appends the malformed line to F as report_call does. ID is the JCS
   bytes of the JSON-RPC id (ID_LEN of them), or NULL for a parse error
   (id null). The keys are budget, decision, id, kind, permit, reason and
   steps. */
int report_malformed(FILE *f, const unsigned char *id, size_t id_len, uint64_t steps, uint64_t budget);

#endif
