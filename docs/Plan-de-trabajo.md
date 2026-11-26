# Plan de trabajo · preparador UEFI con programa propio

## 1. Equipo y responsabilidades

| Rol | Frente | Archivos |
|---|---|---|
| **Lead / integración** | Contratos, `app/`, CLI, CMake, compilación EFI, QEMU, revisión y documentos | `src/domain/`, `src/app/`, `src/cli/`, `CMakeLists.txt`, `tools/`, `docs/` |
| **Arranque** | Programa EFI, mensaje, consola/serie, apagado y prueba en firmware | `boot/` |
| **Imagen** | Geometría, GPT, FAT32, ensamblado y verificador de imagen | `src/image/`, `src/core/` |
| **Dispositivos** | Enumeración sysfs, política de seguridad, montajes, escritura y relectura | `src/device/` |
| **UI** | Escenas, diálogo de confirmación, progreso, resultado y formato | `src/ui/` |

Reglas:

- El **Lead** es el único que cambia los contratos de `src/domain/` y las
  interfaces de `src/app/`. Se anuncian en el chat y se integran **primero** en `main`.
- Cada tarea tiene dueño y un revisor distinto. Nadie mergea su propio cambio.
- El Lead revisa contratos y `app/`; Arranque revisa Imagen; Imagen revisa
  Dispositivos; Dispositivos revisa UI; UI revisa Arranque.
- **WP3a** (enumeración y política) puede empezar después de WP0, sin esperar a
  WP1 ni WP2. **WP3b** (escritura y verificación) no empieza hasta que WP2
  entregue `BootImageBuilder` e `ImageManifest`.
- **WP4a** (UI con `FakePrepController`) puede empezar después de WP0. **WP4b**
  (controlador y flujo real) requiere WP2 + WP3b.
- No se toca una USB física hasta que QEMU, las suites headless y WP4b estén en
  verde.
- Los contratos se congelan al cerrar WP0, después de la ronda de revisión. WP5
  verifica que no cambiaron; no los congela.

## 2. Ronda de revisión y congelamiento

Antes de empezar a implementar los paquetes, el plan pasa por una revisión del
equipo y del profesor:

1. Confirmar que cada WP tiene dueño y revisor distinto.
2. Revisar el alcance de RF-01..RF-16 y las exclusiones del MVP.
3. Confirmar tamaño de ESP, etiqueta, soporte 512/4096 y política de seguridad.
4. Ajustar el orden de integración según la disponibilidad de hardware.
5. Registrar los cambios en este documento.

Al cerrar WP0 se congelan `src/domain/` y las interfaces de `src/app/`. Cualquier cambio
posterior lo integra el Lead primero y se justifica.

No hay fechas ni plazos en este plan: solo tareas, dependencias y criterios de
salida. El cronograma real se maneja fuera del repositorio.

## 3. Paquetes de trabajo

### WP0 · Contratos, esqueleto y cadena de compilación

**Depende de:** nada.
**Objetivo:** que todos los frentes empiecen sin esperar al núcleo y que
compilación EFI + TUI + tests + QEMU funcione de punta a punta.

Entregables:

- `CMakeLists.txt` con los targets de `Arquitectura.md` §18, detección de
  MinGW-w64 y mensaje claro si falta.
- `src/domain/Types.hpp` y `src/domain/Validation.*` con los tipos y reglas de §5.
- Interfaces `IBlockDeviceSource`, `IDeviceWriter`, `IImageVerifier`,
  `IPrepController` y `FakePrepController`.
- `tests/TestHarness.hpp` con `CHECK`/`CHECK_EQ` y pruebas de validación y del
  controlador falso.
- `boot/efi.h` mínimo y `boot/main.cpp` que imprime un "hola", el marcador
  `WP0-BOOT-OK` y devuelve `EFI_SUCCESS` como stub.
- `tools/build-efi.sh`, `tools/run-qemu.sh` y `bootx64_efi` compilando y
  arrancando en QEMU/OVMF.
- `tools/embed-efi.py` y la generación de
  `build/generated/EmbeddedBootImage.cpp` con su SHA-256, a partir del EFI de
  WP0. Así WP2 puede construir imágenes antes de que exista la versión final de
  WP1.
- Submódulo `external/GuayabaTUIEngine` fijado en `v1.3.0-alpha` más el commit
  `9eaf8bb` (formato declarativo de escena/tema) y una escena mínima que abra y
  cierre.

Orden recomendado:

