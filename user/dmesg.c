#include "kernel/types.h"
#include "user/user.h"
#include "kernel/dmesg.h"

static char buf[DMESG_BUF_SIZE + 1];

int
main(void)
{
  int n;

  n = dmesg(buf, sizeof(buf));
  if(n < 0){
    fprintf(2, "dmesg: failed\n");
    exit(1);
  }
  if(write(1, buf, n) != n){
    fprintf(2, "dmesg: write failed\n");
    exit(1);
  }
  exit(0);
}
