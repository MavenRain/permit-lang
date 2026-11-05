/* The langc command line. SHARED by the tcc kits.
   Exit 0: ok. Exit 1: the program is refused. Exit 2: usage or IO. */
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "cedar.h"
#include "front/check.h"
#include "front/codes.h"
#include "front/fmt.h"
#include "front/front.h"
#include "front/lexer.h"
#include "front/lower.h"
#include "front/parser.h"
#include "front/plantab.h"
#include "jsonout.h"
#include "mcp.h"
#include "report.h"
#include "target.h"

#define ARENA_LIMIT_BYTES ((size_t)1 << 30)

typedef enum {
  CMD_CHECK,
  CMD_EVAL,
  CMD_IR,
  CMD_ABI,
  CMD_BUILD,
  CMD_REPORT,
  CMD_CEDAR,
  CMD_FMT,
  CMD_EXPLAIN,
  CMD_BASE,
  CMD_LAW,
  CMD_MCP,
} Command;

typedef struct {
  Command command;
  const char *prog_path;
  const char *entry;
  char *const *args; /* argv, or the arguments of the MCP eval tool */
  int arg_count;
  const char *out_path;
  int runtime;
  int json; /* abi: the JSON form; check: --json (S13 A2) */
  int fmt_check; /* fmt --check (S12 A1): compare PROG with its canonical text */
  CheckMode mode; /* orbit-lang S3: --kit selects the kit mode */
  const char *code; /* explain CODE (S14 Q-S14-2 (a)) */
  const char *law; /* law LN (S14 Q-S14-4 (a)) */
  FILE *out; /* stdout, or the output of an MCP tool (S14 A4) */
  FILE *err; /* stderr, or the error output of an MCP tool */
} Options;

static const struct {
  const char *word;
  Command command;
} COMMANDS[] = {
  {"check", CMD_CHECK},
  {"eval", CMD_EVAL},
  {"ir", CMD_IR},
  {"abi", CMD_ABI},
  {"build", CMD_BUILD},
  {"report", CMD_REPORT},
  {"cedar", CMD_CEDAR},
  {"fmt", CMD_FMT},
  {"explain", CMD_EXPLAIN},
  {"base", CMD_BASE},
  {"law", CMD_LAW},
  {"mcp", CMD_MCP},
};

static int usage(FILE *err) {
  fputs("usage: langc [--kit] check [--json] PROG\n"
        "       langc [--kit] eval PROG NAME [ARGS...]\n"
        "       langc [--kit] ir PROG\n"
        "       langc [--kit] abi PROG [--text]\n"
        "       langc [--kit] build PROG -o OUT [--runtime]\n"
        "       langc [--kit] report PROG\n"
        "       langc [--kit] cedar PROG\n"
        "       langc [--kit] fmt PROG [--check]\n"
        "       langc explain CODE\n"
        "       langc base\n"
        "       langc law LN\n"
        "       langc [--kit] mcp\n"
        "--kit: the kit mode, the trusted test scope of the kit gate\n", err);
  return 2;
}

static int find_command(const char *word, Command *out) {
  for (size_t i = 0; i < sizeof COMMANDS / sizeof COMMANDS[0]; i++) {
    if (strcmp(COMMANDS[i].word, word) == 0) {
      *out = COMMANDS[i].command;
      return 1;
    }
  }
  return 0;
}

static int parse_build_options(int argc, char **argv, Options *opt, Diag *diag) {
  for (int i = 3; i < argc; i++) {
    int is_out = strcmp(argv[i], "-o") == 0 && i + 1 < argc;
    int is_runtime = strcmp(argv[i], "--runtime") == 0;
    if (!is_out && !is_runtime) return diag_fail(diag, "USAGE", NULL, "unknown build argument '%s'", argv[i]);
    opt->out_path = is_out ? argv[i + 1] : opt->out_path;
    opt->runtime |= is_runtime;
    i += is_out;
  }
  return opt->out_path != NULL ? 1 : diag_fail(diag, "USAGE", NULL, "build needs -o OUT");
}