1. CMake + `src/domain/` + contratos + fake + pruebas.
2. `boot/efi.h`, `boot/main.cpp` "hola" y `tools/build-efi.sh`.
3. `tools/run-qemu.sh` y smoke test en QEMU/OVMF.
4. Generación del EFI incrustado con `tools/embed-efi.py`.
5. Submódulo Guayaba y escena mínima.

Criterio de salida:

- `cmake -S . -B build && cmake --build build` compila todos los targets.
- `ctest --test-dir build -j4` pasa las suites y `qemu_boot` (o lo salta si
  faltan QEMU u OVMF).
- `bootx64_efi` arranca en QEMU/OVMF y muestra `WP0-BOOT-OK` en serie.
- `./build/booteable` abre una pantalla mínima con dispositivos simulados y sale
  con `F10`.
- `src/domain/`, los contratos y las interfaces quedaron revisados y
  congelados.

### WP1 · Programa de arranque

**Depende de:** WP0.
**Consume:** `boot/efi.h`, `tools/run-qemu.sh`, generador de EFI incrustado.
**Entrega:** aplicación EFI completa; WP2 vuelve a embeberla al final.

| Paso | Tarea | Archivos |
|---|---|---|
| 1 | Tablas UEFI, `ConOut`/`ConIn`, `Stall`, `memcpy`/`memset` | `boot/efi.h`, `boot/efi.cpp` |
| 2 | Mensaje fijo, colores, hash de compilación y marcador `BOOTEABLE-BOOT-OK` | `boot/main.cpp` |
| 3 | Salida por `SerialIo` (sin fallar si no existe) | `boot/main.cpp` |
| 4 | Espera de tecla y apagado con `ResetSystem(EfiResetShutdown)` | `boot/main.cpp` |
| 5 | Recompilar `EmbeddedBootImage.cpp` con el EFI final | `CMakeLists.txt` |
| 6 | QEMU con ventana y headless; prueba negativa con Secure Boot | `tools/run-qemu.sh` |

Criterios de aceptación:

- `file BOOTX64.EFI` reporta PE32+ y `objdump -p` muestra subsistema 10.
- QEMU headless imprime `BOOTEABLE-BOOT-OK` y el mensaje completo, y el equipo se
  apaga.
- El binario no referencia `msvcrt`, no usa reloj ni RNG y no escribe discos.
- Con Secure Boot activo en OVMF no arranca; el documento lo explica y no se
  intenta evadir.

### WP2 · Construcción de imagen GPT/FAT32

**Depende de:** WP0. Puede avanzar en paralelo con WP1 usando el EFI "hola" de
WP0; al terminar WP1 se reconstruye la imagen con el EFI final.
**Consume:** `src/domain/`, `src/core/Crc32`, `src/core/Sha256`, `EmbeddedBootImage`.
**Entrega:** `ImageLayout`, `GptWriter`, `Fat32Writer`, `BootImageBuilder`,
`ImageVerifier`.

| Paso | Tarea | Archivos |
|---|---|---|
| 1 | CRC32 y SHA-256 con vectores de prueba | `src/core/Crc32.*`, `src/core/Sha256.*` |
| 2 | Geometría: sector 512/4096, ESP, entry_sectors, límites y tamaño/label | `src/image/ImageLayout.*` |
| 3 | MBR protector y GPT primaria/respaldo | `src/image/GptWriter.*` |
| 4 | BPB, FSInfo, FAT doble y cadenas | `src/image/Fat32Writer.*` |
| 5 | Ensamblado de la imagen con el EFI incrustado | `src/image/BootImageBuilder.*` |
| 6 | Verificador de estructura, regiones y contenido | `src/image/ImageVerifier.*` |
| 7 | CLI `build` y `verify` (con el Lead) | `src/cli/main.cpp` |

Criterios de aceptación:

- `booteable-image build --output build/booteable.img --size-mb 64` genera un
  archivo de tamaño exacto y `verify` lo acepta; `--esp-mib`, `--label` y
  `--sector-size` cambian los parámetros.
- El mismo build funciona con `--sector-size 4096`, con ESP mínima de 260 MiB.
- `fsck.vfat -n` sobre la ESP extraída no reporta errores (control cruzado
  opcional).
- `sgdisk -v` o el verificador propio aceptan la GPT; un byte alterado en MBR,
  GPT, FAT o EFI hace fallar `verify`.
- La imagen es determinista con GUID fijos de prueba; dos builds son idénticos
  byte a byte.
- El archivo `EFI/BOOT/BOOTX64.EFI` coincide en tamaño y SHA-256 con el binario.
- Al terminar WP1 se repite el build con el EFI final y se vuelve a verificar.

### WP3a · Enumeración y política de seguridad

