#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
clang -target i386-linux-gnu -std=c11 -O2 -ffreestanding -fno-builtin -fno-pic \
  -fno-stack-protector -Wall -Wextra -Werror -c tests/native_settings_x86.c -o build/native_settings_x86.o
ld.lld -m elf_i386 -N -o build/native_settings_x86 build/native_settings_x86.o
if "${FHD_QEMU_I386:-qemu-i386}" build/native_settings_x86; then
  echo 'x86 settings tab passed'
else
  line=$?
  echo "x86 settings tab failed at test source line or status $line" >&2
  exit "$line"
fi
