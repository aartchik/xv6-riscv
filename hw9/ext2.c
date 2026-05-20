#define _POSIX_C_SOURCE 200809L

#include "ext2.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXT2_SUPER_OFFSET 1024
#define EXT2_SUPER_SIZE 1024
#define EXT2_SUPER_MAGIC 0xef53
#define EXT2_BG_DESC_SIZE 32
#define EXT2_EXTENTS_FL 0x00080000

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define EXT2_HOST_LITTLE_ENDIAN 1
#else
#define EXT2_HOST_LITTLE_ENDIAN 0
#endif

static uint16_t
get_le16(const unsigned char *p)
{
  (void)EXT2_HOST_LITTLE_ENDIAN;
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t
get_le32(const unsigned char *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int
read_full_at(int fd, void *buf, size_t n, off_t off)
{
  unsigned char *p = buf;

  while (n > 0) {
    ssize_t r = pread(fd, p, n, off);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0) {
      errno = EIO;
      return -1;
    }
    p += r;
    off += r;
    n -= (size_t)r;
  }

  return 0;
}

static uint64_t
block_offset(const struct ext2_fs *fs, uint32_t block)
{
  return (uint64_t)block * fs->block_size;
}

int
ext2_open(struct ext2_fs *fs, const char *path)
{
  unsigned char sb[EXT2_SUPER_SIZE];
  uint32_t log_block_size;

  memset(fs, 0, sizeof(*fs));
  fs->fd = open(path, O_RDONLY);
  if (fs->fd < 0)
    return -1;

  if (read_full_at(fs->fd, sb, sizeof(sb), EXT2_SUPER_OFFSET) < 0)
    goto fail;

  if (get_le16(sb + 56) != EXT2_SUPER_MAGIC) {
    errno = EINVAL;
    goto fail;
  }

  log_block_size = get_le32(sb + 24);
  if (log_block_size > 16) {
    errno = EINVAL;
    goto fail;
  }

  fs->path = strdup(path);
  if (fs->path == NULL)
    goto fail;

  fs->block_size = 1024U << log_block_size;
  fs->inodes_count = get_le32(sb + 0);
  fs->blocks_count = get_le32(sb + 4);
  fs->first_data_block = get_le32(sb + 20);
  fs->blocks_per_group = get_le32(sb + 32);
  fs->inodes_per_group = get_le32(sb + 40);
  fs->inode_size = get_le16(sb + 88);
  fs->feature_compat = get_le32(sb + 92);
  fs->feature_incompat = get_le32(sb + 96);
  fs->feature_ro_compat = get_le32(sb + 100);

  if (fs->inode_size == 0)
    fs->inode_size = 128;
  if (fs->inodes_per_group == 0 || fs->blocks_per_group == 0 ||
      fs->inode_size < 128) {
    errno = EINVAL;
    goto fail;
  }

  fs->groups_count = (fs->blocks_count - fs->first_data_block +
                      fs->blocks_per_group - 1) / fs->blocks_per_group;

  return 0;

fail:
  ext2_close(fs);
  return -1;
}

void
ext2_close(struct ext2_fs *fs)
{
  if (fs->fd >= 0)
    close(fs->fd);
  free(fs->path);
  memset(fs, 0, sizeof(*fs));
  fs->fd = -1;
}

int
ext2_read_inode(struct ext2_fs *fs, uint32_t ino, struct ext2_inode *inode)
{
  unsigned char gd[EXT2_BG_DESC_SIZE];
  unsigned char *raw;
  uint32_t group, index, inode_table;
  uint64_t gd_offset, inode_offset;

  if (ino == 0 || ino > fs->inodes_count) {
    errno = EINVAL;
    return -1;
  }
  group = (ino - 1) / fs->inodes_per_group;
  index = (ino - 1) % fs->inodes_per_group;
  if (group >= fs->groups_count) {
    errno = EINVAL;
    return -1;
  }

  gd_offset = fs->block_size == 1024 ? 2ULL * fs->block_size : fs->block_size;
  gd_offset += (uint64_t)group * EXT2_BG_DESC_SIZE;
  if (read_full_at(fs->fd, gd, sizeof(gd), (off_t)gd_offset) < 0)
    return -1;

  raw = malloc(fs->inode_size);
  if (raw == NULL)
    return -1;

  inode_table = get_le32(gd + 8);
  inode_offset = block_offset(fs, inode_table) + (uint64_t)index * fs->inode_size;
  if (read_full_at(fs->fd, raw, fs->inode_size, (off_t)inode_offset) < 0) {
    free(raw);
    return -1;
  }

  memset(inode, 0, sizeof(*inode));
  inode->mode = get_le16(raw + 0);
  inode->uid = get_le16(raw + 2);
  inode->size_low = get_le32(raw + 4);
  inode->atime = get_le32(raw + 8);
  inode->ctime = get_le32(raw + 12);
  inode->mtime = get_le32(raw + 16);
  inode->dtime = get_le32(raw + 20);
  inode->gid = get_le16(raw + 24);
  inode->links_count = get_le16(raw + 26);
  inode->blocks = get_le32(raw + 28);
  inode->flags = get_le32(raw + 32);
  for (int i = 0; i < EXT2_N_BLOCKS; i++)
    inode->block[i] = get_le32(raw + 40 + i * 4);
  inode->generation = get_le32(raw + 100);
  inode->file_acl = get_le32(raw + 104);
  inode->dir_acl = get_le32(raw + 108);
  inode->faddr = get_le32(raw + 112);
  free(raw);

  if ((inode->flags & EXT2_EXTENTS_FL) != 0) {
    errno = ENOTSUP;
    return -1;
  }

  return 0;
}

uint64_t
ext2_inode_size(const struct ext2_inode *inode)
{
  uint64_t size = inode->size_low;

  if ((inode->mode & EXT2_S_IFMT) == EXT2_S_IFREG)
    size |= (uint64_t)inode->dir_acl << 32;
  return size;
}

int
ext2_is_regular(const struct ext2_inode *inode)
{
  return (inode->mode & EXT2_S_IFMT) == EXT2_S_IFREG;
}

const char *
ext2_inode_type(const struct ext2_inode *inode)
{
  switch (inode->mode & EXT2_S_IFMT) {
  case EXT2_S_IFREG:
    return "regular";
  case EXT2_S_IFDIR:
    return "directory";
  case EXT2_S_IFLNK:
    return "symlink";
  case EXT2_S_IFCHR:
    return "char-device";
  case EXT2_S_IFBLK:
    return "block-device";
  case EXT2_S_IFIFO:
    return "fifo";
  case EXT2_S_IFSOCK:
    return "socket";
  default:
    return "unknown";
  }
}

int
ext2_read_block(struct ext2_fs *fs, uint32_t block, void *buf)
{
  if (block == 0 || block >= fs->blocks_count) {
    errno = EINVAL;
    return -1;
  }
  return read_full_at(fs->fd, buf, fs->block_size, (off_t)block_offset(fs, block));
}

static int
read_pointer(struct ext2_fs *fs, uint32_t block, uint32_t index, uint32_t *value)
{
  unsigned char raw[4];
  uint64_t offset;

  offset = block_offset(fs, block) + (uint64_t)index * sizeof(raw);
  if (read_full_at(fs->fd, raw, sizeof(raw), (off_t)offset) < 0)
    return -1;
  *value = get_le32(raw);
  return 0;
}

int
ext2_resolve_block(struct ext2_fs *fs, const struct ext2_inode *inode,
                   uint64_t logical, uint32_t *physical)
{
  uint64_t per_block = fs->block_size / 4;
  uint32_t p1, p2;

  if (logical < EXT2_NDIR_BLOCKS) {
    *physical = inode->block[logical];
    return 0;
  }
  logical -= EXT2_NDIR_BLOCKS;

  if (logical < per_block) {
    if (inode->block[12] == 0) {
      *physical = 0;
      return 0;
    }
    return read_pointer(fs, inode->block[12], (uint32_t)logical, physical);
  }
  logical -= per_block;

  if (logical < per_block * per_block) {
    if (inode->block[13] == 0) {
      *physical = 0;
      return 0;
    }
    if (read_pointer(fs, inode->block[13], (uint32_t)(logical / per_block), &p1) < 0)
      return -1;
    if (p1 == 0) {
      *physical = 0;
      return 0;
    }
    return read_pointer(fs, p1, (uint32_t)(logical % per_block), physical);
  }
  logical -= per_block * per_block;

  if (logical < per_block * per_block * per_block) {
    if (inode->block[14] == 0) {
      *physical = 0;
      return 0;
    }
    if (read_pointer(fs, inode->block[14], (uint32_t)(logical / (per_block * per_block)), &p1) < 0)
      return -1;
    if (p1 == 0) {
      *physical = 0;
      return 0;
    }
    logical %= per_block * per_block;
    if (read_pointer(fs, p1, (uint32_t)(logical / per_block), &p2) < 0)
      return -1;
    if (p2 == 0) {
      *physical = 0;
      return 0;
    }
    return read_pointer(fs, p2, (uint32_t)(logical % per_block), physical);
  }

  errno = EFBIG;
  return -1;
}

static int
visit_indirect(struct ext2_fs *fs, uint32_t block, unsigned level,
               uint64_t base, uint64_t span, ext2_block_visitor visitor,
               void *arg)
{
  unsigned char *buf;
  uint32_t ptr;
  uint64_t per_block = fs->block_size / 4;
  int ret = 0;

  if (block == 0)
    return 0;

  buf = malloc(fs->block_size);
  if (buf == NULL)
    return -1;
  if (ext2_read_block(fs, block, buf) < 0) {
    free(buf);
    return -1;
  }

  for (uint64_t i = 0; i < per_block; i++) {
    ptr = get_le32(buf + i * 4);
    if (ptr == 0)
      continue;
    if (level == 1)
      ret = visitor(base + i, ptr, level, arg);
    else
      ret = visit_indirect(fs, ptr, level - 1, base + i * span,
                           span / per_block, visitor, arg);
    if (ret != 0)
      break;
  }

  free(buf);
  return ret;
}

int
ext2_visit_blocks(struct ext2_fs *fs, const struct ext2_inode *inode,
                  ext2_block_visitor visitor, void *arg)
{
  uint64_t per_block = fs->block_size / 4;
  int ret;

  for (uint64_t i = 0; i < EXT2_NDIR_BLOCKS; i++) {
    if (inode->block[i] == 0)
      continue;
    ret = visitor(i, inode->block[i], 0, arg);
    if (ret != 0)
      return ret;
  }

  ret = visit_indirect(fs, inode->block[12], 1, EXT2_NDIR_BLOCKS, 1,
                       visitor, arg);
  if (ret != 0)
    return ret;

  ret = visit_indirect(fs, inode->block[13], 2,
                       EXT2_NDIR_BLOCKS + per_block, per_block, visitor, arg);
  if (ret != 0)
    return ret;

  return visit_indirect(fs, inode->block[14], 3,
                        EXT2_NDIR_BLOCKS + per_block + per_block * per_block,
                        per_block * per_block, visitor, arg);
}
