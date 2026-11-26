# Evidencias de WP5

Registro de la corrida de cierre del **26 de noviembre de 2026**. Los archivos
crudos quedan en `build/evidencia/`, que está ignorado por Git; este documento
guarda los comandos, los resultados y los hashes que importan.

## Entorno

| Elemento | Valor |
|---|---|
| Sistema | Linux 7.0.0-43-generic, x86_64 |
| CMake | 3.28.3 |
| Compilador | GCC/G++ 13.3.0 (Ubuntu 24.04) |
| QEMU | 8.2.2 |
| OVMF | `/usr/share/OVMF/OVMF_CODE_4M.fd` y `OVMF_VARS_4M.fd` |
| draw.io CLI | 32.7.4 |
| Commit base | `c18e2a5` (`main` antes del cierre de WP5) |
| EFI final | `a8aa32d26a506bfb4311f5d397a6635c5aa1d79882e2156ca6cfa9bd143df2be` (9 492 bytes) |

## Build y pruebas

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build -j4 --output-on-failure
```

Resultado de la corrida de cierre: **21/21 pruebas aprobadas**. Tiempos reales
en 4 núcleos: **61,98 s** en la última corrida con `ctest -j4`, y entre 69,60 y
93,45 s en corridas anteriores con más carga de QEMU. El target `check` dio
80,95 s. Todos quedan por debajo del presupuesto de 2 minutos.

| Prueba | Resultado |
|---|---|
| `test_validation`, `test_fake_controller`, `test_core`, `test_layout`, `test_gpt` | OK |
| `test_fat32`, `test_image_roundtrip` | OK |
| `test_safety_policy`, `test_sysfs_source`, `test_mount_manager`, `test_device_writer` | OK |
| `test_flash` | OK (35,63 s) |
| `test_prep_controller`, `test_ui_flow` | OK |
| `efi_binary` | OK |
| `image_cli` | OK (31,52 s) |
| `flash_cli` | OK |
| `qemu_boot`, `qemu_boot_image`, `qemu_boot_4kn`, `qemu_secure_boot` | OK |

Las cuatro pruebas QEMU comparten OVMF y el log serie; CTest las serializa con
`RESOURCE_LOCK qemu_ovmf` aunque se ejecute con `-j4`.

## Imágenes con el EFI final

```bash
./build/booteable-image build --output build/booteable.img \
  --size-mb 64 --label BOOTEABLE
./build/booteable-image verify --image build/booteable.img

./build/booteable-image build --output build/booteable-4kn.img \
  --size-mb 320 --sector-size 4096 --esp-mib 260 --label BOOTEABLE
./build/booteable-image verify --image build/booteable-4kn.img
```

| Imagen | Tamaño | ESP | Manifiesto SHA-256 | Archivo SHA-256 |
|---|---|---|---|---|
| 512 | 67 108 864 B (64 MiB) | 62 MiB, FAT 993 sectores, 126 973 clústeres | `de68c1f00089ec948f86f78d1e806cae2e8fbc6e6fb850a15c6184b6ec39af26` | `2a0c72ba74dd2067037dfcc2f6b1a6cfa103af5d1332fd94f1748bf42f66a869` |
| 4Kn | 335 544 320 B (320 MiB) | 260 MiB, FAT 65 sectores, 66 398 clústeres | `e5fdfd78c84fa73e69651aac7ad343bd5b6a7ec484fc9e697d44f4d2edce17d6` | `646fbac2df04a1478f5a18b42fcc432c8a0bc64a75d5ca6aef35c2a061ed8c2c` |

`booteable-image verify` releyó MBR, GPT primaria, ESP FAT32, GPT de respaldo y
el manifiesto en ambas imágenes. El binario EFI incrustado coincide en tamaño y
SHA-256 con `build/efi/BOOTX64.EFI`.

La imagen de 64 MiB usa la reducción automática de ESP: el constructor elige
62 MiB para que quepan GPT y respaldo. La validación de la TUI aplica la misma
regla desde WP5; un `esp_bytes = 0` sobre 64 MiB es válido y una ESP explícita
de 64 MiB se rechaza. Un destino de 1 MiB, que ni siquiera llega al LBA de inicio
de la ESP, también se rechaza sin desbordar el cálculo (`test_layout` y
`test_validation`).

## QEMU/OVMF

```bash
./tools/run-qemu.sh --headless
./tools/run-qemu.sh --headless --image build/booteable.img
./tools/run-qemu.sh --headless --image build/booteable-4kn.img --sector-size 4096
./tools/run-qemu.sh --headless --secure-boot
```

Salida común de los tres arranques válidos:

```text
QEMU/OVMF: marcador 'BOOTEABLE-BOOT-OK' encontrado.
QEMU/OVMF: el equipo se apagó solo (ResetSystem).
Grupo: booteable
Build: 723737e91267
BOOTEABLE-BOOT-OK
Apagando el equipo.
```

Prueba negativa con Secure Boot:

```text
QEMU/OVMF con Secure Boot: el binario sin firma fue bloqueado.
BdsDxe: failed to load Boot0001 ...: Access Denied
```

La prueba con ventana requiere `DISPLAY` o `WAYLAND_DISPLAY`; en esta corrida se
usó el modo headless, que es el que ejecuta CTest.

## Diagrama

```bash
python3 tools/generate_diagram.py
for i in 1 2 3 4 5; do
  case $i in
    1) name=01-componentes ;;
    2) name=02-clases ;;
    3) name=03-secuencia ;;
    4) name=04-estados ;;
    5) name=05-leyenda ;;
  esac
  drawio -x -f png -o "docs/diagramas/$name.png" -p "$i" -s 1.3 docs/diagrama.drawio
done
```

Se regeneraron las cinco páginas. La página 2 incorpora `PrepRunner` y
`DeviceFlasher`; la página 3 muestra el flujo
`AppScene → PrepController → PrepRunner → DeviceFlasher → PosixDeviceWriter`.

## Prueba física P9

Pendiente de ejecución en el equipo de laboratorio. El procedimiento y los
datos que hay que registrar están en [`Prueba-fisica.md`](Prueba-fisica.md).
