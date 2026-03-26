#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "sleeplock.h"

int
mutexalloc(struct file** f)
{
  struct file* rf;
  struct sleeplock* lk;

  rf = filealloc();
  if (rf == 0)
    return -1;

  lk = (struct sleeplock*)kalloc();
  if (lk == 0) {
    rf->type = FD_NONE;
    rf->ref = 0;
    return -1;
  }

  memset(lk, 0, sizeof(*lk));
  initsleeplock(lk, "umutex");

  rf->type = FD_MUTEX;
  rf->readable = 0;
  rf->writable = 0;
  rf->pipe = 0;
  rf->ip = 0;
  rf->off = 0;
  rf->major = 0;
  rf->mutex = lk;

  *f = rf;

  printf("mutexalloc: file=%p mutex=%p\n", rf, lk);
  return 0;
}

void
mutexclose(struct sleeplock* lk)
{
  if (lk == 0)
    return;

  printf("mutexclose: mutex=%p\n", lk);
  kfree((void*)lk);
}