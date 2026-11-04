/* The rows of host/permitd.c (S9 U3, N6, docs/MCP.md): each row runs
 * build/permitd on test/host/deploy.lang with build/fakedeploy as the
 * child, sends one line at a time and waits for each answer.
 *
 *   hosttool PERMITD FAKEDEPLOY MODULE DIR
 *
 * Each row has its own directory under DIR with an empty approvals file
 * `appr` and a bad state file `bad`; `{T}` in a flag or a script line is
 * that directory. Prints `host rows: N checked` and exits 0, or prints a
 * FAIL line on stderr for each bad row and exits 1. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

enum { TEXT_CAP = 8192, ARG_MAX_COUNT = 32, WAIT_MS = 10000 };

/* A script line: `> L` sends L, `< L` wants L as the next answer line,
 * `+ L` appends L to {T}/appr, `= L` wants L as the next line of the
 * fakedeploy log (no `=` line: the log is empty or absent), `# L` wants
 * L as the next line of the report file {T}/rep (no `#` line: the file
 * is empty or absent), `! L` closes the input of permitd, wants exit 0
 * and no extra answer, and starts permitd again with the same flags (L
 * is a note). */
typedef struct {
  const char *name;
  const char *const *flags; /* permitd flags before MODULE */
  const char *const *child; /* fakedeploy flags after LOG */
  const char *const *script;
  int rc; /* the exit code of permitd */
} Row;

#define D1 "003f7d4a51a9398c030730021fae95a4ef07e3a395e43f0648ea9929b81445b7"
#define D2 "324434861884b10f2dfab9e79c2c2333133efd891279c9baed0e2413cf82621f"
#define L(...) ((const char *const[]){__VA_ARGS__, NULL})
#define NONE ((const char *const[]){NULL})
#define CALL(ID, PARAMS) "> {\"jsonrpc\":\"2.0\",\"id\":" ID ",\"method\":\"tools/call\",\"params\":" PARAMS "}"
#define DENY(ID, WHY) "< {\"jsonrpc\":\"2.0\",\"id\":" ID ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"permit-lang " WHY "\"}],\"isError\":true}}"
#define OK(ID, TOOL) "< {\"jsonrpc\":\"2.0\",\"id\":" ID ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"ok " TOOL "\"}],\"isError\":false}}"
#define LOG(ID, PARAMS) "= {\"jsonrpc\":\"2.0\",\"id\":" ID ",\"method\":\"tools/call\",\"params\":" PARAMS "}"
#define INSPECT_PROD "{\"name\":\"inspect\",\"arguments\":{\"target\":\"prod\"}}"
#define INSPECT_PROD_JCS "{\"arguments\":{\"target\":\"prod\"},\"name\":\"inspect\"}"
#define EXEC_D1 "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/web\"},\"_meta\":{\"permit/approval\":\"" D1 "\"}}"
#define EXEC_D1_JCS "{\"arguments\":{\"target\":\"prod/web\"},\"name\":\"execute\"}"
/* The digests of the sync_left call of D10 and the restart call of D11. */
#define D_LEFT "a534cd0c709d793939aeaacdfddcc80be8452a2cce26f51e3512d57444623517"
#define D_RESTART "ce90eb9b981bc6b376ebd8b81028820219122ae345f1cfabd7f643828976db52"
#define STEP(ID) CALL(ID, INSPECT_PROD), OK(ID, "inspect"), LOG(ID, INSPECT_PROD_JCS)
#define SYNC(TOOL) "{\"name\":\"" TOOL "\",\"arguments\":{\"left\":\"prod\",\"right\":\"dev\"},\"_meta\":{\"permit/approval\":\"" D_LEFT "\"}}"
#define SYNC_JCS(TOOL) "{\"arguments\":{\"left\":\"prod\",\"right\":\"dev\"},\"name\":\"" TOOL "\"}"
#define PROD(TOOL) "{\"name\":\"" TOOL "\",\"arguments\":{\"target\":\"prod\"},\"_meta\":{\"permit/approval\":\"" D_RESTART "\"}}"
#define PROD_JCS(TOOL) "{\"arguments\":{\"target\":\"prod\"},\"name\":\"" TOOL "\"}"
/* The digests of the sync_right call of R1, the inspect prod and staging
 * calls and the propose prod/web call (Q-S10-3 (a) report rows). */
