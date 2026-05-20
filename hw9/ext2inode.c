#define _POSIX_C_SOURCE 200809L

#include "ext2.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void
usage(void)
{
  fprintf(stderr, "usage: ext2inode image inode\n");
}

static void
format_time(uint32_t value, char *buf, size_t size)
{
  time_t t = value;
  struct tm tm;

  if (gmtime_r(&t, &tm) == NULL) {
    snprintf(buf, size, "%u", value);
    return;
  }
  strftime(buf, size, "%Y-%m-%d %H:%M:%S UTC", &tm);
}

static void
mode_string(uint16_t mode, char out[11])
{
  const char *rwx = "rwxrwxrwx";

  switch (mode & EXT2_S_IFMT) {
  case EXT2_S_IFREG:
    out[0] = '-';
    break;
  case EXT2_S_IFDIR:
    out[0] = 'd';
    break;
  case EXT2_S_IFLNK:
    out[0] = 'l';
    break;
  case EXT2_S_IFCHR:
    out[0] = 'c';
    break;
  case EXT2_S_IFBLK:
    out[0] = 'b';
    break;
  case EXT2_S_IFIFO:
    out[0] = 'p';
    break;
  case EXT2_S_IFSOCK:
    out[0] = 's';
    break;
  default:
    out[0] = '?';
    break;
  }

  for (int i = 0; i < 9; i++)
    out[i + 1] = (mode & (1 << (8 - i))) ? rwx[i] : '-';
  if (mode & 04000)
    out[3] = (mode & 0100) ? 's' : 'S';
  if (mode & 02000)
    out[6] = (mode & 0010) ? 's' : 'S';
  if (mode & 01000)
    out[9] = (mode & 0001) ? 't' : 'T';
  out[10] = '\0';
}

static const char *
level_name(unsigned level)
{
  switch (level) {
  case 0:
    return "direct";
  case 1:
    return "single";
  case 2:
    return "double";
  case 3:
    return "triple";
  default:
    return "unknown";
  }
}

static int
print_block(uint64_t logical, uint32_t physical, unsigned level, void *arg)
{
  (void)arg;
  printf("  logical=%" PRIu64 " physical=%" PRIu32 " via=%s\n",
         logical, physical, level_name(level));
  return 0;
}

int
main(int argc, char **argv)
{
  struct ext2_fs fs;
  struct ext2_inode inode;
  char *end;
  unsigned long ino;
  char mode[11], atime[32], ctime[32], mtime[32];

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
    fprintf(stderr, "ext2inode: cannot open %s: %s\n", argv[1], strerror(errno));
    return 1;
  }
  if (ext2_read_inode(&fs, (uint32_t)ino, &inode) < 0) {
    fprintf(stderr, "ext2inode: cannot read inode %lu: %s\n", ino, strerror(errno));
    ext2_close(&fs);
    return 1;
  }

  mode_string(inode.mode, mode);
  format_time(inode.atime, atime, sizeof(atime));
  format_time(inode.ctime, ctime, sizeof(ctime));
  format_time(inode.mtime, mtime, sizeof(mtime));

  printf("Image: %s\n", argv[1]);
  printf("Block size: %" PRIu32 "\n", fs.block_size);
  printf("Inode: %lu\n", ino);
  printf("Type: %s\n", ext2_inode_type(&inode));
  printf("Mode: %s (%06o)\n", mode, inode.mode);
  printf("UID: %u\n", inode.uid);
  printf("GID: %u\n", inode.gid);
  printf("Size: %" PRIu64 "\n", ext2_inode_size(&inode));
  printf("Blocks (512-byte units): %" PRIu32 "\n", inode.blocks);
  printf("Links: %u\n", inode.links_count);
  printf("Access time: %s (%" PRIu32 ")\n", atime, inode.atime);
  printf("Change time: %s (%" PRIu32 ")\n", ctime, inode.ctime);
  printf("Modify time: %s (%" PRIu32 ")\n", mtime, inode.mtime);

  printf("Block pointers:\n");
  for (int i = 0; i < EXT2_NDIR_BLOCKS; i++)
    printf("  direct[%d]: %" PRIu32 "\n", i, inode.block[i]);
  printf("  single_indirect: %" PRIu32 "\n", inode.block[12]);
  printf("  double_indirect: %" PRIu32 "\n", inode.block[13]);
  printf("  triple_indirect: %" PRIu32 "\n", inode.block[14]);

  printf("Allocated data blocks:\n");
  if (ext2_visit_blocks(&fs, &inode, print_block, NULL) < 0) {
    fprintf(stderr, "ext2inode: cannot read indirect blocks: %s\n", strerror(errno));
    ext2_close(&fs);
    return 1;
  }

  ext2_close(&fs);
  return 0;
}
