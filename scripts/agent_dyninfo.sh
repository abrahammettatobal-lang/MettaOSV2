#!/usr/bin/env bash
set -e
ROOT=/mnt/c/Users/USER/Desktop/MettaOS
gcc -O2 -o /tmp/dynhello "$ROOT/kernel/linux/userland/dynhello.c"
echo "=== ldd ==="
ldd /tmp/dynhello
echo "=== sizes ==="
ls -l /tmp/dynhello /lib64/ld-linux-x86-64.so.2
ldd /tmp/dynhello | awk '/=>/ {print $3}' | while read -r p; do ls -l "$p"; done
echo "=== ld header ==="
readelf -h /lib64/ld-linux-x86-64.so.2 | sed -n '1,20p'
echo "=== ld loads ==="
readelf -l /lib64/ld-linux-x86-64.so.2 | head -30
echo "=== strace ==="
strace -e trace=openat,mmap,arch_prctl,set_tid_address,brk,mprotect,munmap,read,close,fstat,newfstatat,statx,prlimit64,readlink,readlinkat,access,faccessat,faccessat2,ioctl,exit_group,write,uname,getuid,geteuid,getpid /tmp/dynhello 2>&1 | head -100