static int parse_options(int argc, char **argv, Options *opt, Diag *diag) {
  memset(opt, 0, sizeof *opt);
  /* base and mcp take no program path (S14 A4). */
  if (argc < 2 || !find_command(argv[1], &opt->command) ||
      (argc < 3 && opt->command != CMD_BASE && opt->command != CMD_MCP)) {
    return diag_fail(diag, "USAGE", NULL, "expected a command and a program path");
  }
  opt->prog_path = argv[2];
  switch (opt->command) {
    case CMD_CHECK:
      /* check --json (S13 A2, Q-S13-1 (a)): --json comes right after check,
         so `check PROG --json` is a USAGE failure. */
      opt->json = strcmp(argv[2], "--json") == 0;
      if (opt->json && argc < 4) return diag_fail(diag, "USAGE", NULL, "check --json needs a program path");
      opt->prog_path = argv[2 + opt->json];
      return argc == 3 + opt->json ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_IR:
    case CMD_REPORT:
    case CMD_CEDAR:
      return argc == 3 ? 1 : diag_fail(diag, "USAGE", NULL, "too many arguments");
    case CMD_ABI:
      /* The JSON ABI is the default; --text gives the C-K4-16 lines (C-c7-1). */
      opt->json = argc == 3;
      return argc == 3 || (argc == 4 && strcmp(argv[3], "--text") == 0) ? 1
        : diag_fail(diag, "USAGE", NULL, "abi takes only --text after the program");
    case CMD_EVAL:
      if (argc < 4) return diag_fail(diag, "USAGE", NULL, "eval needs a definition name");
      opt->entry = argv[3];
      opt->args = argv + 4;
      opt->arg_count = argc - 4;
      return 1;
    case CMD_BUILD:
      return parse_build_options(argc, argv, opt, diag);
    case CMD_FMT:
      opt->fmt_check = argc == 4;
      return argc == 3 || (argc == 4 && strcmp(argv[3], "--check") == 0) ? 1
        : diag_fail(diag, "USAGE", NULL, "fmt takes only --check after the program");
    case CMD_EXPLAIN:
      opt->code = argv[2];
      return argc == 3 ? 1 : diag_fail(diag, "USAGE", NULL, "explain takes only a code");
    case CMD_LAW:
      opt->law = argv[2];
      return argc == 3 ? 1 : diag_fail(diag, "USAGE", NULL, "law takes only a law id");
    case CMD_BASE:
      return argc == 2 ? 1 : diag_fail(diag, "USAGE", NULL, "base takes no arguments");
    case CMD_MCP:
      return argc == 2 ? 1 : diag_fail(diag, "USAGE", NULL, "mcp takes no arguments");
  }
  return 0;
}

static int read_source(Arena *arena, const char *path, char **out, size_t *len, Diag *diag) {
  FILE *file = fopen(path, "rb");
  if (file == NULL) return diag_fail(diag, "IO", NULL, "cannot open %s", path);
  char *buf = arena_alloc(arena, SOURCE_MAX_BYTES + 1);
  size_t count = buf == NULL ? 0 : fread(buf, 1, SOURCE_MAX_BYTES + 1, file);
  int read_error = ferror(file);
  fclose(file);
  if (buf == NULL) return diag_fail(diag, "OOM", NULL, "no memory for %s", path);
  if (read_error) return diag_fail(diag, "IO", NULL, "cannot read %s", path);
  if (count > SOURCE_MAX_BYTES) return diag_fail(diag, "IO_SIZE", NULL, "%s is larger than %zu bytes", path, SOURCE_MAX_BYTES);
  *out = buf;
  *len = count;
  return 1;
}

/* Lowers the entries, then runs the ir, abi or build command. */
static int run_target(const Options *opt, Machine *machine, Diag *diag) {
  IrProgram prog;
  FILE *out;
  size_t i;
  int ok = 1;
  if (!lower_program(machine, &prog)) return 1;
  switch (opt->command) {
    case CMD_IR:
      ir_print(&prog, opt->out);
      return 0;
    case CMD_ABI:
      for (i = 0; i < prog.func_count && ok; i++) ok = target_abi_line(&prog.funcs[i], opt->out, opt->err);
      return ok ? 0 : 1;
    case CMD_REPORT:
      return report_program(machine, &prog, opt->out) ? 0 : 1;
    case CMD_CEDAR:
      return cedar_program(machine, &prog, opt->out) ? 0 : 1;
    case CMD_CHECK:
    case CMD_EVAL:
    case CMD_BUILD:
    case CMD_FMT:
    case CMD_EXPLAIN:
    case CMD_BASE:
    case CMD_LAW:
    case CMD_MCP:
      break;
  }
  out = fopen(opt->out_path, "wb");
  if (out == NULL) return diag_fail(diag, "IO", NULL, "cannot write '%s'", opt->out_path) + 1;
  ok = target_write(&prog, opt->runtime ? TARGET_PART_RUNTIME : TARGET_PART_MAIN, out, opt->err);
  return fclose(out) == 0 && ok ? 0 : 1;
}

