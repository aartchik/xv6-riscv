#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/memlayout.h"
#include "user/user.h"

int g = 123;
char garr[16] = { 0 };

int
main(int argc, char* argv[])
{
  int s = 10;
  char sarr[64] = { 0 };
  int* hp;
  char* big;
  int x = 0;

  sarr[10] = 11;
  garr[5] = 22;

  printf("=== start ===\n");
  vmprint();

  hp = malloc(sizeof(int));
  big = malloc(3 * 4096);
  if (hp == 0 || big == 0) {
    printf("alloc failed\n");
    exit(1);
  }

  *hp = 7;
  big[0] = 1;
  big[4096] = 2;
  big[8192] = 3;

  printf("=== after alloc ===\n");
  vmprint();

  printf("=== clear A/D ===\n");
  if (pgclear(&g, sizeof(g), PTE_A | PTE_D) < 0 ||
    pgclear(&garr[5], 1, PTE_A | PTE_D) < 0 ||
    pgclear(&s, sizeof(s), PTE_A | PTE_D) < 0 ||
    pgclear(&sarr[10], 1, PTE_A | PTE_D) < 0 ||
    pgclear(hp, sizeof(int), PTE_A | PTE_D) < 0 ||
    pgclear(big, 3 * 4096, PTE_A | PTE_D) < 0) {
    printf("pgclear failed\n");
    free(hp);
    free(big);
    exit(1);
  }
  vmprint();

  printf("=== read ===\n");
  x += g;
  x += garr[5];
  x += s;
  x += sarr[10];
  x += *hp;
  x += big[0];
  x += big[4096];
  x += big[8192];
  printf("dummy=%d\n", x);

  printf("g A = %d\n", pgcheck(&g, sizeof(g), PTE_A));
  printf("garr A = %d\n", pgcheck(&garr[5], 1, PTE_A));
  printf("s A = %d\n", pgcheck(&s, sizeof(s), PTE_A));
  printf("sarr A = %d\n", pgcheck(&sarr[10], 1, PTE_A));
  printf("hp A = %d\n", pgcheck(hp, sizeof(int), PTE_A));
  printf("big A = %d\n", pgcheck(big, 3 * 4096, PTE_A));
  vmprint();

  printf("=== write ===\n");
  g = 1;
  garr[5] = 2;
  s = 3;
  sarr[10] = 4;
  *hp = 5;
  big[0] = 6;
  big[4096] = 7;
  big[8192] = 8;

  printf("g D = %d\n", pgcheck(&g, sizeof(g), PTE_D));
  printf("garr D = %d\n", pgcheck(&garr[5], 1, PTE_D));
  printf("s D = %d\n", pgcheck(&s, sizeof(s), PTE_D));
  printf("sarr D = %d\n", pgcheck(&sarr[10], 1, PTE_D));
  printf("hp D = %d\n", pgcheck(hp, sizeof(int), PTE_D));
  printf("big D = %d\n", pgcheck(big, 3 * 4096, PTE_D));
  vmprint();

  free(hp);
  free(big);

  printf("=== after free ===\n");
  vmprint();

  printf("=== invalid args ===\n");
  printf("bad flags clear = %d\n", pgclear(&g, sizeof(g), PTE_W));
  printf("bad flags check = %d\n", pgcheck(&g, sizeof(g), PTE_W));

  printf("zero flags clear = %d\n", pgclear(&g, sizeof(g), 0));
  printf("zero flags check = %d\n", pgcheck(&g, sizeof(g), 0));

  printf("mixed flags clear = %d\n", pgclear(&g, sizeof(g), PTE_A | PTE_W));
  printf("mixed flags check = %d\n", pgcheck(&g, sizeof(g), PTE_A | PTE_W));

  printf("zero len clear = %d\n", pgclear(&g, 0, PTE_A));
  printf("zero len check = %d\n", pgcheck(&g, 0, PTE_A));

  printf("bad addr clear = %d\n", pgclear((void*)MAXVA, 4, PTE_A));
  printf("bad addr check = %d\n", pgcheck((void*)MAXVA, 4, PTE_A));

  exit(0);
}