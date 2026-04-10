#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "memlayout.h"
#include "defs.h"

static struct spinlock rtc_lock;

static inline uint32
rtc_read_low(void)
{
  return *(volatile uint32*)RTC_LOW;
}

static inline uint32
rtc_read_high(void)
{
  return *(volatile uint32*)RTC_HIGH;
}

void
rtcinit(void)
{
  initlock(&rtc_lock, "rtc");
}

uint64
rtctime(void)
{
  uint32 lo, hi;
  uint64 t;

  acquire(&rtc_lock);

  lo = rtc_read_low();
  hi = rtc_read_high();

  release(&rtc_lock);

  t = ((uint64)hi << 32) | lo;
  return t;
}