/* Adds the span SPAN of the file PATH, or null when SPAN has no source
   position (line 0). */
static int out_span(JsonOut *out, const char *path, Span span) {
  char nums[128];
  snprintf(nums, sizeof nums, ",\"line\":%d,\"col\":%d,\"end_line\":%d,\"end_col\":%d}", span.line, span.col,
           span.end_line, span.end_col);
  return span.line == 0 ? out_text(out, "null")
                        : out_text(out, "{\"file\":") && out_string(out, path) && out_text(out, nums);
}

/* Adds the fix of DIAG: null, or {"span":..,"text":..} (replace the span
   by the text). */
static int out_fix(JsonOut *out, const char *path, const Diag *diag) {
  return !diag->fix_set ? out_text(out, "null")
                        : out_text(out, "{\"span\":") && out_span(out, path, diag->fix_span) &&
                              out_text(out, ",\"text\":") && out_string(out, diag->fix) && out_text(out, "}");
}

/* Adds the text of the check --json line (Q-S13-2, -3 (a)): 0 or 1
   diagnostic (the first failure), then the progress counts. The message is
   the text after `CODE: ` that diag_print writes. */
static int out_check(JsonOut *out, const char *path, const Diag *diag, const CheckProgress *progress) {
  char message[sizeof diag->def + sizeof diag->msg + 2];
  char counts[160];
  snprintf(message, sizeof message, "%s: %s", diag->def, diag->msg);
  snprintf(counts, sizeof counts, "],\"progress\":{\"parsed\":%d,\"typed\":%d,\"laws_discharged\":%d,\"laws_total\":%d}}",
           progress->parsed, progress->typed, progress->laws_discharged, progress->laws_total);
  return out_text(out, "{\"diagnostics\":[") &&
         (!diag->set ||
          (out_text(out, "{\"code\":") && out_string(out, diag->code) && out_text(out, ",\"span\":") &&
           out_span(out, path, diag->span) && out_text(out, ",\"message\":") && out_string(out, message) &&
           out_text(out, ",\"fix\":") && out_fix(out, path, diag) && out_text(out, "}"))) &&
         out_text(out, counts);
}

/* langc [--kit] check --json PROG (S13 A2, ported from accrue-lang
   02964ec): exit 0 or 1 with one JCS line on stdout, and main prints no
   diag. After a lex or parse error the progress stays all zero. OOM is
   exit 2 with no JSON; an IO or USAGE failure exits 2 before this
   function. When the line itself fails (OOM, IO), its diag replaces DIAG
   and the exit is 2. */
static int check_json(const Options *opt, Arena *arena, const char *text, size_t len, Diag *diag) {
  CheckProgress progress;
  DeclList decls;
  Machine machine;
  Diag line_diag;
  JsonOut out = {arena, &line_diag, "check", NULL, 0, 0};
  memset(&progress, 0, sizeof progress);
  diag_init(&line_diag);
  int ok = front_load(arena, opt->prog_path, text, len, &decls, diag) &&
           check_program_progress(arena, &decls, &machine, diag, opt->mode, &progress);
  if (!ok && diag->set && strcmp(diag->code, "OOM") == 0) return 2;
  if (!out_check(&out, opt->prog_path, diag, &progress) || !out_line(&out, opt->out)) {
    *diag = line_diag;
    return 2;
  }
  return ok ? 0 : 1;
}

static int run(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  if (opt->command == CMD_CHECK && opt->json) return check_json(opt, arena, text, len, diag);
  DeclList decls;
  if (!front_load(arena, opt->prog_path, text, len, &decls, diag)) return 1;
  Machine machine;
  if (!check_program(arena, &decls, &machine, diag, opt->mode)) return 1;
  switch (opt->command) {
    case CMD_CHECK:
      fputs("ok\n", opt->out);
      return 0;
    case CMD_EVAL:
      return eval_command(&machine, opt->entry, opt->args, opt->arg_count, opt->out, opt->err);
    case CMD_IR:
    case CMD_ABI:
    case CMD_BUILD:
    case CMD_REPORT:
    case CMD_CEDAR:
    case CMD_FMT: /* main runs run_fmt */
    case CMD_EXPLAIN: /* main runs run_explain */
    case CMD_BASE: /* main runs run_base */
    case CMD_LAW: /* main runs run_law */
    case CMD_MCP: /* main runs mcp_serve */
      break;
  }
  return run_target(opt, &machine, diag);
}

