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
WAIT_SHUTDOWN_SECONDS="${QEMU_SHUTDOWN_TIMEOUT:-20}"
HEADLESS=0
MARKER="${QEMU_MARKER:-BOOTEABLE-BOOT-OK}"
SECURE_BOOT=0

usage() {
    cat <<'EOF'
Uso: tools/run-qemu.sh [opciones]

  --headless          Sin ventana; escribe la consola serie en el log y verifica
                      el marcador.
  --timeout N         Segundos antes de terminar QEMU (por defecto 30).
  --efi RUTA          Binario EFI a arrancar (por defecto build/efi/BOOTX64.EFI).
  --secure-boot       Prueba negativa: usa OVMF con Secure Boot activo y exige
                      que el binario sin firma NO arranque.
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
        --secure-boot)
            SECURE_BOOT=1
            shift
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

if [[ "$SECURE_BOOT" -eq 1 ]]; then
    OVMF_CODE_FILE="$(first_file \
        "${OVMF_CODE:-}" \
        /usr/share/OVMF/OVMF_CODE_4M.secboot.fd \
        /usr/share/ovmf/OVMF_CODE_4M.secboot.fd \
        /usr/share/OVMF/OVMF_CODE.secboot.fd \
        /usr/share/ovmf/OVMF_CODE.secboot.fd)" || true

    OVMF_VARS_FILE="$(first_file \
        "${OVMF_VARS:-}" \
        /usr/share/OVMF/OVMF_VARS_4M.ms.fd \
        /usr/share/ovmf/OVMF_VARS_4M.ms.fd \
        /usr/share/OVMF/OVMF_VARS.ms.fd \
        /usr/share/ovmf/OVMF_VARS.ms.fd)" || true
else
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
fi

if [[ -z "$OVMF_CODE_FILE" || -z "$OVMF_VARS_FILE" ]]; then
    if [[ "$SECURE_BOOT" -eq 1 ]]; then
        echo "No se encontró OVMF con Secure Boot; se salta la prueba negativa." >&2
    else
        echo "No se encontró OVMF (OVMF_CODE/OVMF_VARS); se salta la prueba virtual." >&2
    fi
    exit 77
fi

mkdir -p "$(dirname "$LOG")"

VARS_COPY="$BUILD/ovmf_vars.fd"
MACHINE_ARGS="q35"
if [[ "$SECURE_BOOT" -eq 1 ]]; then
    VARS_COPY="$BUILD/ovmf_vars_secure.fd"
    MACHINE_ARGS="q35,smm=on"
fi
cp -f "$OVMF_VARS_FILE" "$VARS_COPY"

QEMU_ARGS=(
    -machine "$MACHINE_ARGS"
    -m 256
    -net none
    -no-reboot
    -drive "if=pflash,format=raw,readonly=on,file=$OVMF_CODE_FILE"
    -drive "if=pflash,format=raw,file=$VARS_COPY"
)

if [[ "$SECURE_BOOT" -eq 1 ]]; then
    QEMU_ARGS+=(-global driver=cfi.pflash01,property=secure,value=on)
fi

# La ESP temporal se arma con el directorio build/esp y la opción `fat:rw:` de
# QEMU; no se necesita mkfs.vfat, mtools ni root.
mkdir -p "$ESP_DIR/EFI/BOOT"
cp -f "$EFI" "$ESP_DIR/EFI/BOOT/BOOTX64.EFI"
QEMU_ARGS+=(-drive "file=fat:rw:$ESP_DIR,format=raw")

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

    if [[ "$SECURE_BOOT" -eq 1 ]]; then
        BLOCKED=0
        FOUND=0
        for _ in $(seq 1 $((TIMEOUT_SECONDS * 10))); do
            if grep -q "$MARKER" "$LOG" 2>/dev/null; then
                FOUND=1
                break
            fi
            if grep -qE "Access Denied|Security Violation" "$LOG" 2>/dev/null; then
                BLOCKED=1
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

        if [[ "$FOUND" -eq 1 ]]; then
            echo "Con Secure Boot activo el binario arrancó; se esperaba que no." >&2
            exit 1
        fi
        if [[ "$BLOCKED" -eq 1 ]]; then
            echo "QEMU/OVMF con Secure Boot: el binario sin firma fue bloqueado."
            grep -a -E "Access Denied|Security Violation" "$LOG" | tail -n 3 || true
            exit 0
        fi

        echo "No se pudo comprobar el bloqueo de Secure Boot." >&2
        tail -n 30 "$LOG" >&2 || true
        exit 1
    fi

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

    if [[ "$FOUND" -eq 1 ]] || grep -q "$MARKER" "$LOG" 2>/dev/null; then
        # El programa pide una tecla y apaga a los 10 s. Se espera el apagado
        # limpio (QEMU termina por ACPI) antes de matar el proceso.
        SHUTDOWN=0
        for _ in $(seq 1 $((WAIT_SHUTDOWN_SECONDS * 10))); do
            if ! kill -0 "$QEMU_PID" 2>/dev/null; then
                SHUTDOWN=1
                break
            fi
            sleep 0.1
        done

        if kill -0 "$QEMU_PID" 2>/dev/null; then
            kill "$QEMU_PID" 2>/dev/null || true
        fi
        wait "$QEMU_PID" 2>/dev/null || true

        echo "QEMU/OVMF: marcador '$MARKER' encontrado."
        if [[ "$SHUTDOWN" -eq 1 ]]; then
            echo "QEMU/OVMF: el equipo se apagó solo (ResetSystem)."
        else
            echo "QEMU/OVMF: se terminó QEMU tras ${WAIT_SHUTDOWN_SECONDS}s."
        fi
        grep -a -E "BOOTEABLE-BOOT-OK|Grupo:|Build:|Apagando" "$LOG" \
            | tail -n 6 || true
        exit 0
    fi

    if kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
    fi
    wait "$QEMU_PID" 2>/dev/null || true

    echo "QEMU/OVMF no mostró '$MARKER'." >&2
    if [[ -f "$LOG" ]]; then
        echo "--- últimas líneas de $LOG ---" >&2
        tail -n 30 "$LOG" >&2 || true
    fi
    exit 1
fi

echo "QEMU con ventana. La consola serie se guarda en $LOG."
exec "$QEMU" "${QEMU_ARGS[@]}" -serial "file:$LOG"