#define D_RIGHT "37653f0d84e4235179188086d35cdd2fb632c369aafb07c41fbaf10ee52f118f"
#define D_PROD "39367f1f1db7f4c920f5323faa0ef0b4c07b78ce4f7a50175ad6396828b5a24e"
#define D_STAGING "19d95ad0d23c13cc1fed08da123a0c9931300307441900c037bc58ec7fcfe6e7"
#define D_PROPOSE "6e41b5abd60604cbc90b55845a5a874520c32642154ea8e959ff89e492b8bb55"
#define Q(TEXT) "\"" TEXT "\""
/* A call line and a malformed line of {T}/rep, keys in JCS order. ACTION,
 * APPROVAL, APPROVER, ID and REASON are JSON text (null or a string); the
 * other values are the text of a string. */
#define REP(ACTION, APPROVAL, APPROVER, BUDGET, COST, DECISION, DIGEST, ID, REASON, STEPS, TARGET, TOOL)           \
  "# {\"action\":" ACTION ",\"approval\":" APPROVAL ",\"approver\":" APPROVER ",\"budget\":\"" BUDGET               \
  "\",\"cost\":\"" COST "\",\"decision\":\"" DECISION "\",\"digest\":\"" DIGEST "\",\"id\":" ID                     \
  ",\"kind\":\"call\",\"permit\":1,\"reason\":" REASON ",\"steps\":\"" STEPS "\",\"target\":\"" TARGET             \
  "\",\"tool\":\"" TOOL "\"}"
#define MAL(BUDGET, ID, STEPS)                                                                                      \
  "# {\"budget\":\"" BUDGET "\",\"decision\":\"deny\",\"id\":" ID                                                  \
  ",\"kind\":\"malformed\",\"permit\":1,\"reason\":\"malformed\",\"steps\":\"" STEPS "\"}"
#define PROPOSE "{\"name\":\"propose\",\"arguments\":{\"target\":\"prod/web\"}}"
#define PROPOSE_JCS "{\"arguments\":{\"target\":\"prod/web\"},\"name\":\"propose\"}"
#define PROPOSE_STEP(ID, BUDGET, STEPS)                                                                             \
  CALL(ID, PROPOSE), OK(ID, "propose"), LOG(ID, PROPOSE_JCS),                                                      \
      REP(Q("propose"), "null", "null", BUDGET, "1", "permit", D_PROPOSE, ID, "null", STEPS, "prod/web", "propose")
#define INSPECT_REP(BUDGET, COST, DECISION, ID, REASON, STEPS)                                                      \
  REP(Q("inspect"), "null", "null", BUDGET, COST, DECISION, D_PROD, ID, REASON, STEPS, "prod", "inspect")

