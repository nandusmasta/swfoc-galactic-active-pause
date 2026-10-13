#!/bin/sh
# Usage: dis.sh <asm_listing> <hex_addr_without_0x> [lines]
n=$(grep -n -m1 "^   $2:" "$1" | cut -d: -f1)
[ -z "$n" ] && { echo "address $2 not found"; exit 1; }
sed -n "${n},$((n + ${3:-80}))p" "$1"
