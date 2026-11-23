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

**Diseño para implementación.** Este repositorio contiene por ahora el enunciado,
los requerimientos derivados, la arquitectura, el glosario, el plan de trabajo y
el diagrama. Todavía no hay código ni submódulo del motor; el plan indica en qué
orden se agregan.

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
- Git, para el submódulo del motor.
- Opcional: QEMU + OVMF para la prueba virtual.
- Opcional: `draw.io` 24 o superior para editar/exportar el diagrama.

## Compilar (planificado)

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
```

Targets previstos:

| Target | Descripción |
|---|---|
| `booteable_core` | Núcleo headless: dominio, imagen GPT/FAT32, dispositivos, verificador |
| `booteable_ui` | Escenas de la TUI sobre Guayaba |
| `booteable` | Aplicación TUI |
| `booteable-image` | CLI headless para construir, verificar y escribir una imagen |
| `bootx64.efi` | Aplicación EFI que arranca el firmware |
| `booteable_tests` | Suites headless registradas en CTest |

La aplicación EFI se compila con el *cross-compiler* de MinGW-w64
(`-ffreestanding -nostdlib -Wl,--subsystem,10`) y se **incrusta** en el
preparador como un arreglo de bytes. Así el binario de la TUI es autocontenido y
no depende de encontrar `BOOTX64.EFI` en el disco.

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

## Ejecutar (planificado)

```bash
sudo ./build/booteable
```

La TUI enumera las USB, muestra identificador, capacidad, serie, puntos de
montaje y la etiqueta/tamaño elegidos; exige marcar una casilla de advertencia y
pulsar `Enter` antes de escribir. Sin privilegios se puede explorar y exportar
una imagen, pero no escribir un dispositivo.

Para trabajar sin terminal o en pruebas:

```bash
# Construir una imagen de disco reproducible
./build/booteable-image build --output build/booteable.img --size-mb 64

# Verificar estructura, CRC y contenido
./build/booteable-image verify --image build/booteable.img

# Imagen para un dispositivo 4Kn (sector lógico de 4096)
./build/booteable-image build --output build/booteable-4kn.img \
  --sector-size 4096 --esp-mib 260

# Escribir y verificar (rechaza discos del sistema)
sudo ./build/booteable-image flash --device /dev/sdX --image build/booteable.img
```

## Prueba virtual

Antes de la USB física, la imagen se arranca en QEMU con OVMF:

```bash
./tools/run-qemu.sh build/booteable.img
```

El programa de arranque escribe el mismo mensaje en la consola UEFI y en el
puerto serie, de modo que la prueba también puede capturarse sin ventana:

```bash
./tools/run-qemu.sh build/booteable.img --headless
```

Criterio de éxito: aparece el marcador `BOOTEABLE-BOOT-OK` junto con el nombre
del grupo y el hash de compilación.

## Pruebas (planificadas)

```bash
ctest --test-dir build --output-on-failure
```

Las pruebas son headless, no requieren red, root ni una USB real. Las que
necesitan un dispositivo de bloque usan un archivo regular o un dispositivo falso;
la prueba QEMU se registra como opcional y se salta si faltan QEMU u OVMF.

## Diagrama

Páginas:

1. **Clases**: enums, structs del dominio, contratos e implementaciones, con
   compartimentos y relaciones UML nativas.
2. **Secuencia**: enumerar, validar, confirmar, construir, escribir, sincronizar,
   verificar y cancelar.
3. **Leyenda**: relaciones UML, visibilidad y estereotipos.

Las vistas previas están en [`docs/diagramas/`](docs/diagramas/README.md) y se
regeneran con `python3 tools/generate_diagram.py` y el CLI de draw.io.

## Estructura (planificada)

```text
boot/         Aplicación EFI propia (freestanding, sin libc)
src/
  domain/     Tipos, enums y validación pura
  core/       SHA-256, CRC32, formato de tamaños, utilidades
  image/      GPT, FAT32, ensamblado y verificador de imagen
  device/     Enumeración sysfs, política de seguridad, escritor y montajes
  app/        Controlador, runner, estado y mocks
  ui/         Escenas, paneles, diálogo de confirmación y progreso
  cli/        Punto de entrada de booteable-image
tools/        Generador del diagrama y scripts de EFI, QEMU y verificación
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
- Escribe la imagen completa, hace `fsync`, relee el dispositivo y compara
  SHA-256 antes de informar "verificado". Una operación cancelada o interrumpida
  queda marcada como **incompleta**.
- El programa de arranque no escribe en discos: solo usa consola/serie y espera
  una tecla.
- Secure Boot bloquea binarios sin firma. La herramienta **no** desactiva ni
  altera claves: si el equipo lo tiene activo, se documenta el requisito y se
  prueba en un equipo de laboratorio con autorización.