/* The line of the first byte where TEXT and the canonical text CANON
   differ (1 + the newlines in the common prefix), or 0 when they are equal. */
static size_t first_diff_line(const char *text, size_t len, const FmtText *canon) {
  size_t same = 0;
  size_t line = 1;
  for (; same < len && same < canon->len && text[same] == canon->data[same]; same++) line += text[same] == '\n' ? 1u : 0u;
  return same == len && same == canon->len ? 0 : line;
}

/* langc fmt (S12 A1, ported from accrue-lang S11): lexes and parses the
   program only (no domain, no check), then prints the canonical text. With
   --check it prints nothing when the file is canonical, else
   `PROG:LINE: not canonical` at the first line that differs (exit 1). */
static int run_fmt(const Options *opt, Arena *arena, Diag *diag) {
  char *text = NULL;
  size_t len = 0;
  if (!read_source(arena, opt->prog_path, &text, &len, diag)) return 2;
  TokenList toks;
  DeclList decls = {NULL, 0, 0};
  if (!lex_source(arena, opt->prog_path, text, len, &toks, diag)) return 1;
  if (!parse_source(arena, opt->prog_path, ORIGIN_PROGRAM, &toks, &decls, diag)) return 1;
  FmtText out;
  fmt_text_init(&out, arena);
  if (!fmt_program(&out, decls.items, decls.count, &toks)) {
    return diag_fail(diag, "OOM", NULL, "no memory for the canonical text of %s", opt->prog_path) + 2;
  }
  if (!fmt_validate(&out, opt->prog_path, diag)) return strcmp(diag->code, "OOM") == 0 ? 2 : 1;
  if (opt->fmt_check) {
    size_t line = first_diff_line(text, len, &out);
    if (line != 0) fprintf(opt->err, "%s:%zu: not canonical\n", opt->prog_path, line);
    return line != 0;
  }
  size_t written = out.len == 0 ? 0 : fwrite(out.data, 1, out.len, opt->out);
  if (written != out.len || fflush(opt->out) != 0) return diag_fail(diag, "IO", NULL, "cannot write the canonical text") + 2;
  return 0;
}

/* langc explain CODE (S14 A4, Q-S14-2 (a), the accrue-lang 02964ec form):
   one line on OUT, `CODE: MEANING (docs/host/README.md, SECTION)`, and
   exit 0. An unknown code is exit 1 with an EXPLAIN diag. It reads no
   program and uses no arena, so the MCP tool can call it with its own
   OUT. */
static int run_explain(const Options *opt, Diag *diag, FILE *out) {
  const CodeRow *row = code_find(opt->code);
  if (row == NULL) return diag_fail(diag, "EXPLAIN", NULL, "'%s' is not a code of docs/host/README.md", opt->code) + 1;
  fprintf(out, "%s: %s (docs/host/README.md, %s)\n", row->code, row->meaning, row->section);
  return fflush(out) == 0 ? 0 : diag_fail(diag, "IO", NULL, "cannot write the explain line") + 2;
}

/* langc base (S14 A4, Q-S14-3 (a)): one line on OUT for each row of
   docs/PLAN.md section 4, `NAME KIND LINE`, and exit 0. */
static int run_base(Diag *diag, FILE *out) {
  size_t i;
  for (i = 0; i < base_count(); i++) fprintf(out, "%s %s %s\n", base_at(i)->name, base_at(i)->kind, base_at(i)->line);
  return fflush(out) == 0 ? 0 : diag_fail(diag, "IO", NULL, "cannot write the base lines") + 2;
}

/* langc law LN (S14 A4, Q-S14-4 (a)): one line on OUT, `LN NAME:
   STATEMENT` for the law LN of docs/PLAN.md section 7, and exit 0. An
   unknown law is exit 1 with a LAW diag, as explain. */
static int run_law(const Options *opt, Diag *diag, FILE *out) {
  const LawRow *row = law_find(opt->law);
  if (row == NULL) return diag_fail(diag, "LAW", NULL, "'%s' is not a law of docs/PLAN.md section 7", opt->law) + 1;
  fprintf(out, "%s %s: %s\n", row->id, row->name, row->statement);
  return fflush(out) == 0 ? 0 : diag_fail(diag, "IO", NULL, "cannot write the law line") + 2;
}

