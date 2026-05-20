#!/bin/sh
set -eu

IMG=${IMG:-ext2.img}
MNT=${MNT:-ext2}
OUT=${OUT:-test-out}
BLOCK_SIZE=${BLOCK_SIZE:-2048}
SIZE=${SIZE:-96M}
VALGRIND=${VALGRIND:-0}

run_ext2inode()
{
  if [ "$VALGRIND" = 1 ]; then
    valgrind --quiet --leak-check=full --error-exitcode=97 ./ext2inode "$@"
  else
    ./ext2inode "$@"
  fi
}

run_ext2cat()
{
  if [ "$VALGRIND" = 1 ]; then
    valgrind --quiet --leak-check=full --error-exitcode=97 ./ext2cat "$@"
  else
    ./ext2cat "$@"
  fi
}

run_ext2ls()
{
  if [ "$VALGRIND" = 1 ]; then
    valgrind --quiet --leak-check=full --error-exitcode=97 ./ext2ls "$@"
  else
    ./ext2ls "$@"
  fi
}

need()
{
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing tool: $1" >&2
    exit 1
  }
}

inode_of()
{
  stat -c '%i' "$1"
}

make_host_files()
{
  mkdir -p "$OUT/host"
  printf 'hello ext2\n' > "$OUT/host/small.txt"
  dd if=/dev/urandom of="$OUT/host/indirect.bin" bs="$BLOCK_SIZE" count=40 status=none
}

main()
{
  need mkfs.ext2
  need sha512sum
  need sudo
  need losetup
  need lsblk
  if [ "$VALGRIND" = 1 ]; then
    need valgrind
  fi

  mkdir -p "$MNT" "$OUT"
  make_host_files
  truncate --size "$SIZE" "$IMG"
  mkfs.ext2 -F -b "$BLOCK_SIZE" "$IMG" >/dev/null

  mounted=0
  loopdev=
  cleanup()
  {
    if [ "$mounted" = 1 ]; then
      sudo umount "$MNT"
    fi
    if [ -n "$loopdev" ]; then
      sudo losetup -d "$loopdev"
    fi
  }
  trap cleanup EXIT INT TERM

  sudo mount -t ext2 -o loop "$IMG" "$MNT"
  mounted=1
  sudo chown "$(id -u)":"$(id -g)" "$MNT"

  mkdir -p "$MNT/dir1/dir2" "$MNT/dir3"
  cp "$OUT/host/small.txt" "$MNT/dir1/small.txt"
  cp "$OUT/host/indirect.bin" "$MNT/dir1/dir2/indirect.bin"
  truncate -s 5G "$MNT/dir3/sparse-5g.bin"
  printf 'tail' | dd of="$MNT/dir3/sparse-5g.bin" bs=1 seek=5368709116 conv=notrunc status=none

  SMALL_INO=$(inode_of "$MNT/dir1/small.txt")
  BIG_INO=$(inode_of "$MNT/dir1/dir2/indirect.bin")
  SPARSE_INO=$(inode_of "$MNT/dir3/sparse-5g.bin")
  DIR1_INO=$(inode_of "$MNT/dir1")

  sha512sum "$MNT/dir1/small.txt" > "$OUT/small.expected.sha512"
  sha512sum "$MNT/dir1/dir2/indirect.bin" > "$OUT/indirect.expected.sha512"

  sync
  sudo umount "$MNT"
  mounted=0

  run_ext2inode "$IMG" "$BIG_INO" > "$OUT/full-indirect-inode.txt"
  grep -q 'single_indirect: [1-9]' "$OUT/full-indirect-inode.txt"
  run_ext2inode "$IMG" "$SPARSE_INO" > "$OUT/full-sparse-inode.txt"
  grep -q 'Size: 5368709120' "$OUT/full-sparse-inode.txt"

  run_ext2cat "$IMG" "$SMALL_INO" | sha512sum > "$OUT/small.actual.sha512"
  awk '{print $1}' "$OUT/small.expected.sha512" > "$OUT/small.expected.hash"
  awk '{print $1}' "$OUT/small.actual.sha512" | cmp -s - "$OUT/small.expected.hash"

  run_ext2cat "$IMG" "$BIG_INO" | sha512sum > "$OUT/indirect.actual.sha512"
  awk '{print $1}' "$OUT/indirect.expected.sha512" > "$OUT/indirect.expected.hash"
  awk '{print $1}' "$OUT/indirect.actual.sha512" | cmp -s - "$OUT/indirect.expected.hash"

  run_ext2cat "$IMG" "$DIR1_INO" | run_ext2ls > "$OUT/full-dir1.txt"
  grep -q 'small.txt' "$OUT/full-dir1.txt"
  grep -q 'dir2' "$OUT/full-dir1.txt"

  loopdev=$(sudo losetup -f --show "$IMG")
  if ! run_ext2inode "$loopdev" "$BIG_INO" > "$OUT/loop-inode.txt" 2>"$OUT/loop.err"; then
    sudo chmod a+r "$loopdev"
    run_ext2inode "$loopdev" "$BIG_INO" > "$OUT/loop-inode.txt"
  fi
  lsblk -o name,size,fstype "$loopdev" > "$OUT/loop-lsblk.txt"
  sudo losetup -d "$loopdev"
  loopdev=

  echo "test passed; files kept in $OUT, image kept as $IMG"
}

if [ "$#" -ne 0 ]; then
  echo "usage: $0" >&2
  exit 1
fi

main
