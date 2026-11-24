#!/usr/bin/env bash
cd /mnt/c/Users/USER/Desktop/MettaOS || exit 1
make ${@:-all} > /tmp/build.log 2>&1
rc=$?
echo "EXIT=$rc"
grep -E "error|warning|undefined" /tmp/build.log | head -100
tail -n 5 /tmp/build.log
