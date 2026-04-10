// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char* argv[] = { "sh", 0 };

int
main(void)
{
  int pid, wpid, fd;

  if (open("console", O_RDWR) < 0) {
    mknod("console", CONSOLE, 0);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  fd = open("null", O_RDWR);
  if (fd < 0)
    mknod("null", PSEUDODEV, 0);
  else
    close(fd);

  fd = open("zero", O_RDWR);
  if (fd < 0)
    mknod("zero", PSEUDODEV, 1);
  else
    close(fd);

  fd = open("urandom", O_RDWR);
  if (fd < 0)
    mknod("urandom", PSEUDODEV, 2);
  else
    close(fd);

  fd = open("nullstat", O_RDWR);
  if (fd < 0)
    mknod("nullstat", PSEUDODEV, 3);
  else
    close(fd);

  for (;;) {
    printf("init: starting sh\n");
    pid = fork();
    if (pid < 0) {
      printf("init: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      exec("sh", argv);
      printf("init: exec sh failed\n");
      exit(1);
    }

    for (;;) {
      wpid = wait((int*)0);
      if (wpid == pid) {
        break;
      }
      else if (wpid < 0) {
        printf("init: wait returned an error\n");
        exit(1);
      }
      else {
      }
    }
  }
}