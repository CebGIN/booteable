#!/usr/bin/env bash
# Arranca la imagen EFI en QEMU con OVMF.
#
# La ESP de prueba se arma con el directorio build/esp y la opción `fat:rw:`
# de QEMU; no se necesita mkfs.vfat, mtools ni root. En modo headless el
# programa debe imprimir BOOTEABLE-BOOT-OK por la consola serie.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
EFI="${EFI_FILE:-$BUILD/efi/BOOTX64.EFI}"
ESP_DIR="${ESP_DIR:-$BUILD/esp}"
LOG="${SERIAL_LOG:-$BUILD/qemu-serial.log}"
QEMU="${QEMU_BIN:-qemu-system-x86_64}"
TIMEOUT_SECONDS="${QEMU_TIMEOUT:-30}"
HEADLESS=0
MARKER="${QEMU_MARKER:-BOOTEABLE-BOOT-OK}"

usage() {
    cat <<'EOF'
Uso: tools/run-qemu.sh [opciones]

  --headless          Sin ventana; escribe la consola serie en el log y verifica
                      el marcador.
  --timeout N         Segundos antes de terminar QEMU (por defecto 30).
  --efi RUTA          Binario EFI a arrancar (por defecto build/efi/BOOTX64.EFI).
  --log RUTA          Archivo de salida serie (por defecto build/qemu-serial.log).
  -h, --help          Esta ayuda.

Variables: OVMF_CODE, OVMF_VARS, QEMU_BIN, BUILD_DIR.
Sale con 77 si faltan QEMU u OVMF, para que CTest lo pueda saltar.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --headless)
            HEADLESS=1
            shift
            ;;
        --timeout)
            TIMEOUT_SECONDS="$2"
            shift 2
            ;;
        --efi)
            EFI="$2"
            shift 2
            ;;
        --log)
            LOG="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Opción desconocida: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if ! command -v "$QEMU" >/dev/null 2>&1; then
    echo "No se encontró '$QEMU'; se salta la prueba virtual." >&2
    exit 77
fi

if [[ ! -f "$EFI" ]]; then
    echo "Falta '$EFI'. Compilá primero con: cmake --build build" >&2
    exit 1
fi

first_file() {
    local candidate
    for candidate in "$@"; do
        if [[ -n "$candidate" && -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

OVMF_CODE_FILE="$(first_file \
    "${OVMF_CODE:-}" \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/ovmf/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/ovmf/OVMF.fd \
    /usr/share/edk2/x64/OVMF_CODE.fd \
    /usr/share/edk2-ovmf/x64/OVMF_CODE.fd)" || true

OVMF_VARS_FILE="$(first_file \
    "${OVMF_VARS:-}" \
    /usr/share/OVMF/OVMF_VARS_4M.fd \
    /usr/share/ovmf/OVMF_VARS_4M.fd \
    /usr/share/OVMF/OVMF_VARS.fd \
    /usr/share/ovmf/OVMF_VARS.fd \
    /usr/share/edk2/x64/OVMF_VARS.fd \
    /usr/share/edk2-ovmf/x64/OVMF_VARS.fd)" || true

if [[ -z "$OVMF_CODE_FILE" || -z "$OVMF_VARS_FILE" ]]; then
    echo "No se encontró OVMF (OVMF_CODE/OVMF_VARS); se salta la prueba virtual." >&2
    exit 77
fi

mkdir -p "$ESP_DIR/EFI/BOOT" "$(dirname "$LOG")"
cp -f "$EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"

VARS_COPY="$BUILD/ovmf_vars.fd"
cp -f "$OVMF_VARS_FILE" "$VARS_COPY"

QEMU_ARGS=(
    -machine q35
    -m 256
    -net none
    -no-reboot
    -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE_FILE"
    -drive "if=pflash,format=raw,file=$VARS_COPY"
    -drive "file=fat:rw:$ESP_DIR,format=raw"
)

if [[ "$HEADLESS" -eq 0 && -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
    HEADLESS=1
fi

if [[ "$HEADLESS" -eq 1 ]]; then
    rm -f "$LOG"
    "$QEMU" \
        "${QEMU_ARGS[@]}" \
        -display none \
        -serial "file:$LOG" &
    QEMU_PID=$!

    FOUND=0
    for _ in $(seq 1 $((TIMEOUT_SECONDS * 10))); do
        if grep -q "$MARKER" "$LOG" 2>/dev/null; then
            FOUND=1
            break
        fi
        if ! kill -0 "$QEMU_PID" 2>/dev/null; then
            break
        fi
        sleep 0.1
    done

    if kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
    fi
    wait "$QEMU_PID" 2>/dev/null || true

    if [[ "$FOUND" -eq 1 ]] || grep -q "$MARKER" "$LOG" 2>/dev/null; then
        echo "QEMU/OVMF: marcador '$MARKER' encontrado."
        grep -a -E "booteable|BOOTEABLE-BOOT-OK" "$LOG" | tail -n 5 || true
        exit 0
    fi

    echo "QEMU/OVMF no mostró '$MARKER'." >&2
    if [[ -f "$LOG" ]]; then
        echo "--- últimas líneas de $LOG ---" >&2
        tail -n 30 "$LOG" >&2 || true
    fi
    exit 1
fi

echo "QEMU con ventana. La consola serie se guarda en $LOG."
exec "$QEMU" "${QEMU_ARGS[@]}" -serial "file:$LOG"
