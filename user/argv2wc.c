#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
fatal(const char* msg)
{
  fprintf(2, "argv2wc: %s\n", msg);
  exit(1);
}

static void
must_close(int fd, const char* tag)
{
  if (close(fd) < 0) {
    fprintf(2, "argv2wc: close failed (%s)\n", tag);
    exit(1);
  }
}

static int
write_exact(int fd, const char* buf, int n)
{
  int off = 0;
  while (off < n) {
    int w = write(fd, buf + off, n - off);
    if (w <= 0)
      return -1;
    off += w;
  }
  return 0;
}

static int
flush_buf(int fd, char* buf, int* used)
{
  if (*used == 0)
    return 0;
  if (write_exact(fd, buf, *used) < 0)
    return -1;
  *used = 0;
  return 0;
}

int
main(int argc, char** argv)
{
  int p[2];
  if (pipe(p) < 0)
    fatal("pipe failed");

  int pid = fork();
  if (pid < 0) {
    close(p[0]);
    close(p[1]);
    fatal("fork failed");
  }

  if (pid == 0) {
    must_close(p[1], "child: close write-end");

    if (close(0) < 0)
      fatal("child: close(0) failed");

    int d = dup(p[0]);
    if (d < 0)
      fatal("child: dup failed");
    if (d != 0) {
      fprintf(2, "argv2wc: child: dup returned fd %d, expected 0\n", d);
      exit(1);
    }

    must_close(p[0], "child: close read-end");

    char* wcargv[] = { "/wc", 0 };
    exec("/wc", wcargv);

    fatal("child: exec /wc failed");
  }

  if (close(p[0]) < 0) {
    wait(0);
    fatal("parent: close read-end failed");
  }

  char out[256];
  int used = 0;

  for (int i = 1; i < argc; i++) {
    const char* s = argv[i];
    int n = strlen(s);

    int pos = 0;
    while (pos < n) {
      int space = (int)sizeof(out) - used;
      if (space == 0) {
        if (flush_buf(p[1], out, &used) < 0) {
          close(p[1]);
          wait(0);
          fatal("parent: write to pipe failed");
        }
        space = (int)sizeof(out);
      }

      int take = n - pos;
      if (take > space)
        take = space;

      memmove(out + used, s + pos, take);
      used += take;
      pos += take;
    }

    if (used == (int)sizeof(out)) {
      if (flush_buf(p[1], out, &used) < 0) {
        close(p[1]);
        wait(0);
        fatal("parent: write to pipe failed");
      }
    }
    out[used++] = '\n';
  }

  if (flush_buf(p[1], out, &used) < 0) {
    close(p[1]);
    wait(0);
    fatal("parent: write to pipe failed");
  }

  if (close(p[1]) < 0) {
    wait(0);
    fatal("parent: close write-end failed");
  }

  int st = 0;
  if (wait(&st) < 0)
    fatal("parent: wait failed");

  exit(0);
}