static const Row rows[] = {
    {"not JSON", NONE, NONE,
     L("> not json",
       "< {\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"permit-lang deny malformed\"}}"),
     0},
    {"batch is refused before forwarding", NONE, NONE,
     L("> [{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"execute\",\"arguments\":{\"target\":\"prod\"}}}]",
       "< {\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"permit-lang deny malformed\"}}",
       STEP("2"), STEP("3"), STEP("4"), STEP("5"), STEP("6"),
       CALL("7", INSPECT_PROD), DENY("7", "deny boundSpent")), 0},
    {"scalar is refused before forwarding", NONE, NONE,
     L("> null",
       "< {\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"permit-lang deny malformed\"}}"),
     0},
    {"no bind row", NONE, NONE,
     L(CALL("1", "{\"name\":\"nope\",\"arguments\":{\"target\":\"prod\"}}"), DENY("1", "deny malformed")), 0},
    {"target not a string", NONE, NONE,
     L(CALL("1", "{\"name\":\"inspect\",\"arguments\":{\"target\":7}}"), DENY("1", "deny malformed")), 0},
    {"bad approval hex", L("--approvals", "{T}/appr"), NONE,
     L(CALL("1", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod\"},\"_meta\":{\"permit/approval\":\"ABC\"}}"),
       DENY("1", "deny malformed")),
     0},
    {"--error", NONE, L("--error", "inspect"),
     L(CALL("\"a\"", INSPECT_PROD), DENY("\"a\"", "toolFailed error"), LOG("\"a\"", INSPECT_PROD_JCS)), 0},
    {"--exit", NONE, L("--exit", "inspect"),
     L(CALL("1", INSPECT_PROD), DENY("1", "toolFailed closed"), "> {\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\"}",
       "< {\"jsonrpc\":\"2.0\",\"id\":2,\"error\":{\"code\":-32603,\"message\":\"permit-lang upstream closed\"}}",
       LOG("1", INSPECT_PROD_JCS)),
     0},
    {"bad state file", L("--state", "{T}/bad"), NONE, NONE, 2},
    {"forwarded bytes", L("--approvals", "{T}/appr"), NONE,
     L("+ " D1, "+ " D2, CALL("1", EXEC_D1), OK("1", "execute"),
       CALL("2", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/web\"},\"_meta\":{\"progressToken\":5,\"permit/approval\":\"" D2 "\"}}"),
       OK("2", "execute"), LOG("1", "{\"arguments\":{\"target\":\"prod/web\"},\"name\":\"execute\"}"),
       LOG("2", "{\"_meta\":{\"progressToken\":5},\"arguments\":{\"target\":\"prod/web\"},\"name\":\"execute\"}")),
     0},
    {"approval key prefix keeps its metadata", NONE, NONE,
     L(CALL("1", "{\"name\":\"inspect\",\"arguments\":{\"target\":\"prod\"},\"_meta\":{\"permit/approval\":\"" D1 "\",\"permit/approval\\u0000keep\":\"keep\"}}"),
       OK("1", "inspect"),
       LOG("1", "{\"_meta\":{\"permit/approval\\u0000keep\":\"keep\"},\"arguments\":{\"target\":\"prod\"},\"name\":\"inspect\"}")),
     0},
    {"deny isError", NONE, NONE,
     L(CALL("1", "{\"name\":\"inspect\",\"arguments\":{\"target\":\"dev/web\"}}"), DENY("1", "deny scopeMiss"),
       CALL("2", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/web\"}}"), DENY("2", "deny noApproval")),
     0},
    {"--cost", L("--cost", "inspect=11"), NONE, L(CALL("1", INSPECT_PROD), DENY("1", "deny budgetShort")), 0},
    {"--cost uint64 max", L("--cost", "inspect=18446744073709551615"), NONE,
     L(CALL("1", INSPECT_PROD), DENY("1", "deny budgetShort")), 0},
    {"--cost uint64 overflow", L("--cost", "inspect=18446744073709551616"), NONE, NONE, 2},
    {"wide state budget survives a restart", L("--state", "{T}/wide"), NONE,
     L(STEP("1"), "! restart", STEP("2")), 0},
    {"approvals line between calls", L("--approvals", "{T}/appr"), NONE,
     L(CALL("1", EXEC_D1), DENY("1", "deny noApproval"), "+ " D1, CALL("2", EXEC_D1), OK("2", "execute"),
       LOG("2", "{\"arguments\":{\"target\":\"prod/web\"},\"name\":\"execute\"}")),
     0},
    {"--state in a missing dir", L("--state", "{T}/no/st"), NONE,
     L(CALL("1", INSPECT_PROD),
       "< {\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32603,\"message\":\"permit-lang state error\"}}"),
     0},
    /* The demo rows of PLAN sec 8.3 (S9 U4). D9 is the static check row
     * test/check/delegate-widen.lang (Q-S9-7 (a)). */
    {"D1 inspect in scope", NONE, NONE, L(STEP("1")), 0},
    {"D2 propose", NONE, NONE,
     L(CALL("1", "{\"name\":\"propose\",\"arguments\":{\"target\":\"prod/web\"}}"), OK("1", "propose"),
       LOG("1", "{\"arguments\":{\"target\":\"prod/web\"},\"name\":\"propose\"}")),
     0},
    {"D3 execute the approved change", L("--approvals", "{T}/appr"), NONE,
     L("+ " D1, CALL("1", EXEC_D1), OK("1", "execute"), LOG("1", EXEC_D1_JCS)), 0},
    {"D4 changed target", L("--approvals", "{T}/appr"), NONE,
     L("+ " D1,
       CALL("1", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/api\"},\"_meta\":{\"permit/approval\":\"" D1 "\"}}"),
       DENY("1", "deny digestDiffers"), CALL("2", EXEC_D1), OK("2", "execute"), LOG("2", EXEC_D1_JCS)),
     0},
    {"D5 changed payload", L("--approvals", "{T}/appr"), NONE,
     L("+ " D1,
       CALL("1", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/web\",\"replicas\":3},\"_meta\":{\"permit/approval\":\"" D1 "\"}}"),
       DENY("1", "deny digestDiffers"), CALL("2", EXEC_D1), OK("2", "execute"), LOG("2", EXEC_D1_JCS)),
     0},
    {"D6 second execute, also after a restart", L("--approvals", "{T}/appr", "--state", "{T}/st"), NONE,
     L("+ " D1, CALL("1", EXEC_D1), OK("1", "execute"), CALL("2", EXEC_D1), DENY("2", "deny approvalUsed"),
       "! restart", CALL("3", EXEC_D1), DENY("3", "deny approvalUsed"), LOG("1", EXEC_D1_JCS)),
     0},
    {"D7 steps at 0", NONE, NONE,
     L(STEP("1"), STEP("2"), STEP("3"), STEP("4"), STEP("5"), STEP("6"), CALL("7", INSPECT_PROD),
       DENY("7", "deny boundSpent")),
     0},
    {"D8 outside the scope", NONE, NONE,
     L(CALL("1", "{\"name\":\"inspect\",\"arguments\":{\"target\":\"staging\"}}"), DENY("1", "deny scopeMiss")), 0},
    {"D10 same arguments, other pointer", L("--approvals", "{T}/appr"), NONE,
     L("+ " D_LEFT, CALL("1", SYNC("sync_right")), DENY("1", "deny digestDiffers"), CALL("2", SYNC("sync_left")),
       OK("2", "sync_left"), LOG("2", SYNC_JCS("sync_left"))),
     0},
    {"D11 same target and arguments, other tool", L("--approvals", "{T}/appr"), NONE,
     L("+ " D_RESTART, CALL("1", PROD("delete")), DENY("1", "deny digestDiffers"), CALL("2", PROD("restart")),
       OK("2", "restart"), LOG("2", PROD_JCS("restart"))),
     0},
    /* The report rows of S10 U2 (Q-S10-3 (a)): each `#` line is the next
     * line of --report {T}/rep. */
    {"R1 report permit and denies", L("--approvals", "{T}/appr", "--report", "{T}/rep"), NONE,
     L("+ " D1, CALL("1", EXEC_D1), OK("1", "execute"),
       REP(Q("execute"), Q(D1), Q("{T}/appr:1"), "9", "1", "permit", D1, "1", "null", "5", "prod/web", "execute"),
       CALL("2", EXEC_D1), DENY("2", "deny approvalUsed"),
       REP(Q("execute"), Q(D1), Q("{T}/appr:1"), "9", "1", "deny", D1, "2", Q("approvalUsed"), "4", "prod/web",
           "execute"),
       CALL("3", "{\"name\":\"inspect\",\"arguments\":{\"target\":\"staging\"}}"), DENY("3", "deny scopeMiss"),
       REP(Q("inspect"), "null", "null", "9", "1", "deny", D_STAGING, "3", Q("scopeMiss"), "3", "staging", "inspect"),
       "+ " D_LEFT, CALL("4", SYNC("sync_right")), DENY("4", "deny digestDiffers"),
       REP(Q("execute"), Q(D_LEFT), Q("{T}/appr:2"), "9", "1", "deny", D_RIGHT, "4", Q("digestDiffers"), "2", "dev",
           "sync_right"),
       CALL("5", "{\"name\":\"execute\",\"arguments\":{\"target\":\"prod/web\"}}"), DENY("5", "deny noApproval"),
       REP(Q("execute"), "null", "null", "9", "1", "deny", D1, "5", Q("noApproval"), "1", "prod/web", "execute"),
       LOG("1", EXEC_D1_JCS)),
     0},
    {"R2 report malformed", L("--report", "{T}/rep"), NONE,
     L("> not json",
       "< {\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"permit-lang deny malformed\"}}",
       MAL("10", "null", "5"), CALL("\"a\"", "{\"name\":\"inspect\",\"arguments\":{\"other\":\"prod\"}}"),
       DENY("\"a\"", "deny malformed"), MAL("10", Q("a"), "4")),
     0},
    {"R3 report budgetShort and boundSpent", L("--cost", "inspect=11", "--report", "{T}/rep"), NONE,
     L(CALL("1", INSPECT_PROD), DENY("1", "deny budgetShort"), INSPECT_REP("10", "11", "deny", "1", Q("budgetShort"), "5"),
       PROPOSE_STEP("2", "9", "4"), PROPOSE_STEP("3", "8", "3"), PROPOSE_STEP("4", "7", "2"),
       PROPOSE_STEP("5", "6", "1"), PROPOSE_STEP("6", "5", "0"), CALL("7", PROPOSE), DENY("7", "deny boundSpent"),
       REP(Q("propose"), "null", "null", "5", "1", "deny", D_PROPOSE, "7", Q("boundSpent"), "0", "prod/web",
           "propose")),
     0},
    {"R4 report restart appends", L("--state", "{T}/st", "--report", "{T}/rep"), NONE,
     L(STEP("1"), INSPECT_REP("9", "1", "permit", "1", "null", "5"), "! restart", STEP("2"),
       INSPECT_REP("8", "1", "permit", "2", "null", "4")),
     0},
    {"R5 report stays in the gate", L("--report", "{T}/rep"), L("--closed-file", "{T}/rep"),
     L(STEP("1"), INSPECT_REP("9", "1", "permit", "1", "null", "5")), 0},
    {"R6 report cannot be the state file", L("--state", "{T}/st", "--report", "{T}/st"), NONE, NONE, 2},
    {"R7 report cannot alias the state file", L("--state", "{T}/st", "--report", "{T}/./st"), NONE, NONE, 2},
    {"R8 report open failure", L("--report", "{T}/missing/rep"), NONE, NONE, 2},
};

