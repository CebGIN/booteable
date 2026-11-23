#!/usr/bin/env bash
# Compila la aplicación EFI x86_64 con MinGW-w64.
#
# Este script es la única fuente de los flags de compilación EFI. CMake lo
# invoca para producir build/efi/BOOTX64.EFI y las pruebas lo pueden ejecutar
# a mano. No usa EDK2 ni gnu-efi.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${EFI_CXX:-x86_64-w64-mingw32-g++}"
OUTPUT="${EFI_OUTPUT:-$ROOT/build/efi/BOOTX64.EFI}"

if ! command -v "$CXX" >/dev/null 2>&1; then
    echo "No se encontró '$CXX'." >&2
    echo "Instalá MinGW-w64 (por ejemplo: sudo apt install g++-mingw-w64-x86-64)." >&2
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT")"

"$CXX" \
    -std=c++17 \
    -O2 \
    -Wall -Wextra -Wpedantic \
    -ffreestanding \
    -fno-exceptions \
    -fno-rtti \
    -fno-stack-protector \
    -fno-use-cxa-atexit \
    -fno-threadsafe-statics \
    -mno-red-zone \
    -nostdlib \
    -Wl,--subsystem,10 \
    -Wl,-e,efi_main \
    -Wl,--no-insert-timestamp \
    -o "$OUTPUT" \
    "$ROOT/boot/main.cpp" \
    "$ROOT/boot/efi.cpp"

echo "EFI generado: $OUTPUT"
