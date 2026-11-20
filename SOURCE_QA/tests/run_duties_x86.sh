#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
flags=(-target i386-linux-gnu -std=c11 -O2 -ffreestanding -fno-builtin -fno-pic -fno-stack-protector -Wall -Wextra -Werror)
clang "${flags[@]}" -c tests/native_duties_x86.c -o build/native_duties_x86.o
clang "${flags[@]}" -c src/fhd_npc_ai.c -o build/duties_core_x86.o
clang "${flags[@]}" -c src/fhd_profession_policy.c -o build/duties_profession_x86.o
clang "${flags[@]}" -c src/fhd_world.c -o build/world_x86.o
clang "${flags[@]}" -c src/fhd_dialogue.c -o build/dialogue_x86.o
clang "${flags[@]}" -c src/fhd_dialogue_settings.c -o build/dialogue_settings_x86.o
clang "${flags[@]}" -c tests/fixtures/local_dialogue.c -o build/local_dialogue_x86.o
ld.lld -m elf_i386 -N -o build/native_duties_x86 build/{native_duties_x86,duties_core_x86,duties_profession_x86}.o build/world_x86.o build/dialogue_x86.o build/dialogue_settings_x86.o build/local_dialogue_x86.o
if "${FHD_QEMU_I386:-qemu-i386}" build/native_duties_x86; then
  echo 'x86 role duties passed'
else
  line=$?
  echo "x86 role duties failed at test source line or status $line" >&2
  exit "$line"
fi