**Depende de:** WP0. No depende de WP1 ni WP2.
**Consume:** `DeviceInfo`, `SystemContext`, `SafetyReport`.
**Entrega:** detección real y política pura, probadas con fixtures.

| Paso | Tarea | Archivos |
|---|---|---|
| 1 | `SysfsBlockDeviceSource`: `/sys/block`, modelo, serie, tamaño, transporte | `src/device/SysfsBlockDeviceSource.*` |
| 2 | `by_id` y montajes desde `/proc/self/mountinfo` | `src/device/DeviceIdentity.*` |
| 3 | `SystemContext` (raíz, `/boot`, `major:minor`) | `src/device/SystemContext.*` |
| 4 | `SafetyPolicy` pura + fixtures (512, 4096, sector raro) | `src/device/SafetyPolicy.*` |

Criterios de aceptación:

- La lista coincide con `lsblk -o NAME,TRAN,RM,SERIAL,SIZE,MOUNTPOINTS`.
- Un pendrive montado, el disco raíz, un NVMe y un dispositivo sin serie se
  rechazan con motivos distintos.
- 4Kn se acepta; un sector lógico distinto de 512/4096 se rechaza.
- La política se prueba sin hardware, solo con fixtures.

### WP3b · Montajes, escritura y verificación

**Depende de:** WP3a + WP2 (`BootImageBuilder`, `ImageManifest`).
**Consume:** `IDeviceWriter`, `IBlockDeviceSource`, `ImageManifest`.
**Entrega:** desmontaje, escritor real, fakes y CLI `flash`.

| Paso | Tarea | Archivos |
|---|---|---|
| 1 | `MountManager` con `umount2(MNT_DETACH)` y relectura de montajes | `src/device/MountManager.*` |
| 2 | `PosixDeviceWriter` (`open`, `ioctl`, `write`, `fsync`, relectura) | `src/device/PosixDeviceWriter.*` |
| 3 | Fakes para archivo regular y errores inyectados | `src/device/Fake*` |
| 4 | CLI `flash` con validación de sector/tamaño y confirmación (con el Lead) | `src/cli/main.cpp` |

Orden obligatorio del flujo: **validar identidad → desmontar → abrir → escribir
→ sincronizar → verificar**. Abrir con `O_EXCL` antes de desmontar puede fallar
con `EBUSY`.

Criterios de aceptación:

- Escribir y releer un archivo de 80 MiB (512) y 260 MiB (4096) produce el mismo
  `ImageManifest`.
- Un pendrive montado se desmonta antes de abrir; si no se puede, no se escribe.
- Simular `ENODEV` a mitad de escritura produce `Disconnected`/`Incomplete`.
- `umount2` se invoca solo sobre montajes del destino y se verifica después.
- `flash` rechaza una imagen cuyo sector o tamaño no coincide con el dispositivo.

### WP4a · UI con controlador falso

**Depende de:** WP0.
**Consume:** `IPrepController`, `FakePrepController`, `SafetyReport`,
`PrepProgress`, `PrepResult`.
**Entrega:** escenas navegables sin tocar hardware, descritas en
`assets/ui/booteable.gscene` y `booteable.gtheme`.

| Paso | Tarea | Archivos |
|---|---|---|
| 0 | Escena y tema declarativos, incrustados en el binario | `assets/ui/*`, `tools/embed-ui.py` |
| 1 | `MainScene` con lista y detalles | `src/ui/MainScene.*` |
| 2 | `ConfirmDialog` con casilla de advertencia + `Enter` | `src/ui/ConfirmDialog.*` |
| 3 | `ProgressScene` con etapas, barra y cancelar | `src/ui/ProgressScene.*` |
| 4 | `ResultScene` y comandos de prueba | `src/ui/ResultScene.*` |
| 5 | Formato de tamaños, hashes y estados | `src/ui/UiFormat.*` |

Criterios de aceptación:

- A 80×24 no se solapan paneles ni se cortan botones; a 120×40 se usa el layout
  de dos columnas.
- La UI no incluye `src/device/`, `src/image/` ni `src/core/`; todo pasa por
  `IPrepController`.
- Con el fake se recorren selección, confirmación, progreso, cancelación y
  resultado.
- La escena y el tema se validan en la suite `ui_scene` y no dependen de rutas
  de archivos en runtime.

### WP4b · Controlador, runner e integración real

**Depende de:** WP4a + WP2 + WP3b.
**Consume:** `BootImageBuilder`, `SafetyPolicy`, `MountManager`,
`PosixDeviceWriter`, `ImageVerifier`.
**Entrega:** flujo completo de la TUI sobre datos reales.

