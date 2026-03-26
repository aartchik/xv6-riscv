#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void
test_read_write(void)
{
  int fd;
  char c = 'x';
  char buf[4];

  printf("test: read/write on mutex\n");
  fd = mutex();
  if (fd < 0) {
    printf("FAIL: mutex create\n");
    return;
  }

  if (read(fd, buf, 1) >= 0)
    printf("FAIL: read on mutex should fail\n");
  else
    printf("OK: read on mutex failed\n");

  if (write(fd, &c, 1) >= 0)
    printf("FAIL: write on mutex should fail\n");
  else
    printf("OK: write on mutex failed\n");

  close(fd);
}

static void
test_close_locked_by_self(void)
{
  int fd;

  printf("test: close locked mutex by owner\n");
  fd = mutex();
  if (fd < 0) {
    printf("FAIL: mutex create\n");
    return;
  }

  if (mutex_lock(fd) < 0) {
    printf("FAIL: lock\n");
    close(fd);
    return;
  }

  if (close(fd) < 0)
    printf("FAIL: close locked-by-self\n");
  else
    printf("OK: close locked-by-self\n");
}

static void
test_unlock_by_other(void)
{
  int fd;
  int pid;

  printf("test: unlock by other process\n");
  fd = mutex();
  if (fd < 0) {
    printf("FAIL: mutex create\n");
    return;
  }

  if (mutex_lock(fd) < 0) {
    printf("FAIL: parent lock\n");
    close(fd);
    return;
  }

  pid = fork();
  if (pid < 0) {
    printf("FAIL: fork\n");
    close(fd);
    return;
  }

  if (pid == 0) {
    if (mutex_unlock(fd) < 0)
      printf("OK: child cannot unlock parent's mutex\n");
    else
      printf("FAIL: child unlocked чужой mutex\n");

    close(fd);
    exit(0);
  }

  wait(0);
  mutex_unlock(fd);
  close(fd);
}

static void
test_close_locked_by_other(void)
{
  int fd;
  int pid;

  printf("test: close locked mutex by non-owner\n");
  fd = mutex();
  if (fd < 0) {
    printf("FAIL: mutex create\n");
    return;
  }

  pid = fork();
  if (pid < 0) {
    printf("FAIL: fork\n");
    close(fd);
    return;
  }

  if (pid == 0) {
    if (mutex_lock(fd) < 0) {
      printf("FAIL: child lock\n");
      close(fd);
      exit(1);
    }
    pause(20);
    mutex_unlock(fd);
    close(fd);
    exit(0);
  }

  pause(5);

  if (close(fd) < 0)
    printf("FAIL: parent close its fd while child owns lock\n");
  else
    printf("OK: parent close does not unlock child's mutex\n");

  wait(0);
}

static void
test_exit_with_open_mutex(void)
{
  int fd;
  int pid;

  printf("test: exit with open mutex\n");
  fd = mutex();
  if (fd < 0) {
    printf("FAIL: mutex create\n");
    return;
  }

  pid = fork();
  if (pid < 0) {
    printf("FAIL: fork\n");
    close(fd);
    return;
  }

  if (pid == 0) {
    if (mutex_lock(fd) < 0) {
      printf("FAIL: child lock\n");
      exit(1);
    }
    printf("child exits with locked mutex\n");
    exit(0);
  }

  wait(0);

  if (mutex_lock(fd) < 0)
    printf("FAIL: parent could not lock after child exit\n");
  else {
    printf("OK: child exit released/closed correctly\n");
    mutex_unlock(fd);
  }

  close(fd);
}

int
main(void)
{
  test_read_write();
  test_close_locked_by_self();
  test_unlock_by_other();
  test_close_locked_by_other();
  test_exit_with_open_mutex();
  exit(0);
}