static int fails = 0;

static void fail(const Row *row, const char *what, const char *got) {
  fprintf(stderr, "FAIL host %s: %s%s%s\n", row->name, what, got == NULL ? "" : ": ", got == NULL ? "" : got);
  fails = fails + 1;
}

/* Copies TEXT to OUT with each `{T}` replaced by DIR. */
static void subst(const char *text, const char *dir, char *out, size_t cap) {
  size_t n = 0;
  size_t dlen = strlen(dir);
  while (*text != '\0' && n + dlen + 1 < cap) {
    if (strncmp(text, "{T}", 3) == 0) {
      memcpy(out + n, dir, dlen);
      n = n + dlen;
      text = text + 3;
    } else {
      out[n] = *text;
      n = n + 1;
      text = text + 1;
    }
  }
  out[n] = '\0';
}

static int write_file(const char *path, const char *text, const char *mode) {
  FILE *f = fopen(path, mode);
  if (f == NULL) {
    return 0;
  }
  fputs(text, f);
  return fclose(f) == 0;
}

/* The answer lines of permitd: BUF holds LEN bytes not yet taken. */
typedef struct {
  int fd;
  char buf[TEXT_CAP * 4];
  size_t len;
} Reader;

/* Takes the next line (no '\n') into OUT. Returns 0 at the end of the
 * output or after WAIT_MS with no line. */
