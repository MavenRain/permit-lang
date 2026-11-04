/* fakedeploy, the upstream MCP server of the permitd rows (S9 U3, the shape
 * of relay-lang 4781ca7 src/mcp.c).
 *
 *   fakedeploy LOG [--error TOOL] [--exit TOOL] [--closed-file FILE]
 *
 * Reads one JSON-RPC line at a time on stdin and answers on stdout:
 * initialize, ping, tools/list and tools/call for the 7 deploy tools (the
 * result text `ok TOOL`). It appends each raw tools/call line to LOG before
 * it answers. `--error TOOL` answers a call of TOOL with the JSON-RPC error
 * -32603; `--exit TOOL` exits 0 with no answer. A notification gets no
 * answer. Exit 0 at the end of the input, 2 for a usage error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "jcs.h"
#include "jsonin.h"

enum { LINE_CAP = 1 << 20, ID_CAP = 256 };

#define ARENA_LIMIT_BYTES ((size_t)1 << 26)

static const char *const tools[7] = {"inspect", "propose", "execute", "sync_left",
                                     "sync_right", "restart", "delete"};
/* The argument of each tool that names its target (test/host/deploy.lang). */
static const char *const tool_arg[7] = {"target", "target", "target", "left",
                                        "right", "target", "target"};

static char line_buf[LINE_CAP];

/* Reads one line into line_buf without its '\n' (and a final '\r').
 * Returns 0 at the end of the input, 1 for a line, -1 for a line over
 * LINE_CAP (the rest of it is dropped). */
static int read_line(size_t *len) {
  size_t n = 0;
  int over = 0;
  int c = getchar();
  if (c == EOF) {
    return 0;
  }
  while (c != EOF && c != '\n') {
    if (n < LINE_CAP) {
      line_buf[n] = (char)c;
      n = n + 1;
    } else {
      over = 1;
    }
    c = getchar();
  }
  if (n > 0 && line_buf[n - 1] == '\r') {
    n = n - 1;
  }
  *len = n;
  return over ? -1 : 1;
}

/* Writes the JSON form of the LEN bytes of TEXT (a string). */
static void put_string(const char *text, size_t len) {
  static unsigned char buf[2 * LINE_CAP];
  JsonValue v;
  JcsBuf out;
  memset(&v, 0, sizeof v);
  v.kind = JSON_STRING;
  v.text = text;
  v.len = len;
  out.bytes = buf;
  out.cap = sizeof buf;
  out.len = 0;
  if (jcs_write(&v, &out) == JCS_OK) {
    fwrite(buf, 1, out.len, stdout);
  } else {
    fputs("\"\"", stdout);
  }
}

/* Writes `{"jsonrpc":"2.0","id":ID` with ID the JCS form of a string or
 * integer id, else null. */
static void put_head(const JsonValue *id) {
  unsigned char buf[ID_CAP];
  JcsBuf out;
  out.bytes = buf;
  out.cap = sizeof buf;
  out.len = 0;
  fputs("{\"jsonrpc\":\"2.0\",\"id\":", stdout);
  if (id != NULL && (id->kind == JSON_STRING || id->kind == JSON_NUMBER) &&
      jcs_write(id, &out) == JCS_OK) {
    fwrite(buf, 1, out.len, stdout);
  } else {
    fputs("null", stdout);
  }
}

static void put_error(const JsonValue *id, int code, const char *message) {
  put_head(id);
  printf(",\"error\":{\"code\":%d,\"message\":", code);
  put_string(message, strlen(message));
  fputs("}}\n", stdout);
}

static void put_tools(const JsonValue *id) {
  int i;
  put_head(id);
  fputs(",\"result\":{\"tools\":[", stdout);
  for (i = 0; i < 7; i++) {
    printf("%s{\"name\":\"%s\",\"description\":\"fakedeploy %s\",\"inputSchema\":"
           "{\"type\":\"object\",\"properties\":{\"%s\":{\"type\":\"string\"}},"
           "\"required\":[\"%s\"]}}",
           i == 0 ? "" : ",", tools[i], tools[i], tool_arg[i], tool_arg[i]);
  }
  fputs("]}}\n", stdout);
}

/* The index of the tool NAME, or -1. */
static int tool_index(const char *name) {
  int i;
  for (i = 0; i < 7; i++) {
    if (strcmp(name, tools[i]) == 0) {
      return i;
    }
  }
  return -1;
}

static int append_log(const char *log_path, const char *line, size_t len) {
  FILE *f = fopen(log_path, "a");
  if (f == NULL) {
    return 0;
  }
  fwrite(line, 1, len, f);
  fputc('\n', f);
  return fclose(f) == 0;
}

