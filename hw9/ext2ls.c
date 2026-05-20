#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint16_t
get_le16(const unsigned char *p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t
get_le32(const unsigned char *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void
print_name(const unsigned char *name, size_t len)
{
  for (size_t i = 0; i < len; i++) {
    unsigned char c = name[i];
    if (c >= 32 && c <= 126 && c != '\\') {
      putchar(c);
    } else if (c == '\\') {
      fputs("\\\\", stdout);
    } else {
      printf("\\x%02x", c);
    }
  }
}

static int
read_all_stdin(unsigned char **out, size_t *out_size)
{
  size_t cap = 8192, size = 0;
  unsigned char *buf = malloc(cap);

  if (buf == NULL)
    return -1;

  for (;;) {
    ssize_t r;
    if (size == cap) {
      unsigned char *new_buf;
      cap *= 2;
      new_buf = realloc(buf, cap);
      if (new_buf == NULL) {
        free(buf);
        return -1;
      }
      buf = new_buf;
    }

    r = read(STDIN_FILENO, buf + size, cap - size);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      free(buf);
      return -1;
    }
    if (r == 0)
      break;
    size += (size_t)r;
  }

  *out = buf;
  *out_size = size;
  return 0;
}

int
main(void)
{
  unsigned char *buf;
  size_t size, off = 0;

  if (read_all_stdin(&buf, &size) < 0) {
    fprintf(stderr, "ext2ls: cannot read stdin: %s\n", strerror(errno));
    return 1;
  }

  printf("%-12s %s\n", "inode", "name");
  while (off < size) {
    uint32_t inode;
    uint16_t rec_len;
    unsigned char name_len;

    if (size - off < 8) {
      fprintf(stderr, "ext2ls: truncated directory entry at offset %zu\n", off);
      free(buf);
      return 1;
    }

    inode = get_le32(buf + off);
    rec_len = get_le16(buf + off + 4);
    name_len = buf[off + 6];

    if (rec_len < 8 || rec_len > size - off || name_len > rec_len - 8) {
      fprintf(stderr, "ext2ls: bad directory entry at offset %zu\n", off);
      free(buf);
      return 1;
    }

    if (inode != 0) {
      printf("%-12" PRIu32 " ", inode);
      print_name(buf + off + 8, name_len);
      putchar('\n');
    }

    off += rec_len;
  }

  free(buf);
  return 0;
}