static int next_line(Reader *r, char *out, size_t cap) {
  for (;;) {
    char *nl = memchr(r->buf, '\n', r->len);
    struct pollfd p;
    ssize_t got;
    if (nl != NULL) {
      size_t n = (size_t)(nl - r->buf);
      size_t keep = n < cap ? n : cap - 1;
      memcpy(out, r->buf, keep);
      out[keep] = '\0';
      memmove(r->buf, nl + 1, r->len - n - 1);
      r->len = r->len - n - 1;
      return 1;
    }
    p.fd = r->fd;
    p.events = POLLIN;
    p.revents = 0;
    if (r->len == sizeof r->buf || poll(&p, 1, WAIT_MS) <= 0) {
      return 0;
    }
    got = read(r->fd, r->buf + r->len, sizeof r->buf - r->len);
    if (got <= 0) {
      return 0;
    }
    r->len = r->len + (size_t)got;
  }
}

/* Starts ARGV with its stdin and stdout on two new pipes and its stderr
 * in DIR/err. Returns 0 when it cannot. */
static int start_gate(char **argv, const char *dir, int *to, pid_t *pid, Reader *reader) {
  char path[1200];
  int to_gate[2];
  int from_gate[2];
  int ok;
  posix_spawn_file_actions_t fa;
  if (pipe(to_gate) != 0 || pipe(from_gate) != 0) {
    return 0;
  }
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_adddup2(&fa, to_gate[0], 0);
  posix_spawn_file_actions_adddup2(&fa, from_gate[1], 1);
  posix_spawn_file_actions_addclose(&fa, to_gate[1]);
  posix_spawn_file_actions_addclose(&fa, from_gate[0]);
  posix_spawn_file_actions_addopen(&fa, 2, strcat(strcpy(path, dir), "/err"), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  ok = posix_spawn(pid, argv[0], &fa, NULL, argv, environ) == 0;
  posix_spawn_file_actions_destroy(&fa);
  close(to_gate[0]);
  close(from_gate[1]);
  if (!ok) {
    close(to_gate[1]);
    close(from_gate[0]);
    return 0;
  }
  *to = to_gate[1];
  reader->fd = from_gate[0];
  reader->len = 0;
  return 1;
}

/* Closes the input of the gate, wants no extra answer, and returns its
 * exit code (-1 when it did not exit). */
static int stop_gate(const Row *row, int to, Reader *reader, pid_t pid) {
  char extra[TEXT_CAP];
  int status = 0;
  close(to);
  if (next_line(reader, extra, sizeof extra)) {
    fail(row, "an extra answer", extra);
  }
  close(reader->fd);
  if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
    return -1;
  }
  return WEXITSTATUS(status);
}

