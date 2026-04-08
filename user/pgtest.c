#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "user/user.h"

int g = 123;
char garr[16];

int
main(int argc, char* argv[])
{
  int s = 10;
  char sarr[64];
  int* hp = malloc(sizeof(int));
  char* big = malloc(3 * 4096);

  if (hp == 0 || big == 0) {
    printf("alloc failed\n");
    exit(1);
  }

  *hp = 7;
  big[0] = 1;
  big[4096] = 2;
  big[8192] = 3;

  printf("=== start ===\n");
  vmprint();

  printf("=== clear A/D ===\n");
  pgclear(&g, sizeof(g), PTE_A | PTE_D);
  pgclear(&s, sizeof(s), PTE_A | PTE_D);
  pgclear(&sarr[10], 1, PTE_A | PTE_D);
  pgclear(hp, sizeof(int), PTE_A | PTE_D);
  pgclear(big, 3 * 4096, PTE_A | PTE_D);
  vmprint();

  printf("=== read ===\n");
  int x = 0;
  x += g;
  x += s;
  x += sarr[10];
  x += *hp;
  x += big[5000];
  printf("dummy=%d\n", x);

  printf("g A = %d\n", pgcheck(&g, sizeof(g), PTE_A));
  printf("s A = %d\n", pgcheck(&s, sizeof(s), PTE_A));
  printf("sarr[10] A = %d\n", pgcheck(&sarr[10], 1, PTE_A));
  printf("hp A = %d\n", pgcheck(hp, sizeof(int), PTE_A));
  printf("big A = %d\n", pgcheck(big, 3 * 4096, PTE_A));
  vmprint();

  printf("=== write ===\n");
  g = 1;
  s = 2;
  sarr[10] = 3;
  *hp = 4;
  big[5000] = 5;

  printf("g D = %d\n", pgcheck(&g, sizeof(g), PTE_D));
  printf("s D = %d\n", pgcheck(&s, sizeof(s), PTE_D));
  printf("sarr[10] D = %d\n", pgcheck(&sarr[10], 1, PTE_D));
  printf("hp D = %d\n", pgcheck(hp, sizeof(int), PTE_D));
  printf("big D = %d\n", pgcheck(big, 3 * 4096, PTE_D));
  vmprint();

  free(hp);
  free(big);

  printf("=== after free ===\n");
  vmprint();

  printf("bad flags clear = %d\n", pgclear(&g, sizeof(g), PTE_W));
  printf("bad flags check = %d\n", pgcheck(&g, sizeof(g), PTE_W));

  printf("bad addr clear = %d\n", pgclear((void*)MAXVA, 4, PTE_A));
  printf("bad addr check = %d\n", pgcheck((void*)MAXVA, 4, PTE_A));

  exit(0);
}