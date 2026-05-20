#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>
#include <sys/types.h>

#define EXT2_N_BLOCKS 15
#define EXT2_NDIR_BLOCKS 12

#define EXT2_S_IFSOCK 0140000
#define EXT2_S_IFLNK  0120000
#define EXT2_S_IFREG  0100000
#define EXT2_S_IFBLK  0060000
#define EXT2_S_IFDIR  0040000
#define EXT2_S_IFCHR  0020000
#define EXT2_S_IFIFO  0010000
#define EXT2_S_IFMT   0170000

struct ext2_fs {
  int fd;
  char *path;
  uint32_t block_size;
  uint32_t inodes_count;
  uint32_t blocks_count;
  uint32_t first_data_block;
  uint32_t blocks_per_group;
  uint32_t inodes_per_group;
  uint16_t inode_size;
  uint32_t groups_count;
  uint32_t feature_compat;
  uint32_t feature_incompat;
  uint32_t feature_ro_compat;
};

struct ext2_inode {
  uint16_t mode;
  uint16_t uid;
  uint16_t gid;
  uint16_t links_count;
  uint32_t size_low;
  uint32_t atime;
  uint32_t ctime;
  uint32_t mtime;
  uint32_t dtime;
  uint32_t blocks;
  uint32_t flags;
  uint32_t block[EXT2_N_BLOCKS];
  uint32_t generation;
  uint32_t file_acl;
  uint32_t dir_acl;
  uint32_t faddr;
};

typedef int (*ext2_block_visitor)(uint64_t logical, uint32_t physical,
                                  unsigned level, void *arg);

int ext2_open(struct ext2_fs *fs, const char *path);
void ext2_close(struct ext2_fs *fs);
int ext2_read_inode(struct ext2_fs *fs, uint32_t ino, struct ext2_inode *inode);
uint64_t ext2_inode_size(const struct ext2_inode *inode);
const char *ext2_inode_type(const struct ext2_inode *inode);
int ext2_read_block(struct ext2_fs *fs, uint32_t block, void *buf);
int ext2_resolve_block(struct ext2_fs *fs, const struct ext2_inode *inode,
                       uint64_t logical, uint32_t *physical);
int ext2_visit_blocks(struct ext2_fs *fs, const struct ext2_inode *inode,
                      ext2_block_visitor visitor, void *arg);
int ext2_is_regular(const struct ext2_inode *inode);

#endif
