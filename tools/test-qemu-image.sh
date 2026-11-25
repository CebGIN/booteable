#!/usr/bin/env bash
# Construye una imagen GPT/FAT32 completa con el EFI final y la arranca en
# QEMU/OVMF. Con --sector-size 4096 usa una ESP de 260 MiB y un disco virtio
# 4Kn. Sale con 77 si faltan QEMU u OVMF para que CTest lo salte.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_BIN="${1:-$ROOT/build/booteable-image}"
IMAGE="${2:-$ROOT/build/qemu-image-test.img}"
SECTOR_SIZE="${3:-512}"

if [[ "$SECTOR_SIZE" == "4096" ]]; then
    "$IMAGE_BIN" build --output "$IMAGE" --size-mb 320 \
        --sector-size 4096 --esp-mib 260 >/dev/null
else
    "$IMAGE_BIN" build --output "$IMAGE" --size-mb 64 >/dev/null
fi

exec bash "$ROOT/tools/run-qemu.sh" --headless --image "$IMAGE" \
    --sector-size "$SECTOR_SIZE" --timeout 30
