#!/usr/bin/env bash
# Prueba funcional del comando flash sobre un archivo regular: escribe, relee y
# verifica el manifiesto, rechaza tamaños que no coinciden y no escribe sin
# confirmación. No toca dispositivos reales ni requiere root.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_BIN="${1:-$ROOT/build/booteable-image}"
WORK="${2:-$ROOT/build/cli-flash-test}"

rm -rf "$WORK"
mkdir -p "$WORK"

fail() {
    echo "test-cli-flash: $*" >&2
    exit 1
}

"$IMAGE_BIN" build --output "$WORK/image512.img" --size-mb 80 --label BOOTEABLE
"$IMAGE_BIN" verify --image "$WORK/image512.img" >/dev/null

# Destino de prueba del tamaño exacto de la imagen.
truncate -s $((80 * 1024 * 1024)) "$WORK/target.img"

output="$("$IMAGE_BIN" flash --device "$WORK/target.img" \
    --image "$WORK/image512.img" --yes 2>&1)" ||
    fail "flash falló sobre un archivo regular del tamaño correcto"
grep -q "VERIFICADO" <<<"$output" || fail "flash no informó VERIFICADO"
grep -q "Verificado: sí" <<<"$output" || fail "flash no marcó la verificación"
grep -q "Dispositivo sha256:" <<<"$output" ||
    fail "flash no informó el hash releído"
[[ "$(stat -c%s "$WORK/target.img")" == "83886080" ]] ||
    fail "el destino no conserva el tamaño exacto"

# El destino debe contener la misma imagen byte a byte.
if ! cmp -s "$WORK/target.img" "$WORK/image512.img"; then
    fail "el destino no coincide con la imagen escrita"
fi

# Tamaño distinto: se rechaza antes de abrir y no se modifica el destino.
truncate -s $((80 * 1024 * 1024 - 512)) "$WORK/small.img"
before="$(sha256sum "$WORK/small.img" | cut -d' ' -f1)"
if "$IMAGE_BIN" flash --device "$WORK/small.img" \
    --image "$WORK/image512.img" --yes >/dev/null 2>&1; then
    fail "flash aceptó un destino con tamaño distinto"
fi
after="$(sha256sum "$WORK/small.img" | cut -d' ' -f1)"
[[ "$before" == "$after" ]] || fail "flash modificó un destino rechazado"

# Sin --yes y sin entrada interactiva no se escribe nada.
truncate -s $((80 * 1024 * 1024)) "$WORK/unconfirmed.img"
if "$IMAGE_BIN" flash --device "$WORK/unconfirmed.img" \
    --image "$WORK/image512.img" </dev/null >/dev/null 2>&1; then
    fail "flash escribió sin confirmación"
fi
python3 - "$WORK/unconfirmed.img" <<'PY'
import sys

with open(sys.argv[1], "rb") as target:
    chunk = target.read(4096)
if any(chunk):
    raise SystemExit("test-cli-flash: se escribió sin confirmación")
PY

# Una imagen corrupta no debe llegar a la escritura.
cp "$WORK/image512.img" "$WORK/corrupta.img"
python3 - "$WORK/corrupta.img" <<'PY'
import sys

with open(sys.argv[1], "r+b") as image:
    image.seek(600)
    byte = image.read(1)
    image.seek(600)
    image.write(bytes([byte[0] ^ 0x01]))
PY
truncate -s $((80 * 1024 * 1024)) "$WORK/corrupt-target.img"
if "$IMAGE_BIN" flash --device "$WORK/corrupt-target.img" \
    --image "$WORK/corrupta.img" --yes >/dev/null 2>&1; then
    fail "flash aceptó una imagen corrupta"
fi

rm -rf "$WORK"
echo "test-cli-flash: OK"
