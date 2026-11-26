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

**WP0, WP1, WP2, WP3a, WP3b, WP4a, WP4b y WP5 implementados.** La prueba
física P9 queda documentada y pendiente de ejecución en el laboratorio.

- **WP1** entrega la aplicación EFI completa: mensaje fijo, colores, salida por
  consola y puerto serie, espera de tecla con límite, apagado limpio con
  `ResetSystem(EfiResetShutdown)` y marcador `BOOTEABLE-BOOT-OK`.
- **WP2** entrega la construcción de la imagen GPT/FAT32: MBR protector, GPT
  primaria y de respaldo, ESP FAT32 con `EFI/BOOT/BOOTX64.EFI`, CRC32, SHA-256,
  verificación por regiones y CLI `build`/`verify` para sectores de 512 y 4096.
  La imagen es dispersa y determinista.
- **WP3a** entrega la enumeración real desde sysfs (`/sys/block`, modelo, serie,
  tamaño, transporte, `by-id`, particiones y montajes) y la política de seguridad
  pura, probada con fixtures y sin hardware.
- **WP3b** entrega el desmontaje controlado, el escritor POSIX, los fakes y el
  comando `flash`, que valida identidad y geometría, desmonta, abre con `O_EXCL`,
  escribe, sincroniza y relee el manifiesto por regiones.
- **WP4a** entrega las escenas navegables de la TUI (`RailScene`, `MainScene`,
  `StepScene`, `ConfirmScene`, `ResultScene`), el formato de
  tamaños/hashes/estados y el flujo completo contra `FakePrepController`, con
  layout apilado a 80×24 y de dos columnas a 120×40.
- **WP4b** entrega `PrepController`, con hilo de trabajo y cancelación, y
  `PrepRunner`, que construye la imagen, desmonta, escribe, sincroniza y verifica
  sobre datos reales. La TUI usa el controlador real desde `main`; sin `root` se
  puede enumerar y exportar, pero la escritura queda deshabilitada con el mensaje
  de privilegios.
- **WP5** cierra la integración: las suites se ejecutan por separado en CTest,
  la validación aplica la misma reducción automática de ESP que el constructor
  de imagen, las pruebas QEMU no comparten OVMF en paralelo y la evidencia,
  el checklist físico y el guion de demo quedan en `docs/`.

La CLI sigue disponible para construir, verificar y escribir imágenes de forma
headless. El flujo completo de la TUI está probado contra `FakePrepController` y
contra el controlador real con archivos regulares; la única prueba que necesita
hardware es P9.

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
| [`docs/Evidencias.md`](docs/Evidencias.md) | Resultados de build, CTest, imágenes y QEMU/OVMF |
| [`docs/Prueba-fisica.md`](docs/Prueba-fisica.md) | Checklist de P9 en el equipo de laboratorio |
| [`docs/Guion-demo.md`](docs/Guion-demo.md) | Guion de la demo y resultados esperados |
| [`docs/diagrama.drawio`](docs/diagrama.drawio) | Componentes, clases, secuencia, estados y leyenda UML |

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

# Presupuesto de WP5: reparte las suites entre cuatro núcleos.
ctest --test-dir build -j4 --output-on-failure

# Equivalente con target de CMake.
cmake --build build --target check
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
| `check` | Atajo que ejecuta `ctest -j4 --output-on-failure` |

La aplicación EFI se compila con el *cross-compiler* de MinGW-w64
(`-ffreestanding -nostdlib -Wl,--subsystem,10`) y se **incrusta** en el
preparador como un arreglo de bytes. `tools/embed-efi.py` genera el arreglo, su
tamaño y el SHA-256 en `build/generated/EmbeddedBootImage.cpp`. Así el binario de
la TUI es autocontenido y no depende de encontrar `BOOTX64.EFI` en el disco.

