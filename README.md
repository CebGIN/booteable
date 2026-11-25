# booteable · Preparador UEFI con programa de arranque propio

Herramienta de escritorio (TUI) para Linux que prepara una memoria USB con un
**programa de arranque propio** y sin sistema operativo. Al elegir la USB en el
menú de arranque de un equipo UEFI aparece un mensaje identificable generado por
el código del grupo.

El proyecto tiene dos mitades que se diseñan y prueban por separado:

| Mitad | Qué es | Dónde corre |
|---|---|---|
| **Preparador** | Aplicación TUI que construye la imagen GPT/FAT32, la escribe en la USB y la verifica | Linux, con el SO en ejecución |
| **Programa de arranque** | Aplicación EFI (`BOOTX64.EFI`) que usa los servicios de UEFI | Firmware UEFI, sin SO |

Proyecto de la materia Sistemas Operativos. Interfaz construida con
[GuayabaTUIEngine](https://github.com/CebGIN/GuayabaTUIEngine).

## Estado

**WP0, WP1 y WP2 implementados.**

- **WP1** entrega la aplicación EFI completa: mensaje fijo, colores, salida por
  consola y puerto serie, espera de tecla con límite, apagado limpio con
  `ResetSystem(EfiResetShutdown)` y marcador `BOOTEABLE-BOOT-OK`.
- **WP2** entrega la construcción de la imagen GPT/FAT32: MBR protector, GPT
  primaria y de respaldo, ESP FAT32 con `EFI/BOOT/BOOTX64.EFI`, CRC32, SHA-256,
  verificación por regiones y CLI `build`/`verify` para sectores de 512 y 4096.
  La imagen es dispersa y determinista.

La enumeración real de dispositivos, la escritura en la USB y el flujo completo
de la TUI son WP3a/WP3b/WP4a/WP4b/WP5; por eso `booteable-image flash` todavía no
escribe dispositivos.

La ruta elegida es **UEFI x86_64** con tabla **GPT** y una **ESP FAT32** que
contiene `\EFI\BOOT\BOOTX64.EFI`. Es la ruta que el equipo de prueba admite y la
que permite un arranque reproducible en QEMU con OVMF antes de tocar una USB
física.

| Documento | Contenido |
|---|---|
| [`docs/Requerimientos.md`](docs/Requerimientos.md) | Enunciado, requerimientos derivados y pruebas exigidas |
| [`docs/Arquitectura.md`](docs/Arquitectura.md) | ADRs, componentes, modelo, formato de imagen, seguridad, UI y pruebas |
| [`docs/Glosario.md`](docs/Glosario.md) | Términos de UEFI, particiones, FAT32 y de la interfaz |
| [`docs/Plan-de-trabajo.md`](docs/Plan-de-trabajo.md) | Paquetes de trabajo, responsabilidades y criterios de salida |
| [`docs/diagrama.drawio`](docs/diagrama.drawio) | Clases, secuencia de preparación y leyenda UML |

## Requisitos

- Linux x86_64 (el preparador usa `sysfs`, `ioctl` de bloque, `umount2` y `open`
  con `O_EXCL`).
- CMake 3.20 o superior.
- Compilador con C++17.
- `x86_64-w64-mingw32-gcc/g++` para construir la aplicación EFI.
- Python 3.8 o superior, para generar el EFI incrustado en tiempo de compilación.
- Git, para el submódulo del motor.
- Opcional: QEMU + OVMF para la prueba virtual.
- Opcional: `draw.io` 24 o superior para editar/exportar el diagrama.

## Compilar

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Targets:

| Target | Descripción |
|---|---|
| `booteable_core` | Núcleo headless: dominio, contratos, fake, CRC32/SHA-256, imagen y EFI incrustado |
| `booteable_ui` | Escenas de la TUI sobre Guayaba |
| `booteable` | Aplicación TUI |
| `booteable-image` | CLI headless para construir, verificar y escribir una imagen |
| `bootx64_efi` | Aplicación EFI que arranca el firmware |
| `booteable_tests` | Suites headless registradas en CTest |

La aplicación EFI se compila con el *cross-compiler* de MinGW-w64
(`-ffreestanding -nostdlib -Wl,--subsystem,10`) y se **incrusta** en el
preparador como un arreglo de bytes. `tools/embed-efi.py` genera el arreglo, su
tamaño y el SHA-256 en `build/generated/EmbeddedBootImage.cpp`. Así el binario de
la TUI es autocontenido y no depende de encontrar `BOOTX64.EFI` en el disco.

La imagen se crea como archivo disperso del tamaño del destino y solo se
escriben las regiones con contenido (MBR, GPT, ESP y GPT de respaldo). Preparar
una USB de 64 GB no reserva 64 GB en RAM; la verificación compara únicamente esas
regiones mediante un manifiesto de hashes.

La etiqueta y el tamaño de la ESP se eligen en la TUI o por CLI. Por defecto son
`BOOTEABLE` y 64 MiB con sectores de 512 bytes; con sectores de 4096 (4Kn) el
mínimo y el valor por defecto suben a 260 MiB.

El profesor permite librerías y APIs del sistema operativo, pero las operaciones
de detectar, particionar, formatear, escribir, sincronizar y verificar las hace
nuestro código. Por eso GPT y FAT32 se generan en `image/` y no se invocan
`parted`, `mkfs.*` ni `dd` como binarios externos.

## Ejecutar

```bash
./build/booteable        # TUI de WP0: escena mínima con datos simulados; F10 sale
```

En WP4 la TUI va a enumerar las USB, mostrar identificador, capacidad, serie,
puntos de montaje y la etiqueta/tamaño elegidos, y exigir la casilla de
advertencia más `Enter` antes de escribir. Sin privilegios se podrá explorar y
exportar una imagen, pero no escribir un dispositivo.

La CLI de WP2 construye y verifica imágenes sin tocar hardware:

```bash
# Construir una imagen de 64 MiB con el EFI final incrustado
./build/booteable-image build --output build/booteable.img --size-mb 64

# Releer estructura, CRC, FAT, árbol EFI/BOOT y SHA-256 del binario
./build/booteable-image verify --image build/booteable.img

# Imagen para un dispositivo 4Kn (sector lógico de 4096)
./build/booteable-image build --output build/booteable-4kn.img \
  --sector-size 4096 --esp-mib 260

# Cambiar la etiqueta visible en FAT32 y GPT
./build/booteable-image build --output build/lab.img --size-mb 80 \
  --label BOOTLAB
```

`--size-mb` es el tamaño exacto del archivo. En una imagen de 64 MiB la ESP
automática de 64 MiB no entra junto a la GPT, así que se reduce al mayor tamaño
posible (unos 63 MiB, por encima del mínimo FAT32). Si se pide `--esp-mib`
explícito y no entra, el build falla. `flash` llega en WP3b; hasta entonces la
CLI no escribe dispositivos.

## Prueba virtual

El programa EFI se compila e incrusta en cada build. Para arrancarlo en QEMU con
OVMF sin tocar hardware:

```bash
./tools/run-qemu.sh --headless
```

El script detecta OVMF (`/usr/share/OVMF/OVMF_CODE_4M.fd`, `OVMF_CODE.fd` o las
variables `OVMF_CODE`/`OVMF_VARS`), arma una ESP temporal con `fat:rw:` y busca
el marcador `BOOTEABLE-BOOT-OK` en la consola serie. Después espera el apagado
limpio del equipo: el programa se apaga solo a los 10 s o al pulsar una tecla.
Si faltan QEMU u OVMF, sale con código 77 para que CTest lo salte.

También se puede arrancar desde la imagen GPT/FAT32 completa que construye el
preparador, incluida una imagen 4Kn:

```bash
./tools/run-qemu.sh --headless --image build/booteable.img
./tools/run-qemu.sh --headless --image build/booteable-4kn.img \
  --sector-size 4096
```

## Pruebas

```bash
ctest --test-dir build --output-on-failure
```

Las pruebas son headless, no requieren red, root ni una USB real:

- `unit`: dominio, controlador falso, CRC32/SHA-256, geometría, GPT, FAT32,
  roundtrip de imagen y corrupción de cada región.
- `efi_binary`: cabecera PE32+, subsistema EFI, ausencia de imports de Windows y
  presencia del marcador.
- `image_cli`: `build`/`verify` en 512 y 4096, tamaño exacto, etiqueta, detección
  de corrupción y control cruzado opcional con `fsck.vfat`.
- `qemu_boot` y `qemu_boot_image`: arranque real en OVMF, desde la carpeta y
  desde la imagen construida. Se saltan con código 77 si faltan QEMU u OVMF.
- `qemu_boot_4kn`: la misma imagen con sector lógico de 4096 y ESP de 260 MiB,
  presentada como disco virtio 4Kn.
- `qemu_secure_boot`: prueba negativa con OVMF Secure Boot; exige que el binario
  sin firma sea bloqueado. También se salta si faltan las imágenes de OVMF con
  Secure Boot.

## Secure Boot

El binario EFI **no está firmado**. Con Secure Boot activo, el firmware del
equipo de prueba no lo ejecutará; la herramienta no desactiva ni modifica claves.
Para la prueba física se usa un equipo de laboratorio con autorización y Secure
Boot desactivado. La firma con clave propia y `MOK` queda como trabajo futuro.

La prueba negativa se puede repetir a mano:

```bash
./tools/run-qemu.sh --headless --secure-boot
# QEMU/OVMF con Secure Boot: el binario sin firma fue bloqueado.
```

## Diagrama

Páginas:

1. **Clases**: enums, structs del dominio, contratos e implementaciones, con
   compartimentos y relaciones UML nativas.
2. **Secuencia**: enumerar, validar, confirmar, construir, escribir, sincronizar,
   verificar y cancelar.
3. **Leyenda**: relaciones UML, visibilidad y estereotipos.

Las vistas previas están en [`docs/diagramas/`](docs/diagramas/README.md) y se
regeneran con `python3 tools/generate_diagram.py` y el CLI de draw.io.

## Estructura

```text
boot/         Aplicación EFI propia (freestanding, sin libc)
src/
  domain/     Tipos, enums y validación pura (congelado en WP0)
  core/       SHA-256, CRC32 y utilidades de bytes
  image/      ImageLayout, GPT, FAT32, ensamblado y verificador de imagen
  device/     Enumeración sysfs, política, escritor y montajes (WP3)
  app/        Controlador, runner, estado y mocks (WP4b)
  ui/         Escenas, confirmación y progreso (WP4a)
  cli/        build y verify; flash llega en WP3b
tools/        build-efi, embed-efi, run-qemu, check-efi, test-image-cli
tests/        Pruebas headless con CTest
docs/         Requerimientos, arquitectura, glosario, plan y diagrama
```

## Seguridad

- La herramienta solo acepta dispositivos **extraíbles por USB** con identidad
  verificable (serie y `by-id`) y rechaza el disco que contiene `/` o `/boot`.
- Vuelve a comprobar la identidad del dispositivo inmediatamente antes de abrir y
  después de cerrar; si cambió, falla sin declarar éxito.
- Exige marcar una casilla y pulsar `Enter`, y advierte que se perderá todo el
  contenido. Desmonta las particiones montadas antes de escribir.
- WP2 construye la imagen, hace `fsync`, la relee y compara sus regiones antes de
  informar éxito. WP3b hará lo mismo sobre el dispositivo: escribir, sincronizar,
  releer y comparar SHA-256. Una operación cancelada o interrumpida quedará
  marcada como **incompleta**.
- El programa de arranque no escribe en discos: solo usa consola/serie, espera
  una tecla con límite y se apaga con `ResetSystem`.
- Secure Boot bloquea binarios sin firma. La herramienta **no** desactiva ni
  altera claves: si el equipo lo tiene activo, se documenta el requisito y se
  prueba en un equipo de laboratorio con autorización.
