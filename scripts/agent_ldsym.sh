#!/bin/bash
nm -n /mnt/c/Users/USER/Desktop/MettaOS/build/user/ldlinux.so | awk '
$1 ~ /^[0-9a-f]+$/ {
  addr = strtonum("0x"$1)
  if (addr <= strtonum("0x9a25")) last = $0
}
END { print last }
'