La imagen se crea como archivo disperso del tamaño del destino y solo se
escriben las regiones con contenido (MBR, GPT, ESP y GPT de respaldo). Preparar
una USB de 64 GB no reserva 64 GB en RAM: el preparador escribe únicamente esas
regiones, salteando los huecos, y la verificación compara únicamente esas
regiones mediante un manifiesto de hashes.

La CLI permite elegir etiqueta y tamaño de ESP con `--label` y `--esp-mib`. La
TUI usa `BOOTEABLE` y el tamaño automático: 64 MiB con sectores de 512 y 260 MiB
con sectores de 4096 (4Kn). Si el destino es pequeño, la ESP automática se
reduce al mayor tamaño que quepa sin bajar del mínimo FAT32.

El profesor permite librerías y APIs del sistema operativo, pero las operaciones
de detectar, particionar, formatear, escribir, sincronizar y verificar las hace
nuestro código. Por eso GPT y FAT32 se generan en `image/` y no se invocan
`parted`, `mkfs.*` ni `dd` como binarios externos.

## Ejecutar

```bash
sudo ./build/booteable   # TUI real; sin sudo enumera y exporta, pero no escribe
```

La TUI recorre el wizard Detectar → Particionar → Formatear → Escribir →
Sincronizar → Verificar. Enumera las USB, muestra identificador, capacidad,
serie, puntos de montaje y la etiqueta/tamaño de la ESP, y exige la casilla de
advertencia más `Enter` antes de escribir. Las tarjetas de paso se leen con
`[ Continuar ]`; la única pausa real es después de construir la imagen y antes
de abrir la USB, donde el botón pasa a `[ Continuar y escribir ]`. Atajos: `F5`
actualizar, `F8` abrir la exportación del rail (`64 MiB` o geometría de la USB),
`F9` plegar/desplegar el rail, flechas para elegir, `Enter` preparar, `F10`
salir; en los pasos `Esc` cancela. Sin privilegios se puede explorar y exportar
una imagen, pero el botón de preparar queda deshabilitado con el mensaje de
privilegios.

La CLI headless construye y verifica imágenes sin tocar hardware:

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
posible (unos 62 MiB, por encima del mínimo FAT32). Si se pide `--esp-mib`
explícito y no entra, el build falla.

Con la imagen verificada se puede preparar un destino. En una USB real hay que
ejecutar con `sudo`; sin `--yes` la CLI pide confirmación explícita antes de
abrir el dispositivo. Un archivo regular del tamaño exacto sirve como destino de
prueba y no requiere privilegios:

```bash
# Destino real (se requiere root y confirmación)
sudo ./build/booteable-image flash --device /dev/sdb \
  --image build/booteable.img

# Destino de prueba sin tocar hardware
truncate -s $((64 * 1024 * 1024)) build/usb.img
./build/booteable-image flash --device build/usb.img \
  --image build/booteable.img --yes
```

`flash` rechaza una imagen cuyo sector lógico o tamaño no coincide con el
destino, rechaza discos del sistema y dispositivos sin identidad estable, y
desmonta solo las particiones del destino antes de escribir. El resultado
informa la etapa, los bytes escritos y los hashes releídos; solo imprime
`VERIFICADO` cuando la relectura reproduce todas las regiones del manifiesto.

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

La salida esperada (`BOOTEABLE-BOOT-OK`) y los hashes de las imágenes de la
última corrida están en [`docs/Evidencias.md`](docs/Evidencias.md).

## Pruebas

```bash
ctest --test-dir build -j4 --output-on-failure

# Una suite concreta, sin pasar por CTest.
./build/booteable_tests flash
./build/booteable_tests prep_controller
```

Las pruebas son headless, no requieren red, root ni una USB real. En un equipo
de cuatro núcleos el conjunto completo tardó **62,0 s** en la última corrida y
entre 69,6 y 93,5 s en corridas con más contención
(ver [`docs/Evidencias.md`](docs/Evidencias.md)):

- `test_validation`, `test_fake_controller`, `test_core` (CRC32 y SHA-256),
  `test_layout`, `test_gpt`, `test_fat32` e `test_image_roundtrip`: dominio,
  geometría, GPT, FAT32 y corrupción de cada región.
