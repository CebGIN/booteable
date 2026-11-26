# Guion de la demo · WP5

Duración objetivo: 5–7 minutos. La demo muestra el flujo completo sin depender
de una USB física; la prueba P9 se hace aparte con el checklist de
[`Prueba-fisica.md`](Prueba-fisica.md).

## Preparación previa

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build -j4 --output-on-failure

./build/booteable-image build --output build/booteable.img --size-mb 64
./build/booteable-image verify --image build/booteable.img
./build/booteable-image build --output build/booteable-4kn.img \
  --size-mb 320 --sector-size 4096 --esp-mib 260
./build/booteable-image verify --image build/booteable-4kn.img
```

Dejar una terminal en el repositorio y otra para QEMU. Si hay OVMF y QEMU, la
demo no necesita root.

## 1. Enumeración y exportación sin root (1 min)

```bash
./build/booteable
```

- Mostrar la lista con modelo, serie, tamaño y montajes.
- Señalar que la política de seguridad rechaza el disco raíz y los discos no
  extraíbles.
- Pulsar `F8`: se despliega el rail de herramientas con `[ 64 MiB ]` y
  `[ /dev/sdb ]`. Elegir `64 MiB`; la imagen se exporta a
  `build/booteable-export-64mib.img`. La opción con la geometría seleccionada
  produce `build/booteable-export-sdb.img` y no toca la USB.
- Mostrar que `Preparar /dev/sdb` es la única acción sobre la selección.
- Intentar preparar: el botón está deshabilitado y el estado sugiere `sudo`.
- Salir con `F10`.

Mensaje esperado: *"Se requieren privilegios para escribir la USB."*

## 2. Construcción y verificación headless (1 min)

```bash
./build/booteable-image verify --image build/booteable.img
./build/booteable-image verify --image build/booteable-4kn.img
```

Mostrar que `verify` relee MBR, GPT primaria, ESP FAT32, GPT de respaldo, el
árbol `EFI/BOOT/BOOTX64.EFI` y el manifiesto SHA-256. Un byte alterado hace
fallar la verificación (lo cubre `test_image_roundtrip`).

## 3. Escritura y verificación sobre archivo regular (1 min)

```bash
truncate -s $((64 * 1024 * 1024)) build/demo-target.img
./build/booteable-image flash --device build/demo-target.img \
  --image build/booteable.img --yes
```

Resultado esperado: **VERIFICADO**, bytes escritos y hashes de imagen y
dispositivo iguales. Después:

```bash
./build/booteable-image verify --image build/demo-target.img
```

## 4. Cancelación honesta (1 min)

```bash
./build/booteable_tests flash
```

La suite ejercita cancelación durante la escritura, `ENODEV` simulado y
desmontaje fallido. El punto que hay que remarcar: una cancelación produce
**INCOMPLETO**, nunca **VERIFICADO**.

Si se hace con una USB de laboratorio desechable, cancelar con `Esc` en la
tarjeta de paso y mostrar el resultado en ámbar. No usar una USB con datos.

## 5. Arranque virtual (2 min)

```bash
./tools/run-qemu.sh --headless
./tools/run-qemu.sh --headless --image build/booteable.img
./tools/run-qemu.sh --headless --image build/booteable-4kn.img \
  --sector-size 4096
```

Salida esperada en los tres casos:

```text
Grupo: booteable
Build: <hash corto>
BOOTEABLE-BOOT-OK
Apagando el equipo.
```

Cerrar con la prueba negativa:

```bash
./tools/run-qemu.sh --headless --secure-boot
```

Debe informar que el binario sin firma fue bloqueado por Secure Boot.

## 6. Cierre (30 s)

- `ctest -j4` termina en menos de 2 minutos.
- La imagen de 64 MiB usa ESP automática de 62 MiB; la 4Kn usa 260 MiB.
- P9 queda como paso manual en el equipo de laboratorio, con evidencia en
  `docs/Prueba-fisica.md`.

## Si falla algo

| Situación | Plan B |
|---|---|
| No hay OVMF/QEMU | Mostrar `docs/Evidencias.md` y las suites headless; las pruebas QEMU se saltan con código 77. |
| No hay USB o no hay root | La demo headless con archivo regular cubre construir, escribir y verificar; la enumeración real se muestra sin escribir. |
| El equipo no arranca la USB | Registrar el síntoma y revisar Secure Boot/modo UEFI; no improvisar cambios en el firmware. |
| Falta tiempo | Prioridad: `verify` de la imagen 512, `flash` sobre archivo regular y QEMU headless. |
