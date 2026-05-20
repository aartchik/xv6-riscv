#include "ext2.h"

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(void)
{
  fprintf(stderr, "usage: ext2cat image inode\n");
}

static int
write_all(int fd, const void *buf, size_t n)
{
  const unsigned char *p = buf;

  while (n > 0) {
    ssize_t w = write(fd, p, n);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (w == 0) {
      errno = EIO;
      return -1;
    }
    p += w;
    n -= (size_t)w;
  }

  return 0;
}

int
main(int argc, char **argv)
{
  struct ext2_fs fs;
  struct ext2_inode inode;
  char *end;
  unsigned long ino;
  uint64_t size, blocks;
  unsigned char *buf, *zeros;

  if (argc != 3) {
    usage();
    return 1;
  }

  errno = 0;
  ino = strtoul(argv[2], &end, 0);
  if (errno != 0 || *end != '\0' || ino == 0 || ino > UINT32_MAX) {
    usage();
    return 1;
  }

  if (ext2_open(&fs, argv[1]) < 0) {
    fprintf(stderr, "ext2cat: cannot open %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  if (ext2_read_inode(&fs, (uint32_t)ino, &inode) < 0) {
    fprintf(stderr, "ext2cat: cannot read inode %lu: %s\n", ino, strerror(errno));
    ext2_close(&fs);
    return 1;
  }

  size = ext2_inode_size(&inode);
  blocks = (size + fs.block_size - 1) / fs.block_size;
  buf = malloc(fs.block_size);
  zeros = calloc(1, fs.block_size);
  if (buf == NULL || zeros == NULL) {
    fprintf(stderr, "ext2cat: no memory\n");
    free(buf);
    free(zeros);
    ext2_close(&fs);
    return 1;
  }

  for (uint64_t logical = 0; logical < blocks; logical++) {
    uint32_t physical;
    size_t n = fs.block_size;

    if (logical + 1 == blocks && size % fs.block_size != 0)
      n = (size_t)(size % fs.block_size);

    if (ext2_resolve_block(&fs, &inode, logical, &physical) < 0) {
      fprintf(stderr, "ext2cat: cannot resolve block %" PRIu64 ": %s\n",
              logical, strerror(errno));
      free(buf);
      free(zeros);
      ext2_close(&fs);
      return 1;
    }

    if (physical == 0) {
      if (write_all(STDOUT_FILENO, zeros, n) < 0)
        goto write_fail;
    } else {
      if (ext2_read_block(&fs, physical, buf) < 0) {
        fprintf(stderr, "ext2cat: cannot read block %" PRIu32 ": %s\n",
                physical, strerror(errno));
        free(buf);
        free(zeros);
        ext2_close(&fs);
        return 1;
      }
      if (write_all(STDOUT_FILENO, buf, n) < 0)
        goto write_fail;
    }
  }

  free(buf);
  free(zeros);
  ext2_close(&fs);
  return 0;

write_fail:
  fprintf(stderr, "ext2cat: write failed: %s\n", strerror(errno));
  free(buf);
  free(zeros);
  ext2_close(&fs);
  return 1;
}
