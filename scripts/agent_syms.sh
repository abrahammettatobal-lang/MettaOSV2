#!/usr/bin/env bash
cd /mnt/c/Users/USER/Desktop/MettaOS || exit 1
size build/mettaos.bin
nm -S --size-sort build/mettaos.bin | tail -15
nm -n build/mettaos.bin | grep -iE "bss|_end$|kernel_end|g_scene$|g_comp_back$"
grep -nE "0x200000|2 \* 1024|PAGE|pd_|pdpt|map" boot/longmode.S | head -40
