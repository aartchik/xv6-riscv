#include "kernel/types.h"
#include "user/user.h"

#define NSEC_PER_SEC 1000000000L
#define SEC_PER_DAY 86400L
#define DAYS_PER_400_YEARS 146097L

static int
is_leap(long y)
{
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int
days_in_year(long y)
{
  return is_leap(y) ? 366 : 365;
}

static int
days_in_month(long y, int m)
{
  static int mdays[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

  if (m == 2 && is_leap(y))
    return 29;
  return mdays[m - 1];
}

static void
print2(int x)
{
  if (x < 10)
    printf("0");
  printf("%d", x);
}

static void
print9(long x)
{
  long div = 100000000L;

  while (div > 0) {
    printf("%d", (int)(x / div));
    x %= div;
    div /= 10;
  }
}

static long
floor_div(long x, long y)
{
  long q = x / y;
  long r = x % y;

  if (r < 0)
    q--;
  return q;
}

static long
floor_mod(long x, long y)
{
  long r = x % y;

  if (r < 0)
    r += y;
  return r;
}

static void
civil_from_days(long z, long *year, int *month, int *day)
{
  long y, era;
  int m, mdays;

  y = 1970;
  era = floor_div(z, DAYS_PER_400_YEARS);
  y += era * 400;
  z -= era * DAYS_PER_400_YEARS;

  while (z >= days_in_year(y)) {
    z -= days_in_year(y);
    y++;
  }

  m = 1;
  while (z >= (mdays = days_in_month(y, m))) {
    z -= mdays;
    m++;
  }

  *year = y;
  *month = m;
  *day = z + 1;
}

int
main(void)
{
  long ns, sec, nsec, days, daysec, year;
  int month, day, hour, min;

  ns = (long)rtctime();
  sec = floor_div(ns, NSEC_PER_SEC);
  nsec = floor_mod(ns, NSEC_PER_SEC);

  days = floor_div(sec, SEC_PER_DAY);
  daysec = floor_mod(sec, SEC_PER_DAY);

  hour = daysec / 3600;
  daysec %= 3600;
  min = daysec / 60;
  sec = daysec % 60;

  civil_from_days(days, &year, &month, &day);

  printf("%ld-", year);
  print2(month);
  printf("-");
  print2(day);
  printf(" ");
  print2(hour);
  printf(":");
  print2(min);
  printf(":");
  print2(sec);
  printf(".");
  print9(nsec);
  printf("\n");

  exit(0);
}
