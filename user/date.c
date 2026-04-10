#include "kernel/types.h"
#include "user/user.h"

static int
is_leap(int y)
{
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static void
print2(int x)
{
  if (x < 10)
    printf("0");
  printf("%d", x);
}

int
main(void)
{
  uint64 ns = rtctime();     
  uint64 s = ns / 1000000000; 

  int sec = s % 60;
  s /= 60;
  int min = s % 60;
  s /= 60;
  int hour = s % 24;
  s /= 24;

  int year = 1970;

  while (1) {
    int days = is_leap(year) ? 366 : 365;
    if (s >= days) {
      s -= days;
      year++;
    }
    else {
      break;
    }
  }

  int mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

  if (is_leap(year))
    mdays[1] = 29;

  int month = 0;

  while (s >= mdays[month]) {
    s -= mdays[month];
    month++;
  }

  int day = s + 1;

  printf("%d-", year);
  print2(month + 1);
  printf("-");
  print2(day);
  printf(" ");
  print2(hour);
  printf(":");
  print2(min);
  printf(":");
  print2(sec);
  printf("\n");

  exit(0);
}