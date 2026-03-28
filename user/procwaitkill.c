#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
usage(void)
{
  fprintf(2, "usage: procwaitkill wait|kill\n");
  exit(1);
}
int
main(int argc, char* argv[])
{
  if (argc != 2)
    usage();

  int killed = 0;
  if (strcmp(argv[1], "wait") == 0) {
    killed = 0;
  } else if (strcmp(argv[1], "kill") == 0) {
    killed = 1;
  } else {
    usage();
  }

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "procwaitkill: fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    pause(200);
    exit(1);
  }

  printf("parent pid=%d, child pid=%d (kill child: kill %d)\n", getpid(), pid, pid);

  if (killed) {
    if (kill(pid) < 0) {
      fprintf(2, "procwaitkill: kill failed\n");
      exit(1);
    }
    printf("sent kill to child pid=%d\n", pid);
  }

  int status = 0;
  int wpid = wait(&status);
  if (wpid < 0) {
    fprintf(2, "procwaitkill: wait failed\n");
    exit(1);
  }

  printf("wait returned pid=%d, status=%d\n", wpid, status);
  exit(0);
}