#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"
#include "dmesg.h"

struct {
  struct spinlock lock;
  char buf[DMESG_BUF_SIZE];
  uint head;
  uint tail;
  uint size;
} dmesg_buf;

struct spinlock logctl_lock;
volatile int logctl_mask;

extern uint ticks;
extern struct spinlock tickslock;

static char digits[] = "0123456789abcdef";

static void
dmesg_putc(int c)
{
  dmesg_buf.buf[dmesg_buf.tail] = c;
  dmesg_buf.tail = (dmesg_buf.tail + 1) % DMESG_BUF_SIZE;
  if(dmesg_buf.size == DMESG_BUF_SIZE)
    dmesg_buf.head = (dmesg_buf.head + 1) % DMESG_BUF_SIZE;
  else
    dmesg_buf.size++;
}

static void
dmesg_printint(long long xx, int base, int sign)
{
  char buf[20];
  int i;
  unsigned long long x;

  if(sign && (sign = (xx < 0)))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    dmesg_putc(buf[i]);
}

static void
dmesg_printptr(uint64 x)
{
  int i;

  dmesg_putc('0');
  dmesg_putc('x');
  for(i = 0; i < sizeof(uint64) * 2; i++, x <<= 4)
    dmesg_putc(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

static void
dmesg_vprintf(const char *fmt, va_list ap)
{
  int i, cx, c0, c1, c2;
  char *s;

  for(i = 0; (cx = fmt[i] & 0xff) != 0; i++){
    if(cx != '%'){
      dmesg_putc(cx);
      continue;
    }
    i++;
    c0 = fmt[i+0] & 0xff;
    c1 = c2 = 0;
    if(c0)
      c1 = fmt[i+1] & 0xff;
    if(c1)
      c2 = fmt[i+2] & 0xff;
    if(c0 == 'd'){
      dmesg_printint(va_arg(ap, int), 10, 1);
    } else if(c0 == 'l' && c1 == 'd'){
      dmesg_printint(va_arg(ap, uint64), 10, 1);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
      dmesg_printint(va_arg(ap, uint64), 10, 1);
      i += 2;
    } else if(c0 == 'u'){
      dmesg_printint(va_arg(ap, uint32), 10, 0);
    } else if(c0 == 'l' && c1 == 'u'){
      dmesg_printint(va_arg(ap, uint64), 10, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
      dmesg_printint(va_arg(ap, uint64), 10, 0);
      i += 2;
    } else if(c0 == 'x'){
      dmesg_printint(va_arg(ap, uint32), 16, 0);
    } else if(c0 == 'l' && c1 == 'x'){
      dmesg_printint(va_arg(ap, uint64), 16, 0);
      i += 1;
    } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
      dmesg_printint(va_arg(ap, uint64), 16, 0);
      i += 2;
    } else if(c0 == 'p'){
      dmesg_printptr(va_arg(ap, uint64));
    } else if(c0 == 'c'){
      dmesg_putc(va_arg(ap, uint));
    } else if(c0 == 's'){
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        dmesg_putc(*s);
    } else if(c0 == '%'){
      dmesg_putc('%');
    } else if(c0 == 0){
      break;
    } else {
      dmesg_putc('%');
      dmesg_putc(c0);
    }
  }
}

void
dmesginit(void)
{
  initlock(&dmesg_buf.lock, "dmesg");
  initlock(&logctl_lock, "logctl");
  dmesg_buf.head = 0;
  dmesg_buf.tail = 0;
  dmesg_buf.size = 0;
  logctl_mask = 0;
}

void
pr_msg(const char *fmt, ...)
{
  va_list ap;
  uint now;

  acquire(&tickslock);
  now = ticks;
  release(&tickslock);

  acquire(&dmesg_buf.lock);
  dmesg_putc('[');
  dmesg_printint(now, 10, 0);
  dmesg_putc(']');
  dmesg_putc(' ');
  va_start(ap, fmt);
  dmesg_vprintf(fmt, ap);
  va_end(ap);
  dmesg_putc('\n');
  release(&dmesg_buf.lock);
}

int
logenabled(int mask)
{
  return (logctl_mask & mask) != 0;
}

int
logctl(int op, int mask)
{
  int ret;

  if((mask & ~LOG_CLASS_ALL) != 0)
    return -1;

  acquire(&logctl_lock);
  if(op == LOGCTL_GET){
    ret = logctl_mask;
  } else if(op == LOGCTL_SET){
    logctl_mask = mask;
    ret = logctl_mask;
  } else if(op == LOGCTL_ON){
    logctl_mask |= mask;
    ret = logctl_mask;
  } else if(op == LOGCTL_OFF){
    logctl_mask &= ~mask;
    ret = logctl_mask;
  } else {
    ret = -1;
  }
  release(&logctl_lock);

  return ret;
}

int
dmesg(uint64 addr, int max)
{
  struct proc *p;
  uint start, count, i, copied;
  char zero;

  if(max <= 0)
    return -1;

  p = myproc();
  acquire(&dmesg_buf.lock);
  start = dmesg_buf.head;
  count = dmesg_buf.size;
  if(count == DMESG_BUF_SIZE){
    while(count > 0 && dmesg_buf.buf[start] != '\n'){
      start = (start + 1) % DMESG_BUF_SIZE;
      count--;
    }
    if(count > 0 && dmesg_buf.buf[start] == '\n'){
      start = (start + 1) % DMESG_BUF_SIZE;
      count--;
    }
  }
  if(count >= (uint)max)
    count = max - 1;
  for(i = 0, copied = 0; i < count; i++){
    if(copyout(p->pagetable, addr + copied, &dmesg_buf.buf[(start + i) % DMESG_BUF_SIZE], 1) < 0){
      release(&dmesg_buf.lock);
      return -1;
    }
    copied++;
  }
  zero = 0;
  if(copyout(p->pagetable, addr + copied, &zero, 1) < 0){
    release(&dmesg_buf.lock);
    return -1;
  }
  release(&dmesg_buf.lock);
  return copied;
}