| Paso | Tarea | Archivos |
|---|---|---|
| 1 | `PrepController` con estado, hilo y cancelación | `src/app/PrepController.*` |
| 2 | `PrepRunner`: construir → desmontar → abrir → escribir → sync → verificar | `src/app/PrepRunner.*` |
| 3 | Integrar controlador real en las escenas | `src/ui/*`, `src/app/*` |

Criterios de aceptación:

- Sin `root`, se puede enumerar y exportar, pero el botón de escribir está
  deshabilitado con el mensaje de privilegios.
- Sin la casilla marcada, `Enter` no abre el dispositivo.
- Cancelar durante la escritura deja `Incomplete` y la UI lo muestra en ámbar.
- La app no se cae si la USB se desconecta ni si el controlador devuelve error.
- No queda ningún dispositivo montado ni descriptor abierto al cerrar.

### WP5 · Integración, pruebas y entrega

**Depende de:** WP1, WP2, WP3b, WP4b.
**Objetivo:** verificar que todo el conjunto cumple el enunciado y dejarlo
documentado.

| Paso | Tarea |
|---|---|
| 1 | Integrar WP1–WP4 en `main` y auditar los contratos; registrar las extensiones aditivas de WP3/WP4 |
| 2 | Completar suites de `Arquitectura.md` §14 y `ctest -j4` sin root |
| 3 | Repetir build de imagen con el EFI final y verificar 512/4096 |
| 4 | Probar QEMU headless (y con ventana si hay escritorio); capturar evidencia en `docs/Evidencias.md` |
| 5 | Preparar una USB de laboratorio y hacer el arranque físico (P9) con `docs/Prueba-fisica.md` |
| 6 | Regenerar diagrama y vistas previas |
| 7 | Actualizar README, AGENTS y Glosario con el estado real |
| 8 | Ensayo de la demo con `docs/Guion-demo.md` |

Criterios de aceptación:

- `ctest -j4 --output-on-failure` pasa en menos de 2 minutos en un equipo de 4
  núcleos.
- La prueba QEMU se salta limpiamente si faltan OVMF/QEMU.
- No quedan procesos, montajes ni descriptores colgados después de cerrar la TUI.
- La USB arranca en el equipo de prueba y muestra el mensaje del grupo.
- El README refleja comandos y rutas reales, no los planificados.
- P9 queda registrada en `docs/Prueba-fisica.md`; si el hardware no está
  disponible, se documenta como el único pendiente.

## 4. Orden de integración y dependencias

```text
WP0 ──┬──> WP1 ───────────────────────────────┐
      ├──> WP2 ────────────┐                  │
      ├──> WP3a ───────────┴──> WP3b ──┐      │
      └──> WP4a ───────────────────────┴──> WP4b ──> WP5
```

- WP1, WP2, WP3a y WP4a pueden avanzar en paralelo después de WP0.
- WP4b no espera a WP1: puede integrarse con el EFI "hola" y recompilarse al
  final. WP1 sí es obligatorio para WP5 y la prueba física.
- WP2 usa el EFI "hola" de WP0 y se reconstruye con el EFI final de WP1.
- WP3b necesita WP2 (imagen y manifiesto) y WP3a (destino y política).
- WP4b necesita WP4a, WP2 y WP3b.
- WP5 va al final, cuando WP4b esté integrado.

Ruta crítica: **WP0 → WP2 → WP3b → WP4b → WP5**. WP1 debe terminar antes de la
verificación final y de la prueba física.

## 5. Prioridad de recorte

Si hay que reducir el alcance, se recorta en este orden:

1. Firma Secure Boot / MOK.
2. Layout de dos columnas a 120×40 (queda el apilado).
3. Prueba QEMU automatizada (queda la manual documentada).
4. Exportar imagen desde la TUI (queda la CLI).

**No se recorta:** política de seguridad, confirmación, desmontaje, `fsync` +
verificación, cancelación honesta, mensaje de arranque propio.

## 6. Riesgos de coordinación

| Riesgo | Mitigación |
|---|---|
| Cambios de contrato sin aviso | Lead los integra primero y avisa; no hay cambios sin justificación |
| Ramas largas | Ramas cortas e integración frecuente; el plan no fija fechas |
| Compilador MinGW distinto entre máquinas | Fijar flags en CMake; verificar `objdump -p` en CI/local |
| QEMU/OVMF distinto | Documentar la ruta y permitir `OVMF_PATH`; el test se salta si falta |
| Una sola USB de laboratorio | Turnos de prueba y respaldo previo; nunca usar el disco del sistema |
| Secure Boot en el equipo de prueba | Verificarlo antes de la prueba física; pedir autorización |
