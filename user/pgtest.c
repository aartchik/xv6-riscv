#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "user/user.h"

#define BIGPAGES 3
#define BIGSIZE (BIGPAGES * PGSIZE)

int g = 123;

static void
show(const char *name, void *addr, uint64 len)
{
  printf("%s: A=%d D=%d\n",
         name,
         pgcheck(addr, len, PTE_A),
         pgcheck(addr, len, PTE_D));
}

static void
show_all(const char *where, int *sp, char *sarr, int *hp, char *big)
{
  printf("-- %s --\n", where);
  show("global int", &g, sizeof(g));
  show("stack int", sp, sizeof(*sp));
  show("stack array elem", &sarr[10], 1);
  show("heap int", hp, sizeof(*hp));
  show("heap big[0]", &big[0], 1);
  show("heap big[4096]", &big[4096], 1);
  show("heap big all", big, BIGSIZE);
}

static void
clear_all(int *sp, char *sarr, int *hp, char *big)
{
  pgclear(&g, sizeof(g), PTE_A | PTE_D);
  pgclear(sp, sizeof(*sp), PTE_A | PTE_D);
  pgclear(&sarr[10], 1, PTE_A | PTE_D);
  pgclear(hp, sizeof(*hp), PTE_A | PTE_D);
  pgclear(big, BIGSIZE, PTE_A | PTE_D);
}

int
main(int argc, char* argv[])
{
  int s = 10;
  char sarr[64];
  int* hp;
  char* big;
  int x;

  memset(sarr, 0, sizeof(sarr));

  printf("=== at start ===\n");
  vmprint();

  hp = malloc(sizeof(int));
  big = sbrk(BIGSIZE);
  if (hp == 0 || big == SBRK_ERROR) {
    printf("alloc failed\n");
    exit(1);
  }

  *hp = 7;
  big[0] = 1;
  big[4096] = 2;
  big[8192] = 3;

  printf("=== after allocation and init ===\n");
  show_all("before clear", &s, sarr, hp, big);
  vmprint();

  clear_all(&s, sarr, hp, big);
  show_all("after clear A/D", &s, sarr, hp, big);
  vmprint();

  printf("=== read data ===\n");
  x = 0;
  x += g;
  x += s;
  x += sarr[10];
  x += *hp;
  x += big[0];
  x += big[4096];
  printf("read sum=%d\n", x);
  show_all("after read", &s, sarr, hp, big);
  vmprint();

  printf("=== write data ===\n");
  g = 1;
  s = 2;
  sarr[10] = 3;
  *hp = 4;
  big[0] = 5;
  big[4096] = 6;
  show_all("after write", &s, sarr, hp, big);
  vmprint();

  printf("=== invalid arguments ===\n");
  printf("bad flags clear = %d\n", pgclear(&g, sizeof(g), PTE_W));
  printf("bad flags check = %d\n", pgcheck(&g, sizeof(g), PTE_W));
  printf("bad addr clear = %d\n", pgclear((void*)MAXVA, 4, PTE_A));
  printf("bad addr check = %d\n", pgcheck((void*)MAXVA, 4, PTE_A));

  if (sbrk(-BIGSIZE) == SBRK_ERROR) {
    printf("sbrk free failed\n");
    exit(1);
  }

  printf("=== after releasing big heap block ===\n");
  printf("released big check = %d\n", pgcheck(big, BIGSIZE, PTE_A));
  vmprint();

  big = sbrklazy(2 * PGSIZE);
  if (big == SBRK_ERROR) {
    printf("lazy alloc failed\n");
    exit(1);
  }
  big[0] = 9;

  printf("=== lazy internal unmapped page ===\n");
  printf("lazy first page check = %d\n", pgcheck(big, PGSIZE, PTE_A));
  printf("lazy two-page check = %d\n", pgcheck(big, 2 * PGSIZE, PTE_A));
  printf("lazy two-page clear = %d\n", pgclear(big, 2 * PGSIZE, PTE_A | PTE_D));
  if (sbrklazy(-2 * PGSIZE) == SBRK_ERROR) {
    printf("lazy free failed\n");
    exit(1);
  }

  free(hp);
  exit(0);
}
