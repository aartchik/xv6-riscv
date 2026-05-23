#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
print_args_unsync(int argc, char** argv)
{
  int i, j;
  for (i = 1; i < argc; i++) {
    for (j = 0; argv[i][j]; j++) {
      printf("%d: arg %d, char '%c'\n", getpid(), i, argv[i][j]);
      pause(1);
    }
  }
}

static void
print_args_sync(int mfd, int argc, char** argv)
{
  int i, j;
  for (i = 1; i < argc; i++) {
    for (j = 0; argv[i][j]; j++) {
      if (mutex_lock(mfd) < 0) {
        printf("mutex_lock failed\n");
        exit(1);
      }

      printf("%d: arg %d, char '%c'\n", getpid(), i, argv[i][j]);

      if (mutex_unlock(mfd) < 0) {
        printf("mutex_unlock failed\n");
        exit(1);
      }

      pause(1);
    }
  }
}

int
main(int argc, char** argv)
{
  int pid;
  int mfd;

  if (argc < 2) {
    fprintf(2, "usage: mutex_demo args...\n");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }

  if (pid == 0) {
    print_args_unsync(argc, argv);
    exit(0);
  }
  else {
    print_args_unsync(argc, argv);
    wait(0);
  }

  mfd = mutex();
  if (mfd < 0) {
    printf("mutex create failed\n");
    exit(1);
  }

  pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    close(mfd);
    exit(1);
  }

  if (pid == 0) {
    print_args_sync(mfd, argc, argv);
    close(mfd);
    exit(0);
  }
  else {
    print_args_sync(mfd, argc, argv);
    wait(0);
    close(mfd);
  }

  exit(0);
}