/* Appends TEXT and '\n' to the LEN bytes in BUF when they fit. */
static void add_line(char *buf, size_t cap, size_t *len, const char *text) {
  size_t n = strlen(text);
  if (*len + n + 1 < cap) {
    memcpy(buf + *len, text, n);
    buf[*len + n] = '\n';
    *len = *len + n + 1;
    buf[*len] = '\0';
  }
}

/* Fails ROW with WHAT and the bytes of DIR NAME (none when it is absent)
 * when they are not WANT. */
static void check_file(const Row *row, const char *dir, const char *name, const char *want, const char *what) {
  char path[1200];
  char got[TEXT_CAP * 2];
  size_t n = 0;
  FILE *f = fopen(strcat(strcpy(path, dir), name), "r");
  if (f != NULL) {
    n = fread(got, 1, sizeof got - 1, f);
    fclose(f);
  }
  got[n] = '\0';
  if (strcmp(got, want) != 0) {
    fail(row, what, got);
  }
}

static void run_row(const Row *row, const char *const *tools, const char *base, unsigned index) {
  char dir[1024];
  char path[1200];
  char text[TEXT_CAP];
  char want_log[TEXT_CAP * 2];
  char want_rep[TEXT_CAP * 2];
  char args[ARG_MAX_COUNT][1200];
  char *argv[ARG_MAX_COUNT + 1];
  unsigned argc = 0;
  unsigned i;
  int to_gate;
  pid_t pid;
  static Reader reader;
  size_t want_len = 0;
  size_t rep_len = 0;
  snprintf(dir, sizeof dir, "%s/row%u", base, index);
  if (mkdir(dir, 0755) != 0 || !write_file(strcat(strcpy(path, dir), "/appr"), "", "w") ||
      !write_file(strcat(strcpy(path, dir), "/bad"), "garbage\n", "w") ||
      !write_file(strcat(strcpy(path, dir), "/wide"), "6\n18446744073709551615\n0\n", "w")) {
    fail(row, "cannot make the row directory", dir);
    return;
  }
  subst(tools[0], dir, args[argc], sizeof args[0]);
  argc = argc + 1;
  for (i = 0; row->flags[i] != NULL; i++) {
    subst(row->flags[i], dir, args[argc], sizeof args[0]);
    argc = argc + 1;
  }
  subst(tools[2], dir, args[argc++], sizeof args[0]);
  subst("--", dir, args[argc++], sizeof args[0]);
  subst(tools[1], dir, args[argc++], sizeof args[0]);
  subst("{T}/log", dir, args[argc++], sizeof args[0]);
  for (i = 0; row->child[i] != NULL; i++) {
    subst(row->child[i], dir, args[argc], sizeof args[0]);
    argc = argc + 1;
  }
  for (i = 0; i < argc; i++) {
    argv[i] = args[i];
  }
  argv[argc] = NULL;
  if (!start_gate(argv, dir, &to_gate, &pid, &reader)) {
    fail(row, "cannot spawn", argv[0]);
    return;
  }
  want_log[0] = '\0';
  want_rep[0] = '\0';
  for (i = 0; row->script[i] != NULL; i++) {
    const char *line = row->script[i];
    char got[TEXT_CAP];
    subst(line + 2, dir, text, sizeof text);
    if (line[0] == '>') {
      size_t n = strlen(text);
      text[n] = '\n';
      if (write(to_gate, text, n + 1) != (ssize_t)(n + 1)) {
        fail(row, "cannot send", text);
      }
    } else if (line[0] == '<') {
      if (!next_line(&reader, got, sizeof got)) {
        fail(row, "no answer, want", text);
      } else if (strcmp(got, text) != 0) {
        fail(row, "answer", got);
      }
    } else if (line[0] == '+') {
      strcat(text, "\n");
      if (!write_file(strcat(strcpy(path, dir), "/appr"), text, "a")) {
        fail(row, "cannot add an approval", text);
      }
    } else if (line[0] == '!') {
      if (stop_gate(row, to_gate, &reader, pid) != 0) {
        fail(row, "the exit code before the restart differs", NULL);
        return;
      }
      if (!start_gate(argv, dir, &to_gate, &pid, &reader)) {
        fail(row, "cannot restart", argv[0]);
        return;
      }
    } else if (line[0] == '#') {
      add_line(want_rep, sizeof want_rep, &rep_len, text);
    } else {
      add_line(want_log, sizeof want_log, &want_len, text);
    }
  }
  if (stop_gate(row, to_gate, &reader, pid) != row->rc) {
    fail(row, "the exit code differs", NULL);
  }
  check_file(row, dir, "/log", want_log, "the fakedeploy log differs");
  check_file(row, dir, "/rep", want_rep, "the report differs");
}

int main(int argc, char **argv) {
  unsigned i;
  const char *tools[3];
  if (argc != 5) {
    fputs("usage: hosttool PERMITD FAKEDEPLOY MODULE DIR\n", stderr);
    return 2;
  }
  signal(SIGPIPE, SIG_IGN);
  tools[0] = argv[1];
  tools[1] = argv[2];
  tools[2] = argv[3];
  for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
    run_row(&rows[i], tools, argv[4], i);
  }
  if (fails != 0) {
    return 1;
  }
  printf("host rows: %zu checked\n", sizeof rows / sizeof rows[0]);
  return 0;
}