/* Answers one tools/call. Returns 0 when the process must exit now. */
static int call(const char *log_path, const char *error_tool, const char *exit_tool,
                const JsonValue *id, const JsonValue *params, const char *line, size_t len) {
  const char *name = json_get_string(json_member(params, "name"));
  if (name == NULL || tool_index(name) < 0) {
    put_error(id, -32602, "Invalid params");
    return 1;
  }
  if (!append_log(log_path, line, len)) {
    put_error(id, -32603, "fakedeploy log error");
    return 1;
  }
  if (exit_tool != NULL && strcmp(name, exit_tool) == 0) {
    return 0;
  }
  if (error_tool != NULL && strcmp(name, error_tool) == 0) {
    put_error(id, -32603, "fakedeploy error");
    return 1;
  }
  put_head(id);
  fputs(",\"result\":{\"content\":[{\"type\":\"text\",\"text\":", stdout);
  {
    char text[64];
    int n = snprintf(text, sizeof text, "ok %s", name);
    put_string(text, (size_t)n);
  }
  fputs("}],\"isError\":false}}\n", stdout);
  return 1;
}

/* Answers one line. Returns 0 when the process must exit now. */
static int message(const char *log_path, const char *error_tool, const char *exit_tool,
                   const char *line, size_t len) {
  Arena arena;
  Diag diag;
  JsonValue v;
  const JsonValue *id;
  const char *method;
  int keep = 1;
  arena_init(&arena, ARENA_LIMIT_BYTES);
  diag_init(&diag);
  if (!json_parse(&arena, line, len, &v, &diag) || v.kind != JSON_OBJECT) {
    put_error(NULL, -32700, "Parse error");
    arena_release(&arena);
    return 1;
  }
  id = json_member(&v, "id");
  method = json_get_string(json_member(&v, "method"));
  if (method == NULL) {
    if (id != NULL) {
      put_error(id, -32600, "Invalid Request");
    }
  } else if (id == NULL) {
    /* a notification */
  } else if (strcmp(method, "initialize") == 0) {
    put_head(id);
    fputs(",\"result\":{\"protocolVersion\":\"2025-11-25\",\"capabilities\":{\"tools\":{}},"
          "\"serverInfo\":{\"name\":\"fakedeploy\",\"version\":\"S9\"}}}\n",
          stdout);
  } else if (strcmp(method, "ping") == 0) {
    put_head(id);
    fputs(",\"result\":{}}\n", stdout);
  } else if (strcmp(method, "tools/list") == 0) {
    put_tools(id);
  } else if (strcmp(method, "tools/call") == 0) {
    keep = call(log_path, error_tool, exit_tool, id, json_member(&v, "params"), line, len);
  } else {
    put_error(id, -32601, "Method not found");
  }
  fflush(stdout);
  arena_release(&arena);
  return keep;
}

/* The report file must not be open in the upstream child. */
static int file_is_closed(const char *path) {
  struct stat want, got;
  long limit = sysconf(_SC_OPEN_MAX);
  int fd;
  if (limit < 0 || stat(path, &want) != 0)
    return 0;
  for (fd = 3; fd < limit; fd++)
    if (fstat(fd, &got) == 0 && want.st_dev == got.st_dev && want.st_ino == got.st_ino)
      return 0;
  return 1;
}

int main(int argc, char **argv) {
  const char *error_tool = NULL;
  const char *exit_tool = NULL;
  int i;
  size_t len = 0;
  int got;
  if (argc < 2) {
    fputs("usage: fakedeploy LOG [--error TOOL] [--exit TOOL] [--closed-file FILE]\n", stderr);
    return 2;
  }
  for (i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--error") == 0 && i + 1 < argc) {
      error_tool = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "--exit") == 0 && i + 1 < argc) {
      exit_tool = argv[i + 1];
      i++;
    } else if (strcmp(argv[i], "--closed-file") == 0 && i + 1 < argc) {
      if (!file_is_closed(argv[++i])) {
        fputs("fakedeploy: the gate file is open in the child\n", stderr);
        return 1;
      }
    } else {
      fputs("usage: fakedeploy LOG [--error TOOL] [--exit TOOL] [--closed-file FILE]\n", stderr);
      return 2;
    }
  }
  got = read_line(&len);
  while (got != 0) {
    if (got < 0) {
      put_error(NULL, -32700, "Parse error");
      fflush(stdout);
    } else if (!message(argv[1], error_tool, exit_tool, line_buf, len)) {
      return 0;
    }
    got = read_line(&len);
  }
  return 0;
}