/* The diag rule of main, on the err of OPT: fmt --check prints its own
   `PROG:LINE: not canonical` line, with no diag; check --json writes a
   refusal (exit 1) on its out, so err stays empty. Returns STATUS. */
static int finish_status(const Options *opt, Diag *diag, int status) {
  int in_json = opt->command == CMD_CHECK && opt->json && status == 1;
  if (status != 0 && !in_json && (opt->command != CMD_FMT || diag->set)) diag_print(diag, opt->err);
  return status;
}

/* The check mode of `langc [--kit] mcp`: main sets it before mcp_serve. */
static CheckMode mcp_mode = CHECK_GUEST;

/* TOOL on OPT, ARENA and DIAG, as the CLI runs the verb (S14 A4, the
   accrue-lang 02964ec form). mcp.c checks ARGS before the call, so each
   case trusts the fields of its tool. */
static int mcp_run(McpTool tool, const McpArgs *args, Options *opt, Arena *arena, Diag *diag) {
  switch (tool) {
    case MCP_TOOL_CHECK:
      opt->command = CMD_CHECK;
      opt->json = 1;
      return run(opt, arena, diag);
    case MCP_TOOL_FMT:
      opt->command = CMD_FMT;
      return run_fmt(opt, arena, diag);
    case MCP_TOOL_EVAL:
      opt->command = CMD_EVAL;
      opt->entry = args->name;
      opt->args = args->args;
      opt->arg_count = args->arg_count;
      return run(opt, arena, diag);
    case MCP_TOOL_BUILD:
      /* Q-S14-6 (a): main is `-o OUT`, runtime is `-o OUT --runtime`. */
      opt->command = CMD_BUILD;
      opt->runtime = args->target == MCP_TARGET_RUNTIME;
      opt->out_path = args->out_path;
      return run(opt, arena, diag);
    case MCP_TOOL_EXPLAIN:
      opt->command = CMD_EXPLAIN;
      opt->code = args->code;
      return run_explain(opt, diag, opt->out);
    case MCP_TOOL_LIST_BASE:
      opt->command = CMD_BASE;
      return run_base(diag, opt->out);
    case MCP_TOOL_SHOW_LAW:
      opt->command = CMD_LAW;
      opt->law = args->law;
      return run_law(opt, diag, opt->out);
  }
  return diag_fail(diag, "INTERNAL", NULL, "no handler for the MCP tool") + 1;
}

/* The tools of `langc mcp` (mcp.h): each call has fresh Options, a fresh
   Diag and a fresh Arena that is released after the call, and prints its
   output on OUT and its diag on ERR. */
static int mcp_tool(McpTool tool, const McpArgs *args, FILE *out, FILE *err) {
  Options opt;
  Diag diag;
  Arena arena;
  int status;
  memset(&opt, 0, sizeof opt);
  opt.mode = mcp_mode;
  opt.out = out;
  opt.err = err;
  opt.prog_path = args->path;
  diag_init(&diag);
  arena_init(&arena, ARENA_LIMIT_BYTES);
  status = mcp_run(tool, args, &opt, &arena, &diag);
  arena_release(&arena);
  return finish_status(&opt, &diag, status);
}

int main(int argc, char **argv) {
  Diag diag;
  diag_init(&diag);
  Options opt;
  int kit = argc > 1 && strcmp(argv[1], "--kit") == 0;
  if (!parse_options(argc - kit, argv + kit, &opt, &diag)) {
    diag_print(&diag, stderr);
    return usage(stderr);
  }
  opt.mode = kit ? CHECK_KIT : CHECK_GUEST;
  opt.out = stdout;
  opt.err = stderr;
  /* mcp serves the tools on stdin and stdout to the end of stdin; each
     call has its own Options, Diag and Arena (S14 A4). */
  if (opt.command == CMD_MCP) {
    mcp_mode = opt.mode;
    return mcp_serve(stdin, stdout, mcp_tool);
  }
  /* explain, base and law read no program and use no arena. */
  if (opt.command == CMD_EXPLAIN) return finish_status(&opt, &diag, run_explain(&opt, &diag, opt.out));
  if (opt.command == CMD_BASE) return finish_status(&opt, &diag, run_base(&diag, opt.out));
  if (opt.command == CMD_LAW) return finish_status(&opt, &diag, run_law(&opt, &diag, opt.out));
  Arena arena;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  int status = opt.command == CMD_FMT ? run_fmt(&opt, &arena, &diag) : run(&opt, &arena, &diag);
  arena_release(&arena);
  return finish_status(&opt, &diag, status);
}
