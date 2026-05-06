#include "kernel/types.h"
#include "user/user.h"
#include "kernel/dmesg.h"

static int
parse_class(char *name)
{
  if(strcmp(name, "syscall") == 0)
    return LOG_CLASS_SYSCALL;
  if(strcmp(name, "irq") == 0)
    return LOG_CLASS_IRQ;
  if(strcmp(name, "proc") == 0)
    return LOG_CLASS_PROC;
  if(strcmp(name, "exec") == 0)
    return LOG_CLASS_EXEC;
  if(strcmp(name, "all") == 0)
    return LOG_CLASS_ALL;
  return -1;
}

int
main(int argc, char **argv)
{
  int op, mask, bit;

  if(argc < 3){
    fprintf(2, "usage: logctl on|off class...\n");
    exit(1);
  }

  if(strcmp(argv[1], "on") == 0)
    op = LOGCTL_ON;
  else if(strcmp(argv[1], "off") == 0)
    op = LOGCTL_OFF;
  else {
    fprintf(2, "usage: logctl on|off class...\n");
    exit(1);
  }

  mask = 0;
  for(int i = 2; i < argc; i++){
    bit = parse_class(argv[i]);
    if(bit < 0){
      fprintf(2, "logctl: unknown class %s\n", argv[i]);
      exit(1);
    }
    mask |= bit;
  }

  if(logctl(op, mask) < 0){
    fprintf(2, "logctl: failed\n");
    exit(1);
  }

  exit(0);
}
