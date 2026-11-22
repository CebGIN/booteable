#!/usr/bin/env bash
# Prueba funcional de la CLI: build/verify en 512 y 4096, tamaño exacto,
# etiqueta configurable, detección de corrupción y control cruzado con fsck.vfat.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_BIN="${1:-$ROOT/build/booteable-image}"
WORK="${2:-$ROOT/build/cli-test}"

rm -rf "$WORK"
mkdir -p "$WORK"

fail() {
    echo "test-image-cli: $*" >&2
    exit 1
}

"$IMAGE_BIN" build --output "$WORK/esp512.img" --size-mb 64 --label BOOTEABLE
"$IMAGE_BIN" verify --image "$WORK/esp512.img"
[[ "$(stat -c%s "$WORK/esp512.img")" == "67108864" ]] ||
    fail "el tamaño de la imagen de 64 MiB no es exacto"

"$IMAGE_BIN" build --output "$WORK/esp4kn.img" --size-mb 320 \
    --sector-size 4096 --esp-mib 260 --label LAB4KN
"$IMAGE_BIN" verify --image "$WORK/esp4kn.img"
[[ "$(stat -c%s "$WORK/esp4kn.img")" == "335544320" ]] ||
    fail "el tamaño de la imagen 4Kn no es exacto"

# La etiqueta configurable debe llegar al BPB.
python3 - "$WORK/esp4kn.img" <<'PY'
import struct
import sys

path = sys.argv[1]
with open(path, "rb") as image:
    image.seek(4096)
    header = image.read(92)
    if header[:8] != b"EFI PART":
        raise SystemExit("test-image-cli: falta la GPT de 4096")
    entries_lba = struct.unpack_from("<Q", header, 72)[0]
    image.seek(entries_lba * 4096)
    entries = image.read(128 * 128)
    first = struct.unpack_from("<Q", entries, 32)[0]
    last = struct.unpack_from("<Q", entries, 40)[0]
    image.seek(first * 4096)
    boot = image.read(4096)
    if struct.unpack_from("<H", boot, 11)[0] != 4096:
        raise SystemExit("test-image-cli: bytes_per_sector incorrecto")
    label = boot[71:82].decode("ascii")
    if label != "LAB4KN     ":
        raise SystemExit(f"test-image-cli: etiqueta inesperada {label!r}")
    esp_length = (last - first + 1) * 4096
print(f"test-image-cli: ESP 4Kn con etiqueta LAB4KN y {esp_length} bytes")
PY

# Un byte alterado en el EFI debe hacer fallar verify.
cp "$WORK/esp512.img" "$WORK/corrupta.img"
python3 - "$WORK/corrupta.img" <<'PY'
import struct
import sys

path = sys.argv[1]
with open(path, "r+b") as image:
    image.seek(512)
    header = image.read(92)
    entries_lba = struct.unpack_from("<Q", header, 72)[0]
    image.seek(entries_lba * 512)
    entries = image.read(128 * 128)
    first = struct.unpack_from("<Q", entries, 32)[0]
    image.seek(first * 512)
    boot = image.read(512)
    fat_sectors = struct.unpack_from("<I", boot, 36)[0]
    data_start = (32 + 2 * fat_sectors) * 512
    image.seek(first * 512 + data_start + 3 * 512)
    byte = image.read(1)
    image.seek(-1, 1)
    image.write(bytes([byte[0] ^ 0x01]))
PY
if "$IMAGE_BIN" verify --image "$WORK/corrupta.img" >/dev/null 2>&1; then
    fail "verify aceptó una imagen con el EFI alterado"
fi

# Control cruzado opcional: fsck.vfat sobre la ESP extraída.
if command -v fsck.vfat >/dev/null 2>&1; then
    python3 - "$WORK/esp512.img" "$WORK/esp.img" <<'PY'
import struct
import sys

source, target = sys.argv[1], sys.argv[2]
with open(source, "rb") as image:
    image.seek(512)
    header = image.read(92)
    entries_lba = struct.unpack_from("<Q", header, 72)[0]
    image.seek(entries_lba * 512)
    entries = image.read(128 * 128)
    first = struct.unpack_from("<Q", entries, 32)[0]
    last = struct.unpack_from("<Q", entries, 40)[0]
    image.seek(first * 512)
    data = image.read((last - first + 1) * 512)
with open(target, "wb") as out:
    out.write(data)
PY
    fsck.vfat -n "$WORK/esp.img"
fi

rm -rf "$WORK"
echo "test-image-cli: OK"
