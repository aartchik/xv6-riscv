CC ?= cc
CFLAGS ?= -Wall -Wextra -g -std=c11
LDFLAGS ?=

PROGS = ext2inode ext2cat ext2ls
COMMON = ext2.o

.PHONY: all clean test valgrind

all: $(PROGS)

ext2inode: ext2inode.o $(COMMON)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

ext2cat: ext2cat.o $(COMMON)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

ext2ls: ext2ls.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

test: all
	./test.sh

valgrind: all
	VALGRIND=1 ./test.sh

clean:
	rm -f $(PROGS) *.o