- `test_safety_policy`, `test_sysfs_source`, `test_mount_manager` y
  `test_device_writer`: enumeración con fixtures, política, montajes y escritor.
- `test_flash`: escritura y relectura en archivo regular, desconexión simulada,
  cancelación, tamaños/sectores incompatibles y fallo de desmontaje.
- `test_prep_controller` y `test_ui_flow`: hilos, privilegios, exportación,
  cancelación, ausencia de descriptores colgados, escenas a 80×24 y 120×40.
- `efi_binary`: cabecera PE32+, subsistema EFI, ausencia de imports de Windows y
  presencia del marcador.
- `image_cli`: `build`/`verify` en 512 y 4096, tamaño exacto, etiqueta, detección
  de corrupción y control cruzado opcional con `fsck.vfat`.
- `flash_cli`: `flash` sobre un archivo regular del tamaño exacto, rechazo de
  tamaños distintos y de imágenes corruptas, y ninguna escritura sin
  confirmación. No requiere root ni USB.
- `qemu_boot` y `qemu_boot_image`: arranque real en OVMF, desde la carpeta y
  desde la imagen construida. Se saltan con código 77 si faltan QEMU u OVMF.
- `qemu_boot_4kn`: la misma imagen con sector lógico de 4096 y ESP de 260 MiB,
  presentada como disco virtio 4Kn.
- `qemu_secure_boot`: prueba negativa con OVMF Secure Boot; exige que el binario
  sin firma sea bloqueado. También se salta si faltan las imágenes de OVMF con
  Secure Boot.

Las cuatro pruebas QEMU comparten las variables de OVMF y el log serie, así que
CTest las serializa con `RESOURCE_LOCK` aunque se ejecute con `-j4`.

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

1. **Componentes**: los módulos agrupados por mundo, el medio USB como puente y
   la cadena firmware → servicios UEFI → programa propio.
2. **Clases**: enums, structs del dominio, contratos e implementaciones de
   WP3/WP4 (`PrepController`, `PrepRunner`, `DeviceFlasher`), con compartimentos
   y relaciones UML nativas.
3. **Secuencia**: enumerar, validar, confirmar, construir, escribir, sincronizar,
   verificar y cancelar, pasando por `PrepRunner` y `DeviceFlasher`.
4. **Estados**: la máquina `PrepStage` y los terminales de error y cancelación.
5. **Leyenda**: relaciones UML, visibilidad y estereotipos.

Las vistas previas están en [`docs/diagramas/`](docs/diagramas/README.md) y se
regeneran con `python3 tools/generate_diagram.py` y el CLI de draw.io.

## Estructura

```text
boot/         Aplicación EFI propia (freestanding, sin libc)
src/
  domain/     Tipos, enums y validación pura (congelado en WP0)
  core/       SHA-256, CRC32 y utilidades de bytes
  image/      ImageLayout, GPT, FAT32, ensamblado y verificador de imagen
  device/     Enumeración sysfs, política, montajes, escritor y flujo de flash
  app/        FakePrepController, PrepController y PrepRunner
  ui/         Escenas, confirmación, progreso, resultado y formato
  cli/        build, verify y flash
tools/        build-efi, embed-efi, run-qemu, check-efi, test-image-cli,
              test-cli-flash
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
- WP2 construye la imagen y WP3b la escribe sobre el dispositivo: `fsync`,
  `BLKFLSBUF`, relectura de las regiones del manifiesto y comparación de
  SHA-256. Una operación cancelada o interrumpida queda marcada como
  **incompleta** y nunca se informa como verificada.
- El programa de arranque no escribe en discos: solo usa consola/serie, espera
  una tecla con límite y se apaga con `ResetSystem`.
- Secure Boot bloquea binarios sin firma. La herramienta **no** desactiva ni
  altera claves: si el equipo lo tiene activo, se documenta el requisito y se
  prueba en un equipo de laboratorio con autorización.
