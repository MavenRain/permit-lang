/* Checks that langc report observes a buffered write error. */
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>

extern char **environ;

/* The same valid report must pass with writable stdout and fail with
 * read-only stdout. Both descriptors are buffered streams in langc. */
static int run(char *compiler, char *program, int mode, int want) {
  char *args[] = {compiler, "report", program, NULL};
  posix_spawn_file_actions_t actions;
  pid_t pid, waited;
  int status, ok;
  if (posix_spawn_file_actions_init(&actions) != 0)
    return 0;
  ok = posix_spawn_file_actions_addopen(&actions, 1, "/dev/null", mode, 0) == 0 &&
       posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0) == 0 &&
       posix_spawn(&pid, compiler, &actions, NULL, args, environ) == 0;
  posix_spawn_file_actions_destroy(&actions);
  if (!ok)
    return 0;
  do {
    waited = waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);
  return waited == pid && WIFEXITED(status) && WEXITSTATUS(status) == want;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fputs("usage: reporttool LANGC PROG\n", stderr);
    return 2;
  }
  if (!run(argv[1], argv[2], O_WRONLY, 0) || !run(argv[1], argv[2], O_RDONLY, 1)) {
    fputs("FAIL report write error: wrong exit status\n", stderr);
    return 1;
  }
  puts("report write errors: 2 checked");
  return 0;
}
